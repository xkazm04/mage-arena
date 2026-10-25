#pragma once

#include "CoreMinimal.h"
#include "Session/ElementColour.h"

class UMaterialInterface;

/**
 * T58. The spell VFX toward key art S1, in one place: the glow shell around a bolt's core, its trail (droplets for Water,
 * embers for Fire, wisps for Air, grit for Earth), the absorb splash at the ward, the perfect-absorb ring at the palm
 * and the pulse of the unblockable's red ground rim.
 *
 * Every effect is a puff: a sphere instance drawn with one shared translucent material (M_SpellGlow, unlit, alpha
 * blended toward exactly its colour; only the far hemisphere shows, so a bolt's opaque core hides the glow behind it and
 * keeps its exact colour). ASessionPresentation draws all the puffs of one
 * colour with one instanced component, so the whole layer costs one draw call per colour on screen, whatever the number
 * of projectiles (docs/research/SPELL-VFX-T58-2026-10-25.md). Pure functions: no world, no RHI, so the tests run them.
 *
 * The threat colour language is kept exactly: a puff is drawn in GlowColour of the look it belongs to, which is the
 * element colour for magic (absorb) and the rim red for the unblockable (leave). Steel (dodge) has no glow.
 */
namespace SpellVfx
{
enum class ETrail : uint8
{
	Droplets,
	Embers,
	Wisps,
	Grit,
	// The unblockable's shell has no trail: its black core and red rim stay the only thing it reads as.
	None,
};

/** One glow puff: the centre, the full size in centimetres (X along the rotation's forward) and the rotation. */
struct FPuff
{
	FVector CentreCm = FVector::ZeroVector;
	FVector SizeCm = FVector(10.0);
	FQuat Rotation = FQuat::Identity;
};

/**
 * The glow colour of a look and its family. Unblockable (bRim): the rim red. Physical: unset, steel never glows. Anything
 * else (magic, or a cast the player owns, which ElementLook draws in the player's school): the look's body colour.
 */
MAGEARENAVR_API TOptional<FLinearColor> GlowColour(const FThreatLook& Look, const FString& Family);

/** The trail of an element. Water droplets, Fire embers, Air wisps, Earth grit. */
MAGEARENAVR_API ETrail TrailFor(EElement Element);

/** Puffs one bolt adds: the shell around its core and its trail behind it, against FlightDir (unit, world). */
MAGEARENAVR_API void BoltPuffs(ETrail Trail, const FVector& AtCm, const FVector& FlightDir, double CoreDiameterCm, int32 Seed, double TimeS, TArray<FPuff>& Out);

/**
 * The absorb splash at the ward, Age01 from 0 (contact) to 1 (gone), in the plane facing TowardEye: a crown disc (when
 * bCrown) and ten droplets. A perfect absorb draws it without the crown: the ring is its centre-piece, and a crown
 * stacked under it muddied the ring.
 */
MAGEARENAVR_API void SplashPuffs(ETrail Trail, const FVector& AtCm, const FVector& TowardEye, double Age01, int32 Seed, bool bCrown, TArray<FPuff>& Out);

/** The perfect-absorb ring at the palm: 20 beads on a widening ring facing TowardEye. */
MAGEARENAVR_API void PerfectRingPuffs(const FVector& AtCm, const FVector& TowardEye, double Age01, TArray<FPuff>& Out);

/**
 * The unblockable rim's pulse at TimeS, 0 to 1, in RimPulseSteps steps (each step one shared tint). The rim stays opaque:
 * an additive red over the ochre sand reads salmon, toward the fire class, so the rim itself pulses instead.
 */
MAGEARENAVR_API float RimPulse(double TimeS);
/** The rim colour at Pulse: the rim red brightened up to RimPulseGain times, with the rim's own chroma. */
MAGEARENAVR_API FLinearColor RimPulseColour(const FLinearColor& Rim, float Pulse);
/** Pulses a second, the steps, the brightest gain and how far the rim widens (cm, each side) at the top of a pulse. */
inline constexpr double RimPulseHz = 1.6;
inline constexpr float RimPulseSteps = 4.f;
inline constexpr float RimPulseGain = 1.34f;
inline constexpr double RimPulseCm = 14.0;

/** Seconds an absorb splash and a perfect ring last. */
inline constexpr double SplashS = 0.40;
inline constexpr double PerfectS = 0.55;
/** The shell's diameter as a multiple of the core's. */
inline constexpr double ShellScale = 2.4;
/**
 * The most puffs drawn in a frame. A puff is counted against the 2,000-particle budget (PROJECT-PLAN section 7) as if it
 * were a particle: D-G4 measured 1,468 live Niagara particles at the max, and 1,468 + 500 stays under 2,000.
 */
inline constexpr int32 MaxPuffs = 500;

/** The asset the material is saved to by Tools/CreateBudgetFxCommandlet (-spellvfx). */
inline constexpr const TCHAR* GlowMaterialPackage = TEXT("/Game/Vfx/M_SpellGlow");
/** M_SpellGlow, or the engine's flat unlit translucent material (logged once) when the asset is missing. */
MAGEARENAVR_API UMaterialInterface* GlowMaterial();
}
