#include "Budget/BudgetStressPlan.h"

namespace
{
// SessionPresentation's colours and body sizes (Session/SessionPresentation.cpp:34-41, :1094-1101).
const FLinearColor ConscriptColour(0.45f, 0.48f, 0.52f);
const FLinearColor SlingerColour(0.55f, 0.28f, 0.05f);
const FLinearColor HoundColour(0.8f, 0.3f, 0.05f);
const FLinearColor MawColour(0.15f, 0.35f, 0.1f);
const FLinearColor WaterColour(0.009721f, 0.745404f, 0.745404f);
const FLinearColor SteelColour(0.68f, 0.68f, 0.68f);

constexpr double ChestM = 1.35;
constexpr double SphereSpeedMps = 12.0;
constexpr double SpearSpeedMps = 9.0;
constexpr double SpearApexM = 0.75;
constexpr int32 PerRing = BudgetStress::EnemyCount / 2;

FBudgetEnemy MakeEnemy(int32 Index)
{
	FBudgetEnemy Enemy;
	switch (Index % 4)
	{
	case 0:
		Enemy.EnemyId = TEXT("conscript");
		Enemy.BodySizeCm = FVector(48.0, 48.0, 176.0);
		Enemy.Colour = ConscriptColour;
		break;
	case 1:
		Enemy.EnemyId = TEXT("slinger");
		Enemy.BodySizeCm = FVector(42.0, 42.0, 150.0);
		Enemy.Colour = SlingerColour;
		break;
	case 2:
		Enemy.EnemyId = TEXT("cinder_hound");
		Enemy.BodySizeCm = FVector(120.0, 40.0, 40.0);
		Enemy.Colour = HoundColour;
		Enemy.bCube = true;
		break;
	default:
		Enemy.EnemyId = TEXT("mire_maw");
		Enemy.BodySizeCm = FVector(150.0, 150.0, 80.0);
		Enemy.Colour = MawColour;
		break;
	}
	return Enemy;
}

FVector OnRing(const FVector& PadM, const FVector& Forward, double DistanceM, double BearingDeg)
{
	// Positive bearing is the player's right (+Y when the pad faces +X), as arena-layout.json spawnArc says.
	const FVector Right(-Forward.Y, Forward.X, 0.0);
	const double Radians = FMath::DegreesToRadians(BearingDeg);
	return FVector(PadM.X, PadM.Y, 0.0) + (Forward * FMath::Cos(Radians) + Right * FMath::Sin(Radians)) * DistanceM;
}

// The widest bearing, at most the arc limit, whose ring point stays on the floor.
double ClippedArcDeg(const FArenaLayout& Layout, const FVector& PadM, const FVector& Forward, double DistanceM)
{
	for (double Bearing = BudgetStress::ArcLimitDeg; Bearing > 0.0; Bearing -= 0.5)
	{
		if (BudgetPointOnFloor(Layout, OnRing(PadM, Forward, DistanceM, Bearing), BudgetStress::EdgeMarginM)
			&& BudgetPointOnFloor(Layout, OnRing(PadM, Forward, DistanceM, -Bearing), BudgetStress::EdgeMarginM))
		{
			return Bearing;
		}
	}
	return 0.0;
}
}

bool BudgetPointOnFloor(const FArenaLayout& Layout, const FVector& PointM, double MarginM)
{
	const double Rx = Layout.FloorRadiusXM - MarginM;
	const double Ry = Layout.FloorRadiusYM - MarginM;
	if (Rx <= 0.0 || Ry <= 0.0)
	{
		return false;
	}
	const double Dx = (PointM.X - Layout.ArenaCentreM.X) / Rx;
	const double Dy = (PointM.Y - Layout.ArenaCentreM.Y) / Ry;
	return Dx * Dx + Dy * Dy <= 1.0;
}

