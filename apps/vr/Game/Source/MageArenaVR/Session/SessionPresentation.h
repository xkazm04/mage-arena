#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Greybox/GreyboxUtil.h"
#include "Session/SpellVfx.h"
#include "SessionPresentation.generated.h"

class FArenaSession;
class UArenaAudioComponent;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWidgetComponent;
class UTexture2D;
class URivalFigureComponent;

/** An enemy HP bar's two Cube parts: their sizes, and the fill centre's offset from the back centre. */
struct FHpBarParts
{
	FVector BackSizeCm;
	FVector FillSizeCm;
	FVector FillOffsetCm;
};

/**
 * T64: when a text card may be drawn. A widget component draws into its render target only while it is visible, so a card
 * shown the frame it appears has no text yet (the death frame's black plate and white bars). The widget is made visible
 * but hidden in game for PrimeSyncs syncs first; the plate and the label show on the sync after.
 */
struct FCardReveal
{
	static constexpr int32 PrimeSyncs = 2;
	int32 Primed = 0;

	/** One sync: true when the card is wanted and primed. A card that is not wanted starts over. */
	bool Step(bool bWanted)
	{
		if (!bWanted)
		{
			Primed = 0;
			return false;
		}
		if (Primed < PrimeSyncs)
		{
			++Primed;
			return false;
		}
		return true;
	}
};

namespace SessionHpBar
{
/** The bar at Fraction of full HP (clamped to 0..1); the fill is left-anchored on the back's length. */
MAGEARENAVR_API FHpBarParts Parts(double Fraction);
}

/** Greybox bodies, projectiles, telegraphs, hit flashes, HP bars, and the wrist cuff. */
UCLASS()
class MAGEARENAVR_API ASessionPresentation : public AActor
{
	GENERATED_BODY()

public:
	ASessionPresentation();

