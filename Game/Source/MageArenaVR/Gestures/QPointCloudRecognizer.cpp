#include "Gestures/QPointCloudRecognizer.h"

#include "Algo/Sort.h"

#include <cmath>

namespace
{
constexpr double WeightSum = static_cast<double>(FQPointCloudRecognizer::PointCount) * (FQPointCloudRecognizer::PointCount + 1) / 2.0;

int32 GridCoord(double Value)
{
	const int32 Rounded = static_cast<int32>(FMath::FloorToDouble(Value + 0.5));
	return FMath::Clamp(Rounded, 0, FQPointCloudRecognizer::LutSize - 1);
}

}

FQPointCloudRecognizer::FQPointCloudRecognizer()
{
	RejectDistance = DefaultRejectDistance;
}

void FQPointCloudRecognizer::Reset()
{
	Templates.Reset();
}

void FQPointCloudRecognizer::SetRejectDistance(double InRejectDistance)
{
	RejectDistance = InRejectDistance;
}

int32 FQPointCloudRecognizer::CountTemplates(FName Label) const
{
	int32 Count = 0;
	for (const FTemplate& Template : Templates)
	{
		if (Template.Label == Label)
		{
			++Count;
		}
	}
	return Count;
}

bool FQPointCloudRecognizer::AddTemplate(FName Label, const TArray<FQPoint>& Points)
{
	FGridPoint Grid[PointCount];
	int32 Lut[LutSize * LutSize];
	if (!Normalize(Points, Grid, Lut))
	{
		return false;
	}
	FTemplate& Template = Templates.AddDefaulted_GetRef();
	Template.Label = Label;
	Template.Points.Append(Grid, PointCount);
	Template.Lut.Append(Lut, LutSize * LutSize);
	return true;
}

bool FQPointCloudRecognizer::Normalize(const TArray<FQPoint>& InPoints, FGridPoint* OutPoints, int32* OutLut) const
{
	if (InPoints.Num() == 0 || OutPoints == nullptr || OutLut == nullptr)
	{
		return false;
	}

	double Path = 0.0;
	for (int32 Index = 1; Index < InPoints.Num(); ++Index)
	{
		if (InPoints[Index].StrokeId != InPoints[Index - 1].StrokeId)
		{
			continue;
		}
		Path += std::hypot(InPoints[Index].X - InPoints[Index - 1].X, InPoints[Index].Y - InPoints[Index - 1].Y);
	}

	TArray<FQPoint> Working;
	Working.Reserve(InPoints.Num() + PointCount);
	Working.Append(InPoints);
	TArray<FQPoint> Resampled;
	Resampled.Reserve(PointCount);
	if (Path < 1.0e-8)
	{
		for (int32 Index = 0; Index < PointCount; ++Index)
		{
			Resampled.Add(InPoints[0]);
		}
	}
	else
	{
		const double Interval = Path / static_cast<double>(PointCount - 1);
		Resampled.Add(Working[0]);
		double Accumulated = 0.0;
		for (int32 Index = 1; Index < Working.Num() && Resampled.Num() < PointCount; ++Index)
		{
			if (Working[Index].StrokeId != Working[Index - 1].StrokeId)
			{
				continue;
			}
			const double DX = Working[Index].X - Working[Index - 1].X;
			const double DY = Working[Index].Y - Working[Index - 1].Y;
			const double Dist = std::hypot(DX, DY);
			if (Accumulated + Dist >= Interval && Dist > 1.0e-12)
			{
				const double T = (Interval - Accumulated) / Dist;
				FQPoint Inserted;
				Inserted.X = Working[Index - 1].X + T * DX;
				Inserted.Y = Working[Index - 1].Y + T * DY;
				Inserted.StrokeId = Working[Index].StrokeId;
				Resampled.Add(Inserted);
				Working.Insert(Inserted, Index);
				Accumulated = 0.0;
			}
			else
			{
				Accumulated += Dist;
			}
		}
		if (Resampled.Num() == PointCount - 1)
		{
			Resampled.Add(Working.Last());
		}
		while (Resampled.Num() < PointCount)
		{
			Resampled.Add(Resampled.Last());
		}
		if (Resampled.Num() > PointCount)
		{
			Resampled.SetNum(PointCount);
		}
	}

	double CentroidX = 0.0;
	double CentroidY = 0.0;
	for (const FQPoint& Point : Resampled)
	{
		CentroidX += Point.X;
		CentroidY += Point.Y;
	}
	CentroidX /= static_cast<double>(PointCount);
	CentroidY /= static_cast<double>(PointCount);

	double MinX = TNumericLimits<double>::Max();
	double MinY = TNumericLimits<double>::Max();
	double MaxX = TNumericLimits<double>::Lowest();
	double MaxY = TNumericLimits<double>::Lowest();
	for (FQPoint& Point : Resampled)
	{
		Point.X -= CentroidX;
		Point.Y -= CentroidY;
		MinX = FMath::Min(MinX, Point.X);
		MinY = FMath::Min(MinY, Point.Y);
		MaxX = FMath::Max(MaxX, Point.X);
		MaxY = FMath::Max(MaxY, Point.Y);
	}
	const double Span = FMath::Max(MaxX - MinX, MaxY - MinY);
	if (Span < 1.0e-8)
	{
		return false;
	}
	const double Scale = Span / static_cast<double>(LutSize - 1);

	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		OutPoints[Index].X = GridCoord((Resampled[Index].X - MinX) / Scale);
		OutPoints[Index].Y = GridCoord((Resampled[Index].Y - MinY) / Scale);
	}

	// COMPUTE-LUT. Strict < keeps the earlier point on a tie, matching a scan of index 0..n-1.
	constexpr int32 Cells = LutSize * LutSize;
	int32 BestDistance[Cells];
	for (int32 Cell = 0; Cell < Cells; ++Cell)
	{
		BestDistance[Cell] = MAX_int32;
	}
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		const int32 PX = OutPoints[Index].X;
		const int32 PY = OutPoints[Index].Y;
		for (int32 X = 0; X < LutSize; ++X)
		{
			const int32 DX = PX - X;
			const int32 DX2 = DX * DX;
			int32* LutRow = OutLut + X * LutSize;
			int32* BestRow = BestDistance + X * LutSize;
			for (int32 Y = 0; Y < LutSize; ++Y)
			{
				const int32 DY = PY - Y;
				const int32 Distance = DX2 + DY * DY;
				if (Distance < BestRow[Y])
				{
					BestRow[Y] = Distance;
					LutRow[Y] = Index;
				}
			}
		}
	}
	return true;
}

