#include "Greybox/ArenaStands.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Greybox/ArenaLayout.h"
#include "Greybox/GreyboxUtil.h"
#include "MageArenaVR.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
// mulberry32, the generator of apps/vr/tools/crowd/crowd.py: the same sequence on every host.
struct FCrowdRng
{
	uint32 State;

	double Next()
	{
		State += 0x6D2B79F5u;
		uint32 T = State;
		T = (T ^ (T >> 15)) * (T | 1u);
		T ^= T + ((T ^ (T >> 7)) * (T | 61u));
		return static_cast<double>(T ^ (T >> 14)) / 4294967296.0;
	}
};

// crowd.py MIX: the share of each variant per tread, front first. More sit in front, more stand at the back.
constexpr int32 MixTreads = 3;
constexpr double Mix[MixTreads][4] = {{40.0, 30.0, 18.0, 12.0}, {35.0, 25.0, 22.0, 18.0}, {25.0, 20.0, 30.0, 25.0}};

int32 PickVariant(FCrowdRng& Rng, int32 Tier)
{
	const double* Weights = Mix[FMath::Clamp(Tier, 0, MixTreads - 1)];
	double Total = 0.0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Total += Weights[Index];
	}
	double Roll = Rng.Next() * Total;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (Roll < Weights[Index])
		{
			return Index;
		}
		Roll -= Weights[Index];
	}
	return 3;
}

// crowd/manifest.json, variants in sheet order. MageArena.Greybox.Crowd.Variants holds this table to the manifest.
const ArenaCrowd::FVariant VariantTable[] = {
    {TEXT("SeatedCalm"), 1.3, 0.6796, (-0.304 + 0.3757) * 0.5, 0.015625, 0.584961, 0.441406, 0.992188, true},
    {TEXT("SeatedLean"), 1.1753, 0.6703, (-0.3227 + 0.3476) * 0.5, 0.472656, 0.624023, 0.892578, 0.992188, true},
    {TEXT("StandingCheer"), 1.9886, 0.8551, (-0.4176 + 0.4375) * 0.5, 0.015625, 0.081055, 0.435547, 0.569336, false},
    {TEXT("StandingPoint"), 1.75, 1.05, (-0.3102 + 0.7398) * 0.5, 0.466797, 0.139648, 0.982422, 0.569336, false},
};
}

ArenaStands::FStandBox ArenaStands::StandBox(const FArenaLayout& Layout, int32 Segment, int32 Tier)
{
	const double A = Layout.FloorRadiusXM;
	const double B = Layout.FloorRadiusYM;
	const int32 Segments = FMath::Max(Layout.WallSegments, 3);
	const double T0 = 2.0 * PI * Segment / Segments;
	const double T1 = 2.0 * PI * (Segment + 1) / Segments;
	const double MidAngle = (T0 + T1) * 0.5;
	const FVector P0(A * FMath::Cos(T0), B * FMath::Sin(T0), 0.0);
	const FVector P1(A * FMath::Cos(T1), B * FMath::Sin(T1), 0.0);
	const FVector Chord = P1 - P0;
	const FVector Mid = (P0 + P1) * 0.5;
	const double MidRadius = FMath::Max(Mid.Size2D(), 0.01);

	FStandBox Box;
	Box.Along = Chord.GetSafeNormal();
	Box.Outward = FVector(B * FMath::Cos(MidAngle), A * FMath::Sin(MidAngle), 0.0).GetSafeNormal();
	Box.Rotation = FRotationMatrix::MakeFromXZ(Box.Along, FVector::UpVector).ToQuat();
	const double Offset = Layout.WallThicknessM + Layout.StandGapM + (static_cast<double>(Tier) + 0.5) * Layout.StandDepthM;
	Box.MidM = Mid + Box.Outward * Offset;
	Box.RowLengthM = Chord.Size() * (Box.MidM.Size2D() / MidRadius);
	Box.DepthM = Layout.StandDepthM;
	Box.TopM = TierTopM(Layout, Tier);
	return Box;
}

double ArenaStands::TierTopM(const FArenaLayout& Layout, int32 Tier)
{
	return Layout.WallHeightM + Layout.StandFirstTopAboveWallM + Tier * Layout.StandStepRiseM;
}

FVector ArenaCrowd::PlaneUAxis()
{
	return FVector(1.0, 0.0, 0.0);
}

