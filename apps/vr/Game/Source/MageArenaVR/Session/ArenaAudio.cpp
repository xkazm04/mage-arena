#include "Session/ArenaAudio.h"

#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Kernel/Catalog.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "Kernel/VrRules.h"
#include "Kismet/GameplayStatics.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/ArenaSession.h"
#include "Session/ElementColour.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWave.h"

namespace
{
constexpr double ProjectileHeightCm = 120.0;
constexpr double BodyHeightCm = 110.0;
constexpr double HoundMouthCm = 30.0;

// The seated cuff sits on the casting wrist (SessionPresentation EnsureCuff: CuffRoot at camera + (58, -26, -14) cm).
// The ward hand is the mirror of it on the other side, a little higher (a raised palm).
const FVector CuffOffsetCm(58.0, -26.0, -14.0);
const FVector WardHandOffsetCm(45.0, 26.0, -8.0);

int32 PriorityFromString(const FString& Text)
{
	if (Text == TEXT("low"))
	{
		return 0;
	}
	if (Text == TEXT("high"))
	{
		return 2;
	}
	if (Text == TEXT("critical"))
	{
		return 3;
	}
	return 1;
}

FVector ToWorld(const FAudioFrame& Frame, const FSimVec& Pos, double LiftCm)
{
	FVector At = Frame.ToCm ? Frame.ToCm(Pos.X, Pos.Y) : FVector(Pos.X * 100.0, Pos.Y * 100.0, 0.0);
	At.Z += LiftCm;
	return At;
}

FString PendingFamily(const FPendingCast& Pending)
{
	if (!Pending.SpellId.IsSet())
	{
		return FString();
	}
	const FString& Id = Pending.SpellId.GetValue();
	if (Pending.Kind == TEXT("spell"))
	{
		const FSpell* Spell = FindSpellById(Id);
		return Spell ? Spell->Family : FString();
	}
	if (Pending.Kind == TEXT("fire"))
	{
		const FFireSpell* Spell = FindFireSpell(Id);
		return Spell ? Spell->Family : FString();
	}
	if (Pending.Kind == TEXT("air"))
	{
		const FAirSpell* Spell = FindAirSpell(Id);
		return Spell ? Spell->Family : FString();
	}
	return FString();
}

bool IsFireOwner(const FArenaState& State, int32 OwnerId)
{
	const TOptional<EElement> Element = ActorElement(State, OwnerId);
	return Element.IsSet() && Element.GetValue() == EElement::Fire;
}
}

// ---------------------------------------------------------------------------------------------------------------------
// FAudioTable

const TArray<FString>& FAudioTable::KnownEvents()
{
	static const TArray<FString> Events = {
		TEXT("threat.unblockable"),
		TEXT("threat.fire.projectile"),
		TEXT("threat.hound.windup"),
		TEXT("threat.fire.impact"),
		TEXT("player.hit"),
		TEXT("player.ward.absorb"),
		TEXT("player.perfect"),
		TEXT("player.ward.raise"),
		TEXT("player.ward.hold"),
		TEXT("player.blink"),
		TEXT("player.cast.bolt"),
		TEXT("player.cast.tide-orb"),
		TEXT("player.crest"),
		TEXT("player.flow"),
		TEXT("player.unlock"),
		TEXT("rival.phase"),
		TEXT("sigil.complete"),
		TEXT("sigil.reject"),
		TEXT("sigil.draw"),
		TEXT("ui.pause"),
		TEXT("opponent.cast.water"),
		TEXT("opponent.airform"),
		TEXT("bout.crowd"),
		TEXT("crowd.swell"),
	};
	return Events;
}

