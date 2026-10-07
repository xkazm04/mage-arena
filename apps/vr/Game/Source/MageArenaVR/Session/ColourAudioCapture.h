#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "ColourAudioCapture.generated.h"

/**
 * T26 capture. -MageArenaColourAudioCapture with -MageArenaT26Mode=stills or =audio, run by
 * apps/vr/tools/capture-colour-audio.ps1.
 * stills: a hound ember in flight (creatures bout), Brennic's fire bolt and the player's Water bolt in one frame (the
 *   Brennic semifinal), and Lio's Squall in flight (the air final). runs/<run>/shots/.
 * audio: the first 30 s of the Brennic semifinal with the scripted seat, recorded from the master submix to
 *   runs/<run>/audio/brennic-opening.wav, with every play request logged (wall time from the recording start, sim time,
 *   cue, event, source, distance) to runs/<run>/audio/brennic-opening-cues.tsv.
 */
UCLASS()
class MAGEARENAVR_API UColourAudioCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UColourAudioCaptureDriver();

	void Start();

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual void BeginDestroy() override;

private:
	enum class EStep : uint8
	{
		WaitStable,
		// stills
		StartCreatures,
		WaitEmber,
		StartDuel,
		WaitBolts,
		StartAir,
		WaitLio,
		WaitSquall,
		// audio
		StartRecording,
		Recording,
		WaitWav,
		Finish,
		Quit
	};

	void Enter(EStep Next);
	void Advance();
	void Fail(const FString& Reason);
	void RequestShot(const TCHAR* FileName);
	bool ShotFinished() const;
	FString RepoPath(const FString& Relative) const;
	class UArenaSessionSubsystem* SessionSys() const;
	class AMageArenaPawn* FindPawn() const;
	void LogNewCues();

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	bool bAudio = false;
	int32 StableFrames = 0;
	int32 Frames = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	double RecordStartWall = 0.0;
	/** Seconds of game time since the recording started (the audio clock: -deterministicaudio renders each frame's delta). */
	double RecordedS = 0.0;
	double RecordStartClock = 0.0;
	FString PendingShot;
	bool bShotOpen = false;
	FString RunId = TEXT("T26");
	int32 ShotCount = 0;
	int32 CuesLogged = 0;
	FString CueLog;
	int64 LastWavSize = -1;
	int32 WavStableFrames = 0;
	EStep AfterShot = EStep::Finish;
};
