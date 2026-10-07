#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SessionPresentation.generated.h"

class FArenaSession;
class UCameraComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWidgetComponent;
class UTexture2D;

/** Greybox bodies, projectiles, telegraphs, hit flashes, HP bars, and the wrist cuff. */
UCLASS()
class MAGEARENAVR_API ASessionPresentation : public AActor
{
	GENERATED_BODY()

public:
	ASessionPresentation();

	void Sync(const FArenaSession& Session, bool bPaused);
	int32 GetEnemiesOnScreen() const { return EnemiesOnScreen; }

private:
	struct FBody
	{
		int32 Id = 0;
		TObjectPtr<UStaticMeshComponent> Mesh;
		TObjectPtr<UStaticMeshComponent> HpBack;
		TObjectPtr<UStaticMeshComponent> HpFill;
		FString EnemyId;
	};

	struct FShot
	{
		int32 Id = 0;
		TObjectPtr<UStaticMeshComponent> Mesh;
		FVector LastCm = FVector::ZeroVector;
		double GhostUntil = 0.0;
		bool bLive = false;
		bool bOwned = false;
		// A thrown spear is a cylinder. A bolt or a stone stays a sphere. Recycle only the matching mesh.
		bool bSpear = false;
	};

	struct FRing
	{
		int32 Id = 0;
		TObjectPtr<UStaticMeshComponent> Mesh;
		TObjectPtr<UStaticMeshComponent> Rim;
		TArray<TObjectPtr<UStaticMeshComponent>> Beads;
		// A hound ember gathering at the mouth during the spit windup (or where a hound died, for the death ember).
		TObjectPtr<UStaticMeshComponent> Glow;
	};

	struct FFlash
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		double Until = 0.0;
	};

	UStaticMeshComponent* MakePart(const TCHAR* Shape, const FLinearColor& Colour);
	FBody& BodyFor(int32 Id, const FString& EnemyId);
	FShot& ShotFor(int32 Id, bool bSpear);
	FRing& RingFor(int32 Id);
	void HideUnused(const TSet<int32>& LiveIds, double Now, bool bPaused);
	void EnsureCuff();
	void EnsureDome();
	void SyncDome(const FArenaSession& Session, const struct FActor* Player);
	void SyncWalls(const FArenaSession& Session);
	void SyncCuff(const FArenaSession& Session, const struct FActor& Player, bool bPaused);
	void ShowMask(const TCHAR* Name);
	UTexture2D* LoadMask(const FString& Name);
	UTexture2D* LoadPng(const FString& Name, const FString& Path);
	void EnsureTeachVisuals();
	void SyncTeachVisuals(const FArenaSession& Session, const struct FActor* Player);
	void EnsureComfortVisuals();
	void SyncComfortVisuals(const FArenaSession& Session);
	UWidgetComponent* MakeStoneLabel(const TCHAR* Name);
	void EnsurePhaseVisuals(const FArenaSession& Session);
	void SyncPhaseVisuals(const FArenaSession& Session, double Now);
	UCameraComponent* FindCamera() const;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cylinder;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Sphere;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cube;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Plane;

	TArray<FBody> Bodies;
	TArray<FShot> Shots;
	TArray<FRing> Rings;
	TArray<FFlash> Flashes;
	int32 EventCursor = 0;
	int32 EnemiesOnScreen = 0;

	UPROPERTY()
	TObjectPtr<USceneComponent> CuffRoot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> CuffPlane;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HpBar;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ManaBar;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> StaminaBar;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ClockBar;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> CuffCue;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CuffMaterial;

	UPROPERTY()
	TMap<FString, TObjectPtr<UTexture2D>> Masks;

	FString CuffMask;
	bool bLoggedTextureParams = false;
	double LastUnlockSim = -1.0;
	double LastPerfectSim = -1.0;

	UPROPERTY()
	TObjectPtr<USceneComponent> DomeAnchor;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Dome;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> DomeRibs;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WallSlats;

	bool bDomeBuilt = false;

	UPROPERTY()
	TObjectPtr<UWidgetComponent> DaisWidget;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> DaisPlate;

	UPROPERTY()
	TObjectPtr<USceneComponent> GlyphAnchor;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> GlyphPlane;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> GlyphBack;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GlyphMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> GlyphStrokes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PerfectRingParts;

	bool bTeachVisuals = false;
	bool bThreatCm = false;
	FVector ThreatCm = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<USceneComponent> StoneRoot;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ComfortStones;

	UPROPERTY()
	TArray<TObjectPtr<UWidgetComponent>> StoneLabels;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ArcEdges;

	bool bComfortVisuals = false;
	TArray<FLinearColor> StoneBase;

	// T20: the drawn path of a Leviathan Orb (spells-water.csv tide_orb IV A, UNBLOCKABLE: "the orb's path is drawn;
	// leave it or break the caster"), for the 1.00 s telegraph and the flight. Keyed by activation id.
	struct FOrbPath
	{
		TObjectPtr<UStaticMeshComponent> Body;
		TObjectPtr<UStaticMeshComponent> Rim;
	};
	TMap<int32, FOrbPath> OrbPaths;
	void SyncOrbPaths(const FArenaSession& Session);

	// T19 phase break beat: the rival flares in its element colour, a ring pulses on both wrists (the surge), and the
	// crowd band brightens (the swell placeholder; audio is T26). No new threat colours.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RivalFlare;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CuffPulses;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CrowdBand;

	bool bPhaseVisuals = false;
	double PhaseAt = -1.0;
	int32 PhaseRivalId = 0;
};
