#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Greybox/ArenaLayout.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "ThreatDemo.generated.h"

class UStaticMeshComponent;

/** One launched threat. Landing Z is the sand or the dais top. End Z stays at chest height. */
struct FThreatLaunch
{
	FThreatSpec Spec;
	FVector StartM = FVector::ZeroVector;
	FVector EndM = FVector::ZeroVector;
	FVector LandingM = FVector::ZeroVector;
};

/** NullRHI-safe. Launch uses this plan, then skips mesh spawn when there is no RHI. */
TArray<FThreatLaunch> BuildThreatLaunches(const FArenaLayout& Layout, const FRunePad& Pad);

/** One pixel the capture projects into the threat screenshot. TargetSrgb is the encoded authored colour. */
struct FThreatColourProbe
{
	FString Kind;
	FString Role;
	FVector WorldCm = FVector::ZeroVector;
	FLinearColor TargetSrgb = FLinearColor::White;
};

/** Owns the threat meshes. The subsystem moves them. */
UCLASS()
class MAGEARENAVR_API AThreatRig : public AActor
{
	GENERATED_BODY()

public:
	AThreatRig();

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	friend class UThreatDemoSubsystem;
};

/**
 * Console command MageArena.Demo.Threats.
 * Launches water, fire, steel, then an unblockable, in that order, toward the active pad.
 * Visual only.
 */
UCLASS()
class MAGEARENAVR_API UThreatDemoSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UThreatDemoSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	void Launch();
	bool IsShowcaseMoment() const;
	int32 GetFlightCount() const { return Flights.Num(); }
	const TArray<FThreatLaunch>& GetPlan() const { return Plan; }

	/** Hold body and ring transforms so a screenshot matches the projected centres. */
	void SetFrozen(bool bInFrozen);
	bool IsFrozen() const { return bFrozen; }

	/** Body centres, plus a camera-right point on the unblockable rim. */
	TArray<FThreatColourProbe> MakeColourProbes(const FVector& CameraWorldCm) const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual void BeginDestroy() override;

private:
	struct FFlight
	{
		FThreatSpec Spec;
		FVector StartCm = FVector::ZeroVector;
		FVector EndCm = FVector::ZeroVector;
		FVector LandingCm = FVector::ZeroVector;
		double StartTime = 0.0;
		double Duration = 1.0;
		TObjectPtr<UStaticMeshComponent> Body;
		TArray<TObjectPtr<UStaticMeshComponent>> RimSegments;
		TArray<TObjectPtr<UStaticMeshComponent>> RingSegments;
	};

	void EnsureRig();
	void ClearFlights();
	void UpdateFlight(FFlight& Flight, double Now);
	void FillRing(FFlight& Flight, double RadiusM);

	UPROPERTY()
	TObjectPtr<AThreatRig> Rig;

	TArray<FFlight> Flights;
	TArray<FThreatLaunch> Plan;
	FArenaLayout Layout;
	bool bLayout = false;
	bool bRunning = false;
	bool bFrozen = false;
};
