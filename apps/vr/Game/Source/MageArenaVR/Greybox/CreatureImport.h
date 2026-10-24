#pragma once

#include "CoreMinimal.h"

class ASkeletalMeshActor;
class UWorld;
struct FArenaLayout;

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
 * Moves a spawned creature to PointCm, turned to face the seated player and then YawOffsetDeg further (0 front, 45 three-
 * quarter, 180 back). The mesh's own forward is read from its bones, not assumed from the FBX axis conversion.
 */
void PlaceCreature(ASkeletalMeshActor& Actor, const FArenaLayout& Layout, const FVector& PointCm, double YawOffsetDeg);

/** Every spec'd creature at opponent distance, each at its SceneLateralM, in Specs() order: the budget scene and the cast still. */
TArray<ASkeletalMeshActor*> SpawnScene(UWorld& World, const FArenaLayout& Layout);

/** Opponent distance (m). -MageArenaCreatureDistanceM= overrides it. */
double OpponentDistanceM();
}
