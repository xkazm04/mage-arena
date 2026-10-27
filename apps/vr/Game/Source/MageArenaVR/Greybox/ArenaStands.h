#pragma once

#include "CoreMinimal.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
struct FArenaLayout;

/**
 * The greybox stands and, since T57, the crowd on them. One stand box per wall segment and tier, derived from the layout
 * the way AArenaGreybox has always built them; the crowd cards stand on those boxes' tops.
 */
namespace ArenaStands
{
/** One tier box behind one wall segment. Metres, Unreal axes. */
struct FStandBox
{
	/** Box centre in plan, on the ground (z = 0). The box runs from the ground to TopM. */
	FVector MidM = FVector::ZeroVector;
	/** Unit vector along the wall chord (the row direction). */
	FVector Along = FVector::ForwardVector;
	/** Unit vector away from the arena, square to the chord. */
	FVector Outward = FVector::RightVector;
	/** The chord length scaled out to this tier: the length the cards share. The drawn box is 4 % longer (overlap). */
	double RowLengthM = 0.0;
	double DepthM = 0.0;
	double TopM = 0.0;
	/** The chord's rotation (X along, Z up): the box rotation. */
	FQuat Rotation = FQuat::Identity;
};

/** The box of tier Tier behind wall segment Segment, out of Max(Layout.WallSegments, 3). */
MAGEARENAVR_API FStandBox StandBox(const FArenaLayout& Layout, int32 Segment, int32 Tier);

/** The tread top of Tier in metres: the wall height, the first step above it, and Tier steps. */
MAGEARENAVR_API double TierTopM(const FArenaLayout& Layout, int32 Tier);
}

/**
 * T57: the crowd. Grey-clay spectator cards (apps/vr/art/environment-kit/crowd, the A07 cut-outs) on the greybox tier tops,
 * all drawn by ONE instanced component of one quad (the engine's 1 m plane), so the whole crowd is one draw per pass. Each instance's custom data
 * (8 floats) is its variant's atlas rect, a tunic tint and a flat-colour flag, which M_Crowd_Atlas reads. Optional: with
 * -MageArenaNoCrowd or the assets missing the stands stay bare, and nothing else in the scene changes.
 *
 * T65 (docs/research/CROWD-TINT-T65-2026-10-25.md): warm, muted tunics on the clay figures and a few red banners, as extra
 * instances of the same component whose flag draws a flat colour at full opacity. No tint is ever brighter or more
 * saturated than a threat colour, nor within BrazierFire::ThreatChromaMargin of one by chroma (MageArena.Greybox.Crowd.TintNotThreat).
 */
