#pragma once

#include "CoreMinimal.h"
#include "Session/CaptureEvidence.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "SpellVfxCapture.generated.h"

/**
 * T58 capture. -MageArenaSpellVfxCapture, run by apps/vr/tools/capture-spell-vfx.ps1. Posed stills of the spell VFX
 * from the seated view, written to runs/<run>/shots/: for Water, Fire and Air a bolt in flight, an absorb at the ward and
 * a perfect absorb; the unblockable's ground telegraph; and a steel stone (the dodge class, for reference).
 *
 * The Brennic semifinal is started and frozen (frame hold), as the D-G4 stress driver does, and the capture owns the
 * kernel state: each still poses one school mage 8 m ahead and its projectile, telegraph or ward event. The presentation
 * draws the posed state with its own code, so the same capture shoots the before and the after of a VFX change.
 */
UCLASS()
class MAGEARENAVR_API USpellVfxCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	USpellVfxCaptureDriver();

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
		StartBout,
		Pose,
		Impact,
		Settle,
		Finish,
		Quit
	};

	void Enter(EStep Next);
	void Fail(const FString& Reason);
	void PoseScene(int32 Scene);
	void ImpactScene(int32 Scene);
	class UArenaSessionSubsystem* SessionSys() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	int32 Frames = 0;
	int32 Scene = 0;
	double StepStartWall = 0.0;
	bool bShotOpen = false;
	FCaptureEvidence Evidence;
	FString RunId = TEXT("T58");
	int32 ShotCount = 0;
};
