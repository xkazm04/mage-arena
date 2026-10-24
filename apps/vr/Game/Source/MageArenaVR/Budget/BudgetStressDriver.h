#pragma once

#include "Async/Future.h"
#include "Budget/BudgetFx.h"
#include "Budget/BudgetStressPlan.h"
#include "Budget/SceneCensus.h"
#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "BudgetStressDriver.generated.h"

class ASessionPresentation;
class UNiagaraComponent;
struct FMinimalViewInfo;

/**
 * D-G4 stress capture. Started by -MageArenaGreyboxCapture -MageArenaBudgetStress (apps/vr/tools/capture-budget.ps1).
 * Waits for a stable frame, starts the session's own bout (BudgetStress::SessionBout) and freezes it, then poses the
 * stress population into it as kernel actors: 20 enemies and 100 projectiles, with the staff dome planted and a fire wall
 * up. The session's ASessionPresentation draws all of it, its own parts included (T46: the measured frame is the shipping
 * presentation's, not a copy of it). Loops a two-hand clip, warms up, then samples 60 s of draw calls, triangles and frame
 * times into runs/DG4/<run>/.
 * Also spawns the six NS_Budget_ Niagara systems (Budget/BudgetFx.h, docs/research/NIAGARA-PROXY-2026-10.md) and counts
 * their live particles and GPU emitters on every frame of the window.
 * Scene parity (Budget/SceneCensus.h): at steady state and at the end the presentation's drawn parts must hold the plan's
 * (BudgetStressCensus); a divergence fails the run with MAGEVR_BUDGET_FAIL scene parity <what diverged>.
 * Logs MAGEVR_BUDGET_POP twice and ends with MAGEVR_BUDGET_DONE or MAGEVR_BUDGET_FAIL.
 * The presentation's parts of one colour share one material (T37, D-G4 lever B), so the default run measures the shared
 * path. -MageArenaBudgetSharedMaterials, the diagnostic switch that did this before T37, is a no-op.
 */
UCLASS()
class MAGEARENAVR_API UBudgetStressDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UBudgetStressDriver();

	void Start();

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

	static constexpr double WarmupS = 10.0;
	static constexpr double SampleS = 60.0;

	/** T46: the call site of the shared visible-LOD0 sum (Greybox::CountVisibleLod0) that SampleFrame reads. */
	static int32 VisibleLod0(UWorld* World, const FMinimalViewInfo& View);

private:
	enum class EStep : uint8
	{
		WaitStable,
		Warmup,
		Sample,
		WaitCsv,
		Quit
	};

	struct FPopulation
	{
		int32 Enemies = 0;
		int32 Projectiles = 0;
		int32 Hands = 0;
		int32 HandInstances = 0;
		/** Niagara systems live: not complete, with a system instance. */
		int32 Niagara = 0;
	};

	void Enter(EStep Next);
	void Fail(const FString& Why);
	bool SpawnPopulation();
	bool StartSession();
	void PoseSession(double Seconds);
	void CheckParity(const TCHAR* Phase);
	bool SpawnFx(UWorld* World, const FArenaLayout& Layout, const FVector& PadCm);
	void UpdateFx(double Seconds);
	void SampleFx();
	void JudgeParticles();
	int32 CountLiveFx() const;
	void UpdateShots(double Seconds);
	void KeepHandsLooping();
	FPopulation CountPopulation() const;
	void LogPopulation(const TCHAR* Phase, const FPopulation& Population) const;
	bool PopulationFull(const FPopulation& Population) const;
	void SampleFrame();
	void BeginCsv();
	void EndCsv();
	void WriteResults();
	void LogSharedMaterials();
	FString RunDir() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	bool bFailed = false;
	FString FailReason;
	int32 StableFrames = 0;
	int32 ShaderIdleFrames = 0;
	double RunStartWall = 0.0;
	double StepStartWall = 0.0;
	double PopulationStartWall = 0.0;
	double WarmupWallS = 0.0;
	double NextDumpWall = 0.0;
	int32 DumpsRequested = 0;
	int64 Launches = 0;
	int32 MinLiveShots = TNumericLimits<int32>::Max();
	FString RunName;

	int32 SharedMaterialParts = 0;
	int32 SharedMaterialDistinct = 0;
	int32 SharedMaterialTints = 0;

	FBudgetStressPlan Plan;

	/** The session's own presentation (UArenaSessionSubsystem spawns it); the capture draws through it. */
	UPROPERTY()
	TObjectPtr<ASessionPresentation> Presentation;

	/** Kernel ids of the posed enemies and projectiles, in plan order. */
	TArray<int32> EnemyActorIds;
	TArray<int32> ShotIds;
	FString SessionNote;
	bool bDomePosed = false;
	bool bWallPosed = false;

	FSceneParity Parity;
	FSceneCensus PlannedCensus;
	FSceneCensus DrawnCensus;
	bool bParityChecked = false;

	/** The six NS_Budget_ systems, in BudgetStress::FxRows() order. */
	UPROPERTY()
	TArray<TObjectPtr<UNiagaraComponent>> FxComponents;

	FPopulation SteadyPopulation;
	FPopulation EndPopulation;

	TArray<int32> DrawCalls;
	TArray<int32> RhiPrimitives;
	TArray<int32> Lod0Triangles;
	TArray<double> FrameMs;
	TArray<double> GpuMs;
	TArray<int32> LiveShots;
	TArray<int32> LiveParticles;
	TArray<int32> LiveParticlesCpu;
	TArray<int32> LiveParticlesGpu;
	TArray<int32> GpuEmitterCounts;
	TArray<int32> GpuCrossCheckCounts;
	TArray<int32> NiagaraLive;
	int32 ZeroParticleFrames = 0;
	EBudgetParticleVerdict ParticleVerdict = EBudgetParticleVerdict::NotExercised;
	FString ParticlesNote = TEXT("not exercised: the window did not finish");
	bool bAllowGpuParticles = false;
	uint64 PeakWorkingSetBytes = 0;
	FString CsvPath;
	bool bCsvStarted = false;
	bool bCsvWritten = false;
	TSharedFuture<FString> CsvFuture;
};
