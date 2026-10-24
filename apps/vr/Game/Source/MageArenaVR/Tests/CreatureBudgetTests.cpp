#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/SkeletalMeshActor.h"
#include "Budget/BudgetStressDriver.h"
#include "Camera/CameraTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Greybox/CreatureImport.h"
#include "Greybox/GreyboxCapture.h"
#include "Greybox/VisibleLod0.h"
#include "Session/SessionPresentation.h"

// docs/research/DRAWCALL-BUDGET-2026-10-25.md: the visible-LOD0 estimate counts the creatures (skinned meshes), and the
// creature cast's draw-call cuts (the section merge, per-bone velocity off, the LODs, the shadow policy) are pinned.

namespace
{
const TCHAR* SkeletalCubePath = TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube");

ASkeletalMeshActor* SpawnSkinned(UWorld& World, USkeletalMesh* Mesh, const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkeletalMeshActor* Actor = World.SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(Location), Params);
	if (Actor)
	{
		Actor->GetSkeletalMeshComponent()->SetSkeletalMesh(Mesh);
		Actor->GetSkeletalMeshComponent()->UpdateBounds();
	}
	return Actor;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBudgetTrianglesSkeletal, "MageArena.Budget.Triangles.Skeletal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBudgetTrianglesSkeletal::RunTest(const FString& Parameters)
{
	// The creatures are skeletal meshes, and until this card the estimate counted static meshes only, so with all three
	// in view lod0InFrustumTriangles stayed at the no-creature value (CREATURE-CAST-UE-2026-10-24 section 2.6). A skinned
	// component in the frustum now counts its LOD0 render data, in its own field and in the total; one behind the view or
	// hidden does not.
	USkeletalMesh* Cube = LoadObject<USkeletalMesh>(nullptr, SkeletalCubePath);
	if (!TestNotNull(TEXT("the engine's skeletal cube loads"), Cube))
	{
		return false;
	}
	const int32 SkinnedTris = Greybox::SkinnedLod0Triangles(Cube);
	TestTrue(*FString::Printf(TEXT("the skeletal cube has LOD0 triangles (%d)"), SkinnedTris), SkinnedTris > 0);
	TestEqual(TEXT("no asset has no triangles"), Greybox::SkinnedLod0Triangles(nullptr), 0);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CreatureBudgetTests"));
	if (!TestNotNull(TEXT("a world is made"), World))
	{
		return false;
	}
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASessionPresentation* Owner = World->SpawnActor<ASessionPresentation>(ASessionPresentation::StaticClass(), FTransform::Identity, Params);
	bool bPass = TestNotNull(TEXT("an owner for the static part spawns"), Owner);
	ASkeletalMeshActor* Ahead = SpawnSkinned(*World, Cube, FVector(800.0, 0.0, 0.0));
	ASkeletalMeshActor* AheadLeft = SpawnSkinned(*World, Cube, FVector(800.0, -250.0, 0.0));
	ASkeletalMeshActor* Behind = SpawnSkinned(*World, Cube, FVector(-800.0, 0.0, 0.0));
	ASkeletalMeshActor* Hidden = SpawnSkinned(*World, Cube, FVector(800.0, 250.0, 0.0));
	bPass &= TestTrue(TEXT("four skinned actors spawn"), Ahead && AheadLeft && Behind && Hidden);
	UStaticMeshComponent* Static = bPass ? Owner->MakePart(TEXT("Cube"), FLinearColor(0.5f, 0.5f, 0.5f)) : nullptr;
	bPass &= TestNotNull(TEXT("a static part is made"), Static);
	if (bPass)
	{
		Static->SetWorldLocation(FVector(800.0, 120.0, 0.0));
		Hidden->SetActorHiddenInGame(true);
		FMinimalViewInfo View;
		View.Location = FVector::ZeroVector;
		View.Rotation = FRotator::ZeroRotator;
		View.FOV = 90.f;
		View.AspectRatio = 16.f / 9.f;
		View.ProjectionMode = ECameraProjectionMode::Perspective;
		const int32 StaticTris = Greybox::MeshLod0Triangles(Static->GetStaticMesh());
		const Greybox::FVisibleLod0 Split = Greybox::CountVisibleLod0Split(World, View);
		bPass &= TestEqual(TEXT("the two skinned meshes in front count; behind and hidden do not"), Split.SkeletalTriangles, 2 * SkinnedTris);
		bPass &= TestEqual(TEXT("the static share is the static part alone, as before the skinned count"), Split.StaticTriangles, StaticTris);
		bPass &= TestEqual(TEXT("the total is both shares"), Split.Total(), StaticTris + 2 * SkinnedTris);
		bPass &= TestEqual(TEXT("the shared sum is the split's total"), Greybox::CountVisibleLod0(World, View), Split.Total());
		bPass &= TestEqual(TEXT("the greybox capture's call site counts the skinned meshes"), UGreyboxCaptureDriver::VisibleLod0(World, View), Split.Total());
		bPass &= TestEqual(TEXT("the stress capture's call site counts the skinned meshes"), UBudgetStressDriver::VisibleLod0(World, View), Split.Total());
		// A drawn LOD does not change the figure: it is the LOD0 estimate whatever the screen size picks.
		Ahead->GetSkeletalMeshComponent()->SetForcedLOD(2);
		bPass &= TestEqual(TEXT("a forced LOD still counts LOD0"), Greybox::CountVisibleLod0Split(World, View).SkeletalTriangles, 2 * SkinnedTris);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreatureImportSectionMerge, "MageArena.CreatureImport.SectionMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreatureImportSectionMerge::RunTest(const FString& Parameters)
{
	// Every mesh section costs a draw in each pass (base, depth prepass, velocity, two cascades), so the cut folds a
	// creature's slots into one. The maw's tongue keeps a flat dark red by pointing its own corners at a body texel of that
	// colour; a corner a body triangle also uses keeps its UV, so the body's texture never moves.
	const CreatureImport::FCreatureSpec* Maw = CreatureImport::FindSpec(TEXT("MireMaw"));
	const CreatureImport::FCreatureSpec* Hound = CreatureImport::FindSpec(TEXT("CinderHound"));
	const CreatureImport::FCreatureSpec* Brennic = CreatureImport::FindSpec(TEXT("Brennic"));
	if (!TestNotNull(TEXT("the maw has a spec"), Maw) || !TestNotNull(TEXT("the hound has a spec"), Hound) || !TestNotNull(TEXT("Brennic has a spec"), Brennic))
	{
		return false;
	}
	TestTrue(TEXT("the tongue texel is inside the texture"), Maw->FlatSlotUv.X > 0.f && Maw->FlatSlotUv.X < 1.f && Maw->FlatSlotUv.Y > 0.f && Maw->FlatSlotUv.Y < 1.f);

	// The maw: two body triangles (corners 0-5), a tongue triangle of its own (6-8) and one that shares corner 0 with the
	// body (0, 9, 10).
	const FName MawSlots[] = {TEXT("M_MireMaw"), TEXT("M_MireMaw_Tongue")};
	const int32 MawGroups[] = {0, 0, 1, 1};
	const int32 MawCorners[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 9, 10};
	const CreatureImport::FSectionMerge Merge = CreatureImport::PlanSectionMerge(MawSlots, MawGroups, MawCorners, *Maw);
	TestEqual(TEXT("two slots before"), Merge.SlotsBefore, 2);
	TestEqual(TEXT("one slot after"), Merge.SlotsAfter, 1);
	TestEqual(TEXT("the body is the target (the most triangles)"), Merge.TargetGroup, 0);
	TestEqual(TEXT("both tongue triangles fold in"), Merge.TrianglesMoved, 2);
	TestTrue(TEXT("the tongue's own corners take the texel"), Merge.FlatCorners == TArray<int32>({6, 7, 8, 9, 10}));
	TestEqual(TEXT("the shared corner is counted and left"), Merge.FlatCornersShared, 1);

	// The hound's two slots both take M_CinderHound: they fold without a UV moving. The larger group is kept even when it
	// is not the first.
	const FName HoundSlots[] = {TEXT("M_CinderHound"), TEXT("M_CinderHound_Ember")};
	const int32 HoundGroups[] = {0, 1, 1};
	const int32 HoundCorners[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	const CreatureImport::FSectionMerge HoundMerge = CreatureImport::PlanSectionMerge(HoundSlots, HoundGroups, HoundCorners, *Hound);
	TestEqual(TEXT("the hound folds to one slot"), HoundMerge.SlotsAfter, 1);
	TestEqual(TEXT("the larger group is the target"), HoundMerge.TargetGroup, 1);
	TestEqual(TEXT("one hound triangle folds in"), HoundMerge.TrianglesMoved, 1);
	TestEqual(TEXT("no hound corner moves"), HoundMerge.FlatCorners.Num(), 0);

	// One slot (Brennic), or two groups already on one slot name (a merged mesh): nothing to fold.
	const FName OneSlot[] = {TEXT("M_Brennic")};
	const int32 OneGroups[] = {0, 0};
	const int32 OneCorners[] = {0, 1, 2, 3, 4, 5};
	const CreatureImport::FSectionMerge One = CreatureImport::PlanSectionMerge(OneSlot, OneGroups, OneCorners, *Brennic);
	TestTrue(TEXT("one slot merges nothing"), One.SlotsBefore == 1 && One.SlotsAfter == 1 && One.TrianglesMoved == 0);
	const FName Merged[] = {TEXT("M_CinderHound"), TEXT("M_CinderHound")};
	const CreatureImport::FSectionMerge Again = CreatureImport::PlanSectionMerge(Merged, HoundGroups, HoundCorners, *Hound);
	TestTrue(TEXT("a merged mesh merges nothing"), Again.SlotsBefore == 1 && Again.SlotsAfter == 1);
	// An empty group does not count as a slot.
	const int32 OnlyBody[] = {0, 0};
	const CreatureImport::FSectionMerge Empty = CreatureImport::PlanSectionMerge(MawSlots, OnlyBody, OneCorners, *Maw);
	TestTrue(TEXT("an empty tongue group leaves one slot"), Empty.SlotsBefore == 1 && Empty.FlatCorners.Num() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreatureImportBudgetPolicy, "MageArena.CreatureImport.BudgetPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreatureImportBudgetPolicy::RunTest(const FString& Parameters)
{
	// The policy this card settles (docs/DECISIONS.md 2026-10-25, DRAWCALL-BUDGET-2026-10-25 section 3).
	const CreatureImport::FCreatureBudget& Budget = CreatureImport::DefaultBudget();
	TestTrue(TEXT("the sections merge"), Budget.bMergeSections);
	TestFalse(TEXT("per-bone velocity is off"), Budget.bPerBoneVelocity);
	TestTrue(TEXT("the shadow stays in the cascades"), Budget.Shadow == CreatureImport::ECreatureShadow::Cascades);
	TestTrue(TEXT("LODs are generated"), Budget.bGenerateLods);
	TestEqual(TEXT("no LOD is forced"), Budget.ForcedLod, -1);

	// The spawn rings of arena-layout.json: near at 8 m, far at 15 m. A near-ring creature keeps LOD0 and its shadow; LOD1
	// takes over between the rings and LOD2 past the far ring.
	constexpr double NearRingM = 8.0;
	constexpr double FarRingM = 15.0;
	TestTrue(TEXT("LOD1 takes over past the near ring"), Budget.Lod1DistanceM > NearRingM + 2.0 && Budget.Lod1DistanceM < FarRingM);
	TestTrue(TEXT("LOD2 takes over past the far ring"), Budget.Lod2DistanceM > FarRingM);
	TestTrue(TEXT("each LOD keeps a share of the one before"), Budget.Lod1TriangleShare < 1.0 && Budget.Lod2TriangleShare < Budget.Lod1TriangleShare && Budget.Lod2TriangleShare > 0.0);
	for (const double Distance : {NearRingM, FarRingM, 30.0})
	{
		TestTrue(*FString::Printf(TEXT("a creature casts a shadow at %.0f m"), Distance), CreatureImport::CastsShadowAt(Budget, Distance) == (Budget.ShadowMaxDistanceM <= 0.0 || Distance <= Budget.ShadowMaxDistanceM));
	}
	TestTrue(TEXT("a near-ring creature casts its shadow"), CreatureImport::CastsShadowAt(Budget, NearRingM + 0.5));

	// The engine's screen size: the bounds sphere's diameter over the view. Brennic's sphere (105 cm) at 8 m is about
	// 0.12 of a Quest 3 eye and 0.23 of the desktop's 90 deg view; LOD1's threshold sits below both.
	constexpr double BrennicSphereCm = 105.4;
	constexpr double DesktopScreenMultiple = 0.889;
	const double QuestAt8 = CreatureImport::ScreenSizeAt(BrennicSphereCm, NearRingM);
	const double DesktopAt8 = CreatureImport::ScreenSizeAt(BrennicSphereCm, NearRingM, DesktopScreenMultiple);
	const double Lod1 = CreatureImport::ScreenSizeAt(BrennicSphereCm, Budget.Lod1DistanceM);
	TestTrue(*FString::Printf(TEXT("Brennic at 8 m on Quest is about 0.12: %.3f"), QuestAt8), FMath::IsNearlyEqual(QuestAt8, 0.1186, 0.002));
	TestTrue(*FString::Printf(TEXT("and on the desktop about 0.23: %.3f"), DesktopAt8), FMath::IsNearlyEqual(DesktopAt8, 0.2342, 0.002));
	TestTrue(TEXT("LOD0 holds at 8 m on Quest"), QuestAt8 > Lod1);
	TestTrue(TEXT("the screen size halves when the distance doubles"), FMath::IsNearlyEqual(CreatureImport::ScreenSizeAt(100.0, 16.0), 0.5 * CreatureImport::ScreenSizeAt(100.0, 8.0), 1.0e-9));

	// The uncut budget is the import scene before this card: the before row of DRAWCALL-BUDGET-2026-10-25.
	const CreatureImport::FCreatureBudget Uncut = CreatureImport::UncutBudget();
	TestTrue(TEXT("uncut keeps the imported sections, per-bone velocity, the cascades and LOD0 only"),
	    !Uncut.bMergeSections && Uncut.bPerBoneVelocity && Uncut.Shadow == CreatureImport::ECreatureShadow::Cascades && !Uncut.bGenerateLods && Uncut.ShadowMaxDistanceM <= 0.0);
	CreatureImport::FCreatureBudget Off = Budget;
	Off.Shadow = CreatureImport::ECreatureShadow::Off;
	TestFalse(TEXT("shadow off casts none"), CreatureImport::CastsShadowAt(Off, 1.0));
	CreatureImport::FCreatureBudget Near = Budget;
	Near.ShadowMaxDistanceM = 10.0;
	TestTrue(TEXT("within the shadow distance a creature casts"), CreatureImport::CastsShadowAt(Near, 9.9));
	TestFalse(TEXT("past it none"), CreatureImport::CastsShadowAt(Near, 10.1));
	return true;
}

#endif
