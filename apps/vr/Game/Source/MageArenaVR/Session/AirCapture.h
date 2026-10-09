#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "AirCapture.generated.h"

/**
 * T23 stills: Lio, the Tiro final's air mage, on the floor; a Veering Bolt's arc telegraph; a Squall volley in flight;
 * Air Form active; the Tempest Lance line telegraph. -MageArenaAirCapture. Stills in runs/<run>/shots/.
 */
UCLASS()
class MAGEARENAVR_API UAirCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UAirCaptureDriver();

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
		WaitLio,
		Grant,
		WaitCast,
		WaitShot,
		Finish,
		Quit
	};

	void Enter(EStep Next);
	void Advance();
	void Fail(const TCHAR* Reason);
	void RequestShot(const TCHAR* FileName);
	bool ShotFinished() const;
	FString RepoPath(const TCHAR* Relative) const;
	class UArenaSessionSubsystem* SessionSys() const;
	class AMageArenaPawn* FindPawn() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	int32 StableFrames = 0;
	int32 Frames = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	FString PendingShot;
	bool bShotOpen = false;
	FString RunId = TEXT("T23");
	int32 ShotCount = 0;
	// 0 veer, 1 squall, 2 form, 3 tempest.
	int32 Stage = 0;
	int32 GrantTick = 0;
	int32 GrantActivation = -1;
};