FVector ArenaCrowd::PlaneVAxis()
{
	return FVector(0.0, 1.0, 0.0);
}

TConstArrayView<ArenaCrowd::FVariant> ArenaCrowd::Variants()
{
	return VariantTable;
}

namespace
{
// T65: each tint is a chroma at a luminance of 0.054, under the dimmest threat colour in the greybox palette and in styles
// 13, 25 and 30 (style 25's steel, 0.056), with its brightest channel over the black core class's 0.08 and its chroma at
// least 0.15 off every threat's (MageArena.Greybox.Crowd.TintNotThreat holds all of it). Reds and browns stay under the
// ember orange and the rim red by saturation and by blue share; the plum and the slate vary the row.
const FLinearColor TunicTints[] = {
    FLinearColor(0.0839f, 0.0470f, 0.0352f), // rust
    FLinearColor(0.1026f, 0.0411f, 0.0390f), // maroon
    FLinearColor(0.1098f, 0.0395f, 0.0329f), // brick
    FLinearColor(0.0925f, 0.0416f, 0.0629f), // plum
    FLinearColor(0.0823f, 0.0477f, 0.0329f), // umber
    FLinearColor(0.0448f, 0.0535f, 0.0862f), // slate
};
// Luminance 0.0525, saturation 0.63: a dull red, a chroma 0.24 or more off the ember orange, the earth and the rim red.
const FLinearColor BannerRed(0.0986f, 0.0404f, 0.0365f);

// The plane's normal turns to face along Yaw and its U axis to the viewer's right (the viewer looks back along the card's
// -X, so their right is the card's -Y). Its V then runs down or up the card: bVDown tells which, so a rect swaps its top
// and bottom instead of mirroring the mesh, and the front face stays front. The plane's pivot is its centre: up half the
// figure, and across by CentreRightM so the feet sit on the pivot.
FTransform CardTransform(const FVector& FeetM, double YawDeg, double WidthM, double HeightM, double CentreRightM, bool& bVDown)
{
	const FVector Normal(0.0, 0.0, 1.0);
	const FVector Right = -ArenaCrowd::PlaneUAxis();
	const FMatrix Basis(Normal, Right, Normal ^ Right, FVector::ZeroVector);
	const FQuat PlaneToCard(Basis.GetTransposed());
	bVDown = PlaneToCard.RotateVector(ArenaCrowd::PlaneVAxis()).Z < 0.0;
	const FQuat Yaw = FRotator(0.0, YawDeg, 0.0).Quaternion();
	const FVector Centre = FeetM + Yaw.RotateVector(FVector(0.0, -CentreRightM, HeightM * 0.5));
	const bool bUAlongX = FMath::Abs(ArenaCrowd::PlaneUAxis().X) > 0.5;
	const FVector Scale = bUAlongX ? FVector(WidthM, HeightM, 1.0) : FVector(HeightM, WidthM, 1.0);
	return FTransform(Yaw * PlaneToCard, Centre * 100.0, Scale);
}

double FacingYawDeg(const FArenaLayout& Layout, const FVector& FeetM, const ArenaStands::FStandBox& Box)
{
	FVector Facing = Layout.ArenaCentreM - FeetM;
	Facing.Z = 0.0;
	Facing = Facing.GetSafeNormal();
	if (Facing.IsNearlyZero())
	{
		Facing = -Box.Outward;
	}
	return FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X));
}
}

TConstArrayView<FLinearColor> ArenaCrowd::TunicPalette()
{
	return TunicTints;
}

FLinearColor ArenaCrowd::BannerColour()
{
	return BannerRed;
}

FLinearColor ArenaCrowd::Chroma(const FLinearColor& Colour)
{
	const float Most = FMath::Max3(Colour.R, Colour.G, Colour.B);
	return Most > 1.0e-4f ? FLinearColor(Colour.R / Most, Colour.G / Most, Colour.B / Most, 1.f) : FLinearColor::Black;
}

float ArenaCrowd::ChromaDistance(const FLinearColor& A, const FLinearColor& B)
{
	const FLinearColor CA = Chroma(A);
	const FLinearColor CB = Chroma(B);
	return FMath::Sqrt(FMath::Square(CA.R - CB.R) + FMath::Square(CA.G - CB.G) + FMath::Square(CA.B - CB.B));
}

