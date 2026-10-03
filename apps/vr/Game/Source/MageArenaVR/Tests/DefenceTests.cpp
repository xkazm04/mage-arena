#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "Hands/HandClipPlayer.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/KernelData.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/Paths.h"

#include <cmath>

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
	const double Diff = std::abs(Actual - Expected);
	const double Scale = std::max(std::abs(Actual), std::abs(Expected));
	const bool bOk = Actual == Expected || Diff <= 1.0e-9 || Diff <= 1.0e-6 * Scale;
	if (!bOk)
	{
		Test.AddError(FString::Printf(TEXT("%s: expected %.17g got %.17g"), What, Expected, Actual));
	}
	return bOk;
}

const FComposition* FindRotation()
{
	for (const FComposition& Preset : KernelData().Presets)
	{
		if (Preset.Name == TEXT("Rotation"))
		{
			return &Preset;
		}
	}
	return nullptr;
}

struct FDuel
{
	FArenaState State;
	FVrRuleset Rules;
	int32 Player = 0;
	int32 Foe = 0;
};

bool OpenDuel(FAutomationTestBase& Test, FDuel& Out, double FoeX)
{
	FString Error;
	if (!LoadVrRuleset(Out.Rules, Error))
	{
		Test.AddError(Error);
		return false;
	}
	const FComposition* Rotation = FindRotation();
	if (!Rotation)
	{
		Test.AddError(TEXT("Rotation preset missing"));
		return false;
	}
	Out.State = CreateArena(11);
	Out.Player = AddMage(Out.State, 0, FSimVec{10.0, 10.0}, TEXT("Cassia")).Id;
	Out.Foe = AddMage(Out.State, 1, FSimVec{FoeX, 10.0}, TEXT("Foe")).Id;
	FActor* Actor = SimFindActor(Out.State, Out.Player);
	Actor->Water = NewWaterState(Rotation);
	Actor->LastUnlockTick = 1000000000;
	return true;
}

FInputFrame AimAt(const FDuel& Duel)
{
	const FActor* Foe = SimFindActor(Duel.State, Duel.Foe);
	return SimIdleInput(Foe ? Foe->Pos : FSimVec{14.0, 10.0});
}

void StepPlayer(FDuel& Duel, const FInputFrame& Input, FVrRuleset* Rules)
{
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(Duel.Player, Input);
	StepArena(Duel.State, Inputs, Rules);
}

FActor& PlayerOf(FDuel& Duel)
{
	return *SimFindActor(Duel.State, Duel.Player);
}

FActor& FoeOf(FDuel& Duel)
{
	return *SimFindActor(Duel.State, Duel.Foe);
}

bool AssertProposals(FAutomationTestBase& Test, const FVrRuleset& Rules)
{
	const FSpell* Bolt = FindSpellById(TEXT("bolt:0:base"));
	const FSpell* Lash = FindSpellById(TEXT("lash:1:base"));
	bool bPass = Test.TestNotNull(TEXT("bolt row"), Bolt) && Test.TestNotNull(TEXT("lash row"), Lash);
	bPass &= Test.TestTrue(TEXT("split enabled"), Rules.Split.bEnabled);
	bPass &= Test.TestTrue(TEXT("staff enabled"), Rules.Staff.bEnabled);
	bPass &= Near(Test, TEXT("one-hand power"), Rules.Split.OneHandPower, 0.6);
	bPass &= Test.TestEqual(TEXT("max tier"), Rules.Split.MaxTier, 2);
	bPass &= Near(Test, TEXT("dome duration"), Rules.Staff.DurationS, 6.0);
	bPass &= Near(Test, TEXT("dome mana up front"), Rules.Staff.ManaUpFront, 8.0);
	bPass &= Near(Test, TEXT("dome drain"), Rules.Staff.DrainPerSecond, 4.0);
	bPass &= Near(Test, TEXT("dome magic"), Rules.Staff.MagicReduction, 0.85);
	bPass &= Near(Test, TEXT("dome physical"), Rules.Staff.PhysicalReduction, 0.6);
	bPass &= Near(Test, TEXT("dome unblockable"), Rules.DomeReduction(TEXT("unblockable")), 0.0);
	if (Bolt)
	{
		bPass &= Near(Test, TEXT("bolt damage"), Bolt->Damage, 2.05);
	}
	if (Lash)
	{
		bPass &= Near(Test, TEXT("lash damage"), Lash->Damage, 8.0);
	}
	bPass &= Near(Test, TEXT("flow max"), KernelData().FlowMax, 5.0);
	bPass &= Near(Test, TEXT("crest multiplier"), KernelData().CrestDamageMult, 1.5);
	bPass &= Near(Test, TEXT("ward magic"), KernelData().Absorb.ReductionMagic, 0.85);
	bPass &= Near(Test, TEXT("outside the arc is open"), KernelData().Absorb.ReductionOutsideArc, 0.0);
	return bPass;
}

void SpawnIncoming(FArenaState& State, int32 OwnerId, const FSimVec& At, const FSimVec& Direction, double Damage, const TCHAR* Family)
{
	FHit Hit;
	Hit.OwnerId = OwnerId;
	Hit.ActivationId = State.NextId++;
	Hit.Damage = Damage;
	Hit.Family = Family;
	Hit.Tier = 1;
	Hit.Source = At;
	SpawnProjectile(State, Hit, At, Direction, 60.0, 30.0);
}

