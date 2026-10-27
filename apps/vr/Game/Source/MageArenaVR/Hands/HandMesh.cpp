#include "Hands/HandMesh.h"

#include "Components.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"

namespace
{
using K = EHandKeypoint;

/** One drawn bone: a finger segment along a phalanx, or a palm block along a metacarpal or carpal. */
struct FHandBone
{
	K From;
	K To;
	EHandPart Part;
	/** Finger: diameter. Palm: width across the hand. */
	double WidthCm;
	/** Thickness from palm to back of the hand (a finger segment is round: thickness = width). */
	double ThickCm;
};

// Chunky heroic proportions (VISUAL-PLAN section c, "big hands"): each finger is a little wider than its share of the
// clips' knuckle line (about 1.7 cm), so neighbouring fingers touch as in S1 and never gap. The metacarpal and carpal
// blocks fan from the wrist and overlap into one palm.
const FHandBone Bones[] = {
    {K::Wrist, K::Palm, EHandPart::Palm, 5.4, 3.0},
    {K::Wrist, K::ThumbMetacarpal, EHandPart::Palm, 2.6, 2.8},
    {K::ThumbMetacarpal, K::ThumbProximal, EHandPart::Finger, 2.3, 2.3},
    {K::ThumbProximal, K::ThumbDistal, EHandPart::Finger, 2.15, 2.15},
    {K::ThumbDistal, K::ThumbTip, EHandPart::Finger, 2.0, 2.0},
    {K::Wrist, K::IndexMetacarpal, EHandPart::Palm, 2.5, 2.7},
    {K::IndexMetacarpal, K::IndexProximal, EHandPart::Palm, 2.6, 2.5},
    {K::IndexProximal, K::IndexIntermediate, EHandPart::Finger, 1.95, 1.95},
    {K::IndexIntermediate, K::IndexDistal, EHandPart::Finger, 1.8, 1.8},
    {K::IndexDistal, K::IndexTip, EHandPart::Finger, 1.7, 1.7},
    {K::Wrist, K::MiddleMetacarpal, EHandPart::Palm, 2.5, 2.7},
    {K::MiddleMetacarpal, K::MiddleProximal, EHandPart::Palm, 2.6, 2.5},
    {K::MiddleProximal, K::MiddleIntermediate, EHandPart::Finger, 2.0, 2.0},
    {K::MiddleIntermediate, K::MiddleDistal, EHandPart::Finger, 1.85, 1.85},
    {K::MiddleDistal, K::MiddleTip, EHandPart::Finger, 1.75, 1.75},
    {K::Wrist, K::RingMetacarpal, EHandPart::Palm, 2.4, 2.6},
    {K::RingMetacarpal, K::RingProximal, EHandPart::Palm, 2.5, 2.4},
    {K::RingProximal, K::RingIntermediate, EHandPart::Finger, 1.9, 1.9},
    {K::RingIntermediate, K::RingDistal, EHandPart::Finger, 1.75, 1.75},
    {K::RingDistal, K::RingTip, EHandPart::Finger, 1.65, 1.65},
    {K::Wrist, K::LittleMetacarpal, EHandPart::Palm, 2.3, 2.5},
    {K::LittleMetacarpal, K::LittleProximal, EHandPart::Palm, 2.4, 2.3},
    {K::LittleProximal, K::LittleIntermediate, EHandPart::Finger, 1.7, 1.7},
    {K::LittleIntermediate, K::LittleDistal, EHandPart::Finger, 1.55, 1.55},
    {K::LittleDistal, K::LittleTip, EHandPart::Finger, 1.45, 1.45},
};

/** A part runs this share of its width past each joint, so neighbouring parts overlap and no seam opens on a bend. */
constexpr double JointOverlap = 0.4;
/** The forearm block behind the wrist, so the hand does not end in a cut. */
constexpr double ForearmLengthCm = 11.0;
constexpr double ForearmWidthCm = 5.0;
constexpr double ForearmThickCm = 3.6;

/** Unit mesh shapes: a superellipsoid in the -0.5..0.5 box, Z along the bone. */
struct FShape
{
	/** Cross-section exponent: 1 is round, below 1 is a rounded rectangle. */
	double Around;
	/** Length exponent: below 1 gives straight sides and rounded ends. */
	double Along;
	/** Cross-section scale at +Z against -Z (a finger narrows toward its tip). */
	double Taper;
	int32 Sides;
	int32 Rings;
};

const FShape FingerShape = {1.0, 0.55, 0.82, 12, 10};
const FShape PalmShape = {0.45, 0.5, 1.0, 12, 10};

double SignedPow(double Value, double Exponent)
{
	return FMath::Sign(Value) * FMath::Pow(FMath::Abs(Value), Exponent);
}

void ShapePoints(const FShape& Shape, TArray<FVector3f>& Points, TArray<FIntVector>& Triangles)
{
	// Poles at both ends, Rings - 1 rings of Sides points between them.
	Points.Reset();
	Triangles.Reset();
	auto Point = [&Shape](double Lat, double Lon)
	{
		const double Ring = SignedPow(FMath::Cos(Lat), Shape.Along);
		const double Z = 0.5 * SignedPow(FMath::Sin(Lat), Shape.Along);
		const double Scale = FMath::Lerp(1.0, Shape.Taper, Z + 0.5);
		return FVector3f(
		    static_cast<float>(0.5 * Ring * SignedPow(FMath::Cos(Lon), Shape.Around) * Scale),
		    static_cast<float>(0.5 * Ring * SignedPow(FMath::Sin(Lon), Shape.Around) * Scale),
		    static_cast<float>(Z));
	};
	Points.Add(FVector3f(0.f, 0.f, -0.5f));
	for (int32 Ring = 1; Ring < Shape.Rings; ++Ring)
	{
		const double Lat = -HALF_PI + PI * Ring / Shape.Rings;
		for (int32 Side = 0; Side < Shape.Sides; ++Side)
		{
			Points.Add(Point(Lat, 2.0 * PI * Side / Shape.Sides));
		}
	}
	Points.Add(FVector3f(0.f, 0.f, 0.5f));
	const int32 Top = Points.Num() - 1;
	auto RingPoint = [&Shape](int32 Ring, int32 Side)
	{ return 1 + (Ring - 1) * Shape.Sides + (Side % Shape.Sides); };
	for (int32 Side = 0; Side < Shape.Sides; ++Side)
	{
		Triangles.Add(FIntVector(0, RingPoint(1, Side), RingPoint(1, Side + 1)));
		for (int32 Ring = 1; Ring < Shape.Rings - 1; ++Ring)
		{
			Triangles.Add(FIntVector(RingPoint(Ring, Side), RingPoint(Ring + 1, Side), RingPoint(Ring + 1, Side + 1)));
			Triangles.Add(FIntVector(RingPoint(Ring, Side), RingPoint(Ring + 1, Side + 1), RingPoint(Ring, Side + 1)));
		}
		Triangles.Add(FIntVector(RingPoint(Shape.Rings - 1, Side), Top, RingPoint(Shape.Rings - 1, Side + 1)));
	}

	// Fit the box exactly, so a part's SizeCm is the size it draws at (Greybox::AddSized divides by the bounds).
	FBox3f Box(ForceInit);
	for (const FVector3f& P : Points)
	{
		Box += P;
	}
	const FVector3f Centre = Box.GetCenter();
	const FVector3f Size = Box.GetSize();
	for (FVector3f& P : Points)
	{
		P = (P - Centre) / Size;
	}

	// The engine's front face: (P2 - P0) x (P1 - P0) points out. The shape is convex about its centre.
	for (FIntVector& T : Triangles)
	{
		const FVector3f Out = (Points[T.Z] - Points[T.X]) ^ (Points[T.Y] - Points[T.X]);
		if ((Out | (Points[T.X] + Points[T.Y] + Points[T.Z])) < 0.f)
		{
			Swap(T.Y, T.Z);
		}
	}
}
}

