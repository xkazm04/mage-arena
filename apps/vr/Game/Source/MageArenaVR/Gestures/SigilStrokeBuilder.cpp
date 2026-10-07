#include "Gestures/SigilStrokeBuilder.h"

#include "Hands/HandClip.h"
#include "HeadMountedDisplayTypes.h"

#include <cmath>

namespace
{
struct FDrawPlane
{
	FVector Origin = FVector::ZeroVector;
	FVector AxisX = FVector::ForwardVector;
	FVector AxisY = FVector::UpVector;
	bool bValid = false;
};

void JacobiRotate(double A[3][3], double V[3][3], int32 P, int32 Q)
{
	const double Apq = A[P][Q];
	if (FMath::Abs(Apq) < 1.0e-18)
	{
		return;
	}
	const double App = A[P][P];
	const double Aqq = A[Q][Q];
	const double Tau = (Aqq - App) / (2.0 * Apq);
	const double T = (Tau >= 0.0 ? 1.0 : -1.0) / (FMath::Abs(Tau) + FMath::Sqrt(1.0 + Tau * Tau));
	const double C = 1.0 / FMath::Sqrt(1.0 + T * T);
	const double S = T * C;
	A[P][P] = App - T * Apq;
	A[Q][Q] = Aqq + T * Apq;
	A[P][Q] = 0.0;
	A[Q][P] = 0.0;
	for (int32 R = 0; R < 3; ++R)
	{
		if (R == P || R == Q)
		{
			continue;
		}
		const double Arp = A[R][P];
		const double Arq = A[R][Q];
		A[R][P] = A[P][R] = C * Arp - S * Arq;
		A[R][Q] = A[Q][R] = S * Arp + C * Arq;
	}
	for (int32 R = 0; R < 3; ++R)
	{
		const double Vrp = V[R][P];
		const double Vrq = V[R][Q];
		V[R][P] = C * Vrp - S * Vrq;
		V[R][Q] = S * Vrp + C * Vrq;
	}
}

/** How far Points[From..] reach, whatever path they took: the diagonal of their bounding box. 0 when empty. */
double BoxDiagonal(const TArray<FQPoint>& Points, int32 From)
{
	double MinX = TNumericLimits<double>::Max();
	double MinY = TNumericLimits<double>::Max();
	double MaxX = TNumericLimits<double>::Lowest();
	double MaxY = TNumericLimits<double>::Lowest();
	for (int32 Index = FMath::Max(0, From); Index < Points.Num(); ++Index)
	{
		MinX = FMath::Min(MinX, Points[Index].X);
		MaxX = FMath::Max(MaxX, Points[Index].X);
		MinY = FMath::Min(MinY, Points[Index].Y);
		MaxY = FMath::Max(MaxY, Points[Index].Y);
	}
	return MaxX >= MinX ? std::hypot(MaxX - MinX, MaxY - MinY) : 0.0;
}

FDrawPlane FitPlane(const TArray<FVector>& Points)
{
	FDrawPlane Plane;
	if (Points.Num() < 2)
	{
		return Plane;
	}
	FVector Centroid = FVector::ZeroVector;
	for (const FVector& Point : Points)
	{
		Centroid += Point;
	}
	Centroid /= static_cast<double>(Points.Num());

	double A[3][3] = {};
	for (const FVector& Point : Points)
	{
		const FVector D = Point - Centroid;
		A[0][0] += D.X * D.X;
		A[0][1] += D.X * D.Y;
		A[0][2] += D.X * D.Z;
		A[1][1] += D.Y * D.Y;
		A[1][2] += D.Y * D.Z;
		A[2][2] += D.Z * D.Z;
	}
	A[1][0] = A[0][1];
	A[2][0] = A[0][2];
	A[2][1] = A[1][2];

	double V[3][3] = {
		{1.0, 0.0, 0.0},
		{0.0, 1.0, 0.0},
		{0.0, 0.0, 1.0},
	};
	for (int32 Iteration = 0; Iteration < 16; ++Iteration)
	{
		JacobiRotate(A, V, 0, 1);
		JacobiRotate(A, V, 0, 2);
		JacobiRotate(A, V, 1, 2);
	}

	int32 Smallest = 0;
	if (A[1][1] < A[Smallest][Smallest])
	{
		Smallest = 1;
	}
	if (A[2][2] < A[Smallest][Smallest])
	{
		Smallest = 2;
	}
	FVector Normal(V[0][Smallest], V[1][Smallest], V[2][Smallest]);
	if (Normal.IsNearlyZero())
	{
		return Plane;
	}
	Normal.Normalize();
	// Seated origin looks along +X. Point the plane normal along that view so the in-plane
	// right axis is the player's right. The sign of whichever covariance axis is largest
	// flips a tilted drawing.
	const FVector ViewDirection(1.0, 0.0, 0.0);
	if (FVector::DotProduct(Normal, ViewDirection) < 0.0)
	{
		Normal *= -1.0;
	}

	// World up projected onto the plane. The view-aligned normal keeps the axes from mirroring.
	const FVector WorldUp(0.0, 0.0, 1.0);
	FVector AxisY = WorldUp - Normal * FVector::DotProduct(WorldUp, Normal);
	if (AxisY.SizeSquared() < 1.0e-16)
	{
		const FVector WorldY(0.0, 1.0, 0.0);
		AxisY = WorldY - Normal * FVector::DotProduct(WorldY, Normal);
	}
	if (AxisY.SizeSquared() < 1.0e-16)
	{
		return Plane;
	}
	AxisY.Normalize();
	FVector AxisX = FVector::CrossProduct(AxisY, Normal);
	if (AxisX.SizeSquared() < 1.0e-16)
	{
		return Plane;
	}
	AxisX.Normalize();

	Plane.Origin = Centroid;
	Plane.AxisX = AxisX;
	Plane.AxisY = AxisY;
	Plane.bValid = true;
	return Plane;
}
}

