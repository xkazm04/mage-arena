#pragma once

#include "CoreMinimal.h"
#include "Session/ElementColour.h"

class UMaterialInterface;

/**
 * T58. The spell VFX toward key art S1, in one place: the glow shell around a bolt's core, its trail (droplets for Water,
 * embers for Fire, wisps for Air, grit for Earth), the absorb splash at the ward, the perfect-absorb ring at the palm
 * and the pulse of the unblockable's red ground rim.
 *
 * Every effect is a puff. Since T63 a puff is a card (the engine plane) facing the eye through its centre, showing one
 * cell of the flipbook atlas (ECell): flames, spray, crests, embers and wisps. One shared translucent material
 * (M_SpellGlow, unlit, alpha blended) draws Emissive = colour x the cell's shade (at most 1), so every pixel is the
 * threat colour or a darker tone of it, never another hue. A card through a bolt's centre lies behind the near half of
 * its opaque core, so the core keeps its exact colour. ASessionPresentation draws all the puffs of one colour with one
 * instanced component (FGlowFrame), the cell travelling in per-instance custom data, so the whole layer still costs one
 * draw call per colour on screen, whatever the number of projectiles or cells (docs/research/SPELL-VFX-T58-2026-10-25.md,
 * docs/research/SPELL-FLIPBOOK-T63-2026-10-25.md). Pure functions: no world, no RHI, so the tests run them.
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

/**
 * T63. The cells of the flipbook atlas, row by row: a row a school, then the shared row. The atlas is
 * apps/vr/art/vfx/spell-flipbook.png, written by apps/vr/tools/vfx/spell-flipbook.mjs; keep this in step with its CELLS.
 */
enum class ECell : uint8
{
	WaterShellA,
	WaterShellB,
	WaterSpray,
	WaterCrown,
	FireFlameA,
	FireFlameB,
	FireEmbers,
	FireCrown,
	AirSwirlA,
	AirSwirlB,
	AirWisp,
	AirCrown,
	// T58's soft falloff, unchanged: the unblockable's shell, so the leave cue reads as it did.
	SoftGlow,
	DropletStreak,
	SparkStreak,
	Grit,
};
/** Cells a side of the atlas. */
inline constexpr int32 FlipbookGrid = 4;
/** Frames a second of a shell's flicker (two cells, each also mirrored: four looks). */
inline constexpr double FlipbookFps = 10.0;

/**
 * One glow puff: the centre, the full size in centimetres (X along the rotation's forward) and the rotation. T63: the
 * atlas cell its card shows, mirrored left to right when bMirror. A streak's card lies along the rotation's forward as
 * seen from the eye (a drop or a wisp in flight); any other card stands upright, turned by Roll radians about the line
 * of sight (a shell, a crown, an ember cluster).
 */
