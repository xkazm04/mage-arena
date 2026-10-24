#pragma once

#include "CoreMinimal.h"
#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// T47: a design measurement is logged, never asserted. Each one prints a greppable line,
//   MAGEVR_MEASURE <test> <name>=<value> target=<lo>-<hi> INSIDE|OUTSIDE
// so a number can be read out of an automation log without parsing prose. The verdict a test returns must never
// depend on these lines: -MageArenaMeasureSelfTest shifts every target window past its value, so every line reads
// OUTSIDE, and the per-test pass/fail must not move under it.
namespace MageArenaMeasure
{
inline bool SelfTest()
{
	return FParse::Param(FCommandLine::Get(), TEXT("MageArenaMeasureSelfTest"));
}

// Logs the line and returns INSIDE. bQualifies false (a lost bout has no victory time) reads OUTSIDE.
inline bool Report(const TCHAR* Test, const FString& Name, double Value, double Lo, double Hi, bool bQualifies = true)
{
	if (SelfTest())
	{
		const double Width = Hi - Lo;
		Lo = Value + 1.0 + FMath::Abs(Width);
		Hi = Lo + Width;
	}
	const bool bInside = bQualifies && Value >= Lo && Value <= Hi;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_MEASURE %s %s=%.2f target=%.2f-%.2f %s"), Test, *Name, Value, Lo, Hi,
	    bInside ? TEXT("INSIDE") : TEXT("OUTSIDE"));
	return bInside;
}
}
