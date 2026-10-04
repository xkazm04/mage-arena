#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "CreaturesCapture.generated.h"

UCLASS()
class MAGEARENAVR_API UCreaturesCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UCreaturesCaptureDriver();

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
		WaitHoundsApproaching,
		WaitHoundsApproachingShot,
		WaitDeathBurst,
		WaitDeathBurstShot,
		WaitTonguePull,
		WaitTonguePullShot,
		WaitIntermission,
		WaitIntermissionShot,
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
	int32 StableFrames = 0;
	int32 PromptHold = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	FString PendingShot;
	bool bShotOpen = false;
	FString RunId = TEXT("T15");
	int32 ShotCount = 0;
};
