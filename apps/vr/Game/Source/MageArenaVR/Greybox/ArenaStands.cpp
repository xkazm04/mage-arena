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
	FCrowdRng Rng{Seed};
	const int32 Segments = FMath::Max(Layout.WallSegments, 3);
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		if (IsAisle(Segment, Segments))
		{
			continue;
		}
		for (int32 Tier = 0; Tier < Layout.StandTiers; ++Tier)
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
				Card.Variant = PickVariant(Rng, Tier);
				const double Lateral = (Rng.Next() - 0.5) * LateralJitterM;
				const double YawJitter = (Rng.Next() - 0.5) * YawJitterDeg;
				Card.Scale = ScaleMin + Rng.Next() * ScaleSpan;
				Card.Segment = Segment;
				Card.Tier = Tier;
				// Alternate cards stand 6 cm further back, so overlapping neighbours never z-fight.
				const double Back = (Slot % 2) ? AlternateBackM : 0.0;
				const double Along = (Slot - (Slots - 1) * 0.5) * Step + Lateral;
				Card.FeetM = Box.MidM + Box.Along * Along + Box.Outward * Back;
				Card.FeetM.Z = Box.TopM;

				FVector Facing = Layout.ArenaCentreM - Card.FeetM;
				Facing.Z = 0.0;
				Facing = Facing.GetSafeNormal();
				if (Facing.IsNearlyZero())
				{
					Facing = -Box.Outward;
				}
				Card.YawDeg = FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X)) + YawJitter;

				// The plane's normal turns to face the centre and its U axis to the viewer's right (the viewer looks back
				// along the card's -X, so their right is the card's -Y). Its V then runs down or up the card; a V that
				// runs up swaps the rect's top and bottom instead of mirroring the mesh, so the front face stays front.
				const FVariant& Variant = VariantTable[Card.Variant];
				const FVector Normal(0.0, 0.0, 1.0);
				const FVector Right = -PlaneUAxis();
				const FMatrix Basis(Normal, Right, Normal ^ Right, FVector::ZeroVector);
				const FQuat PlaneToCard(Basis.GetTransposed());
				const bool bVDown = PlaneToCard.RotateVector(PlaneVAxis()).Z < 0.0;
				const FQuat Yaw = FRotator(0.0, Card.YawDeg, 0.0).Quaternion();
				const double Width = Variant.WidthM * Card.Scale;
				const double Height = Variant.HeightM * Card.Scale;
				// The plane's pivot is its centre: up half the figure, and across so the feet sit on the pivot.
				const FVector Centre = Card.FeetM + Yaw.RotateVector(FVector(0.0, -Variant.CentreRightM * Card.Scale, Height * 0.5));
				const bool bUAlongX = FMath::Abs(PlaneUAxis().X) > 0.5;
				const FVector Scale = bUAlongX ? FVector(Width, Height, 1.0) : FVector(Height, Width, 1.0);
				Card.Transform = FTransform(Yaw * PlaneToCard, Centre * 100.0, Scale);
				const float Top = static_cast<float>(1.0 - Variant.V1);
				const float Bottom = static_cast<float>(1.0 - Variant.V0);
				Card.Rect[0] = static_cast<float>(Variant.U0);
				Card.Rect[1] = bVDown ? Top : Bottom;
				Card.Rect[2] = static_cast<float>(Variant.U1);
				Card.Rect[3] = bVDown ? Bottom : Top;
				Cards.Add(Card);
			}
		}
	}
	return Cards;
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
		Instances->SetCustomData(Index, MakeArrayView(Cards[Index].Rect, CustomDataFloats), false);
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
	const TArray<FCard> Cards = Place(Layout);
	UInstancedStaticMeshComponent* Instances = Spawn(Owner, Cards, Card, Material);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_ARENA_CROWD cards=%d components=%d"), Instances ? Instances->GetInstanceCount() : 0, Instances ? 1 : 0);
	return Instances;
}
