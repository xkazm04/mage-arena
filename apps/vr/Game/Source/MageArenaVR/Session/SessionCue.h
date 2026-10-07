#pragma once

#include "CoreMinimal.h"

/**
 * T26. A session-level moment the kernel does not see (the recognizer's verdicts), appended by FArenaSession and read by
 * the audio director with its own cursor. Kind is "sigil-complete" or "sigil-reject".
 */
struct FSessionCue
{
	FString Kind;
	double SimS = 0.0;
};
