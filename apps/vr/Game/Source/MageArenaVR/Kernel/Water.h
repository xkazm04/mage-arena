#pragma once

#include "Kernel/SimTypes.h"

bool TrySpellCast(FArenaState& State, FActor& Actor, const FInputFrame& Input);
void ReleaseSpell(FArenaState& State, FActor& Actor);
void UpdateWater(FArenaState& State, FActor& Actor);
double WardDrainMult(const FArenaState& State, const FActor& Actor);
double RefundMult(const FArenaState& State, const FActor& Actor);
void WaterAbsorbed(FArenaState& State, FActor& Actor, const FHit& Hit, double Prevented, bool bPerfect);
void ReflectProjectile(FArenaState& State, FActor& Target, const FProjectile& Projectile);
bool HasLineOfSight(const FArenaState& State, const FSimVec& A, const FSimVec& B);
void ResetWater(FActor& Actor);
