#pragma once

#include "CoreMinimal.h"
#include "Session/CaptureEvidence.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "StylePickCapture.generated.h"

/**
 * T16: one scripted Brennic duel (seed 1) in a given style, one frame per kernel tick (the launcher runs it with
 * -usefixedtimestep -fps=60), and the duel's event log. Ends in exactly one of MAGEVR_CAPTURE_DONE shots=<n> with
 * shots-<style>/manifest.json, or MAGEVR_CAPTURE_FAIL <reason> (T45).
 */
UCLASS()
class MAGEARENAVR_API UStylePickCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** The deterministic end of a run: 60 s of the duel. */
	static constexpr int32 SequenceTarget = 3600;
	/**
	 * A run that ends before this many frames is a FAIL, never a DONE. 20 s: the shortest bout in the recorded chains
	 * ends at 23.12 s (T47), and the 2026-10-04 run of this duel ended at frame 1930.
	 */
	static constexpr int32 MinSequenceFrames = 1200;

	UStylePickCaptureDriver();

	void Start();

	/** Logs MAGEVR_CAPTURE_FAIL short run ... and returns false when Seq is short of MinSequenceFrames. */
	static bool CheckRunLength(int32 Seq, const FString& Stage);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual void BeginDestroy() override;

private:
	enum class EStep : uint8
	{
		WaitStable,
		Running,
		WaitShots,
		Quit
	};

	void Enter(EStep Next);
	void Fail(const FString& Reason);
	void StartDuel();
	void Record();
	void EndRun(const FString& Stage);
	void Finish();
	void RequestShot(const FString& FileName);
	void WriteEventLog() const;
	class UArenaSessionSubsystem* SessionSys() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	double RunStartWall = 0.0;
	double StepStartWall = 0.0;
	int32 StableFrames = 0;
	int32 SequenceIndex = 0;
	int32 SequenceEnd = SequenceTarget;
	int32 LastFrameTick = -1;
	int32 BoutWave = 2;

	int32 LastTick = -1;
	FString LastEventKind;
	int32 LastEventActorId = 0;

	FString StyleId = TEXT("greybox");
	FCaptureEvidence Evidence;
	UPROPERTY()
	TObjectPtr<class UTextRenderComponent> OverlayText;
};
