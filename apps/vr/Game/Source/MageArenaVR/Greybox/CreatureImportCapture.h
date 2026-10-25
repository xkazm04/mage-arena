#pragma once

#include "CoreMinimal.h"
#include "Greybox/ArenaLayout.h"
#include "Session/CaptureEvidence.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "CreatureImportCapture.generated.h"

class ASkeletalMeshActor;
class UMaterialInstanceDynamic;
class URivalReplayCaptureDriver;
class UTexture2D;

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

	/** -MageArenaRivalReplay: this run is the T38 rival replay instead of the stills above. */
	UPROPERTY()
	TObjectPtr<URivalReplayCaptureDriver> RivalReplay;
};

/**
 * T38 (-MageArenaCreatureImportCapture -MageArenaRivalReplay, apps/vr/tools/capture-rival.ps1;
 * docs/research/T38-RIVAL-2026-10-25.md): first the cinder hound and the mire maw stilled at their 8 m spots under the
 * T56 dusk, then the seated Brennic duel replayed (the reference seated script, bout 3: SetBout(2, true)), with Brennic
 * drawn by the session presentation's rival figure (Session/RivalFigure.h) and the two creatures beside him. Seed 1 is
 * the T16 replay, and Brennic wins it (DF-011), so it never shows Death; seed 2, the lowest seed where the same script
 * downs him, follows (-MageArenaRivalSeeds=1,2). Each of the four clips gets short sets of frames from the first times
 * it plays through (three for Idle, Cast and Hit, one for Death): runs/<run>/rival/<clip>/seed<n>-<clip>-a<try>-<frame>.png.
 * The launcher runs it with -usefixedtimestep -fps=60, so one engine frame is one kernel tick and a set's frames are
 * clip time. Once Brennic is down the session runs on into its intermission while Death plays and holds where he fell
 * (-MageArenaRivalHold=1 holds the kernel at the down instead). Ends in MAGEVR_CAPTURE_DONE with runs/<run>/manifest.json, or
 * MAGEVR_CAPTURE_FAIL (the figure did not load, a clip never played through, the duel ended without Brennic down).
 */
UCLASS()
class MAGEARENAVR_API URivalReplayCaptureDriver : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	URivalReplayCaptureDriver();

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
		PoseCreatures,
		SettleCreatures,
		Duel,
		Finish,
		Quit
	};

	/**
	 * The frames of one clip: Offsets (engine frames after the clip starts), from each start that plays them all, until
	 * Wanted full sets are shot (the fight's spell light hides him in some; the owner picks).
	 */
	struct FClipSet
	{
		TArray<int32> Offsets;
		int32 Wanted = 1;
		int32 Completed = 0;
		int32 Attempt = 0;
		int32 Starts = -1;
		int32 StartFrame = 0;
		int32 Next = 0;
		bool bDone = false;
	};

	void Enter(EStep Next);
	void Fail(const FString& Reason);
	bool StartDuel();
	void StepDuel();
	void RequestShot(const FString& Name, const FString& Stage);
	FString CropOfFigure() const;
	class UArenaSessionSubsystem* SessionSys() const;

	EStep Step = EStep::WaitStable;
	bool bRunning = false;
	double RunStartWall = 0.0;
	double StepStartWall = 0.0;
	int32 StableFrames = 0;
	int32 Frames = 0;
	int32 DuelFrame = 0;
	int32 CreatureShot = 0;
	int32 SeenStarts = -1;
	int32 DownFrame = -1;
	int32 EndedFrame = -1;
	FString RunId = TEXT("T38-rival");
	/** The duels in turn; the next starts when one ends with a clip still unshot. */
	TArray<uint32> Seeds;
	int32 SeedIndex = 0;
	FString Timeline;
	FArenaLayout Layout;
	FCaptureEvidence Evidence;
	/** By ERivalClip. */
	TArray<FClipSet> Sets;

	/** The hound and the maw at their scene spots (CreatureImport::Specs() order, Brennic left out). */
	UPROPERTY()
	TArray<TObjectPtr<ASkeletalMeshActor>> Creatures;

	/** Held resident for the duel (the figure itself is the presentation's). */
	UPROPERTY()
	TObjectPtr<UTexture2D> BrennicTexture;
};