void FSigilStrokeBuilder::Reset()
{
	Strokes.Reset();
	bPen = false;
	bHasSample = false;
	LastSampleTime = 0.0;
	PenUpTime = 0.0;
	bHasMotionSample = false;
	bInMotion = false;
	MotionTip = FVector::ZeroVector;
	MotionTime = 0.0;
	MotionStill = 0.0;
}

void FSigilStrokeBuilder::SetStyle(ESigilDrawStyle InStyle)
{
	Reset();
	Style = InStyle;
}

float FSigilStrokeBuilder::SynthesizePinch(const FVector& TipCm, double TimeSeconds)
{
	if (!bHasMotionSample)
	{
		bHasMotionSample = true;
		MotionTip = TipCm;
		MotionTime = TimeSeconds;
		bInMotion = false;
		MotionStill = 0.0;
		return 0.0f;
	}
	const double Dt = FMath::Clamp(TimeSeconds - MotionTime, 0.0, 0.1);
	const double Distance = FVector::Distance(TipCm, MotionTip);
	const double Speed = Dt > 1.0e-4 ? Distance / Dt : 0.0;
	MotionTip = TipCm;
	MotionTime = TimeSeconds;
	if (!bInMotion)
	{
		if (Speed >= StillSpeedCmPerSec)
		{
			bInMotion = true;
			MotionStill = 0.0;
			return 1.0f;
		}
		return 0.0f;
	}
	if (Speed < StillSpeedCmPerSec)
	{
		MotionStill += Dt;
		if (MotionStill >= StillGapSeconds)
		{
			bInMotion = false;
			MotionStill = 0.0;
			return 0.0f;
		}
	}
	else
	{
		MotionStill = 0.0;
	}
	return 1.0f;
}

