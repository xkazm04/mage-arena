#pragma once

#include "CoreMinimal.h"
#include "ClipVariant.generated.h"

/** Which recording of an action to play. Shift selects Sloppy, Ctrl selects Slow. */
UENUM(BlueprintType)
enum class EClipVariant : uint8
{
	Normal,
	Slow,
	Sloppy
};
