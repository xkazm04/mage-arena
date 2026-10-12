#pragma once

#include "CoreMinimal.h"
#include "Gestures/QPointCloudRecognizer.h"
#include "InputCoreTypes.h"

struct FHandClip;

enum class ESigilDrawStyle : uint8
{
	/** Pinch is the pen. Down at >= 0.7, up below 0.5. Strokes are separated by pen-up. */
	StyleA,
	/** One stroke while the fingertip is moving. A 150 ms stillness ends it. Pinch is ignored. */
	StyleB,
};

enum class ESigilStrokeEvent : uint8
{
	None,
	/** Circle check passed and the gesture is complete. OutPoints is the 2D path. */
	Gesture,
	/** Pen lifted on a path that is not a sigil (no circle), or a bare circle timed out. */
	Rejected,
};

/**
 * Index-tip samples in centimetres to a 2D path on the best-fit plane.
 * A gesture must contain a loop. The loop may close on a later sample, not only
 * the stroke start, and the peak turn counts when the inner stroke unwinds the
 * final turn. A closed loop with no inner stroke after it waits, then rejects.
 * A later stroke that reaches less far than a spell line rejects. Both tests
 * measure reach, not path, relative to the loop's own size.
 */
class FSigilStrokeBuilder
{
public:
	static constexpr double PinchDown = 0.7;
	static constexpr double PinchUp = 0.5;
	static constexpr double ClosedStartEndRatio = 0.40;
	static constexpr double LoopTurnDegrees = 300.0;
	static constexpr double MinSegmentCm = 1.0;
	static constexpr double MinLoopPathCm = 25.0;
	// The inner mark is measured by its reach (the diagonal of its bounding box),
	// not its path. A path adds up every back-and-forth: a zigzag's legs, and the
	// overshoot and snap-back that an extrapolating hand tracker adds at every
	// corner and stop (G1, docs/research/FEASIBILITY-2026-10.md). On the
	// extrapolated 25 Hz model that took a zigzag's path from <= 0.273 to 0.408
	// of the loop diagonal, past spell lines, and a circle-only tail past the old
	// 0.65 path ratio. A spell line is a straight reach across the circle, so its
	// reach is close to its path; a jagged mark's reach stays small.
	//
	// One stroke: the tail after the loop closes. Circle-only tails reach <= 0.40
	// of the loop diagonal at 72 Hz and <= 0.475 on the modelled 30 and 25 Hz
	// streams. A circle drawn through into the inner stroke reaches >= 0.595.
	// The ratio is size-free, so a larger circle still waits.
	static constexpr double InnerTailRatio = 0.53;
	// Later strokes are the inner mark. Spell lines reach >= 0.385 of the loop
	// diagonal at 72 Hz and >= 0.351 on the modelled streams; a dot, zigzag or
	// short hook reaches <= 0.200 on every stream.
	static constexpr double MinInnerStrokeRatio = 0.28;
	static constexpr double InnerWaitSeconds = 0.40;
	static constexpr double StillSpeedCmPerSec = 5.0;
	static constexpr double StillGapSeconds = 0.150;

	void Reset();
	void SetStyle(ESigilDrawStyle InStyle);
	ESigilDrawStyle GetStyle() const { return Style; }
	/** T26: the pen is down (a stroke is being drawn). Read by the audio for the draw loop. */
	bool IsPenDown() const { return bPen; }

	ESigilStrokeEvent AddTip(const FVector& TipCm, float Pinch, double TimeSeconds, TArray<FQPoint>& OutPoints);

	/** Ends a pen that is still down. Does not promote a bare circle that is waiting. */
	ESigilStrokeEvent Flush(TArray<FQPoint>& OutPoints);

private:
	ESigilStrokeEvent AddPinch(const FVector& TipCm, float Pinch, double TimeSeconds, TArray<FQPoint>& OutPoints);
	float SynthesizePinch(const FVector& TipCm, double TimeSeconds);
	ESigilStrokeEvent Decide(TArray<FQPoint>& OutPoints);
	bool Project(TArray<TArray<FQPoint>>& OutStrokes) const;
	static void WritePoints(const TArray<TArray<FQPoint>>& Strokes, TArray<FQPoint>& OutPoints);

	struct FStrokeStats
	{
		double Path = 0.0;
		double Ratio = TNumericLimits<double>::Max();
		/** Peak |net turn| reached anywhere in the stroke, in degrees. */
		double Turn = 0.0;
		bool bClosed = false;
		/** A prefix of the stroke closed the loop (returned near the start, or turned 300 degrees). */
		bool bLoopClosed = false;
		/** Reach of the stroke after the loop closed: the diagonal of its bounding box. */
		double TailDiagonal = 0.0;
		double LoopDiagonal = 0.0;
	};

	static FStrokeStats StatsOf(const TArray<FQPoint>& Points);
	static bool HasLoop(const TArray<TArray<FQPoint>>& Strokes);
	static bool HasInnerStroke(const FStrokeStats& Stats);
	static bool IsReady(const TArray<TArray<FQPoint>>& Strokes);

	ESigilDrawStyle Style = ESigilDrawStyle::StyleA;
	TArray<TArray<FVector>> Strokes;
	bool bPen = false;
	bool bHasSample = false;
	double LastSampleTime = 0.0;
	double PenUpTime = 0.0;

	bool bHasMotionSample = false;
	bool bInMotion = false;
	FVector MotionTip = FVector::ZeroVector;
	double MotionTime = 0.0;
	double MotionStill = 0.0;
};

/** First completed gesture on Hand. The default is the right hand, which is how templates are stored. */
bool ExtractFirstGesture(const FHandClip& Clip, ESigilDrawStyle Style, TArray<FQPoint>& OutPoints, EControllerHand Hand = EControllerHand::Right);
