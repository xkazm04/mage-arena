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

/**
 * One line of a beat or a scene: its text (authored in C2, shown as is), how long it stands, and (T27) the cue id that
 * joins it to the cue sheet (C2 section 9) and its speaker. Cue and speaker are empty where slice.json has null.
 */
struct FStoryLine
{
	FString Text;
	double DwellS = 0.0;
	FString Cue;
	FString Speaker;
};

/**
 * T27: the lines of a beat (C2 section 5). A beat with top-level lines has one entry with no key; intro-w4 has one per
 * rival of the final (lio, the corvo cut fallback), closing one per result of the final.
 */
struct FStoryBeatLines
{
	FString Id;
	FString Rival;
	/** closing's requiresFinalWon: 1 won, 0 lost, -1 either. */
	int32 FinalWon = -1;
	TArray<FStoryLine> Lines;
};

/** T27: one beat of slice.json (prologue, ritual, the intros, the tablet, the closing card). floorS <= naturalS <= maxS. */
struct FStoryBeat
{
	FString Id;
	FString Phase;
	/** The pinned Tiro wave n of an intro beat; 0 otherwise. */
	int32 Wave = 0;
	double FloorS = 0.0;
	double NaturalS = 0.0;
	double MaxS = 0.0;
	TArray<FStoryBeatLines> Variants;

	/** The lines for this rival of the final and this result: the first entry whose keys all match, else the first. */
	const TArray<FStoryLine>* SelectLines(const FString& Rival, bool bFinalWon) const;
};

/** T27: a rival's phase-break taunt (C2 section 5). Voice only: no text is shown mid-duel (C2 section 7). */
struct FStoryTaunt
{
	FString Rival;
	double AtHpFraction = 0.0;
	FStoryLine Line;
};

/** T27: one place a cue can play, for the cue-sheet check (MageArena.Voice.TriggerPerRow). */
struct FStoryVoiceTrigger
{
	/** "<beat or scene id>#<line, 1-based>" or "taunt.<rival>#<line>": the FVoiceCue event the session raises. */
	FString Event;
	FString Text;
	FString Speaker;
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
 * section 8). This reads the intermission scenes, each beat's floor (the earliest the skip gesture works) and natural
 * dwell, and (T27) every beat's lines and the rivals' taunts with their cue ids. Strict: a missing, unreadable,
 * non-version-1 or malformed file loads nothing (never half-read), and the day then plays one line per beat from
 * strings.json, as it did before the slice existed.
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
	/** T27: a beat's naturalS, the dwell of a first-time player who reads every line (C2 section 4); 0 without it. */
	double BeatNaturalS(const TCHAR* BeatId) const;
	/** T27: a beat by id, or null (also when the slice is not loaded). */
	const FStoryBeat* FindBeat(const TCHAR* BeatId) const;
	/** T27: the intro beat of the pinned Tiro wave n, or null. */
	const FStoryBeat* FindIntroBeat(int32 WaveN) const;
	/** T27: the taunt of this rival whose atHpFraction is the phase threshold that broke, or null. */
	const FStoryTaunt* FindTaunt(const FString& Rival, double AtHpFraction) const;
	/**
	 * T27: every cue the slice can raise, by cue id, with each place it plays. A line shared by the variants of one beat
	 * or scene at the same position (OPH-05, OPH-10) is one event; the session raises the same event names.
	 */
	TMap<FString, TArray<FStoryVoiceTrigger>> VoiceTriggers() const;
	/** T27: the event name of line Index (0-based) of a beat or scene, "<id>#<Index + 1>". */
	static FString LineEvent(const FString& Id, int32 Index);
	/** T27: the lines a scene plays in order: the selected variant's, then the ones every variant shares. */
	static TArray<const FStoryLine*> SceneLines(const FStoryScene& Scene, int32 Variant);

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
	TArray<FStoryBeat> Beats;
	TArray<FStoryTaunt> Taunts;
};
