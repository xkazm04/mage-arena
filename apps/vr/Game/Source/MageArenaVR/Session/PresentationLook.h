#pragma once

#include "CoreMinimal.h"

/**
 * T46. How the session presentation draws a body and its HP bar, in one place: ASessionPresentation draws from it, and the
 * D-G4 stress plan (Budget/BudgetStressPlan.cpp) takes its colours and sizes from it, so the stress scene cannot drift
 * from what ships (docs/research/BUDGET-DG4-2026-10.md section 12).
 */
struct FBodyLook
{
	/** An engine basic shape (Greybox::LoadShape). */
	const TCHAR* Shape = TEXT("Cylinder");
	FLinearColor Colour = FLinearColor::White;
	/** Length, width and height in centimetres, the full size Greybox::SetSized takes. */
	FVector SizeCm = FVector(48.0, 48.0, 176.0);
};

namespace PresentationLook
{
/**
 * The body of an enemy id (conscript, slinger, cinder_hound, mire_maw) or an opponent mage (mage-fire, mage-water,
 * mage-air). Any other id stands as a conscript. A body is never an absorb cue (T20, T26).
 */
MAGEARENAVR_API FBodyLook Body(const FString& EnemyId);
/** The training dummy: a conscript's column in a pale tone. */
MAGEARENAVR_API FBodyLook Dummy();
/** The back of an enemy's HP bar. Never near-black: that is the unblockable's core (T49). */
MAGEARENAVR_API FLinearColor HpBackColour();
/** The fill of an enemy's HP bar, and the HP bar on the wrist cuff (T49: one HP colour on the field and the wrist). */
MAGEARENAVR_API FLinearColor HpFillColour();
/** The HP bar: a back and a fill, HpBarLengthCm long and HpBarThicknessCm square, HpBarGapCm above the body top. */
inline constexpr double HpBarLengthCm = 70.0;
inline constexpr double HpBarThicknessCm = 8.0;
inline constexpr double HpBarGapCm = 18.0;
/** The fill shrinks from the right with the HP fraction and never below this length. */
inline constexpr double HpFillMinCm = 4.0;
/**
 * T16 style pick (10-04 work, merged 10-12): -MageArenaStyle=<id> recolours the soldiers (conscript and slinger) from the
 * style file, once per process. The threat and cast colours are ElementColour's since T26 and are applied there.
 */
MAGEARENAVR_API void LoadStyleFromCommandLine();
}
