#pragma once

#include "CoreMinimal.h"
#include "Hands/HandFrame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BlinkDetector.generated.h"

class UHandInputSubsystem;

/** Flick toward the pad on the player's left, the pad on the right, or back to centre. */
enum class EBlinkDirection : uint8
{
	Left,
	Right,
	Back
};

struct FBlinkEvent
{
	EBlinkDirection Direction = EBlinkDirection::Left;
	/** Pad resolved from Direction and the active pad at the moment of the flick. */
	int32 PadIndex = 1;
	double TimeSeconds = 0.0;
	double YawDegrees = 0.0;
};

struct FBoltFlickEvent
{
	double TimeSeconds = 0.0;
	double YawDegrees = 0.0;
};

/**
 * Casting-hand point plus the flick's angular-rate spike.
 * The synthetic clips translate a rigid point, so the spike is |r x v| / |r|^2 of the
 * index tip about the wrist, not a change in the joint quaternion. Forward is Bolt.
 * Left, right and back are blinks. Gates are measured on the nine flick clips.
 */
struct FBlinkThresholds
{
	static constexpr double PointRatio = 1.50;
	static constexpr double MinOmegaRadPerS = 1.40;
	static constexpr double MinTipSpeedMps = 1.20;
	static constexpr double ForwardYawDeg = 35.0;
	static constexpr double SideYawDeg = 120.0;
	static constexpr double FallFraction = 0.55;
	// A latched flick lets go once the tip has moved under RearmSpeedMps for RearmQuietS, so a stream that never rewinds
	// (a headset) still sees the next flick. Half the arming speed, held for about ten frames at 72 Hz.
	static constexpr double RearmSpeedMps = 0.60;
	static constexpr double RearmQuietS = 0.15;
};

/**
 * One flick per peak. The hottest pointing sample above the gates is kept, and the
 * event fires once the tip slows after that peak. It then re-arms when the tip has
 * been still for RearmQuietS, or at once on a backward time jump (a new clip).
 */
class FBlinkDetector
{
public:
	void Reset();
	void SetActivePad(int32 PadIndex);
	void Ingest(const FHandFrame& Frame);
	/** Fire a latched peak if the clip ended before the tip slowed. */
	void Flush();

	int32 GetActivePad() const { return ActivePad; }
	int32 GetBlinkCount() const { return BlinkCount; }
	int32 GetBoltCount() const { return BoltCount; }
	double GetHottestSpeed() const { return HottestSpeed; }
	const FBlinkEvent& GetLastBlink() const { return LastBlink; }
	const FBoltFlickEvent& GetLastBolt() const { return LastBolt; }

	/** Left steps down, right steps up, back returns to centre. Clamped to 0..2. */
	static int32 ResolvePad(EBlinkDirection Direction, int32 ActivePad);

private:
	void Arm();
	void Fire(double YawDegrees, double TimeSeconds);

	int32 ActivePad = 1;
	int32 BlinkCount = 0;
	int32 BoltCount = 0;
	FBlinkEvent LastBlink;
	FBoltFlickEvent LastBolt;
	bool bHasPrev = false;
	bool bLatched = false;
	double QuietFor = 0.0;
	double LastTime = 0.0;
	FVector PrevIndexTip = FVector::ZeroVector;
	double HottestSpeed = 0.0;
	double HottestOmega = 0.0;
	double HottestYaw = 0.0;
	double HottestTime = 0.0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlinkFlick, const FBlinkEvent& /*Blink*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBoltFlick, const FBoltFlickEvent& /*Bolt*/);

/** Forwards casting-hand flicks from the active hand source. */
UCLASS()
class MAGEARENAVR_API UBlinkDetectorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void BindToHands(UHandInputSubsystem* Hands);
	void SetActivePad(int32 PadIndex);
	/** Reset the detector and the broadcast counters together. */
	void ResetDetector();
	/** Fire a clip-end peak and broadcast it. Mid-clip this would steal the flick yaw. */
	void FlushPending();

	FBlinkDetector& GetDetector() { return Detector; }
	const FBlinkDetector& GetDetector() const { return Detector; }

	FOnBlinkFlick OnBlink;
	FOnBoltFlick OnBolt;

private:
	void HandleHandFrame(const FHandFrame& Frame);
	void Publish();

	FBlinkDetector Detector;
	FDelegateHandle HandHandle;
	TWeakObjectPtr<UHandInputSubsystem> BoundHands;
	int32 SeenBlinks = 0;
	int32 SeenBolts = 0;
};
