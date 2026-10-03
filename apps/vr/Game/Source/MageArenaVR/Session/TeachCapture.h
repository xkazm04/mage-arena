#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "TeachCapture.generated.h"

/** Stills of the cold start, the teach, the pause, and the 3-2-1 count. */
UCLASS()
class MAGEARENAVR_API UTeachCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UTeachCaptureDriver();

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
		WaitCold,
		WaitColdShot,
		WaitPerfect,
		WaitPerfectShot,
		WaitSigil,
		WaitSigilShot,
		WaitBlink,
		WaitBlinkShot,
		WaitPause,
		WaitPauseShot,
		WaitCount,
		WaitCountShot,
		Finish,
		Quit
	};

	void Enter(EStep Next);
	void Advance();
	void Fail(const TCHAR* Reason);
	bool RequestShot(const TCHAR* FileName);
	bool ShotFinished() const;
	// True once the prompt has been the expected line long enough for the widget to redraw.
	bool PromptSettled(const FString& Prompt, const TCHAR* Expected);
	FString RepoPath(const TCHAR* Relative) const;
	class UArenaSessionSubsystem* SessionSys() const;
	class AMageArenaPawn* FindPawn() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	bool bCountClip = false;
	int32 StableFrames = 0;
	int32 PromptHold = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	FString PendingShot;
	bool bShotOpen = false;
	FString RunId = TEXT("T13");
	int32 ShotCount = 0;
};