bool FAudioTable::LoadFromString(const FString& Json, FString& OutError)
{
	*this = FAudioTable();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutError = TEXT("audio.json: not JSON");
		return false;
	}
	double Number = 0.0;
	if (Root->TryGetNumberField(TEXT("maxVoices"), Number))
	{
		MaxVoices = FMath::Max(1, static_cast<int32>(Number));
	}
	Root->TryGetNumberField(TEXT("masterDb"), MasterDb);
	if (MasterDb > 0.0)
	{
		OutError = TEXT("audio.json: masterDb is above 0");
		return false;
	}
	const TSharedPtr<FJsonObject>* Swell = nullptr;
	if (Root->TryGetObjectField(TEXT("crowdSwell"), Swell) && Swell && Swell->IsValid())
	{
		(*Swell)->TryGetNumberField(TEXT("gainDb"), CrowdSwellDb);
		(*Swell)->TryGetNumberField(TEXT("seconds"), CrowdSwellS);
	}
	const TSharedPtr<FJsonObject>* AttObject = nullptr;
	if (!Root->TryGetObjectField(TEXT("attenuations"), AttObject) || !AttObject || !AttObject->IsValid())
	{
		OutError = TEXT("audio.json: attenuations missing");
		return false;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*AttObject)->Values)
	{
		const TSharedPtr<FJsonObject> Object = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
		if (!Object.IsValid())
		{
			OutError = FString::Printf(TEXT("audio.json: attenuation %s is not an object"), *Pair.Key);
			return false;
		}
		FAudioAttenuationSpec Spec;
		Spec.Id = FName(*Pair.Key);
		if (!Object->TryGetNumberField(TEXT("falloffCm"), Spec.FalloffCm) || Spec.FalloffCm <= 0.0)
		{
			OutError = FString::Printf(TEXT("audio.json: attenuation %s falloffCm missing"), *Pair.Key);
			return false;
		}
		Object->TryGetNumberField(TEXT("innerFraction"), Spec.InnerFraction);
		Object->TryGetNumberField(TEXT("innerFloorCm"), Spec.InnerFloorCm);
		Object->TryGetNumberField(TEXT("farCutoffHz"), Spec.FarCutoffHz);
		Attenuations.Add(Spec.Id, Spec);
	}
	const TSharedPtr<FJsonObject>* CueObject = nullptr;
	if (!Root->TryGetObjectField(TEXT("cues"), CueObject) || !CueObject || !CueObject->IsValid())
	{
		OutError = TEXT("audio.json: cues missing");
		return false;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*CueObject)->Values)
	{
		const TSharedPtr<FJsonObject> Object = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
		if (!Object.IsValid())
		{
			OutError = FString::Printf(TEXT("audio.json: cue %s is not an object"), *Pair.Key);
			return false;
		}
		FAudioCueSpec Cue;
		Cue.Id = FName(*Pair.Key);
		Cue.Asset = FString::Printf(TEXT("/Game/Audio/SFX/%s.%s"), *Pair.Key, *Pair.Key);
		if (!Object->TryGetNumberField(TEXT("gainDb"), Cue.GainDb))
		{
			OutError = FString::Printf(TEXT("audio.json: cue %s gainDb missing"), *Pair.Key);
			return false;
		}
		if (Cue.GainDb > 0.0)
		{
			OutError = FString::Printf(TEXT("audio.json: cue %s gainDb %.1f is above 0 (the files peak at -1 dBFS)"), *Pair.Key, Cue.GainDb);
			return false;
		}
		FString Text;
		Object->TryGetStringField(TEXT("priority"), Text);
		Cue.Priority = PriorityFromString(Text);
		Object->TryGetBoolField(TEXT("spatial"), Cue.bSpatial);
		Object->TryGetBoolField(TEXT("loop"), Cue.bLoop);
		if (Object->TryGetStringField(TEXT("attenuation"), Text))
		{
			Cue.Attenuation = FName(*Text);
		}
		if (!Attenuations.Contains(Cue.Attenuation))
		{
			OutError = FString::Printf(TEXT("audio.json: cue %s names attenuation %s, which is not defined"), *Pair.Key, *Cue.Attenuation.ToString());
			return false;
		}
		if (Object->TryGetNumberField(TEXT("maxVoices"), Number))
		{
			Cue.MaxVoices = FMath::Max(1, static_cast<int32>(Number));
		}
		Object->TryGetNumberField(TEXT("cooldownMs"), Cue.CooldownMs);
		Object->TryGetNumberField(TEXT("seconds"), Cue.Seconds);
		Cues.Add(Cue.Id, Cue);
	}
	const TArray<TSharedPtr<FJsonValue>>* EventArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("events"), EventArray) || !EventArray)
	{
		OutError = TEXT("audio.json: events missing");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *EventArray)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FAudioEventSpec Spec;
		FString CueName;
		if (!Object.IsValid() || !Object->TryGetStringField(TEXT("event"), Spec.Event) || !Object->TryGetStringField(TEXT("cue"), CueName)
			|| !Object->TryGetStringField(TEXT("at"), Spec.At))
		{
			OutError = TEXT("audio.json: an event row needs event, cue and at");
			return false;
		}
		Spec.Cue = FName(*CueName);
		if (!KnownEvents().Contains(Spec.Event))
		{
			OutError = FString::Printf(TEXT("audio.json: unknown event %s"), *Spec.Event);
			return false;
		}
		if (FindEvent(Spec.Event))
		{
			OutError = FString::Printf(TEXT("audio.json: event %s is mapped twice"), *Spec.Event);
			return false;
		}
		if (!Cues.Contains(Spec.Cue))
		{
			OutError = FString::Printf(TEXT("audio.json: event %s names cue %s, which is not defined"), *Spec.Event, *CueName);
			return false;
		}
		Events.Add(Spec);
	}
	bLoaded = true;
	return true;
}

