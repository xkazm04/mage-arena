#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "CollarCapture.generated.h"

/**
 * T22 stills of the collar ledger: the Crack seam at the wrists right after a real perfect, the Wardstones glowing with
 * the day's Tithe right after a Tithe point, and the aftermath tablet with the Cracks and Tithe columns.
 * -MageArenaCollarCapture.
 */
UCLASS()
class MAGEARENAVR_API UCollarCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UCollarCaptureDriver();

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
		WaitBout,
		WaitCrack,
		CrackHeld,
		WaitCrackShot,
		ToTitheBout,
		WaitTithe,
		TitheHeld,
		WaitTitheShot,
		Day,
		WaitTablet,
		WaitTabletShot,
		Finish,
		Quit
	};

	void Enter(EStep Next);
	void Advance();
	void Fail(const TCHAR* Reason);
	void RequestShot(const TCHAR* FileName);
	bool ShotFinished() const;
	void RaisePalm();
	void DownOpponents(class FArenaSession& Bout) const;
	FString RepoPath(const TCHAR* Relative) const;
	class UArenaSessionSubsystem* SessionSys() const;
	class AMageArenaPawn* FindPawn() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	int32 StableFrames = 0;
	int32 Frames = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	double FightStartWall = 0.0;
	FString PendingShot;
	bool bShotOpen = false;
	FString RunId = TEXT("T22");
	FString SaveDir;
	int32 ShotCount = 0;
	int32 SeenPerfects = 0;
	double SeenTitheSim = -1.0;
	FString LastStage;
};
