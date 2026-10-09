#pragma once

#include "Kernel/KernelData.h"

// Air school (T23, docs/schools/AIR.md). Parallel to Fire. Numbers come from the parsed pin (schools.json air) and the
// VR proposal sheet (spells-air.csv), not from literals here. A non-air actor (bSchool false) is untouched by every call.

struct FVrRuleset;

const FAirSpell* AirSpellFor(const FActor& Actor, int32 Slot);
int32 AirCooldownUntil(const FActor& Actor, const FString& SpellId);
// Tier, cooldown, mana and, for Air Form, the Momentum the row needs. The cast and the AI ask the same question.
bool AirCastLegal(const FArenaState& State, const FActor& Actor, const FAirSpell& Spell);
bool TryAirCast(FArenaState& State, FActor& Actor, const FInputFrame& Input, const FVrRuleset* Rules = nullptr);
// VR rivals: a cast that skips the tier gate, the cooldown and the mana cost once. The normal cast path otherwise.
bool StartGrantedAirCast(FArenaState& State, FActor& Actor, const FString& SpellId, const FSimVec& Aim, const FVrRuleset* Rules);
void ReleaseAir(FArenaState& State, FActor& Actor, const FVrRuleset* Rules = nullptr);
// Runs once per actor step, right after the move and before the same-tick release (see Air.cpp).
void UpdateAirResource(FArenaState& State, FActor& Actor);
void UpdateAirOngoing(FArenaState& State, FActor& Actor, const FVrRuleset* Rules = nullptr);
void GainMomentum(FArenaState& State, FActor& Actor, double Amount);

double AirAbsorbDrainMult(const FArenaState& State, const FActor& Actor);
double AirRollStaminaMult(const FActor& Actor);
double AirSpellRangeMult(const FActor& Actor);
int32 AirSpellsPierce(const FActor& Actor);
bool AirCastRoots(const FActor& Actor);
// A Cyclone channel or the rest of a Squall volley. Either one keeps the hands busy.
bool AirChannelBusy(const FActor& Actor);
void CancelAirChannel(FActor& Actor);
void ResetAir(FActor& Actor);

// ResolveHit hooks. Evade runs before the ward; the payment runs after the hit lands.
bool AirTryEvade(FArenaState& State, FActor& Target, const FHit& Hit);
void AirOnHitResolved(FArenaState& State, const FHit& Hit);
// Perfect-absorb deflect: a projectile of tier <= the air actor's unlocked tier leaves along the actor's aim.
bool AirTryDeflect(FArenaState& State, FActor& Target, const FProjectile& Projectile);

// Veering Bolt geometry, shared by the kernel and the presentation so the drawn arc is the flown arc.
struct FAirArc
{
	FSimVec From;
	FSimVec To;
	FSimVec Centre;
	double RadiusM = 0.0;
	double StartAngle = 0.0;
	// +1 turns counter-clockwise, -1 clockwise.
	double TurnSign = 1.0;
	double LengthM = 0.0;
	FSimVec LaunchDir;
	bool bStraight = false;
};
// Side +1 launches to the left of the line of sight, -1 to the right. Deterministic: no random draw.
double AirVeerSide(int32 ActivationId);
FAirArc AirVeerArc(const FSimVec& From, const FSimVec& To, double EntryDeg, double Side);
FSimVec AirArcPoint(const FAirArc& Arc, double DistanceM);
// The end of this tick's flight for a curved projectile. Fills the angle, the velocity and the arc still to fly.
FSimVec AirCurvedStep(const FProjectile& Projectile, double Travel, double& OutAngle, FSimVec& OutVelocity, double& OutArcLeft);
