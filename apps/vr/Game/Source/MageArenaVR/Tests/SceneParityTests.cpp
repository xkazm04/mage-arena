#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Budget/BudgetStressDriver.h"
#include "Budget/BudgetStressPlan.h"
#include "Budget/SceneCensus.h"
#include "Camera/CameraTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Greybox/ArenaLayout.h"
#include "Greybox/GreyboxCapture.h"
#include "Greybox/GreyboxUtil.h"
#include "Greybox/VisibleLod0.h"
#include "Kernel/VrRules.h"
#include "Session/PresentationLook.h"
#include "Session/SessionPresentation.h"

// T46: the D-G4 stress capture measures the shipping presentation, and a scene-parity check fails the moment the stress
// plan and the presentation diverge (docs/research/BUDGET-DG4-2026-10.md section 12, runs/T46/parity-census.md). The
// census comparison is a pure function over two part lists, so these run under -nullrhi.

namespace
{
// The cinder hound colour the stress plan carried before T46 (BudgetStressPlan.cpp HoundColour), against the
// presentation's HoundBody (T26). A divergence the plan's copy hid until T46.
const FLinearColor PreT46HoundColour(0.8f, 0.3f, 0.05f);

bool BuildPlan(FAutomationTestBase& Test, FBudgetStressPlan& Plan)
{
	FArenaLayout Layout;
	FVrRuleset Rules;
	FString Error;
	if (!Test.TestTrue(TEXT("layout loads"), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error)) || !Test.TestTrue(TEXT("the VR overlay loads"), LoadVrRuleset(Rules, Error)) || !Test.TestTrue(TEXT("plan builds"), BuildBudgetStressPlan(Layout, Plan, Error, &Rules)))
	{
		Test.AddError(Error);
		return false;
	}
	return true;
}

// The bodies of the plan's enemies: as the plan colours them, or as ASessionPresentation draws them (PresentationLook).
FSceneCensus Bodies(const FBudgetStressPlan& Plan, bool bAsPresentation)
{
	FSceneCensus Census;
	for (const FBudgetEnemy& Enemy : Plan.Enemies)
	{
		const FBodyLook Look = PresentationLook::Body(Enemy.EnemyId);
		Census.Add(bAsPresentation ? OpaquePartKey(Look.Shape, Look.Colour) : OpaquePartKey(*Enemy.Shape, Enemy.Colour), 1, Enemy.EnemyId + TEXT(" body"));
	}
	return Census;
}