ESigilStrokeEvent FSigilStrokeBuilder::AddTip(const FVector& TipCm, float Pinch, double TimeSeconds, TArray<FQPoint>& OutPoints)
{
	const float UsePinch = Style == ESigilDrawStyle::StyleB ? SynthesizePinch(TipCm, TimeSeconds) : Pinch;
	return AddPinch(TipCm, UsePinch, TimeSeconds, OutPoints);
}

ESigilStrokeEvent FSigilStrokeBuilder::AddPinch(const FVector& TipCm, float Pinch, double TimeSeconds, TArray<FQPoint>& OutPoints)
{
	const double Dt = bHasSample ? FMath::Clamp(TimeSeconds - LastSampleTime, 0.0, 0.1) : 0.0;
	bHasSample = true;
	LastSampleTime = TimeSeconds;

	const bool bWasDown = bPen;
	if (!bPen)
	{
		if (Pinch >= PinchDown)
		{
			bPen = true;
		}
	}
	else if (Pinch < PinchUp)
	{
		bPen = false;
	}

	if (bPen)
	{
		PenUpTime = 0.0;
		if (!bWasDown)
		{
			Strokes.AddDefaulted();
		}
		Strokes.Last().Add(TipCm);
		return ESigilStrokeEvent::None;
	}
	if (bWasDown)
	{
		return Decide(OutPoints);
	}
	if (Strokes.Num() > 0)
	{
		PenUpTime += Dt;
		if (PenUpTime >= InnerWaitSeconds)
		{
			TArray<TArray<FQPoint>> Flat;
			if (Project(Flat))
			{
				WritePoints(Flat, OutPoints);
			}
			Strokes.Reset();
			PenUpTime = 0.0;
			return ESigilStrokeEvent::Rejected;
		}
	}
	return ESigilStrokeEvent::None;
}

ESigilStrokeEvent FSigilStrokeBuilder::Flush(TArray<FQPoint>& OutPoints)
{
	bInMotion = false;
	if (!bPen)
	{
		return ESigilStrokeEvent::None;
	}
	bPen = false;
	return Decide(OutPoints);
}

