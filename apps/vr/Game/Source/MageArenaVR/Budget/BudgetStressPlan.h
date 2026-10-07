#pragma once

#include "CoreMinimal.h"
#include "Greybox/ArenaLayout.h"

/**
 * D-G4 stress scene (docs/PROJECT-PLAN.md D-G4 row): the greybox arena, 20 enemy proxies,
 * 100 projectiles in flight and both hands. The plan is pure data so a test can build it
 * without a renderer. The driver in BudgetStressDriver.h draws it.
 */
namespace BudgetStress
{
inline constexpr int32 EnemyCount = 20;
inline constexpr int32 ProjectileCount = 100;
inline constexpr int32 HandCount = 2;
/** Every 4th shot is a spear (cylinder), the rest are spheres, as the presentation draws them. */
inline constexpr int32 SpearEvery = 4;
/** The looping two-hand clip: Game/Clips/both-palms.normal.jsonl (hands L and R). */
inline const TCHAR* const HandClipAction = TEXT("both-palms");
/** The plan's +/-70 degree threat arc, from the centre pad (arena-layout.json spawnArc.limitDeg). */
inline constexpr double ArcLimitDeg = 70.0;
/** Distance kept between any placement and the floor ellipse edge. The wall stands on that edge. */
inline constexpr double EdgeMarginM = 1.0;
}

/** One enemy proxy: 3 parts (body, HP back, HP fill), sized as SessionPresentation sizes them. */
struct FBudgetEnemy
{
	FString EnemyId;
	/** Ground point, metres. */
	FVector GroundM = FVector::ZeroVector;
	double DistanceM = 0.0;
	double BearingDeg = 0.0;
	/** Body size in centimetres: length, width, height. */
	FVector BodySizeCm = FVector(48.0, 48.0, 176.0);
	FLinearColor Colour = FLinearColor::White;
	bool bCube = false;
};

/** One projectile lane: it flies StartM to EndM in FlightS, then respawns at StartM. */
struct FBudgetShot
{
	int32 EnemyIndex = 0;
	bool bSpear = false;
	FVector StartM = FVector::ZeroVector;
	FVector EndM = FVector::ZeroVector;
	double FlightS = 1.5;
	/** Fraction of the flight already flown at t = 0, so the 100 shots are spread along their lanes. */
	double Phase = 0.0;
	FLinearColor Colour = FLinearColor::White;
};

struct FBudgetStressPlan
{
	TArray<FBudgetEnemy> Enemies;
	TArray<FBudgetShot> Shots;
	int32 Hands = 0;
	/** The arc actually used per ring after clipping to the floor ellipse, degrees each side. */
	double NearArcDeg = 0.0;
	double FarArcDeg = 0.0;
	double NearDistanceM = 0.0;
	double FarDistanceM = 0.0;
};

/** True when XY lies inside the floor ellipse shrunk by MarginM. */
bool BudgetPointOnFloor(const FArenaLayout& Layout, const FVector& PointM, double MarginM);

/** Builds the population from the layout's centre pad and spawn-marker radii. False with an error when the layout lacks them. */
bool BuildBudgetStressPlan(const FArenaLayout& Layout, FBudgetStressPlan& OutPlan, FString& OutError);

/** Shot position at Seconds after the start, metres. Spears arc; spheres fly straight. */
FVector BudgetShotPositionM(const FBudgetShot& Shot, double Seconds, int32* OutLaunches = nullptr);