struct FPuff
{
	FVector CentreCm = FVector::ZeroVector;
	FVector SizeCm = FVector(10.0);
	FQuat Rotation = FQuat::Identity;
	ECell Cell = ECell::SoftGlow;
	bool bMirror = false;
	bool bStreak = false;
	float Roll = 0.f;
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

/** The perfect-absorb ring at the palm: 20 beads on a widening ring facing TowardEye, each a streak of Trail's school. */
MAGEARENAVR_API void PerfectRingPuffs(const FVector& AtCm, const FVector& TowardEye, double Age01, TArray<FPuff>& Out, ETrail Trail = ETrail::Droplets);

/**
 * L2. One flame of a fire wall standing at BaseCm (the sand under it), Index along its wall's arc: the school's flame
 * cell, flickering and mirrored per flame. The flames are WallFlameWidthCm wide and WallFlameHeightCm tall, set apart by
 * the wall's slat spacing, so the sand and the rival behind show between them (the cards are clear outside the flame).
 */
MAGEARENAVR_API void WallFlamePuffs(const FVector& BaseCm, int32 Index, double TimeS, TArray<FPuff>& Out);
/** L2: a wall flame's card, centimetres. It stands below the seated eye line (1.2 m), so the rival's body shows above it. */
inline constexpr double WallFlameWidthCm = 70.0;
inline constexpr double WallFlameHeightCm = 105.0;

/** T63. The custom data floats a card carries into M_SpellGlow: the atlas U scale (negative when mirrored), U offset, V offset. */
inline constexpr int32 CardCustomFloats = 3;
MAGEARENAVR_API void CardCustomData(const FPuff& Puff, float (&Out)[CardCustomFloats]);
/** The cell a card's custom data shows, and whether mirrored: the inverse of CardCustomData. */
MAGEARENAVR_API ECell CellOfCustomData(const float (&Data)[CardCustomFloats], bool& bOutMirror);
/**
 * T63. The instance transform of a puff's card, the engine plane (100 cm, U along +X, V along +Y, facing +Z), turned to
 * face EyeCm. Upright, V runs down the screen, so the atlas reads top up (a flame licks upward). A streak seen head-on
 * shortens toward a round card, as the sphere it replaced did.
 */
MAGEARENAVR_API FTransform CardTransform(const FPuff& Puff, const FVector& EyeCm);
/** A card is this much wider than its puff: the atlas keeps each cell's content inside 0.9 of it (a clear border). */
inline constexpr double CardOversize = 1.0 / 0.9;

/**
 * T63. One frame of glow puffs grouped by colour: each colour is one layer (one instanced draw) whatever cells its puffs
 * show, the cell riding in the custom data. Reset keeps the colours, so a layer emptied this frame is hidden.
 */
struct MAGEARENAVR_API FGlowFrame
{
	struct FLayer
	{
		TArray<FTransform> Transforms;
		TArray<float> CustomData;
	};
	TMap<FLinearColor, FLayer> Layers;
	FVector EyeCm = FVector::ZeroVector;
	int32 Drawn = 0;

	void Reset(const FVector& InEyeCm);
	/** Queues Puffs as cards facing EyeCm on the layer of Colour, up to MaxPuffs in the frame. */
	void Add(const FLinearColor& Colour, TConstArrayView<FPuff> Puffs);
};

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
 * T63: a trail puff that shows a cluster (Water's spray, Fire's embers, Earth's grit) is this much wider than T58's single
 * drop: its cluster fills the card, and the drops inside are finer than the one sphere they replace.
 */
inline constexpr double ClusterCardScale = 1.5;
/** T63: the absorb crown's card is this much wider than T58's crown disc: its splash body fills the middle of the card. */
inline constexpr double CrownCardScale = 1.4;
/** T63: a perfect-ring bead's streak card is this much larger than T58's bead: the streak's head is a part of its card. */
inline constexpr double RingCardScale = 1.7;
/**
 * T67: Fire's volume at the same puff count. The shell card is this much wider and taller than the other schools' (a flame
 * licks upward, so more tall than wide), a trail ember cluster this much wider, and the absorb flame star this much larger.
 * Size only: the colour, the cells' shade (at most 1) and the puff count are T63's.
 */
inline constexpr double FireShellWidth = 1.15;
inline constexpr double FireShellHeight = 1.45;
inline constexpr double FireEmberScale = 1.25;
inline constexpr double FireCrownScale = 1.15;
/** T67: Fire's streak beads (the perfect ring, the splash) are this much larger. */
inline constexpr double FireStreakScale = 1.3;
/**
 * The most puffs drawn in a frame. A puff is counted against the 2,000-particle budget (PROJECT-PLAN section 7) as if it
 * were a particle: D-G4 measured 1,468 live Niagara particles at the max, and 1,468 + 500 stays under 2,000.
 */
inline constexpr int32 MaxPuffs = 500;

/** The asset the material is saved to by Tools/CreateBudgetFxCommandlet (-spellvfx). */
inline constexpr const TCHAR* GlowMaterialPackage = TEXT("/Game/Vfx/M_SpellGlow");
/** T63: the flipbook atlas, imported by the same commandlet from apps/vr/art/vfx/spell-flipbook.png. */
inline constexpr const TCHAR* FlipbookTexturePackage = TEXT("/Game/Vfx/T_SpellFlipbook");
/** M_SpellGlow, or the engine's flat unlit translucent material (logged once) when the asset is missing. */
MAGEARENAVR_API UMaterialInterface* GlowMaterial();
}
