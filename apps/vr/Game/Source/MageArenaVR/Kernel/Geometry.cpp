#include "Kernel/Geometry.h"

#include "Kernel/KernelData.h"

#include <cmath>

namespace
{
FSimVec Axes(double Inset)
{
	const FKernelData& Data = KernelData();
	const double Rx = Data.ArenaWidthM * 0.5;
	const double Ry = Data.ArenaHeightM * 0.5;
	const double Factor = std::max(ArenaInsetMinFactor, 1.0 - Inset / std::min(Rx, Ry));
	return {Rx * Factor, Ry * Factor};
}
}

bool ArenaContains(const FSimVec& Point, double Inset)
{
	const FSimVec Radius = Axes(Inset);
	const FSimVec Centre = KernelData().ArenaCentre;
	return std::hypot((Point.X - Centre.X) / Radius.X, (Point.Y - Centre.Y) / Radius.Y) <= 1.0 + ArenaContainsEpsilon;
}

FSimVec ConstrainToArena(const FSimVec& Point, double Inset)
{
	const FSimVec Radius = Axes(Inset);
	const FSimVec Centre = KernelData().ArenaCentre;
	const double Dx = Point.X - Centre.X;
	const double Dy = Point.Y - Centre.Y;
	const double Ratio = std::hypot(Dx / Radius.X, Dy / Radius.Y);
	if (Ratio <= 1.0)
	{
		return Point;
	}
	return {Centre.X + Dx / Ratio, Centre.Y + Dy / Ratio};
}

FSimVec OpeningPosition(int32 Index, int32 Count, const FSimVec& Jitter)
{
	const FKernelData& Data = KernelData();
	const double Rows = std::ceil(std::sqrt(static_cast<double>(Count)));
	const double Spacing = Data.OpeningSeparationM + 2.0 * Data.SpawnJitterM;
	const FSimVec Point{
		Data.EnemySpawn.X + std::floor(static_cast<double>(Index) / Rows) * Spacing + Jitter.X,
		Data.EnemySpawn.Y + (std::fmod(static_cast<double>(Index), Rows) - (Rows - 1.0) / 2.0) * Spacing + Jitter.Y};
	return ConstrainToArena(Point, Data.SpawnMarginM);
}
