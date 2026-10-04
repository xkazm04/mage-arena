#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "SettingsCapture.generated.h"

/** Stills of the comfort stones, a left-hand sigil, and narrow-FOV Wave 1. */
UCLASS()
class MAGEARENAVR_API USettingsCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	USettingsCaptureDriver();

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
		WaitToggles,
		WaitTogglesShot,
		WaitSigil,
		WaitSigilShot,
		WaitWave,
		WaitWaveShot,
		Finish,
		Quit
	};

	void Enter(EStep Next);
	void Advance();
	void Fail(const TCHAR* Reason);
	bool RequestShot(const TCHAR* FileName);
	bool ShotFinished() const;
	bool PromptSettled(const FString& Prompt, const TCHAR* Expected);
	FString RepoPath(const TCHAR* Relative) const;
	class UArenaSessionSubsystem* SessionSys() const;
	class AMageArenaPawn* FindPawn() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	bool bSigilStarted = false;
	int32 StableFrames = 0;
	int32 PromptHold = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	FString PendingShot;
	bool bShotOpen = false;
	FString RunId = TEXT("T14");
	int32 ShotCount = 0;
};