bool ArenaCrowd::IsAisle(int32 Segment, int32 Segments)
{
	// One segment in each quarter, mirrored across both axes: at 32 segments, 4, 11, 20 and 27 (the diagonals).
	const int32 Quarter = Segments / 8;
	if (Segments < 8)
	{
		return false;
	}
	const int32 Half = Segments / 2;
	return Segment == Quarter || Segment == Half - 1 - Quarter || Segment == Half + Quarter || Segment == Segments - 1 - Quarter;
}

TArray<ArenaCrowd::FCard> ArenaCrowd::Place(const FArenaLayout& Layout)
{
	TArray<FCard> Cards;
	const int32 Segments = FMath::Max(Layout.WallSegments, 3);
	// Tiers 0 to 2 first, in the order and from the streams T57/T65 fixed; then the upper tiers from their own streams.
	const int32 LowerEnd = FMath::Min(Layout.StandTiers, FirstOwnStreamTier);
	FCrowdRng Rng{Seed};
	FCrowdRng TintRng{TintSeed};
	FCrowdRng UpperRng{UpperSeed};
	FCrowdRng UpperTintRng{UpperTintSeed};
	for (const bool bUpper : {false, true})
	{
		const int32 FirstTier = bUpper ? LowerEnd : 0;
		const int32 EndTier = bUpper ? Layout.StandTiers : LowerEnd;
		FCrowdRng& CardRng = bUpper ? UpperRng : Rng;
		FCrowdRng& CardTintRng = bUpper ? UpperTintRng : TintRng;
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			if (IsAisle(Segment, Segments))
			{
				continue;
			}
			for (int32 Tier = FirstTier; Tier < EndTier; ++Tier)
			{
				const ArenaStands::FStandBox Box = ArenaStands::StandBox(Layout, Segment, Tier);
				// One row along the middle of the tread, spread evenly so the rows of neighbouring boxes meet at the pitch.
				const int32 Slots = FMath::FloorToInt32(Box.RowLengthM / PitchM);
				if (Slots <= 0)
				{
					continue;
				}
				const double Step = Box.RowLengthM / Slots;
				for (int32 Slot = 0; Slot < Slots; ++Slot)
				{
					FCard Card;
					Card.Variant = PickVariant(CardRng, Tier);
					const double Lateral = (CardRng.Next() - 0.5) * LateralJitterM;
					const double YawJitter = (CardRng.Next() - 0.5) * YawJitterDeg;
					Card.Scale = ScaleMin + CardRng.Next() * ScaleSpan;
					Card.Segment = Segment;
					Card.Tier = Tier;
					// Alternate cards stand 6 cm further back, so overlapping neighbours never z-fight.
					const double Back = (Slot % 2) ? AlternateBackM : 0.0;
					const double Along = (Slot - (Slots - 1) * 0.5) * Step + Lateral;
					Card.FeetM = Box.MidM + Box.Along * Along + Box.Outward * Back;
					Card.FeetM.Z = Box.TopM;

					Card.YawDeg = FacingYawDeg(Layout, Card.FeetM, Box) + YawJitter;

					const FVariant& Variant = VariantTable[Card.Variant];
					bool bVDown = false;
					Card.Transform = CardTransform(Card.FeetM, Card.YawDeg, Variant.WidthM * Card.Scale, Variant.HeightM * Card.Scale, Variant.CentreRightM * Card.Scale, bVDown);
					const float Top = static_cast<float>(1.0 - Variant.V1);
					const float Bottom = static_cast<float>(1.0 - Variant.V0);
					Card.Rect[0] = static_cast<float>(Variant.U0);
					Card.Rect[1] = bVDown ? Top : Bottom;
					Card.Rect[2] = static_cast<float>(Variant.U1);
					Card.Rect[3] = bVDown ? Bottom : Top;
					const int32 TintIndex = FMath::Min(FMath::FloorToInt32(CardTintRng.Next() * UE_ARRAY_COUNT(TunicTints)), static_cast<int32>(UE_ARRAY_COUNT(TunicTints)) - 1);
					Card.Tint[0] = TunicTints[TintIndex].R;
					Card.Tint[1] = TunicTints[TintIndex].G;
					Card.Tint[2] = TunicTints[TintIndex].B;
					Cards.Add(Card);
				}
			}
		}
	}
	return Cards;
}