bool FAudioTable::LoadFromFile(const FString& Path, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("audio.json: cannot read %s"), *Path);
		return false;
	}
	return LoadFromString(Text, OutError);
}

const FAudioEventSpec* FAudioTable::FindEvent(const FString& Event) const
{
	return Events.FindByPredicate([&Event](const FAudioEventSpec& Spec) { return Spec.Event == Event; });
}

FString FAudioTable::DefaultPath()
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/audio.json"));
	FPaths::CollapseRelativeDirectories(Path);
	return Path;
}

const FAudioTable& FAudioTable::Default()
{
	static FAudioTable Table;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		FString Error;
		if (!Table.LoadFromFile(DefaultPath(), Error))
		{
			UE_LOG(LogMageArena, Error, TEXT("audio: %s"), *Error);
		}
		else
		{
			UE_LOG(LogMageArena, Log, TEXT("audio: %d cues, %d events from %s"), Table.Cues.Num(), Table.Events.Num(), *DefaultPath());
		}
	}
	return Table;
}

// ---------------------------------------------------------------------------------------------------------------------
// FArenaAudioDirector

void FArenaAudioDirector::Reset()
{
	EventCursor = 0;
	LastTick = -1;
	SessionCursor = 0;
	SeenProjectiles.Reset();
	SeenTelegraphs.Reset();
	SeenPendings.Reset();
	Voices.Reset();
	LastStart.Reset();
	LoopsOn.Reset();
	bWasAbsorb = false;
	bWasPaused = false;
	LastFlow = 0;
	LastCrests = 0;
	Follow.Reset();
}

void FArenaAudioDirector::Expire(double NowS)
{
	Voices.RemoveAll([NowS](const FVoice& Voice) { return !Voice.bLoop && Voice.EndS <= NowS + 1.0e-9; });
}

int32 FArenaAudioDirector::LiveVoices(FName Cue, double NowS) const
{
	int32 Count = 0;
	for (const FVoice& Voice : Voices)
	{
		Count += (Voice.Cue == Cue && (Voice.bLoop || Voice.EndS > NowS + 1.0e-9)) ? 1 : 0;
	}
	return Count;
}

int32 FArenaAudioDirector::LiveVoicesTotal(double NowS) const
{
	int32 Count = 0;
	for (const FVoice& Voice : Voices)
	{
		Count += (!Voice.bLoop && Voice.EndS > NowS + 1.0e-9) ? 1 : 0;
	}
	return Count;
}

bool FArenaAudioDirector::Admit(const FAudioCueSpec& Cue, double NowS)
{
	Expire(NowS);
	if (Cue.CooldownMs > 0.0)
	{
		if (const double* Last = LastStart.Find(Cue.Id))
		{
			if (NowS - *Last < Cue.CooldownMs / 1000.0 - 1.0e-9)
			{
				return false;
			}
		}
	}
	if (LiveVoices(Cue.Id, NowS) >= Cue.MaxVoices)
	{
		return false;
	}
	// Critical cues are reserved: the global budget never refuses them.
	if (Cue.Priority < 3 && LiveVoicesTotal(NowS) >= Table->MaxVoices)
	{
		return false;
	}
	return true;
}

void FArenaAudioDirector::Raise(const FString& Event, const FVector& At, int32 SourceId, int32 FollowId, double NowS, TArray<FAudioPlayRequest>& Out)
{
	const FAudioEventSpec* Spec = Table->FindEvent(Event);
	const FAudioCueSpec* Cue = Spec ? Table->FindCue(Spec->Cue) : nullptr;
	if (!Cue)
	{
		return;
	}
	if (!Admit(*Cue, NowS))
	{
		++Dropped;
		return;
	}
	FAudioPlayRequest Request;
	Request.Kind = EAudioRequestKind::OneShot;
	Request.Cue = Cue->Id;
	Request.Event = Event;
	Request.LocationCm = At;
	Request.FollowId = FollowId;
	Request.SourceId = SourceId;
	Request.AtS = NowS;
	Request.GainDb = Cue->GainDb + Table->MasterDb;
	Out.Add(Request);
	LastStart.Add(Cue->Id, NowS);
	FVoice Voice;
	Voice.Cue = Cue->Id;
	Voice.StartS = NowS;
	Voice.EndS = NowS + FMath::Max(0.05, Cue->Seconds);
	Voice.Priority = Cue->Priority;
	Voices.Add(Voice);
	PeakVoices = FMath::Max(PeakVoices, LiveVoicesTotal(NowS));
	if (FollowId >= 0)
	{
		Follow.Add(FollowId, At);
	}
}

