#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/Air.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Enemies.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "Kernel/VrRules.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Session/ArenaAudio.h"
#include "Sound/SoundWave.h"

// T26, the first audio. The director (FArenaAudioDirector) is the test seam: it turns a frame of the bout into play
// requests and plays nothing, so every mapping in apps/vr/data/vr/audio.json is checked here under -nullrhi, where the
// component itself is muted. Positions: the harness maps kernel metres to centimetres as x100 with no surface height,
// so a source at kernel (X, Y) is expected at (100 X, 100 Y, lift): projectiles +120 cm, bodies +110, a hound's mouth
// +30, a telegraph +20, as ArenaAudio.cpp places them.

namespace
{
const FVector Cuff(11.0, -22.0, 33.0);
const FVector WardHand(44.0, 55.0, 66.0);
const FVector Listener(1.0, 2.0, 3.0);

bool LoadTable(FAutomationTestBase& Test, FAudioTable& Table)
{
	FString Error;
	if (!Table.LoadFromFile(FAudioTable::DefaultPath(), Error))
	{
		Test.AddError(Error);
		return false;
	}
	return true;
}

FVector At(const FSimVec& Pos, double LiftCm)
{
	return FVector(Pos.X * 100.0, Pos.Y * 100.0, LiftCm);
}

struct FHarness
{
	FArenaState State;
	FVrRuleset Rules;
	bool bRules = false;
	TArray<FSessionCue> Cues;
	int32 Player = 0;
	int32 WaterMage = 0;
	int32 FireMage = 0;
	int32 AirMage = 0;
	int32 Hound = 0;
	double Now = 10.0;
	bool bPaused = false;
	bool bDrawing = false;

	FHarness()
	{
		State = CreateArena(26);
		const FSimVec Centre = KernelData().ArenaCentre;
		Player = AddMage(State, 0, Centre, TEXT("Player")).Id;
		WaterMage = AddMage(State, 1, FSimVec{Centre.X + 8.0, Centre.Y}, TEXT("Water mage")).Id;
		FActor& Fire = AddMage(State, 1, FSimVec{Centre.X + 9.0, Centre.Y + 3.0}, TEXT("Brennic"));
		Fire.Fire.bSchool = true;
		FireMage = Fire.Id;
		FActor& Air = AddMage(State, 1, FSimVec{Centre.X + 9.0, Centre.Y - 3.0}, TEXT("Lio"));
		Air.Air.bSchool = true;
		AirMage = Air.Id;
		Hound = AddEnemy(State, TEXT("cinder_hound"), FSimVec{Centre.X + 6.0, Centre.Y + 5.0}).Id;
		FString Error;
		bRules = LoadVrRuleset(Rules, Error, false);
	}

	FActor& Actor(int32 Id) { return *SimFindActor(State, Id); }

	FAudioFrame Frame()
	{
		FAudioFrame Out;
		Out.State = &State;
		Out.Rules = bRules ? &Rules : nullptr;
		Out.PlayerId = Player;
		Out.NowS = Now;
		Out.bActive = true;
		Out.bPaused = bPaused;
		Out.bSigilDrawing = bDrawing;
		Out.SessionCues = &Cues;
		Out.CuffCm = Cuff;
		Out.WardHandCm = WardHand;
		Out.ListenerCm = Listener;
		return Out;
	}