TArray<ArenaCrowd::FCard> ArenaCrowd::PlaceBanners(const FArenaLayout& Layout)
{
	TArray<FCard> Banners;
	const int32 Segments = FMath::Max(Layout.WallSegments, 3);
	const int32 Tier = FMath::Max(Layout.StandTiers - 1, 0);
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const bool bAisle = IsAisle(Segment, Segments);
		if (!bAisle && Segment % BannerEvery != 0)
		{
			continue;
		}
		const ArenaStands::FStandBox Box = ArenaStands::StandBox(Layout, Segment, Tier);
		for (const double Side : {-1.0, 1.0})
		{
			if (!bAisle && Side > 0.0)
			{
				continue;
			}
			FCard Banner;
			Banner.Variant = -1;
			Banner.Segment = Segment;
			Banner.Tier = Tier;
			Banner.Scale = 1.0;
			Banner.FeetM = Box.MidM + Box.Along * (bAisle ? Side * BannerAisleOffsetM : 0.0) + Box.Outward * BannerBackM;
			Banner.FeetM.Z = Box.TopM;
			Banner.YawDeg = FacingYawDeg(Layout, Banner.FeetM, Box);
			bool bVDown = false;
			Banner.Transform = CardTransform(Banner.FeetM, Banner.YawDeg, BannerWidthM, BannerHeightM, 0.0, bVDown);
			// A flat colour: the rect is never sampled.
			Banner.Rect[2] = 1.f;
			Banner.Rect[3] = 1.f;
			Banner.Tint[0] = BannerRed.R;
			Banner.Tint[1] = BannerRed.G;
			Banner.Tint[2] = BannerRed.B;
			Banner.Flat = 1.f;
			Banners.Add(Banner);
		}
	}
	return Banners;
}

bool ArenaCrowd::IsSwitchedOff()
{
	return FParse::Param(FCommandLine::Get(), TEXT("MageArenaNoCrowd"));
}

FString ArenaCrowd::ObjectPath(const TCHAR* Name)
{
	return FString::Printf(TEXT("%s/%s.%s"), Folder, Name, Name);
}

UInstancedStaticMeshComponent* ArenaCrowd::Spawn(AActor* Owner, const TArray<FCard>& Cards, UStaticMesh* Card, UMaterialInterface* Material)
{
	UInstancedStaticMeshComponent* Instances = Greybox::MakeInstances(Owner, Card, Material);
	if (!Instances)
	{
		return nullptr;
	}
	Instances->SetNumCustomDataFloats(CustomDataFloats);
	TArray<FTransform> Transforms;
	Transforms.Reserve(Cards.Num());
	for (const FCard& Each : Cards)
	{
		Transforms.Add(Each.Transform);
	}
	Instances->AddInstances(Transforms, false);
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		float Data[CustomDataFloats];
		FMemory::Memcpy(Data, Cards[Index].Rect, sizeof(float) * RectFloats);
		FMemory::Memcpy(Data + TintFloatIndex, Cards[Index].Tint, sizeof(float) * 3);
		Data[FlatFloatIndex] = Cards[Index].Flat;
		Instances->SetCustomData(Index, MakeArrayView(Data, CustomDataFloats), false);
	}
	Instances->MarkRenderStateDirty();
	return Instances;
}

UInstancedStaticMeshComponent* ArenaCrowd::Build(AActor* Owner, const FArenaLayout& Layout, bool bSwitchedOff)
{
	if (bSwitchedOff)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_ARENA_CROWD off (-MageArenaNoCrowd)"));
		return nullptr;
	}
	UStaticMesh* Card = LoadObject<UStaticMesh>(nullptr, CardMeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *ObjectPath(MaterialName), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Card || !Material)
	{
		UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_ARENA_CROWD missing=%s"), Card ? MaterialName : TEXT("Plane"));
		return nullptr;
	}
	TArray<FCard> Cards = Place(Layout);
	const int32 Figures = Cards.Num();
	Cards.Append(PlaceBanners(Layout));
	UInstancedStaticMeshComponent* Instances = Spawn(Owner, Cards, Card, Material);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_ARENA_CROWD cards=%d banners=%d components=%d"), Instances ? Figures : 0, Instances ? Cards.Num() - Figures : 0, Instances ? 1 : 0);
	return Instances;
}
