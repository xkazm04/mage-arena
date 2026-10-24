#pragma once

#include "CoreMinimal.h"
#include "Greybox/ArenaLayout.h"
#include "Session/CaptureEvidence.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "CreatureImportCapture.generated.h"

class ASkeletalMeshActor;

/**
 * The creature UE import stills (-MageArenaCreatureImportCapture, apps/vr/tools/capture-creature-import.ps1): the
 * imported Brennic and cinder hound in the greybox arena under the T56 dusk, at opponent distance from the seated view.
 * Front, three-quarter and back of each (the asset turns in place; the camera stays seated), the hound's ember glow at
 * 5 m, and both side by side. The session stays idle, as in the greybox capture.
 */
UCLASS()
class MAGEARENAVR_API UCreatureImportCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UCreatureImportCaptureDriver();

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
		Pose,
		Settle,
		Finish,
		Quit
	};

	struct FShotPlan
	{
		const TCHAR* File;
		/** "Brennic", "CinderHound" or "both". */
		const TCHAR* Subject;
		double YawOffsetDeg;
		double DistanceM;
	};

	void Enter(EStep Next);
	void Advance();
	void Fail(const FString& Reason);
	void PoseShot(const FShotPlan& Plan);
	void RequestShot(const TCHAR* FileName);
	void NoteMemory();
	bool TexturesResident() const;
	class AMageArenaPawn* FindPawn() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	int32 StableFrames = 0;
	int32 Frames = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	bool bShotOpen = false;
	FCaptureEvidence Evidence;
	FString RunId = TEXT("creature-import");
	int32 ShotIndex = 0;
	int32 ShotCount = 0;
	FArenaLayout Layout;

	UPROPERTY()
	TObjectPtr<ASkeletalMeshActor> Brennic;

	UPROPERTY()
	TObjectPtr<ASkeletalMeshActor> Hound;
};