void FQPointCloudRecognizer::FillLowerBound(const FGridPoint* From, const FGridPoint* To, const int32* ToLut, double* Bound) const
{
	constexpr int32 Count = PointCount;
	const int32 Step = FMath::FloorToInt(FMath::Sqrt(static_cast<double>(Count)));
	double Sat[PointCount];
	Bound[0] = 0.0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FGridPoint Point = From[Index];
		const int32 Near = ToLut[Point.X * LutSize + Point.Y];
		const double Distance = static_cast<double>(Squared(Point, To[Near]));
		Sat[Index] = Index == 0 ? Distance : Sat[Index - 1] + Distance;
		Bound[0] += static_cast<double>(Count - Index) * Distance;
	}
	int32 Slot = 1;
	for (int32 Index = Step; Index < Count; Index += Step, ++Slot)
	{
		Bound[Slot] = Bound[0] + static_cast<double>(Index) * Sat[Count - 1] - static_cast<double>(Count) * Sat[Index - 1];
	}
}

double FQPointCloudRecognizer::MinimumLowerBound(const FGridPoint* Points, const int32* PointsLut, const FTemplate& Template) const
{
	if (Template.Points.Num() != PointCount || Template.Lut.Num() != LutSize * LutSize)
	{
		return TNumericLimits<double>::Max();
	}
	const int32 Step = FMath::FloorToInt(FMath::Sqrt(static_cast<double>(PointCount)));
	double BoundForward[PointCount];
	double BoundReverse[PointCount];
	const FGridPoint* TemplatePoints = Template.Points.GetData();
	FillLowerBound(Points, TemplatePoints, Template.Lut.GetData(), BoundForward);
	FillLowerBound(TemplatePoints, Points, PointsLut, BoundReverse);
	double MinBound = BoundForward[0];
	int32 Slot = 0;
	for (int32 Index = 0; Index < PointCount; Index += Step, ++Slot)
	{
		MinBound = FMath::Min(MinBound, BoundForward[Slot]);
		MinBound = FMath::Min(MinBound, BoundReverse[Slot]);
	}
	return MinBound;
}

