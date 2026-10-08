#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Budget/BudgetFx.h"
#include "Budget/BudgetStressDriver.h"
#include "Budget/BudgetStressPlan.h"
#include "Greybox/ArenaLayout.h"
#include "Greybox/GreyboxUtil.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/Package.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetSharedMaterials, "MageArena.Budget.SharedMaterials",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetSharedMaterials::RunTest(const FString& Parameters)
{
	// The -MageArenaBudgetSharedMaterials lever: every part of the stress plan asks the cache for its colour,
	// and the cache hands out exactly one material per distinct colour. No renderer is needed to make the instances.
	FArenaLayout Layout;
	FBudgetStressPlan Plan;
	FString Error;
	if (!TestTrue(TEXT("layout loads"), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error))
		|| !TestTrue(TEXT("plan builds"), BuildBudgetStressPlan(Layout, Plan, Error)))
	{
		AddError(Error);
		return false;
	}
	UMaterialInterface* Base = Greybox::UnlitOpaqueMaterial();
	if (!TestNotNull(TEXT("unlit base material loads"), Base))
	{
		return false;
	}
	TArray<FLinearColor> Colours;
	for (const FBudgetEnemy& Enemy : Plan.Enemies)
	{
		Colours.Add(Enemy.Colour);
	}
	for (const FBudgetShot& Shot : Plan.Shots)
	{
		Colours.Add(Shot.Colour);
	}
	TSet<FLinearColor> Distinct(Colours);

	FBudgetMaterialCache Cache;
	TMap<FLinearColor, UMaterialInstanceDynamic*> First;
	for (const FLinearColor& Colour : Colours)
	{
		UMaterialInstanceDynamic* Instance = BudgetSharedTint(Cache, Base, GetTransientPackage(), Colour);
		if (!TestNotNull(TEXT("the cache makes a material"), Instance))
		{
			return false;
		}
		UMaterialInstanceDynamic*& Seen = First.FindOrAdd(Colour, Instance);
		TestTrue(*FString::Printf(TEXT("colour %s gets the same material every time"), *Colour.ToString()), Seen == Instance);
		FLinearColor Tint;
		TestTrue(TEXT("the material carries a colour parameter"), Instance->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Tint));
		TestTrue(*FString::Printf(TEXT("colour %s is the tint"), *Colour.ToString()), Tint.Equals(Colour));
	}
	TestEqual(TEXT("one material per distinct colour"), Cache.Num(), Distinct.Num());
	TSet<UMaterialInstanceDynamic*> Instances;
	for (const TPair<FLinearColor, UMaterialInstanceDynamic*>& Pair : Cache)
	{
		Instances.Add(Pair.Value);
	}
	TestEqual(TEXT("distinct colours never share a material"), Instances.Num(), Distinct.Num());
	TestTrue(TEXT("the plan has fewer colours than parts"), Distinct.Num() < Colours.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetFxPlan, "MageArena.Budget.FxPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetFxPlan::RunTest(const FString& Parameters)
{
	const TArray<FBudgetFxRow>& Rows = BudgetStress::FxRows();
	TestEqual(TEXT("six proxy systems"), Rows.Num(), BudgetStress::FxRowCount);
	int32 Gpu = 0;
	int32 SummedPeak = 0;
	for (const FBudgetFxRow& Row : Rows)
	{
		Gpu += Row.bGpu ? 1 : 0;
		SummedPeak += Row.PeakLive;
		if (!Row.bGpu)
		{
			TestTrue(*FString::Printf(TEXT("%s: a CPU row stays at or under %d"), *Row.Name, BudgetStress::MaxCpuPeakLive),
				Row.PeakLive <= BudgetStress::MaxCpuPeakLive);
		}
		TestTrue(*FString::Printf(TEXT("%s: asset path %s is under /Game/Budget/"), *Row.Name, *Row.AssetPath),
			Row.AssetPath.StartsWith(TEXT("/Game/Budget/")));
		TestTrue(*FString::Printf(TEXT("%s: template %s is an engine emitter template"), *Row.Name, *Row.TemplatePath),
			Row.TemplatePath.StartsWith(TEXT("/Niagara/DefaultAssets/Templates/Emitters/")));
	}
	TestEqual(TEXT("exactly one GPU row"), Gpu, BudgetStress::MaxGpuEmitters);
	TestTrue(TEXT("the summed peak is not trivially under the row"), SummedPeak >= 1000);
	TestTrue(TEXT("the summed peak is within the row"), SummedPeak <= BudgetStress::MaxLiveParticles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetParticleCount, "MageArena.Budget.ParticleCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetParticleCount::RunTest(const FString& Parameters)
{
	const FBudgetParticleCount Empty = CountEmitters(TConstArrayView<FBudgetEmitterSample>());
	TestEqual(TEXT("an empty list counts 0"), Empty.Total(), 0);
	TestEqual(TEXT("an empty list has no GPU emitter"), Empty.ActiveGpuEmitters, 0);

	auto Sample = [](bool bEnabled, EBudgetSimTarget Target, EBudgetExecutionState State, int32 Count)
	{
		FBudgetEmitterSample Out;
		Out.bEnabled = bEnabled;
		Out.SimTarget = Target;
		Out.ExecutionState = State;
		Out.ParticleCount = Count;
		return Out;
	};
	TArray<FBudgetEmitterSample> Samples;
	Samples.Add(Sample(true, EBudgetSimTarget::Cpu, EBudgetExecutionState::Active, 100));
	Samples.Add(Sample(true, EBudgetSimTarget::Cpu, EBudgetExecutionState::Inactive, 20));
	Samples.Add(Sample(false, EBudgetSimTarget::Cpu, EBudgetExecutionState::Active, 999));
	Samples.Add(Sample(false, EBudgetSimTarget::Gpu, EBudgetExecutionState::Active, 999));
	Samples.Add(Sample(true, EBudgetSimTarget::Gpu, EBudgetExecutionState::Active, 400));
	Samples.Add(Sample(true, EBudgetSimTarget::Gpu, EBudgetExecutionState::Inactive, 500));
	Samples.Add(Sample(true, EBudgetSimTarget::Gpu, EBudgetExecutionState::Complete, 700));
	const FBudgetParticleCount Count = CountEmitters(Samples);
	TestEqual(TEXT("disabled CPU emitters are skipped; an inactive CPU emitter keeps its live particles"), Count.Cpu, 120);
	TestEqual(TEXT("a GPU emitter counts once, and only while Active"), Count.Gpu, 400);
	TestEqual(TEXT("one active GPU emitter"), Count.ActiveGpuEmitters, 1);
	TestEqual(TEXT("the total is CPU plus GPU"), Count.Total(), 520);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetZeroIsNotAPass, "MageArena.Budget.ZeroIsNotAPass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetZeroIsNotAPass::RunTest(const FString& Parameters)
{
	const int32 Rows = BudgetStress::FxRowCount;
	FString Reason;
	TestTrue(TEXT("fewer live systems than rows is not exercised"),
		EvaluateParticles(Rows - 1, 1500, 0, 1, Reason) == EBudgetParticleVerdict::NotExercised);
	TestTrue(TEXT("0 live particles is not exercised"),
		EvaluateParticles(Rows, 0, 0, 0, Reason) == EBudgetParticleVerdict::NotExercised);
	TestTrue(TEXT("a frame with a total of 0 is not exercised"),
		EvaluateParticles(Rows, 1500, 1, 1, Reason) == EBudgetParticleVerdict::NotExercised);
	TestTrue(TEXT("a zero frame is not exercised even when the maximum is over the row"),
		EvaluateParticles(Rows, 5000, 3, 4, Reason) == EBudgetParticleVerdict::NotExercised);
	TestTrue(TEXT("the reason says why"), Reason.StartsWith(TEXT("not exercised")));
	TestTrue(TEXT("over 2,000 live particles fails"),
		EvaluateParticles(Rows, BudgetStress::MaxLiveParticles + 1, 0, 1, Reason) == EBudgetParticleVerdict::Fail);
	TestTrue(TEXT("over 1 GPU emitter fails"),
		EvaluateParticles(Rows, 1500, 0, BudgetStress::MaxGpuEmitters + 1, Reason) == EBudgetParticleVerdict::Fail);
	TestTrue(TEXT("exactly at the limits passes"),
		EvaluateParticles(Rows, BudgetStress::MaxLiveParticles, 0, BudgetStress::MaxGpuEmitters, Reason) == EBudgetParticleVerdict::Pass);
	TestEqual(TEXT("a pass reads exercised"), Reason, FString(TEXT("exercised")));
	return true;
}

#endif
