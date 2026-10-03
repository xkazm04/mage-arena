#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "Wave1Capture.generated.h"

/** Records the scripted Wave 1 at the moments the card names. Pauses the kernel for each still. */
UCLASS()
class MAGEARENAVR_API UWave1CaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UWave1CaptureDriver();

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
		WaitSoldiers,
		ShotSoldiers,
		WaitSoldiersShot,
		WaitWard,
		ShotWard,
		WaitWardShot,
		WaitSigil,
		ShotSigil,
		WaitSigilShot,
		WaitBlink,
		ShotBlink,
		WaitBlinkShot,
		WaitVictory,
		ShotVictory,
		WaitVictoryShot,
		WriteBudget,
		Quit
	};

	void Enter(EStep Next);
	void Advance();
	bool RequestShot(const TCHAR* FileName, bool bSequence);
	bool ShotFinished() const;
	void SampleBudget();
	void WriteBudgetFile();
	FString RepoPath(const TCHAR* Relative) const;
	class UArenaSessionSubsystem* SessionSys() const;
	class AMageArenaPawn* FindPawn() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	int32 StableFrames = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	double LastSequenceWall = 0.0;
	int32 SequenceIndex = 0;
	FString PendingShot;
	bool bShotOpen = false;

	TArray<int32> DrawCalls;
	TArray<int32> MeasuredTriangles;
	TArray<double> FrameMs;
	TArray<int32> EnemiesOnScreen;
	TArray<FString> ShotNames;
	// -MageArenaRun= on the command line. T08 keeps the original five stills.
	FString RunId = TEXT("T08");
};
