#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "Kernel/Enemies.h"
#include "Kernel/Games.h"
#include "Kernel/KernelData.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#include <cmath>

// DF-004 option A (owner, 2026-10-07): cinder hounds hold at the dais hold line and spit slow magic embers, and a dying
// hound throws one last ember. Sources for every literal below:
//   combat.vr.json attacks cinder_hound: family magic, tier 0, damage 5, windupS 0.6, projectileMps 7, rangeM 9.
//   enemies.json cinder_hound: bite physical 6, windupS 0.35; onDeath ember_burst magic 0 damage 5 radius 1.5 delay 0.6.
//   combat.json: simStepHz 60; absorb.perfect.windowS 0.15 (9 ticks); absorb.reduction.magic 0.85.
//   runtime.json: playerSpawn (8, 10); mageRadiusM 0.38; projectileRadiusM 0.12.
//   combat.vr.json daisNoEntry with arena-layout.json: dais max x on the arena side is the centre pad plus 2.9 m
//   (layout -9.1 vs pad -12), so 10.9 in the kernel frame, and the hold line is 0.6 m further, 11.5.

namespace
{
bool CreaturesReady(FAutomationTestBase& Test)
{
	if (KernelData().bReady)
	{
		return true;
	}
	Test.AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
	return false;
}

bool CreaturesNear(FAutomationTestBase& Test, const TCHAR* What, double Actual, double Expected)
{
	const bool bOk = std::abs(Actual - Expected) <= 1.0e-6;
	if (!bOk)
	{
		Test.AddError(FString::Printf(TEXT("%s: expected %.9g got %.9g"), What, Expected, Actual));
	}
	return bOk;
}

// The overlay as the live session loads it, ignoring the calibration proposal switch so these literals hold in any run.
bool LoadLiveRules(FAutomationTestBase& Test, FVrRuleset& Rules)
{
	FString Error;
	if (!LoadVrRuleset(Rules, Error, false))
	{
		Test.AddError(Error);
		return false;
	}
	return true;
}

// The player on the centre pad spawn and one hound just outside the hold line on the arena side, level with the player.
int32 MakeHoundDuel(FGames& Games, const FVrRuleset* Rules, double HoundX)
{
	Games = FGames();
	Games.State = CreateArena(1);
	const FSimVec Spawn = KernelData().PlayerSpawn;
	Games.PlayerId = AddMage(Games.State, 0, Spawn, TEXT("Cassia")).Id;
	const int32 HoundId = AddEnemy(Games.State, TEXT("cinder_hound"), FSimVec{HoundX, Spawn.Y}).Id;
	Games.Phase = TEXT("active");
	Games.Wave = 1;
	if (Rules)
	{
		Games.VrRules = *Rules;
	}
	return HoundId;
}

FInputFrame AimAt(const FGames& Games, int32 ActorId)
{
	const FActor* Actor = SimFindActor(Games.State, ActorId);
	return SimIdleInput(Actor ? Actor->Pos : KernelData().PlayerSpawn);
}

const FArenaEvent* PlayerEvent(const FGames& Games, const TCHAR* Kind)
{
	for (const FArenaEvent& Event : Games.State.Events)
	{
		if (Event.Kind == Kind && Event.ActorId == Games.PlayerId)
		{
			return &Event;
		}
	}
	return nullptr;
}

int32 CountOwned(const FGames& Games, int32 OwnerId, const TCHAR* Kind)
{
	int32 Count = 0;
	for (const FTelegraph& Telegraph : Games.State.Telegraphs)
	{
		if (Telegraph.OwnerId == OwnerId && Telegraph.Kind == Kind)
		{
			++Count;
		}
	}
	return Count;
}

int32 CountShots(const FGames& Games, int32 OwnerId)
{
	int32 Count = 0;
	for (const FProjectile& Projectile : Games.State.Projectiles)
	{
		Count += Projectile.OwnerId == OwnerId ? 1 : 0;
	}
	return Count;
}

// 0.3 m outside the hold line (11.5), so 3.8 m from the player at x 8.
constexpr double LipX = 11.8;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreaturesSpitLoads, "MageArena.Creatures.SpitLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreaturesSpitLoads::RunTest(const FString& Parameters)
{
	if (!CreaturesReady(*this))
	{
		return false;
	}
	FVrRuleset Rules;
	if (!LoadLiveRules(*this, Rules))
	{
		return false;
	}
	bool bPass = true;
	const FVrAttackMode* Spit = Rules.FindSpit(TEXT("cinder_hound"));
	if (!TestNotNull(TEXT("cinder_hound spit"), Spit))
	{
		return false;
	}
	bPass &= TestTrue(TEXT("a spit holds like a thrower"), Rules.FindThrow(TEXT("cinder_hound")) == Spit);
	bPass &= TestEqual(TEXT("family"), Spit->Family, FString(TEXT("magic")));
	bPass &= TestEqual(TEXT("tier"), Spit->Tier, 0);
	bPass &= CreaturesNear(*this, TEXT("damage is the pinned ember_burst 5"), Spit->Damage, 5.0);
	bPass &= CreaturesNear(*this, TEXT("windup 0.6"), Spit->WindupS, 0.6);
	bPass &= CreaturesNear(*this, TEXT("speed 7"), Spit->ProjectileMps, 7.0);
	bPass &= CreaturesNear(*this, TEXT("range 9"), Spit->RangeM, 9.0);
	bPass &= TestTrue(TEXT("death ember"), Spit->bDeathEmber);
	bPass &= TestNull(TEXT("conscript is not a spit"), Rules.FindSpit(TEXT("conscript")));
	bPass &= TestNull(TEXT("mire maw is not a spit"), Rules.FindSpit(TEXT("mire_maw")));

	// The loader accepts spit for cinder_hound only. The same entry under another id fails the whole load.
	const FString VrDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr")));
	FString Overlay;
	FString Layout;
	if (!TestTrue(TEXT("read combat.vr.json"), FFileHelper::LoadFileToString(Overlay, *FPaths::Combine(VrDir, TEXT("combat.vr.json"))))
		|| !TestTrue(TEXT("read arena-layout.json"), FFileHelper::LoadFileToString(Layout, *FPaths::Combine(VrDir, TEXT("arena-layout.json")))))
	{
		return false;
	}
	const FString Needle = TEXT("\"enemyId\": \"cinder_hound\"");
	if (!TestTrue(TEXT("overlay names the hound entry"), Overlay.Contains(Needle)))
	{
		return false;
	}
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("SpitLoads-") + FGuid::NewGuid().ToString());
	IFileManager::Get().MakeDirectory(*Dir, true);
	FFileHelper::SaveStringToFile(Layout, *FPaths::Combine(Dir, TEXT("arena-layout.json")));
	FFileHelper::SaveStringToFile(Overlay.Replace(*Needle, TEXT("\"enemyId\": \"mire_maw\"")), *FPaths::Combine(Dir, TEXT("combat.vr.json")));
	SetVrDataDirForTest(Dir);
	FVrRuleset Wrong;
	FString Error;
	const bool bLoaded = LoadVrRuleset(Wrong, Error, false);
	SetVrDataDirForTest(FString());
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	bPass &= TestFalse(TEXT("spit under mire_maw fails the load"), bLoaded);
	bPass &= TestTrue(*FString::Printf(TEXT("the error names the spit (%s)"), *Error), Error.Contains(TEXT("spit")));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreaturesSpitFromHoldLine, "MageArena.Creatures.SpitFromHoldLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreaturesSpitFromHoldLine::RunTest(const FString& Parameters)
{
	if (!CreaturesReady(*this))
	{
		return false;
	}
	FVrRuleset Rules;
	if (!LoadLiveRules(*this, Rules))
	{
		return false;
	}
	bool bPass = true;
	bPass &= CreaturesNear(*this, TEXT("hold line x (layout dais and standoff)"), Rules.HoldBox().MaxX, 11.5);

	FGames Games;
	const int32 HoundId = MakeHoundDuel(Games, &Rules, LipX);
	bool bSawSpit = false;
	bool bSawShot = false;
	bool bBite = false;
	int32 Spits = 0;
	double WorstGap = 1.0e9;
	const int32 Limit = SimTicks(6.0);
	while (Games.Phase == TEXT("active") && Games.State.Tick < Limit)
	{
		const FInputFrame Input = AimAt(Games, HoundId);
		StepGames(Games, &Input);
		const FActor* Hound = SimFindActor(Games.State, HoundId);
		if (!Hound)
		{
			AddError(TEXT("hound missing"));
			return false;
		}
		if (Rules.InsideDais(Hound->Pos))
		{
			AddError(FString::Printf(TEXT("hound entered the dais at %.3f,%.3f tick %d"), Hound->Pos.X, Hound->Pos.Y, Games.State.Tick));
			bPass = false;
		}
		WorstGap = FMath::Min(WorstGap, Rules.OutsideGap(Hound->Pos));
		for (const FTelegraph& Telegraph : Games.State.Telegraphs)
		{
			if (Telegraph.OwnerId != HoundId)
			{
				continue;
			}
			if (Telegraph.Kind == TEXT("melee"))
			{
				bBite = true;
			}
			if (Telegraph.Kind != TEXT("projectile") || Telegraph.StartTick != Games.State.Tick)
			{
				continue;
			}
			++Spits;
			if (bSawSpit)
			{
				continue;
			}
			bSawSpit = true;
			bPass &= TestEqual(TEXT("spit family"), Telegraph.Family, FString(TEXT("magic")));
			bPass &= TestEqual(TEXT("spit tier"), Telegraph.Tier, 0);
			bPass &= CreaturesNear(*this, TEXT("spit damage"), Telegraph.Damage, 5.0);
			bPass &= CreaturesNear(*this, TEXT("spit speed"), Telegraph.SpeedMps, 7.0);
			bPass &= CreaturesNear(*this, TEXT("spit range"), Telegraph.RangeM, 9.0);
			// windupS 0.6 at 60 Hz.
			bPass &= TestEqual(TEXT("spit windup ticks"), Telegraph.ResolveTick - Telegraph.StartTick, 36);
			const double Gap = Rules.OutsideGap(Telegraph.Origin);
			bPass &= TestTrue(*FString::Printf(TEXT("spat from the hold line (gap %.3f m)"), Gap), Gap >= 0.0 && Gap <= 0.5);
		}
		for (const FProjectile& Projectile : Games.State.Projectiles)
		{
			if (Projectile.OwnerId == HoundId && !bSawShot)
			{
				bSawShot = true;
				bPass &= CreaturesNear(*this, TEXT("ember flies at 7 m/s"), SimLength(Projectile.Velocity), 7.0);
				bPass &= TestEqual(TEXT("ember is magic"), Projectile.Family, FString(TEXT("magic")));
			}
		}
	}
	UE_LOG(LogMageArena, Log, TEXT("Creatures spit spits=%d worstGap=%.3f phase=%s"), Spits, WorstGap, *Games.Phase);
	bPass &= TestTrue(TEXT("the hound spat"), bSawSpit);
	bPass &= TestTrue(TEXT("an ember flew"), bSawShot);
	bPass &= TestFalse(TEXT("no bite with the spit overlay"), bBite);
	bPass &= TestTrue(*FString::Printf(TEXT("the hound held outside the hold box (worst gap %.4f m)"), WorstGap), WorstGap >= -1.0e-3);
	// Pinned bite recoveryS 0.4 plus windup 0.6: one spit a second, ready again on tick 61, 121, ... (tick 1 + 60 k).
	bPass &= TestTrue(*FString::Printf(TEXT("a spit about every second (%d in 6 s)"), Spits), Spits >= 5 && Spits <= 6);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreaturesSpitPerfect, "MageArena.Creatures.SpitEmberPerfect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreaturesSpitPerfect::RunTest(const FString& Parameters)
{
	if (!CreaturesReady(*this))
	{
		return false;
	}
	FVrRuleset Rules;
	if (!LoadLiveRules(*this, Rules))
	{
		return false;
	}
	// Hand timing. The hound is ready on tick 0, so the first StepGames schedules the spit: start tick 1, resolve tick
	// 1 + 36 = 37 (windupS 0.6). The ember spawns at x 11.8 on tick 37 and moves 7/60 m that same tick. It touches the
	// player when it has covered 3.8 - 0.38 - 0.12 = 3.3 m: 3.3 / (7/60) = 28.29, so on its 29th move, tick 37 + 28 = 65.
	// A raise on tick 60 is 5 ticks before the hit, inside absorb.perfect.windowS 0.15 (9 ticks). A raise on tick 50 is
	// 15 ticks before, outside it, and the held ward takes absorb.reduction.magic 0.85: 5 x 0.15 = 0.75.
	constexpr int32 HitTick = 65;
	struct FCase
	{
		const TCHAR* Label;
		int32 RaiseTick;
		bool bPerfect;
		double Damage;
	};
	const FCase Cases[] = {
		{TEXT("fresh"), HitTick - 5, true, 0.0},
		{TEXT("stale"), HitTick - 15, false, 0.75},
		{TEXT("none"), -1, false, 5.0},
	};
	bool bPass = true;
	for (const FCase& Case : Cases)
	{
		FGames Games;
		const int32 HoundId = MakeHoundDuel(Games, &Rules, LipX);
		const FArenaEvent* Hit = nullptr;
		int32 HitAt = -1;
		while (!Hit && Games.Phase == TEXT("active") && Games.State.Tick < SimTicks(3.0))
		{
			FInputFrame Input = AimAt(Games, HoundId);
			// StepGames advances to Tick + 1, and UpdateWard records the raise on that tick.
			Input.bAbsorb = Case.RaiseTick >= 0 && Games.State.Tick + 1 >= Case.RaiseTick;
			StepGames(Games, &Input);
			Hit = PlayerEvent(Games, TEXT("hit"));
			HitAt = Games.State.Tick;
		}
		const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
		if (!TestNotNull(*FString::Printf(TEXT("%s: ember hit"), Case.Label), Hit) || !Player)
		{
			bPass = false;
			continue;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s: hit tick"), Case.Label), HitAt, HitTick);
		bPass &= CreaturesNear(*this, *FString::Printf(TEXT("%s: damage"), Case.Label), Hit->Value, Case.Damage);
		bPass &= TestEqual(*FString::Printf(TEXT("%s: perfects"), Case.Label), Player->Metrics.Perfects, Case.bPerfect ? 1 : 0);
		bPass &= TestEqual(*FString::Printf(TEXT("%s: perfect event"), Case.Label), PlayerEvent(Games, TEXT("perfect")) != nullptr, Case.bPerfect);
		if (Case.RaiseTick >= 0)
		{
			bPass &= TestEqual(*FString::Printf(TEXT("%s: fresh tick"), Case.Label), Player->AbsorbFreshTick, Case.RaiseTick);
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreaturesDeathEmber, "MageArena.Creatures.DeathEmber",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreaturesDeathEmber::RunTest(const FString& Parameters)
{
	if (!CreaturesReady(*this))
	{
		return false;
	}
	FVrRuleset Rules;
	if (!LoadLiveRules(*this, Rules))
	{
		return false;
	}
	bool bPass = true;
	// Near: dies at the lip, 3.8 m out. Far: dies 12 m out, beyond the 9 m spit range; its ember still flies to the seat.
	const double Spots[] = {LipX, 20.0};
	for (const double HoundX : Spots)
	{
		FGames Games;
		const int32 HoundId = MakeHoundDuel(Games, &Rules, HoundX);
		// A second hound keeps the bout active, far off and out of range so it never spits in this window.
		const int32 KeeperId = AddEnemy(Games.State, TEXT("cinder_hound"), FSimVec{40.0, 60.0}).Id;
		FActor* Hound = SimFindActor(Games.State, HoundId);
		Hound->Hp = 0.0;
		Hound->bDown = true;
		const FInputFrame Idle = AimAt(Games, HoundId);
		StepGames(Games, &Idle);
		const int32 DeathTick = Games.State.Tick;
		const FSimVec DeathPos = Hound->Pos;
		bPass &= TestEqual(TEXT("one pinned burst"), CountOwned(Games, HoundId, TEXT("area")), 1);
		bPass &= TestEqual(TEXT("one ember telegraph"), CountOwned(Games, HoundId, TEXT("projectile")), 1);
		for (const FTelegraph& Telegraph : Games.State.Telegraphs)
		{
			if (Telegraph.OwnerId != HoundId)
			{
				continue;
			}
			// ember_burst delayS 0.6 = 36 ticks for both.
			bPass &= TestEqual(TEXT("resolves 36 ticks after the death tick"), Telegraph.ResolveTick, DeathTick + 36);
			bPass &= CreaturesNear(*this, TEXT("damage 5"), Telegraph.Damage, 5.0);
			bPass &= TestEqual(TEXT("magic"), Telegraph.Family, FString(TEXT("magic")));
			if (Telegraph.Kind == TEXT("area"))
			{
				bPass &= CreaturesNear(*this, TEXT("pinned burst radius 1.5"), Telegraph.WidthM, 1.5);
			}
			else
			{
				bPass &= CreaturesNear(*this, TEXT("ember speed 7"), Telegraph.SpeedMps, 7.0);
				const double Reach = SimDistance(DeathPos, KernelData().PlayerSpawn);
				bPass &= CreaturesNear(*this, TEXT("ember reach is the spit range, or the seat when farther"), Telegraph.RangeM, FMath::Max(9.0, Reach));
			}
		}
		int32 EmberTick = -1;
		int32 MaxShots = 0;
		while (Games.Phase == TEXT("active") && Games.State.Tick < DeathTick + 40)
		{
			const FInputFrame Input = AimAt(Games, HoundId);
			StepGames(Games, &Input);
			const int32 Shots = CountShots(Games, HoundId);
			MaxShots = FMath::Max(MaxShots, Shots);
			if (Shots > 0 && EmberTick < 0)
			{
				EmberTick = Games.State.Tick;
				for (const FProjectile& Projectile : Games.State.Projectiles)
				{
					if (Projectile.OwnerId == HoundId)
					{
						bPass &= CreaturesNear(*this, TEXT("launched from the death x"), Projectile.OriginPos.X, DeathPos.X);
						bPass &= CreaturesNear(*this, TEXT("launched from the death y"), Projectile.OriginPos.Y, DeathPos.Y);
						bPass &= CreaturesNear(*this, TEXT("in flight at 7 m/s"), SimLength(Projectile.Velocity), 7.0);
					}
				}
			}
		}
		UE_LOG(LogMageArena, Log, TEXT("Creatures death ember x=%.1f death=%d ember=%d shots=%d"), HoundX, DeathTick, EmberTick, MaxShots);
		bPass &= TestEqual(*FString::Printf(TEXT("x %.1f: ember 36 ticks after the death tick"), HoundX), EmberTick - DeathTick, 36);
		bPass &= TestEqual(*FString::Printf(TEXT("x %.1f: exactly one ember"), HoundX), MaxShots, 1);
		bPass &= TestEqual(TEXT("burst resolved"), CountOwned(Games, HoundId, TEXT("area")), 0);
		bPass &= TestEqual(TEXT("the keeper never spat"), CountShots(Games, KeeperId), 0);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreaturesNoRulesBite, "MageArena.Creatures.NoRulesBite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreaturesNoRulesBite::RunTest(const FString& Parameters)
{
	if (!CreaturesReady(*this))
	{
		return false;
	}
	bool bPass = true;
	// Null ruleset: the hound bites. Inside defaultMeleeRangeM 1.2 of the player, ready on tick 0.
	{
		FArenaState State = CreateArena();
		AddMage(State, 0, FSimVec{8.0, 10.0});
		const int32 HoundId = AddEnemy(State, TEXT("cinder_hound"), FSimVec{9.0, 10.0}).Id;
		const int32 NextIdBefore = State.NextId;
		StepArena(State, EnemyInputs(State));
		if (!TestEqual(TEXT("one telegraph"), State.Telegraphs.Num(), 1))
		{
			return false;
		}
		const FTelegraph& Bite = State.Telegraphs[0];
		bPass &= TestEqual(TEXT("owner"), Bite.OwnerId, HoundId);
		bPass &= TestEqual(TEXT("melee"), Bite.Kind, FString(TEXT("melee")));
		bPass &= TestEqual(TEXT("physical"), Bite.Family, FString(TEXT("physical")));
		bPass &= CreaturesNear(*this, TEXT("pinned bite damage 6"), Bite.Damage, 6.0);
		// windupS 0.35 at 60 Hz is 21 ticks.
		bPass &= TestEqual(TEXT("pinned bite windup"), Bite.ResolveTick - Bite.StartTick, 21);
		bPass &= TestEqual(TEXT("one id spent"), State.NextId, NextIdBefore + 1);
	}
	// Null ruleset: a death queues the pinned burst only, and nothing launches.
	{
		FArenaState State = CreateArena();
		AddMage(State, 0, FSimVec{8.0, 10.0});
		FActor& Hound = AddEnemy(State, TEXT("cinder_hound"), FSimVec{11.8, 10.0});
		const int32 HoundId = Hound.Id;
		Hound.Hp = 0.0;
		Hound.bDown = true;
		StepArena(State);
		QueueDeathEffects(State);
		bPass &= TestEqual(TEXT("burst only"), State.Telegraphs.Num(), 1);
		bPass &= TestEqual(TEXT("burst is the area"), State.Telegraphs.Num() > 0 ? State.Telegraphs[0].Kind : FString(), FString(TEXT("area")));
		int32 Shots = 0;
		for (int32 Step = 0; Step < 45; ++Step)
		{
			StepArena(State);
			for (const FProjectile& Projectile : State.Projectiles)
			{
				Shots += Projectile.OwnerId == HoundId ? 1 : 0;
			}
		}
		bPass &= TestEqual(TEXT("no ember without the overlay"), Shots, 0);
	}
	return bPass;
}

#endif
