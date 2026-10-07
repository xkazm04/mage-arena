#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// T22: the collar ledger. Sources for the literals:
// - apps/vr/data/vr/collar.json (proposals, owner sign-off pending): cracks perfect 1, reflect 1, reflectHitsCaster 1,
//   unblockableBlinked 2; tithe areaCastTier3Plus 2, missedCast 1, crest 1; wardstones fullGlowTithe 30.
// - spells-water.csv: tide_orb 1 Bubble Shot (projectile 12 m/s, range 12, damage 7); tide_orb 3 Twin Tides (two
//   projectiles at +-10 deg, 11 each); tide_orb 4 A Leviathan Orb (one slow projectile, r 1.5 m); tide_orb 4 B Rain of
//   Orbs (5-orb fan); lash 3 Maelstrom Lash (ring 360 deg r 3 m, damage 15, cast 0.30, telegraph 0.30); lash 4 Drown
//   Coil (cone, UNBLOCKABLE); mire 3 Freeze Field (ground r 3 m, damage 5); mire 4 Glacier Tomb (one target); mirror 4
//   Return Tide (wave); mend 3 Spring; mirror 3 Mirage.
// - stats.csv rank 1: max HP 95. combat.json simStepHz 60. The roll's i-frames are the pinned ImmuneUntil window.
// - pinned arena-tiers.json Tiro wave n 4 kind final.

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
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"
#include "Session/CollarLedger.h"
#include "Session/PlayerPreset.h"

namespace
{
const double kCollarTick = 1.0 / 60.0;

bool CollarReady(FAutomationTestBase& Test)
{
	if (KernelData().bReady)
	{
		return true;
	}
	Test.AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
	return false;
}

FCollarWeights FileWeights(FAutomationTestBase& Test)
{
	FCollarWeights Weights;
	FString Error;
	if (!LoadCollarWeights(Weights, Error))
	{
		Test.AddError(Error);
	}
	return Weights;
}

int32 CountKind(const FArenaState& State, const TCHAR* Kind)
{
	int32 Count = 0;
	for (const FArenaEvent& Event : State.Events)
	{
		Count += Event.Kind == Kind ? 1 : 0;
	}
	return Count;
}

const FArenaEvent* FirstKind(const FArenaState& State, const TCHAR* Kind)
{
	for (const FArenaEvent& Event : State.Events)
	{
		if (Event.Kind == Kind)
		{
			return &Event;
		}
	}
	return nullptr;
}

bool LoadRules(FAutomationTestBase& Test, FVrRuleset& Rules)
{
	FString Error;
	if (!LoadVrRuleset(Rules, Error, false))
	{
		Test.AddError(FString::Printf(TEXT("overlay loads: %s"), *Error));
		return false;
	}
	return true;
}

FComposition CassiaComposition(FAutomationTestBase& Test)
{
	FPlayerPreset Preset;
	FString Error;
	if (!LoadPlayerPreset(Preset, Error))
	{
		Test.AddError(Error);
	}
	return Preset.Composition;
}

// Feeds a whole event log to a fresh ledger the way the session does: a player's cast names the spell it started.
FCollarLedger Tally(const FArenaState& State, int32 PlayerId, const FCollarWeights& Weights, const FSpell* CastSpell)
{
	FCollarLedger Ledger;
	Ledger.SetWeights(Weights);
	Ledger.BeginBout();
	for (const FArenaEvent& Event : State.Events)
	{
		Ledger.Observe(Event, PlayerId, Event.Kind == TEXT("cast") ? CastSpell : nullptr, Event.Tick * kCollarTick);
	}
	return Ledger;
}

// The reflector at (10, 10) facing +X, a caster 3.51 m away, a magic tier 1 projectile (7 damage, 12 m/s) from the
// caster; the absorb rises three ticks before contact, inside the perfect window. As MageArena.Reflection.* does.
struct FCollarDuel
{
	FArenaState State;
	int32 Reflector = 0;
	int32 Caster = 0;
};

FCollarDuel ReflectDuel(const FComposition& Composition, FVrRuleset* Rules)
{
	FCollarDuel Duel;
	Duel.State = CreateArena(11);
	FActor& Reflector = AddMage(Duel.State, 0, FSimVec{10.0, 10.0});
	Reflector.Water = NewWaterState(&Composition);
	Reflector.Tier = 2;
	Reflector.LastUnlockTick = 1000000000;
	Reflector.Facing = FSimVec{1.0, 0.0};
	Duel.Reflector = Reflector.Id;
	FActor& Caster = AddMage(Duel.State, 1, FSimVec{13.51, 10.0});
	Caster.Facing = FSimVec{-1.0, 0.0};
	Duel.Caster = Caster.Id;
	FHit Hit;
	Hit.OwnerId = Caster.Id;
	Hit.ActivationId = Duel.State.NextId++;
	Hit.Damage = 7.0;
	Hit.Family = TEXT("magic");
	Hit.Tier = 1;
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
				bRaised = SimDistance(Projectile.Pos, Me->Pos) - Me->Radius - Projectile.Radius <= 3.0 * Step;
			}
		}
		FInputFrame Input = SimIdleInput(CasterPos);
		Input.bAbsorb = bRaised;
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Duel.Reflector, Input);
		StepArena(Duel.State, Inputs, Rules);
	}
	return Duel;
}

// A player at (10, 10) facing +X at tier Tier, with an opponent mage at (10 + FoeGapM, 10). The player casts the spell
// in Slot once, aimed at Aim, then idles for 150 ticks (2.5 s: every Water projectile has run out of range by then).
struct FCastRun
{
	FArenaState State;
	int32 Player = 0;
	int32 Foe = 0;
	const FSpell* Spell = nullptr;
};