void FArenaAudioDirector::Loop(FName CueId, const FString& Event, bool bOn, const FVector& At, double NowS, TArray<FAudioPlayRequest>& Out)
{
	const FAudioEventSpec* Spec = Table->FindEvent(Event);
	const FAudioCueSpec* Cue = Spec ? Table->FindCue(Spec->Cue) : nullptr;
	if (!Cue)
	{
		return;
	}
	const bool bWasOn = LoopsOn.Contains(Cue->Id);
	if (bOn == bWasOn)
	{
		return;
	}
	FAudioPlayRequest Request;
	Request.Kind = bOn ? EAudioRequestKind::LoopStart : EAudioRequestKind::LoopStop;
	Request.Cue = Cue->Id;
	Request.Event = Event;
	Request.LocationCm = At;
	Request.AtS = NowS;
	Request.GainDb = Cue->GainDb + Table->MasterDb;
	Out.Add(Request);
	if (bOn)
	{
		LoopsOn.Add(Cue->Id);
		FVoice Voice;
		Voice.Cue = Cue->Id;
		Voice.StartS = NowS;
		Voice.bLoop = true;
		Voice.Priority = Cue->Priority;
		Voices.Add(Voice);
	}
	else
	{
		LoopsOn.Remove(Cue->Id);
		const FName Id = Cue->Id;
		Voices.RemoveAll([Id](const FVoice& Voice) { return Voice.bLoop && Voice.Cue == Id; });
	}
}