FSigilStrokeBuilder::FStrokeStats FSigilStrokeBuilder::StatsOf(const TArray<FQPoint>& Points)
{
	FStrokeStats Stats;
	if (Points.Num() < 2)
	{
		return Stats;
	}
	double MinX = TNumericLimits<double>::Max();
	double MinY = TNumericLimits<double>::Max();
	double MaxX = TNumericLimits<double>::Lowest();
	double MaxY = TNumericLimits<double>::Lowest();
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		MinX = FMath::Min(MinX, Points[Index].X);
		MaxX = FMath::Max(MaxX, Points[Index].X);
		MinY = FMath::Min(MinY, Points[Index].Y);
		MaxY = FMath::Max(MaxY, Points[Index].Y);
		if (Index > 0)
		{
			Stats.Path += std::hypot(Points[Index].X - Points[Index - 1].X, Points[Index].Y - Points[Index - 1].Y);
		}
	}
	const double Diagonal = std::hypot(MaxX - MinX, MaxY - MinY);
	const double StartEnd = std::hypot(Points[0].X - Points.Last().X, Points[0].Y - Points.Last().Y);
	Stats.Ratio = Diagonal > 1.0e-6 ? StartEnd / Diagonal : TNumericLimits<double>::Max();

	double Net = 0.0;
	double Peak = 0.0;
	bool bHaveDir = false;
	FVector2D PreviousDir = FVector2D::ZeroVector;
	FVector2D Anchor(Points[0].X, Points[0].Y);
	int32 Closure = INDEX_NONE;
	int32 TurnClosure = INDEX_NONE;
	double ClosureDiagonal = 0.0;
	double PrefixMinX = TNumericLimits<double>::Max();
	double PrefixMinY = TNumericLimits<double>::Max();
	double PrefixMaxX = TNumericLimits<double>::Lowest();
	double PrefixMaxY = TNumericLimits<double>::Lowest();
	double PrefixPath = 0.0;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		PrefixMinX = FMath::Min(PrefixMinX, Points[Index].X);
		PrefixMaxX = FMath::Max(PrefixMaxX, Points[Index].X);
		PrefixMinY = FMath::Min(PrefixMinY, Points[Index].Y);
		PrefixMaxY = FMath::Max(PrefixMaxY, Points[Index].Y);
		if (Index > 0)
		{
			PrefixPath += std::hypot(Points[Index].X - Points[Index - 1].X, Points[Index].Y - Points[Index - 1].Y);
		}
		if (Index > 0)
		{
			const double DX = Points[Index].X - Anchor.X;
			const double DY = Points[Index].Y - Anchor.Y;
			if (DX * DX + DY * DY >= MinSegmentCm * MinSegmentCm)
			{
				const double Length = std::hypot(DX, DY);
				const FVector2D Dir(DX / Length, DY / Length);
				if (bHaveDir)
				{
					Net += FMath::Atan2(PreviousDir.X * Dir.Y - PreviousDir.Y * Dir.X, PreviousDir.X * Dir.X + PreviousDir.Y * Dir.Y);
					Peak = FMath::Max(Peak, FMath::Abs(Net));
				}
				PreviousDir = Dir;
				bHaveDir = true;
				Anchor = FVector2D(Points[Index].X, Points[Index].Y);
			}
		}
		const double PrefixDiagonal = std::hypot(PrefixMaxX - PrefixMinX, PrefixMaxY - PrefixMinY);
		const double PrefixStartEnd = std::hypot(Points[0].X - Points[Index].X, Points[0].Y - Points[Index].Y);
		const double PrefixRatio = PrefixDiagonal > 1.0e-6 ? PrefixStartEnd / PrefixDiagonal : TNumericLimits<double>::Max();
		const double PeakDegrees = Peak * 180.0 / PI;
		if (TurnClosure == INDEX_NONE && PrefixPath >= MinLoopPathCm && PeakDegrees >= LoopTurnDegrees)
		{
			TurnClosure = Index;
		}
		if (Closure == INDEX_NONE && PrefixPath >= MinLoopPathCm && PrefixRatio <= ClosedStartEndRatio)
		{
			Closure = Index;
			ClosureDiagonal = PrefixDiagonal;
		}
	}
	// The inner stroke can unwind the final net turn. The loop still happened if the peak crossed 300.
	Stats.Turn = Peak * 180.0 / PI;
	Stats.bClosed = Stats.Path >= MinLoopPathCm && Stats.Ratio <= ClosedStartEndRatio;

	const int32 Used = Closure != INDEX_NONE ? Closure : TurnClosure;
	if (Used != INDEX_NONE)
	{
		Stats.bLoopClosed = true;
		if (Closure != INDEX_NONE)
		{
			Stats.LoopDiagonal = ClosureDiagonal;
		}
		else
		{
			double LoopMinX = TNumericLimits<double>::Max();
			double LoopMinY = TNumericLimits<double>::Max();
			double LoopMaxX = TNumericLimits<double>::Lowest();
			double LoopMaxY = TNumericLimits<double>::Lowest();
			for (int32 Index = 0; Index <= Used; ++Index)
			{
				LoopMinX = FMath::Min(LoopMinX, Points[Index].X);
				LoopMaxX = FMath::Max(LoopMaxX, Points[Index].X);
				LoopMinY = FMath::Min(LoopMinY, Points[Index].Y);
				LoopMaxY = FMath::Max(LoopMaxY, Points[Index].Y);
			}
			Stats.LoopDiagonal = std::hypot(LoopMaxX - LoopMinX, LoopMaxY - LoopMinY);
		}
		Stats.TailDiagonal = BoxDiagonal(Points, Used);
	}

	// The loop does not have to start at the first sample. Style B includes the
	// reach from rest, so the circle closes on a later point, not on the stroke start.
	TArray<double> Along;
	Along.SetNum(Points.Num());
	Along[0] = 0.0;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		Along[Index] = Along[Index - 1] + std::hypot(Points[Index].X - Points[Index - 1].X, Points[Index].Y - Points[Index - 1].Y);
	}
	int32 SubEnd = INDEX_NONE;
	double SubDiag = 0.0;
	double SubRatio = TNumericLimits<double>::Max();
	for (int32 Start = 0; Start < Points.Num(); ++Start)
	{
		double BoxMinX = Points[Start].X;
		double BoxMaxX = Points[Start].X;
		double BoxMinY = Points[Start].Y;
		double BoxMaxY = Points[Start].Y;
		for (int32 Index = Start + 1; Index < Points.Num(); ++Index)
		{
			BoxMinX = FMath::Min(BoxMinX, Points[Index].X);
			BoxMaxX = FMath::Max(BoxMaxX, Points[Index].X);
			BoxMinY = FMath::Min(BoxMinY, Points[Index].Y);
			BoxMaxY = FMath::Max(BoxMaxY, Points[Index].Y);
			const double Span = Along[Index] - Along[Start];
			if (Span < MinLoopPathCm)
			{
				continue;
			}
			const double Diag = std::hypot(BoxMaxX - BoxMinX, BoxMaxY - BoxMinY);
			const double PairEnd = std::hypot(Points[Index].X - Points[Start].X, Points[Index].Y - Points[Start].Y);
			const double Ratio = Diag > 1.0e-6 ? PairEnd / Diag : TNumericLimits<double>::Max();
			if (Ratio <= ClosedStartEndRatio && (SubEnd == INDEX_NONE || Index < SubEnd || (Index == SubEnd && Ratio < SubRatio)))
			{
				SubEnd = Index;
				SubDiag = Diag;
				SubRatio = Ratio;
			}
		}
	}
	if (SubEnd != INDEX_NONE)
	{
		Stats.bLoopClosed = true;
		Stats.LoopDiagonal = SubDiag;
		Stats.TailDiagonal = BoxDiagonal(Points, SubEnd);
	}
	return Stats;
}

