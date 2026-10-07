#pragma once

#include "CoreMinimal.h"
#include "Kernel/SimTypes.h"

/**
 * The stone pick at an intermission: one branch of one line, chosen with a palm held over the left or right stone.
 * apps/vr/data/vr/presets.json stonePick.
 */
struct FStonePick
{
	bool bEnabled = false;
	// The composition branch the pick sets. Only "tide_orb" today (Tide Orb IV A or B).
	FString Branch;
	// The pick is offered in the intermission after this bout (1-based). Bout 1 is Tiro wave index 0.
	int32 AfterBout = 1;
	FString Left;
	FString Right;
	FString Default;
	double TimeoutS = 20.0;
};

/** A VR-owned player preset. The pinned runtime.json presets stay the opponents' (and conformance's). */
struct FPlayerPreset
{
	FString Id;
	FString BasedOn;
	FComposition Composition;
	FStonePick Pick;

	/** The composition with the stone pick applied. An empty pick keeps the preset's own branch. */
	FComposition WithPick(const FString& Picked) const;
};

/** apps/vr/data/vr/presets.json, full path. */
FString PlayerPresetPath();

/**
 * Reads the player preset. Strict like combat.vr.json: a missing file, a wrong type, an unknown line, a branch that is
 * not A or B, or a basedOn preset that the pinned data does not have fails the load with a message. Needs KernelData.
 */
bool LoadPlayerPreset(FPlayerPreset& Out, FString& Error, const FString& Path = FString());
