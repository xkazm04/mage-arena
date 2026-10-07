#pragma once

#include "CoreMinimal.h"

struct FArenaEvent;
struct FSpell;

/**
 * T22: the collar ledger (DECISIONS 2026-10-07, "v2 vision accepted": the collar economy). Cracks are clean play,
 * Tithe is magic spilled into the sand. Weights come from apps/vr/data/vr/collar.json (proposals, owner sign-off
 * pending). The ledger reads kernel events and the player's Crest counter; it never writes the kernel.
 */
struct FCollarWeights
{
	int32 Perfect = 0;
	int32 Reflect = 0;
	int32 ReflectHitsCaster = 0;
	int32 UnblockableBlinked = 0;
	int32 AreaCastTier3Plus = 0;
	int32 MissedCast = 0;
	int32 Crest = 0;
	/** Presentation: the day's Tithe at which the Wardstones glow fully. */
	double WardstoneFullTithe = 0.0;
};

/** Strict: version 1, known keys only, every number a non-negative integer with its _<key> line. */
MAGEARENAVR_API bool ParseCollarWeights(const FString& Text, FCollarWeights& Out, FString& Error);
MAGEARENAVR_API bool LoadCollarWeights(FCollarWeights& Out, FString& Error);
MAGEARENAVR_API FString CollarWeightsPath();

enum class ECollarSource : uint8
{
	Perfect,
	Reflect,
	ReflectHitsCaster,
	UnblockableBlinked,
	AreaCastTier3Plus,
	MissedCast,
	Crest,
	Count
};

class MAGEARENAVR_API FCollarLedger
{
public:
	void SetWeights(const FCollarWeights& InWeights) { Weights = InWeights; }
	const FCollarWeights& GetWeights() const { return Weights; }

	/** A tier III or IV spell whose shape is an area or several projectiles. */
	static bool IsAreaCastTier3Plus(const FSpell& Spell);
	static bool IsCrack(ECollarSource Source);
	static const TCHAR* SourceName(ECollarSource Source);

	/** Clears the bout tally (a new bout or a retry). Lifetime totals stay. CrestsNow is the player's Crest counter. */
	void BeginBout(int32 CrestsNow = 0);
	/**
	 * One kernel event. Only events about the player count. CastSpell is the spell of the player's cast this event
	 * names (null for any other event, or when it is unknown).
	 */
	void Observe(const FArenaEvent& Event, int32 PlayerId, const FSpell* CastSpell, double SimSeconds);
	/** The player's kernel Crest counter after a step. Each rise is one Crest spent. */
	void ObserveCrests(int32 Crests, double SimSeconds);
	void Add(ECollarSource Source, double SimSeconds);

	int32 GetBoutCracks() const { return BoutCracks; }
	int32 GetBoutTithe() const { return BoutTithe; }
	int32 GetBoutCount(ECollarSource Source) const { return BoutCounts[static_cast<int32>(Source)]; }
	/** "perfect 3 reflect 0 ..." for the logs. */
	FString Breakdown() const;

	/** Adds the bout tally to the lifetime totals (once per bout end). */
	void CommitBout();
	int32 GetLifetimeCracks() const { return LifetimeCracks; }
	int32 GetLifetimeTithe() const { return LifetimeTithe; }
	void SetLifetime(int32 Cracks, int32 Tithe);

	/** Sim seconds of the latest Crack and Tithe point in this bout, or -1. Presentation flashes from these. */
	double GetLastCrackSim() const { return LastCrackSim; }
	double GetLastTitheSim() const { return LastTitheSim; }

	/** File shape: { "version": 1, "lifetimeCracks": n, "lifetimeTithe": n }. */
	bool SaveLifetime(const FString& Path) const;
	/**
	 * A missing file is a new collar (0, 0) and returns true. A corrupt file is treated like a corrupt save: a warning,
	 * the totals start at 0, and false.
	 */
	bool LoadLifetime(const FString& Path);

private:
	FCollarWeights Weights;
	int32 BoutCounts[static_cast<int32>(ECollarSource::Count)] = {};
	int32 BoutCracks = 0;
	int32 BoutTithe = 0;
	int32 LifetimeCracks = 0;
	int32 LifetimeTithe = 0;
	int32 LastCrests = 0;
	double LastCrackSim = -1.0;
	double LastTitheSim = -1.0;
};
