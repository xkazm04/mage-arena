#pragma once

#include "CoreMinimal.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** Engine basic shapes and flat-colour material instances. No project assets. */
namespace Greybox
{
UStaticMesh* LoadShape(const TCHAR* ShapeName);
UMaterialInterface* BasicMaterial();
UMaterialInterface* TranslucentMaterial();
/** Opaque unlit engine material. Its Color parameter is linear. It cannot instance. */
UMaterialInterface* UnlitOpaqueMaterial();
UMaterialInstanceDynamic* Tint(UMaterialInterface* Base, UObject* Outer, const FLinearColor& Colour);

UInstancedStaticMeshComponent* MakeInstances(AActor* Owner, UStaticMesh* Mesh, UMaterialInterface* Material);
UStaticMeshComponent* MakeMesh(AActor* Owner, UStaticMesh* Mesh, UMaterialInterface* Material);

/** Full size is the desired width, depth and height in centimetres. */
void AddSized(UInstancedStaticMeshComponent* Instances, const FVector& CentreCm, const FQuat& Rotation, const FVector& FullSizeCm);
void SetSized(UStaticMeshComponent* MeshComponent, const FVector& FullSizeCm);
}
