#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Greybox/ArenaLayout.h"
#include "HandPresentationComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class USigilRecognizerSubsystem;
class UStaticMeshComponent;

/**
 * Draws the active hand source as joint spheres and bone capsules in clip space.
 * A cast or reject flashes a ring. A raised ward draws the 140 degree arc.
 */
UCLASS()
class MAGEARENAVR_API UHandPresentationComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHandPresentationComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsCastFlashVisible() const { return bFlashOn && bFlashCast; }
	bool IsWardArcVisible() const { return bWardVisible; }

private:
	void BindSignals();
	void UnbindSignals();
	void HandleCast(FName Line, float Score, double LatencyMs);
	void HandleReject(double Distance);
	void UpdateHands();
	void UpdateFlash();
	void UpdateWard();
	void FillRing(UInstancedStaticMeshComponent* Instances, const FVector& CentreCm, const FVector& AxisA, const FVector& AxisB, double RadiusCm, double TubeCm);

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Joints;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Bones;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Flash;

	// Static mesh components, not instances. The unlit translucent material has no
	// instanced-mesh usage flag, and -game will not compile one. A handful of segments
	// stays inside the draw-call budget and keeps the arc translucent.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WardSegments;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CastMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RejectMaterial;

	FArenaLayout Layout;
	bool bReady = false;
	bool bFlashOn = false;
	bool bFlashCast = false;
	float FlashLeft = 0.f;
	bool bWardVisible = false;
	bool bHasRightTip = false;
	FVector RightIndexTip = FVector::ZeroVector;
	FDelegateHandle CastHandle;
	FDelegateHandle RejectHandle;
	TWeakObjectPtr<USigilRecognizerSubsystem> Sigils;
};
