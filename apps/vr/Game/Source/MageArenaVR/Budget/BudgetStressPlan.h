#pragma once

#include "CoreMinimal.h"
#include "Greybox/ArenaLayout.h"

struct FSceneCensus;
struct FVrRuleset;

/**
 * D-G4 stress scene (docs/PROJECT-PLAN.md D-G4 row): the greybox arena, 20 enemies, 100 projectiles in flight and both
 * hands. The plan is pure data so a test can build it without a renderer. Since T46 the driver in BudgetStressDriver.h
 * poses it into a live session bout and the session's own ASessionPresentation draws it.
 */
namespace BudgetStress
{
inline constexpr int32 EnemyCount = 20;
inline constexpr int32 ProjectileCount = 100;
inline constexpr int32 HandCount = 2;
/** The Tiro wave the capture's session bout plays (SetBout(2, true): Brennic's semifinal, a rival bout with a fire wall). */
inline constexpr int32 SessionBout = 2;
/** The looping two-hand clip: Game/Clips/both-palms.normal.jsonl (hands L and R). */
inline const TCHAR* const HandClipAction = TEXT("both-palms");
/** The plan's +/-70 degree threat arc, from the centre pad (arena-layout.json spawnArc.limitDeg). */
inline constexpr double ArcLimitDeg = 70.0;
/** Distance kept between any placement and the floor ellipse edge. The wall stands on that edge. */
inline constexpr double EdgeMarginM = 1.0;
}

/** One enemy: 3 parts (body, HP back, HP fill), as ASessionPresentation draws an enemy. */
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
	/** The engine shape of the body. */
	FString Shape = TEXT("Cylinder");
	/** HP over max HP, so the fills have five lengths. */
	double HpFraction = 1.0;
};

/**
 * One projectile lane: it flies StartM to EndM in FlightS, then respawns at StartM. The shot is its enemy's own attack:
 * the family is the enemy's (the overlay spit's for a spitter), a conscript's is a thrown spear, a spitter's an ember.
 */
struct FBudgetShot
{
	int32 EnemyIndex = 0;
	/** "physical" or "magic": the colour follows it (ElementColour.h ThreatLook). */
	FString Family;
	bool bSpear = false;
	/** A hound ember (DF-004 option A): the presentation rings it on the ground where it will land. */
	bool bEmber = false;
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

/**
 * Builds the population from the layout's centre pad and spawn-marker radii. False with an error when the layout lacks them.
 * Rules is the session's VR overlay: with it a spitter's shot takes the spit's family (DF-004); without it the pinned attack's.
 */
bool BuildBudgetStressPlan(const FArenaLayout& Layout, FBudgetStressPlan& OutPlan, FString& OutError, const FVrRuleset* Rules = nullptr);

/**
 * T46: the parts the plan puts in the frame, as ASessionPresentation draws them: per enemy a body and the HP back and fill,
 * per shot one part, and one ground ring per spear and per ember. Every pass the posed session frame must draw is required
 * (translucent: the cuff plane, the dome and the fire-wall curtains). The live census must hold it (CompareSceneCensus).
 */
FSceneCensus BudgetStressCensus(const FBudgetStressPlan& Plan);

/** Shot position at Seconds after the start, metres. Spears arc; spheres fly straight. */
FVector BudgetShotPositionM(const FBudgetShot& Shot, double Seconds, int32* OutLaunches = nullptr);
