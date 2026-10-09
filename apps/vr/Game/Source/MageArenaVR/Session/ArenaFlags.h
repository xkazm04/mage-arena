#pragma once

#include "CoreMinimal.h"

/**
 * T21: the session-owned arena flag map (docs/campaign/04-vr-campaign-design.md section 4), in memory and as a small
 * JSON file next to bout.txt. Values are bools or numbers. After each Tiro bout the session writes
 * arena.tiro.1.w<n>.won, .attempts, .perfects and .time. No consumer yet: T22 (collar ledger) and T28 (scenes) read it.
 *
 * File shape: { "version": 1, "flags": { "<key>": true | <number>, ... } }. A missing file is an empty map. A file
 * that is not that shape is ignored with a warning (the map stays empty), never half-read.
 */
class MAGEARENAVR_API FArenaFlags
{
public:
	void Reset();
	void SetBool(const FString& Key, bool bValue);
	void SetNumber(const FString& Key, double Value);
	bool GetBool(const FString& Key, bool& bOut) const;
	bool GetNumber(const FString& Key, double& Out) const;
	bool Has(const FString& Key) const;
	int32 Num() const { return Values.Num(); }
	/** Keys in sorted order. */
	TArray<FString> Keys() const;

	bool Save(const FString& Path) const;
	/** False when the file exists but is not a valid flag file. A missing file loads as empty and returns true. */
	bool Load(const FString& Path);

	/** arena.tiro.1.w<WaveN>.<Field>. WaveN is the pinned wave n (1-based). */
	static FString TiroKey(int32 WaveN, const TCHAR* Field);

private:
	struct FValue
	{
		bool bIsBool = false;
		bool bBool = false;
		double Number = 0.0;
	};
	TMap<FString, FValue> Values;
};