	void Sync(const FArenaSession& Session, bool bPaused);
	int32 GetEnemiesOnScreen() const { return EnemiesOnScreen; }
	/** T26: the bout's audio (spatialised cues from the event stream). */
	UArenaAudioComponent* GetAudio() const { return Audio; }
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** T37: an opaque part whose material is the shared instance of Colour (one material per colour, D-G4 lever B). */
	UStaticMeshComponent* MakePart(const TCHAR* Shape, const FLinearColor& Colour);
	/** T37: a runtime recolour is a swap to the shared instance of the new colour, never a write into a shared one. */
	void Recolour(UStaticMeshComponent* Part, const FLinearColor& Colour);
	/** T37: distinct materials on this actor's mesh components (MAGEVR_PRESENTATION_MATERIALS). */
	int32 CountDistinctMaterials() const;
	/** T37: shared instances in the opaque part cache and the translucent curtain cache together. */
	int32 GetSharedTintCount() const { return PartTints.Tints.Num() + CurtainTints.Tints.Num(); }
	/** T46: the body and both HP bar parts of this actor are visible (the D-G4 stress capture counts its population by it). */
	bool IsBodyDrawn(int32 ActorId) const;
	/** T46: this projectile is live and its part is visible. */
	bool IsShotDrawn(int32 ProjectileId) const;
	/** The visible beads of the ring this id holds (a spear, an ember or a telegraph), or -1 when no ring holds it. */
	int32 CountRingBeadsDrawn(int32 Id) const;
	/** T58: the glow puffs (SpellVfx) drawn this frame, at most SpellVfx::MaxPuffs. */
	int32 GetGlowPuffsDrawn() const { return GlowFrame.Drawn; }
	/** T58: the glow layers drawn this frame: one instanced component, so one draw call, per glow colour on screen. */
	int32 CountGlowLayersDrawn() const;
	/**
	 * T38: where Brennic's rigged figure is loaded from (a Content folder and the asset name, Session/RivalFigure.h). The
	 * default is the creature cast's scratch import. Read once, on the first frame his duel is drawn; a test points it at
	 * a folder that does not exist.
	 */
	void SetRivalFigureSource(const FString& Folder, const FString& Asset);
	/**
	 * T64: the text cards drawn without their text: an opaque plate or slab drawn while its widget has not drawn into a render
	 * target, or a stone label drawn before its first render. A widget component draws only while visible, so a card that
	 * appears the same frame it is first shown has no text yet. The captured death frame showed that as a black plate and
	 * two white bars. Zero is the only right answer; the style-pick capture fails any frame that counts one.
	 */
	int32 CountBlankCards() const;
	/** T22 seam: the crack seams, the wrist cuff root and its plane, for the test that every visible seam sits on a shown cuff. */
	int32 GetCrackSeamCount() const { return CrackSeams.Num(); }
	const UStaticMeshComponent* GetCrackSeam(int32 Index) const { return CrackSeams.IsValidIndex(Index) ? CrackSeams[Index].Get() : nullptr; }
	const USceneComponent* GetCuffRoot() const { return CuffRoot; }
	const UStaticMeshComponent* GetCuffPlane() const { return CuffPlane; }
	/** T38: Brennic's figure when it loaded, else null (the greybox body stands in). */
	const URivalFigureComponent* GetRivalFigure() const { return RivalFigure; }
	/** T38: the figure loaded and stands in for the rival this frame. */
	bool IsRivalFigureShown() const;

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
		// T58: whose shot it is, its glow colour (unset for steel) and its trail, for the splash when the ward takes it.
		int32 OwnerId = 0;
		TOptional<FLinearColor> Glow;
		SpellVfx::ETrail Trail = SpellVfx::ETrail::None;
	};

	// T58: an absorb splash at the ward, and for a perfect absorb the ring at the palm as well.
	struct FBurst
	{
		FVector AtCm = FVector::ZeroVector;
		FLinearColor Colour = FLinearColor::White;
		SpellVfx::ETrail Trail = SpellVfx::ETrail::Droplets;
		double Start = 0.0;
		int32 Seed = 0;
		bool bPerfect = false;
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

	FBody& BodyFor(int32 Id, const FString& EnemyId);
	FShot& ShotFor(int32 Id, bool bSpear);
	FRing& RingFor(int32 Id);
	void HideUnused(const TSet<int32>& LiveIds, double Now, bool bPaused);
	void EnsureCuff();
	void EnsureDome();
	void SyncDome(const FArenaSession& Session, const struct FActor* Player);
	void SyncWalls(const FArenaSession& Session, double Now);
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
	void SyncTablet(const FArenaSession& Session);
	void SyncPhaseVisuals(const FArenaSession& Session, double Now);
	void EnsureCollarVisuals(const FArenaSession& Session);
	void SyncCollarVisuals(const FArenaSession& Session);
	UCameraComponent* FindCamera() const;
	/** T58: the absorb splashes and perfect rings started by this frame's ward events, and the live ones drawn. */
	void SyncBursts(const struct FGames& Games, const struct FActor& Player, const TSet<int32>& LiveShots, const FVector& EyeCm, double Now);
	/** T58: writes this frame's puffs into the glow layers and hides the layers with none. */
	void FlushGlow();
	/** T38: Brennic is the bound rival of this bout and his figure loads (tried once). */
	bool UsesRivalFigure(const FArenaSession& Session);
	/** T38: the figure follows the rival's kernel position, facing and events, down included (Death holds). */
	void SyncRivalFigure(const FArenaSession& Session, bool bPaused);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UArenaAudioComponent> Audio;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cylinder;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Sphere;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cube;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Plane;

	// T37: one material per colour for the opaque parts, and one per colour for the translucent dome (the wall is glow flames since L2).
	UPROPERTY()
	FGreyboxTintCache PartTints;

	UPROPERTY()
	FGreyboxTintCache CurtainTints;

	// T58: one tint of M_SpellGlow per glow colour, and one instanced layer per colour, so every puff of a colour is one
	// draw call. T63: the layer draws cards (the engine plane), each showing its flipbook cell through per-instance custom
	// data. GlowFrame holds this frame's card transforms and custom data per colour, and the frame's puff count.
	UPROPERTY()
	FGreyboxTintCache GlowTints;

	UPROPERTY()
	TMap<FLinearColor, TObjectPtr<UInstancedStaticMeshComponent>> GlowLayers;

	SpellVfx::FGlowFrame GlowFrame;
	TArray<FBurst> Bursts;
	// The seat's Metrics.Blocks last frame: a rise is a plain absorb ("hit" carries no verdict).
	int32 LastBlocks = -1;

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

	bool bDomeBuilt = false;

	UPROPERTY()
	TObjectPtr<UWidgetComponent> DaisWidget;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> DaisPlate;

	FCardReveal DaisReveal;
	FCardReveal TabletReveal;
	FCardReveal StoneReveal[3];

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

	// T38: Brennic's rigged figure in place of his greybox column, when the scratch import is there (Session/RivalFigure.h).
	UPROPERTY()
	TObjectPtr<URivalFigureComponent> RivalFigure;

	FString RivalFigureFolder;
	FString RivalFigureAsset;
	bool bRivalFigureTried = false;

	// T19 phase break beat: the rival flares in its element colour, a ring pulses on both wrists (the surge), and the
	// crowd band brightens (the swell placeholder; audio is T26). No new threat colours.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RivalFlare;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CuffPulses;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CrowdBand;

	bool bPhaseVisuals = false;

	// T21 aftermath: a greybox stone tablet on the dais with one row per bout (results and times from the arena flags).
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TabletSlab;

	UPROPERTY()
	TObjectPtr<UWidgetComponent> TabletText;

	// T22 diegetic collar feedback, no numbers: a bright seam flashes on the wrist cuff (only over a shown cuff) on each Crack, and two greybox
	// Wardstones at the arena edge (inside the front 70 degree arc) glow with the day's Tithe and pulse on each point.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CrackSeams;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Wardstones;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WardstoneCaps;

	bool bCollarVisuals = false;
	double PhaseAt = -1.0;
	int32 PhaseRivalId = 0;

	// T23 air telegraphs, drawn from the air mage's pending cast: the Veering Bolt's arc (beads along the flown arc, in
	// the air colour) with a shrinking ring where it lands, Downdraft's shrinking ring, and the Tempest Lance line
	// (black core, red rim: the only leave-threat Air has). Air Form: a ring of air-coloured beads around the mage.
	void SyncAirVisuals(const FArenaSession& Session, double Now);

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> AirArcBeads;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AirRing;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AirLineBody;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AirLineRim;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> AirFormBeads;
};
