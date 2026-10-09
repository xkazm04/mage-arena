#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/KernelData.h"
#include "Kernel/VrRules.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"
#include "Session/PlayerPreset.h"

#include <cmath>

// T20. Sources for the literals:
// - runtime.json water.presets Rotation: lines tide_orb, lash, mirror; branches lash B, mirror B, tide_orb B.
//   games.referencePreset "Rotation"; games.opponentPresets "Undertow", "Mirror tide".
// - apps/vr/data/vr/presets.json cassia: Rotation with mirror A; stonePick left A, right B, default B, timeoutS 20,
//   afterBout 1. teach.json offerHoldS 0.40.
// - spells-water.csv: tide_orb 1 Bubble Shot (projectile 12 m/s, damage 7); tide_orb 3 Twin Tides (12 m/s, 11 each);
//   tide_orb 4 A Leviathan Orb (slow projectile 6 m/s r 1.5 m, cast 0.60, mana 45, damage 40, UNBLOCKABLE,
//   telegraph 1.00, range 14); mirror 2 A Reflection ("only projectiles of tier <= own unlocked tier"); mirror 2 B Ripple
//   (ring r 3 m, damage 7.5).
// - stats.csv rank 1: max HP 80 + 15 = 95, max mana 80 + 15 = 95 (as MageArena.Session.ChainAdvance).

namespace
{
bool Ready(FAutomationTestBase& Test)
{
	if (KernelData().bReady)
	{
		return true;
	}
	Test.AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
	return false;
}

bool Near(FAutomationTestBase& Test, const TCHAR* What, double Actual, double Expected)
{
	const bool bOk = std::abs(Actual - Expected) <= 1.0e-9;
	if (!bOk)
	{
		Test.AddError(FString::Printf(TEXT("%s: expected %.17g got %.17g"), What, Expected, Actual));
	}
	return bOk;
}

const FComposition* Pinned(const TCHAR* Name)
{
	for (const FComposition& Preset : KernelData().Presets)
	{
		if (Preset.Name == Name)
		{
			return &Preset;
		}
	}
	return nullptr;
}

struct FRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;

	bool Open(FAutomationTestBase& Test)
	{
		Instance = NewObject<UGameInstance>(GetTransientPackage());
		Instance->AddToRoot();
		Hands = NewObject<UHandInputSubsystem>(Instance);
		Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
		Sigils->LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
		Wards = NewObject<UWardDetectorSubsystem>(Instance);
		FString Error;
		if (!Wards->InitDetector(Error))
		{
			Test.AddError(Error);
			return false;
		}
		Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
		Staff = NewObject<UStaffDetectorSubsystem>(Instance);
		return true;
	}

	void Bind(FArenaSession& Session)
	{
		Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
	}

	void Close(FArenaSession& Session)
	{
		Session.Unbind();
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
		}
	}
};

FString ScratchDir(const TCHAR* Name)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), Name, FGuid::NewGuid().ToString());
}

// Bout 1 won at once: the enemies are downed in the state, as MageArena.Session.ChainAdvance does.
void WinBout(FArenaSession& Session)
{
	FArenaState& State = const_cast<FArenaState&>(Session.GetGames().State);
	for (FActor& Actor : State.Actors)
	{
		if (Actor.Id != Session.GetGames().PlayerId)
		{
			Actor.bDown = true;
		}
	}
	State.Projectiles.Reset();
	State.Telegraphs.Reset();
	// One second covers the kernel ticks that notice the win (1/72 s is less than one 60 Hz tick).
	Session.Advance(1.0, true);
}

int32 CountChain(const TArray<FString>& Chain, const TCHAR* Piece)
{
	int32 Count = 0;
	for (const FString& Line : Chain)
	{
		Count += Line.Contains(Piece) ? 1 : 0;
	}
	return Count;
}

const FActor* PlayerOf(const FArenaSession& Session)
{
	return SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
}

// A kernel duel of two: the reflector at (10, 10) facing +X and a caster 3.51 m away (mirror-2a's spacing), with a
// magic projectile of the given tier and damage loosed from the caster at 12 m/s (the Tide Orb speed). The reflector
// raises a fresh absorb three ticks before contact, inside the 0.15 s (9 tick) perfect window.
struct FDuel
{
	FArenaState State;
	int32 Reflector = 0;
	int32 Caster = 0;
	int32 PerfectTick = -1;
};

