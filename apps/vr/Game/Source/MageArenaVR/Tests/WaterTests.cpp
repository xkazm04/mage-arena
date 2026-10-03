#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Water.h"

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

struct FPair
{
	FArenaState State;
	int32 Actor = 0;
	int32 Target = 0;
};

// water.test.ts:3. lastUnlockTick 1e9 is that fixture, matching kernel.ts NeverTick's magnitude.
FPair MakeSetup(const TCHAR* Line, int32 Tier, const TCHAR* Branch, double Range)
{
	FPair Pair;
	Pair.State = CreateArena(11);
	FComposition Composition = KernelData().Presets[0];
	Composition.Lines.Reset();
	Composition.Lines.Add(Line);
	for (const FString& Other : WaterLines())
	{
		if (Other != Line && Composition.Lines.Num() < KernelData().LineSlots)
		{
			Composition.Lines.Add(Other);
		}
	}
	Composition.Branches.Lash = Branch;
	Composition.Branches.Mirror = Branch;
	Composition.Branches.TideOrb = Branch;
	Pair.Actor = AddMage(Pair.State, 0, FSimVec{10.0, 10.0}).Id;
	Pair.Target = AddMage(Pair.State, 1, FSimVec{10.0 + Range, 10.0}).Id;
	FActor& Actor = *SimFindActor(Pair.State, Pair.Actor);
	Actor.Water = NewWaterState(&Composition);
	Actor.Tier = Tier;
	Actor.LastUnlockTick = 1000000000;
	return Pair;
}

void Advance(FArenaState& State, int32 Count)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		StepArena(State);
	}
}

FHit MakeHit(int32 Owner, int32 Activation, double Damage, const TCHAR* Family, int32 Tier, const FSimVec& Source)
{
	FHit Hit;
	Hit.OwnerId = Owner;
	Hit.ActivationId = Activation;
	Hit.Damage = Damage;
	Hit.Family = Family;
	Hit.Tier = Tier;
	Hit.Source = Source;
	return Hit;
}

