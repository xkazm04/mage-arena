#pragma once

#include "CoreMinimal.h"
#include "Gestures/BlinkDetector.h"
#include "Greybox/ArenaLayout.h"
#include "Kernel/Games.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArenaSession.generated.h"

class AMageArenaPawn;
class ASessionPresentation;
class UBlinkDetectorSubsystem;
class UHandInputSubsystem;
class USigilRecognizerSubsystem;
class UWardDetectorSubsystem;
class UWave1CaptureDriver;

/**
 * Owns one Tiro bout. Gestures become kernel inputs. Kernel actors stay in kernel
 * metres; presentation reads them. The player is pinned to the active pad on every
 * tick. There is no move input. Blink is the only position change: one roll edge
 * (i-frames and the stamina cost stay the pinned roll values) and the pin moves
 * to the target pad.
 */
class FArenaSession
{
public:
	FArenaSession();
	~FArenaSession();

	void Bind(
		UHandInputSubsystem* InHands,
		USigilRecognizerSubsystem* InSigils,
		UWardDetectorSubsystem* InWards,
		UBlinkDetectorSubsystem* InBlinks);
	void Unbind();

	/** Wave index 0, Rotation composition, human player. Clears pause. */
	bool Start(uint32 Seed);
	void SetScripted(bool bInScripted);
	void SetPaused(bool bInPaused);
	void TogglePause();

	bool IsPaused() const { return bPaused; }
	bool IsRunning() const { return bRunning; }
	bool IsScripted() const { return bScripted; }

	/**
	 * bStepHands is for tests, where the clip player has no world and does not tick.
	 * In -game the clip player ticks itself; the session must not step it again.
	 */
	void Advance(double DeltaSeconds, bool bStepHands);
	void SetViewAim(const FSimVec& Aim);

	const FGames& GetGames() const { return Games; }
	const TArray<FString>& GetChain() const { return Chain; }
	const FArenaLayout& GetLayout() const { return Layout; }
	bool HasLayout() const { return bHasLayout; }
	int32 GetActivePad() const { return ActivePad; }
	double GetSimSeconds() const;
	int32 GetBlinkAccepts() const { return BlinkAccepts; }
	bool WasSigilHit() const { return bSawSigilHit; }
	bool WasWardOnStone() const { return bSawWardOnStone; }
	bool ConsumeCameraPad(int32& OutPad);

	FVector KernelToUnrealCm(const FSimVec& Pos) const;
	FSimVec PadKernel(int32 PadIndex) const;

private:
	void HandleSigil(FName Line, float Score, double LatencyMs);
	void HandleWardRaised(double OnsetTime, FVector Facing);
	void HandleWardLowered();
	void HandleBlink(const FBlinkEvent& Event);
	void HandleBolt(const FBoltFlickEvent& Event);
	void StepKernel();
	void DecideScript();
	void DrainEvents(const FActor* PlayerBefore);
	void Note(const FString& Line);
	void BeginCast(int32 Slot, const TCHAR* Gesture);
	void PlayAction(FName Action);
	FString ClipAction() const;
	bool ClipPlaying() const;
	FSimVec ComputeAim(int32 Slot) const;
	void ScanThreats(double& MeleeEta, double& ProjectileEta) const;
	/** Stamina, recovery and a blink clip already playing. No side effects. */
	bool CanBlink(const FActor& Player) const;
	bool TryScriptBlink();
	/**
	 * Seconds until the next steel hit, including attacks that have not been scheduled yet.
	 * bMeleeOnly ignores slingers. A conscript in backoff is timed through the walk back in.
	 */
	double SoonestThreat(double MeleeEta, double ProjectileEta, bool bMeleeOnly) const;
	void SeatOnActivePad(FActor& Player) const;
	void CheckEnd();

	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	FDelegateHandle SigilHandle;
	FDelegateHandle WardRaisedHandle;
	FDelegateHandle WardLoweredHandle;
	FDelegateHandle BlinkHandle;
	FDelegateHandle BoltHandle;

	FGames Games;
	FArenaLayout Layout;
	bool bHasLayout = false;
	bool bRunning = false;
	bool bPaused = false;
	bool bScripted = false;
	bool bLoggedEnd = false;
	double Accumulator = 0.0;

	FSimVec ViewAim{IdleAimX, IdleAimY};
	FSimVec AimPoint{IdleAimX, IdleAimY};
	bool bAbsorb = false;
	bool bSuppressWard = false;
	bool bBlinkQueued = false;
	int32 QueuedPad = 1;
	bool bCastPulse = false;
	int32 CastSlot = 0;
	int32 CastExpireTick = 0;
	int32 CastsBeforePulse = 0;
	bool bBoltFlicked = false;
	double BoltFlickSim = -1.0;
	bool bLastCastSigil = false;

	int32 ActivePad = 1;
	int32 BlinkAccepts = 0;
	bool bCameraDirty = false;
	int32 CameraPad = 1;

	bool bWardStarted = false;
	bool bWardReleased = false;
	double WardReleaseSim = 0.0;
	bool bTideStarted = false;
	int32 SideToggle = 0;
	bool bSawSigilHit = false;
	bool bSawWardOnStone = false;
	int32 EventCursor = 0;
	bool bClipWasPlaying = false;

	TArray<FString> Chain;
};

/** Steps the bout on the game world and pushes presentation. Auto-starts in -game. */
UCLASS()
class MAGEARENAVR_API UArenaSessionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	bool Start(uint32 Seed, bool bScripted);
	void TogglePause();
	FArenaSession& GetSession() { return Session; }
	const FArenaSession& GetSession() const { return Session; }
	ASessionPresentation* GetPresentation() const { return Presentation; }

private:
	void ApplyCamera();
	void ApplyViewAim();

	FArenaSession Session;
	bool bRunning = false;
	bool bBegan = false;

	UPROPERTY()
	TObjectPtr<ASessionPresentation> Presentation;

	UPROPERTY()
	TObjectPtr<UWave1CaptureDriver> Capture;
};
