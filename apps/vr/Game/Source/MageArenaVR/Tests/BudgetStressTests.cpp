#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Budget/BudgetStressPlan.h"
#include "Greybox/ArenaLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetStressPopulation, "MageArena.Budget.StressPopulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetStressPopulation::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	FString Error;
	if (!TestTrue(TEXT("layout file loads"), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error)))
	{
		AddError(Error);
		return false;
	}
	FBudgetStressPlan Plan;
	if (!TestTrue(TEXT("plan builds without a renderer"), BuildBudgetStressPlan(Layout, Plan, Error)))
	{
		AddError(Error);
		return false;
	}

	// The D-G4 row: 20 enemy proxies, 100 projectiles, both hands.
	TestEqual(TEXT("20 enemy proxies"), Plan.Enemies.Num(), 20);
	TestEqual(TEXT("100 projectiles"), Plan.Shots.Num(), 100);
	TestEqual(TEXT("2 hands"), Plan.Hands, 2);

	// The rings are the spawn-marker radii (8 m and 15 m), inside the +/-70 degree arc.
	TestEqual(TEXT("near ring is the near marker radius"), Plan.NearDistanceM, 8.0);
	TestEqual(TEXT("far ring is the far marker radius"), Plan.FarDistanceM, 15.0);
	int32 Near = 0;
	int32 Far = 0;
	for (const FBudgetEnemy& Enemy : Plan.Enemies)
	{
		Near += FMath::IsNearlyEqual(Enemy.DistanceM, Plan.NearDistanceM) ? 1 : 0;
		Far += FMath::IsNearlyEqual(Enemy.DistanceM, Plan.FarDistanceM) ? 1 : 0;
		TestTrue(*FString::Printf(TEXT("%s at %.1f deg is inside the 70 deg arc"), *Enemy.EnemyId, Enemy.BearingDeg),
			FMath::Abs(Enemy.BearingDeg) <= BudgetStress::ArcLimitDeg + 1.0e-6);
		TestTrue(*FString::Printf(TEXT("%s at (%.2f, %.2f) stands on the floor, clear of the wall"), *Enemy.EnemyId, Enemy.GroundM.X, Enemy.GroundM.Y),
			BudgetPointOnFloor(Layout, Enemy.GroundM, BudgetStress::EdgeMarginM));
		TestEqual(*FString::Printf(TEXT("%s stands on the surface"), *Enemy.EnemyId), Enemy.GroundM.Z, Layout.SurfaceHeightM(Enemy.GroundM.X, Enemy.GroundM.Y));
	}
	TestEqual(TEXT("10 enemies on the near ring"), Near, 10);
	TestEqual(TEXT("10 enemies on the far ring"), Far, 10);

	// A straight lane between two points inside the convex floor ellipse stays inside it. Spears only add height.
	const FRunePad* Pad = Layout.FindPadById(TEXT("centre"));
	if (!TestNotNull(TEXT("centre pad"), Pad))
	{
		return false;
	}
	int32 Spears = 0;
	for (int32 Index = 0; Index < Plan.Shots.Num(); ++Index)
	{
		const FBudgetShot& Shot = Plan.Shots[Index];
		Spears += Shot.bSpear ? 1 : 0;
		TestTrue(*FString::Printf(TEXT("shot %d starts on the floor"), Index), BudgetPointOnFloor(Layout, Shot.StartM, 0.0));
		TestTrue(*FString::Printf(TEXT("shot %d ends on the floor"), Index), BudgetPointOnFloor(Layout, Shot.EndM, 0.0));
		TestTrue(*FString::Printf(TEXT("shot %d ends within 1.5 m of the centre pad"), Index),
			FVector::Dist2D(Shot.EndM, Pad->PositionM) <= 1.5);
		TestTrue(*FString::Printf(TEXT("shot %d has a positive flight"), Index), Shot.FlightS > 0.0);
		// Respawn on arrival: one flight later the shot is back at the same point of its lane.
		const FVector Now = BudgetShotPositionM(Shot, 3.0);
		const FVector Later = BudgetShotPositionM(Shot, 3.0 + Shot.FlightS);
		TestTrue(*FString::Printf(TEXT("shot %d loops"), Index), FVector::Dist(Now, Later) < 1.0e-3);
		int32 Before = 0;
		int32 After = 0;
		BudgetShotPositionM(Shot, 3.0, &Before);
		BudgetShotPositionM(Shot, 3.0 + Shot.FlightS, &After);
		TestEqual(*FString::Printf(TEXT("shot %d relaunches once per flight"), Index), After - Before, 1);
	}
	TestEqual(TEXT("every 4th shot is a spear"), Spears, 25);
	return true;
}

#endif