int32 SlotFor(const FActor& Actor, const TCHAR* SpellId)
{
	for (int32 Slot = 0; Slot < 6; ++Slot)
	{
		const FSpell* Spell = SpellFor(Actor, Slot);
		if (Spell && Spell->Id == SpellId)
		{
			return Slot;
		}
	}
	return -1;
}

FCastRun CastOnce(const FComposition& Composition, const TCHAR* SpellId, int32 Tier, double FoeGapM, const FSimVec& Aim, FVrRuleset* Rules)
{
	FCastRun Run;
	Run.State = CreateArena(5);
	FActor& Player = AddMage(Run.State, 0, FSimVec{10.0, 10.0});
	Player.Water = NewWaterState(&Composition);
	Player.Tier = Tier;
	Player.LastUnlockTick = 1000000000;
	Player.Facing = FSimVec{1.0, 0.0};
	Run.Player = Player.Id;
	Run.Foe = AddMage(Run.State, 1, FSimVec{10.0 + FoeGapM, 10.0}).Id;
	const int32 Slot = SlotFor(*SimFindActor(Run.State, Run.Player), SpellId);
	Run.Spell = FindSpellById(SpellId);
	for (int32 Tick = 0; Tick < 151; ++Tick)
	{
		FInputFrame Input = SimIdleInput(Aim);
		Input.Slot = Slot < 0 ? 0 : Slot;
		Input.bCast = Tick == 0 && Slot >= 0;
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Run.Player, Input);
		StepArena(Run.State, Inputs, Rules);
	}
	return Run;
}

// A team 1 mage at (20, 10) owns one unblockable telegraph (tier 4, 25 damage, 1.5 m: the area's radius or the lane's
// width; a lane runs RangeM from the mage toward AimAt) aimed at AimAt, resolving in 5 ticks. The player sits at PlayerAt; bBlinking gives the player i-frames over the resolve; MoveTo (if set) moves
// the player there after the telegraph is placed (a blink to another pad).
struct FDodgeRun
{
	FArenaState State;
	int32 Player = 0;
	int32 Owner = 0;
};

FDodgeRun AreaDodge(const FSimVec& PlayerAt, const FSimVec& AimAt, bool bBlinking, TOptional<FSimVec> MoveTo, FVrRuleset* Rules,
	const TCHAR* Kind = TEXT("area"), double RangeM = 0.0)
{
	FDodgeRun Run;
	Run.State = CreateArena(3);
	Run.Player = AddMage(Run.State, 0, PlayerAt).Id;
	FActor& Owner = AddMage(Run.State, 1, FSimVec{20.0, 10.0});
	Run.Owner = Owner.Id;
	FTelegraph Telegraph;
	Telegraph.Id = Run.State.NextId++;
	Telegraph.ActivationId = Run.State.NextId++;
	Telegraph.OwnerId = Owner.Id;
	Telegraph.Family = TEXT("unblockable");
	Telegraph.Tier = 4;
	Telegraph.Damage = 25.0;
	Telegraph.Source = Owner.Pos;
	Telegraph.Kind = Kind;
	Telegraph.RangeM = RangeM;
	Telegraph.Origin = Owner.Pos;
	Telegraph.Target = AimAt;
	Telegraph.WidthM = 1.5;
	Telegraph.StartTick = Run.State.Tick;
	Telegraph.ResolveTick = Run.State.Tick + 5;
	Telegraph.bCommitted = true;
	Run.State.Telegraphs.Add(Telegraph);
	if (FActor* Player = SimFindActor(Run.State, Run.Player))
	{
		if (bBlinking)
		{
			Player->ImmuneUntil = 1000;
		}
		if (MoveTo.IsSet())
		{
			Player->Pos = MoveTo.GetValue();
			Player->PreviousPos = MoveTo.GetValue();
		}
	}
	for (int32 Tick = 0; Tick < 10; ++Tick)
	{
		StepArena(Run.State, TMap<int32, FInputFrame>(), Rules);
	}
	return Run;
}

// An opposing unblockable projectile (tier 4, 40 damage, r 1.5 m, 6 m/s, range 14: the Leviathan Orb row) from a team
// 1 mage at (20, 10) toward (10, 10). The player at PlayerAt, with i-frames over the whole flight when bBlinking.
FDodgeRun OrbDodge(const FSimVec& PlayerAt, bool bBlinking, FVrRuleset* Rules)
{
	FDodgeRun Run;
	Run.State = CreateArena(4);
	Run.Player = AddMage(Run.State, 0, PlayerAt).Id;
	FActor& Owner = AddMage(Run.State, 1, FSimVec{20.0, 10.0});
	Run.Owner = Owner.Id;
	FHit Hit;
	Hit.OwnerId = Owner.Id;
	Hit.ActivationId = Run.State.NextId++;
	Hit.Damage = 40.0;
	Hit.Family = TEXT("unblockable");
	Hit.Tier = 4;
	Hit.Source = Owner.Pos;
	const FSimVec From = Owner.Pos;
	SpawnProjectile(Run.State, Hit, From, SimSub(FSimVec{10.0, 10.0}, From), 6.0, 14.0, 1.5);
	if (bBlinking)
	{
		SimFindActor(Run.State, Run.Player)->ImmuneUntil = 100000;
	}
	for (int32 Tick = 0; Tick < 180; ++Tick)
	{
		StepArena(Run.State, TMap<int32, FInputFrame>(), Rules);
	}
	return Run;
}