TArray<FAudioPlayRequest> FArenaAudioDirector::Step(const FAudioFrame& Frame)
{
	TArray<FAudioPlayRequest> Out;
	Follow.Reset();
	if (!Table || !Table->bLoaded || !Frame.State)
	{
		return Out;
	}
	const FArenaState& State = *Frame.State;
	const double Now = Frame.NowS;
	// A new bout replaces the kernel state: the tick goes back and the event list starts again.
	if (State.Tick < LastTick || State.Events.Num() < EventCursor)
	{
		EventCursor = 0;
		SeenProjectiles.Reset();
		SeenTelegraphs.Reset();
		SeenPendings.Reset();
		LastFlow = 0;
		LastCrests = 0;
		bWasAbsorb = false;
	}
	LastTick = State.Tick;
	Expire(Now);

	const FActor* Player = SimFindActor(State, Frame.PlayerId);
	const bool bLive = Frame.bActive && !Frame.bPaused;

	if (Frame.bPaused && !bWasPaused)
	{
		Raise(TEXT("ui.pause"), Frame.CuffCm, Frame.PlayerId, -1, Now, Out);
	}
	bWasPaused = Frame.bPaused;

	// Loops: the crowd under the bout, the ward's hum while the palm is up, the trail while a pen is down.
	auto LoopCue = [this](const TCHAR* Event) -> FName
	{
		const FAudioEventSpec* Spec = Table->FindEvent(Event);
		return Spec ? Spec->Cue : NAME_None;
	};
	const bool bAbsorb = Player && Player->bAbsorb && !Player->bDown;
	Loop(LoopCue(TEXT("bout.crowd")), TEXT("bout.crowd"), bLive, Frame.ListenerCm, Now, Out);
	Loop(LoopCue(TEXT("player.ward.hold")), TEXT("player.ward.hold"), bLive && bAbsorb, Frame.WardHandCm, Now, Out);
	Loop(LoopCue(TEXT("sigil.draw")), TEXT("sigil.draw"), !Frame.bPaused && Frame.bSigilDrawing, Frame.CuffCm, Now, Out);

	if (bAbsorb && !bWasAbsorb && !Frame.bPaused)
	{
		Raise(TEXT("player.ward.raise"), Frame.WardHandCm, Frame.PlayerId, -1, Now, Out);
	}
	bWasAbsorb = bAbsorb;

	if (Player)
	{
		if (Player->Water.Crests > LastCrests)
		{
			Raise(TEXT("player.crest"), Frame.CuffCm, Frame.PlayerId, -1, Now, Out);
		}
		if (Player->Water.Flow > LastFlow)
		{
			Raise(TEXT("player.flow"), Frame.WardHandCm, Frame.PlayerId, -1, Now, Out);
		}
		LastCrests = Player->Water.Crests;
		LastFlow = Player->Water.Flow;
	}

	if (Frame.SessionCues)
	{
		const TArray<FSessionCue>& Cues = *Frame.SessionCues;
		if (Cues.Num() < SessionCursor)
		{
			SessionCursor = 0;
		}
		for (int32 Index = SessionCursor; Index < Cues.Num(); ++Index)
		{
			if (Cues[Index].Kind == TEXT("sigil-complete"))
			{
				Raise(TEXT("sigil.complete"), Frame.CuffCm, Frame.PlayerId, -1, Now, Out);
			}
			else if (Cues[Index].Kind == TEXT("sigil-reject"))
			{
				Raise(TEXT("sigil.reject"), Frame.CuffCm, Frame.PlayerId, -1, Now, Out);
			}
		}
		SessionCursor = Cues.Num();
	}

	// New projectiles. A fire bolt is heard from where it is and its voice follows it.
	TSet<int32> LiveProjectiles;
	for (const FProjectile& Projectile : State.Projectiles)
	{
		LiveProjectiles.Add(Projectile.Id);
		const FVector At = ToWorld(Frame, Projectile.Pos, ProjectileHeightCm);
		Follow.Add(Projectile.Id, At);
		if (SeenProjectiles.Contains(Projectile.Id))
		{
			continue;
		}
		SeenProjectiles.Add(Projectile.Id);
		if (Projectile.OwnerId != Frame.PlayerId && Projectile.Family == TEXT("magic") && IsFireOwner(State, Projectile.OwnerId))
		{
			Raise(TEXT("threat.fire.projectile"), At, Projectile.OwnerId, Projectile.Id, Now, Out);
		}
	}
	SeenProjectiles = SeenProjectiles.Intersect(LiveProjectiles);

	// New telegraphs: the leave cue at the threat, the hound's snarl at the hound.
	TSet<int32> LiveTelegraphs;
	for (const FTelegraph& Telegraph : State.Telegraphs)
	{
		LiveTelegraphs.Add(Telegraph.Id);
		if (SeenTelegraphs.Contains(Telegraph.Id))
		{
			continue;
		}
		SeenTelegraphs.Add(Telegraph.Id);
		if (Telegraph.OwnerId == Frame.PlayerId)
		{
			continue;
		}
		if (Telegraph.Family == TEXT("unblockable"))
		{
			if (!SeenPendings.Contains(Telegraph.ActivationId))
			{
				SeenPendings.Add(Telegraph.ActivationId);
				FSimVec Where = Telegraph.Origin;
				if (Telegraph.Kind == TEXT("lane"))
				{
					Where = FSimVec{(Telegraph.Origin.X + Telegraph.Target.X) * 0.5, (Telegraph.Origin.Y + Telegraph.Target.Y) * 0.5};
				}
				Raise(TEXT("threat.unblockable"), ToWorld(Frame, Where, 20.0), Telegraph.ActivationId, -1, Now, Out);
			}
			continue;
		}
		const FActor* Owner = SimFindActor(State, Telegraph.OwnerId);
		if (Telegraph.Kind == TEXT("projectile") && Telegraph.Family == TEXT("magic") && Owner && Owner->Enemy.IsSet() && Frame.Rules
			&& Frame.Rules->bActive && Frame.Rules->FindSpit(Owner->Enemy->Id))
		{
			Raise(TEXT("threat.hound.windup"), ToWorld(Frame, Owner->Pos, HoundMouthCm), Owner->Id, -1, Now, Out);
		}
	}
	SeenTelegraphs = SeenTelegraphs.Intersect(LiveTelegraphs);

	// An opponent's pending cast of an unblockable spell is drawn before any telegraph exists (the Tempest Lance line,
	// the Leviathan Orb lane): the leave cue starts with the drawing, once per activation.
	for (const FActor& Actor : State.Actors)
	{
		if (Actor.Id == Frame.PlayerId || Actor.bDown || !Actor.Pending.IsSet())
		{
			continue;
		}
		const FPendingCast& Pending = Actor.Pending.GetValue();
		if (SeenPendings.Contains(Pending.ActivationId) || PendingFamily(Pending) != TEXT("unblockable"))
		{
			continue;
		}
		SeenPendings.Add(Pending.ActivationId);
		Raise(TEXT("threat.unblockable"), ToWorld(Frame, Actor.Pos, ProjectileHeightCm), Pending.ActivationId, -1, Now, Out);
	}

	int32 PerfectTick = -1;
	for (int32 Index = EventCursor; Index < State.Events.Num(); ++Index)
	{
		const FArenaEvent& Event = State.Events[Index];
		const FActor* Actor = SimFindActor(State, Event.ActorId);
		if (Event.Kind == TEXT("cast"))
		{
			if (Event.ActorId == Frame.PlayerId)
			{
				const bool bOrb = Player && Player->Water.LastLine == TEXT("tide_orb");
				Raise(bOrb ? TEXT("player.cast.tide-orb") : TEXT("player.cast.bolt"), Frame.CuffCm, Event.ActorId, -1, Now, Out);
			}
			else if (Actor && !Actor->Enemy.IsSet() && !Actor->bDummy)
			{
				const TOptional<EElement> Element = ActorElement(State, Event.ActorId);
				if (Element.IsSet() && Element.GetValue() == EElement::Water)
				{
					Raise(TEXT("opponent.cast.water"), ToWorld(Frame, Actor->Pos, ProjectileHeightCm), Event.ActorId, -1, Now, Out);
				}
			}
		}
		else if (Event.Kind == TEXT("roll") && Event.ActorId == Frame.PlayerId && Actor)
		{
			Raise(TEXT("player.blink"), ToWorld(Frame, Actor->Pos, BodyHeightCm), Event.ActorId, -1, Now, Out);
		}
		else if (Event.Kind == TEXT("airform") && Actor)
		{
			Raise(TEXT("opponent.airform"), ToWorld(Frame, Actor->Pos, BodyHeightCm), Event.ActorId, -1, Now, Out);
		}
		else if (Event.Kind == TEXT("perfect") && Event.ActorId == Frame.PlayerId)
		{
			PerfectTick = Event.Tick;
			Raise(TEXT("player.perfect"), Frame.WardHandCm, Event.ActorId, -1, Now, Out);
		}
		else if (Event.Kind == TEXT("hit"))
		{
			const int32 OwnerId = Event.TargetId.Get(-1);
			bool bAbsorbed = false;
			if (Event.ActorId == Frame.PlayerId)
			{
				// ResolveHit emits the perfect before the hit of the same resolve: that hit is already heard.
				if (PerfectTick == Event.Tick)
				{
					bAbsorbed = true;
				}
				else
				{
					bAbsorbed = bAbsorb || Frame.bPlanted;
					const FVector At = bAbsorbed ? Frame.WardHandCm : (Actor ? ToWorld(Frame, Actor->Pos, BodyHeightCm) : Frame.ListenerCm);
					Raise(bAbsorbed ? TEXT("player.ward.absorb") : TEXT("player.hit"), At, OwnerId, -1, Now, Out);
				}
			}
			if (Actor && OwnerId != Frame.PlayerId && !bAbsorbed && IsFireOwner(State, OwnerId))
			{
				Raise(TEXT("threat.fire.impact"), ToWorld(Frame, Actor->Pos, BodyHeightCm), Event.ActorId, -1, Now, Out);
			}
		}
		else if (Event.Kind == TEXT("unlock") && Event.ActorId == Frame.PlayerId)
		{
			Raise(TEXT("player.unlock"), Frame.CuffCm, Event.ActorId, -1, Now, Out);
		}
		else if (Event.Kind == TEXT("phase"))
		{
			Raise(TEXT("rival.phase"), Frame.CuffCm, Event.ActorId, -1, Now, Out);
			const FAudioEventSpec* Spec = Table->FindEvent(TEXT("crowd.swell"));
			if (Spec && Table->CrowdSwellS > 0.0)
			{
				FAudioPlayRequest Swell;
				Swell.Kind = EAudioRequestKind::Swell;
				Swell.Cue = Spec->Cue;
				Swell.Event = TEXT("crowd.swell");
				Swell.LocationCm = Frame.ListenerCm;
				Swell.SourceId = Event.ActorId;
				Swell.AtS = Now;
				Swell.GainDb = Table->CrowdSwellDb;
				Out.Add(Swell);
			}
		}
	}
	EventCursor = State.Events.Num();
	return Out;
}

