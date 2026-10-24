#include "Greybox/VisibleLod0.h"

#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ConvexVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "StaticMeshResources.h"

int32 Greybox::MeshLod0Triangles(const UStaticMesh* Mesh)
{
	if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0)
	{
		return 0;
	}
	return Mesh->GetRenderData()->LODResources[0].GetNumTriangles();
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

int32 Greybox::CountVisibleLod0(UWorld* World, const FMinimalViewInfo& View)
{
	if (!World)
	{
		return 0;
	}
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FMatrix ViewProjection;
	UGameplayStatics::GetViewProjectionMatrix(View, ViewMatrix, ProjectionMatrix, ViewProjection);
	FConvexVolume Frustum;
	GetViewFrustumBounds(Frustum, ViewProjection, false);

	int32 Total = 0;
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
						Total += Tris;
					}
				}
			}
			else if (Frustum.IntersectBox(Component->Bounds.Origin, Component->Bounds.BoxExtent))
			{
				Total += Tris;
			}
		}
	}
	return Total;
}
