#pragma once

#include "CoreMinimal.h"

/**
 * Displacement of one tracked point that survives held samples (F7(b) of docs/research/STACK-OPPORTUNITIES-2026-10.md).
 * A tracker that updates slower than the frame rate re-sends its last pose: at 30 Hz under 72 Hz frames, two of every
 * three frames repeat. Measured frame to frame, the speed then reads 0 on a repeat and about three times the real speed
 * on a change. A ward onset walk stops on the repeat, and a blink release fires on it after the first stepped burst.
 *
 * A repeat of the point is not a new sample. It is reported Held and measures nothing; the next change is measured from
 * the last measured sample, over the whole hold. A repeat that lasts past MaxHoldS is stillness and is measured frame to
 * frame, so a stream without repeats, or with only long still spells, measures exactly what it did before.
 */
struct FHeldPointRate
{
	/** A 30 Hz update under 72 Hz frames holds a pose for at most 2/72 s after the change. Slack for one late update. */
	static constexpr double MaxHoldS = 0.05;
	/** A re-sent pose equals the last one up to the clip player's interpolation rounding, far under any tracker noise. */
	static constexpr double RepeatToleranceCm = 1.0e-3;

	enum class EStep : uint8
	{
		/** The first point of a stream. Nothing to measure against. */
		First,
		/** The point repeats inside MaxHoldS: no new sample. The caller keeps its last measurement. */
		Held,
		/** OutDelta and OutDt span from the last measured sample to this one. */
		Measured
	};

	void Reset()
	{
		bHasPoint = false;
	}

	EStep Step(const FVector& Point, double TimeSeconds, FVector& OutDelta, double& OutDt)
	{
		OutDelta = FVector::ZeroVector;
		OutDt = 0.0;
		if (!bHasPoint)
		{
			bHasPoint = true;
			LastPoint = Point;
			ChangeTime = TimeSeconds;
			MeasuredTime = TimeSeconds;
			return EStep::First;
		}
		const bool bRepeat = FVector::DistSquared(Point, LastPoint) <= FMath::Square(RepeatToleranceCm);
		if (bRepeat && TimeSeconds - ChangeTime <= MaxHoldS)
		{
			return EStep::Held;
		}
		OutDelta = Point - LastPoint;
		OutDt = TimeSeconds - MeasuredTime;
		if (!bRepeat)
		{
			ChangeTime = TimeSeconds;
		}
		LastPoint = Point;
		MeasuredTime = TimeSeconds;
		return EStep::Measured;
	}

private:
	FVector LastPoint = FVector::ZeroVector;
	/** When LastPoint first appeared. A repeat within MaxHoldS of it is a held sample. */
	double ChangeTime = 0.0;
	/** When the last measurement was taken. The next one spans from here. */
	double MeasuredTime = 0.0;
	bool bHasPoint = false;
};
