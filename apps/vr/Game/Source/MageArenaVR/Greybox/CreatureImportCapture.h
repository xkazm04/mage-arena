#pragma once

#include "CoreMinimal.h"
#include "Greybox/ArenaLayout.h"
#include "Session/CaptureEvidence.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "CreatureImportCapture.generated.h"

class ASkeletalMeshActor;
class UMaterialInstanceDynamic;

/**
 * The creature cast stills (-MageArenaCreatureImportCapture, apps/vr/tools/capture-creature-import.ps1;
 * docs/research/CREATURE-CAST-UE-2026-10-24.md): Brennic, the cinder hound and the mire maw in the greybox arena under the
 * T56 dusk, at opponent distance from the seated view. An empty plate; the maw's front, three-quarter and back; the
 * hound's glow candidates at 8 m, front and three-quarter, each beside a shadowless mask shot of the same frozen pose (so
 * the hound's pixels can be told from the sand); and the whole cast. The session stays idle, as in the greybox capture.
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

	/** The hound's glow for a shot: its material's EmberIntensity, SoftIntensity and RimIntensity. */
	struct FGlow
	{
		const TCHAR* Label = TEXT("default");
		double Intensity = 0.0;
		double Soft = 0.0;
		double Rim = 0.0;
	};

	struct FShotPlan
	{
		const TCHAR* File;
		/** A spec name ("Brennic", "CinderHound", "MireMaw"), "cast" (all three) or "none" (the empty plate). */
		const TCHAR* Subject;
		double YawOffsetDeg;
		/** Index into Glows; the hound's glow for this shot. */
		int32 Glow;
		/** The hound casts no shadow and holds its pose: the mask shot its glow shots are compared with. */
		bool bMask;
	};

	void Enter(EStep Next);
	void Advance();
	void Fail(const FString& Reason);
	void PoseShot(const FShotPlan& Plan);
	void ApplyGlow(const FGlow& Glow);
	void RequestShot(const TCHAR* FileName);
	void NoteMemory();
	bool TexturesResident() const;
	ASkeletalMeshActor* FindActor(const FString& Name) const;
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
	/** base (the bc630e0 look), intensity, rim, soft, and the material's defaults (the chosen glow). */
	TArray<FGlow> Glows;

	/** The spawned creatures, in CreatureImport::Specs() order. */
	UPROPERTY()
	TArray<TObjectPtr<ASkeletalMeshActor>> Actors;

	/** One dynamic instance per hound material slot (both slots take M_CinderHound). */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> HoundMaterials;
};
