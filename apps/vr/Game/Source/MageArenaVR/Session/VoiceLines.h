#pragma once

#include "CoreMinimal.h"

/**
 * T27: the C2 voice lines (docs/campaign/05-c2-slice-story-bible-and-script.md sections 7-9). The session raises a voice
 * cue when a slice line with a cue id starts to stand (the prologue's, the ritual's, a bout intro's, an intermission
 * scene's, the closing card's) and when a bound rival breaks a phase (the taunts). UArenaAudioComponent plays the cue's
 * asset, /Game/Audio/Voice/<cue>, from the speaker's place, or skips it silently when no asset was imported: voice is
 * additive (C2 section 8), the day is paced by the text, and nothing ever waits for a voice. No runtime service is
 * called; the WAVs are pre-rendered and imported by apps/vr/tools/import-voice.ps1.
 */
struct FVoiceCue
{
	/** The cue id, PRO-01 ... LIO-02 (C2 section 9). */
	FString Cue;
	/** slice.json speaker id (septima, venno, ophel, brennic, lio), empty for the unvoiced prologue tableau. */
	FString Speaker;
	/** The trigger: "<beat or scene id>#<line, 1-based>" or "taunt.<rival>#<line>" (FStorySlice::VoiceTriggers). */
	FString Event;
	/** The day phase the line stood in (prologue, ritual, intro, intermission, aftermath, closed, active). */
	FString Phase;
	/** Seconds into that phase when the line started (the scene's clock in an intermission). */
	double PhaseS = 0.0;
};

/** One row of apps/vr/art/audio/voice/manifest.json, the cue sheet of C2 section 9. */
struct FVoiceManifestRow
{
	FString Id;
	FString Text;
	FString Speaker;
	int32 Words = 0;
	/** The WAV's path relative to the voice folder. */
	FString File;
};

struct MAGEARENAVR_API FVoiceManifest
{
	TArray<FVoiceManifestRow> Rows;

	/** Strict: version 1, a cues array, every row with an id, text and file, and no id twice. */
	bool LoadFromFile(const FString& Path, FString& OutError);
	const FVoiceManifestRow* Find(const FString& Id) const;

	/** apps/vr/art/audio/voice/manifest.json. */
	static FString DefaultPath();
	/** apps/vr/art/audio/voice, where each row's file lives. */
	static FString VoiceDir();
};

namespace VoiceLines
{
/** The imported asset's long object path, /Game/Audio/Voice/<cue>.<cue>. */
MAGEARENAVR_API FString AssetPath(const FString& Cue);

/**
 * Where a speaker's line comes from (C2 section 7, every speaker inside the front +/-70 degrees). Septima stands at the
 * dais rail's left and Ophel at its right, beside the rail prompt (220 cm ahead of the seat); Venno at the grille above
 * the gate, at the far end of the oval; a rival at RivalCm. Greybox anchors: no rail, grille or gate mesh exists yet.
 * False for no speaker (the prologue tableau): the line plays positionless.
 */
MAGEARENAVR_API bool SpeakerLocationCm(const FString& Speaker, const FVector& EyeCm, const FVector& RivalCm, FVector& OutCm);
}