void MageHandMesh::PoseParts(const FHandFrame& Frame, TArray<FHandPartPose>& Out)
{
	Out.Reset();
	if (Frame.Joints.Num() < MageHandJointCount)
	{
		return;
	}
	auto At = [&Frame](K Key)
	{ return Frame.Joints[static_cast<int32>(Key)].Location; };
	const FVector Wrist = At(K::Wrist);
	// The palm plane from the wrist and the index and little knuckles. Its side does not matter: every part is symmetric.
	FVector Normal = FVector::CrossProduct(At(K::IndexProximal) - Wrist, At(K::LittleProximal) - Wrist).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;
	}
	auto Orient = [&Normal](const FVector& Along)
	{
		// Z along the bone, Y as near the palm normal as the bone allows (Y is the thickness axis).
		const FVector Side = FMath::Abs(FVector::DotProduct(Along, Normal)) > 0.98 ? FVector::ForwardVector : Normal;
		return FRotationMatrix::MakeFromZY(Along, Side).ToQuat();
	};
	for (const FHandBone& Bone : Bones)
	{
		const FVector A = At(Bone.From);
		const FVector B = At(Bone.To);
		const double Length = FVector::Distance(A, B);
		const FVector Along = Length > UE_KINDA_SMALL_NUMBER ? (B - A) / Length : FVector::ForwardVector;
		FHandPartPose& Pose = Out.AddDefaulted_GetRef();
		Pose.Part = Bone.Part;
		Pose.CentreCm = (A + B) * 0.5;
		Pose.Rotation = Orient(Along);
		Pose.SizeCm = FVector(Bone.WidthCm, Bone.ThickCm, Length + 2.0 * JointOverlap * Bone.WidthCm);
		Pose.From = static_cast<int32>(Bone.From);
		Pose.To = static_cast<int32>(Bone.To);
	}
	// The forearm continues the wrist away from the palm.
	const FVector Back = (Wrist - At(K::Palm)).GetSafeNormal();
	if (!Back.IsNearlyZero())
	{
		FHandPartPose& Pose = Out.AddDefaulted_GetRef();
		Pose.Part = EHandPart::Palm;
		Pose.CentreCm = Wrist + Back * (ForearmLengthCm * 0.5 - JointOverlap * ForearmWidthCm * 0.5);
		Pose.Rotation = Orient(Back);
		Pose.SizeCm = FVector(ForearmWidthCm, ForearmThickCm, ForearmLengthCm);
	}
}

