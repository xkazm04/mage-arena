#pragma once

#include "CoreMinimal.h"
#include "Session/SpellVfx.h"

class AActor;
class UInstancedStaticMeshComponent;
struct FArenaLayout;
struct FArenaProp;
struct FElementPalette;

/**
 * T60. The arena braziers as fire props: an iron bowl that reads as a bowl, and a slow, licking flame in an environment
 * colour of its own (docs/research/BRAZIERS-T60-2026-10-25.md).
 *
 * The bowl is a stack of flaring cylinder rings on the brazier's existing instanced component, so it costs no draw call.
 * The flame is T58's glow puff (M_SpellGlow, Session/SpellVfx.h): two instanced layers for all four braziers, a broad ochre
 * body and a brighter amber heart, so at most two draw calls whatever the number of braziers.
 *
 * The flame is scenery, never a cue. Its colours are derived here, never read from a style's `projectiles.fire` (the fire
 * spell's absorb colour) nor from `arena.flame` (style 25 samples that as the leave red): an amber and an ochre whose
 * chroma stands off every threat class, dimmer than the dimmest threat glow of the palette in force. Its flicker is a slow
 * sway and breath of the puffs in place, under 1.5 Hz, unlike the fast, rising embers of a fire bolt's trail.
 */
namespace BrazierFire
{
enum class ELayer : uint8
{
	// The broad, dim ochre lick.
	Body,
	// The brighter amber heart low in the bowl.
	Heart,
};

/**
 * The layers' chroma (each divided by its brightest channel): a muted amber heart and a warmer body, both less saturated
 * than Earth (whose hue they sit near) so they stand off it, and less yellow than an ochre, which at this luminance reads
 * olive. Earth and Fire hold the saturated oranges.
 */
inline const FLinearColor HeartChroma(1.0f, 0.66f, 0.26f);
inline const FLinearColor BodyChroma(1.0f, 0.58f, 0.27f);
/** The heart's luminance as a share of the dimmest threat glow's, and the body's as a share of the heart's. */
inline constexpr float HeartShare = 0.95f;
inline constexpr float BodyShare = 0.85f;
/** M_SpellGlow's GlowGain on the flame layers (a spell glow uses 1). */
inline constexpr float FlameGlowGain = 1.4f;
/**
 * The least brightest channel of a flame layer: a dim palette (style 25's rim) must not darken it into the black core
 * class (every channel under 0.08), the unblockable's half of the leave pair.
 */
inline constexpr float MinBrightest = 0.10f;
/** The least chroma distance (linear RGB, each divided by its brightest channel) from every threat colour. */
inline constexpr float ThreatChromaMargin = 0.15f;

/** The least luminance of the colours a threat glows in: the four elements and the unblockable's rim. Steel never glows. */
MAGEARENAVR_API float DimmestGlowLuminance(const FElementPalette& Palette);
/**
 * The colour of a flame layer under Palette: its chroma at its share of the dimmest threat glow's luminance, and never
 * with its brightest channel under MinBrightest.
 */
MAGEARENAVR_API FLinearColor FlameColour(const FElementPalette& Palette, ELayer Layer);

/** One piece of the brazier: a cylinder by its centre and full size, in metres. */
struct FPiece
{
	FVector CentreM = FVector::ZeroVector;
	FVector SizeM = FVector::OneVector;
};

/** The foot, the stem and the bowl's five rings, widening upward to the lip (BowlRadiusM) at StandHeightM + BowlHeightM. */
MAGEARENAVR_API void BowlPieces(const FArenaProp& Brazier, TArray<FPiece>& Out);
inline constexpr int32 BowlPiecesPerBrazier = 7;

/**
 * The puffs of one brazier's flame at TimeS, in centimetres, by layer. Seed sets its own phase, so no two braziers lick
 * together. Three body puffs (the bed in the bowl's mouth and two forked tongues) and two heart puffs.
 */
MAGEARENAVR_API void FlamePuffs(const FArenaProp& Brazier, int32 Seed, double TimeS, TArray<SpellVfx::FPuff>& OutBody, TArray<SpellVfx::FPuff>& OutHeart);
inline constexpr int32 BodyPuffsPerBrazier = 3;
inline constexpr int32 HeartPuffsPerBrazier = 2;
/** The old cone's height in flame radii: no puff reaches above it, nor outside the cone it draws from the lip up. */
inline constexpr double MaxFlameTopRadii = 2.5;
/** The fastest of the flicker's three waves, in Hz. */
inline constexpr double FastestFlickerHz = 1.4;

/** -MageArenaNoBrazierFire: the bowls without their flames (the before stills, the D-G4 parity run). */
MAGEARENAVR_API bool IsSwitchedOff();

struct FFlameLayers
{
	UInstancedStaticMeshComponent* Body = nullptr;
	UInstancedStaticMeshComponent* Heart = nullptr;
};

/** Adds the two flame layers to Owner for every brazier of Layout, posed at time 0. Nothing when bSwitchedOff. */
MAGEARENAVR_API FFlameLayers Build(AActor* Owner, const FArenaLayout& Layout, const FElementPalette& Palette, bool bSwitchedOff);
/** Poses every brazier's puffs at TimeS (no instance is added or removed). */
MAGEARENAVR_API void Animate(const FFlameLayers& Layers, const FArenaLayout& Layout, double TimeS);
}
