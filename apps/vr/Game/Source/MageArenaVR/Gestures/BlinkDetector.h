#pragma once

#include "CoreMinimal.h"
#include "Gestures/HeldPointRate.h"
#include "Hands/HandFrame.h"
#include "Hands/SystemGestureGate.h"
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
	// G1 of docs/research/FEASIBILITY-2026-10.md. A tracker that updates at 25 to 30 Hz and is extrapolated to the frame
	// shows each camera update as a one-frame spike (the snap to the new sample), then frames that run on at the older,
	// slower velocity. Frame to frame that reads as a fall while the tip is still speeding up, so the flick fired up to 125 ms
	// early. Over a span that holds a whole camera period the speed is still at its peak. FallSpanS is one period at the
	// 25 Hz bound, three frames at 72 Hz. 25 to 40 ms give the same; 45 ms (four frames) lags and moves four 72 Hz fires.
	static constexpr double FallSpanS = 0.040;
	// A fall counts only when the FallSpanS speed is under this fraction of its peak since the flick armed. It sits between
	// the classes measured on the nine blink and three bolt clips (72 Hz, held and extrapolated at 30 and 25 Hz, three camera
	// phases): every fall that fired reads at most 0.854 (0.843 at 72 Hz), every grid dip at least 0.900, 206 of 223 exactly 1.
	static constexpr double FallSpanPeakFraction = 0.875;
	// A latched sample is also quiet when the tip is back within RearmSpeedMps of where it was RearmSpanMinS to RearmSpanMaxS
	// ago. An extrapolated stop runs on by up to one camera period and snaps back on the next update, so over a span of
	// one to three 25 Hz periods the overshoot cancels, while a recoil that keeps going at RearmSpeedMps never does. On held
	// samples the span also outlasts the 72 Hz rounding of the update time, which read a 0.54 m/s recoil as 0.64 m/s. The
	// re-arm within one camera period plus one frame: held30 18/27, extrap30 14/27, held25 27/27, extrap25 2/27 before;
	// 27/27 on all four after. Measured around these values: a lower end of 28 to 50 ms and an upper end of 100 to 150 ms
	// give the same; an upper end of 80 ms leaves held30 at 24/27, and 200 ms loses cases on three streams.
	static constexpr double RearmSpanMinS = 0.040;
	static constexpr double RearmSpanMaxS = 0.120;
};

/**
 * One flick per peak. The hottest pointing sample above the gates is kept, and the
 * event fires once the tip slows after that peak, along the peak's direction, and the
 * speed over FallSpanS has fallen too. It then re-arms when the tip has been still for
 * RearmQuietS, or at once on a backward time jump (a new clip).
 */
class FBlinkDetector
{
public:
	void Reset();
	void SetActivePad(int32 PadIndex);
	void Ingest(const FHandFrame& Frame);
	/** The system gesture took the casting hand. Forget the flick in progress; it must not fire when the bit clears. */
	void CancelForSystemGesture(EControllerHand Hand);
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
	/** Index-tip motion over held samples: a re-sent pose is no new sample, so it can neither peak nor release a flick. */
	FHeldPointRate TipRate;
	double HottestSpeed = 0.0;
	double HottestOmega = 0.0;
	double HottestYaw = 0.0;
	double HottestTime = 0.0;
	/** Unit horizontal direction of the hottest sample. The fall is the speed along it, so a snap back is a fall too. */
	FVector2D HottestDirection = FVector2D::ZeroVector;
	/** Peak of the FallSpanS speed since the flick armed. */
	double HottestSpanSpeed = 0.0;

	/** Ring of measured tip positions for the span speeds; the newest sits before HistoryHead. Covers RearmSpanMaxS up to 250 Hz. */
	struct FTipSample
	{
		FVector2D Point = FVector2D::ZeroVector;
		double TimeSeconds = 0.0;
	};
	static constexpr int32 HistoryCapacity = 32;
	FTipSample History[HistoryCapacity];
	int32 HistoryCount = 0;
	int32 HistoryHead = 0;

	void PushHistory(const FVector2D& Point, double TimeSeconds);
	/** Horizontal speed, m/s, from the newest sample at least SpanS old to Point. False while no sample is that old. */
	bool SpanSpeed(const FVector2D& Point, double TimeSeconds, double SpanS, double& OutSpeed) const;
	/** True when the tip is within RearmSpeedMps of where it was at some sample RearmSpanMinS to RearmSpanMaxS old. */
	bool BackWithinRearmSpan(const FVector2D& Point, double TimeSeconds) const;
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
	FSystemGestureGate Gate;
	FDelegateHandle HandHandle;
	TWeakObjectPtr<UHandInputSubsystem> BoundHands;
	int32 SeenBlinks = 0;
	int32 SeenBolts = 0;
};