struct FCollarRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;
	FString Dir;

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
		// Never Saved/MageArena: that is the desktop playthrough's save.
		Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("collar"), FGuid::NewGuid().ToString());
		IFileManager::Get().MakeDirectory(*Dir, true);
		return true;
	}

	void Bind(FArenaSession& Session) const
	{
		Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
		Session.SetSaveDirectory(Dir);
		Session.SetScripted(false);
	}

	void Close(FArenaSession& Session)
	{
		Session.Unbind();
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
			Instance = nullptr;
		}
	}
};

void RunWhile(FArenaSession& Session, const TCHAR* Stage, double LimitS)
{
	for (double Elapsed = 0.0; Elapsed < LimitS && Session.GetStage() == Stage; Elapsed += kCollarTick)
	{
		Session.Advance(kCollarTick, true);
	}
}

// From the offer (bout.txt names a bout) through the ritual into the bout: a palm continues, both palms offer the wrists.
bool IntoBout(FCollarRig& Rig, FArenaSession& Session)
{
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(kCollarTick, true);
	for (double Elapsed = 0.0; Elapsed < 5.0 && Session.GetPhaseClock() + 1.0e-9 < Session.GetDayTuning().RitualLineS; Elapsed += kCollarTick)
	{
		Session.Advance(kCollarTick, true);
	}
	Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
	RunWhile(Session, TEXT("ritual"), 5.0);
	RunWhile(Session, TEXT("intro"), 5.0);
	return Session.GetStage() == TEXT("active");
}

// Appends scripted kernel events for the player; the next kernel step's ledger pass reads them (the cursor pattern).
void Script(FArenaSession& Session, const TCHAR* Kind, int32 Count)
{
	FArenaState& State = const_cast<FArenaState&>(Session.GetGames().State);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FArenaEvent Event;
		Event.Tick = State.Tick;
		Event.Kind = Kind;
		Event.ActorId = Session.GetGames().PlayerId;
		State.Events.Add(Event);
	}
}

