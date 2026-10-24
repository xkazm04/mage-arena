#include "Budget/BudgetStressPlan.h"

#include "Budget/SceneCensus.h"
#include "Kernel/KernelData.h"
#include "Kernel/VrRules.h"
#include "Session/ElementColour.h"
#include "Session/PresentationLook.h"

namespace
{
constexpr double ChestM = 1.35;
constexpr double SphereSpeedMps = 12.0;
constexpr double SpearSpeedMps = 9.0;
constexpr double SpearApexM = 0.75;
constexpr int32 PerRing = BudgetStress::EnemyCount / 2;

// The kinds take turns (5 of each). The body is the one ASessionPresentation draws (PresentationLook::Body): the stress
// plan keeps no copy of a presentation colour or size (T46).
FBudgetEnemy MakeEnemy(int32 Index)
{
	static const TCHAR* const Kinds[] = {TEXT("conscript"), TEXT("slinger"), TEXT("cinder_hound"), TEXT("mire_maw")};
	FBudgetEnemy Enemy;
	Enemy.EnemyId = Kinds[Index % 4];
	const FBodyLook Look = PresentationLook::Body(Enemy.EnemyId);
	Enemy.Shape = Look.Shape;
	Enemy.Colour = Look.Colour;
	Enemy.BodySizeCm = Look.SizeCm;
	return Enemy;
}

// The family of an enemy's own shot: the overlay spit's for a spitter (DF-004), else the pinned first attack's.
FString ShotFamily(const FString& EnemyId, const FVrRuleset* Rules)
{
	if (const FVrAttackMode* Spit = Rules ? Rules->FindSpit(EnemyId) : nullptr)
	{
		return Spit->Family;
	}
	for (const FEnemySpec& Spec : KernelData().Enemies)
	{
		if (Spec.Id == EnemyId && Spec.Attacks.Num() > 0)
		{
			return Spec.Attacks[0].Family;
		}
	}
	return FString();
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

bool BuildBudgetStressPlan(const FArenaLayout& Layout, FBudgetStressPlan& OutPlan, FString& OutError, const FVrRuleset* Rules)
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
		Enemy.HpFraction = static_cast<double>(Index % 5 + 1) / 5.0;
		OutPlan.Enemies.Add(Enemy);
	}

	const FVector Right(-Forward.Y, Forward.X, 0.0);
	const double PadTopM = Pad->PositionM.Z;
	for (int32 Index = 0; Index < BudgetStress::ProjectileCount; ++Index)
	{
		FBudgetShot Shot;
		Shot.EnemyIndex = Index % BudgetStress::EnemyCount;
		const FBudgetEnemy& Source = OutPlan.Enemies[Shot.EnemyIndex];
		// Each enemy throws its own attack, drawn as the presentation draws it: a conscript's aimed shot is a spear (every
		// 4th shot), a spitter's an ember, and the colour is the threat language's for the owner's school and the family.
		Shot.Family = ShotFamily(Source.EnemyId, Rules);
		if (Shot.Family.IsEmpty())
		{
			OutError = FString::Printf(TEXT("no attack family for %s (kernel data not ready?)"), *Source.EnemyId);
			return false;
		}
		Shot.bSpear = Source.EnemyId == TEXT("conscript");
		Shot.bEmber = Rules && Rules->FindSpit(Source.EnemyId) != nullptr && Shot.Family == TEXT("magic");
		Shot.Colour = ThreatLook(ActiveElementPalette(), CreatureElement(Source.EnemyId).Get(EElement::Water), Shot.Family).Body;
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

FSceneCensus BudgetStressCensus(const FBudgetStressPlan& Plan)
{
	FSceneCensus Census;
	for (const FBudgetEnemy& Enemy : Plan.Enemies)
	{
		Census.Add(OpaquePartKey(*Enemy.Shape, Enemy.Colour), 1, Enemy.EnemyId + TEXT(" body"));
		Census.Add(OpaquePartKey(TEXT("Cube"), PresentationLook::HpBackColour()), 1, TEXT("HP back"));
		Census.Add(OpaquePartKey(TEXT("Cube"), PresentationLook::HpFillColour()), 1, TEXT("HP fill"));
	}
	for (const FBudgetShot& Shot : Plan.Shots)
	{
		const FString& Owner = Plan.Enemies.IsValidIndex(Shot.EnemyIndex) ? Plan.Enemies[Shot.EnemyIndex].EnemyId : FString();
		Census.Add(OpaquePartKey(Shot.bSpear ? TEXT("Cylinder") : TEXT("Sphere"), Shot.Colour), 1,
		    Owner + (Shot.bSpear ? TEXT(" spear") : (Shot.bEmber ? TEXT(" ember") : TEXT(" shot"))));
		if (Shot.bSpear || Shot.bEmber)
		{
			Census.Add(OpaquePartKey(TEXT("Cylinder"), Shot.Colour), 1, Owner + TEXT(" ground ring"));
		}
	}
	Census.RequiredPasses.Add(ECensusPass::Translucent);
	return Census;
}
