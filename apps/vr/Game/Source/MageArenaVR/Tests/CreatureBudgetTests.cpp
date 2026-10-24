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
#include "Greybox/GreyboxCapture.h"
#include "Greybox/VisibleLod0.h"
#include "Session/SessionPresentation.h"

// docs/research/DRAWCALL-BUDGET-2026-10-25.md: the visible-LOD0 estimate counts the creatures (skinned meshes).

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

#endif