void WinNow(FArenaSession& Session)
{
	for (int32 Step = 0; Step < 120 && Session.GetStage() == TEXT("active"); ++Step)
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
		Session.Advance(kCollarTick, true);
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCollarWeights, "MageArena.Collar.Weights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCollarWeights::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCollarWeights W;
	FString Error;
	bool bPass = TestTrue(*FString::Printf(TEXT("collar.json loads (%s)"), *Error), LoadCollarWeights(W, Error));
	bPass &= TestEqual(TEXT("perfect 1"), W.Perfect, 1);
	bPass &= TestEqual(TEXT("reflect 1"), W.Reflect, 1);
	bPass &= TestEqual(TEXT("reflectHitsCaster 1"), W.ReflectHitsCaster, 1);
	bPass &= TestEqual(TEXT("unblockableBlinked 2"), W.UnblockableBlinked, 2);
	bPass &= TestEqual(TEXT("areaCastTier3Plus 2"), W.AreaCastTier3Plus, 2);
	bPass &= TestEqual(TEXT("missedCast 1"), W.MissedCast, 1);
	bPass &= TestEqual(TEXT("crest 1"), W.Crest, 1);
	bPass &= TestEqual(TEXT("fullGlowTithe 30"), W.WardstoneFullTithe, 30.0);

	FString Text;
	FFileHelper::LoadFileToString(Text, *CollarWeightsPath());
	bPass &= TestTrue(TEXT("every weight is marked a proposal"), Text.Contains(TEXT("Proposal, owner sign-off pending")));
	auto Variant = [&Text](const FString& From, const FString& To)
	{
		FString Out = Text;
		Out.ReplaceInline(*From, *To);
		return Out;
	};
	FCollarWeights V;
	// Reserved keys (T25 spare, T29 interrupt) are accepted and ignored.
	const FString Reserved = Variant(TEXT("\"perfect\": 1,"), TEXT("\"spare\": 3, \"interrupt\": 4, \"perfect\": 1,"));
	bPass &= TestTrue(TEXT("spare and interrupt are accepted"), ParseCollarWeights(Reserved, V, Error));
	bPass &= TestEqual(TEXT("reserved keys change nothing"), V.Perfect, 1);
	bPass &= TestFalse(TEXT("an unknown key fails"), ParseCollarWeights(Variant(TEXT("\"perfect\": 1,"), TEXT("\"bogus\": 1, \"perfect\": 1,")), V, Error));
	bPass &= TestTrue(TEXT("the unknown key is named"), Error.Contains(TEXT("bogus")));
	bPass &= TestFalse(TEXT("a weight without its _why fails"), ParseCollarWeights(Variant(TEXT("\"_perfect\":"), TEXT("\"_perfectX\":")), V, Error));
	bPass &= TestFalse(TEXT("a fractional weight fails"), ParseCollarWeights(Variant(TEXT("\"perfect\": 1,"), TEXT("\"perfect\": 1.5,")), V, Error));
	bPass &= TestFalse(TEXT("a negative weight fails"), ParseCollarWeights(Variant(TEXT("\"crest\": 1,"), TEXT("\"crest\": -1,")), V, Error));
	bPass &= TestFalse(TEXT("version 2 fails"), ParseCollarWeights(Variant(TEXT("\"version\": 1"), TEXT("\"version\": 2")), V, Error));
	bPass &= TestFalse(TEXT("not JSON fails"), ParseCollarWeights(TEXT("{ nope"), V, Error));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCollarEachSource, "MageArena.Collar.EachSourceOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCollarEachSource::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!CollarReady(*this))
	{
		return false;
	}
	FCollarLedger Ledger;
	Ledger.SetWeights(FileWeights(*this));
	Ledger.BeginBout();
	const int32 Me = 7;
	auto Event = [](const TCHAR* Kind, int32 Actor)
	{
		FArenaEvent Out;
		Out.Kind = Kind;
		Out.ActorId = Actor;
		return Out;
	};
	bool bPass = true;
	auto Expect = [this, &Ledger, &bPass](const TCHAR* What, int32 Cracks, int32 Tithe)
	{
		bPass &= TestEqual(*FString::Printf(TEXT("%s: cracks"), What), Ledger.GetBoutCracks(), Cracks);
		bPass &= TestEqual(*FString::Printf(TEXT("%s: tithe"), What), Ledger.GetBoutTithe(), Tithe);
	};
	Ledger.Observe(Event(TEXT("perfect"), Me), Me, nullptr, 1.0);
	Expect(TEXT("perfect +1 crack"), 1, 0);
	Ledger.Observe(Event(TEXT("reflect"), Me), Me, nullptr, 1.1);
	Expect(TEXT("reflect +1 crack"), 2, 0);
	Ledger.Observe(Event(TEXT("reflectHit"), Me), Me, nullptr, 1.2);
	Expect(TEXT("reflected hit on the caster +1 more"), 3, 0);
	Ledger.Observe(Event(TEXT("dodge"), Me), Me, nullptr, 1.3);
	Expect(TEXT("unblockable blinked +2 cracks"), 5, 0);
	bPass &= TestEqual(TEXT("last crack time"), Ledger.GetLastCrackSim(), 1.3);
	const FSpell* Maelstrom = FindSpellById(TEXT("lash:3:base"));
	const FSpell* Bubble = FindSpellById(TEXT("tide_orb:1:base"));
	if (!TestTrue(TEXT("Maelstrom Lash and Bubble Shot exist"), Maelstrom && Bubble))
	{
		return false;
	}
	Ledger.Observe(Event(TEXT("cast"), Me), Me, Maelstrom, 2.0);
	Expect(TEXT("Maelstrom Lash (tier 3 ring) +2 tithe"), 5, 2);
	Ledger.Observe(Event(TEXT("cast"), Me), Me, Bubble, 2.1);
	Expect(TEXT("Bubble Shot (tier 1) adds nothing"), 5, 2);
	Ledger.Observe(Event(TEXT("miss"), Me), Me, nullptr, 2.2);
	Expect(TEXT("missed cast +1 tithe"), 5, 3);
	Ledger.ObserveCrests(1, 2.3);
	Expect(TEXT("a Crest spent +1 tithe"), 5, 4);
	Ledger.ObserveCrests(1, 2.4);
	Expect(TEXT("the same Crest count adds nothing"), 5, 4);
	Ledger.ObserveCrests(0, 2.5);
	Expect(TEXT("a reset counter adds nothing"), 5, 4);
	bPass &= TestEqual(TEXT("last tithe time"), Ledger.GetLastTitheSim(), 2.3);
	// Another actor's events, and events that are not sources, add nothing.
	for (const TCHAR* Kind : {TEXT("perfect"), TEXT("reflect"), TEXT("reflectHit"), TEXT("dodge"), TEXT("miss"), TEXT("cast")})
	{
		Ledger.Observe(Event(Kind, Me + 1), Me, Maelstrom, 3.0);
	}
	Ledger.Observe(Event(TEXT("hit"), Me), Me, nullptr, 3.0);
	Ledger.Observe(Event(TEXT("unlock"), Me), Me, nullptr, 3.0);
	Expect(TEXT("other actors and other kinds"), 5, 4);
	for (int32 Index = 0; Index < static_cast<int32>(ECollarSource::Count); ++Index)
	{
		const ECollarSource Source = static_cast<ECollarSource>(Index);
		bPass &= TestEqual(*FString::Printf(TEXT("%s counted once"), FCollarLedger::SourceName(Source)), Ledger.GetBoutCount(Source), 1);
	}
	Ledger.BeginBout();
	Expect(TEXT("a new bout starts at zero"), 0, 0);

	// The shapes that count as an area (or several projectiles) at tier III-IV, from the pinned catalog.
	for (const TCHAR* Id : {TEXT("lash:3:base"), TEXT("lash:4:base"), TEXT("mire:3:base"), TEXT("tide_orb:3:base"), TEXT("tide_orb:4:B"), TEXT("mirror:4:base")})
	{
		const FSpell* Spell = FindSpellById(Id);
		bPass &= TestTrue(*FString::Printf(TEXT("%s is a tier III-IV area cast"), Id), Spell && FCollarLedger::IsAreaCastTier3Plus(*Spell));
	}
	for (const TCHAR* Id : {TEXT("tide_orb:4:A"), TEXT("mire:4:base"), TEXT("mend:3:base"), TEXT("mirror:3:base"), TEXT("lash:1:base"), TEXT("tide_orb:2:base")})
	{
		const FSpell* Spell = FindSpellById(Id);
		bPass &= TestTrue(*FString::Printf(TEXT("%s is not"), Id), Spell && !FCollarLedger::IsAreaCastTier3Plus(*Spell));
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCollarReflectedHit, "MageArena.Collar.ReflectedHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCollarReflectedHit::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FVrRuleset Rules;
	if (!CollarReady(*this) || !LoadRules(*this, Rules))
	{
		return false;
	}
	// The cassia preset (Mirror II A) at tier 2 perfects a tier 1 Bubble Shot: it is reflected and hits its caster for 7.
	const FCollarDuel Duel = ReflectDuel(CassiaComposition(*this), &Rules);
	bool bPass = TestEqual(TEXT("one perfect"), CountKind(Duel.State, TEXT("perfect")), 1);
	bPass &= TestEqual(TEXT("one reflect"), CountKind(Duel.State, TEXT("reflect")), 1);
	const FArenaEvent* Hit = FirstKind(Duel.State, TEXT("reflectHit"));
	if (TestNotNull(TEXT("a reflectHit event"), Hit))
	{
		bPass &= TestEqual(TEXT("reflectHit actor is the reflector"), Hit->ActorId, Duel.Reflector);
		bPass &= TestEqual(TEXT("reflectHit target is the original caster"), Hit->TargetId.Get(-1), Duel.Caster);
		bPass &= TestEqual(TEXT("reflectHit value is the Bubble Shot's 7 damage"), Hit->Value, 7.0);
	}
	else
	{
		bPass = false;
	}
	bPass &= TestEqual(TEXT("one reflectHit"), CountKind(Duel.State, TEXT("reflectHit")), 1);
	bPass &= TestEqual(TEXT("the caster is at 95 - 7"), SimFindActor(Duel.State, Duel.Caster)->Hp, 88.0);
	// The reflected projectile hit, so it is no miss of the reflector's; the caster's own orb was absorbed (the caster is
	// team 1, not the player's side) and emits no miss either.
	bPass &= TestEqual(TEXT("no miss"), CountKind(Duel.State, TEXT("miss")), 0);
	const FCollarLedger Ledger = Tally(Duel.State, Duel.Reflector, FileWeights(*this), nullptr);
	bPass &= TestEqual(TEXT("perfect 1 + reflect 1 + reflected hit 1 = 3 cracks"), Ledger.GetBoutCracks(), 3);
	bPass &= TestEqual(TEXT("no tithe"), Ledger.GetBoutTithe(), 0);
	bPass &= TestTrue(TEXT("the ruleset forgot the reflected projectile"), Rules.ReflectedCasters.Num() == 0);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCollarMissedCasts, "MageArena.Collar.MissedCasts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCollarMissedCasts::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FVrRuleset Rules;
	if (!CollarReady(*this) || !LoadRules(*this, Rules))
	{
		return false;
	}
	const FComposition Cassia = CassiaComposition(*this);
	const FCollarWeights Weights = FileWeights(*this);
	bool bPass = true;

	// Maelstrom Lash (tier 3, ring r 3 m) with the only foe 8 m away: the ring damages nobody.
	{
		const FCastRun Run = CastOnce(Cassia, TEXT("lash:3:base"), 3, 8.0, FSimVec{18.0, 10.0}, &Rules);
		const FArenaEvent* Cast = FirstKind(Run.State, TEXT("cast"));
		const FArenaEvent* Miss = FirstKind(Run.State, TEXT("miss"));
		bPass &= TestTrue(TEXT("Maelstrom: the player cast tier 3"), Cast && Cast->ActorId == Run.Player && Cast->Value == 3.0);
		if (TestNotNull(TEXT("Maelstrom: a miss event"), Miss))
		{
			bPass &= TestEqual(TEXT("Maelstrom: miss actor is the caster"), Miss->ActorId, Run.Player);
			bPass &= TestEqual(TEXT("Maelstrom: miss value is the tier"), Miss->Value, 3.0);
		}
		else
		{
			bPass = false;
		}
		bPass &= TestEqual(TEXT("Maelstrom: one miss"), CountKind(Run.State, TEXT("miss")), 1);
		const FCollarLedger Ledger = Tally(Run.State, Run.Player, Weights, Run.Spell);
		bPass &= TestEqual(TEXT("an area tier III cast that misses: 2 + 1 tithe"), Ledger.GetBoutTithe(), 3);
		bPass &= TestEqual(TEXT("and no cracks"), Ledger.GetBoutCracks(), 0);
	}
	// The same ring with the foe 2 m away damages it: no miss, the area cast still pays 2.
	{
		const FCastRun Run = CastOnce(Cassia, TEXT("lash:3:base"), 3, 2.0, FSimVec{12.0, 10.0}, &Rules);
		bPass &= TestEqual(TEXT("Maelstrom on the foe: no miss"), CountKind(Run.State, TEXT("miss")), 0);
		bPass &= TestTrue(TEXT("Maelstrom on the foe: the foe was hurt"), SimFindActor(Run.State, Run.Foe)->Hp < 95.0);
		bPass &= TestEqual(TEXT("Maelstrom on the foe: 2 tithe"), Tally(Run.State, Run.Player, Weights, Run.Spell).GetBoutTithe(), 2);
	}
	// Twin Tides (tier 3, two projectiles) aimed away from the foe: both run out of range, one miss for the cast.
	{
		const FCastRun Run = CastOnce(Cassia, TEXT("tide_orb:3:base"), 3, 5.0, FSimVec{10.0, 20.0}, &Rules);
		bPass &= TestEqual(TEXT("Twin Tides: one cast"), CountKind(Run.State, TEXT("cast")), 1);
		bPass &= TestEqual(TEXT("Twin Tides away: one miss for both projectiles"), CountKind(Run.State, TEXT("miss")), 1);
		bPass &= TestEqual(TEXT("Twin Tides away: 2 + 1 tithe"), Tally(Run.State, Run.Player, Weights, Run.Spell).GetBoutTithe(), 3);
	}
	// Twin Tides at the foe 2 m ahead: at least one orb lands, so the cast is no miss.
	{
		const FCastRun Run = CastOnce(Cassia, TEXT("tide_orb:3:base"), 3, 2.0, FSimVec{12.0, 10.0}, &Rules);
		bPass &= TestTrue(TEXT("Twin Tides at the foe: the foe was hurt"), SimFindActor(Run.State, Run.Foe)->Hp < 95.0);
		bPass &= TestEqual(TEXT("Twin Tides at the foe: no miss"), CountKind(Run.State, TEXT("miss")), 0);
	}
	// A bolt (tier 0) into the empty sand is a missed cast too: 1 tithe.
	{
		const FCastRun Run = CastOnce(Cassia, TEXT("bolt:0:base"), 1, 5.0, FSimVec{10.0, 20.0}, &Rules);
		const FArenaEvent* Miss = FirstKind(Run.State, TEXT("miss"));
		bPass &= TestTrue(TEXT("bolt away: a miss of tier 0"), Miss && Miss->Value == 0.0 && Miss->ActorId == Run.Player);
		bPass &= TestEqual(TEXT("bolt away: 1 tithe"), Tally(Run.State, Run.Player, Weights, FindSpellById(TEXT("bolt:0:base"))).GetBoutTithe(), 1);
	}
	bPass &= TestEqual(TEXT("the ruleset holds no finished activation"), Rules.CollarDamaged.Num(), 0);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCollarUnblockableDodge, "MageArena.Collar.UnblockableDodge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCollarUnblockableDodge::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FVrRuleset Rules;
	if (!CollarReady(*this) || !LoadRules(*this, Rules))
	{
		return false;
	}
	const FCollarWeights Weights = FileWeights(*this);
	const FSimVec Seat{10.0, 10.0};
	bool bPass = true;
	// Blinking through the black core: the area resolves on the player inside the i-frames.
	{
		const FDodgeRun Run = AreaDodge(Seat, Seat, true, {}, &Rules);
		const FArenaEvent* Dodge = FirstKind(Run.State, TEXT("dodge"));
		if (TestNotNull(TEXT("blinked: a dodge event"), Dodge))
		{
			bPass &= TestEqual(TEXT("blinked: dodge actor is the player"), Dodge->ActorId, Run.Player);
			bPass &= TestEqual(TEXT("blinked: dodge target is the caster"), Dodge->TargetId.Get(-1), Run.Owner);
			bPass &= TestEqual(TEXT("blinked: dodge value is the tier"), Dodge->Value, 4.0);
		}
		else
		{
			bPass = false;
		}
		bPass &= TestEqual(TEXT("blinked: unhurt"), SimFindActor(Run.State, Run.Player)->Hp, 95.0);
		bPass &= TestEqual(TEXT("blinked: 2 cracks"), Tally(Run.State, Run.Player, Weights, nullptr).GetBoutCracks(), 2);
	}
	// Leaving the area: aimed at the seat, the player is 3 m away (another pad) when it resolves.
	{
		const FDodgeRun Run = AreaDodge(Seat, Seat, false, FSimVec{10.0, 13.0}, &Rules);
		bPass &= TestEqual(TEXT("left the area: one dodge"), CountKind(Run.State, TEXT("dodge")), 1);
		bPass &= TestEqual(TEXT("left the area: 2 cracks"), Tally(Run.State, Run.Player, Weights, nullptr).GetBoutCracks(), 2);
	}
	// Staying in it is a hit, not a dodge.
	{
		const FDodgeRun Run = AreaDodge(Seat, Seat, false, {}, &Rules);
		bPass &= TestEqual(TEXT("stayed: no dodge"), CountKind(Run.State, TEXT("dodge")), 0);
		bPass &= TestEqual(TEXT("stayed: 95 - 25"), SimFindActor(Run.State, Run.Player)->Hp, 70.0);
	}
	// A lane aimed at the seat that falls 6 m short (4 m long from 10 m away) never threatened the player: no dodge.
	// The same lane at full reach, left by a blink to the next pad, is one.
	{
		const FDodgeRun Short = AreaDodge(Seat, Seat, false, {}, &Rules, TEXT("lane"), 4.0);
		bPass &= TestEqual(TEXT("short lane: no dodge"), CountKind(Short.State, TEXT("dodge")), 0);
		const FDodgeRun Left = AreaDodge(Seat, Seat, false, FSimVec{10.0, 13.0}, &Rules, TEXT("lane"), 12.0);
		bPass &= TestEqual(TEXT("full lane left: one dodge"), CountKind(Left.State, TEXT("dodge")), 1);
		const FDodgeRun Taken = AreaDodge(Seat, Seat, false, {}, &Rules, TEXT("lane"), 12.0);
		bPass &= TestTrue(TEXT("full lane taken: no dodge, 95 - 25"), CountKind(Taken.State, TEXT("dodge")) == 0 && SimFindActor(Taken.State, Taken.Player)->Hp == 70.0);
	}
	// An unblockable orb (the Leviathan row): through it in i-frames, or never on its path.
	{
		const FDodgeRun Through = OrbDodge(Seat, true, &Rules);
		bPass &= TestEqual(TEXT("orb blinked through: one dodge"), CountKind(Through.State, TEXT("dodge")), 1);
		bPass &= TestEqual(TEXT("orb blinked through: unhurt"), SimFindActor(Through.State, Through.Player)->Hp, 95.0);
		const FDodgeRun Off = OrbDodge(FSimVec{10.0, 13.0}, false, &Rules);
		bPass &= TestEqual(TEXT("orb left: one dodge when it runs out"), CountKind(Off.State, TEXT("dodge")), 1);
		const FDodgeRun Hit = OrbDodge(Seat, false, &Rules);
		bPass &= TestEqual(TEXT("orb taken: no dodge"), CountKind(Hit.State, TEXT("dodge")), 0);
		bPass &= TestEqual(TEXT("orb taken: 95 - 40"), SimFindActor(Hit.State, Hit.Player)->Hp, 55.0);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCollarNullPathSilent, "MageArena.Collar.NullPathSilent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCollarNullPathSilent::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!CollarReady(*this))
	{
		return false;
	}
	// Every scenario above, without a ruleset (the conformance path): the kernel plays them the same way and emits no
	// miss, dodge or reflectHit. The 45 MageArena.Kernel.Conformance.* vectors are the byte-level evidence.
	const FComposition Cassia = CassiaComposition(*this);
	const FSimVec Seat{10.0, 10.0};
	TArray<FArenaState> States;
	States.Add(ReflectDuel(Cassia, nullptr).State);
	States.Add(CastOnce(Cassia, TEXT("lash:3:base"), 3, 8.0, FSimVec{18.0, 10.0}, nullptr).State);
	States.Add(CastOnce(Cassia, TEXT("tide_orb:3:base"), 3, 5.0, FSimVec{10.0, 20.0}, nullptr).State);
	States.Add(CastOnce(Cassia, TEXT("bolt:0:base"), 1, 5.0, FSimVec{10.0, 20.0}, nullptr).State);
	States.Add(AreaDodge(Seat, Seat, true, {}, nullptr).State);
	States.Add(AreaDodge(Seat, Seat, false, FSimVec{10.0, 13.0}, nullptr).State);
	States.Add(OrbDodge(Seat, true, nullptr).State);
	States.Add(OrbDodge(FSimVec{10.0, 13.0}, false, nullptr).State);
	bool bPass = true;
	int32 Casts = 0;
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		const FArenaState& State = States[Index];
		Casts += CountKind(State, TEXT("cast"));
		for (const TCHAR* Kind : {TEXT("miss"), TEXT("dodge"), TEXT("reflectHit"), TEXT("reflect")})
		{
			bPass &= TestEqual(*FString::Printf(TEXT("scenario %d: no %s on the null path"), Index, Kind), CountKind(State, Kind), 0);
		}
	}
	bPass &= TestEqual(TEXT("the three casts still happen"), Casts, 3);
	bPass &= TestEqual(TEXT("the null-path reflection still hits the caster"), SimFindActor(States[0], 2)->Hp, 88.0);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCollarLifetime, "MageArena.Collar.LifetimeSurvives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCollarLifetime::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCollarRig Rig;
	if (!CollarReady(*this) || !Rig.Open(*this))
	{
		return false;
	}
	const FString LedgerPath = FPaths::Combine(Rig.Dir, TEXT("collar.ledger.json"));
	FFileHelper::SaveStringToFile(TEXT("bout=0\n"), *FPaths::Combine(Rig.Dir, TEXT("bout.txt")));
	FArenaSession Session;
	Rig.Bind(Session);
	bool bPass = TestTrue(TEXT("day begins"), Session.BeginArc(1));
	bPass &= TestEqual(TEXT("an old install has no ledger: 0 cracks"), Session.GetCollar().GetLifetimeCracks(), 0);
	bPass &= TestEqual(TEXT("collar file next to bout.txt"), Session.CollarFilePath(), LedgerPath);
	bPass &= TestTrue(TEXT("into Bout 1"), IntoBout(Rig, Session));
	// Bout 1: two perfects and a blinked unblockable (2 + 2 = 4 cracks), three misses and a Crest (3 + 1 = 4 tithe).
	Script(Session, TEXT("perfect"), 2);
	Script(Session, TEXT("dodge"), 1);
	Script(Session, TEXT("miss"), 3);
	if (FActor* Player = SimFindActor(const_cast<FArenaState&>(Session.GetGames().State), Session.GetGames().PlayerId))
	{
		Player->Water.Crests += 1;
	}
	Session.Advance(kCollarTick, true);
	bPass &= TestEqual(TEXT("bout cracks 4"), Session.GetCollar().GetBoutCracks(), 4);
	bPass &= TestEqual(TEXT("bout tithe 4"), Session.GetCollar().GetBoutTithe(), 4);
	bPass &= TestEqual(TEXT("the day's tithe includes the live bout"), Session.GetDayTithe(), 4);
	WinNow(Session);
	bPass &= TestEqual(TEXT("Bout 1 won"), Session.GetStage(), FString(TEXT("intermission")));
	double Value = -1.0;
	bPass &= TestTrue(TEXT("w1.cracks 4"), Session.GetFlags().GetNumber(TEXT("arena.tiro.1.w1.cracks"), Value) && Value == 4.0);
	bPass &= TestTrue(TEXT("w1.tithe 4"), Session.GetFlags().GetNumber(TEXT("arena.tiro.1.w1.tithe"), Value) && Value == 4.0);
	bPass &= TestEqual(TEXT("day cracks 4"), Session.GetDayCracks(), 4);
	bPass &= TestEqual(TEXT("lifetime cracks 4"), Session.GetCollar().GetLifetimeCracks(), 4);
	FCollarLedger Saved;
	bPass &= TestTrue(TEXT("the ledger file loads"), Saved.LoadLifetime(LedgerPath));
	bPass &= TestEqual(TEXT("saved lifetime cracks 4"), Saved.GetLifetimeCracks(), 4);
	bPass &= TestEqual(TEXT("saved lifetime tithe 4"), Saved.GetLifetimeTithe(), 4);

	// Start over (both palms in the intermission): the day's tally goes with the flags, the lifetime Cracks stay.
	Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
	RunWhile(Session, TEXT("intermission"), 5.0);
	bPass &= TestEqual(TEXT("start over opens the teach"), Session.GetStage(), FString(TEXT("teach")));
	bPass &= TestFalse(TEXT("the day's cracks are gone"), Session.GetFlags().Has(TEXT("arena.tiro.1.w1.cracks")));
	bPass &= TestEqual(TEXT("the day's tithe is gone"), Session.GetDayTithe(), 0);
	bPass &= TestEqual(TEXT("lifetime cracks survive start over"), Session.GetCollar().GetLifetimeCracks(), 4);
	bPass &= TestTrue(TEXT("the ledger file survives start over"), Saved.LoadLifetime(LedgerPath) && Saved.GetLifetimeCracks() == 4);
	Session.Unbind();

	// A relaunch loads it back.
	FArenaSession Again;
	Rig.Bind(Again);
	bPass &= TestTrue(TEXT("relaunch"), Again.BeginArc(1));
	bPass &= TestEqual(TEXT("relaunch: lifetime cracks 4"), Again.GetCollar().GetLifetimeCracks(), 4);
	bPass &= TestEqual(TEXT("relaunch: lifetime tithe 4"), Again.GetCollar().GetLifetimeTithe(), 4);
	Again.Unbind();

	// A corrupt ledger is treated like a corrupt save: the collar starts at 0, and the next bout end rewrites the file.
	FFileHelper::SaveStringToFile(TEXT("{ \"version\": 1, \"lifetimeCracks\": \"many\" }"), *LedgerPath);
	FArenaSession Corrupt;
	Rig.Bind(Corrupt);
	bPass &= TestTrue(TEXT("a corrupt ledger does not stop the day"), Corrupt.BeginArc(1));
	bPass &= TestEqual(TEXT("corrupt: 0 cracks"), Corrupt.GetCollar().GetLifetimeCracks(), 0);
	FCollarLedger Probe;
	bPass &= TestFalse(TEXT("the corrupt file reads as corrupt"), Probe.LoadLifetime(LedgerPath));
	Rig.Close(Corrupt);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCollarTablet, "MageArena.Collar.TabletColumns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCollarTablet::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCollarRig Rig;
	if (!CollarReady(*this) || !Rig.Open(*this))
	{
		return false;
	}
	// Earlier days left 10 Cracks and 7 Tithe on the collar. Today: the final only (bout=3).
	FFileHelper::SaveStringToFile(TEXT("{ \"version\": 1, \"lifetimeCracks\": 10, \"lifetimeTithe\": 7 }"),
		*FPaths::Combine(Rig.Dir, TEXT("collar.ledger.json")));
	FFileHelper::SaveStringToFile(TEXT("bout=3\n"), *FPaths::Combine(Rig.Dir, TEXT("bout.txt")));
	FArenaSession Session;
	Rig.Bind(Session);
	bool bPass = TestTrue(TEXT("day begins"), Session.BeginArc(1));
	bPass &= TestEqual(TEXT("lifetime cracks 10 loaded"), Session.GetCollar().GetLifetimeCracks(), 10);
	bPass &= TestTrue(TEXT("into the final"), IntoBout(Rig, Session));
	bPass &= TestEqual(TEXT("no tablet during the bout"), Session.GetTabletText(), FString());
	// The final: a perfect and a reflection (2 cracks), two misses (2 tithe).
	Script(Session, TEXT("perfect"), 1);
	Script(Session, TEXT("reflect"), 1);
	Script(Session, TEXT("miss"), 2);
	Session.Advance(kCollarTick, true);
	WinNow(Session);
	bPass &= TestEqual(TEXT("aftermath"), Session.GetStage(), FString(TEXT("aftermath")));
	const FString Tablet = Session.GetTabletText();
	UE_LOG(LogTemp, Log, TEXT("Collar tablet:\n%s"), *Tablet);
	TArray<FString> Rows;
	Tablet.ParseIntoArray(Rows, TEXT("\n"), true);
	if (!TestEqual(TEXT("title, header, four bouts, day, lifetime"), Rows.Num(), 8))
	{
		Rig.Close(Session);
		return false;
	}
	bPass &= TestEqual(TEXT("title"), Rows[0], FString(TEXT("Tiro, day 1")));
	bPass &= TestEqual(TEXT("the header names both columns"), Rows[1], FString(TEXT("bout  kind  result  time  tries  Cracks  Tithe")));
	bPass &= TestEqual(TEXT("bout 1 not fought"), Rows[2], FString(TEXT("1  soldiers  -")));
	bPass &= TestTrue(*FString::Printf(TEXT("the final's row ends with its 2 cracks and 2 tithe: %s"), *Rows[5]),
		Rows[5].StartsWith(TEXT("4  final  won  ")) && Rows[5].EndsWith(TEXT("  x1  2  2")));
	bPass &= TestEqual(TEXT("the day's totals"), Rows[6], FString(TEXT("Day:  Cracks 2  Tithe 2")));
	bPass &= TestEqual(TEXT("the lifetime cracks (10 + 2)"), Rows[7], FString(TEXT("Collar, all days:  Cracks 12")));
	Rig.Close(Session);
	return bPass;
}

#endif