UStaticMesh* MageHandMesh::BuildPartMesh(EHandPart Part, UObject* Outer, UMaterialInterface* Material)
{
	if (Part != EHandPart::Finger && Part != EHandPart::Palm)
	{
		return nullptr;
	}
	TArray<FVector3f> Points;
	TArray<FIntVector> Triangles;
	ShapePoints(Part == EHandPart::Finger ? FingerShape : PalmShape, Points, Triangles);

	// Smooth normals: each point averages its triangles' outward normals, weighted by area.
	TArray<FVector3f> Normals;
	Normals.Init(FVector3f::ZeroVector, Points.Num());
	for (const FIntVector& T : Triangles)
	{
		const FVector3f Face = (Points[T.Z] - Points[T.X]) ^ (Points[T.Y] - Points[T.X]);
		Normals[T.X] += Face;
		Normals[T.Y] += Face;
		Normals[T.Z] += Face;
	}

	// Filled straight into LOD0's render buffers (the engine's fast runtime build path without an FMeshDescription,
	// whose modules this target does not link). One vertex per point: the shape is smooth all over.
	TArray<FStaticMeshBuildVertex> Vertices;
	Vertices.SetNumZeroed(Points.Num());
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVector3f Normal = Normals[Index].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UnitZ());
		FVector3f Tangent = FVector3f::UnitZ() ^ Normal;
		if (Tangent.SizeSquared() < 1e-4f)
		{
			Tangent = FVector3f::UnitX();
		}
		Tangent.Normalize();
		FStaticMeshBuildVertex& Vertex = Vertices[Index];
		Vertex.Position = Points[Index];
		Vertex.TangentX = Tangent;
		Vertex.TangentY = Normal ^ Tangent;
		Vertex.TangentZ = Normal;
		Vertex.UVs[0] = FVector2f(FMath::Atan2(Points[Index].Y, Points[Index].X) / (2.f * PI) + 0.5f, Points[Index].Z + 0.5f);
		Vertex.Color = FColor::White;
	}
	TArray<uint32> Indices;
	Indices.Reserve(Triangles.Num() * 3);
	for (const FIntVector& T : Triangles)
	{
		Indices.Append({static_cast<uint32>(T.X), static_cast<uint32>(T.Y), static_cast<uint32>(T.Z)});
	}

	const TCHAR* Name = Part == EHandPart::Finger ? TEXT("SM_HandFinger") : TEXT("SM_HandPalm");
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer ? Outer : GetTransientPackage(), MakeUniqueObjectName(Outer ? Outer : GetTransientPackage(), UStaticMesh::StaticClass(), Name), RF_Transient);
	const FName Slot(TEXT("Hand"));
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, Slot, Slot));
	Mesh->NeverStream = true;
	Mesh->bDoFastBuild = true;
	Mesh->bSupportRayTracing = false;
	Mesh->SetRenderData(MakeUnique<FStaticMeshRenderData>());
	FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	RenderData->AllocateLODResources(1);
	FStaticMeshLODResources& Lod = RenderData->LODResources[0];
	Lod.VertexBuffers.PositionVertexBuffer.Init(Vertices);
	Lod.VertexBuffers.StaticMeshVertexBuffer.Init(Vertices, 1);
	Lod.VertexBuffers.ColorVertexBuffer.InitFromSingleColor(FColor::White, Vertices.Num());
	Lod.bHasColorVertexData = false;
	Lod.IndexBuffer.SetIndices(Indices, EIndexBufferStride::Force16Bit);
	FStaticMeshSection& Section = Lod.Sections.AddDefaulted_GetRef();
	Section.MaterialIndex = 0;
	Section.FirstIndex = 0;
	Section.NumTriangles = Triangles.Num();
	Section.MinVertexIndex = 0;
	Section.MaxVertexIndex = Vertices.Num() - 1;
	Section.bEnableCollision = false;
	Section.bCastShadow = false;
	RenderData->ScreenSize[0].Default = 1.f;
	FBox3f Box(Points);
	RenderData->Bounds = FBoxSphereBounds(FBox(Box));
	Mesh->InitResources();
	Mesh->CalculateExtendedBounds();
	return Mesh;
}