FCensusKey TranslucentKey(const TCHAR* Mesh, const FLinearColor& Colour)
{
	FCensusKey Key;
	Key.Mesh = Mesh;
	Key.Colour = Colour;
	Key.Material = TEXT("M_SimpleUnlitTranslucent");
	Key.Pass = ECensusPass::Translucent;
	return Key;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetParityDivergentColour, "MageArena.Budget.Parity.DivergentColourFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetParityDivergentColour::RunTest(const FString& Parameters)
{
	FBudgetStressPlan Plan;
	if (!BuildPlan(*this, Plan))
	{
		return false;
	}
	// The real pair: the stress plan's enemy bodies must be the bodies the presentation draws. Before T46 the plan kept its
	// own copy of the colours and the hound had drifted, so this was red.
	const FSceneParity Real = CompareSceneCensus(Bodies(Plan, false), Bodies(Plan, true));
	const FString RealWhy = Real.bOk ? FString(TEXT("no divergence")) : FString::Join(Real.Divergences, TEXT(" | "));
	TestTrue(*FString::Printf(TEXT("the stress plan's bodies are the presentation's (%s)"), *RealWhy), Real.bOk);

	// Two censuses that differ only in one enemy colour: the presentation's bodies, and the same with the hound in the
	// colour the plan used to carry. The check must fail and name the enemy and both colours.
	const FSceneCensus Shipping = Bodies(Plan, true);
	FSceneCensus Drifted;
	for (const FBudgetEnemy& Enemy : Plan.Enemies)
	{
		const FBodyLook Look = PresentationLook::Body(Enemy.EnemyId);
		const FLinearColor Colour = Enemy.EnemyId == TEXT("cinder_hound") ? PreT46HoundColour : Look.Colour;
		Drifted.Add(OpaquePartKey(Look.Shape, Colour), 1, Enemy.EnemyId + TEXT(" body"));
	}
	const FSceneParity Diverged = CompareSceneCensus(Drifted, Shipping);
	const FString Why = FString::Join(Diverged.Divergences, TEXT(" | "));
	TestFalse(TEXT("one enemy colour apart fails the check"), Diverged.bOk);
	TestEqual(TEXT("exactly one divergence"), Diverged.Divergences.Num(), 1);
	TestTrue(*FString::Printf(TEXT("the divergence names the hound (%s)"), *Why), Why.Contains(TEXT("cinder_hound body")));
	const FBodyLook Hound = PresentationLook::Body(TEXT("cinder_hound"));
	TestTrue(*FString::Printf(TEXT("the divergence names the planned colour (%s)"), *Why), Why.Contains(OpaquePartKey(TEXT("Cube"), PreT46HoundColour).ColourText()));
	TestTrue(*FString::Printf(TEXT("the divergence names the drawn colour (%s)"), *Why), Why.Contains(OpaquePartKey(TEXT("Cube"), Hound.Colour).ColourText()));
	TestTrue(TEXT("a recoloured census hashes differently"), Drifted.Hash() != Shipping.Hash());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetParityIdentical, "MageArena.Budget.Parity.IdenticalScenesPass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetParityIdentical::RunTest(const FString& Parameters)
{
	FBudgetStressPlan Plan;
	if (!BuildPlan(*this, Plan))
	{
		return false;
	}
	FSceneCensus Planned = BudgetStressCensus(Plan);
	// The plan's parts: 20 enemies x 3, 100 shots, one ground ring per spear and per ember.
	const int32 Rings = Plan.Shots.FilterByPredicate([](const FBudgetShot& Shot)
	{ return Shot.bSpear || Shot.bEmber; })
	                        .Num();
	TestEqual(TEXT("planned parts"), Planned.Parts(), 3 * BudgetStress::EnemyCount + BudgetStress::ProjectileCount + Rings);
	TestTrue(TEXT("the plan requires the translucent pass of the session frame"), Planned.RequiredPasses.Contains(ECensusPass::Translucent));

	// The drawn census: the same rows, plus a translucent part, which the plan requires.
	FSceneCensus Drawn;
	for (const FCensusRow& Row : Planned.Rows)
	{
		Drawn.Add(Row.Key, Row.Count);
	}
	Drawn.Add(TranslucentKey(TEXT("Sphere"), FLinearColor(0.009721f, 0.745404f, 0.745404f, 0.10f)));
	const FSceneParity Same = CompareSceneCensus(Planned, Drawn);
	TestTrue(*FString::Printf(TEXT("identical scenes pass (%s)"), *FString::Join(Same.Divergences, TEXT(" | "))), Same.bOk);
	TestEqual(TEXT("the one extra row is the session's own part, listed"), Same.Extras.Num(), 1);

	// Equal censuses hash equal, in any row order.
	FSceneCensus Reversed;
	for (int32 Index = Planned.Rows.Num() - 1; Index >= 0; --Index)
	{
		Reversed.Add(Planned.Rows[Index].Key, Planned.Rows[Index].Count);
	}
	TestEqual(TEXT("row order does not change the hash"), Reversed.Hash(), Planned.Hash());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetParityMissingPass, "MageArena.Budget.Parity.MissingPassFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetParityMissingPass::RunTest(const FString& Parameters)
{
	// A4's gap: the shipping frame draws a translucent pass (cuff, dome, curtains) and the pre-T46 stress scene had none.
	const FCensusKey Body = OpaquePartKey(TEXT("Cylinder"), FLinearColor(0.45f, 0.48f, 0.52f));
	FSceneCensus Shipping;
	Shipping.Add(Body, 5, TEXT("conscript body"));
	Shipping.Add(TranslucentKey(TEXT("Sphere"), FLinearColor(0.009721f, 0.745404f, 0.745404f, 0.10f)), 1, TEXT("dome shell"));
	FSceneCensus OpaqueOnly;
	OpaqueOnly.Add(Body, 5);
	const FSceneParity Missing = CompareSceneCensus(Shipping, OpaqueOnly);
	const FString Why = FString::Join(Missing.Divergences, TEXT(" | "));
	TestFalse(TEXT("a census with no translucent pass fails against one that has one"), Missing.bOk);
	TestTrue(*FString::Printf(TEXT("the divergence names the missing pass (%s)"), *Why), Why.Contains(TEXT("missing translucent pass")));

	// A required pass fails the same way with no planned row in it: the stress plan requires the session frame's
	// translucency, so a frame of the plan's opaque parts alone is not the shipping frame.
	FBudgetStressPlan Plan;
	FArenaLayout Layout;
	FString Error;
	if (Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error) && BuildBudgetStressPlan(Layout, Plan, Error))
	{
		const FSceneCensus Planned = BudgetStressCensus(Plan);
		FSceneCensus Drawn;
		for (const FCensusRow& Row : Planned.Rows)
		{
			Drawn.Add(Row.Key, Row.Count);
		}
		const FSceneParity Required = CompareSceneCensus(Planned, Drawn);
		TestFalse(TEXT("the plan's opaque parts alone miss the required translucent pass"), Required.bOk);
		TestTrue(TEXT("and only that"), Required.Divergences.Num() == 1 && Required.Divergences[0].Contains(TEXT("missing translucent pass")));
	}
	else
	{
		AddError(Error);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetTrianglesSingleSourced, "MageArena.Budget.Triangles.SingleSourced",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetTrianglesSingleSourced::RunTest(const FString& Parameters)
{
	// One visible-LOD0 sum (Greybox::CountVisibleLod0) behind both captures: the same world and view give the same value
	// through UGreyboxCaptureDriver's call site and UBudgetStressDriver's, and that value is the hand sum.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SceneParityTests"));
	if (!TestNotNull(TEXT("a world is made"), World))
	{
		return false;
	}
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASessionPresentation* Owner = World->SpawnActor<ASessionPresentation>(ASessionPresentation::StaticClass(), FTransform::Identity, Params);
	bool bPass = TestNotNull(TEXT("an owner for the parts spawns"), Owner);
	if (bPass)
	{
		const FLinearColor Grey(0.5f, 0.5f, 0.5f);
		UStaticMeshComponent* Ahead = Owner->MakePart(TEXT("Cube"), Grey);
		UStaticMeshComponent* AheadSphere = Owner->MakePart(TEXT("Sphere"), Grey);
		UStaticMeshComponent* Behind = Owner->MakePart(TEXT("Cube"), Grey);
		UStaticMeshComponent* Hidden = Owner->MakePart(TEXT("Cube"), Grey);
		bPass &= TestTrue(TEXT("four parts are made"), Ahead && AheadSphere && Behind && Hidden);
		if (bPass)
		{
			Ahead->SetWorldLocation(FVector(800.0, 0.0, 0.0));
			AheadSphere->SetWorldLocation(FVector(800.0, 200.0, 0.0));
			Behind->SetWorldLocation(FVector(-800.0, 0.0, 0.0));
			Hidden->SetWorldLocation(FVector(800.0, -200.0, 0.0));
			Hidden->SetVisibility(false);
			FMinimalViewInfo View;
			View.Location = FVector::ZeroVector;
			View.Rotation = FRotator::ZeroRotator;
			View.FOV = 90.f;
			View.AspectRatio = 16.f / 9.f;
			View.ProjectionMode = ECameraProjectionMode::Perspective;
			const int32 CubeTris = Greybox::MeshLod0Triangles(Ahead->GetStaticMesh());
			const int32 SphereTris = Greybox::MeshLod0Triangles(AheadSphere->GetStaticMesh());
			bPass &= TestTrue(*FString::Printf(TEXT("the shapes have LOD0 triangles (cube %d, sphere %d)"), CubeTris, SphereTris), CubeTris > 0 && SphereTris > 0);
			const int32 Shared = Greybox::CountVisibleLod0(World, View);
			const int32 FromGreybox = UGreyboxCaptureDriver::VisibleLod0(World, View);
			const int32 FromStress = UBudgetStressDriver::VisibleLod0(World, View);
			bPass &= TestEqual(TEXT("in front and visible counts; behind and hidden do not"), Shared, CubeTris + SphereTris);
			bPass &= TestEqual(TEXT("the greybox capture's call site reads the shared sum"), FromGreybox, Shared);
			bPass &= TestEqual(TEXT("the stress capture's call site reads the shared sum"), FromStress, Shared);
		}
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return bPass;
}

#endif