	void Emit(const TCHAR* Kind, int32 ActorId, double Value = 0.0, TOptional<int32> Target = {})
	{
		FArenaEvent Event;
		Event.Tick = State.Tick;
		Event.Kind = Kind;
		Event.ActorId = ActorId;
		Event.Value = Value;
		Event.TargetId = Target;
		State.Events.Add(Event);
	}
};

int32 OneShots(const TArray<FAudioPlayRequest>& Requests)
{
	int32 Count = 0;
	for (const FAudioPlayRequest& Request : Requests)
	{
		Count += Request.Kind == EAudioRequestKind::OneShot ? 1 : 0;
	}
	return Count;
}

// Exactly one request raised by Event, with the table's cue for it and the expected position and kind.
bool ExpectOne(FAutomationTestBase& Test, const FAudioTable& Table, const TArray<FAudioPlayRequest>& Requests, const FString& Event,
	const FVector& Where, EAudioRequestKind Kind = EAudioRequestKind::OneShot)
{
	const FAudioEventSpec* Spec = Table.FindEvent(Event);
	if (!Spec)
	{
		Test.AddError(FString::Printf(TEXT("%s is not mapped in audio.json"), *Event));
		return false;
	}
	TArray<const FAudioPlayRequest*> Hits;
	for (const FAudioPlayRequest& Request : Requests)
	{
		if (Request.Event == Event && Request.Kind == Kind)
		{
			Hits.Add(&Request);
		}
	}
	if (Hits.Num() != 1)
	{
		Test.AddError(FString::Printf(TEXT("%s: expected exactly one request, got %d"), *Event, Hits.Num()));
		return false;
	}
	bool bOk = Test.TestEqual(*FString::Printf(TEXT("%s cue"), *Event), Hits[0]->Cue, Spec->Cue);
	if (!Hits[0]->LocationCm.Equals(Where, 0.01))
	{
		Test.AddError(FString::Printf(TEXT("%s: expected at %s got %s"), *Event, *Where.ToString(), *Hits[0]->LocationCm.ToString()));
		bOk = false;
	}
	return bOk;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAudioAssets, "MageArena.Audio.Assets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAudioAssets::RunTest(const FString& Parameters)
{
	FAudioTable Table;
	if (!LoadTable(*this, Table))
	{
		return false;
	}
	bool bPass = TestEqual(TEXT("20 cues (the whole A05 pack)"), Table.Cues.Num(), 20);
	bPass &= TestEqual(TEXT("every known event is mapped"), Table.Events.Num(), FAudioTable::KnownEvents().Num());
	for (const TPair<FName, FAudioCueSpec>& Pair : Table.Cues)
	{
		const FAudioCueSpec& Cue = Pair.Value;
		USoundWave* Wave = LoadObject<USoundWave>(nullptr, *Cue.Asset);
		if (!TestNotNull(*FString::Printf(TEXT("%s asset %s"), *Cue.Id.ToString(), *Cue.Asset), Wave))
		{
			bPass = false;
			continue;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s loop flag"), *Cue.Id.ToString()), static_cast<bool>(Wave->bLooping), Cue.bLoop);
		bPass &= TestTrue(*FString::Printf(TEXT("%s gain at or under 0 dB"), *Cue.Id.ToString()), Cue.GainDb <= 0.0);
		bPass &= TestTrue(*FString::Printf(TEXT("%s has a source WAV"), *Cue.Id.ToString()),
			FPaths::FileExists(FPaths::Combine(FPaths::ProjectDir(), TEXT("../art/audio/sfx"), Cue.Id.ToString() + TEXT(".wav"))));
	}
	// Strictness: an unknown event name and a missing cue are load errors, not silent gaps.
	FAudioTable Bad;
	FString Error;
	bPass &= TestFalse(TEXT("unknown event rejected"), Bad.LoadFromString(TEXT(R"({"attenuations":{"a":{"falloffCm":100}},"cues":{"x":{"gainDb":-1,"attenuation":"a"}},"events":[{"event":"no.such","cue":"x","at":"cuff"}]})"), Error));
	bPass &= TestFalse(TEXT("missing cue rejected"), Bad.LoadFromString(TEXT(R"({"attenuations":{"a":{"falloffCm":100}},"cues":{"x":{"gainDb":-1,"attenuation":"a"}},"events":[{"event":"ui.pause","cue":"y","at":"cuff"}]})"), Error));
	bPass &= TestFalse(TEXT("positive gain rejected"), Bad.LoadFromString(TEXT(R"({"attenuations":{"a":{"falloffCm":100}},"cues":{"x":{"gainDb":2,"attenuation":"a"}},"events":[]})"), Error));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAudioEventMap, "MageArena.Audio.EventMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAudioEventMap::RunTest(const FString& Parameters)
{
	FAudioTable Table;
	if (!LoadTable(*this, Table) || !KernelData().bReady)
	{
		AddError(TEXT("audio.json or kernel data failed to load"));
		return false;
	}
	bool bPass = true;
	TSet<FString> Covered;
	// Each case: a fresh harness and director, one primed frame (the bout's loops start there), then one change and
	// one frame. The change must raise exactly one request of its event (and no other one-shot unless stated).
	auto Run = [&](const FString& Event, TFunctionRef<void(FHarness&)> Setup, TFunctionRef<void(FHarness&)> Change,
		TFunctionRef<FVector(FHarness&)> Where, EAudioRequestKind Kind = EAudioRequestKind::OneShot, int32 ExpectOneShots = 1)
	{
		Covered.Add(Event);
		FHarness H;
		Setup(H);
		FArenaAudioDirector Director(Table);
		Director.Step(H.Frame());
		H.Now += 2.0;
		H.State.Tick += 1;
		Change(H);
		const TArray<FAudioPlayRequest> Requests = Director.Step(H.Frame());
		bool bOk = ExpectOne(*this, Table, Requests, Event, Where(H), Kind);
		bOk &= TestEqual(*FString::Printf(TEXT("%s one-shots this frame"), *Event), OneShots(Requests), ExpectOneShots);
		bPass &= bOk;
	};
	auto Nothing = [](FHarness&) {};
	auto PlayerBody = [](FHarness& H) { return At(H.Actor(H.Player).Pos, 110.0); };

	Run(TEXT("threat.unblockable"), Nothing,
		[](FHarness& H)
		{
			FTelegraph Telegraph;
			Telegraph.Id = 900;
			Telegraph.ActivationId = 901;
			Telegraph.OwnerId = H.FireMage;
			Telegraph.Family = TEXT("unblockable");
			Telegraph.Kind = TEXT("area");
			Telegraph.Origin = FSimVec{3.0, 4.0};
			Telegraph.Target = FSimVec{3.0, 4.0};
			H.State.Telegraphs.Add(Telegraph);
		},
		[](FHarness&) { return FVector(300.0, 400.0, 20.0); });
	Run(TEXT("threat.fire.projectile"), Nothing,
		[](FHarness& H)
		{
			FHit Hit;
			Hit.OwnerId = H.FireMage;
			Hit.Family = TEXT("magic");
			FProjectile& Shot = SpawnProjectile(H.State, Hit, FSimVec{5.0, 1.0}, FSimVec{-1.0, 0.0}, 12.0, 20.0);
			(void)Shot;
		},
		[](FHarness& H) { return At(H.State.Projectiles.Last().Pos, 120.0); });
	if (TestTrue(TEXT("VR overlay loads (hound spit)"), FHarness().bRules))
	{
		Run(TEXT("threat.hound.windup"), Nothing,
			[](FHarness& H)
			{
				FTelegraph Telegraph;
				Telegraph.Id = 910;
				Telegraph.ActivationId = 911;
				Telegraph.OwnerId = H.Hound;
				Telegraph.Family = TEXT("magic");
				Telegraph.Kind = TEXT("projectile");
				Telegraph.Origin = H.Actor(H.Hound).Pos;
				Telegraph.Target = H.Actor(H.Player).Pos;
				H.State.Telegraphs.Add(Telegraph);
			},
			[](FHarness& H) { return At(H.Actor(H.Hound).Pos, 30.0); });
	}
	Run(TEXT("threat.fire.impact"), Nothing,
		[](FHarness& H) { H.Emit(TEXT("hit"), H.WaterMage, 9.0, H.FireMage); },
		[](FHarness& H) { return At(H.Actor(H.WaterMage).Pos, 110.0); });
	Run(TEXT("player.hit"), Nothing,
		[](FHarness& H) { H.Emit(TEXT("hit"), H.Player, 7.0, H.WaterMage); },
		PlayerBody);
	Run(TEXT("player.ward.absorb"), [](FHarness& H) { H.Actor(H.Player).bAbsorb = true; },
		[](FHarness& H) { H.Emit(TEXT("hit"), H.Player, 1.0, H.WaterMage); },
		[](FHarness&) { return WardHand; });
	Run(TEXT("player.perfect"), [](FHarness& H) { H.Actor(H.Player).bAbsorb = true; },
		[](FHarness& H)
		{
			H.Emit(TEXT("perfect"), H.Player, 5.0, H.FireMage);
			H.Emit(TEXT("hit"), H.Player, 0.0, H.FireMage);
		},
		[](FHarness&) { return WardHand; });
	Run(TEXT("player.ward.raise"), Nothing, [](FHarness& H) { H.Actor(H.Player).bAbsorb = true; }, [](FHarness&) { return WardHand; });
	Run(TEXT("player.ward.hold"), Nothing, [](FHarness& H) { H.Actor(H.Player).bAbsorb = true; }, [](FHarness&) { return WardHand; },
		EAudioRequestKind::LoopStart);
	Run(TEXT("player.blink"), Nothing, [](FHarness& H) { H.Emit(TEXT("roll"), H.Player); }, PlayerBody);
	Run(TEXT("player.cast.bolt"), Nothing,
		[](FHarness& H)
		{
			H.Actor(H.Player).Water.LastLine = TEXT("lash");
			H.Emit(TEXT("cast"), H.Player, 1.0);
		},
		[](FHarness&) { return Cuff; });
	Run(TEXT("player.cast.tide-orb"), Nothing,
		[](FHarness& H)
		{
			H.Actor(H.Player).Water.LastLine = TEXT("tide_orb");
			H.Emit(TEXT("cast"), H.Player, 1.0);
		},
		[](FHarness&) { return Cuff; });
	Run(TEXT("player.crest"), Nothing, [](FHarness& H) { H.Actor(H.Player).Water.Crests++; }, [](FHarness&) { return Cuff; });
	Run(TEXT("player.flow"), Nothing, [](FHarness& H) { H.Actor(H.Player).Water.Flow++; }, [](FHarness&) { return WardHand; });
	Run(TEXT("player.unlock"), Nothing, [](FHarness& H) { H.Emit(TEXT("unlock"), H.Player, 2.0); }, [](FHarness&) { return Cuff; });
	Run(TEXT("rival.phase"), Nothing, [](FHarness& H) { H.Emit(TEXT("phase"), H.FireMage, 2.0, 0); }, [](FHarness&) { return Cuff; });
	Run(TEXT("crowd.swell"), Nothing, [](FHarness& H) { H.Emit(TEXT("phase"), H.FireMage, 2.0, 0); }, [](FHarness&) { return Listener; },
		EAudioRequestKind::Swell);
	Run(TEXT("sigil.complete"), Nothing, [](FHarness& H) { H.Cues.Add(FSessionCue{TEXT("sigil-complete"), 1.0}); }, [](FHarness&) { return Cuff; });
	Run(TEXT("sigil.reject"), Nothing, [](FHarness& H) { H.Cues.Add(FSessionCue{TEXT("sigil-reject"), 1.0}); }, [](FHarness&) { return Cuff; });
	Run(TEXT("sigil.draw"), Nothing, [](FHarness& H) { H.bDrawing = true; }, [](FHarness&) { return Cuff; }, EAudioRequestKind::LoopStart, 0);
	Run(TEXT("ui.pause"), Nothing, [](FHarness& H) { H.bPaused = true; }, [](FHarness&) { return Cuff; });
	Run(TEXT("opponent.cast.water"), Nothing, [](FHarness& H) { H.Emit(TEXT("cast"), H.WaterMage, 1.0); },
		[](FHarness& H) { return At(H.Actor(H.WaterMage).Pos, 120.0); });
	Run(TEXT("opponent.airform"), Nothing, [](FHarness& H) { H.Emit(TEXT("airform"), H.AirMage, 3.0); },
		[](FHarness& H) { return At(H.Actor(H.AirMage).Pos, 110.0); });

	// The crowd bed starts with the bout: the first frame of an active bout.
	{
		Covered.Add(TEXT("bout.crowd"));
		FHarness H;
		FArenaAudioDirector Director(Table);
		bPass &= ExpectOne(*this, Table, Director.Step(H.Frame()), TEXT("bout.crowd"), Listener, EAudioRequestKind::LoopStart);
	}
	for (const FString& Event : FAudioTable::KnownEvents())
	{
		bPass &= TestTrue(*FString::Printf(TEXT("%s has a case here"), *Event), Covered.Contains(Event));
	}

	// A fire hit on an unwarded player is heard twice on purpose (the body thud and the fire); a warded one only as
	// the absorb (the ward swallowed the fire).
	{
		FHarness H;
		FArenaAudioDirector Director(Table);
		Director.Step(H.Frame());
		H.Now += 2.0;
		H.Emit(TEXT("hit"), H.Player, 7.0, H.FireMage);
		const TArray<FAudioPlayRequest> Open = Director.Step(H.Frame());
		bPass &= ExpectOne(*this, Table, Open, TEXT("player.hit"), At(H.Actor(H.Player).Pos, 110.0));
		bPass &= ExpectOne(*this, Table, Open, TEXT("threat.fire.impact"), At(H.Actor(H.Player).Pos, 110.0));
		H.Actor(H.Player).bAbsorb = true;
		H.Now += 2.0;
		H.Emit(TEXT("hit"), H.Player, 1.0, H.FireMage);
		const TArray<FAudioPlayRequest> Warded = Director.Step(H.Frame());
		bPass &= ExpectOne(*this, Table, Warded, TEXT("player.ward.absorb"), WardHand);
		bPass &= TestFalse(TEXT("no fire impact through the ward"),
			Warded.ContainsByPredicate([](const FAudioPlayRequest& Request) { return Request.Event == TEXT("threat.fire.impact"); }));
	}

	// The record seam on the component: requests are kept, nothing is played, and audio is muted in this process.
	{
		UArenaAudioComponent* Component = NewObject<UArenaAudioComponent>();
		Component->SetRecordOnly(true);
		FAudioPlayRequest Request;
		Request.Cue = TEXT("blink");
		Request.Event = TEXT("player.blink");
		Component->Handle({Request, Request}, 1.0);
		bPass &= TestEqual(TEXT("record seam keeps the requests"), Component->GetRecorded().Num(), 2);
		bPass &= TestTrue(TEXT("muted in automation and under -nullrhi"), UArenaAudioComponent::ShouldMute(nullptr));
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAudioSquallCap, "MageArena.Audio.SquallCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAudioSquallCap::RunTest(const FString& Parameters)
{
	FAudioTable Table;
	if (!LoadTable(*this, Table) || !KernelData().bReady)
	{
		AddError(TEXT("audio.json or kernel data failed to load"));
		return false;
	}
	// Lio's Squall (spells-air.csv air_squall: four orbs of 11, 0.25 s apart, 14 m/s) at a player 6 m away with the ward
	// up, on the real kernel. Every orb that lands is a hit on the warded player, so each one asks for ward-absorb;
	// ward-absorb is capped at its maxVoices for its 0.8 s length.
	FArenaState State = CreateArena(26);
	const FSimVec Centre = KernelData().ArenaCentre;
	FActor& PlayerActor = AddMage(State, 0, Centre, TEXT("Player"));
	const int32 Player = PlayerActor.Id;
	PlayerActor.MaxHp = PlayerActor.Hp = 1000.0;
	PlayerActor.MaxMana = PlayerActor.Mana = 1000.0;
	FActor& Lio = AddMage(State, 1, FSimVec{Centre.X + 6.0, Centre.Y}, TEXT("Lio"));
	Lio.Air.bSchool = true;
	Lio.Tier = 4;
	Lio.MaxMana = Lio.Mana = 500.0;
	const int32 LioId = Lio.Id;
	const FSimVec LioPos = Lio.Pos;
	if (!StartGrantedAirCast(State, Lio, TEXT("air_squall"), Centre, nullptr))
	{
		AddError(TEXT("the Squall did not start"));
		return false;
	}
	const FAudioCueSpec* Absorb = Table.FindCue(TEXT("ward-absorb"));
	if (!TestNotNull(TEXT("ward-absorb cue"), Absorb))
	{
		return false;
	}
	FArenaAudioDirector Director(Table);
	TArray<FSessionCue> Cues;
	int32 Hits = 0;
	int32 Requests = 0;
	int32 PeakAbsorb = 0;
	int32 PeakTotal = 0;
	int32 EventCursor = State.Events.Num();
	for (int32 Step = 0; Step < SimTicks(3.0); ++Step)
	{
		FInputFrame Guard = SimIdleInput(LioPos);
		Guard.bAbsorb = true;
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Player, Guard);
		Inputs.Add(LioId, SimIdleInput(Centre));
		StepArena(State, Inputs);
		for (int32 Index = EventCursor; Index < State.Events.Num(); ++Index)
		{
			Hits += (State.Events[Index].Kind == TEXT("hit") && State.Events[Index].ActorId == Player) ? 1 : 0;
		}
		EventCursor = State.Events.Num();
		FAudioFrame Frame;
		Frame.State = &State;
		Frame.PlayerId = Player;
		Frame.NowS = SimSeconds(State.Tick);
		Frame.bActive = true;
		Frame.SessionCues = &Cues;
		for (const FAudioPlayRequest& Request : Director.Step(Frame))
		{
			Requests += Request.Event == TEXT("player.ward.absorb") ? 1 : 0;
		}
		PeakAbsorb = FMath::Max(PeakAbsorb, Director.LiveVoices(Absorb->Id, Frame.NowS));
		PeakTotal = FMath::Max(PeakTotal, Director.LiveVoicesTotal(Frame.NowS));
	}
	AddInfo(FString::Printf(TEXT("squall: %d hits on the warded player, %d ward-absorb requests, peak %d ward-absorb voices, peak %d total, %d dropped"),
		Hits, Requests, PeakAbsorb, PeakTotal, Director.GetDropped()));
	bool bPass = TestEqual(TEXT("all four orbs land on the ward"), Hits, 4);
	bPass &= TestTrue(TEXT("ward-absorb voices never above its maxVoices"), PeakAbsorb <= Absorb->MaxVoices);
	bPass &= TestTrue(TEXT("one-shot voices never above the global maxVoices"), PeakTotal <= Table.MaxVoices);
	bPass &= TestTrue(TEXT("the cap dropped at least one orb's absorb"), Requests < Hits && Director.GetDropped() > 0);
	bPass &= TestTrue(TEXT("but the volley is heard"), Requests >= 1);
	return bPass;
}

#endif
