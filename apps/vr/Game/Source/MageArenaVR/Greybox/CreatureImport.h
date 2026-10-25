#pragma once

#include "CoreMinimal.h"

class ASkeletalMeshActor;
class USkeletalMesh;
class USkeletalMeshComponent;
class UWorld;
struct FArenaLayout;
struct FMeshDescription;

/**
 * The creature UE import (docs/research/CREATURE-UE-IMPORT-2026-10-24.md, CREATURE-CAST-UE-2026-10-24.md): Brennic (LOD0,
 * bake finish), the cinder hound s7101 and the mire maw (split-and-merge, seam fixed), imported by
 * Tools/ImportCreatureMeshCommandlet into a scratch Content folder that the capture script deletes before it ends.
 * Nothing here is committed content: the DINOv3 licence hold stands, so no TRELLIS asset ships.
 * -MageArenaCreatureImportScene places all three in the greybox arena at opponent distance (the D-G4 budget re-run passes
 * it through capture-budget.ps1 -ExtraArgs); -MageArenaCreatureImportCapture takes the stills (CreatureImportCapture.h).
 */
namespace CreatureImport
{
/** The scratch Content folder. Never committed; capture-creature-import.ps1 removes it before it exits. */
inline const TCHAR* ScratchRoot = TEXT("/Game/CreatureImportScratch");

struct FCreatureSpec
{
	/** The FBX and texture base name: <Name>.fbx, T_<Name>_BaseColor.png. */
	const TCHAR* Name;
	/** The research doc's measured height (m): the import is refused when the mesh bounds miss it by more than 5 %. */
	double HeightM;
	/** The hound takes the ember material; Brennic and the maw the plain base-colour one. */
	bool bEmber;
	/** Where the scene stands it (m to the seated player's right, at opponent distance): apart, and off the centre post. */
	double SceneLateralM;
	/**
	 * A slot whose name contains this takes a flat FlatColour material instead (the maw's tongue: beast_parts.py's capsule
	 * has no baked texels of its own). Null: every slot takes the one material.
	 */
	const TCHAR* FlatSlot = nullptr;
	/** Linear base colour of the flat slot. */
	FLinearColor FlatColour = FLinearColor::Black;
	/**
	 * A body texel near FlatColour (UV, UE's top-left origin). The section merge (FCreatureBudget::bMergeSections) points
	 * every UV of the flat slot here, so the slot folds into the body section and keeps a flat colour.
	 */
	FVector2f FlatSlotUv = FVector2f::ZeroVector;
};

TConstArrayView<FCreatureSpec> Specs();
const FCreatureSpec* FindSpec(const FString& Name);

/** /Game/CreatureImportScratch/<Name> */
FString FolderPath(const FString& Name);
/** Object paths of the imported mesh, texture and material. */
FString MeshObjectPath(const FString& Name);
FString TextureObjectPath(const FString& Name);
FString MaterialObjectPath(const FString& Name);
/** The flat slot's material (M_<Name>_Flat). */
FString FlatMaterialObjectPath(const FString& Name);

/**
 * The ember key's parameter values, shared by the material the commandlet builds and the C++ mirror below.
 * Measured offline on T_CinderHound_BaseColor.png (sRGB-encoded, island texels): the body is V 0.20-0.30, S 0.53, hue
 * 15 deg; the lava cracks are V 0.40-0.55, S 0.77-0.80, hue 19-20 deg; the eyes V 0.6-0.9, hue 30-41 deg. Hue alone
 * barely parts body and crack, so saturation and value carry the key and the hue window only drops off-orange texels;
 * it is wide enough (full to 40 deg) that the yellow eyes glow with the cracks. About 10.5 % of the island texels key
 * above 0.5 (the veins, the paw pads and the eyes).
 */
struct FEmberParams
{
	double HueCentreDeg = 20.0;
	/** Full weight within HueWidthDeg / 2 of the centre, zero at HueWidthDeg. */
	double HueWidthDeg = 40.0;
	double SatLow = 0.60;
	double SatHigh = 0.72;
	double ValLow = 0.34;
	double ValHigh = 0.46;
	/** Linear emissive colour (warm orange) and its multiplier, exposed as the EmberColour / EmberIntensity parameters. */
	// The camera runs no tonemapper, so an emissive channel above 1 clips: at (1, 0.32, 0.05) x 6 (the first stills)
	// the cracks clipped to pale yellow. Green stays near 0.4 here, so the cracks read orange.
	FLinearColor EmberColour = FLinearColor(1.0f, 0.25f, 0.03f);
	double EmberIntensity = 1.6;
	/**
	 * The glow at duel distance (Athena 20:13Z: the cracks read weakly at 8 m). Three terms add to the emissive, each
	 * x EmberColour: the crack mask x EmberIntensity; a soft crack bloom, the key averaged over a ring of taps SoftRadiusUv
	 * around the texel (EmberSoftHlsl), x (1 - mask) x SoftIntensity, so a crack's glow spreads onto the hide beside it
	 * while its core keeps the base orange; and a rim, the Fresnel term ^ RimExponent x RimIntensity on the whole body.
	 * The tonemapper and the exposure are not touched. The soft bloom is the one chosen
	 * (docs/research/CREATURE-CAST-UE-2026-10-24.md section 4): more intensity turns the crack cores yellow, and the rim
	 * lights the whole hide brown-orange and loses the dark body the cracks read against.
	 */
	double SoftIntensity = 1.2;
	double SoftRadiusUv = 0.008;
	double RimIntensity = 0.0;
	double RimExponent = 3.0;
	/** The body (1 - mask) moves toward BodyTint at the texel's own luminance x BodyGain, by BodyTintAmount. */
	FLinearColor BodyTint = FLinearColor(0.32f, 0.03f, 0.025f);
	double BodyTintAmount = 0.7;
	double BodyGain = 1.0;
};

const FEmberParams& DefaultEmber();

/** The material's key over one sRGB-encoded texel (0-1), mirrored from the HLSL in ImportCreatureMeshCommandlet. */
double EmberMask(const FLinearColor& Srgb, const FEmberParams& Params);
/** The HLSL of the ember Custom node; its inputs are named as the FEmberParams fields. */
const TCHAR* EmberHlsl();
/** The soft crack bloom: the same key, averaged over 12 taps on two rings (SoftRadiusUv and half of it) of texture Tex. */
const TCHAR* EmberSoftHlsl();
/** The soft bloom's C++ mirror: the mean of EmberMask over the tap colours. */
double EmberSoftMask(TConstArrayView<FLinearColor> SrgbTaps, const FEmberParams& Params);

/** How a creature casts its dynamic shadow. */
enum class ECreatureShadow : uint8
{
	/** Into the whole-scene cascades (CSM; 2 on desktop and on Quest): a draw per section per cascade it touches. */
	Cascades,
	/** A per-object (inset) shadow instead of the cascades. Desktop only: mobile draws none without r.Mobile.EnableInsetShadows. */
	Inset,
	/** None. */
	Off
};

/**
 * The draw-call budget of the creature cast (docs/research/DRAWCALL-BUDGET-2026-10-25.md). Before it, each mesh section
 * cost 5 draws (base pass, depth prepass, velocity, two shadow cascades), 25 for the cast. The spawn applies the cuts;
 * each lever has a command-line switch, so a capture measures it on and off in one build. The section merge and the LODs
 * change the loaded asset in memory (editor builds, which the desktop core runs); nothing is saved.
 */
struct FCreatureBudget
{
	/**
	 * One section per mesh: every slot folds into the largest (the hound's two M_CinderHound slots; the maw's tongue onto
	 * a body texel of its colour, FCreatureSpec::FlatSlotUv). -MageArenaCreatureMerge=0 keeps the imported sections.
	 */
	bool bMergeSections = true;
	/**
	 * Per-bone velocity (USkinnedMeshComponent::bPerBoneMotionBlur). Off, a creature draws velocity only on a frame its
	 * transform moves, so one standing at its spot draws none. The project runs MSAA (no TAA or TSR to feed), and Quest
	 * draws velocity only for TAA or space-warp motion vectors. -MageArenaCreatureVelocity=1 turns it back on.
	 */
	bool bPerBoneVelocity = false;
	/** -MageArenaCreatureShadow=cascades|inset|off. */
	ECreatureShadow Shadow = ECreatureShadow::Cascades;
	/** No dynamic shadow beyond this distance (m) from the seated view; 0 casts at any distance. -MageArenaCreatureShadowMaxM= */
	double ShadowMaxDistanceM = 0.0;
	/** Two generated LODs (the engine's skeletal reduction, from LOD0). -MageArenaCreatureLods=0 keeps LOD0 only. */
	bool bGenerateLods = true;
	/** Where LOD1 and LOD2 take over for the seated Quest 3 view (m), and their share of LOD0's triangles. */
	double Lod1DistanceM = 12.0;
	double Lod1TriangleShare = 0.5;
	double Lod2DistanceM = 20.0;
	double Lod2TriangleShare = 0.25;
	/** -MageArenaCreatureForceLod=N draws LOD N at any distance (a measurement switch); -1 picks by screen size. */
	int32 ForcedLod = -1;
};

/** The card's policy: the defaults of FCreatureBudget. */
const FCreatureBudget& DefaultBudget();
/** The levers as the import scene had them before the card: imported sections, per-bone velocity, cascades, LOD0 only. */
FCreatureBudget UncutBudget();
/** DefaultBudget(), or UncutBudget() under -MageArenaCreatureUncut, with each lever's switch applied over it. */
FCreatureBudget BudgetFromCommandLine();

/**
 * Quest 3's per-eye projection multiple (the larger of half the projection matrix's [0][0] and [1][1]; about 96 deg
 * vertical). The desktop's 90 deg 16:9 view is 0.889, so a desktop LOD switch lands at about twice the Quest distance.
 */
constexpr double Quest3ScreenMultiple = 0.45;
/** The engine's screen size (ComputeBoundsScreenSize: the bounds sphere's diameter over the view) at DistanceM. */
double ScreenSizeAt(double SphereRadiusCm, double DistanceM, double ScreenMultiple = Quest3ScreenMultiple);
/** Whether a creature DistanceM from the seated view casts a dynamic shadow under Budget. */
bool CastsShadowAt(const FCreatureBudget& Budget, double DistanceM);

/** What a section merge does. */
struct FSectionMerge
{
	/** Distinct material slots the triangles use, before and after. */
	int32 SlotsBefore = 0;
	int32 SlotsAfter = 0;
	/** The group every other one folds into (the one with the most triangles), as an index into the slot names. */
	int32 TargetGroup = INDEX_NONE;
	int32 TrianglesMoved = 0;
	/** Corners (vertex instances) of the flat slot that take FCreatureSpec::FlatSlotUv. */
	TArray<int32> FlatCorners;
	/** Flat-slot corners a body triangle also uses, left as they were (none on the maw). */
	int32 FlatCornersShared = 0;
};

/**
 * The merge over plain arrays: SlotNames[g] is group g's material slot, TriangleGroups[t] triangle t's group and
 * TriangleCorners[3t .. 3t+2] its corners. Every group folds into the largest; the flat slot's own corners
 * (FCreatureSpec::FlatSlot) are listed to take FlatSlotUv.
 */
FSectionMerge PlanSectionMerge(TConstArrayView<FName> SlotNames, TConstArrayView<int32> TriangleGroups, TConstArrayView<int32> TriangleCorners, const FCreatureSpec& Spec);

/**
 * Applies PlanSectionMerge to a mesh description: every polygon group takes the target's slot name, so the build maps
 * them all to one material slot and makes one section (FLODUtilities::AdjustImportDataFaceMaterialIndex matches faces
 * to slots by name), and the flat corners take FlatSlotUv on every UV channel. Polygons and topology are not touched.
 */
FSectionMerge MergeSections(FMeshDescription& Description, const FCreatureSpec& Spec);

/**
 * The mesh cuts of Budget (the section merge and the generated LODs) on the loaded asset, in memory and once per asset;
 * rebuilt before it returns. A no-op outside the editor binary.
 */
void PrepareMesh(USkeletalMesh& Mesh, const FCreatureSpec& Spec, const FCreatureBudget& Budget);
/** The component levers of Budget for a creature DistanceM from the seated view: shadow, velocity, a forced LOD. */
void ApplyComponentBudget(USkeletalMeshComponent& Component, double DistanceM, const FCreatureBudget& Budget);

/** -MageArenaCreatureImportScene or -MageArenaCreatureImportCapture is on the command line. */
bool IsSceneRequested();

/** Where an asset stands: DistanceM ahead of the centre pad, LateralM to the seated player's right, on the sand. */
FVector StandPointCm(const FArenaLayout& Layout, double DistanceM, double LateralM);

/**
 * Spawns the imported mesh of Name at Point, facing the seated player, with its idle clip looping when one was
 * imported. Null (with a MAGEVR_CREATURE_IMPORT fail line) when the scratch asset does not load.
 */
ASkeletalMeshActor* SpawnCreature(UWorld& World, const FArenaLayout& Layout, const FString& Name, const FVector& PointCm);

/**
 * The mesh's forward in component space, read from its ref pose (the head and ball_* bones), not assumed from the FBX
 * axis conversion. PlaceCreature and the T38 rival figure (Session/RivalFigure.h) turn the mesh by it.
 */
FVector MeshForward(const USkeletalMesh& Mesh);

/**
 * Moves a spawned creature to PointCm, turned to face the seated player and then YawOffsetDeg further (0 front, 45 three-
 * quarter, 180 back). The mesh's own forward is read from its bones, not assumed from the FBX axis conversion.
 */
void PlaceCreature(ASkeletalMeshActor& Actor, const FArenaLayout& Layout, const FVector& PointCm, double YawOffsetDeg);

/**
 * Every spec'd creature at opponent distance, each at its SceneLateralM, in Specs() order: the budget scene and the cast
 * still. -MageArenaCreatureOnly=<Name> spawns that one alone (a per-creature budget run); -MageArenaCreatureSkip=<Name>
 * leaves that one out (T38: Brennic, when the session draws him as the rival figure).
 */
TArray<ASkeletalMeshActor*> SpawnScene(UWorld& World, const FArenaLayout& Layout);

/** The distance (m) from the seated view (the centre pad) to PointCm, on the ground plane. */
double DistanceFromViewM(const FArenaLayout& Layout, const FVector& PointCm);

/** Opponent distance (m). -MageArenaCreatureDistanceM= overrides it. */
double OpponentDistanceM();
}