bool FQPointCloudRecognizer::CloudMatch(
	const FGridPoint* Points,
	const int32* PointsLut,
	const FTemplate& Template,
	double MinSoFar,
	bool bKeepTies,
	double& OutDistance) const
{
	constexpr int32 Count = PointCount;
	const int32 Step = FMath::FloorToInt(FMath::Sqrt(static_cast<double>(Count)));
	if (Points == nullptr || PointsLut == nullptr || Step <= 0 || Template.Points.Num() != Count || Template.Lut.Num() != LutSize * LutSize)
	{
		return false;
	}
	const FGridPoint* TemplatePoints = Template.Points.GetData();
	const int32* TemplateLut = Template.Lut.GetData();

	double BoundForward[PointCount];
	double BoundReverse[PointCount];
	FillLowerBound(Points, TemplatePoints, TemplateLut, BoundForward);
	FillLowerBound(TemplatePoints, Points, PointsLut, BoundReverse);

	// Completed sum, or false when early-abandon proves the alignment cannot win.
	auto CloudDistance = [&](const FGridPoint* From, const FGridPoint* To, int32 Start, double AbandonAt, bool bAbandonOnEqual, double& OutSum) -> bool
	{
		int32 Unmatched[PointCount];
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Unmatched[Index] = Index;
		}
		int64 Sum = 0;
		int32 Cursor = Start;
		int32 Weight = Count;
		int32 Matched = 0;
		do
		{
			int32 BestSlot = Matched;
			int64 BestDistance = MAX_int64;
			const FGridPoint FromPoint = From[Cursor];
			for (int32 Slot = Matched; Slot < Count; ++Slot)
			{
				const int64 Distance = Squared(FromPoint, To[Unmatched[Slot]]);
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					BestSlot = Slot;
				}
			}
			Unmatched[BestSlot] = Unmatched[Matched];
			Sum += static_cast<int64>(Weight) * BestDistance;
			--Weight;
			const double SumReal = static_cast<double>(Sum);
			if (bAbandonOnEqual ? (SumReal >= AbandonAt) : (SumReal > AbandonAt))
			{
				return false;
			}
			Cursor = (Cursor + 1) % Count;
			++Matched;
		}
		while (Cursor != Start);
		OutSum = static_cast<double>(Sum);
		return true;
	};

	double Threshold = MinSoFar;
	double BestCompleted = 0.0;
	bool bFound = false;
	int32 Slot = 0;
	for (int32 Index = 0; Index < Count; Index += Step, ++Slot)
	{
		auto Run = [&](double Bound, const FGridPoint* From, const FGridPoint* To, int32 Start)
		{
			const bool bTieProbe = bKeepTies && Bound == MinSoFar;
			if (!(Bound < Threshold || bTieProbe))
			{
				return;
			}
			const bool bAbandonOnEqual = !(bKeepTies && Threshold == MinSoFar);
			double Sum = 0.0;
			if (!CloudDistance(From, To, Start, Threshold, bAbandonOnEqual, Sum))
			{
				return;
			}
			if (!bFound || Sum < BestCompleted)
			{
				BestCompleted = Sum;
				bFound = true;
			}
			if (Sum < Threshold)
			{
				Threshold = Sum;
			}
		};
		Run(BoundForward[Slot], Points, TemplatePoints, Index);
		Run(BoundReverse[Slot], TemplatePoints, Points, Index);
	}

	if (!bFound)
	{
		return false;
	}
	if (BestCompleted < MinSoFar || (bKeepTies && BestCompleted == MinSoFar))
	{
		OutDistance = BestCompleted;
		return true;
	}
	return false;
}

FQClassifyResult FQPointCloudRecognizer::Classify(const TArray<FQPoint>& Points) const
{
	const double Started = FPlatformTime::Seconds();
	FQClassifyResult Result;
	FGridPoint Grid[PointCount];
	int32 Lut[LutSize * LutSize];
	if (!Normalize(Points, Grid, Lut) || Templates.Num() == 0)
	{
		Result.Milliseconds = (FPlatformTime::Seconds() - Started) * 1000.0;
		return Result;
	}

	// Same minimum as a forward scan. Trying the tighter lower bound first lets later
	// alignments abandon sooner. An equal distance still keeps the lower original index,
	// which is what a forward scan does when it replaces only on a strictly smaller distance.
	struct FOrder
	{
		double Bound = 0.0;
		int32 Index = 0;
	};
	const FTemplate* TemplateData = Templates.GetData();
	const int32 TemplateCount = Templates.Num();
	TArray<FOrder, TInlineAllocator<128>> Order;
	Order.Reserve(TemplateCount);
	for (int32 Index = 0; Index < TemplateCount; ++Index)
	{
		FOrder& Item = Order.AddDefaulted_GetRef();
		Item.Index = Index;
		Item.Bound = MinimumLowerBound(Grid, Lut, TemplateData[Index]);
	}
	Algo::Sort(Order, [](const FOrder& A, const FOrder& B)
	{
		if (A.Bound < B.Bound)
		{
			return true;
		}
		if (B.Bound < A.Bound)
		{
			return false;
		}
		return A.Index < B.Index;
	});

	double Best = TNumericLimits<double>::Max();
	int32 BestIndex = MAX_int32;
	FName BestLabel;
	for (const FOrder& Item : Order)
	{
		if (Item.Bound > Best || (Item.Bound == Best && Item.Index > BestIndex))
		{
			continue;
		}
		double Distance = 0.0;
		const bool bKeepTies = Item.Index < BestIndex;
		if (!CloudMatch(Grid, Lut, TemplateData[Item.Index], Best, bKeepTies, Distance))
		{
			continue;
		}
		if (Distance < Best || (Distance == Best && Item.Index < BestIndex))
		{
			Best = Distance;
			BestIndex = Item.Index;
			BestLabel = TemplateData[Item.Index].Label;
		}
	}

	Result.Label = BestLabel;
	Result.Distance = Best;
	Result.Score = 1.0f / (1.0f + static_cast<float>(Best / WeightSum));
	Result.bReject = Best > RejectDistance;
	Result.Milliseconds = (FPlatformTime::Seconds() - Started) * 1000.0;
	return Result;
}
