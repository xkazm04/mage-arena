#pragma once

#include "CoreMinimal.h"

/** Hit family names match absorb.reduction and absorb.perfect.notAgainst in the pinned combat file. */
enum class EAbsorbHitKind : uint8
{
	Magic,
	Physical,
	Unblockable
};

/** What the detector last decided. The resolver does not own a detector. */
struct FWardState
{
	bool bRaised = false;
	bool bFresh = false;
	double OnsetTime = 0.0;
	FVector Facing = FVector::ForwardVector;
};

struct FAbsorbResult
{
	double Reduction = 0.0;
	bool bPerfect = false;
	double ManaReturned = 0.0;
	double TierClockAdvanceS = 0.0;
};

/**
 * Absorb numbers parsed from the pinned combat file.
 * Fields stay zero until LoadFromPinnedFile succeeds, so a missed load cannot look like the real rules.
 */
struct FAbsorbRules
{
	double ArcDeg = 0.0;
	double RaiseCostMana = 0.0;
	double MinReleaseS = 0.0;
	double ReductionMagic = 0.0;
	double ReductionPhysical = 0.0;
	double ReductionUnblockable = 0.0;
	double ReductionOutsideArc = 0.0;
	double WindowS = 0.0;
	double PerfectReduction = 0.0;
	double TierClockAdvanceS = 0.0;
	double MoveSpeedWhileHeldMult = 0.0;
	double DrainBase = 0.0;
	double DrainSubtractPerRank = 0.0;
	double ManaPerTier = 0.0;
	double ManaPerRank = 0.0;
	double TierZeroReturn = 0.0;
	FString DrainFormula;
	FString ManaFormula;
	FString SourcePath;
	TArray<FString> NotAgainst;

	bool LoadFromPinnedFile(FString& OutError);
	double ReductionFor(EAbsorbHitKind Kind) const;
	bool BlocksPerfect(EAbsorbHitKind Kind) const;
	double ManaForPerfect(int32 IncomingTier, double NerveRank) const;
	double DrainPerSecond(double NerveRank) const;
	static const TCHAR* KindName(EAbsorbHitKind Kind);
	static FString PinnedFilePath();
};

/** Pure hit rule. No actors. Continuous time, read from the pinned absorb block. */
class FAbsorbResolver
{
public:
	/** No nerve stat is wired yet. Coefficients still come from the file; this rank does not. */
	static constexpr double NerveRank = 0.0;

	bool Init(FString& OutError);
	bool IsReady() const { return bReady; }
	const FAbsorbRules& Rules() const { return LoadedRules; }

	FAbsorbResult Resolve(double HitTime, EAbsorbHitKind Kind, int32 IncomingTier, const FVector& HitDirection, const FWardState& WardState) const;

private:
	FAbsorbRules LoadedRules;
	bool bReady = false;
};

/** Shared absorb decision. Callers decide guarded and fresh; this applies reduction, perfect, mana and the clock reward. */
struct FAbsorbVerdict
{
	double Reduction = 0.0;
	bool bPerfect = false;
	double ManaReturned = 0.0;
	double TierClockAdvanceS = 0.0;
};

inline FAbsorbVerdict EvaluateAbsorb(const FAbsorbRules& Rules, bool bGuarded, bool bFreshWindow, EAbsorbHitKind Kind, int32 IncomingTier, double NerveRank)
{
	FAbsorbVerdict Verdict;
	if (!bGuarded)
	{
		Verdict.Reduction = Rules.ReductionOutsideArc;
		return Verdict;
	}
	if (!(bFreshWindow && !Rules.BlocksPerfect(Kind)))
	{
		Verdict.Reduction = Rules.ReductionFor(Kind);
		return Verdict;
	}
	Verdict.Reduction = Rules.PerfectReduction;
	Verdict.bPerfect = true;
	Verdict.ManaReturned = Rules.ManaForPerfect(IncomingTier, NerveRank);
	Verdict.TierClockAdvanceS = Rules.TierClockAdvanceS;
	return Verdict;
}
