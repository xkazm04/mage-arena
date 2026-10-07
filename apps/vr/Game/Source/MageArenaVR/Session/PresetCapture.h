#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "PresetCapture.generated.h"

/**
 * T20 stills: the intermission stones with the Tide Orb IV pick, the left stone glowing during the hold, a reflected
 * fire bolt flying back at the Fire mage, and a Leviathan Orb in flight with its drawn path. -MageArenaPresetCapture.
 */
UCLASS()
class MAGEARENAVR_API UPresetCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UPresetCaptureDriver();

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
		WaitIntermission,
		WaitIntermissionShot,
		WaitHold,
		WaitHoldShot,
		WaitPicked,
		StartDuel,
		WaitReflect,
		WaitReflectAim,
		WaitReflectShot,
		StartOrb,
		WaitOrb,
		WaitTelegraphShot,
		WaitOrbShot,
		Finish,
		Quit
	};

	void Enter(EStep Next);
	void Advance();
	void Fail(const TCHAR* Reason);
	bool RequestShot(const TCHAR* FileName);
	bool ShotFinished() const;
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
	FString PendingShot;
	bool bShotOpen = false;
	FString RunId = TEXT("T20");
	FString SaveDir;
	int32 ShotCount = 0;
	int32 EventCursor = 0;
	int32 DuelSeed = 1;
	int32 ReflectedId = -1;
	int32 OrbTries = 0;
	bool bSigilPlayed = false;
	bool bTelegraphShot = false;
	double OrbSeen = 0.0;
};
