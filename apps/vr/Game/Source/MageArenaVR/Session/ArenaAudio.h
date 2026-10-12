#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Session/SessionCue.h"
#include "ArenaAudio.generated.h"

class FArenaSession;
class UAudioComponent;
class USoundAttenuation;
class USoundConcurrency;
class USoundWave;
struct FArenaState;
struct FVrRuleset;

/**
 * T26, the first audio. apps/vr/data/vr/audio.json maps kernel and session events to the A05 cues (imported as
 * USoundWave assets under /Game/Audio/SFX by apps/vr/tools/import-sfx.ps1) with a per-cue gain, an attenuation, a
 * priority band, a concurrency limit and a cooldown. FArenaAudioDirector turns one frame of the bout into play requests
 * (pure, no UObject: the tests drive it with a hand-built kernel state); UArenaAudioComponent plays them, spatialised at
 * the source, or only records them (the test seam). Muted under -nullrhi, -nosound and automation tests.
 */

/** One attenuation shape from audio.json. Every distance is centimetres (Unreal units); the cutoff is hertz. */
struct FAudioAttenuationSpec
{
	FName Id;
	double FalloffCm = 3000.0;
	double InnerFraction = 0.3;
	double InnerFloorCm = 50.0;
	double FarCutoffHz = 2000.0;
	/** The inner radius: InnerFraction of FalloffCm, never below InnerFloorCm. */
	double InnerCm() const { return FMath::Max(InnerFloorCm, InnerFraction * FalloffCm); }
};

/** One cue row: the catalog row of the registry's event-priority technique, plus the asset and the gain. */
struct FAudioCueSpec
{
	FName Id;
	/** Long object path, /Game/Audio/SFX/<id>.<id>. */
	FString Asset;
	double GainDb = 0.0;
	/** 0 low, 1 normal, 2 high, 3 critical. A critical voice is never stolen. */
	int32 Priority = 1;
	bool bSpatial = true;
	FName Attenuation;
	int32 MaxVoices = 1;
	double CooldownMs = 0.0;
	bool bLoop = false;
	/** Playback length used to count a one-shot voice as live (the WAV duration from the A05 manifest). */
	double Seconds = 1.0;
};

/** One event row: a named hook in the director, the cue it plays and where the sound is placed. */
struct FAudioEventSpec
{
	FString Event;
	FName Cue;
	/** projectile, telegraph, caster, target, player, cuff, ward-hand, listener. */
	FString At;
};

struct MAGEARENAVR_API FAudioTable
{
	TMap<FName, FAudioCueSpec> Cues;
	TArray<FAudioEventSpec> Events;
	TMap<FName, FAudioAttenuationSpec> Attenuations;
	int32 MaxVoices = 12;
	/** Headroom for the whole mix, added to every cue's gain (audio.json masterDb). */
	double MasterDb = 0.0;
	double CrowdSwellDb = 0.0;
	double CrowdSwellS = 0.0;
	bool bLoaded = false;

	/** Strict: an unknown event name, an event naming a missing cue, or a cue naming a missing attenuation fails. */
	bool LoadFromString(const FString& Json, FString& OutError);
	bool LoadFromFile(const FString& Path, FString& OutError);
	const FAudioEventSpec* FindEvent(const FString& Event) const;
	const FAudioCueSpec* FindCue(FName Cue) const { return Cues.Find(Cue); }

	/** apps/vr/data/vr/audio.json, loaded once per process. bLoaded false (and an error logged) when it fails. */
	static const FAudioTable& Default();
	static FString DefaultPath();
	/** The closed vocabulary of event names the director raises. audio.json may map each at most once. */
	static const TArray<FString>& KnownEvents();
};

enum class EAudioRequestKind : uint8
{
	OneShot,
	LoopStart,
	LoopStop,
	/** Crowd swell: GainDb above the loop's own gain for Seconds. */
	Swell,
};

struct FAudioPlayRequest
{
	EAudioRequestKind Kind = EAudioRequestKind::OneShot;
	FName Cue;
	FString Event;
	FVector LocationCm = FVector::ZeroVector;
	/** A projectile id the voice follows while it flies, or -1. */
	int32 FollowId = -1;
	/** The actor or activation the request is about (for the tests and the capture log), or -1. */
	int32 SourceId = -1;
	double AtS = 0.0;
	double GainDb = 0.0;
};

/** One frame of the bout, as the director reads it. Tests fill it by hand. */
struct FAudioFrame
{
	const FArenaState* State = nullptr;
	const FVrRuleset* Rules = nullptr;
	int32 PlayerId = 0;
	/** Seconds on the clock the voices are counted on (sim seconds in the tests, world seconds in the game). */
	double NowS = 0.0;
	bool bActive = false;
	bool bPaused = false;
	bool bSigilDrawing = false;
	bool bPlanted = false;
	/** All session cues so far; the director keeps its own cursor. */
	const TArray<FSessionCue>* SessionCues = nullptr;
	/** Kernel metres to world centimetres (the session's KernelToUnrealCm). Identity x100 when unset. */
	TFunction<FVector(double X, double Y)> ToCm;
	FVector CuffCm = FVector::ZeroVector;
	FVector WardHandCm = FVector::ZeroVector;
	FVector ListenerCm = FVector::ZeroVector;
};

