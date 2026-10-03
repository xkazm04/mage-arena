#pragma once

#include "Kernel/KernelData.h"

// Fire school. Parallel to Water. Numbers come from the parsed pin, not from literals here.

const FFireSpell* FireSpellFor(const FActor& Actor, int32 Slot);
int32 FireCooldownUntil(const FActor& Actor, const FString& SpellId);
bool TryFireCast(FArenaState& State, FActor& Actor, const FInputFrame& Input);
void ReleaseFire(FArenaState& State, FActor& Actor);
void UpdateFireResource(FArenaState& State, FActor& Actor);
void UpdateFireOngoing(FArenaState& State, FActor& Actor);
double FireSpellDamageMult(const FActor& Actor);
double FireAbsorbDrainMult(const FArenaState& State, const FActor& Actor);
double FireStaminaRegenMult(const FActor& Actor);
bool FireCastRoots(const FActor& Actor);
bool FireChannelBusy(const FActor& Actor);
void CancelFireChannel(FActor& Actor);
void ResetFire(FActor& Actor);
void FireOnHitResolved(FArenaState& State, FActor& Target, const FHit& Hit, double Incoming, double Reduction);
// The same grant the spells use. A non-fire actor, a non-positive amount, or a heat lock pays nothing.
void GainHeat(FArenaState& State, FActor& Actor, double Amount);
