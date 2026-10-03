#pragma once

#include "Kernel/SimConstants.h"

#include "CoreMinimal.h"

#include <cmath>
#include <optional>

// 2D arena math. math.ts. Zero-length `toward` is inside the arc; the 3D ward test does not use this.
struct FSimVec
{
	double X = 0.0;
	double Y = 0.0;
};

inline double SimLength(const FSimVec& V)
{
	return std::hypot(V.X, V.Y);
}

inline FSimVec SimSub(const FSimVec& A, const FSimVec& B)
{
	return {A.X - B.X, A.Y - B.Y};
}

inline FSimVec SimAdd(const FSimVec& A, const FSimVec& B)
{
	return {A.X + B.X, A.Y + B.Y};
}

inline FSimVec SimScale(const FSimVec& V, double Scale)
{
	return {V.X * Scale, V.Y * Scale};
}

inline double SimDot(const FSimVec& A, const FSimVec& B)
{
	return A.X * B.X + A.Y * B.Y;
}

inline FSimVec SimUnit(const FSimVec& V, const FSimVec& Fallback = FSimVec{1.0, 0.0})
{
	const double N = SimLength(V);
	return N > 0.0 ? FSimVec{V.X / N, V.Y / N} : Fallback;
}

inline double SimDistance(const FSimVec& A, const FSimVec& B)
{
	return SimLength(SimSub(A, B));
}

inline double SimClamp(double N, double Lo, double Hi)
{
	return std::max(Lo, std::min(Hi, N));
}

inline bool SimInArc(const FSimVec& Facing, const FSimVec& Toward, double ArcDeg)
{
	if (SimLength(Toward) == 0.0 || ArcDeg >= 360.0)
	{
		return true;
	}
	const FSimVec A = SimUnit(Facing);
	const FSimVec B = SimUnit(Toward);
	return SimDot(A, B) >= std::cos(ArcDeg * SimPi / 360.0) - ArcDotEpsilon;
}

inline TOptional<double> SimSegmentHit(const FSimVec& From, const FSimVec& To, const FSimVec& Centre, double Radius)
{
	const FSimVec D = SimSub(To, From);
	const FSimVec F = SimSub(From, Centre);
	const double A = D.X * D.X + D.Y * D.Y;
	const double C = F.X * F.X + F.Y * F.Y - Radius * Radius;
	if (C <= 0.0)
	{
		return 0.0;
	}
	if (A == 0.0)
	{
		return {};
	}
	const double B = 2.0 * (F.X * D.X + F.Y * D.Y);
	const double Disc = B * B - 4.0 * A * C;
	if (Disc < 0.0)
	{
		return {};
	}
	const double T = (-B - std::sqrt(Disc)) / (2.0 * A);
	if (T >= 0.0 && T <= 1.0)
	{
		return T;
	}
	return {};
}

inline FSimVec SimRotate(const FSimVec& V, double Radians)
{
	const double C = std::cos(Radians);
	const double S = std::sin(Radians);
	return {V.X * C - V.Y * S, V.X * S + V.Y * C};
}