FDuel RunDuel(const FComposition& Composition, int32 ReflectorTier, int32 ProjectileTier, double Damage, double GapM, FVrRuleset* Rules)
{
	FDuel Duel;
	Duel.State = CreateArena(11);
	FActor& Reflector = AddMage(Duel.State, 0, FSimVec{10.0, 10.0});
	Reflector.Water = NewWaterState(&Composition);
	Reflector.Tier = ReflectorTier;
	Reflector.LastUnlockTick = 1000000000;
	Reflector.Facing = FSimVec{1.0, 0.0};
	Duel.Reflector = Reflector.Id;
	FActor& Caster = AddMage(Duel.State, 1, FSimVec{10.0 + GapM, 10.0});
	Caster.Facing = FSimVec{-1.0, 0.0};
	Duel.Caster = Caster.Id;
	FHit Hit;
	Hit.OwnerId = Caster.Id;
	Hit.ActivationId = Duel.State.NextId++;
	Hit.Damage = Damage;
	Hit.Family = TEXT("magic");
	Hit.Tier = ProjectileTier;
	Hit.Source = Caster.Pos;
	const FSimVec CasterPos = Caster.Pos;
	SpawnProjectile(Duel.State, Hit, CasterPos, SimSub(FSimVec{10.0, 10.0}, CasterPos), 12.0, 12.0);
	const double Step = 12.0 * SimDt();
	bool bRaised = false;
	for (int32 Tick = 0; Tick < 120; ++Tick)
	{
		const FActor* Me = SimFindActor(Duel.State, Duel.Reflector);
		for (const FProjectile& Projectile : Duel.State.Projectiles)
		{
			if (Projectile.OwnerId == Duel.Caster && !bRaised)
			{
				const double Gap = SimDistance(Projectile.Pos, Me->Pos) - Me->Radius - Projectile.Radius;
				bRaised = Gap <= 3.0 * Step;
			}
		}
		FInputFrame Input = SimIdleInput(FSimVec{10.0 + GapM, 10.0});
		Input.bAbsorb = bRaised;
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Duel.Reflector, Input);
		StepArena(Duel.State, Inputs, Rules);
		if (Duel.PerfectTick < 0)
		{
			for (const FArenaEvent& Event : Duel.State.Events)
			{
				if (Event.Kind == TEXT("perfect") && Event.ActorId == Duel.Reflector)
				{
					Duel.PerfectTick = Event.Tick;
				}
			}
		}
	}
	return Duel;
}

int32 CountEvents(const FArenaState& State, const TCHAR* Kind)
{
	int32 Count = 0;
	for (const FArenaEvent& Event : State.Events)
	{
		Count += Event.Kind == Kind ? 1 : 0;
	}
	return Count;
}