bool FSigilStrokeBuilder::HasInnerStroke(const FStrokeStats& Stats)
{
	return Stats.bLoopClosed && Stats.LoopDiagonal > 1.0 && Stats.TailDiagonal >= InnerTailRatio * Stats.LoopDiagonal;
}

bool FSigilStrokeBuilder::HasLoop(const TArray<TArray<FQPoint>>& Flat)
{
	if (Flat.Num() == 0 || Flat[0].Num() == 0)
	{
		return false;
	}
	const FStrokeStats First = StatsOf(Flat[0]);
	// bLoopClosed is a loop that returns to a later sample. The whole stroke's
	// start is the reach-in, so it does not itself close, and the running turn
	// can stay just under 300.
	if (First.bLoopClosed || (First.Path >= MinLoopPathCm && First.Turn >= LoopTurnDegrees))
	{
		return true;
	}
	for (const TArray<FQPoint>& Stroke : Flat)
	{
		if (StatsOf(Stroke).bClosed)
		{
			return true;
		}
	}
	return false;
}

bool FSigilStrokeBuilder::IsReady(const TArray<TArray<FQPoint>>& Flat)
{
	if (!HasLoop(Flat))
	{
		return false;
	}
	if (Flat.Num() >= 2)
	{
		const FStrokeStats First = StatsOf(Flat[0]);
		if (First.LoopDiagonal <= 1.0)
		{
			return false;
		}
		TArray<FQPoint> Later;
		for (int32 Stroke = 1; Stroke < Flat.Num(); ++Stroke)
		{
			Later.Append(Flat[Stroke]);
		}
		return BoxDiagonal(Later, 0) >= MinInnerStrokeRatio * First.LoopDiagonal;
	}
	return HasInnerStroke(StatsOf(Flat[0]));
}