/** Turns frames into play requests: event cursors, threat detection, concurrency, cooldown and the global cap. */
class MAGEARENAVR_API FArenaAudioDirector
{
public:
	explicit FArenaAudioDirector(const FAudioTable& InTable) : Table(&InTable) {}

	/** Requests for this frame, in order. Dropped requests (cap or cooldown) are counted, not returned. */
	TArray<FAudioPlayRequest> Step(const FAudioFrame& Frame);
	/** Forget every cursor and voice (a new bout's state). */
	void Reset();

	int32 LiveVoices(FName Cue, double NowS) const;
	int32 LiveVoicesTotal(double NowS) const;
	int32 GetDropped() const { return Dropped; }
	int32 GetPeakVoices() const { return PeakVoices; }
	/** Live positions of followed projectiles this frame (id -> cm), for the component to move their voices. */
	const TMap<int32, FVector>& GetFollow() const { return Follow; }

private:
	struct FVoice
	{
		FName Cue;
		double StartS = 0.0;
		double EndS = 0.0;
		int32 Priority = 1;
		bool bLoop = false;
	};

	void Raise(const FString& Event, const FVector& At, int32 SourceId, int32 FollowId, double NowS, TArray<FAudioPlayRequest>& Out);
	void Loop(FName Cue, const FString& Event, bool bOn, const FVector& At, double NowS, TArray<FAudioPlayRequest>& Out);
	bool Admit(const FAudioCueSpec& Cue, double NowS);
	void Expire(double NowS);

	const FAudioTable* Table = nullptr;
	int32 EventCursor = 0;
	int32 LastTick = -1;
	int32 SessionCursor = 0;
	TSet<int32> SeenProjectiles;
	TSet<int32> SeenTelegraphs;
	TSet<int32> SeenPendings;
	TArray<FVoice> Voices;
	TMap<FName, double> LastStart;
	TSet<FName> LoopsOn;
	bool bWasAbsorb = false;
	bool bWasPaused = false;
	int32 LastFlow = 0;
	int32 LastCrests = 0;
	int32 Dropped = 0;
	int32 PeakVoices = 0;
	TMap<int32, FVector> Follow;
};

/** Plays the director's requests on the presentation actor. */
UCLASS()
class MAGEARENAVR_API UArenaAudioComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UArenaAudioComponent();

	void Sync(const FArenaSession& Session, bool bPaused, const FVector& CameraCm, const FRotator& CameraRot);

	/** Test seam: requests are recorded and never played. */
	void SetRecordOnly(bool bInRecordOnly) { bRecordOnly = bInRecordOnly; }
	const TArray<FAudioPlayRequest>& GetRecorded() const { return Recorded; }
	/** Plays (or records) a batch of requests. Public for the tests. */
	void Handle(const TArray<FAudioPlayRequest>& Requests, double NowS);
	bool IsMuted() const { return bMuted; }
	/** Muted under -nullrhi, -nosound, automation tests, or with no world or no audio device. */
	static bool ShouldMute(const UWorld* World);
	/** Every request so far (sim seconds, cue, event, source, position), for the capture's cue log. */
	const TArray<FAudioPlayRequest>& GetHistory() const { return History; }

private:
	USoundWave* WaveFor(FName Cue);
	USoundAttenuation* AttenuationFor(FName Id);
	USoundConcurrency* ConcurrencyFor(const struct FAudioCueSpec& Cue);
	void PlayOne(const FAudioPlayRequest& Request);
	void SetLoop(const FAudioPlayRequest& Request, bool bOn);

	TUniquePtr<FArenaAudioDirector> Director;
	bool bRecordOnly = false;
	bool bMuted = false;
	bool bMuteDecided = false;
	TArray<FAudioPlayRequest> Recorded;
	TArray<FAudioPlayRequest> History;

	UPROPERTY()
	TMap<FName, TObjectPtr<USoundWave>> Waves;

	UPROPERTY()
	TMap<FName, TObjectPtr<USoundAttenuation>> AttenuationObjects;

	UPROPERTY()
	TMap<FName, TObjectPtr<USoundConcurrency>> ConcurrencyObjects;

	UPROPERTY()
	TMap<FName, TObjectPtr<UAudioComponent>> Loops;

	UPROPERTY()
	TMap<int32, TObjectPtr<UAudioComponent>> Followers;

	double SwellUntil = -1.0;
	double SwellDb = 0.0;
	int32 LastStateTick = -1;
};
