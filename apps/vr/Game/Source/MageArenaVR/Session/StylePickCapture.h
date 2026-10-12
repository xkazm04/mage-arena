#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "StylePickCapture.generated.h"

UCLASS()
class MAGEARENAVR_API UStylePickCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UStylePickCaptureDriver();

	void Start();

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual void BeginDestroy() override;

private:
	bool RequestShot(const TCHAR* FileName, bool bSequence);
	bool ShotFinished() const;
	class UArenaSessionSubsystem* SessionSys() const;

	bool bRunning = false;
	double RunStartWall = 0.0;
	double LastSequenceWall = 0.0;
	int32 SequenceIndex = 0;
	FString PendingShot;
	bool bShotOpen = false;

	int32 LastTick = -1;
	FString LastEventKind;
	int32 LastEventActorId = 0;
	
	FString StyleId = TEXT("greybox");
	class UTextRenderComponent* OverlayText = nullptr;
};
