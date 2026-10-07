#pragma once

#include "Kernel/SimTypes.h"

// Null is the pinned cast: Flow, Crest mana, and the crest multiplier. The overlay passes a mod.
struct FSpellCastMod
{
	bool bSuppressFlow = false;
	double PowerMult = 1.0;
};

bool TrySpellCast(FArenaState& State, FActor& Actor, const FInputFrame& Input, const FSpellCastMod* Mod = nullptr);
void ReleaseSpell(FArenaState& State, FActor& Actor);
void UpdateWater(FArenaState& State, FActor& Actor);
double WardDrainMult(const FArenaState& State, const FActor& Actor);
double RefundMult(const FArenaState& State, const FActor& Actor);
void WaterAbsorbed(FArenaState& State, FActor& Actor, const FHit& Hit, double Prevented, bool bPerfect);
// Mirror II-A Reflection. True when a reflected copy was spawned back at the caster. Emits nothing: the caller owns
// the event, so the null-ruleset path (conformance) keeps its event log.
bool ReflectProjectile(FArenaState& State, FActor& Target, const FProjectile& Projectile);
bool HasLineOfSight(const FArenaState& State, const FSimVec& A, const FSimVec& B);
void ResetWater(FActor& Actor);