// ---------------------------------------------------------------------------------------------------------------------
// UArenaAudioComponent

UArenaAudioComponent::UArenaAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

USoundWave* UArenaAudioComponent::WaveFor(FName Cue)
{
	if (TObjectPtr<USoundWave>* Found = Waves.Find(Cue))
	{
		return *Found;
	}
	const FAudioCueSpec* Spec = FAudioTable::Default().FindCue(Cue);
	USoundWave* Wave = Spec ? LoadObject<USoundWave>(nullptr, *Spec->Asset) : nullptr;
	if (!Wave)
	{
		UE_LOG(LogMageArena, Warning, TEXT("audio: no asset for cue %s (%s)"), *Cue.ToString(), Spec ? *Spec->Asset : TEXT("-"));
	}
	Waves.Add(Cue, Wave);
	return Wave;
}

USoundAttenuation* UArenaAudioComponent::AttenuationFor(FName Id)
{
	if (TObjectPtr<USoundAttenuation>* Found = AttenuationObjects.Find(Id))
	{
		return *Found;
	}
	const FAudioAttenuationSpec* Spec = FAudioTable::Default().Attenuations.Find(Id);
	if (!Spec)
	{
		return nullptr;
	}
	USoundAttenuation* Attenuation = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
	const float Inner = static_cast<float>(Spec->InnerCm());
	const float Falloff = static_cast<float>(Spec->FalloffCm);
	Settings.bAttenuate = true;
	Settings.bSpatialize = true;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	Settings.AttenuationShapeExtents = FVector(Inner, 0.0, 0.0);
	Settings.FalloffDistance = FMath::Max(1.f, Falloff - Inner);
	Settings.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
	Settings.bAttenuateWithLPF = true;
	Settings.LPFRadiusMin = Inner;
	Settings.LPFRadiusMax = Falloff;
	Settings.LPFFrequencyAtMin = 20000.f;
	Settings.LPFFrequencyAtMax = static_cast<float>(Spec->FarCutoffHz);
	AttenuationObjects.Add(Id, Attenuation);
	return Attenuation;
}