bool BuildBudgetStressPlan(const FArenaLayout& Layout, FBudgetStressPlan& OutPlan, FString& OutError)
{
	OutPlan = FBudgetStressPlan();
	const FRunePad* Pad = Layout.FindPadById(TEXT("centre"));
	if (!Pad)
	{
		OutError = TEXT("layout has no centre pad");
		return false;
	}
	if (Layout.SpawnMarkers.Num() == 0)
	{
		OutError = TEXT("layout has no spawn markers");
		return false;
	}
	double Near = TNumericLimits<double>::Max();
	double Far = 0.0;
	for (const FSpawnMarker& Marker : Layout.SpawnMarkers)
	{
		Near = FMath::Min(Near, Marker.DistanceM);
		Far = FMath::Max(Far, Marker.DistanceM);
	}
	const FVector Forward = Layout.FlatForwardM(*Pad);
	OutPlan.NearDistanceM = Near;
	OutPlan.FarDistanceM = Far;
	OutPlan.NearArcDeg = ClippedArcDeg(Layout, Pad->PositionM, Forward, Near);
	OutPlan.FarArcDeg = ClippedArcDeg(Layout, Pad->PositionM, Forward, Far);
	if (OutPlan.NearArcDeg <= 0.0 || OutPlan.FarArcDeg <= 0.0)
	{
		OutError = FString::Printf(TEXT("no bearing keeps the %.1f m or %.1f m ring on the floor"), Near, Far);
		return false;
	}

	for (int32 Index = 0; Index < BudgetStress::EnemyCount; ++Index)
	{
		const bool bFar = Index >= PerRing;
		const int32 Slot = Index % PerRing;
		const double Arc = bFar ? OutPlan.FarArcDeg : OutPlan.NearArcDeg;
		FBudgetEnemy Enemy = MakeEnemy(Index);
		Enemy.DistanceM = bFar ? Far : Near;
		Enemy.BearingDeg = -Arc + 2.0 * Arc * static_cast<double>(Slot) / static_cast<double>(PerRing - 1);
		Enemy.GroundM = OnRing(Pad->PositionM, Forward, Enemy.DistanceM, Enemy.BearingDeg);
		Enemy.GroundM.Z = Layout.SurfaceHeightM(Enemy.GroundM.X, Enemy.GroundM.Y);
		OutPlan.Enemies.Add(Enemy);
	}

	const FVector Right(-Forward.Y, Forward.X, 0.0);
	const double PadTopM = Pad->PositionM.Z;
	for (int32 Index = 0; Index < BudgetStress::ProjectileCount; ++Index)
	{
		FBudgetShot Shot;
		Shot.EnemyIndex = Index % BudgetStress::EnemyCount;
		Shot.bSpear = (Index % BudgetStress::SpearEvery) == BudgetStress::SpearEvery - 1;
		Shot.Colour = Shot.bSpear ? SteelColour : WaterColour;
		const FBudgetEnemy& Source = OutPlan.Enemies[Shot.EnemyIndex];
		const FVector ToPad = (FVector(Pad->PositionM.X, Pad->PositionM.Y, 0.0) - FVector(Source.GroundM.X, Source.GroundM.Y, 0.0)).GetSafeNormal();
		Shot.StartM = Source.GroundM + ToPad * 0.5;
		Shot.StartM.Z = Source.GroundM.Z + ChestM;
		const double Lateral = (static_cast<double>(Index % 5) - 2.0) * 0.35;
		Shot.EndM = FVector(Pad->PositionM.X, Pad->PositionM.Y, 0.0) + Right * Lateral + Forward * 0.6;
		Shot.EndM.Z = PadTopM + ChestM;
		const double Distance = FVector::Dist(Shot.StartM, Shot.EndM);
		Shot.FlightS = FMath::Max(0.25, Distance / (Shot.bSpear ? SpearSpeedMps : SphereSpeedMps));
		// Golden-ratio phases spread the shots along their lanes, so all 100 are in the air at every moment.
		Shot.Phase = FMath::Frac(static_cast<double>(Index) * 0.6180339887498949);
		OutPlan.Shots.Add(Shot);
	}
	OutPlan.Hands = BudgetStress::HandCount;
	return true;
}

FVector BudgetShotPositionM(const FBudgetShot& Shot, double Seconds, int32* OutLaunches)
{
	const double Flown = Shot.Phase + FMath::Max(0.0, Seconds) / Shot.FlightS;
	const double Progress = FMath::Frac(Flown);
	if (OutLaunches)
	{
		*OutLaunches = FMath::FloorToInt32(Flown);
	}
	FVector At = FMath::Lerp(Shot.StartM, Shot.EndM, Progress);
	if (Shot.bSpear)
	{
		At.Z += 4.0 * SpearApexM * Progress * (1.0 - Progress);
	}
	return At;
}