void FSigilStrokeBuilder::WritePoints(const TArray<TArray<FQPoint>>& Flat, TArray<FQPoint>& OutPoints)
{
	OutPoints.Reset();
	for (int32 StrokeIndex = 0; StrokeIndex < Flat.Num(); ++StrokeIndex)
	{
		for (const FQPoint& Source : Flat[StrokeIndex])
		{
			FQPoint Point = Source;
			Point.StrokeId = StrokeIndex;
			OutPoints.Add(Point);
		}
	}
}

bool FSigilStrokeBuilder::Project(TArray<TArray<FQPoint>>& OutStrokes) const
{
	TArray<FVector> All;
	for (const TArray<FVector>& Stroke : Strokes)
	{
		All.Append(Stroke);
	}
	const FDrawPlane Plane = FitPlane(All);
	if (!Plane.bValid)
	{
		return false;
	}
	OutStrokes.Reset();
	OutStrokes.Reserve(Strokes.Num());
	for (const TArray<FVector>& Stroke : Strokes)
	{
		TArray<FQPoint>& Flat = OutStrokes.AddDefaulted_GetRef();
		Flat.Reserve(Stroke.Num());
		for (const FVector& Point : Stroke)
		{
			const FVector Delta = Point - Plane.Origin;
			FQPoint Sample;
			Sample.X = FVector::DotProduct(Delta, Plane.AxisX);
			Sample.Y = FVector::DotProduct(Delta, Plane.AxisY);
			Flat.Add(Sample);
		}
	}
	return true;
}

ESigilStrokeEvent FSigilStrokeBuilder::Decide(TArray<FQPoint>& OutPoints)
{
	if (Strokes.Num() == 0 || Strokes.Last().Num() < 2)
	{
		if (Strokes.Num() > 0 && Strokes.Last().Num() < 2)
		{
			Strokes.RemoveAt(Strokes.Num() - 1);
		}
		return ESigilStrokeEvent::None;
	}

	TArray<TArray<FQPoint>> Flat;
	if (!Project(Flat))
	{
		Strokes.Reset();
		PenUpTime = 0.0;
		return ESigilStrokeEvent::Rejected;
	}
	if (IsReady(Flat))
	{
		WritePoints(Flat, OutPoints);
		Strokes.Reset();
		PenUpTime = 0.0;
		return ESigilStrokeEvent::Gesture;
	}
	if (!HasLoop(Flat))
	{
		WritePoints(Flat, OutPoints);
		Strokes.Reset();
		PenUpTime = 0.0;
		return ESigilStrokeEvent::Rejected;
	}
	// The circle closed and a later stroke was drawn, but it is too short to be a spell line.
	if (Flat.Num() >= 2)
	{
		WritePoints(Flat, OutPoints);
		Strokes.Reset();
		PenUpTime = 0.0;
		return ESigilStrokeEvent::Rejected;
	}
	PenUpTime = 0.0;
	return ESigilStrokeEvent::None;
}

bool ExtractFirstGesture(const FHandClip& Clip, ESigilDrawStyle Style, TArray<FQPoint>& OutPoints, EControllerHand Hand)
{
	FSigilStrokeBuilder Builder;
	Builder.SetStyle(Style);
	const int32 TipIndex = static_cast<int32>(EHandKeypoint::IndexTip);
	for (const FHandClipTrack& Track : Clip.GetTracks())
	{
		if (Track.Hand != Hand)
		{
			continue;
		}
		for (const FHandFrame& Frame : Track.Frames)
		{
			if (!Frame.Joints.IsValidIndex(TipIndex))
			{
				continue;
			}
			TArray<FQPoint> Points;
			const ESigilStrokeEvent Event = Builder.AddTip(Frame.Joints[TipIndex].Location, Frame.Pinch, Frame.TimeSeconds, Points);
			if (Event == ESigilStrokeEvent::Gesture)
			{
				OutPoints = MoveTemp(Points);
				return true;
			}
		}
	}
	return Builder.Flush(OutPoints) == ESigilStrokeEvent::Gesture;
}
