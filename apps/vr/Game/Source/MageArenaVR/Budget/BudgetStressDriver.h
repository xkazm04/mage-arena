#pragma once

#include "Async/Future.h"
#include "Budget/BudgetStressPlan.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "BudgetStressDriver.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/** Diagnostic only (-MageArenaBudgetSharedMaterials): one tinted material per colour, reused by every part of that colour. */
using FBudgetMaterialCache = TMap<FLinearColor, UMaterialInstanceDynamic*>;

/** Returns the cached tint of Base for Colour, making it with Greybox::Tint the first time a colour is asked for. */
UMaterialInstanceDynamic* BudgetSharedTint(FBudgetMaterialCache& Cache, UMaterialInterface* Base, UObject* Outer, const FLinearColor& Colour);

/** Owns the stress population's parts. Each part is one basic-shape component with its own tint, as SessionPresentation builds them. */
UCLASS()
class MAGEARENAVR_API ABudgetStressRig : public AActor
{
	GENERATED_BODY()

public:
	ABudgetStressRig();

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	friend class UBudgetStressDriver;
};

/**
 * D-G4 stress capture. Started by -MageArenaGreyboxCapture -MageArenaBudgetStress (apps/vr/tools/capture-budget.ps1).
 * Waits for a stable frame, spawns 20 enemy proxies and 100 projectiles, loops a two-hand clip, warms up,
 * then samples 60 s of draw calls, triangles and frame times into runs/DG4/<run>/.
 * Logs MAGEVR_BUDGET_POP twice and ends with MAGEVR_BUDGET_DONE or MAGEVR_BUDGET_FAIL.
 * Diagnostic variant: -MageArenaBudgetSharedMaterials makes the parts of one colour share one material (D-G4 levers).
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
	};

	void Enter(EStep Next);
	void Fail(const FString& Why);
	bool SpawnPopulation();
	void UpdateShots(double Seconds);
	void KeepHandsLooping();
	FPopulation CountPopulation() const;
	void LogPopulation(const TCHAR* Phase, const FPopulation& Population) const;
	bool PopulationFull(const FPopulation& Population) const;
	void SampleFrame();
	int32 CountVisibleLod0() const;
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
	bool bSharedMaterials = false;
	FBudgetMaterialCache SharedMaterials;
	int32 SharedMaterialParts = 0;
	int32 SharedMaterialDistinct = 0;

	FBudgetStressPlan Plan;

	UPROPERTY()
	TObjectPtr<ABudgetStressRig> Rig;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> EnemyParts;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ShotParts;

	FPopulation SteadyPopulation;
	FPopulation EndPopulation;

	TArray<int32> DrawCalls;
	TArray<int32> RhiPrimitives;
	TArray<int32> Lod0Triangles;
	TArray<double> FrameMs;
	TArray<double> GpuMs;
	TArray<int32> LiveShots;
	uint64 PeakWorkingSetBytes = 0;
	FString CsvPath;
	bool bCsvStarted = false;
	bool bCsvWritten = false;
	TSharedFuture<FString> CsvFuture;
};