USoundConcurrency* UArenaAudioComponent::ConcurrencyFor(const FAudioCueSpec& Cue)
{
	if (TObjectPtr<USoundConcurrency>* Found = ConcurrencyObjects.Find(Cue.Id))
	{
		return *Found;
	}
	// The engine-side twin of the director's per-cue limit, so a voice the director let through can never stack past it.
	USoundConcurrency* Concurrency = NewObject<USoundConcurrency>(this);
	Concurrency->Concurrency.MaxCount = Cue.MaxVoices;
	Concurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
	ConcurrencyObjects.Add(Cue.Id, Concurrency);
	return Concurrency;
}

void UArenaAudioComponent::PlayOne(const FAudioPlayRequest& Request)
{
	const FAudioCueSpec* Spec = FAudioTable::Default().FindCue(Request.Cue);
	USoundWave* Wave = WaveFor(Request.Cue);
	if (!Spec || !Wave)
	{
		return;
	}
	const float Volume = FMath::Pow(10.f, static_cast<float>(Request.GainDb) / 20.f);
	UAudioComponent* Voice = nullptr;
	if (Spec->bSpatial)
	{
		Voice = UGameplayStatics::SpawnSoundAtLocation(this, Wave, Request.LocationCm, FRotator::ZeroRotator, Volume, 1.f, 0.f,
			AttenuationFor(Spec->Attenuation), ConcurrencyFor(*Spec), true);
	}
	else
	{
		Voice = UGameplayStatics::SpawnSound2D(this, Wave, Volume, 1.f, 0.f, ConcurrencyFor(*Spec), false, true);
	}
	if (Voice && Request.FollowId >= 0)
	{
		Followers.Add(Request.FollowId, Voice);
	}
}

void UArenaAudioComponent::SetLoop(const FAudioPlayRequest& Request, bool bOn)
{
	if (!bOn)
	{
		if (TObjectPtr<UAudioComponent>* Found = Loops.Find(Request.Cue))
		{
			if (*Found)
			{
				(*Found)->Stop();
				(*Found)->DestroyComponent();
			}
			Loops.Remove(Request.Cue);
		}
		return;
	}
	const FAudioCueSpec* Spec = FAudioTable::Default().FindCue(Request.Cue);
	USoundWave* Wave = WaveFor(Request.Cue);
	if (!Spec || !Wave || Loops.Contains(Request.Cue))
	{
		return;
	}
	const float Volume = FMath::Pow(10.f, static_cast<float>(Request.GainDb) / 20.f);
	UAudioComponent* Voice = Spec->bSpatial
		? UGameplayStatics::SpawnSoundAtLocation(this, Wave, Request.LocationCm, FRotator::ZeroRotator, Volume, 1.f, 0.f,
			AttenuationFor(Spec->Attenuation), ConcurrencyFor(*Spec), false)
		: UGameplayStatics::SpawnSound2D(this, Wave, Volume, 1.f, 0.f, ConcurrencyFor(*Spec), false, false);
	if (Voice)
	{
		Loops.Add(Request.Cue, Voice);
	}
}

void UArenaAudioComponent::Handle(const TArray<FAudioPlayRequest>& Requests, double NowS)
{
	if (bRecordOnly)
	{
		Recorded.Append(Requests);
		return;
	}
	if (bMuted)
	{
		return;
	}
	for (const FAudioPlayRequest& Request : Requests)
	{
		switch (Request.Kind)
		{
		case EAudioRequestKind::OneShot:
			PlayOne(Request);
			break;
		case EAudioRequestKind::LoopStart:
			SetLoop(Request, true);
			break;
		case EAudioRequestKind::LoopStop:
			SetLoop(Request, false);
			break;
		case EAudioRequestKind::Swell:
			SwellDb = Request.GainDb;
			SwellUntil = NowS + FAudioTable::Default().CrowdSwellS;
			break;
		default:
			break;
		}
	}
}

bool UArenaAudioComponent::ShouldMute(const UWorld* World)
{
	const TCHAR* Command = FCommandLine::Get();
	return FParse::Param(Command, TEXT("nullrhi")) || FParse::Param(Command, TEXT("nosound")) || GIsAutomationTesting
		|| !FApp::CanEverRenderAudio() || !World || !World->GetAudioDeviceRaw();
}

