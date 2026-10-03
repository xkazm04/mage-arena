#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "GreyboxCapture.generated.h"

/**
 * Scripted desktop capture: initial view, sigil-line2 cast flash, ward arc,
 * four threats mid-flight, blink to the left pad. Writes shots and budget.json.
 */
UCLASS()
class MAGEARENAVR_API UGreyboxCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UGreyboxCaptureDriver();

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
		ShotInitial,
		WaitInitial,
		PlaySigil,
		WaitSigil,
		ShotSigil,
		WaitSigilShot,
		PlayWard,
		WaitWard,
		ShotWard,
		WaitWardShot,
		PlayThreats,
		WaitThreats,
		ShotThreats,
		WaitThreatsShot,
		PlayBlink,
		WaitBlink,
		ShotBlink,
		WaitBlinkShot,
		WriteBudget,
		Quit
	};

	struct FGreyboxColourRow
	{
		FString Kind;
		FString Role;
		FString TargetHex;
		FString PixelHex;
		double DeltaE = 0.0;
		int32 X = 0;
		int32 Y = 0;
		bool bInside = false;
	};

	void Advance();
	bool RequestShot(const TCHAR* FileName);
	bool ShotFinished() const;
	void SampleBudget();
	int32 CountVisibleLod0() const;
	void SampleThreatColours();
	void WriteBudgetFile();
	FString RepoPath(const TCHAR* Relative) const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	int32 StableFrames = 0;
	double StepStartWall = 0.0;
	double RunStartWall = 0.0;
	void Enter(EStep Next);
	class AMageArenaPawn* FindPawn() const;
	bool RequestWarmupShot();

	FString PendingShot;
	bool bWarmupPending = false;
	bool bWarmupDone = false;
	bool bSawCast = false;
	bool bSawReject = false;
	bool bSawWard = false;
	double CastWall = 0.0;
	double WardWall = 0.0;
	double BlinkSettledWall = -1.0;
	FDelegateHandle CastHandle;
	FDelegateHandle RejectHandle;
	FDelegateHandle WardHandle;
	TWeakObjectPtr<class USigilRecognizerSubsystem> Sigils;
	TWeakObjectPtr<class UWardDetectorSubsystem> Wards;

	TArray<int32> DrawCalls;
	TArray<int32> MeasuredTriangles;
	TArray<int32> VisibleLod0Triangles;
	TArray<int32> RhiPrimitives;
	TArray<double> FrameMs;
	TArray<FString> ShotNames;
	TArray<FGreyboxColourRow> ColourRows;
	bool bColoursChecked = false;
	bool bColoursOk = false;
	bool bPlausible = false;
	bool bWithinBudget = false;
};
