#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "DayCapture.generated.h"

/**
 * T21 stills of the Tiro day: Septima's ritual line, the offer line with both palms raised, the Bout 3 intro (Brennic),
 * the Bout 4 intro (Corvo), and the aftermath tablet. -MageArenaDayCapture.
 */
UCLASS()
class MAGEARENAVR_API UDayCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UDayCaptureDriver();

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
		WaitRitual,
		WaitSeptimaShot,
		WaitOfferLine,
		WaitPalmsUp,
		WaitPalmsShot,
		Day,
		WaitIntroShot,
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
	FString RepoPath(const TCHAR* Relative) const;
	class UArenaSessionSubsystem* SessionSys() const;
	class AMageArenaPawn* FindPawn() const;
	class UHandInputSubsystem* HandsSys() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	int32 StableFrames = 0;
	int32 Frames = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	double FightStartWall = 0.0;
	FString PendingShot;
	bool bShotOpen = false;
	FString RunId = TEXT("T21");
	FString SaveDir;
	int32 ShotCount = 0;
	int32 IntroShotWave = -1;
	FString LastStage;
};
