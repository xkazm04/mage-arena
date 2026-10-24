#include "Greybox/VisibleLod0.h"

#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ConvexVolume.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "StaticMeshResources.h"

int32 Greybox::MeshLod0Triangles(const UStaticMesh* Mesh)
{
	if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0)
	{
		return 0;
	}
	return Mesh->GetRenderData()->LODResources[0].GetNumTriangles();
}

int32 Greybox::SkinnedLod0Triangles(const USkinnedAsset* Asset)
{
	const FSkeletalMeshRenderData* RenderData = Asset ? Asset->GetResourceForRendering() : nullptr;
	if (!RenderData || RenderData->LODRenderData.Num() == 0)
	{
		return 0;
	}
	return RenderData->LODRenderData[0].GetTotalFaces();
}

bool Greybox::FirstPlayerView(UWorld* World, FMinimalViewInfo& OutView)
{
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller || !Controller->PlayerCameraManager)
	{
		return false;
	}
	OutView = Controller->PlayerCameraManager->GetCameraCacheView();
	return true;
}

Greybox::FVisibleLod0 Greybox::CountVisibleLod0Split(UWorld* World, const FMinimalViewInfo& View)
{
	FVisibleLod0 Out;
	if (!World)
	{
		return Out;
	}
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FMatrix ViewProjection;
	UGameplayStatics::GetViewProjectionMatrix(View, ViewMatrix, ProjectionMatrix, ViewProjection);
	FConvexVolume Frustum;
	GetViewFrustumBounds(Frustum, ViewProjection, false);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UStaticMeshComponent*> Components;
		It->GetComponents<UStaticMeshComponent>(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			if (!Component || !Component->IsVisible())
			{
				continue;
			}
			UStaticMesh* Mesh = Component->GetStaticMesh();
			const int32 Tris = MeshLod0Triangles(Mesh);
			if (Tris <= 0)
			{
				continue;
			}
			if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
			{
				const int32 Count = Instances->GetInstanceCount();
				for (int32 Index = 0; Index < Count; ++Index)
				{
					FTransform InstanceTransform;
					if (!Instances->GetInstanceTransform(Index, InstanceTransform, true))
					{
						continue;
					}
					const FBoxSphereBounds WorldBounds = Mesh->GetBounds().TransformBy(InstanceTransform);
					if (Frustum.IntersectBox(WorldBounds.Origin, WorldBounds.BoxExtent))
					{
						Out.StaticTriangles += Tris;
					}
				}
			}
			else if (Frustum.IntersectBox(Component->Bounds.Origin, Component->Bounds.BoxExtent))
			{
				Out.StaticTriangles += Tris;
			}
		}
		// The creatures (Greybox/CreatureImport): LOD0 of the skinned asset, the same estimate as a static mesh's. A hidden
		// actor (SetActorHiddenInGame) or component is not drawn and not counted.
		TArray<USkinnedMeshComponent*> Skinned;
		It->GetComponents<USkinnedMeshComponent>(Skinned);
		for (USkinnedMeshComponent* Component : Skinned)
		{
			if (!Component || !Component->IsVisible() || It->IsHidden())
			{
				continue;
			}
			const int32 Tris = SkinnedLod0Triangles(Component->GetSkinnedAsset());
			if (Tris > 0 && Frustum.IntersectBox(Component->Bounds.Origin, Component->Bounds.BoxExtent))
			{
				Out.SkeletalTriangles += Tris;
			}
		}
	}
	return Out;
}

int32 Greybox::CountVisibleLod0(UWorld* World, const FMinimalViewInfo& View)
{
	return CountVisibleLod0Split(World, View).Total();
}
