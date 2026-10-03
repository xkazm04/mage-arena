#pragma once

#include "CoreMinimal.h"
#include "Combat/AbsorbResolver.h"
#include "Hands/HandFrame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WardDetector.generated.h"

class UHandInputSubsystem;

/**
 * Detector gates. 35 degrees is the task. Height, hysteresis and 0.30 m/s are measured off
 * ward-raise.normal: the smoothstep's first sample is under 0.20 m/s and the next is about 0.56,
 * the 35-degree crossing is near 0.167 s, and fingertip jitter is not on the palm.
 *
 * OnsetTailS is 29/72 s. ward-raise.normal.settle rises, sits still for 0.30 s short of the pose,
 * then rotates into it. A binary search of that clip plus ward-raise.normal, at injected latency
 * <= 60 ms, puts the shortest working tail just over 28 frames. 29 frames is the next sample.
 * The 60 ms confirm-to-last-fast gap on the settle clip is 30 frames; the walk only has to land
 * on the first slow sample, and the fast-region walk does the rest.
 *
 * Sub-threshold frames are bridged only while the palm stays above LowerHeightM. Stillness in
 * the lap is not the settle, and stepping onto it would pull the onset back into an earlier
 * twitch. The fast walk is not height-gated: a real rise starts below this line.
 */
struct FWardThresholds
{
	static constexpr double RaiseAngleDeg = 35.0;
	static constexpr double LowerAngleDeg = 50.0;
	static constexpr double RaiseHeightM = 0.20;
	static constexpr double LowerHeightM = 0.16;
	static constexpr double OnsetSpeedMps = 0.30;
	static constexpr double OnsetTailS = 29.0 / 72.0;
	static constexpr double HistorySeconds = 1.0;
};

struct FWardEvent
{
	double OnsetTime = 0.0;
	FVector Facing = FVector::ForwardVector;
	bool bFresh = false;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWardRaised, double /*OnsetTime*/, FVector /*Facing*/);
DECLARE_MULTICAST_DELEGATE(FOnWardLowered);

/** Off-hand palm ward. Onset is the palm-speed rising edge, not the frame that finally looks raised. */
class FWardDetector
{
public:
	bool Init(FString& OutError);
	void ResetStream();
	void Ingest(const FHandFrame& Frame);

	void SetAimFacing(const FVector& Facing);
	void SetThreatFacing(const FVector& Facing, bool bTargeted);

	bool IsReady() const { return bReady; }
	const FAbsorbRules& Rules() const { return LoadedRules; }
	const FWardState& GetState() const { return State; }
	const FWardEvent& GetLastRaise() const { return LastRaise; }
	int32 GetRaiseCount() const { return RaiseCount; }
	int32 GetLowerCount() const { return LowerCount; }
	/** Time of the last transition out of the raised pose. Huge and negative until that happens. */
	double GetLoweredTime() const { return LoweredTime; }
	bool HasSeenLowered() const { return bSeenLowered; }

	FOnWardRaised OnRaised;
	FOnWardLowered OnLowered;

	static FVector PalmNormal(const FHandFrame& Frame);
	static double PalmHeightMetres(const FHandFrame& Frame);
	static double AngleToFacingDeg(const FVector& Normal, const FVector& Facing);

private:
	struct FSample
	{
		double Time = 0.0;
		FVector PalmCm = FVector::ZeroVector;
		double SpeedMps = 0.0;
		double HeightM = 0.0;
	};

	FVector ReferenceFacing() const;
	FVector PalmFacing(const FVector& Normal) const;
	double FindOnset() const;
	void UpdateState(const FVector& Facing);
	void NoteRewind();

	FAbsorbRules LoadedRules;
	FWardState State;
	FWardEvent LastRaise;
	TArray<FSample> History;
	FVector AimFacing = FVector::ForwardVector;
	FVector ThreatFacing = FVector::ForwardVector;
	double LastTime = 0.0;
	double LoweredTime = -1.0e9;
	/** First sample whose palm was in the lap. The first raise cannot see a lower, so this is its walk bound. */
	double LapBoundTime = -1.0e9;
	int32 RaiseCount = 0;
	int32 LowerCount = 0;
	bool bReady = false;
	bool bRaised = false;
	bool bHasTime = false;
	bool bEverLowered = false;
	bool bSeenLowered = false;
	bool bHasLapBound = false;
	bool bThreatTargeted = false;
};

/** Broadcasts ward edges from the active hand source. Tests can Init and bind without GameInstance::Init. */
UCLASS()
class MAGEARENAVR_API UWardDetectorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool InitDetector(FString& OutError);
	void BindToHands(UHandInputSubsystem* Hands);
	void SetAimFacing(const FVector& Facing);
	void SetThreatFacing(const FVector& Facing, bool bTargeted);

	FWardDetector& GetDetector() { return Detector; }
	const FWardDetector& GetDetector() const { return Detector; }
	const FWardState& GetState() const { return Detector.GetState(); }
	const FWardEvent& GetLastRaise() const { return Detector.GetLastRaise(); }
	int32 GetRaiseCount() const { return Detector.GetRaiseCount(); }
	int32 GetLowerCount() const { return Detector.GetLowerCount(); }

	FOnWardRaised OnWardRaised;
	FOnWardLowered OnWardLowered;

private:
	void HandleHandFrame(const FHandFrame& Frame);

	FWardDetector Detector;
	FDelegateHandle HandHandle;
	FDelegateHandle RaisedHandle;
	FDelegateHandle LoweredHandle;
	TWeakObjectPtr<UHandInputSubsystem> BoundHands;
	bool bForwarding = false;
};
