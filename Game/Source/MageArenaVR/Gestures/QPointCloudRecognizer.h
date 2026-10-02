#pragma once

#include "CoreMinimal.h"

/** One 2D sample. StrokeId keeps resample from bridging a pen-up gap. */
struct FQPoint
{
	double X = 0.0;
	double Y = 0.0;
	int32 StrokeId = 0;
};

struct FQClassifyResult
{
	FName Label;
	float Score = 0.0f;
	double Distance = TNumericLimits<double>::Max();
	bool bReject = true;
	double Milliseconds = 0.0;
};

/**
 * $Q point-cloud recognizer (Vatavu, Anthony, Wobbrock, MobileHCI 2018).
 * Resample n=32, translate to the centroid, scale onto the integer grid 0..m-1
 * (m=64), LUT lower bounds, cloud-match both directions with early abandoning.
 */
class FQPointCloudRecognizer
{
public:
	static constexpr int32 PointCount = 32;
	static constexpr int32 LutSize = 64;
	// Correct matches on this corpus top out at 23595 (one sloppy line3). Open arcs
	// start at 49796. Bare circles sit at 13084-14583 and a short inner mark at
	// 4271-11034, overlapping the correct tail. A cut below 13084 also rejects the
	// correct sloppy matches at 14659 and 23595 and drops sloppy accuracy from
	// 82/90 to 80/90, under the 90% bar, so the stroke builder rejects those shapes.
	// 26000 is 2405 above the worst correct match and 23796 below the nearest
	// impostor that does not overlap it.
	static constexpr double DefaultRejectDistance = 26000.0;

	FQPointCloudRecognizer();

	void Reset();
	void SetRejectDistance(double InRejectDistance);
	double GetRejectDistance() const { return RejectDistance; }

	/** Ignores a path that cannot be normalized. Returns false when nothing was stored. */
	bool AddTemplate(FName Label, const TArray<FQPoint>& Points);

	int32 GetTemplateCount() const { return Templates.Num(); }
	int32 CountTemplates(FName Label) const;

	FQClassifyResult Classify(const TArray<FQPoint>& Points) const;

private:
	struct FGridPoint
	{
		int32 X = 0;
		int32 Y = 0;
	};

	struct FTemplate
	{
		FName Label;
		TArray<FGridPoint> Points;
		TArray<int32> Lut;
	};

	static int64 Squared(const FGridPoint& A, const FGridPoint& B)
	{
		const int64 DX = static_cast<int64>(A.X) - static_cast<int64>(B.X);
		const int64 DY = static_cast<int64>(A.Y) - static_cast<int64>(B.Y);
		return DX * DX + DY * DY;
	}

	/** OutPoints holds PointCount samples. OutLut holds LutSize * LutSize entries. */
	bool Normalize(const TArray<FQPoint>& Points, FGridPoint* OutPoints, int32* OutLut) const;
	void FillLowerBound(const FGridPoint* From, const FGridPoint* To, const int32* ToLut, double* Bound) const;
	double MinimumLowerBound(const FGridPoint* Points, const int32* PointsLut, const FTemplate& Template) const;
	/**
	 * True when this template's completed cloud distance is strictly under MinSoFar,
	 * or equal to it when bKeepTies (a lower original index may still win a tie).
	 */
	bool CloudMatch(const FGridPoint* Points, const int32* PointsLut, const FTemplate& Template, double MinSoFar, bool bKeepTies, double& OutDistance) const;

	TArray<FTemplate> Templates;
	double RejectDistance = DefaultRejectDistance;
};