const FProjectile* PlayerShot(const FArenaState& State, int32 PlayerId)
{
	for (const FProjectile& Projectile : State.Projectiles)
	{
		if (Projectile.OwnerId == PlayerId)
		{
			return &Projectile;
		}
	}
	return nullptr;
}

bool HasRefusal(const FVrRuleset& Rules, const TCHAR* Reason)
{
	return Rules.Refusals.Contains(FString(Reason));
}

struct FDefenceLogProbe : FOutputDevice
{
	int32 Lines = 0;

	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName&) override
	{
		if (Message && FCString::Strstr(Message, TEXT("defence ")))
		{
			++Lines;
		}
	}
};

struct FDefenceLogScope
{
	FDefenceLogProbe& Probe;

	explicit FDefenceLogScope(FDefenceLogProbe& InProbe)
		: Probe(InProbe)
	{
		GLog->AddOutputDevice(&Probe);
	}

	~FDefenceLogScope()
	{
		GLog->RemoveOutputDevice(&Probe);
	}
};

struct FHandRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;

	bool Start(FAutomationTestBase& Test, bool bWard, bool bSigil, bool bStaff)
	{
		Instance = NewObject<UGameInstance>(GetTransientPackage());
		Instance->AddToRoot();
		Hands = NewObject<UHandInputSubsystem>(Instance);
		Hands->AddToRoot();
		if (bWard)
		{
			Wards = NewObject<UWardDetectorSubsystem>(Instance);
			Wards->AddToRoot();
			FString Error;
			if (!Wards->InitDetector(Error))
			{
				Test.AddError(Error);
				return false;
			}
			Wards->BindToHands(Hands);
		}
		if (bSigil)
		{
			Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
			Sigils->AddToRoot();
			Sigils->LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
			Sigils->BindToHands(Hands);
		}
		if (bStaff)
		{
			Staff = NewObject<UStaffDetectorSubsystem>(Instance);
			Staff->AddToRoot();
			Staff->BindToHands(Hands);
		}
		return true;
	}

	void Finish()
	{
		if (Staff)
		{
			Staff->BindToHands(nullptr);
			Staff->RemoveFromRoot();
			Staff->MarkAsGarbage();
			Staff = nullptr;
		}
		if (Sigils)
		{
			Sigils->BindToHands(nullptr);
			Sigils->RemoveFromRoot();
			Sigils->MarkAsGarbage();
			Sigils = nullptr;
		}
		if (Wards)
		{
			Wards->BindToHands(nullptr);
			Wards->RemoveFromRoot();
			Wards->MarkAsGarbage();
			Wards = nullptr;
		}
		if (Hands)
		{
			if (UHandClipPlayer* Player = Hands->GetClipPlayer())
			{
				Player->Stop();
			}
			Hands->RemoveFromRoot();
			Hands->MarkAsGarbage();
			Hands = nullptr;
		}
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
			Instance = nullptr;
		}
	}
};

