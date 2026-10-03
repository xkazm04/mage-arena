#pragma once

#include "CoreMinimal.h"
#include "Hands/HandFrame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StaffDetector.generated.h"

class UHandInputSubsystem;

DECLARE_MULTICAST_DELEGATE(FOnStaffPlant);
DECLARE_MULTICAST_DELEGATE(FOnStaffLift);

/**
 * Both hands grip a shaft and thrust it down about 25 cm, or lift it back up.
 * A mudra rises into the seal and is only close at the end, so it does not pass.
 * One event per gesture, until the grip breaks or the clip rewinds.
 */
class FStaffDetector
{
public:
	void Reset();
	void Ingest(const FHandFrame& Frame);

	int32 GetPlantCount() const { return PlantCount; }
	int32 GetLiftCount() const { return LiftCount; }

	FOnStaffPlant OnPlant;
	FOnStaffLift OnLift;

private:
	struct FHandSample
	{
		double Time = 0.0;
		double PalmZM = 0.0;
		double PalmYM = 0.0;
		float Pinch = 0.0f;
		bool bSeen = false;
	};

	struct FWindowSample
	{
		double Time = 0.0;
		double PalmZM = 0.0;
	};

	void ResetWindow();

	FHandSample Left;
	FHandSample Right;
	TArray<FWindowSample> Window;
	int32 PlantCount = 0;
	int32 LiftCount = 0;
	bool bLatched = false;
};

/** Broadcasts plant and lift edges from the hand source. */
UCLASS()
class MAGEARENAVR_API UStaffDetectorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void BindToHands(UHandInputSubsystem* Hands);
	FStaffDetector& GetDetector() { return Detector; }
	const FStaffDetector& GetDetector() const { return Detector; }

	FOnStaffPlant OnPlant;
	FOnStaffLift OnLift;

private:
	// NewObject skips Initialize. The session listens to this subsystem, not FStaffDetector.
	void EnsureForwarding();
	void HandleHandFrame(const FHandFrame& Frame);

	FStaffDetector Detector;
	FDelegateHandle HandHandle;
	FDelegateHandle PlantHandle;
	FDelegateHandle LiftHandle;
	TWeakObjectPtr<UHandInputSubsystem> BoundHands;
	bool bForwarding = false;
};
