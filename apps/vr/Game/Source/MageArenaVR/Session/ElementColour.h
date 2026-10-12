#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FArenaState;
struct FVrRuleset;

/**
 * T26. The one place that decides what colour a threat or a cast is drawn in.
 *
 * The threat language (PROJECT-PLAN 6.3, combat.json threatLanguage) is kept exactly: magic draws in the colour of the
 * school that threw it (absorb), physical draws steel (dodge), unblockable draws a black core with a red rim (leave),
 * whatever its school. The school is the owner's: the player casts Water (the slice's only player school), a Fire mage
 * Fire, an air mage Air, a creature the school in CreatureElement. Earth has a colour reserved and no caster yet.
 */
enum class EElement : uint8
{
	Water,
	Fire,
	Air,
	Earth,
};

/** The base colours (linear). A style file may override any of them; the greybox values are Greybox(). */
struct MAGEARENAVR_API FElementPalette
{
	FLinearColor Water;
	FLinearColor Fire;
	FLinearColor Air;
	FLinearColor Earth;
	FLinearColor Steel;
	FLinearColor UnblockableBody;
	FLinearColor UnblockableRim;

	static FElementPalette Greybox();
	FLinearColor ElementColour(EElement Element) const;
};

/** What a threat or a cast is drawn with. bRim is true only for the unblockable family. */
struct FThreatLook
{
	FLinearColor Body;
	FLinearColor Rim;
	bool bRim = false;
};

/** The palette the presentation draws with: Greybox() unless a style override was applied. */
MAGEARENAVR_API const FElementPalette& ActiveElementPalette();
MAGEARENAVR_API void SetActiveElementPalette(const FElementPalette& Palette);
MAGEARENAVR_API void ResetActiveElementPalette();

/**
 * Applies a style file's colours over Palette. Reads the T16 style schema (apps/vr/data/vr/styles/style-<id>.json):
 * "projectiles": { "magic" (the Water colour; "water" is accepted too), "fire", "air", "earth", "steel",
 * "unblockableBody", "unblockableRim" }, each [r, g, b] linear. Missing keys keep the palette's value. Returns the number
 * of colours applied.
 */
MAGEARENAVR_API int32 ApplyStylePalette(FElementPalette& Palette, const FJsonObject& Style);

/**
 * -MageArenaStyle=<id> (not "greybox"): applies apps/vr/data/vr/styles/style-<id>.json over the greybox palette once per
 * process. A missing file logs a warning and keeps the greybox palette.
 */
MAGEARENAVR_API void LoadStylePaletteFromCommandLine();

/** The school of a creature's magic by enemy id: cinder_hound Fire (its embers), mire_maw Water (the bog glob). */
MAGEARENAVR_API TOptional<EElement> CreatureElement(const FString& EnemyId);

/**
 * The school an actor casts in. The player and every mage without Fire or Air cast the kernel's Water spellbook; a Fire
 * mage (Fire.bSchool) Fire; an air mage (Air.bSchool) Air; a creature its CreatureElement. Unset for an unknown actor id
 * or a creature with no school (none of those throws magic in the pinned enemies.json; MageArena.Colour.CreatureSchools
 * keeps it that way).
 */
MAGEARENAVR_API TOptional<EElement> ActorElement(const FArenaState& State, int32 ActorId);

/** Family "unblockable" -> black body, red rim; "physical" -> steel; anything else (magic) -> the element colour. */
MAGEARENAVR_API FThreatLook ThreatLook(const FElementPalette& Palette, EElement Element, const FString& Family);

/**
 * The look of a projectile, telegraph or cast from its owner and family. An owner with no school (unknown id) can only
 * be drawn by family: unblockable and physical are unaffected; magic falls back to Water and logs a warning once.
 */
MAGEARENAVR_API FThreatLook ThreatLookFor(const FElementPalette& Palette, const FArenaState& State, int32 OwnerId, const FString& Family);

/**
 * The one entry point the presentation uses for every projectile, telegraph and cast. A cast the player owns draws in the
 * player's school colour with no rim, whatever its family (T20: the player's own Leviathan Orb lane is Water so it never
 * reads as a threat to the seat; a reflected bolt is the player's from the reflect on). Everything else is a threat:
 * ThreatLookFor.
 */
MAGEARENAVR_API FThreatLook ElementLook(const FElementPalette& Palette, const FArenaState& State, int32 PlayerId, int32 OwnerId, const FString& Family);
