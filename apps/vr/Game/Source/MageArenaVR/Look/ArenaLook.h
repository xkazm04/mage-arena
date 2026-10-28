#pragma once

#include "CoreMinimal.h"

/**
 * The graphics push's look switch (docs/concepts/GRAPHICS-PUSH-2026-10-28.md, DECISIONS 2026-10-28).
 *
 * -MageArenaLook=flat|quest|epic picks how the arena is presented. Flat is the default and is the greybox exactly as it
 * was before the push (tonemapper off, manual exposure, tinted engine materials), so every automation test and every
 * earlier capture keeps its meaning. Quest is the Quest-viable tier (mobile forward features only, D-G4 budget); Epic is
 * the desktop showcase tier (anything the desktop renderer can do). An unknown value falls back to flat with a warning.
 */
enum class EArenaLook : uint8
{
	Flat,
	Quest,
	Epic,
};

namespace ArenaLook
{
/** The look named on the command line, parsed once per process. */
EArenaLook Current();

/** Parses a -MageArenaLook= value; false (and Flat) for anything else. Exposed for tests. */
bool Parse(const FString& Value, EArenaLook& OutLook);

const TCHAR* Name(EArenaLook Look);

inline bool IsFlat()
{
	return Current() == EArenaLook::Flat;
}
}
