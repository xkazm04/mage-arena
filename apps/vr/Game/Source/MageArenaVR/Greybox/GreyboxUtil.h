#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "GreyboxUtil.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * T37, D-G4 lever B: one tinted instance of a base material per colour, shared by every part of that colour, so parts
 * with one mesh and one colour merge under dynamic instancing (docs/research/BUDGET-DG4-2026-10.md 7.7, 9). The owner
 * holds it in a UPROPERTY so GC keeps the instances. A shared instance is never written after it is made: a recolour
 * swaps the part to the instance of the new colour (Greybox::Recolour).
 */
USTRUCT()
struct FGreyboxTintCache
{
	GENERATED_BODY()

	/** Base of every tint here. Null means Greybox::UnlitOpaqueMaterial(), set on the first SharedTint. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Base;

	UPROPERTY()
	TMap<FLinearColor, TObjectPtr<UMaterialInstanceDynamic>> Tints;
};

/** Engine basic shapes and flat-colour material instances. No project assets. */
namespace Greybox
{
UStaticMesh* LoadShape(const TCHAR* ShapeName);
UMaterialInterface* BasicMaterial();
UMaterialInterface* TranslucentMaterial();
/** Opaque unlit engine material. Its Color parameter is linear. It cannot instance. */
UMaterialInterface* UnlitOpaqueMaterial();
UMaterialInstanceDynamic* Tint(UMaterialInterface* Base, UObject* Outer, const FLinearColor& Colour);
/** The cache's instance for Colour, made with Tint the first time the colour is asked for. */
UMaterialInstanceDynamic* SharedTint(FGreyboxTintCache& Cache, UObject* Outer, const FLinearColor& Colour);
/** Swaps Part to the cache's instance for Colour. It writes no parameter, so no other part of the old colour changes. */
void Recolour(UStaticMeshComponent* Part, FGreyboxTintCache& Cache, UObject* Outer, const FLinearColor& Colour);
/** Steps of a per-frame fade amount in [0, 1]: a fade between two fixed colours asks the cache for at most 17 colours. */
constexpr float FadeSteps = 16.f;
/** Rounds a fade amount, clamped to [0, 1], to a multiple of 1 / Steps. */
float QuantiseFade(float Amount, float Steps = FadeSteps);

UInstancedStaticMeshComponent* MakeInstances(AActor* Owner, UStaticMesh* Mesh, UMaterialInterface* Material);
UStaticMeshComponent* MakeMesh(AActor* Owner, UStaticMesh* Mesh, UMaterialInterface* Material);

/** Full size is the desired width, depth and height in centimetres. */
void AddSized(UInstancedStaticMeshComponent* Instances, const FVector& CentreCm, const FQuat& Rotation, const FVector& FullSizeCm);
void SetSized(UStaticMeshComponent* MeshComponent, const FVector& FullSizeCm);
}
