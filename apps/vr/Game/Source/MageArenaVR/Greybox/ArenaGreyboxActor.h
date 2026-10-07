#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Greybox/ArenaLayout.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArenaGreyboxActor.generated.h"

class UBudgetStressDriver;
class UGreyboxCaptureDriver;

/** Spawns the procedural greybox from the layout. Basic shapes only. */
UCLASS()
class MAGEARENAVR_API AArenaGreybox : public AActor
{
	GENERATED_BODY()

public:
	AArenaGreybox();

	void Build();

private:
	void BuildShell(UStaticMesh* Cube, UStaticMesh* Cylinder);
	void BuildProps(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Cone);
	void BuildLights();
	void BuildSky(UStaticMesh* Cube);
	void DestroyStockStage();

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	FArenaLayout Layout;
	bool bBuilt = false;
};

/** Builds the greybox when a game world starts. Skips -nullrhi so automation stays headless. */
UCLASS()
class MAGEARENAVR_API UArenaGreyboxSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	UPROPERTY()
	TObjectPtr<UGreyboxCaptureDriver> Capture;

	UPROPERTY()
	TObjectPtr<UBudgetStressDriver> BudgetStress;

	bool bStarted = false;
};