void UArenaAudioComponent::Sync(const FArenaSession& Session, bool bPaused, const FVector& CameraCm, const FRotator& CameraRot)
{
	const FAudioTable& Table = FAudioTable::Default();
	if (!Table.bLoaded)
	{
		return;
	}
	if (!Director)
	{
		Director = MakeUnique<FArenaAudioDirector>(Table);
		// Load every cue now: a first play that loads its wave synchronously is a hitch in the middle of a bout.
		for (const TPair<FName, FAudioCueSpec>& Pair : Table.Cues)
		{
			WaveFor(Pair.Key);
		}
	}
	UWorld* World = GetWorld();
	if (!bMuteDecided)
	{
		bMuteDecided = true;
		bMuted = ShouldMute(World);
		UE_LOG(LogMageArena, Log, TEXT("audio: %s"), bMuted ? TEXT("muted (nullrhi, nosound, automation or no device)") : TEXT("on"));
	}
	const FGames& Games = Session.GetGames();
	const double NowS = World ? World->GetRealTimeSeconds() : FPlatformTime::Seconds();
	FAudioFrame Frame;
	Frame.State = &Games.State;
	Frame.Rules = Session.GetVrRules();
	Frame.PlayerId = Games.PlayerId;
	Frame.NowS = NowS;
	Frame.bActive = Games.Phase == TEXT("active");
	Frame.bPaused = bPaused;
	Frame.bSigilDrawing = Session.IsSigilDrawing();
	Frame.bPlanted = Session.IsStaffPlanted();
	Frame.SessionCues = &Session.GetSessionCues();
	Frame.ToCm = [&Session](double X, double Y) { return Session.KernelToUnrealCm(FSimVec{X, Y}); };
	Frame.CuffCm = CameraCm + CameraRot.RotateVector(CuffOffsetCm);
	Frame.WardHandCm = CameraCm + CameraRot.RotateVector(WardHandOffsetCm);
	Frame.ListenerCm = CameraCm;
	const TArray<FAudioPlayRequest> Requests = Director->Step(Frame);
	const double SimS = Session.GetSimSeconds();
	for (const FAudioPlayRequest& Request : Requests)
	{
		FAudioPlayRequest Logged = Request;
		Logged.AtS = SimS;
		if (History.Num() < 8192)
		{
			History.Add(Logged);
		}
		const FVector Rel = Request.LocationCm - CameraCm;
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_AUDIO sim=%.3f kind=%d cue=%s event=%s source=%d dist=%.0fcm rel=(%.0f,%.0f,%.0f) gain=%.1f"), SimS,
			static_cast<int32>(Request.Kind), *Request.Cue.ToString(), *Request.Event, Request.SourceId, Rel.Size(), Rel.X, Rel.Y, Rel.Z, Request.GainDb);
	}
	Handle(Requests, NowS);
	if (bMuted || bRecordOnly)
	{
		return;
	}
	for (auto It = Followers.CreateIterator(); It; ++It)
	{
		UAudioComponent* Voice = It.Value();
		if (!Voice || !Voice->IsPlaying())
		{
			It.RemoveCurrent();
			continue;
		}
		if (const FVector* At = Director->GetFollow().Find(It.Key()))
		{
			Voice->SetWorldLocation(*At);
		}
	}
	// Loops at the hands follow the camera (a blink moves the seat); the crowd's volume carries the swell.
	for (TPair<FName, TObjectPtr<UAudioComponent>>& Pair : Loops)
	{
		UAudioComponent* Voice = Pair.Value;
		if (!Voice)
		{
			continue;
		}
		const FAudioEventSpec* Hold = Table.FindEvent(TEXT("player.ward.hold"));
		const FAudioEventSpec* Draw = Table.FindEvent(TEXT("sigil.draw"));
		const FAudioEventSpec* Crowd = Table.FindEvent(TEXT("bout.crowd"));
		if (Hold && Pair.Key == Hold->Cue)
		{
			Voice->SetWorldLocation(Frame.WardHandCm);
		}
		else if (Draw && Pair.Key == Draw->Cue)
		{
			Voice->SetWorldLocation(Frame.CuffCm);
		}
		if (Crowd && Pair.Key == Crowd->Cue)
		{
			const FAudioCueSpec* Spec = Table.FindCue(Pair.Key);
			const double Db = (Spec ? Spec->GainDb : 0.0) + Table.MasterDb + (NowS < SwellUntil ? SwellDb : 0.0);
			Voice->SetVolumeMultiplier(FMath::Pow(10.f, static_cast<float>(Db) / 20.f));
		}
	}
}