bool StepAction(FAutomationTestBase& Test, UHandInputSubsystem* Hands, FName Action)
{
	UHandClipPlayer* Player = Hands->GetClipPlayer();
	if (!Player || Player->GetClip().Action != Action.ToString() || Player->GetClip().GetTracks().Num() == 0)
	{
		Test.AddError(FString::Printf(TEXT("%s did not start"), *Action.ToString()));
		return false;
	}
	const FHandClipTrack& Track = Player->GetClip().GetTracks()[0];
	const double Duration = Player->GetDuration();
	for (int32 Index = 1; Index < Track.Frames.Num(); ++Index)
	{
		if (Player->GetTime() + 1.0e-9 >= Duration)
		{
			break;
		}
		const double Dt = Track.Frames[Index].TimeSeconds - Player->GetTime();
		if (Dt > 1.0e-9)
		{
			Hands->Step(Dt);
		}
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesProposals, "MageArena.Defences.Proposals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesProposals::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FVrRuleset Rules;
	FString Error;
	if (!LoadVrRuleset(Rules, Error))
	{
		AddError(Error);
		return false;
	}
	return AssertProposals(*this, Rules);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesSplitBolt, "MageArena.Defences.SplitBolt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesSplitBolt::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FDuel Duel;
	// 4 m is outside the 1.6 m staff, so slot 0 is the bolt. One step of 20 m/s does not arrive.
	if (!OpenDuel(*this, Duel, 14.0))
	{
		return false;
	}
	bool bPass = AssertProposals(*this, Duel.Rules);
	const FSpell* Bolt = FindSpellById(TEXT("bolt:0:base"));
	if (!Bolt)
	{
		return false;
	}
	const double OneHand = Bolt->Damage * Duel.Rules.Split.OneHandPower;
	bPass &= Near(*this, TEXT("one-hand bolt is 2.05 * 0.6"), OneHand, 1.23);

	// In front of the palm, close enough that 60 m/s crosses the body this tick.
	SpawnIncoming(Duel.State, Duel.Foe, FSimVec{10.55, 10.0}, FSimVec{-1.0, 0.0}, 20.0, TEXT("magic"));
	FInputFrame Opening = AimAt(Duel);
	Opening.bAbsorb = true;
	Opening.bCast = true;
	Opening.Slot = 0;
	StepPlayer(Duel, Opening, &Duel.Rules);

	const double Warded = 20.0 * (1.0 - KernelData().Absorb.ReductionMagic);
	bPass &= Near(*this, TEXT("split ward keeps 0.15 of a magic hit"), PlayerOf(Duel).Metrics.DamageTaken, Warded);
	bPass &= Near(*this, TEXT("split ward damage is 3"), PlayerOf(Duel).Metrics.DamageTaken, 3.0);
	bPass &= TestEqual(TEXT("no perfect while the ward is also casting"), PlayerOf(Duel).Metrics.Perfects, 0);
	bPass &= TestEqual(TEXT("one bolt"), PlayerOf(Duel).Metrics.Casts, 1);
	bPass &= TestEqual(TEXT("split builds no flow"), PlayerOf(Duel).Water.Flow, 0);
	bPass &= TestTrue(TEXT("split latch is on"), Duel.Rules.IsSplitCasting(Duel.Player));

	bool bLanded = false;
	for (int32 Index = 0; Index < 40 && !bLanded; ++Index)
	{
		FInputFrame Held = AimAt(Duel);
		Held.bAbsorb = true;
		StepPlayer(Duel, Held, &Duel.Rules);
		bLanded = FoeOf(Duel).Metrics.Hits > 0;
	}
	bPass &= TestTrue(TEXT("one-hand bolt lands"), bLanded);
	bPass &= Near(*this, TEXT("foe took the one-hand bolt"), FoeOf(Duel).Metrics.DamageTaken, OneHand);
	bPass &= Near(*this, TEXT("caster dealt the one-hand bolt"), PlayerOf(Duel).Metrics.DamageDealt, OneHand);
	bPass &= TestEqual(TEXT("flow still zero after the bolt"), PlayerOf(Duel).Water.Flow, 0);
	bPass &= TestEqual(TEXT("still one cast"), PlayerOf(Duel).Metrics.Casts, 1);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesSplitLine, "MageArena.Defences.SplitLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesSplitLine::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FDuel Duel;
	// 2.4 m is inside Tide Lash (3 m) and outside the staff (1.6 m). Slot 2 is lash on Rotation.
	if (!OpenDuel(*this, Duel, 12.4))
	{
		return false;
	}
	bool bPass = AssertProposals(*this, Duel.Rules);
	PlayerOf(Duel).Tier = 1;
	const FSpell* Lash = SpellFor(PlayerOf(Duel), 2);
	if (!TestNotNull(TEXT("tide lash"), Lash))
	{
		return false;
	}
	bPass &= TestEqual(TEXT("lash id"), Lash->Id, FString(TEXT("lash:1:base")));
	const double Expected = Lash->Damage * Duel.Rules.Split.OneHandPower;
	bPass &= Near(*this, TEXT("one-hand lash is 8 * 0.6"), Expected, 4.8);
	const int32 ReleaseTick = 1 + SimTicks(std::max(Lash->CastS, Lash->TelegraphS));

	FInputFrame Cast = AimAt(Duel);
	Cast.bAbsorb = true;
	Cast.bCast = true;
	Cast.Slot = 2;
	StepPlayer(Duel, Cast, &Duel.Rules);
	bPass &= TestEqual(TEXT("lash accepted"), PlayerOf(Duel).Metrics.Casts, 1);
	bPass &= TestEqual(TEXT("no flow on the cast tick"), PlayerOf(Duel).Water.Flow, 0);

	for (int32 Tick = Duel.State.Tick; Tick < ReleaseTick - 1; ++Tick)
	{
		FInputFrame Held = AimAt(Duel);
		Held.bAbsorb = true;
		StepPlayer(Duel, Held, &Duel.Rules);
	}
	bPass &= TestEqual(TEXT("still winding"), Duel.State.Tick, ReleaseTick - 1);
	bPass &= Near(*this, TEXT("lash has not landed"), FoeOf(Duel).Metrics.DamageTaken, 0.0);

	FInputFrame Release = AimAt(Duel);
	Release.bAbsorb = true;
	StepPlayer(Duel, Release, &Duel.Rules);
	bPass &= TestEqual(TEXT("released on the cast-time tick"), Duel.State.Tick, ReleaseTick);
	bPass &= Near(*this, TEXT("one-hand lash damage"), FoeOf(Duel).Metrics.DamageTaken, Expected);
	bPass &= TestEqual(TEXT("line built no flow"), PlayerOf(Duel).Water.Flow, 0);
	bPass &= TestEqual(TEXT("no crest"), PlayerOf(Duel).Water.Crests, 0);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesTierAndCrest, "MageArena.Defences.TierAndCrest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesTierAndCrest::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;

	{
		FDuel Duel;
		if (!OpenDuel(*this, Duel, 14.0))
		{
			return false;
		}
		PlayerOf(Duel).Tier = 3;
		const FSpell* Twin = SpellFor(PlayerOf(Duel), 1);
		if (!TestNotNull(TEXT("twin tides"), Twin))
		{
			return false;
		}
		bPass &= TestTrue(TEXT("tier is above the gate"), Twin->Tier > Duel.Rules.Split.MaxTier);
		bPass &= TestTrue(TEXT("the refused spell costs more than a drain tick"), Twin->Mana > 2.0);

		FInputFrame Raise = AimAt(Duel);
		Raise.bAbsorb = true;
		StepPlayer(Duel, Raise, &Duel.Rules);
		bPass &= TestTrue(TEXT("ward is up before the refused cast"), PlayerOf(Duel).bAbsorb);

		const double Before = PlayerOf(Duel).Mana;
		FInputFrame Cast = AimAt(Duel);
		Cast.bAbsorb = true;
		Cast.bCast = true;
		Cast.Slot = 1;
		StepPlayer(Duel, Cast, &Duel.Rules);
		// The refused cast spends no spell mana, so the tick is regen minus the ward drain.
		// AddMage uses runtime.json training.defaultRanks: focus 1, nerve 1.
		// stats.csv: manaRegen = 6 + 1.5*rank, so focus 1 is 7.5/s. combat.json simStepHz is 60, so 7.5/60 = 0.125.
		// combat.json absorb.drainPerSecond is "30 - 3 * nerveRank", so nerve 1 is 27/s and 27/60 = 0.45.
		// WardUntil is still 0, so a ward spell does not cut that drain.
		bPass &= TestEqual(TEXT("default focus"), PlayerOf(Duel).Ranks.Focus, 1);
		bPass &= TestEqual(TEXT("default nerve"), PlayerOf(Duel).Ranks.Nerve, 1);
		bPass &= Near(*this, TEXT("tier refuse spends no spell mana"), PlayerOf(Duel).Mana - Before, 0.125 - 0.45);
		bPass &= TestEqual(TEXT("tier refuse does not cast"), PlayerOf(Duel).Metrics.Casts, 0);
		bPass &= TestTrue(TEXT("refusal is tier"), HasRefusal(Duel.Rules, TEXT("tier")));
		bPass &= TestNull(TEXT("no projectile"), PlayerShot(Duel.State, Duel.Player));
	}

	{
		FDuel Duel;
		if (!OpenDuel(*this, Duel, 14.0))
		{
			return false;
		}
		FInputFrame Raise = AimAt(Duel);
		Raise.bAbsorb = true;
		StepPlayer(Duel, Raise, &Duel.Rules);
		PlayerOf(Duel).Water.Flow = static_cast<int32>(KernelData().FlowMax);
		PlayerOf(Duel).Water.LastActivityTick = Duel.State.Tick;

		FInputFrame Cast = AimAt(Duel);
		Cast.bAbsorb = true;
		Cast.bCast = true;
		Cast.Slot = 0;
		StepPlayer(Duel, Cast, &Duel.Rules);
		bPass &= TestEqual(TEXT("crest stays refused"), PlayerOf(Duel).Water.Crests, 0);
		bPass &= TestEqual(TEXT("flow stays full"), PlayerOf(Duel).Water.Flow, static_cast<int32>(KernelData().FlowMax));
		bPass &= TestEqual(TEXT("crest refuse does not cast"), PlayerOf(Duel).Metrics.Casts, 0);
		bPass &= TestTrue(TEXT("refusal is crest"), HasRefusal(Duel.Rules, TEXT("crest")));
		bPass &= TestNull(TEXT("no crest bolt"), PlayerShot(Duel.State, Duel.Player));
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesDome, "MageArena.Defences.Dome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesDome::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;

	auto PlantAndHit = [&](FDuel& Duel, double FoeX, const FSimVec& At, const FSimVec& Direction, double Damage, const TCHAR* Family, bool bAbsorb, bool bSameTickAbsorb) -> bool
	{
		if (!OpenDuel(*this, Duel, FoeX))
		{
			return false;
		}
		if (!bSameTickAbsorb)
		{
			FInputFrame Plant = AimAt(Duel);
			Plant.bPlantStaff = true;
			StepPlayer(Duel, Plant, &Duel.Rules);
			bPass &= TestTrue(TEXT("dome is up"), Duel.Rules.IsPlanted(Duel.Player));
		}
		SpawnIncoming(Duel.State, Duel.Foe, At, Direction, Damage, Family);
		FInputFrame Hit = AimAt(Duel);
		Hit.bPlantStaff = bSameTickAbsorb;
		Hit.bAbsorb = bAbsorb;
		StepPlayer(Duel, Hit, &Duel.Rules);
		return true;
	};

	{
		FDuel Duel;
		if (!PlantAndHit(Duel, 14.0, FSimVec{10.55, 10.0}, FSimVec{-1.0, 0.0}, 20.0, TEXT("magic"), false, true))
		{
			return false;
		}
		const double Expected = 20.0 * (1.0 - Duel.Rules.Staff.MagicReduction);
		bPass &= Near(*this, TEXT("dome magic keeps 0.15"), PlayerOf(Duel).Metrics.DamageTaken, Expected);
		bPass &= Near(*this, TEXT("dome magic is 3"), PlayerOf(Duel).Metrics.DamageTaken, 3.0);
		bPass &= TestEqual(TEXT("dome does not perfect magic"), PlayerOf(Duel).Metrics.Perfects, 0);
	}
	{
		// Behind the facing. A palm ward would miss this. The dome still takes its cut.
		FDuel Duel;
		if (!PlantAndHit(Duel, 14.0, FSimVec{9.45, 10.0}, FSimVec{1.0, 0.0}, 10.0, TEXT("physical"), true, false))
		{
			return false;
		}
		bPass &= TestTrue(TEXT("ward is up behind the spear"), PlayerOf(Duel).bAbsorb);
		const double Expected = 10.0 * (1.0 - Duel.Rules.Staff.PhysicalReduction);
		bPass &= Near(*this, TEXT("dome physical from behind"), PlayerOf(Duel).Metrics.DamageTaken, Expected);
		bPass &= Near(*this, TEXT("dome physical is 4"), PlayerOf(Duel).Metrics.DamageTaken, 4.0);
		bPass &= TestEqual(TEXT("physical does not perfect"), PlayerOf(Duel).Metrics.Perfects, 0);
	}
	{
		FDuel Duel;
		if (!PlantAndHit(Duel, 14.0, FSimVec{10.55, 10.0}, FSimVec{-1.0, 0.0}, 10.0, TEXT("unblockable"), false, true))
		{
			return false;
		}
		bPass &= Near(*this, TEXT("unblockable passes the dome"), PlayerOf(Duel).Metrics.DamageTaken, 10.0);
		bPass &= TestEqual(TEXT("unblockable does not perfect"), PlayerOf(Duel).Metrics.Perfects, 0);
	}
	{
		// Fresh palm, not casting. The dome must not downgrade a real perfect.
		FDuel Duel;
		if (!PlantAndHit(Duel, 14.0, FSimVec{10.55, 10.0}, FSimVec{-1.0, 0.0}, 20.0, TEXT("magic"), true, true))
		{
			return false;
		}
		bPass &= TestTrue(TEXT("fresh ward is up under the dome"), PlayerOf(Duel).bAbsorb);
		bPass &= TestFalse(TEXT("a pure ward is not split-casting"), Duel.Rules.IsSplitCasting(Duel.Player));
		bPass &= Near(*this, TEXT("fresh ward still perfects"), PlayerOf(Duel).Metrics.DamageTaken, 0.0);
		bPass &= TestEqual(TEXT("one perfect"), PlayerOf(Duel).Metrics.Perfects, 1);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesPlantedMana, "MageArena.Defences.PlantedMana",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesPlantedMana::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FDuel Duel;
	if (!OpenDuel(*this, Duel, 14.0))
	{
		return false;
	}
	bool bPass = AssertProposals(*this, Duel.Rules);
	bPass &= TestEqual(TEXT("arena starts at tick 0"), Duel.State.Tick, 0);
	const double StartMana = 40.0;
	PlayerOf(Duel).Mana = StartMana;
	bPass &= TestTrue(TEXT("40 sits under the pool"), PlayerOf(Duel).MaxMana > 60.0);

	const double RegenPerTick = (KernelData().ManaRegenBase + KernelData().ManaRegenPerRank * static_cast<double>(PlayerOf(Duel).Ranks.Focus)) * SimDt();
	const double DrainPerTick = Duel.Rules.Staff.DrainPerSecond * SimDt();
	const int32 PlantedTicks = SimTicks(Duel.Rules.Staff.DurationS);
	bPass &= TestEqual(TEXT("six seconds is 360 ticks"), PlantedTicks, 360);
	// Plant tick pays the up-front cost and that tick's drain. Expiry lifts before the drain.
	const double AtExpiryEdge = StartMana + PlantedTicks * RegenPerTick - Duel.Rules.Staff.ManaUpFront - PlantedTicks * DrainPerTick;
	const double AfterLift = AtExpiryEdge + RegenPerTick;
	bPass &= Near(*this, TEXT("hand total at 6 s"), AtExpiryEdge, 53.0);
	bPass &= Near(*this, TEXT("hand total after the lift tick"), AfterLift, 53.125);

	FInputFrame Plant = AimAt(Duel);
	Plant.bPlantStaff = true;
	StepPlayer(Duel, Plant, &Duel.Rules);
	bPass &= TestTrue(TEXT("planted on tick 1"), Duel.Rules.IsPlanted(Duel.Player));
	bPass &= TestEqual(TEXT("until is the plant tick plus 360"), Duel.Rules.StaffRuntime.UntilTick, 1 + PlantedTicks);

	while (Duel.State.Tick < PlantedTicks)
	{
		StepPlayer(Duel, AimAt(Duel), &Duel.Rules);
	}
	bPass &= TestEqual(TEXT("tick 360"), Duel.State.Tick, PlantedTicks);
	bPass &= TestTrue(TEXT("still planted on the last included tick"), Duel.Rules.IsPlanted(Duel.Player));
	bPass &= Near(*this, TEXT("mana after 360 planted ticks"), PlayerOf(Duel).Mana, AtExpiryEdge);

	StepPlayer(Duel, AimAt(Duel), &Duel.Rules);
	bPass &= TestEqual(TEXT("tick 361"), Duel.State.Tick, PlantedTicks + 1);
	bPass &= TestFalse(TEXT("expiry lifts"), Duel.Rules.IsPlanted(Duel.Player));
	bPass &= Near(*this, TEXT("lift tick regens and does not drain"), PlayerOf(Duel).Mana, AfterLift);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesPlantedHands, "MageArena.Defences.PlantedHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesPlantedHands::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;

	{
		FDuel Duel;
		if (!OpenDuel(*this, Duel, 14.0))
		{
			return false;
		}
		const double Stamina = PlayerOf(Duel).Stamina;
		const FSimVec Pos = PlayerOf(Duel).Pos;
		FInputFrame Plant = AimAt(Duel);
		Plant.bPlantStaff = true;
		StepPlayer(Duel, Plant, &Duel.Rules);

		FInputFrame Blink = AimAt(Duel);
		Blink.bRoll = true;
		StepPlayer(Duel, Blink, &Duel.Rules);
		bPass &= TestEqual(TEXT("blink while planted does not roll"), PlayerOf(Duel).Metrics.Rolls, 0);
		bPass &= TestFalse(TEXT("blink lifts the staff"), Duel.Rules.IsPlanted(Duel.Player));
		bPass &= Near(*this, TEXT("no blink stamina"), PlayerOf(Duel).Stamina, Stamina);
		bPass &= Near(*this, TEXT("no blink travel x"), PlayerOf(Duel).Pos.X, Pos.X);
		bPass &= Near(*this, TEXT("no blink travel y"), PlayerOf(Duel).Pos.Y, Pos.Y);
	}

	{
		FDuel Duel;
		if (!OpenDuel(*this, Duel, 14.0))
		{
			return false;
		}
		const double Stamina = PlayerOf(Duel).Stamina;
		FInputFrame Blink = AimAt(Duel);
		Blink.bRoll = true;
		Blink.Move = FSimVec{1.0, 0.0};
		StepPlayer(Duel, Blink, &Duel.Rules);
		bPass &= TestEqual(TEXT("unplanted blink still rolls"), PlayerOf(Duel).Metrics.Rolls, 1);
		bPass &= Near(*this, TEXT("roll spent stamina"), PlayerOf(Duel).Stamina, Stamina - KernelData().RollStaminaCost);
	}

	{
		FDuel Duel;
		if (!OpenDuel(*this, Duel, 14.0))
		{
			return false;
		}
		const FSpell* Bolt = FindSpellById(TEXT("bolt:0:base"));
		if (!Bolt)
		{
			return false;
		}
		PlayerOf(Duel).Water.Flow = static_cast<int32>(KernelData().FlowMax);
		PlayerOf(Duel).Water.LastActivityTick = Duel.State.Tick;
		FInputFrame Cast = AimAt(Duel);
		Cast.bPlantStaff = true;
		Cast.bCast = true;
		Cast.Slot = 0;
		StepPlayer(Duel, Cast, &Duel.Rules);
		bPass &= TestTrue(TEXT("crest cast under the dome"), Duel.Rules.IsPlanted(Duel.Player));
		bPass &= TestFalse(TEXT("crest cast is not absorbing"), PlayerOf(Duel).bAbsorb);
		bPass &= TestEqual(TEXT("one crest"), PlayerOf(Duel).Water.Crests, 1);
		bPass &= TestEqual(TEXT("crest spent the flow"), PlayerOf(Duel).Water.Flow, 0);
		bPass &= TestFalse(TEXT("crest was not refused"), HasRefusal(Duel.Rules, TEXT("crest")));
		const FProjectile* Shot = PlayerShot(Duel.State, Duel.Player);
		bPass &= TestNotNull(TEXT("crest bolt"), Shot);
		if (Shot)
		{
			const double Expected = Bolt->Damage * KernelData().CrestDamageMult;
			bPass &= Near(*this, TEXT("crest bolt is 2.05 * 1.5"), Shot->Damage, Expected);
			bPass &= Near(*this, TEXT("crest bolt damage"), Shot->Damage, 3.075);
		}
	}

	{
		FDuel Duel;
		if (!OpenDuel(*this, Duel, 14.0))
		{
			return false;
		}
		PlayerOf(Duel).Tier = 4;
		const FSpell* Rain = SpellFor(PlayerOf(Duel), 1);
		if (!TestNotNull(TEXT("rain of orbs"), Rain))
		{
			return false;
		}
		bPass &= TestEqual(TEXT("tide orb IV B"), Rain->Id, FString(TEXT("tide_orb:4:B")));
		bPass &= TestTrue(TEXT("tier IV is above the one-hand gate"), Rain->Tier > Duel.Rules.Split.MaxTier);
		FInputFrame Cast = AimAt(Duel);
		Cast.bPlantStaff = true;
		Cast.bCast = true;
		Cast.Slot = 1;
		StepPlayer(Duel, Cast, &Duel.Rules);
		bPass &= TestTrue(TEXT("tier IV under the dome"), Duel.Rules.IsPlanted(Duel.Player));
		bPass &= TestEqual(TEXT("tier IV cast"), PlayerOf(Duel).Metrics.Casts, 1);
		bPass &= TestEqual(TEXT("first line does not gain flow"), PlayerOf(Duel).Water.Flow, 0);
		bPass &= TestFalse(TEXT("tier IV was not refused"), HasRefusal(Duel.Rules, TEXT("tier")));
		bool bSawTier = false;
		for (const FArenaEvent& Event : Duel.State.Events)
		{
			if (Event.Kind == TEXT("cast") && Event.ActorId == Duel.Player && Event.Value == 4.0)
			{
				bSawTier = true;
			}
		}
		bPass &= TestTrue(TEXT("cast event tier 4"), bSawTier);
		bPass &= TestTrue(TEXT("rain is still winding"), PlayerOf(Duel).Pending.IsSet());
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesPinnedPath, "MageArena.Defences.PinnedPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesPinnedPath::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FDuel Duel;
	if (!OpenDuel(*this, Duel, 14.0))
	{
		return false;
	}
	bool bPass = true;
	// A full pool hides regen. 40 leaves room to see a plant cost of 8 if the null path spent it.
	// Focus 1 regen is 0.125 per tick: stats.csv manaRegen = 6 + 1.5*rank, combat.json simStepHz 60.
	PlayerOf(Duel).Mana = 40.0;
	bPass &= TestEqual(TEXT("default focus"), PlayerOf(Duel).Ranks.Focus, 1);
	const double Before = PlayerOf(Duel).Mana;
	SpawnIncoming(Duel.State, Duel.Foe, FSimVec{10.55, 10.0}, FSimVec{-1.0, 0.0}, 20.0, TEXT("magic"));
	FInputFrame Plant = AimAt(Duel);
	Plant.bPlantStaff = true;
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(Duel.Player, Plant);
	FDefenceLogProbe PlantLog;
	{
		FDefenceLogScope Scope(PlantLog);
		StepArena(Duel.State, Inputs);
	}
	bPass &= Near(*this, TEXT("null rules ignore the plant"), PlayerOf(Duel).Mana - Before, 0.125);
	// A planted dome would leave 20 * (1 - 0.85) = 3. The null path has no dome.
	bPass &= Near(*this, TEXT("null path does not dome a magic hit"), PlayerOf(Duel).Metrics.DamageTaken, 20.0);
	bPass &= TestEqual(TEXT("null plant emits no defence line"), PlantLog.Lines, 0);

	FDuel Locked;
	if (!OpenDuel(*this, Locked, 14.0))
	{
		return false;
	}
	FInputFrame Cast = AimAt(Locked);
	Cast.bAbsorb = true;
	Cast.bCast = true;
	Cast.Slot = 0;
	TMap<int32, FInputFrame> LockedInputs;
	LockedInputs.Add(Locked.Player, Cast);
	FDefenceLogProbe CastLog;
	{
		FDefenceLogScope Scope(CastLog);
		StepArena(Locked.State, LockedInputs);
	}
	bPass &= TestTrue(TEXT("ward still locks the cast"), PlayerOf(Locked).bAbsorb);
	bPass &= TestEqual(TEXT("null rules do not split-cast"), PlayerOf(Locked).Metrics.Casts, 0);
	// No log assertion here: a legal split Bolt emits no defence line under either path, so it could not fail.
	// The cast count above is the check that tells the two paths apart (active rules cast the Bolt).
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesWardWhileSigil, "MageArena.Defences.WardWhileSigil",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesWardWhileSigil::RunTest(const FString& Parameters)
{
	FHandRig Rig;
	if (!Rig.Start(*this, true, true, false))
	{
		Rig.Finish();
		return false;
	}
	int32 Casts = 0;
	FName Line = NAME_None;
	Rig.Sigils->OnSigilCast.AddLambda([&Casts, &Line](FName InLine, float, double)
	{
		++Casts;
		Line = InLine;
	});

	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	const double Frame = 1.0 / 72.0;
	int32 Guard = 0;
	while (Rig.Wards->GetRaiseCount() < 1 && Guard < 720)
	{
		Rig.Hands->Step(Frame);
		++Guard;
	}
	bool bPass = TestTrue(TEXT("ward raised before the sigil"), Rig.Wards->GetRaiseCount() >= 1);
	bPass &= TestTrue(TEXT("ward pose is up"), Rig.Wards->GetState().bRaised);
	const int32 Lowers = Rig.Wards->GetLowerCount();

	Rig.Hands->PlayQuickAction(TEXT("sigil-line2"), EClipVariant::Normal);
	bPass &= TestTrue(TEXT("ward stays held when the sigil starts"), Rig.Hands->IsWardHeld());
	bPass &= TestTrue(TEXT("sigil is the action"), Rig.Hands->GetActionName() == TEXT("sigil-line2"));
	if (!StepAction(*this, Rig.Hands, TEXT("sigil-line2")))
	{
		Rig.Finish();
		return false;
	}

	bPass &= TestTrue(TEXT("ward still held after the sigil"), Rig.Hands->IsWardHeld());
	bPass &= TestTrue(TEXT("ward pose stayed up"), Rig.Wards->GetState().bRaised);
	bPass &= TestEqual(TEXT("sigil did not lower the ward"), Rig.Wards->GetLowerCount(), Lowers);
	bPass &= TestEqual(TEXT("one sigil cast"), Casts, 1);
	bPass &= TestTrue(TEXT("cast line2"), Line == FName(TEXT("line2")));
	UE_LOG(LogMageArena, Log, TEXT("Defence ward-while-sigil raises=%d lowers=%d casts=%d line=%s"),
		Rig.Wards->GetRaiseCount(), Rig.Wards->GetLowerCount(), Casts, *Line.ToString());
	Rig.Finish();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDefencesStaffClips, "MageArena.Defences.StaffClips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDefencesStaffClips::RunTest(const FString& Parameters)
{
	const EClipVariant Variants[] = {EClipVariant::Normal, EClipVariant::Slow, EClipVariant::Sloppy};
	const TCHAR* Names[] = {TEXT("normal"), TEXT("slow"), TEXT("sloppy")};
	bool bPass = true;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FHandRig Rig;
		if (!Rig.Start(*this, false, false, true))
		{
			Rig.Finish();
			return false;
		}
		int32 HeardPlant = 0;
		int32 HeardLift = 0;
		const FDelegateHandle PlantHeard = Rig.Staff->OnPlant.AddLambda([&HeardPlant]() { ++HeardPlant; });
		const FDelegateHandle LiftHeard = Rig.Staff->OnLift.AddLambda([&HeardLift]() { ++HeardLift; });
		Rig.Hands->PlayQuickAction(TEXT("staff-plant"), Variants[Index]);
		if (!StepAction(*this, Rig.Hands, TEXT("staff-plant")))
		{
			Rig.Staff->OnPlant.Remove(PlantHeard);
			Rig.Staff->OnLift.Remove(LiftHeard);
			Rig.Finish();
			return false;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s plant once"), Names[Index]), Rig.Staff->GetDetector().GetPlantCount(), 1);
		bPass &= TestEqual(*FString::Printf(TEXT("%s plant does not lift"), Names[Index]), Rig.Staff->GetDetector().GetLiftCount(), 0);
		bPass &= TestEqual(*FString::Printf(TEXT("%s subsystem heard the plant"), Names[Index]), HeardPlant, 1);
		bPass &= TestEqual(*FString::Printf(TEXT("%s subsystem heard no lift"), Names[Index]), HeardLift, 0);

		Rig.Hands->PlayQuickAction(TEXT("staff-lift"), Variants[Index]);
		if (!StepAction(*this, Rig.Hands, TEXT("staff-lift")))
		{
			Rig.Staff->OnPlant.Remove(PlantHeard);
			Rig.Staff->OnLift.Remove(LiftHeard);
			Rig.Finish();
			return false;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s lift once"), Names[Index]), Rig.Staff->GetDetector().GetLiftCount(), 1);
		bPass &= TestEqual(*FString::Printf(TEXT("%s lift does not plant again"), Names[Index]), Rig.Staff->GetDetector().GetPlantCount(), 1);
		bPass &= TestEqual(*FString::Printf(TEXT("%s subsystem heard the lift"), Names[Index]), HeardLift, 1);
		bPass &= TestEqual(*FString::Printf(TEXT("%s subsystem did not hear a second plant"), Names[Index]), HeardPlant, 1);
		Rig.Staff->OnPlant.Remove(PlantHeard);
		Rig.Staff->OnLift.Remove(LiftHeard);
		UE_LOG(LogMageArena, Log, TEXT("Defence staff %s plant=%d lift=%d"),
			Names[Index], Rig.Staff->GetDetector().GetPlantCount(), Rig.Staff->GetDetector().GetLiftCount());
		Rig.Finish();
	}

	for (int32 Index = 0; Index < 3; ++Index)
	{
		FHandRig Rig;
		if (!Rig.Start(*this, false, false, true))
		{
			Rig.Finish();
			return false;
		}
		int32 HeardPlant = 0;
		int32 HeardLift = 0;
		const FDelegateHandle PlantHeard = Rig.Staff->OnPlant.AddLambda([&HeardPlant]() { ++HeardPlant; });
		const FDelegateHandle LiftHeard = Rig.Staff->OnLift.AddLambda([&HeardLift]() { ++HeardLift; });
		Rig.Hands->PlayQuickAction(TEXT("mudra"), Variants[Index]);
		if (!StepAction(*this, Rig.Hands, TEXT("mudra")))
		{
			Rig.Staff->OnPlant.Remove(PlantHeard);
			Rig.Staff->OnLift.Remove(LiftHeard);
			Rig.Finish();
			return false;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s mudra does not plant"), Names[Index]), Rig.Staff->GetDetector().GetPlantCount(), 0);
		bPass &= TestEqual(*FString::Printf(TEXT("%s mudra does not lift"), Names[Index]), Rig.Staff->GetDetector().GetLiftCount(), 0);
		bPass &= TestEqual(*FString::Printf(TEXT("%s mudra stays silent"), Names[Index]), HeardPlant + HeardLift, 0);
		Rig.Staff->OnPlant.Remove(PlantHeard);
		Rig.Staff->OnLift.Remove(LiftHeard);
		Rig.Finish();
	}
	return bPass;
}

#endif