namespace ArenaCrowd
{
/** One figure of the atlas: crowd/manifest.json variants[Name]. */
struct FVariant
{
	const TCHAR* Name;
	double HeightM;
	double WidthM;
	/** The card centre to the viewer's right of the feet: (boundsM.min.y + boundsM.max.y) / 2. */
	double CentreRightM;
	/** manifest uvRect: u0, v0, u1, v1, v up from the atlas bottom. */
	double U0;
	double V0;
	double U1;
	double V1;
	bool bSeated;
};

MAGEARENAVR_API TConstArrayView<FVariant> Variants();

/** Placement constants, as crowd.py placed the kit's treads (seed, pitch, jitter, the 6 cm alternate step). */
constexpr uint32 Seed = 20261022u;
constexpr double PitchM = 0.55;
constexpr double LateralJitterM = 0.12;
constexpr double YawJitterDeg = 12.0;
constexpr double ScaleMin = 0.92;
constexpr double ScaleSpan = 0.16;
constexpr double AlternateBackM = 0.06;
/**
 * Floats of per-instance custom data: the atlas rect u0, top v, u1, bottom v (UE texture space, v down), then the tint
 * RGB (linear) and the flat flag (1 draws the tint as a flat colour at full opacity: a banner; 0 tints the clay tunic).
 */
constexpr int32 RectFloats = 4;
constexpr int32 TintFloatIndex = 4;
constexpr int32 FlatFloatIndex = 7;
constexpr int32 CustomDataFloats = 8;
/** Tint stream: its own mulberry32, so the tint pick never moves a card. */
constexpr uint32 TintSeed = Seed ^ 0x7165u;
/**
 * T71 option C: tiers from this one up (the rows the layout gained beyond the original three) draw their variant, jitter and
 * tint from their own two streams, after every card of the original tiers, so no T57/T65 card changes.
 */
constexpr int32 FirstOwnStreamTier = 3;
constexpr uint32 UpperSeed = Seed ^ 0x7131u;
constexpr uint32 UpperTintSeed = TintSeed ^ 0x7131u;
/** The tunic starts this far down the figure (the head, hands and raised arms above it keep their clay) and fades in over the edge. */
constexpr float TunicStartFraction = 0.22f;
constexpr float TunicEdgeFraction = 0.10f;
/** The clay luminance a tunic shade is divided by (saturated): the tunic never exceeds its tint. About the clay's 0.17 mean, 0.36 at its 95th percentile. */
constexpr float ClayShadeRef = 0.20f;
/** The least brightest channel of a tint: under it every channel is under the black core class's 0.08. */
constexpr float MinTintBrightest = 0.08f;
/** A banner: a flat card behind the top row, in metres. */
constexpr double BannerWidthM = 1.0;
constexpr double BannerHeightM = 2.8;
/** How far a banner stands from the middle of the top tread towards its back edge, and from an aisle's middle. */
constexpr double BannerBackM = 0.5;
constexpr double BannerAisleOffsetM = 1.3;
/** Every BannerEvery-th wall segment that is not an aisle carries one banner in its middle; each aisle carries two flanking it. */
constexpr int32 BannerEvery = 4;

/** Wall segments left empty, in mirror pairs across both axes, as aisles (crowd.py left the kit's stairs empty). */
MAGEARENAVR_API bool IsAisle(int32 Segment, int32 Segments);

struct FCard
{
	int32 Variant = 0;
	int32 Segment = 0;
	int32 Tier = 0;
	/** The feet, on the tread top. Metres. */
	FVector FeetM = FVector::ZeroVector;
	double YawDeg = 0.0;
	double Scale = 1.0;
	/** The instance transform of the engine plane, centimetres. */
	FTransform Transform;
	/** Custom data: u0, top v, u1, bottom v. */
	float Rect[RectFloats] = {0.f, 0.f, 0.f, 0.f};
	/** Custom data: the tint, linear RGB. */
	float Tint[3] = {1.f, 1.f, 1.f};
	/** Custom data: 1 for a banner (flat colour, full opacity), 0 for a figure. */
	float Flat = 0.f;
};

/** The tunic tints (linear RGB): muted warm browns, reds and a plum and a slate, each under the dimmest threat colour. */
MAGEARENAVR_API TConstArrayView<FLinearColor> TunicPalette();
/** The banner red (linear RGB), muted, off the rim red and the ember orange. */
MAGEARENAVR_API FLinearColor BannerColour();

/** A colour divided by its brightest channel (black below 1e-4), so a gain never changes the answer: the threat classes' chroma. */
MAGEARENAVR_API FLinearColor Chroma(const FLinearColor& Colour);
/** The distance between two colours' chroma in linear RGB (BrazierFire::ThreatChromaMargin is the least safe distance). */
MAGEARENAVR_API float ChromaDistance(const FLinearColor& A, const FLinearColor& B);

/** Every card, in a fixed order from the layout and the seed. */
MAGEARENAVR_API TArray<FCard> Place(const FArenaLayout& Layout);

/** The banners behind the top row, in a fixed order from the layout: the same card type, flat. After Place's cards in the instance order. */
MAGEARENAVR_API TArray<FCard> PlaceBanners(const FArenaLayout& Layout);

/** -MageArenaNoCrowd on the command line. */
MAGEARENAVR_API bool IsSwitchedOff();

constexpr const TCHAR* Folder = TEXT("/Game/Environment/ArenaKit/Crowd");
constexpr const TCHAR* AtlasName = TEXT("T_Crowd_Atlas");
constexpr const TCHAR* MaterialName = TEXT("M_Crowd_Atlas");
/** The card: the engine's 100 cm plane (normal +Z, pivot at its centre, UV 0..1 across it). */
constexpr const TCHAR* CardMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
/** The plane-local axis along which its U rises, and the one along which its V rises (MageArena.Greybox.Crowd.Card). */
MAGEARENAVR_API FVector PlaneUAxis();
MAGEARENAVR_API FVector PlaneVAxis();

/** "/Game/Environment/ArenaKit/Crowd/<Name>.<Name>". */
MAGEARENAVR_API FString ObjectPath(const TCHAR* Name);

/** One instanced component holding every card, attached to Owner. Null without a mesh. */
MAGEARENAVR_API UInstancedStaticMeshComponent* Spawn(AActor* Owner, const TArray<FCard>& Cards, UStaticMesh* Card, UMaterialInterface* Material);

/**
 * The arena's crowd: null and nothing placed when the switch is set or an asset is missing (one MAGEVR_ARENA_CROWD line
 * says which).
 */
MAGEARENAVR_API UInstancedStaticMeshComponent* Build(AActor* Owner, const FArenaLayout& Layout, bool bSwitchedOff);
}
