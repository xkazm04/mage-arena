#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Greybox/ArenaLayout.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Enemies.h"
#include "Kernel/Games.h"
#include "Kernel/KernelData.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"

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

const FEnemySpec* FindSpec(const FString& Id)
{
	for (const FEnemySpec& Spec : KernelData().Enemies)
	{
		if (Spec.Id == Id)
		{
			return &Spec;
		}
	}
	return nullptr;
}

const FAttackSpec* FindAttack(const FEnemySpec& Spec, const FString& Id)
{
	for (const FAttackSpec& Attack : Spec.Attacks)
	{
		if (Attack.Id == Id)
		{
			return &Attack;
		}
	}
	return nullptr;
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

const FActor* FindEnemy(const FGames& Games, const TCHAR* Id)
{
	for (const FActor& Actor : Games.State.Actors)
	{
		if (Actor.Enemy.IsSet() && Actor.Enemy->Id == Id)
		{
			return &Actor;
		}
	}
	return nullptr;
}

const FArenaEvent* PlayerHit(const FGames& Games)
{
	for (const FArenaEvent& Event : Games.State.Events)
	{
		if (Event.Kind == TEXT("hit") && Event.ActorId == Games.PlayerId)
		{
			return &Event;
		}
	}
	return nullptr;
}

bool MakeSpearDuel(FGames& Games, const FVrRuleset& Rules)
{
	Games = FGames();
	Games.State = CreateArena(1);
	const FSimVec Spawn = KernelData().PlayerSpawn;
	Games.PlayerId = AddMage(Games.State, 0, Spawn, TEXT("Cassia")).Id;
	// Just outside the hold line, on the arena side of the dais, level with the player.
	const FSimVec Stand{Rules.Dais.MaxX + Rules.StandoffM + 0.3, Spawn.Y};
	AddEnemy(Games.State, TEXT("conscript"), Stand);
	Games.Phase = TEXT("active");
	Games.Wave = 0;
	Games.VrRules = Rules;
	return SimFindActor(Games.State, Games.PlayerId) != nullptr;
}

FInputFrame AimAtConscript(const FGames& Games)
{
	const FActor* Conscript = FindEnemy(Games, TEXT("conscript"));
	const FSimVec Aim = Conscript ? Conscript->Pos : KernelData().PlayerSpawn;
	return SimIdleInput(Aim);
}

const FProjectile* ConscriptSpear(const FGames& Games)
{
	for (const FProjectile& Projectile : Games.State.Projectiles)
	{
		if (Projectile.OwnerId == Games.PlayerId)
		{
			continue;
		}
		const FActor* Owner = SimFindActor(Games.State, Projectile.OwnerId);
		if (Owner && Owner->Enemy.IsSet() && Owner->Enemy->Id == TEXT("conscript"))
		{
			return &Projectile;
		}
	}
	return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaVrOverlayLoads, "MageArena.VrOverlay.Loads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaVrOverlayLoads::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FEnemySpec* Conscript = FindSpec(TEXT("conscript"));
	const FEnemySpec* Slinger = FindSpec(TEXT("slinger"));
	if (!TestNotNull(TEXT("conscript spec"), Conscript) || !TestNotNull(TEXT("slinger spec"), Slinger))
	{
		return false;
	}
	const FAttackSpec* Spear = FindAttack(*Conscript, TEXT("spear_lunge"));
	const FAttackSpec* Stone = FindAttack(*Slinger, TEXT("sling_stone"));
	if (!TestNotNull(TEXT("spear"), Spear) || !TestNotNull(TEXT("stone"), Stone))
	{
		return false;
	}
	const double SpearDamage = Spear->Damage;
	const double SpearWindup = Spear->WindupS;
	const bool bSpearHadSpeed = Spear->ProjectileMps.IsSet();
	const double StoneSpeed = Stone->ProjectileMps.Get(0.0);

	FArenaLayout Layout;
	FString LayoutError;
	if (!TestTrue(TEXT("layout"), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), LayoutError)))
	{
		AddError(LayoutError);
		return false;
	}
	const FThreatSpec* Steel = nullptr;
	for (const FThreatSpec& Threat : Layout.Threats)
	{
		if (Threat.Kind == TEXT("steel"))
		{
			Steel = &Threat;
			break;
		}
	}
	if (!TestNotNull(TEXT("steel threat"), Steel))
	{
		return false;
	}

	FVrRuleset Rules;
	FString Error;
	if (!TestTrue(TEXT("overlay loads"), LoadVrRuleset(Rules, Error)))
	{
		AddError(Error);
		return false;
	}

	bool bPass = true;
	const FRunePad* CentrePad = Layout.FindPad(1);
	bPass &= TestTrue(TEXT("rules active"), Rules.bActive);
	bPass &= TestNotNull(TEXT("centre pad"), CentrePad);
	bPass &= Near(*this, TEXT("standoff is the dais height"), Rules.StandoffM, Layout.DaisHeightM);
	if (CentrePad)
	{
		bPass &= Near(*this, TEXT("dais centre x"), (Rules.Dais.MinX + Rules.Dais.MaxX) * 0.5,
			KernelData().PlayerSpawn.X + (Layout.DaisCentreM.X - CentrePad->PositionM.X));
	}
	bPass &= Near(*this, TEXT("dais half x"), (Rules.Dais.MaxX - Rules.Dais.MinX) * 0.5, Layout.DaisSizeM.X * 0.5);
	bPass &= Near(*this, TEXT("dais half y"), (Rules.Dais.MaxY - Rules.Dais.MinY) * 0.5, Layout.DaisSizeM.Y * 0.5);
	bPass &= Near(*this, TEXT("kernel dais min x"), Rules.Dais.MinX, 5.7);
	bPass &= Near(*this, TEXT("kernel dais max x"), Rules.Dais.MaxX, 10.9);
	bPass &= Near(*this, TEXT("kernel dais min y"), Rules.Dais.MinY, 5.6);
	bPass &= Near(*this, TEXT("kernel dais max y"), Rules.Dais.MaxY, 14.4);
	bPass &= TestTrue(TEXT("player spawn is on the dais"), Rules.InsideDais(KernelData().PlayerSpawn));
	bPass &= TestFalse(TEXT("enemy spawn is on the dais"), Rules.InsideDais(KernelData().EnemySpawn));
	bPass &= TestFalse(TEXT("enemy spawn is inside the hold"), Rules.InsideHold(KernelData().EnemySpawn));

	const FVrAttackMode* Throw = Rules.FindThrow(TEXT("conscript"));
	bPass &= TestNotNull(TEXT("conscript throw"), Throw);
	if (Throw)
	{
		bPass &= Near(*this, TEXT("throw speed matches the stone"), Throw->ProjectileMps, StoneSpeed);
		bPass &= Near(*this, TEXT("throw speed"), Throw->ProjectileMps, 14.0);
		bPass &= Near(*this, TEXT("throw range"), Throw->RangeM, 9.0);
		bPass &= Near(*this, TEXT("arc is chest minus dais"), Throw->ArcApexM, Layout.ChestHeightAboveSandM - Layout.DaisHeightM);
		bPass &= Near(*this, TEXT("spear length"), Throw->SpearLengthM, Steel->LengthM);
		bPass &= Near(*this, TEXT("spear radius"), Throw->SpearRadiusM, Steel->RadiusM);
	}
	bPass &= TestNull(TEXT("slinger is not a throw"), Rules.FindThrow(TEXT("slinger")));
	bPass &= TestFalse(TEXT("pinned spear had no speed initially"), bSpearHadSpeed);
	bPass &= TestFalse(TEXT("pinned spear still has no speed"), Spear->ProjectileMps.IsSet());
	bPass &= Near(*this, TEXT("pinned spear damage untouched"), Spear->Damage, SpearDamage);
	bPass &= Near(*this, TEXT("pinned spear windup untouched"), Spear->WindupS, SpearWindup);
	bPass &= Near(*this, TEXT("pinned stone speed untouched"), Stone->ProjectileMps.Get(0.0), StoneSpeed);
	bPass &= Near(*this, TEXT("spear damage stays 10"), SpearDamage, 10.0);
	bPass &= Near(*this, TEXT("spear windup stays 0.6"), SpearWindup, 0.6);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaVrOverlayDaisClear, "MageArena.VrOverlay.DaisClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaVrOverlayDaisClear::RunTest(const FString& Parameters)
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
	const FComposition* Preset = FindRotation();
	if (!TestNotNull(TEXT("rotation"), Preset))
	{
		return false;
	}
	FGames Games;
	if (!TestTrue(TEXT("wave"), TryCreateGames(Games, 1, Preset, 0, false, false, &Rules)))
	{
		return false;
	}

	bool bPass = true;
	bool bReached = false;
	bool bSpear = false;
	bool bMelee = false;
	bool bSlinger = false;
	bool bSlingerBad = false;
	double Closest = 1.0e9;
	const FInputFrame Idle = SimIdleInput();
	auto NoteBody = [&](const FActor& Actor)
	{
		if (!Actor.Enemy.IsSet() || Actor.bDown)
		{
			return;
		}
		if (Rules.InsideDais(Actor.Pos))
		{
			AddError(FString::Printf(TEXT("%s entered the dais at %.3f,%.3f"), *Actor.Enemy->Id, Actor.Pos.X, Actor.Pos.Y));
			bPass = false;
		}
		if (Actor.Enemy->Id == TEXT("conscript"))
		{
			const double Gap = Rules.OutsideGap(Actor.Pos);
			Closest = FMath::Min(Closest, Gap);
			if (Gap >= -0.05 && Gap < 0.5)
			{
				bReached = true;
			}
		}
	};
	for (const FActor& Actor : Games.State.Actors)
	{
		NoteBody(Actor);
	}
	const int32 Limit = SimTicks(120.0);
	while (Games.Phase == TEXT("active") && Games.State.Tick < Limit)
	{
		StepGames(Games, &Idle);
		for (const FActor& Actor : Games.State.Actors)
		{
			NoteBody(Actor);
		}
		for (const FTelegraph& Telegraph : Games.State.Telegraphs)
		{
			const FActor* Owner = SimFindActor(Games.State, Telegraph.OwnerId);
			if (!Owner || !Owner->Enemy.IsSet())
			{
				continue;
			}
			if (Owner->Enemy->Id == TEXT("conscript") && Telegraph.Kind == TEXT("melee"))
			{
				bMelee = true;
			}
			if (Owner->Enemy->Id == TEXT("slinger"))
			{
				bSlinger = true;
				if (Telegraph.Kind != TEXT("projectile") || std::abs(Telegraph.Damage - 7.0) > 1.0e-6 || std::abs(Telegraph.SpeedMps - 14.0) > 1.0e-6)
				{
					bSlingerBad = true;
					AddError(FString::Printf(TEXT("slinger telegraph kind=%s damage=%.3f speed=%.3f"), *Telegraph.Kind, Telegraph.Damage, Telegraph.SpeedMps));
				}
			}
		}
		for (const FProjectile& Projectile : Games.State.Projectiles)
		{
			const FActor* Owner = SimFindActor(Games.State, Projectile.OwnerId);
			if (!Owner || !Owner->Enemy.IsSet())
			{
				continue;
			}
			if (Owner->Enemy->Id == TEXT("conscript"))
			{
				bSpear = true;
				bPass &= Near(*this, TEXT("spear damage"), Projectile.Damage, 10.0);
				bPass &= Near(*this, TEXT("spear speed"), SimLength(Projectile.Velocity), 14.0);
				bPass &= TestEqual(TEXT("spear family"), Projectile.Family, FString(TEXT("physical")));
			}
			if (Owner->Enemy->Id == TEXT("slinger"))
			{
				bSlinger = true;
				if (std::abs(Projectile.Damage - 7.0) > 1.0e-6 || std::abs(SimLength(Projectile.Velocity) - 14.0) > 1.0e-6)
				{
					bSlingerBad = true;
					AddError(FString::Printf(TEXT("slinger shot damage=%.3f speed=%.3f"), Projectile.Damage, SimLength(Projectile.Velocity)));
				}
			}
		}
	}
	UE_LOG(LogMageArena, Log, TEXT("VrOverlay dais phase=%s t=%.2f closestGap=%.3f spear=%d slinger=%d"),
		*Games.Phase, Games.State.Tick * SimDt(), Closest, bSpear ? 1 : 0, bSlinger ? 1 : 0);
	bPass &= TestTrue(*FString::Printf(TEXT("a conscript reached the lip (closest gap %.3f)"), Closest), bReached);
	bPass &= TestTrue(TEXT("a conscript threw"), bSpear);
	bPass &= TestFalse(TEXT("no conscript melee"), bMelee);
	bPass &= TestTrue(TEXT("a slinger shot"), bSlinger);
	bPass &= TestFalse(TEXT("slinger shot stayed pinned"), bSlingerBad);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaVrOverlaySpearWardBlink, "MageArena.VrOverlay.ThrownSpearWardAndBlink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaVrOverlaySpearWardBlink::RunTest(const FString& Parameters)
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
	const FSimVec Spawn = KernelData().PlayerSpawn;
	const int32 Limit = SimTicks(8.0);
	bool bPass = true;

	{
		FGames Games;
		if (!TestTrue(TEXT("bare duel"), MakeSpearDuel(Games, Rules)))
		{
			return false;
		}
		const FArenaEvent* Hit = nullptr;
		while (!Hit && Games.Phase == TEXT("active") && Games.State.Tick < Limit)
		{
			const FInputFrame Input = AimAtConscript(Games);
			StepGames(Games, &Input);
			Hit = PlayerHit(Games);
		}
		bPass &= TestNotNull(TEXT("unwarded hit"), Hit);
		if (Hit)
		{
			bPass &= Near(*this, TEXT("unwarded spear is 10"), Hit->Value, 10.0);
		}
		const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
		bPass &= TestNotNull(TEXT("bare player"), Player);
		if (Player)
		{
			bPass &= Near(*this, TEXT("unwarded hp"), Player->Hp, Player->MaxHp - 10.0);
			bPass &= TestEqual(TEXT("unwarded perfects"), Player->Metrics.Perfects, 0);
		}
	}

	{
		FGames Games;
		if (!TestTrue(TEXT("ward duel"), MakeSpearDuel(Games, Rules)))
		{
			return false;
		}
		const FArenaEvent* Hit = nullptr;
		bool bRaisedFresh = false;
		while (!Hit && Games.Phase == TEXT("active") && Games.State.Tick < Limit)
		{
			FInputFrame Input = AimAtConscript(Games);
			const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
			const FProjectile* Spear = ConscriptSpear(Games);
			if (Player && Spear && SimDistance(Spear->Pos, Player->Pos) <= 1.2)
			{
				Input.bAbsorb = true;
				bRaisedFresh = true;
			}
			else if (Player && Player->bAbsorb)
			{
				Input.bAbsorb = true;
			}
			StepGames(Games, &Input);
			Hit = PlayerHit(Games);
		}
		bPass &= TestTrue(TEXT("ward was raised inside the fresh window"), bRaisedFresh);
		bPass &= TestNotNull(TEXT("warded hit"), Hit);
		if (Hit)
		{
			const double Expected = 10.0 * (1.0 - KernelData().Absorb.ReductionPhysical);
			bPass &= Near(*this, TEXT("warded spear matches the physical reduction"), Hit->Value, Expected);
			bPass &= Near(*this, TEXT("warded spear is 7"), Hit->Value, 7.0);
			const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
			if (Player)
			{
				const int32 Since = Games.State.Tick - Player->AbsorbFreshTick;
				bPass &= TestTrue(TEXT("hit landed while the ward was fresh"), Since <= SimTicks(KernelData().Absorb.WindowS));
				bPass &= TestEqual(TEXT("physical is not a perfect"), Player->Metrics.Perfects, 0);
				bPass &= Near(*this, TEXT("warded hp"), Player->Hp, Player->MaxHp - Expected);
			}
		}
	}

	{
		FGames Games;
		if (!TestTrue(TEXT("blink duel"), MakeSpearDuel(Games, Rules)))
		{
			return false;
		}
		bool bAsked = false;
		bool bImmune = false;
		const FActor* Start = SimFindActor(Games.State, Games.PlayerId);
		const double StartingHp = Start ? Start->Hp : -1.0;
		while (Games.Phase == TEXT("active") && Games.State.Tick < Limit)
		{
			FInputFrame Input = AimAtConscript(Games);
			const FActor* Before = SimFindActor(Games.State, Games.PlayerId);
			const FProjectile* Spear = ConscriptSpear(Games);
			if (!bAsked && Before && Spear && SimDistance(Spear->Pos, Before->Pos) <= 2.5 && Before->Stamina >= KernelData().RollStaminaCost)
			{
				Input.bRoll = true;
				bAsked = true;
			}
			StepGames(Games, &Input);
			FActor* Player = SimFindActor(Games.State, Games.PlayerId);
			if (!Player)
			{
				bPass = false;
				AddError(TEXT("blink player missing"));
				break;
			}
			if (bAsked && Player->Metrics.Rolls == 1 && Player->ImmuneUntil > Games.State.Tick)
			{
				bImmune = true;
			}
			Player->Pos = Spawn;
			Player->PreviousPos = Spawn;
			if (bAsked && !ConscriptSpear(Games))
			{
				break;
			}
		}
		const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
		bPass &= TestTrue(TEXT("blink edge sent"), bAsked);
		bPass &= TestTrue(TEXT("roll opened i-frames"), bImmune);
		bPass &= TestNotNull(TEXT("blink player"), Player);
		if (Player)
		{
			bPass &= TestEqual(TEXT("one roll"), Player->Metrics.Rolls, 1);
			bPass &= Near(*this, TEXT("blink took no damage"), Player->Hp, StartingHp);
		}
		bPass &= TestNull(TEXT("blink left no hit"), PlayerHit(Games));
	}

	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaVrOverlayNoRulesMelee, "MageArena.VrOverlay.NoRulesMelee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaVrOverlayNoRulesMelee::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FArenaState State = CreateArena();
	AddMage(State, 0, FSimVec{8.0, 10.0});
	const int32 EnemyId = AddEnemy(State, TEXT("conscript"), FSimVec{9.2, 10.0}).Id;
	const TMap<int32, FInputFrame> Inputs = EnemyInputs(State);
	StepArena(State, Inputs);
	if (!TestTrue(TEXT("one telegraph"), State.Telegraphs.Num() == 1))
	{
		return false;
	}
	const FTelegraph& Telegraph = State.Telegraphs[0];
	bool bPass = true;
	bPass &= TestEqual(TEXT("owner"), Telegraph.OwnerId, EnemyId);
	bPass &= TestEqual(TEXT("melee"), Telegraph.Kind, FString(TEXT("melee")));
	bPass &= Near(*this, TEXT("pinned damage"), Telegraph.Damage, 10.0);
	bPass &= TestEqual(TEXT("no projectile yet"), State.Projectiles.Num(), 0);
	bPass &= TestTrue(TEXT("still winding"), Telegraph.ResolveTick > State.Tick);
	return bPass;
}

#endif