FComposition Cassia(FAutomationTestBase& Test)
{
	FPlayerPreset Preset;
	FString Error;
	if (!LoadPlayerPreset(Preset, Error))
	{
		Test.AddError(Error);
	}
	return Preset.Composition;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresetPlayerHasReflection, "MageArena.Preset.PlayerHasReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresetPlayerHasReflection::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FPlayerPreset Preset;
	FString Error;
	if (!TestTrue(*FString::Printf(TEXT("presets.json loads (%s)"), *Error), LoadPlayerPreset(Preset, Error)))
	{
		return false;
	}
	const FComposition* Rotation = Pinned(TEXT("Rotation"));
	if (!TestNotNull(TEXT("pinned Rotation"), Rotation))
	{
		return false;
	}
	bool bPass = true;
	bPass &= TestEqual(TEXT("player preset id"), Preset.Id, FString(TEXT("cassia")));
	bPass &= TestEqual(TEXT("based on Rotation"), Preset.BasedOn, FString(TEXT("Rotation")));
	bPass &= TestEqual(TEXT("Rotation lines"), Preset.Composition.Lines, Rotation->Lines);
	bPass &= TestEqual(TEXT("lash stays Rotation's B"), Preset.Composition.Branches.Lash, FString(TEXT("B")));
	bPass &= TestEqual(TEXT("Mirror II is A, Reflection"), Preset.Composition.Branches.Mirror, FString(TEXT("A")));
	bPass &= TestEqual(TEXT("Tide Orb IV default B, Rotation's"), Preset.Composition.Branches.TideOrb, FString(TEXT("B")));
	bPass &= TestEqual(TEXT("pinned Rotation is untouched (mirror B)"), Rotation->Branches.Mirror, FString(TEXT("B")));
	bPass &= TestTrue(TEXT("stone pick on"), Preset.Pick.bEnabled);
	bPass &= TestEqual(TEXT("pick after Bout 1"), Preset.Pick.AfterBout, 1);
	bPass &= TestEqual(TEXT("left stone A"), Preset.Pick.Left, FString(TEXT("A")));
	bPass &= TestEqual(TEXT("right stone B"), Preset.Pick.Right, FString(TEXT("B")));
	bPass &= TestEqual(TEXT("pick default B"), Preset.Pick.Default, FString(TEXT("B")));
	bPass &= Near(*this, TEXT("pick timeout 20 s"), Preset.Pick.TimeoutS, 20.0);

	FRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetSaveDirectory(ScratchDir(TEXT("preset-player")));
	bPass &= TestTrue(TEXT("session starts"), Session.Start(1));
	if (const FActor* Player = PlayerOf(Session))
	{
		bPass &= TestEqual(TEXT("session player is cassia"), Player->Water.Composition.Name, FString(TEXT("cassia")));
		bPass &= TestEqual(TEXT("session player mirror A"), Player->Water.Composition.Branches.Mirror, FString(TEXT("A")));
		bPass &= TestEqual(TEXT("session player tide orb B"), Player->Water.Composition.Branches.TideOrb, FString(TEXT("B")));
		FActor AtTwo = *Player;
		AtTwo.Tier = 2;
		const FSpell* Mirror = SpellFor(AtTwo, 3);
		bPass &= TestTrue(TEXT("slot 3 at tier 2 is Reflection"), Mirror && Mirror->Id == TEXT("mirror:2:A") && Mirror->Effect == TEXT("reflection"));
	}
	else
	{
		AddError(TEXT("no player"));
		bPass = false;
	}
	bPass &= TestEqual(TEXT("start line names the preset"), CountChain(Session.GetChain(), TEXT("preset=cassia mirror=A tideOrbIV=B")), 1);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresetBadFileFailsStart, "MageArena.Preset.BadFileFailsStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresetBadFileFailsStart::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FString Dir = ScratchDir(TEXT("preset-bad"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	FString Good;
	FFileHelper::LoadFileToString(Good, *PlayerPresetPath());
	bool bPass = TestTrue(TEXT("read presets.json"), !Good.IsEmpty());
	struct FCase
	{
		const TCHAR* Name;
		FString Text;
		const TCHAR* Message;
	};
	const FCase Cases[] = {
		{TEXT("missing"), FString(), TEXT("cannot read")},
		{TEXT("not json"), TEXT("{ cassia"), TEXT("is not JSON")},
		{TEXT("mirror C"), Good.Replace(TEXT("\"mirror\": \"A\""), TEXT("\"mirror\": \"C\"")), TEXT("must be \"A\" or \"B\"")},
		{TEXT("unknown basedOn"), Good.Replace(TEXT("\"basedOn\": \"Rotation\""), TEXT("\"basedOn\": \"Nope\"")), TEXT("is not a pinned")},
		{TEXT("timeout as text"), Good.Replace(TEXT("\"timeoutS\": 20"), TEXT("\"timeoutS\": \"20\"")), TEXT("timeoutS must be a number")},
		{TEXT("unknown line"), Good.Replace(TEXT("[\"tide_orb\", \"lash\", \"mirror\"]"), TEXT("[\"tide_orb\", \"lash\", \"ember\"]")), TEXT("Water lines")},
	};
	for (const FCase& Case : Cases)
	{
		const FString Path = FPaths::Combine(Dir, FString::Printf(TEXT("%s.json"), *FString(Case.Name).Replace(TEXT(" "), TEXT("-"))));
		if (!Case.Text.IsEmpty())
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s: the mutation applied"), Case.Name), Case.Text != Good || FString(Case.Name) == TEXT("not json"));
			FFileHelper::SaveStringToFile(Case.Text, *Path);
		}
		FPlayerPreset Out;
		FString Error;
		bPass &= TestFalse(*FString::Printf(TEXT("%s fails the load"), Case.Name), LoadPlayerPreset(Out, Error, Path));
		bPass &= TestTrue(*FString::Printf(TEXT("%s says why (%s)"), Case.Name, *Error), Error.Contains(Case.Message));
	}
	FRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetSaveDirectory(Dir);
	Session.SetPresetPath(FPaths::Combine(Dir, TEXT("missing.json")));
	AddExpectedError(TEXT("Session start failed: presets: cannot read"), EAutomationExpectedErrorFlags::Contains, 1);
	bPass &= TestFalse(TEXT("a missing presets.json fails the session start"), Session.Start(1));
	bPass &= TestFalse(TEXT("nothing runs"), Session.IsRunning());
	Rig.Close(Session);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresetOpponentsKeepPinned, "MageArena.Preset.OpponentsKeepPinned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresetOpponentsKeepPinned::RunTest(const FString& Parameters)
{
	// The card says "opponents still use Ripple". The pinned opponent presets are Undertow (no mirror slotted) and
	// Mirror tide (mirror A, already Reflection in the pinned data); no opponent slots Ripple. What T20 must keep is that
	// every opponent plays its pinned preset, and that the pinned Rotation (the census and conformance player) stays B.
	if (!Ready(*this))
	{
		return false;
	}
	FRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	bool bPass = true;
	const TArray<FString>& Opponents = KernelData().OpponentPresets;
	bPass &= TestEqual(TEXT("two pinned opponent presets"), Opponents.Num(), 2);
	bPass &= TestEqual(TEXT("reference preset is Rotation"), KernelData().ReferencePreset, FString(TEXT("Rotation")));
	for (int32 Index = 0; Index < Opponents.Num(); ++Index)
	{
		const FComposition* Expected = Pinned(*Opponents[Index]);
		FArenaSession Session;
		Rig.Bind(Session);
		Session.SetSaveDirectory(ScratchDir(TEXT("preset-opponents")));
		// Games.cpp SpawnWave: wave index W >= 2 spawns its Water proxy with opponentPresets[W - 2], so the semifinal
		// (index 2) is Undertow and the final (index 3) is Mirror tide.
		Session.SetBout(Index + 2, false);
		if (!TestTrue(TEXT("session starts"), Session.Start(1)) || !TestNotNull(TEXT("pinned opponent preset"), Expected))
		{
			bPass = false;
			continue;
		}
		int32 Seen = 0;
		for (const FActor& Actor : Session.GetGames().State.Actors)
		{
			if (Actor.Id == Session.GetGames().PlayerId || Actor.Enemy.IsSet() || Actor.Fire.bSchool)
			{
				continue;
			}
			++Seen;
			bPass &= TestEqual(*FString::Printf(TEXT("wave %d opponent preset name"), Index + 3), Actor.Water.Composition.Name, Expected->Name);
			bPass &= TestEqual(*FString::Printf(TEXT("wave %d opponent lines"), Index + 3), Actor.Water.Composition.Lines, Expected->Lines);
			bPass &= TestEqual(*FString::Printf(TEXT("wave %d opponent mirror"), Index + 3), Actor.Water.Composition.Branches.Mirror, Expected->Branches.Mirror);
			bPass &= TestEqual(*FString::Printf(TEXT("wave %d opponent tide orb"), Index + 3), Actor.Water.Composition.Branches.TideOrb, Expected->Branches.TideOrb);
		}
		bPass &= TestTrue(*FString::Printf(TEXT("wave %d has a water opponent (%d)"), Index + 3, Seen), Seen >= 1);
		if (const FActor* Player = PlayerOf(Session))
		{
			bPass &= TestEqual(TEXT("the player is still cassia"), Player->Water.Composition.Name, FString(TEXT("cassia")));
		}
		Session.Unbind();
	}
	const FComposition* Rotation = Pinned(TEXT("Rotation"));
	bPass &= TestTrue(TEXT("pinned Rotation keeps Ripple (mirror B)"), Rotation && Rotation->Branches.Mirror == TEXT("B"));
	FArenaSession Dummy;
	Rig.Close(Dummy);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresetStonePickSaves, "MageArena.Preset.StonePickSaves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresetStonePickSaves::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	const FString Dir = ScratchDir(TEXT("preset-pick"));
	const FString SavePath = FPaths::Combine(Dir, TEXT("bout.txt"));
	bool bPass = true;
	{
		FArenaSession Session;
		Rig.Bind(Session);
		Session.SetScripted(false);
		Session.SetSaveDirectory(Dir);
		Session.SetBout(0, false);
		if (!TestTrue(TEXT("start Bout 1"), Session.Start(1)))
		{
			Rig.Close(Session);
			return false;
		}
		WinBout(Session);
		bPass &= TestEqual(TEXT("intermission after Bout 1"), Session.GetGames().Phase, FString(TEXT("intermission")));
		bPass &= TestTrue(TEXT("the pick is offered"), Session.IsPickOffered());
		bPass &= TestEqual(TEXT("left stone label"), Session.TeachString(*Session.PickStoneKey(0)), FString(TEXT("Leviathan")));
		bPass &= TestEqual(TEXT("centre stone label"), Session.TeachString(*Session.PickStoneKey(1)), FString(TEXT("Continue")));
		bPass &= TestEqual(TEXT("right stone label"), Session.TeachString(*Session.PickStoneKey(2)), FString(TEXT("Rain of Orbs")));
		bPass &= TestTrue(*FString::Printf(TEXT("the prompt says palm (%s)"), *Session.GetPromptText()), Session.GetPromptText().Contains(TEXT("palm")));
		const int32 ChainBefore = Session.GetChain().Num();
		// The desktop path: the Z key plays this clip. The palm crosses into the stone radius at about 0.42 s.
		Rig.Hands->PlayQuickAction(TEXT("stone-left"), EClipVariant::Normal);
		const double Dt = 1.0 / 72.0;
		double Clock = 0.0;
		double HoldStart = -1.0;
		double PickedAt = -1.0;
		double GlowSeen = 0.0;
		while (Clock < 3.0 && Session.GetGames().Phase == TEXT("intermission"))
		{
			Session.Advance(Dt, true);
			Clock += Dt;
			if (Session.GetPickHoldStone() == 0)
			{
				if (HoldStart < 0.0)
				{
					HoldStart = Clock;
				}
				GlowSeen = FMath::Max(GlowSeen, Session.GetPickHoldFraction());
			}
		}
		PickedAt = Clock;
		bPass &= TestTrue(*FString::Printf(TEXT("the left stone hold started (%.3f s)"), HoldStart), HoldStart > 0.0);
		// The hold is counted per frame, from the frame the palm arrives: 0.40 s is 28.8 frames at 72 Hz, so the pick
		// lands on the 29th frame of the hold, (29 - 1) / 72 = 0.389 s after the first held frame.
		const double Held = PickedAt - HoldStart;
		bPass &= TestTrue(*FString::Printf(TEXT("the pick waited for offerHoldS (held %.3f s)"), Held), Held >= 0.40 - Dt - 1.0e-6 && Held <= 0.40 + Dt);
		bPass &= TestTrue(*FString::Printf(TEXT("the stone glowed through the hold (%.2f)"), GlowSeen), GlowSeen >= 0.9);
		bPass &= TestEqual(TEXT("Bout 2 started"), Session.GetGames().Phase, FString(TEXT("active")));
		bPass &= TestEqual(TEXT("wave index 1"), Session.GetGames().Wave, 1);
		bPass &= TestEqual(TEXT("the pick is A"), Session.GetStonePick(), FString(TEXT("A")));
		if (const FActor* Player = PlayerOf(Session))
		{
			bPass &= TestEqual(TEXT("Bout 2 player tide orb A"), Player->Water.Composition.Branches.TideOrb, FString(TEXT("A")));
			bPass &= TestEqual(TEXT("mirror still A"), Player->Water.Composition.Branches.Mirror, FString(TEXT("A")));
			FActor AtFour = *Player;
			AtFour.Tier = 4;
			const FSpell* Orb = SpellFor(AtFour, 1);
			bPass &= TestTrue(TEXT("slot 1 at tier 4 is the Leviathan Orb"), Orb && Orb->Id == TEXT("tide_orb:4:A"));
		}
		const TArray<FString>& Chain = Session.GetChain();
		TArray<FString> After;
		for (int32 Index = ChainBefore; Index < Chain.Num(); ++Index)
		{
			After.Add(Chain[Index]);
		}
		bPass &= TestEqual(TEXT("one pick line"), CountChain(After, TEXT("pick stone stone=0")), 1);
		bPass &= TestEqual(TEXT("the reach is not a ward"), CountChain(After, TEXT("gesture ward")), 0);
		bPass &= TestEqual(TEXT("the reach is not a blink"), CountChain(After, TEXT("gesture blink")), 0);
		bPass &= TestEqual(TEXT("the reach is not a bolt"), CountChain(After, TEXT("gesture bolt")), 0);
		bPass &= TestEqual(TEXT("the reach is not a sigil"), CountChain(After, TEXT("gesture sigil")), 0);
		FString Saved;
		FFileHelper::LoadFileToString(Saved, *SavePath);
		bPass &= TestEqual(TEXT("save file"), Saved, FString(TEXT("bout=1\ntideOrbIV=A\n")));
		Rig.Close(Session);
	}
	{
		// A new launch reads the save: offer, then continue into Bout 2 with A.
		FRig Next;
		if (!Next.Open(*this))
		{
			return false;
		}
		FArenaSession Session;
		Next.Bind(Session);
		Session.SetSaveDirectory(Dir);
		bPass &= TestTrue(TEXT("arc begins"), Session.BeginArc(1));
		bPass &= TestEqual(TEXT("offer from the save"), Session.GetStage(), FString(TEXT("offer")));
		bPass &= TestEqual(TEXT("loaded pick A"), Session.GetStonePick(), FString(TEXT("A")));
		Next.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
		Session.Advance(0.1, true);
		// T21: the collar ritual stands between the offer and the bout; with no hands it continues at its 20 s timeout.
		bPass &= TestEqual(TEXT("the ritual after the offer"), Session.GetStage(), FString(TEXT("ritual")));
		for (int32 Step = 0; Step < 60 * 25 && Session.GetStage() == TEXT("ritual"); ++Step)
		{
			Session.Advance(1.0 / 60.0, true);
		}
		bPass &= TestEqual(TEXT("continued into Bout 2"), Session.GetGames().Wave, 1);
		if (const FActor* Player = PlayerOf(Session))
		{
			bPass &= TestEqual(TEXT("loaded Bout 2 tide orb A"), Player->Water.Composition.Branches.TideOrb, FString(TEXT("A")));
		}
		Next.Close(Session);
	}
	{
		// An old save without the pick line still loads, and keeps B. A junk pick line is a corrupt save.
		FRig Old;
		if (!Old.Open(*this))
		{
			return false;
		}
		FFileHelper::SaveStringToFile(TEXT("bout=2\n"), *SavePath);
		FArenaSession Session;
		Old.Bind(Session);
		Session.SetSaveDirectory(Dir);
		bPass &= TestTrue(TEXT("old save arc begins"), Session.BeginArc(1));
		bPass &= TestEqual(TEXT("old save offers"), Session.GetStage(), FString(TEXT("offer")));
		bPass &= TestEqual(TEXT("old save bout"), Session.GetBoutIndex(), 2);
		bPass &= TestTrue(TEXT("old save has no pick"), Session.GetStonePick().IsEmpty());
		Old.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
		Session.Advance(0.1, true);
		if (const FActor* Player = PlayerOf(Session))
		{
			bPass &= TestEqual(TEXT("old save tide orb B"), Player->Water.Composition.Branches.TideOrb, FString(TEXT("B")));
		}
		Session.Unbind();
		FFileHelper::SaveStringToFile(TEXT("bout=1\ntideOrbIV=C\n"), *SavePath);
		FArenaSession Junk;
		Old.Bind(Junk);
		Junk.SetSaveDirectory(Dir);
		bPass &= TestTrue(TEXT("junk save arc begins"), Junk.BeginArc(1));
		bPass &= TestEqual(TEXT("a junk pick line starts from the teach"), Junk.GetStage(), FString(TEXT("cold")));
		Old.Close(Junk);
	}
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresetNoPickKeepsB, "MageArena.Preset.NoPickKeepsB",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresetNoPickKeepsB::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	const FString Dir = ScratchDir(TEXT("preset-timeout"));
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetScripted(false);
	Session.SetSaveDirectory(Dir);
	Session.SetBout(0, false);
	bool bPass = TestTrue(TEXT("start Bout 1"), Session.Start(1));
	WinBout(Session);
	bPass &= TestTrue(TEXT("the pick is offered"), Session.IsPickOffered());
	// presets.json stonePick.timeoutS 20. Steps of 0.05 s: 399 steps is 19.95 s, still open; the 400th reaches 20.0.
	const double Dt = 0.05;
	for (int32 Step = 0; Step < 399; ++Step)
	{
		Session.Advance(Dt, true);
	}
	bPass &= TestEqual(*FString::Printf(TEXT("still in the intermission at %.2f s"), Session.GetPickClock()), Session.GetGames().Phase, FString(TEXT("intermission")));
	bPass &= TestTrue(TEXT("still offered before 20 s"), Session.IsPickOffered());
	Session.Advance(Dt, true);
	bPass &= TestEqual(TEXT("the run continues at 20 s"), Session.GetGames().Phase, FString(TEXT("active")));
	bPass &= TestEqual(TEXT("Bout 2"), Session.GetGames().Wave, 1);
	bPass &= TestTrue(TEXT("no pick recorded"), Session.GetStonePick().IsEmpty());
	bPass &= TestEqual(TEXT("timeout line"), CountChain(Session.GetChain(), TEXT("pick timeout")), 1);
	if (const FActor* Player = PlayerOf(Session))
	{
		bPass &= TestEqual(TEXT("tide orb stays B"), Player->Water.Composition.Branches.TideOrb, FString(TEXT("B")));
	}
	FString Saved;
	FFileHelper::LoadFileToString(Saved, *FPaths::Combine(Dir, TEXT("bout.txt")));
	bPass &= TestEqual(TEXT("save without a pick line"), Saved, FString(TEXT("bout=1\n")));
	// The pick is offered once: the intermission after Bout 2 is a plain one. Bout 2's creatures arrive over time, so
	// the bout is won once nothing is left to spawn.
	for (int32 Try = 0; Try < 20 && Session.GetGames().Phase == TEXT("active"); ++Try)
	{
		WinBout(Session);
	}
	bPass &= TestEqual(TEXT("intermission after Bout 2"), Session.GetGames().Phase, FString(TEXT("intermission")));
	bPass &= TestFalse(TEXT("no pick after Bout 2"), Session.IsPickOffered());
	Rig.Close(Session);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaReflectionReturnsToCaster, "MageArena.Reflection.ReturnsToCaster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaReflectionReturnsToCaster::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FVrRuleset Rules;
	FString Error;
	if (!TestTrue(*FString::Printf(TEXT("overlay loads (%s)"), *Error), LoadVrRuleset(Rules, Error, false)))
	{
		return false;
	}
	const FSpell* Bubble = FindSpellById(TEXT("tide_orb:1:base"));
	bool bPass = TestTrue(TEXT("Bubble Shot is tier 1, damage 7"), Bubble && Bubble->Tier == 1 && Bubble->Damage == 7.0);
	// A Bubble Shot (tier 1, 7 damage) at a cassia player of tier 2: Reflection takes it, 1 <= 2.
	FDuel Duel = RunDuel(Cassia(*this), 2, 1, 7.0, 3.51, &Rules);
	bPass &= TestTrue(*FString::Printf(TEXT("perfect absorb (tick %d)"), Duel.PerfectTick), Duel.PerfectTick > 0);
	TArray<FArenaEvent> Reflects;
	for (const FArenaEvent& Event : Duel.State.Events)
	{
		if (Event.Kind == TEXT("reflect"))
		{
			Reflects.Add(Event);
		}
	}
	if (TestEqual(TEXT("one reflect event"), Reflects.Num(), 1))
	{
		bPass &= TestEqual(TEXT("reflect on the perfect tick"), Reflects[0].Tick, Duel.PerfectTick);
		bPass &= TestEqual(TEXT("reflect actor is the reflector"), Reflects[0].ActorId, Duel.Reflector);
		bPass &= TestEqual(TEXT("reflect target is the original caster"), Reflects[0].TargetId.Get(-1), Duel.Caster);
		bPass &= Near(*this, TEXT("reflect value is the projectile tier"), Reflects[0].Value, 1.0);
	}
	else
	{
		bPass = false;
	}
	const FActor* Reflector = SimFindActor(Duel.State, Duel.Reflector);
	const FActor* Caster = SimFindActor(Duel.State, Duel.Caster);
	// The perfect absorbs all of it (mirror-2a: hit value 0), and the caster takes the projectile's own 7: 95 - 7 = 88.
	bPass &= Near(*this, TEXT("reflector unhurt"), Reflector->Hp, 95.0);
	bPass &= Near(*this, TEXT("caster hit by its own Bubble Shot"), Caster->Hp, 88.0);
	bPass &= TestEqual(TEXT("caster hit once"), Caster->Metrics.Hits, 1);
	bPass &= TestEqual(TEXT("the reflector dealt it"), Reflector->Metrics.DamageDealt, 7.0);
	bPass &= TestEqual(TEXT("no Ripple ring on branch A"), CountEvents(Duel.State, TEXT("hit")), 2);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaReflectionTierGate, "MageArena.Reflection.TierGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaReflectionTierGate::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FVrRuleset Rules;
	FString Error;
	if (!TestTrue(*FString::Printf(TEXT("overlay loads (%s)"), *Error), LoadVrRuleset(Rules, Error, false)))
	{
		return false;
	}
	const FSpell* Twin = FindSpellById(TEXT("tide_orb:3:base"));
	bool bPass = TestTrue(TEXT("Twin Tides is tier 3, 11 each"), Twin && Twin->Tier == 3 && Twin->Damage == 11.0);
	// One Twin Tides orb (tier 3, 11 damage) at a tier 2 player: 3 > 2, so it is absorbed normally and not returned.
	FDuel Duel = RunDuel(Cassia(*this), 2, 3, 11.0, 3.51, &Rules);
	bPass &= TestTrue(*FString::Printf(TEXT("perfect absorb (tick %d)"), Duel.PerfectTick), Duel.PerfectTick > 0);
	bPass &= TestEqual(TEXT("no reflect event"), CountEvents(Duel.State, TEXT("reflect")), 0);
	bool bAnyReflected = false;
	for (const FProjectile& Projectile : Duel.State.Projectiles)
	{
		bAnyReflected |= Projectile.bReflected;
	}
	bPass &= TestFalse(TEXT("no reflected projectile"), bAnyReflected);
	bPass &= Near(*this, TEXT("reflector unhurt (perfect)"), SimFindActor(Duel.State, Duel.Reflector)->Hp, 95.0);
	bPass &= Near(*this, TEXT("caster untouched"), SimFindActor(Duel.State, Duel.Caster)->Hp, 95.0);
	// The same orb at tier 3 is returned: the gate is the player's unlocked tier.
	FDuel AtThree = RunDuel(Cassia(*this), 3, 3, 11.0, 3.51, &Rules);
	bPass &= TestEqual(TEXT("tier 3 player reflects a tier 3 orb"), CountEvents(AtThree.State, TEXT("reflect")), 1);
	bPass &= Near(*this, TEXT("caster takes 11 at tier 3: 95 - 11"), SimFindActor(AtThree.State, AtThree.Caster)->Hp, 84.0);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaReflectionRippleAndNullPath, "MageArena.Reflection.RippleAndNullPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaReflectionRippleAndNullPath::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FVrRuleset Rules;
	FString Error;
	if (!TestTrue(*FString::Printf(TEXT("overlay loads (%s)"), *Error), LoadVrRuleset(Rules, Error, false)))
	{
		return false;
	}
	const FComposition* Rotation = Pinned(TEXT("Rotation"));
	if (!TestNotNull(TEXT("pinned Rotation"), Rotation))
	{
		return false;
	}
	const FSpell* Ripple = FindSpellById(TEXT("mirror:2:B"));
	bool bPass = TestTrue(TEXT("Ripple is 7.5 at r 3"), Ripple && Ripple->Damage == 7.5 && Ripple->RangeM == 3.0);
	// Branch B is unchanged: Rotation at tier 2 perfects a Bubble Shot from 2.5 m and the ring hits the caster for 7.5.
	FDuel Ring = RunDuel(*Rotation, 2, 1, 7.0, 2.5, &Rules);
	bPass &= TestTrue(TEXT("Rotation perfect"), Ring.PerfectTick > 0);
	bPass &= TestEqual(TEXT("Ripple: no reflect event"), CountEvents(Ring.State, TEXT("reflect")), 0);
	bPass &= Near(*this, TEXT("Ripple: caster 95 - 7.5"), SimFindActor(Ring.State, Ring.Caster)->Hp, 87.5);
	// The null ruleset (conformance) path: the kernel still reflects, as the pinned mirror-2a vector records, and emits no
	// reflect event, so that vector's event log is unchanged.
	FDuel Null = RunDuel(Cassia(*this), 2, 1, 7.0, 3.51, nullptr);
	bPass &= TestTrue(TEXT("null path perfect"), Null.PerfectTick > 0);
	bPass &= TestEqual(TEXT("null path: no reflect event"), CountEvents(Null.State, TEXT("reflect")), 0);
	bPass &= Near(*this, TEXT("null path: the caster still takes 7"), SimFindActor(Null.State, Null.Caster)->Hp, 88.0);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresetLeviathanOrb, "MageArena.Preset.LeviathanOrb",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresetLeviathanOrb::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FSpell* Orb = FindSpellById(TEXT("tide_orb:4:A"));
	if (!TestNotNull(TEXT("tide_orb:4:A"), Orb))
	{
		return false;
	}
	bool bPass = true;
	bPass &= TestEqual(TEXT("name"), Orb->Name, FString(TEXT("Leviathan Orb")));
	bPass &= TestEqual(TEXT("kind"), Orb->Kind, FString(TEXT("projectile")));
	bPass &= TestEqual(TEXT("family"), Orb->Family, FString(TEXT("unblockable")));
	bPass &= Near(*this, TEXT("speed 6 m/s"), Orb->SpeedMps, 6.0);
	bPass &= Near(*this, TEXT("radius 1.5 m"), Orb->RadiusM, 1.5);
	bPass &= Near(*this, TEXT("cast 0.60 s"), Orb->CastS, 0.6);
	bPass &= Near(*this, TEXT("telegraph 1.00 s"), Orb->TelegraphS, 1.0);
	bPass &= Near(*this, TEXT("mana 45"), Orb->Mana, 45.0);
	bPass &= Near(*this, TEXT("damage 40"), Orb->Damage, 40.0);
	bPass &= Near(*this, TEXT("range 14 m"), Orb->RangeM, 14.0);
	bPass &= TestEqual(TEXT("one orb"), Orb->Count, 1);

	// Cast it: cassia with the pick A, tier 4, full mana, at a dummy 8 m ahead.
	FPlayerPreset Preset;
	FString Error;
	if (!TestTrue(*FString::Printf(TEXT("presets.json loads (%s)"), *Error), LoadPlayerPreset(Preset, Error)))
	{
		return false;
	}
	const FComposition Picked = Preset.WithPick(TEXT("A"));
	FArenaState State = CreateArena(11);
	FActor& Me = AddMage(State, 0, FSimVec{10.0, 10.0});
	Me.Water = NewWaterState(&Picked);
	Me.Tier = 4;
	Me.LastUnlockTick = 1000000000;
	const int32 MeId = Me.Id;
	const int32 Target = AddMage(State, 1, FSimVec{18.0, 10.0}).Id;
	FInputFrame Input = SimIdleInput(FSimVec{18.0, 10.0});
	Input.Slot = 1;
	Input.bCast = true;
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(MeId, Input);
	StepArena(State, Inputs);
	const FActor* Caster = SimFindActor(State, MeId);
	bPass &= TestTrue(TEXT("the cast is pending"), Caster->Pending.IsSet() && Caster->Pending->SpellId.Get(FString()) == TEXT("tide_orb:4:A"));
	if (Caster->Pending.IsSet())
	{
		// max(cast 0.60, telegraph 1.00) = 1.00 s = 60 ticks at 60 Hz.
		bPass &= TestEqual(TEXT("released after the 1.00 s telegraph"), Caster->Pending->ReleaseTick - Caster->Pending->StartTick, 60);
	}
	bPass &= Near(*this, TEXT("mana 95 - 45"), Caster->Mana, 50.0);
	for (int32 Tick = 0; Tick < 60 && State.Projectiles.Num() == 0; ++Tick)
	{
		StepArena(State);
	}
	if (TestEqual(TEXT("one orb in flight"), State.Projectiles.Num(), 1))
	{
		const FProjectile& Flight = State.Projectiles[0];
		bPass &= Near(*this, TEXT("flight speed 6"), SimLength(Flight.Velocity), 6.0);
		bPass &= Near(*this, TEXT("flight radius 1.5"), Flight.Radius, 1.5);
		bPass &= Near(*this, TEXT("flight damage 40 (Flow 0)"), Flight.Damage, 40.0);
		bPass &= TestEqual(TEXT("flight family"), Flight.Family, FString(TEXT("unblockable")));
	}
	// The dummy raises a fresh ward when the orb is close: an UNBLOCKABLE hit is not reduced. 95 - 40 = 55.
	bool bRaised = false;
	for (int32 Tick = 0; Tick < 180; ++Tick)
	{
		const FActor* Dummy = SimFindActor(State, Target);
		for (const FProjectile& Flight : State.Projectiles)
		{
			bRaised |= SimDistance(Flight.Pos, Dummy->Pos) < 2.5;
		}
		FInputFrame Ward = SimIdleInput(FSimVec{10.0, 10.0});
		Ward.bAbsorb = bRaised;
		TMap<int32, FInputFrame> WardInputs;
		WardInputs.Add(Target, Ward);
		StepArena(State, WardInputs);
	}
	bPass &= Near(*this, TEXT("unblockable through the ward: 95 - 40"), SimFindActor(State, Target)->Hp, 55.0);
	return bPass;
}

#endif
