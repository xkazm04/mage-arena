#pragma once

#include "CoreMinimal.h"

class FArenaFlags;

/**
 * T28: one clause of a scene variant's when, the clause shape of docs/campaign/04-vr-campaign-design.md section 4:
 * { flag, op (eq|gte|lte), value (bool or number) }. A flag that is not set, or is set with the other type, does not
 * match; it is never an error.
 */
struct FStoryWhen
{
	FString Flag;
	FString Op;
	bool bIsBool = false;
	bool bBool = false;
	double Number = 0.0;

	bool Matches(const FArenaFlags& Flags) const;
};

/** One line of a scene: its text (authored in C2, shown as is) and how long it stands. */
struct FStoryLine
{
	FString Text;
	double DwellS = 0.0;
};

/** A scene variant. Every clause must match (when, then andWhen); no clause is when: null, always. */
struct FStoryVariant
{
	FString Id;
	TArray<FStoryWhen> When;
	TArray<FStoryLine> Lines;
};

/**
 * One intermission scene of slice.json (C2 section 6): the variant lines, the lines every variant shares, and at most
 * one optional choice on the three stones. floorS <= naturalS <= maxS <= 20.
 */
struct FStoryScene
{
	FString Id;
	/** 1-based: arena.intermission.<Intermission>.choice. */
	int32 Intermission = 0;
	/** The pinned Tiro wave n the scene follows. */
	int32 AfterWave = 0;
	double FloorS = 0.0;
	double NaturalS = 0.0;
	double MaxS = 0.0;
	TArray<FStoryVariant> Variants;
	TArray<FStoryLine> Always;
	/** The flag a stone pick writes (the stone index); empty when the scene has no choice. */
	FString Writes;
	/** The scene wraps the T20 stone pick (PlayerPreset.Pick) instead of offering stones of its own. */
	bool bKeepsExistingPick = false;
	/** Stone labels, 0 left, 1 centre, 2 right: a strings.json key or the label text. Empty without a choice. */
	TArray<FString> Stones;

	/** The first variant whose clauses all match, in file order; INDEX_NONE when none does. */
	int32 SelectVariant(const FArenaFlags& Flags) const;
};

/**
 * T28: apps/vr/data/vr/story/slice.json, the C2 story content (docs/campaign/05-c2-slice-story-bible-and-script.md
 * section 8). This reads the intermission scenes and each beat's floor (the earliest the skip gesture works). Strict:
 * a missing, unreadable, non-version-1 or malformed file loads nothing (never half-read), and the day then plays one
 * line per beat from strings.json, as it did before the slice existed.
 */
class MAGEARENAVR_API FStorySlice
{
public:
	/** False, with the reason in OutError, when the file is missing or not a valid slice. The slice is then empty. */
	bool Load(const FString& Path, FString& OutError);
	void Reset();
	bool IsLoaded() const { return bLoaded; }
	const TArray<FStoryScene>& GetScenes() const { return Scenes; }
	/** The scene after the pinned Tiro wave n, or null. */
	const FStoryScene* FindSceneAfterWave(int32 WaveN) const;
	/** A beat's floorS by beat id (prologue, closing, ...); 0 when the slice is not loaded or has no such beat. */
	double BeatFloorS(const TCHAR* BeatId) const;

	static FString DefaultFilePath();
	/** Whitespace-separated words. */
	static int32 CountWords(const FString& Text);
	/** The longest a scene may hold the day (T28: 20 s). */
	static constexpr double MaxSceneS = 20.0;
	/** The most words on a stone label (C2 section 8). */
	static constexpr int32 MaxLabelWords = 5;

private:
	bool bLoaded = false;
	TArray<FStoryScene> Scenes;
	TMap<FString, double> BeatFloors;
};