void Cast(FArenaState& State, int32 ActorId, int32 TargetId, int32 Slot = 1)
{
	FInputFrame Input = SimIdleInput(SimFindActor(State, TargetId)->Pos);
	Input.Slot = Slot;
	Input.bCast = true;
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(ActorId, Input);
	StepArena(State, Inputs);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelCatalog, "MageArena.Kernel.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelCatalog::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const TArray<FSpell>& Spells = KernelData().Spells;
	bool bPass = TestEqual(TEXT("spell count"), Spells.Num(), 24);
	bPass &= TestEqual(TEXT("lint"), LintSpells().Num(), 0);
	const TCHAR* Branched[] = {
		TEXT("tide_orb:4:A"), TEXT("tide_orb:4:B"), TEXT("lash:2:A"), TEXT("lash:2:B"), TEXT("mirror:2:A"), TEXT("mirror:2:B")};
	TArray<FString> Found;
	for (const FSpell& Spell : Spells)
	{
		if (!Spell.Branch.IsEmpty())
		{
			Found.Add(FString::Printf(TEXT("%s:%d:%s"), *Spell.Line, Spell.Tier, *Spell.Branch));
		}
	}
	bPass &= TestEqual(TEXT("branch rows"), Found.Num(), 6);
	for (int32 Index = 0; Index < Found.Num() && Index < 6; ++Index)
	{
		bPass &= TestEqual(*FString::Printf(TEXT("branch %d"), Index), Found[Index], FString(Branched[Index]));
	}
	for (const FComposition& Preset : KernelData().Presets)
	{
		bPass &= TestEqual(*FString::Printf(TEXT("%s validates"), *Preset.Name), ValidateComposition(Preset).Num(), 0);
	}
	TArray<FSpell> Mutation = Spells;
	for (FSpell& Spell : Mutation)
	{
		if (Spell.Family == TEXT("unblockable"))
		{
			Spell.TelegraphS = 0.0;
			break;
		}
	}
	for (FSpell& Spell : Mutation)
	{
		if (Spell.Line == TEXT("mire"))
		{
			Spell.TelegraphS = 0.0;
			break;
		}
	}
	for (FSpell& Spell : Mutation)
	{
		if (Spell.Line == TEXT("mend"))
		{
			Spell.Branch = TEXT("A");
			break;
		}
	}
	Mutation.Pop();
	bPass &= TestEqual(TEXT("mutation lint"), LintSpells(&Mutation).Num(), 4);
	FComposition Duplicate = KernelData().Presets[0];
	Duplicate.Lines = {TEXT("mire"), TEXT("mire"), TEXT("mend")};
	bPass &= TestTrue(TEXT("duplicate lines fail"), ValidateComposition(Duplicate).Num() > 0);

	FPair Pair = MakeSetup(TEXT("lash"), 2, TEXT("B"), 2.6);
	const FSpell* Riptide = SpellFor(*SimFindActor(Pair.State, Pair.Actor), 1);
	bPass &= TestNotNull(TEXT("riptide"), Riptide);
	if (Riptide)
	{
		bPass &= TestEqual(TEXT("lash II B"), Riptide->Id, FString(TEXT("lash:2:B")));
		bPass &= TestEqual(TEXT("riptide name"), Riptide->Name, FString(TEXT("Riptide")));
	}
	SimFindActor(Pair.State, Pair.Actor)->Tier = 3;
	const FSpell* Maelstrom = SpellFor(*SimFindActor(Pair.State, Pair.Actor), 1);
	bPass &= TestNotNull(TEXT("maelstrom"), Maelstrom);
	if (Maelstrom)
	{
		bPass &= TestEqual(TEXT("lash III"), Maelstrom->Id, FString(TEXT("lash:3:base")));
		bPass &= TestEqual(TEXT("maelstrom name"), Maelstrom->Name, FString(TEXT("Maelstrom Lash")));
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelFlow, "MageArena.Kernel.Flow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelFlow::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	{
		FPair Pair = MakeSetup(TEXT("tide_orb"), 1, TEXT("A"), 2.6);
		Cast(Pair.State, Pair.Actor, Pair.Target);
		SimFindActor(Pair.State, Pair.Actor)->Tier = 2;
		const FActor& Caster = *SimFindActor(Pair.State, Pair.Actor);
		bPass &= TestTrue(TEXT("pending kept"), Caster.Pending.IsSet());
		if (Caster.Pending.IsSet() && Caster.Pending->SpellId.IsSet())
		{
			bPass &= TestEqual(TEXT("tier snapshot"), Caster.Pending->SpellId.GetValue(), FString(TEXT("tide_orb:1:base")));
		}
		Advance(Pair.State, SimTicks(0.5));
		const FSpell* Bubble = FindSpellById(TEXT("tide_orb:1:base"));
		const FActor& Target = *SimFindActor(Pair.State, Pair.Target);
		bPass &= TestNotNull(TEXT("bubble"), Bubble);
		if (Bubble)
		{
			bPass &= Near(*this, TEXT("bubble damage"), Target.MaxHp - Target.Hp, Bubble->Damage);
		}
		FActor& Again = *SimFindActor(Pair.State, Pair.Actor);
		const double Mana = Again.Mana;
		FInputFrame Input = SimIdleInput(SimFindActor(Pair.State, Pair.Target)->Pos);
		Input.Slot = 1;
		Input.bCast = true;
		bPass &= TestFalse(TEXT("cooldown refuses"), TrySpellCast(Pair.State, Again, Input));
		bPass &= Near(*this, TEXT("mana unchanged"), Again.Mana, Mana);
	}
	{
		FPair Pair = MakeSetup(TEXT("tide_orb"), 4, TEXT("A"), 2.6);
		Cast(Pair.State, Pair.Actor, Pair.Target);
		Advance(Pair.State, SimTicks(0.7));
		bPass &= TestEqual(TEXT("still warning"), Pair.State.Projectiles.Num(), 0);
		bPass &= TestTrue(TEXT("still pending"), SimFindActor(Pair.State, Pair.Actor)->Pending.IsSet());
		Interrupt(Pair.State, *SimFindActor(Pair.State, Pair.Actor));
		Advance(Pair.State, SimTicks(1.0));
		const FActor& Target = *SimFindActor(Pair.State, Pair.Target);
		bPass &= Near(*this, TEXT("interrupt prevents damage"), Target.Hp, Target.MaxHp);
		bPass &= TestTrue(TEXT("cooldown survives interrupt"), SimCooldownUntil(SimFindActor(Pair.State, Pair.Actor)->Water, TEXT("tide_orb")) > Pair.State.Tick);
	}
	{
		FPair Pair = MakeSetup(TEXT("tide_orb"), 1, TEXT("A"), 2.6);
		FActor& Caster = *SimFindActor(Pair.State, Pair.Actor);
		Caster.Water.Composition.Lines = {TEXT("tide_orb"), TEXT("lash"), TEXT("mirror")};
		FActor& Target = *SimFindActor(Pair.State, Pair.Target);
		Target.Hp = 10000.0;
		Target.MaxHp = 10000.0;
		const int32 Slots[] = {0, 1, 2, 0, 1, 2};
		for (int32 Slot : Slots)
		{
			Cast(Pair.State, Pair.Actor, Pair.Target, Slot);
			Advance(Pair.State, SimTicks(1.5) - 1);
		}
		bPass &= TestEqual(TEXT("flow five"), SimFindActor(Pair.State, Pair.Actor)->Water.Flow, 5);
		bPass &= TestEqual(TEXT("no crest yet"), SimFindActor(Pair.State, Pair.Actor)->Water.Crests, 0);
		SimFindActor(Pair.State, Pair.Actor)->Mana = 0.0;
		FInputFrame Crest = SimIdleInput(SimFindActor(Pair.State, Pair.Target)->Pos);
		Crest.bCast = true;
		Crest.Slot = 0;
		bPass &= TestTrue(TEXT("crest casts free"), TrySpellCast(Pair.State, *SimFindActor(Pair.State, Pair.Actor), Crest));
		const FActor& Crested = *SimFindActor(Pair.State, Pair.Actor);
		bPass &= Near(*this, TEXT("crest mana"), Crested.Mana, 0.0);
		bPass &= TestTrue(TEXT("crest pending"), Crested.Pending.IsSet() && Crested.Pending->DamageMult.IsSet());
		if (Crested.Pending.IsSet() && Crested.Pending->DamageMult.IsSet())
		{
			bPass &= Near(*this, TEXT("crest multiplier"), Crested.Pending->DamageMult.GetValue(), KernelData().CrestDamageMult);
		}
		bPass &= TestEqual(TEXT("flow spent"), Crested.Water.Flow, 0);
		bPass &= TestEqual(TEXT("one crest"), Crested.Water.Crests, 1);
	}
	{
		FPair Pair = MakeSetup(TEXT("mirror"), 2, TEXT("A"), 2.6);
		FActor& Caster = *SimFindActor(Pair.State, Pair.Actor);
		Caster.Water.Flow = 3;
		Caster.Water.LastActivityTick = Pair.State.Tick;
		Cast(Pair.State, Pair.Actor, Pair.Target);
		bPass &= TestEqual(TEXT("passive leaves flow"), SimFindActor(Pair.State, Pair.Actor)->Water.Flow, 3);
		bPass &= TestEqual(TEXT("passive is not a cast"), SimFindActor(Pair.State, Pair.Actor)->Metrics.Casts, 0);
		SimFindActor(Pair.State, Pair.Actor)->Tier = 1;
		SimFindActor(Pair.State, Pair.Actor)->Mana = 0.0;
		FInputFrame Broke = SimIdleInput(SimFindActor(Pair.State, Pair.Target)->Pos);
		Broke.Slot = 1;
		Broke.bCast = true;
		bPass &= TestFalse(TEXT("unaffordable"), TrySpellCast(Pair.State, *SimFindActor(Pair.State, Pair.Actor), Broke));
		Advance(Pair.State, SimTicks(KernelData().FlowIdleS) - 1);
		bPass &= TestEqual(TEXT("idle resets flow"), SimFindActor(Pair.State, Pair.Actor)->Water.Flow, 0);
		FActor& Refilled = *SimFindActor(Pair.State, Pair.Actor);
		Refilled.Mana = Refilled.MaxMana;
		Cast(Pair.State, Pair.Actor, Pair.Target, 0);
		Advance(Pair.State, SimTicks(0.3));
		SimFindActor(Pair.State, Pair.Actor)->Water.Flow = 2;
		Cast(Pair.State, Pair.Actor, Pair.Target, 0);
		bPass &= TestEqual(TEXT("same line resets flow"), SimFindActor(Pair.State, Pair.Actor)->Water.Flow, 0);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelWaterGeometry, "MageArena.Kernel.WaterGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelWaterGeometry::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	{
		FPair Pair = MakeSetup(TEXT("tide_orb"), 2, TEXT("A"), 2.6);
		const FSimVec NeighbourPos{SimFindActor(Pair.State, Pair.Target)->Pos.X, SimFindActor(Pair.State, Pair.Target)->Pos.Y + 1.0};
		const int32 Neighbour = AddMage(Pair.State, 1, NeighbourPos).Id;
		Cast(Pair.State, Pair.Actor, Pair.Target);
		Advance(Pair.State, SimTicks(1.0));
		const FSpell* Crash = FindSpellById(TEXT("tide_orb:2:base"));
		bPass &= TestNotNull(TEXT("crash"), Crash);
		if (Crash)
		{
			const FActor& Target = *SimFindActor(Pair.State, Pair.Target);
			const FActor& Other = *SimFindActor(Pair.State, Neighbour);
			bPass &= Near(*this, TEXT("direct"), Target.MaxHp - Target.Hp, Crash->Damage);
			bPass &= Near(*this, TEXT("burst once"), Other.MaxHp - Other.Hp, Crash->Damage);
		}
	}
	{
		const TTuple<int32, const TCHAR*, int32> Fans[] = {TTuple<int32, const TCHAR*, int32>(3, TEXT("A"), 2), TTuple<int32, const TCHAR*, int32>(4, TEXT("B"), 5)};
		for (const auto& Fan : Fans)
		{
			FPair Pair = MakeSetup(TEXT("tide_orb"), Fan.Get<0>(), Fan.Get<1>(), 10.0);
			const FSpell* Spell = SpellFor(*SimFindActor(Pair.State, Pair.Actor), 1);
			Cast(Pair.State, Pair.Actor, Pair.Target);
			Advance(Pair.State, Spell ? SimTicks(Spell->CastS) : 0);
			bPass &= TestEqual(TEXT("fan count"), Pair.State.Projectiles.Num(), Fan.Get<2>());
			if (Pair.State.Projectiles.Num() >= 2)
			{
				bPass &= Near(*this, TEXT("fan symmetry"), Pair.State.Projectiles[0].Velocity.Y, -Pair.State.Projectiles.Last().Velocity.Y);
			}
		}
	}
	for (const TCHAR* Branch : {TEXT("A"), TEXT("B")})
	{
		FPair Pair = MakeSetup(TEXT("lash"), 2, Branch, 2.6);
		const int32 Behind = AddMage(Pair.State, 1, FSimVec{8.0, 10.0}).Id;
		const double Initial = SimFindActor(Pair.State, Pair.Target)->Pos.X;
		Cast(Pair.State, Pair.Actor, Pair.Target);
		Advance(Pair.State, SimTicks(0.3));
		const double Moved = SimFindActor(Pair.State, Pair.Target)->Pos.X;
		bPass &= TestTrue(TEXT("cone displaces"), Branch[0] == TCHAR('A') ? Moved < Initial : Moved > Initial);
		bPass &= Near(*this, TEXT("behind is safe"), SimFindActor(Pair.State, Behind)->Hp, SimFindActor(Pair.State, Behind)->MaxHp);
	}
	{
		FPair Pair = MakeSetup(TEXT("lash"), 3, TEXT("A"), 2.6);
		const int32 Behind = AddMage(Pair.State, 1, FSimVec{8.0, 10.0}).Id;
		Cast(Pair.State, Pair.Actor, Pair.Target);
		Advance(Pair.State, SimTicks(0.5));
		const FSpell* Ring = FindSpellById(TEXT("lash:3:base"));
		if (Ring)
		{
			const FActor& Other = *SimFindActor(Pair.State, Behind);
			bPass &= Near(*this, TEXT("ring reaches behind"), Other.MaxHp - Other.Hp, Ring->Damage);
		}
	}
	{
		FPair Pair = MakeSetup(TEXT("lash"), 4, TEXT("A"), 2.0);
		Cast(Pair.State, Pair.Actor, Pair.Target);
		Advance(Pair.State, SimTicks(0.7));
		bPass &= TestEqual(TEXT("coil still warning"), SimFindActor(Pair.State, Pair.Target)->Water.RootUntil, 0);
		Advance(Pair.State, SimTicks(0.2));
		bPass &= TestTrue(TEXT("coil roots"), SimFindActor(Pair.State, Pair.Target)->Water.RootUntil > Pair.State.Tick);
		const double X = SimFindActor(Pair.State, Pair.Target)->Pos.X;
		FInputFrame Escape = SimIdleInput();
		Escape.Move = FSimVec{1.0, 0.0};
		Escape.bRoll = true;
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Pair.Target, Escape);
		StepArena(Pair.State, Inputs);
		bPass &= TestTrue(TEXT("root holds"), SimFindActor(Pair.State, Pair.Target)->Pos.X == X);
	}
	{
		FPair Puddle = MakeSetup(TEXT("mire"), 1, TEXT("A"), 2.6);
		Cast(Puddle.State, Puddle.Actor, Puddle.Target);
		Advance(Puddle.State, SimTicks(0.5));
		const double X = SimFindActor(Puddle.State, Puddle.Target)->Pos.X;
		FInputFrame Walk = SimIdleInput();
		Walk.Move = FSimVec{1.0, 0.0};
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Puddle.Target, Walk);
		StepArena(Puddle.State, Inputs);
		const FSpell* Slow = FindSpellById(TEXT("mire:1:base"));
		if (Slow)
		{
			bPass &= Near(*this, TEXT("puddle slow"), SimFindActor(Puddle.State, Puddle.Target)->Pos.X - X,
				KernelData().WalkMps * (1.0 - Slow->Amount) * SimDt());
		}
		FPair Fog = MakeSetup(TEXT("mire"), 2, TEXT("A"), 2.6);
		Cast(Fog.State, Fog.Actor, Fog.Target);
		Advance(Fog.State, SimTicks(0.5));
		bPass &= TestFalse(TEXT("fog blocks"), HasLineOfSight(Fog.State, SimFindActor(Fog.State, Fog.Actor)->Pos, SimFindActor(Fog.State, Fog.Target)->Pos));
		Advance(Fog.State, SimTicks(5.0));
		bPass &= TestTrue(TEXT("fog expires"), HasLineOfSight(Fog.State, SimFindActor(Fog.State, Fog.Actor)->Pos, SimFindActor(Fog.State, Fog.Target)->Pos));
		FPair Freeze = MakeSetup(TEXT("mire"), 3, TEXT("A"), 2.6);
		Cast(Freeze.State, Freeze.Actor, Freeze.Target);
		Advance(Freeze.State, SimTicks(0.8));
		const FSpell* Root = FindSpellById(TEXT("mire:3:base"));
		bPass &= TestTrue(TEXT("freeze roots"), SimFindActor(Freeze.State, Freeze.Target)->Water.RootUntil > Freeze.State.Tick);
		if (Root)
		{
			bPass &= Near(*this, TEXT("freeze damage"), SimFindActor(Freeze.State, Freeze.Target)->Hp, SimFindActor(Freeze.State, Freeze.Target)->MaxHp - Root->Damage);
		}
	}
	{
		FPair Pair = MakeSetup(TEXT("mire"), 4, TEXT("A"), 2.6);
		SimFindActor(Pair.State, Pair.Target)->Water.Flow = 4;
		Cast(Pair.State, Pair.Actor, Pair.Target);
		Advance(Pair.State, SimTicks(0.8));
		bPass &= TestEqual(TEXT("tomb clears flow"), SimFindActor(Pair.State, Pair.Target)->Water.Flow, 0);
		const double Hp = SimFindActor(Pair.State, Pair.Target)->Hp;
		const FSimVec Position = SimFindActor(Pair.State, Pair.Target)->Pos;
		FInputFrame Struggle = SimIdleInput(SimFindActor(Pair.State, Pair.Actor)->Pos);
		Struggle.bCast = true;
		Struggle.Move = FSimVec{1.0, 0.0};
		Struggle.bRoll = true;
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Pair.Target, Struggle);
		StepArena(Pair.State, Inputs);
		bPass &= TestEqual(TEXT("tomb silences"), SimFindActor(Pair.State, Pair.Target)->Metrics.Casts, 0);
		bPass &= TestTrue(TEXT("tomb holds"), SimFindActor(Pair.State, Pair.Target)->Pos.X == Position.X && SimFindActor(Pair.State, Pair.Target)->Pos.Y == Position.Y);
		FActor& Target = *SimFindActor(Pair.State, Pair.Target);
		ResolveHit(Pair.State, Target, MakeHit(Pair.Actor, 100, 80.0, TEXT("unblockable"), 4, SimFindActor(Pair.State, Pair.Actor)->Pos));
		bPass &= Near(*this, TEXT("tomb immunity"), Target.Hp, Hp);
		Advance(Pair.State, SimTicks(2.0));
		bPass &= TestTrue(TEXT("tomb ends"), Pair.State.Tick > SimFindActor(Pair.State, Pair.Target)->Water.EncasedUntil);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelSustain, "MageArena.Kernel.Sustain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelSustain::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	{
		FPair Heal = MakeSetup(TEXT("mend"), 1, TEXT("A"), 2.6);
		SimFindActor(Heal.State, Heal.Actor)->Hp = 50.0;
		Cast(Heal.State, Heal.Actor, Heal.Target);
		Advance(Heal.State, SimTicks(0.4));
		const FSpell* Draught = FindSpellById(TEXT("mend:1:base"));
		if (Draught)
		{
			bPass &= Near(*this, TEXT("draught"), SimFindActor(Heal.State, Heal.Actor)->Hp, 50.0 + Draught->Amount);
		}
		FPair Ward = MakeSetup(TEXT("mend"), 2, TEXT("A"), 2.6);
		Cast(Ward.State, Ward.Actor, Ward.Target);
		Advance(Ward.State, SimTicks(0.2));
		FInputFrame Absorb = SimIdleInput(SimFindActor(Ward.State, Ward.Target)->Pos);
		Absorb.bAbsorb = true;
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Ward.Actor, Absorb);
		StepArena(Ward.State, Inputs);
		const FSpell* TideWard = FindSpellById(TEXT("mend:2:base"));
		if (TideWard)
		{
			bPass &= Near(*this, TEXT("ward drain"), SimFindActor(Ward.State, Ward.Actor)->Metrics.ManaDrained,
				DrainPerSecond(1.0) * (1.0 - TideWard->Amount) * SimDt());
		}
		FPair Spring = MakeSetup(TEXT("mend"), 3, TEXT("A"), 2.6);
		SimFindActor(Spring.State, Spring.Actor)->Hp = 50.0;
		Cast(Spring.State, Spring.Actor, Spring.Target);
		Advance(Spring.State, SimTicks(5.4));
		const FSpell* Hot = FindSpellById(TEXT("mend:3:base"));
		if (Hot)
		{
			bPass &= Near(*this, TEXT("spring"), SimFindActor(Spring.State, Spring.Actor)->Hp, 50.0 + Hot->Amount);
		}
		FPair Font = MakeSetup(TEXT("mend"), 4, TEXT("A"), 2.6);
		SimFindActor(Font.State, Font.Actor)->Mana = 10.0;
		Cast(Font.State, Font.Actor, Font.Target);
		const double X = SimFindActor(Font.State, Font.Actor)->Pos.X;
		FInputFrame Rooted = SimIdleInput();
		Rooted.Move = FSimVec{1.0, 0.0};
		Rooted.bRoll = true;
		TMap<int32, FInputFrame> FontInputs;
		FontInputs.Add(Font.Actor, Rooted);
		StepArena(Font.State, FontInputs);
		bPass &= TestTrue(TEXT("font roots"), SimFindActor(Font.State, Font.Actor)->Pos.X == X);
		Advance(Font.State, SimTicks(0.6));
		bPass &= TestTrue(TEXT("font restores mana"), SimFindActor(Font.State, Font.Actor)->Mana > 70.0);
	}
	{
		FPair Pair = MakeSetup(TEXT("mirror"), 1, TEXT("A"), 2.6);
		Cast(Pair.State, Pair.Actor, Pair.Target);
		Advance(Pair.State, SimTicks(0.1));
		FActor& Actor = *SimFindActor(Pair.State, Pair.Actor);
		Actor.Mana = 20.0;
		Actor.bAbsorb = true;
		Actor.AbsorbFreshTick = Pair.State.Tick;
		Actor.Facing = FSimVec{1.0, 0.0};
		const FActor& Target = *SimFindActor(Pair.State, Pair.Target);
		ResolveHit(Pair.State, Actor, MakeHit(Target.Id, 100, 40.0, TEXT("magic"), 1, Target.Pos));
		const FSpell* Sheen = FindSpellById(TEXT("mirror:1:base"));
		if (Sheen)
		{
			bPass &= Near(*this, TEXT("sheen refund"), Actor.Mana, 20.0 + PerfectReturn(1, Actor.Ranks.Nerve) * (1.0 + Sheen->Amount));
		}
		bPass &= Near(*this, TEXT("stored half"), Actor.Water.Stored, 40.0 * KernelData().StoredFraction);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			ResolveHit(Pair.State, Actor, MakeHit(Target.Id, 100, 40.0, TEXT("magic"), 1, Target.Pos));
		}
		const FSpell* Return = FindSpellById(TEXT("mirror:4:base"));
		if (Return)
		{
			bPass &= Near(*this, TEXT("stored cap"), Actor.Water.Stored, Return->Amount);
		}
		Actor.bAbsorb = false;
		Actor.Water.Flow = 0;
		Actor.Tier = 4;
		Actor.Water.Cooldowns.Reset();
		Cast(Pair.State, Pair.Actor, Pair.Target);
		Advance(Pair.State, SimTicks(0.4));
		bPass &= Near(*this, TEXT("wave empties stored"), SimFindActor(Pair.State, Pair.Actor)->Water.Stored, 0.0);
		bPass &= TestTrue(TEXT("wave exists"), Pair.State.Projectiles.Num() > 0);
		if (Pair.State.Projectiles.Num() > 0)
		{
			bPass &= Near(*this, TEXT("wave damage"), Pair.State.Projectiles[0].Damage, 40.0);
		}
	}
	struct FReflectCase
	{
		int32 Tier;
		const TCHAR* Family;
		bool bReflected;
	};
	const FReflectCase ReflectCases[] = {{1, TEXT("magic"), true}, {4, TEXT("magic"), false}, {1, TEXT("physical"), false}};
	for (const FReflectCase& Case : ReflectCases)
	{
		FPair Pair = MakeSetup(TEXT("mirror"), 3, TEXT("A"), 2.6);
		SimFindActor(Pair.State, Pair.Actor)->Facing = FSimVec{1.0, 0.0};
		const FActor& Target = *SimFindActor(Pair.State, Pair.Target);
		FProjectile Projectile;
		Projectile.Id = 90;
		Projectile.ActivationId = 90;
		Projectile.OwnerId = Target.Id;
		Projectile.Damage = 6.0;
		Projectile.Family = Case.Family;
		Projectile.Tier = Case.Tier;
		Projectile.Source = Target.Pos;
		Projectile.Pos = FSimVec{10.6, 10.0};
		Projectile.PreviousPos = Projectile.Pos;
		Projectile.Velocity = FSimVec{-20.0, 0.0};
		Projectile.Radius = KernelData().ProjectileRadiusM;
		Projectile.RemainingM = 12.0;
		Pair.State.Projectiles.Add(Projectile);
		FInputFrame Absorb = SimIdleInput(Target.Pos);
		Absorb.bAbsorb = true;
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Pair.Actor, Absorb);
		StepArena(Pair.State, Inputs);
		bool bReflected = false;
		for (const FProjectile& Shot : Pair.State.Projectiles)
		{
			bReflected |= Shot.OwnerId == Pair.Actor && Shot.bReflected;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("reflect %s %d"), Case.Family, Case.Tier), bReflected, Case.bReflected);
	}
	{
		FPair Pair = MakeSetup(TEXT("mirror"), 2, TEXT("B"), 2.6);
		FActor& Actor = *SimFindActor(Pair.State, Pair.Actor);
		Actor.bAbsorb = true;
		Actor.AbsorbFreshTick = 0;
		const FActor& Target = *SimFindActor(Pair.State, Pair.Target);
		ResolveHit(Pair.State, Actor, MakeHit(Target.Id, 40, 10.0, TEXT("magic"), 1, Target.Pos));
		const FSpell* Ripple = FindSpellByEffect(TEXT("ripple"));
		if (Ripple)
		{
			const FActor& Hit = *SimFindActor(Pair.State, Pair.Target);
			bPass &= Near(*this, TEXT("ripple"), Hit.MaxHp - Hit.Hp, Ripple->Damage);
		}
		SimFindActor(Pair.State, Pair.Actor)->Tier = 3;
		SimFindActor(Pair.State, Pair.Actor)->bAbsorb = false;
		Cast(Pair.State, Pair.Actor, Pair.Target);
		Advance(Pair.State, SimTicks(0.2));
		bPass &= TestTrue(TEXT("decoy up"), SimFindActor(Pair.State, Pair.Actor)->Water.Decoy.IsSet());
		Advance(Pair.State, SimTicks(3.0));
		bPass &= TestFalse(TEXT("decoy expires"), SimFindActor(Pair.State, Pair.Actor)->Water.Decoy.IsSet());
		FActor& Reset = *SimFindActor(Pair.State, Pair.Actor);
		Reset.Water.Flow = 5;
		Reset.Water.Stored = 80.0;
		Reset.Water.EncasedUntil = 500;
		SimSetCooldown(Reset.Water, TEXT("mirror"), 500);
		ResetWave(Pair.State, Reset);
		bPass &= TestEqual(TEXT("reset flow"), Reset.Water.Flow, 0);
		bPass &= Near(*this, TEXT("reset stored"), Reset.Water.Stored, 0.0);
		bPass &= TestEqual(TEXT("reset encase"), Reset.Water.EncasedUntil, 0);
		bPass &= TestEqual(TEXT("reset cooldowns"), Reset.Water.Cooldowns.Num(), 0);
		bPass &= TestEqual(TEXT("branch kept"), Reset.Water.Composition.Branches.Mirror, FString(TEXT("B")));
	}
	return bPass;
}

#endif
