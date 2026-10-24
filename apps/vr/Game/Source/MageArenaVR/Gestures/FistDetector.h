#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayTypes.h"
#include "Hands/HandFrame.h"
#include "Hands/SystemGestureGate.h"

/**
 * T25 closed-fist gates. A finger's curl is the summed flexion between its four bones (metacarpal, proximal,
 * intermediate, distal), in degrees; the thumb is over the fingers when its tip is near the middle of the index and
 * middle intermediate joints (the second knuckles a closing thumb lies across).
 *
 * Measured off the clipgen hands (apps/vr/tools/clipgen/generate.mjs CURLS, fist.mjs): the fist reads 203-218 degrees
 * on every finger and the thumb tip 3.5-4.2 cm from the knuckles; the loose half-curl impostor (fist-loose) reads at most
 * 106 degrees; the sigil pen's index finger at most 130 degrees on the sloppy takes; an open or relaxed hand under 50. A
 * fist with the thumb out (pinch 0) puts the thumb 7.3 cm away. CloseCurlDeg sits between the pen and the fist;
 * OpenCurlDeg and ThumbReleaseM are the hysteresis, so tip jitter on a sloppy take does not break a held fist.
 * OpenPalmCurlDeg is the other side: the open and relaxed hands read 9-49 degrees, so an open palm (the spare) and a
 * fist (the finish) can never both hold on one frame.
 *
 * HoldS is teach.json offerHoldS, the stone hold (docs/DESKTOP-INPUT.md); MageArena.Fist.HoldMatchesStones checks it.
 * MinConfidence is the floor the both-palms check uses (FArenaSession::BothPalmsRaised).
 *
 * The staff grip (staff-plant, staff-lift) closes both hands around the shaft and reads as a fist; only the missio
 * choice reads this detector, and no staff is held there.
 */
struct FFistThresholds
{
	static constexpr double CloseCurlDeg = 160.0;
	static constexpr double OpenCurlDeg = 140.0;
	static constexpr double ThumbOverM = 0.055;
	static constexpr double ThumbReleaseM = 0.065;
	static constexpr double OpenPalmCurlDeg = 60.0;
	static constexpr double HoldS = 0.40;
	static constexpr float MinConfidence = 0.2f;
};

/**
 * Closed fist held for FFistThresholds::HoldS, on either hand. Pure: no actor, no world, no subsystem. Feed it hand
 * frames; each hand is timed on its own clock. One count per hold, until the hand opens, drops below the confidence
 * floor, rewinds, or the system gesture takes it.
 */
class FFistDetector
{
public:
	void Reset();
	void Ingest(const FHandFrame& Frame);

	/** The hand is closed into a fist now (not necessarily for HoldS yet). */
	bool IsClosed(EControllerHand Hand) const;
	/** Fists held for HoldS since the last Reset. */
	int32 GetHeldCount() const { return HeldCount; }

	static double FingerCurlDeg(const FHandFrame& Frame, EHandKeypoint Metacarpal);
	static double ThumbOverMetres(const FHandFrame& Frame);
	/** The pose alone, one frame. bWasClosed applies the release thresholds instead of the close ones. */
	static bool IsFistPose(const FHandFrame& Frame, bool bWasClosed);
	/** Every finger straighter than OpenPalmCurlDeg, above the confidence floor. */
	static bool IsOpenHand(const FHandFrame& Frame);

private:
	struct FHandState
	{
		double Since = 0.0;
		double LastTime = 0.0;
		bool bClosed = false;
		bool bCounted = false;
		bool bHasTime = false;
	};

	FHandState* StateFor(EControllerHand Hand);
	const FHandState* StateFor(EControllerHand Hand) const;

	FHandState Left;
	FHandState Right;
	FSystemGestureGate Gate;
	int32 HeldCount = 0;
};
