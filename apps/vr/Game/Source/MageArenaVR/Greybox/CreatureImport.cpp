#include "Greybox/CreatureImport.h"

#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Greybox/ArenaLayout.h"
#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ReferenceSkeleton.h"

namespace CreatureImport
{
namespace
{
const FCreatureSpec GSpecs[] = {
    // docs/research/BRENNIC-MESH-REWORK-2026-10-24.md: 23 bones, 1.7992 m, 23,990 tris.
    {TEXT("Brennic"), 1.7992, false},
    // docs/research/CREATURE-MESH-2026-10-24.md: 26 bones, 1.3569 m, 11,796 tris after parts and rig.
    {TEXT("CinderHound"), 1.3569, true},
};

double SmoothStep(double Low, double High, double X)
{
	const double T = FMath::Clamp((X - Low) / FMath::Max(High - Low, 1.0e-6), 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

/** The ref-pose component-space location of a bone (cm), walking its parents. */
FVector RefComponentLocation(const FReferenceSkeleton& Skeleton, int32 Bone)
{
	FTransform Accumulated = FTransform::Identity;
	while (Bone != INDEX_NONE)
	{
		Accumulated = Accumulated * Skeleton.GetRefBonePose()[Bone];
		Bone = Skeleton.GetParentIndex(Bone);
	}
	return Accumulated.GetLocation();
}

/**
 * The mesh's forward in component space, from bones that sit ahead of the body on both rigs: the head (far forward on
 * the hound, slightly forward on Brennic) and the balls of the feet (Brennic's toes). Both FBX were exported facing -Y in
 * Blender by rig_anim.py; this reads the result of the import instead of assuming the axis conversion.
 */
FVector MeshForward(const USkeletalMesh& Mesh)
{
	const FReferenceSkeleton& Skeleton = Mesh.GetRefSkeleton();
	FVector Sum = FVector::ZeroVector;
	for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
	{
		const FString Name = Skeleton.GetBoneName(Bone).ToString().ToLower();
		if (Name == TEXT("head") || Name.StartsWith(TEXT("ball_")))
		{
			Sum += RefComponentLocation(Skeleton, Bone);
		}
	}
	Sum.Z = 0.0;
	return Sum.GetSafeNormal();
}

UAnimSequence* FindIdle(const FString& Name)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FString Folder = FolderPath(Name);
	Registry.ScanPathsSynchronous({Folder}, true);
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*Folder), Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetName.ToString().Contains(TEXT("Idle")) && Asset.AssetClassPath == UAnimSequence::StaticClass()->GetClassPathName())
		{
			return Cast<UAnimSequence>(Asset.GetAsset());
		}
	}
	return nullptr;
}
}

TConstArrayView<FCreatureSpec> Specs()
{
	return GSpecs;
}

const FCreatureSpec* FindSpec(const FString& Name)
{
	for (const FCreatureSpec& Spec : GSpecs)
	{
		if (Name == Spec.Name)
		{
			return &Spec;
		}
	}
	return nullptr;
}

FString FolderPath(const FString& Name)
{
	return FString::Printf(TEXT("%s/%s"), ScratchRoot, *Name);
}

FString MeshObjectPath(const FString& Name)
{
	return FString::Printf(TEXT("%s/SK_%s.SK_%s"), *FolderPath(Name), *Name, *Name);
}

FString TextureObjectPath(const FString& Name)
{
	return FString::Printf(TEXT("%s/T_%s_BaseColor.T_%s_BaseColor"), *FolderPath(Name), *Name, *Name);
}

FString MaterialObjectPath(const FString& Name)
{
	return FString::Printf(TEXT("%s/M_%s.M_%s"), *FolderPath(Name), *Name, *Name);
}

const FEmberParams& DefaultEmber()
{
	static const FEmberParams Params;
	return Params;
}

double EmberMask(const FLinearColor& Srgb, const FEmberParams& Params)
{
	const double R = FMath::Clamp<double>(Srgb.R, 0.0, 1.0);
	const double G = FMath::Clamp<double>(Srgb.G, 0.0, 1.0);
	const double B = FMath::Clamp<double>(Srgb.B, 0.0, 1.0);
	const double Max = FMath::Max3(R, G, B);
	const double Delta = Max - FMath::Min3(R, G, B);
	const double Sat = Max > 0.0 ? Delta / Max : 0.0;
	double Hue = 0.0;
	if (Delta > 1.0e-5)
	{
		if (Max == R)
		{
			Hue = FMath::Fmod((G - B) / Delta + 6.0, 6.0);
		}
		else if (Max == G)
		{
			Hue = (B - R) / Delta + 2.0;
		}
		else
		{
			Hue = (R - G) / Delta + 4.0;
		}
	}
	Hue *= 60.0;
	const double HueKey = 1.0 - SmoothStep(Params.HueWidthDeg * 0.5, Params.HueWidthDeg, FMath::Abs(Hue - Params.HueCentreDeg));
	return HueKey * SmoothStep(Params.SatLow, Params.SatHigh, Sat) * SmoothStep(Params.ValLow, Params.ValHigh, Max);
}

const TCHAR* EmberHlsl()
{
	// C is the sampled base colour, linear (the texture is sRGB). The key was measured on the encoded PNG, so it is
	// taken on pow(C, 1/2.2). Returns the shaded body colour in rgb and the crack mask in a.
	return TEXT(
	    "float3 S = pow(saturate(C), 1.0 / 2.2);\n"
	    "float Mx = max(S.r, max(S.g, S.b));\n"
	    "float Dl = Mx - min(S.r, min(S.g, S.b));\n"
	    "float Sat = Mx > 0.0 ? Dl / Mx : 0.0;\n"
	    "float Hue = 0.0;\n"
	    "if (Dl > 1e-5)\n"
	    "{\n"
	    "    if (Mx == S.r) { Hue = fmod((S.g - S.b) / Dl + 6.0, 6.0); }\n"
	    "    else if (Mx == S.g) { Hue = (S.b - S.r) / Dl + 2.0; }\n"
	    "    else { Hue = (S.r - S.g) / Dl + 4.0; }\n"
	    "}\n"
	    "Hue *= 60.0;\n"
	    "float HueKey = 1.0 - smoothstep(HueWidthDeg * 0.5, HueWidthDeg, abs(Hue - HueCentreDeg));\n"
	    "float Mask = HueKey * smoothstep(SatLow, SatHigh, Sat) * smoothstep(ValLow, ValHigh, Mx);\n"
	    "float3 Lum = float3(0.2126, 0.7152, 0.0722);\n"
	    "float TintLum = max(dot(BodyTint, Lum), 1e-4);\n"
	    "float3 Body = lerp(C, BodyTint * (dot(C, Lum) / TintLum) * BodyGain, BodyTintAmount);\n"
	    "return float4(lerp(Body, C, Mask), Mask);\n");
}

bool IsSceneRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("MageArenaCreatureImportScene")) || FParse::Param(FCommandLine::Get(), TEXT("MageArenaCreatureImportCapture"));
}

double OpponentDistanceM()
{
	// The near-centre spawn marker's distance (arena-layout.json spawnMarkers: 8 m from the centre pad), where the
	// greybox rival and the nearest creatures stand in a duel.
	double Distance = 8.0;
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaCreatureDistanceM="), Distance);
	return Distance;
}

FVector StandPointCm(const FArenaLayout& Layout, double DistanceM, double LateralM)
{
	const FRunePad* Centre = Layout.FindPad(1);
	const FVector Pad = Centre ? Centre->PositionM : FVector(-12.0, 0.0, 0.0);
	// The centre pad faces +X (arena-layout.json spawnArc); +Y is the seated player's right.
	const double X = Pad.X + DistanceM;
	const double Y = Pad.Y + LateralM;
	return FVector(X, Y, Layout.SurfaceHeightM(X, Y)) * 100.0;
}

ASkeletalMeshActor* SpawnCreature(UWorld& World, const FArenaLayout& Layout, const FString& Name, const FVector& PointCm)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshObjectPath(Name));
	if (!Mesh)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CREATURE_IMPORT fail %s does not load (run ImportCreatureMesh first)"), *MeshObjectPath(Name));
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkeletalMeshActor* Actor = World.SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(PointCm), Params);
	if (!Actor)
	{
		return nullptr;
	}
	USkeletalMeshComponent* Component = Actor->GetSkeletalMeshComponent();
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetSkeletalMesh(Mesh);
	Component->SetCastShadow(true);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceCreature(*Actor, Layout, PointCm, 0.0);
	UAnimSequence* Idle = FindIdle(Name);
	if (Idle)
	{
		Component->PlayAnimation(Idle, true);
	}
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector Forward = MeshForward(*Mesh);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CREATURE_IMPORT spawned %s at (%.0f, %.0f, %.0f) cm yaw=%.1f forward=(%.2f, %.2f) height=%.1f cm materials=%d idle=%s"),
	    *Name, PointCm.X, PointCm.Y, PointCm.Z, Actor->GetActorRotation().Yaw, Forward.X, Forward.Y, Bounds.BoxExtent.Z * 2.0, Mesh->GetMaterials().Num(),
	    Idle ? *Idle->GetName() : TEXT("none"));
	return Actor;
}

void PlaceCreature(ASkeletalMeshActor& Actor, const FArenaLayout& Layout, const FVector& PointCm, double YawOffsetDeg)
{
	const USkeletalMesh* Mesh = Actor.GetSkeletalMeshComponent()->GetSkeletalMeshAsset();
	const FRunePad* Centre = Layout.FindPad(1);
	const FVector Pad = (Centre ? Centre->PositionM : FVector(-12.0, 0.0, 0.0)) * 100.0;
	const FVector ToPlayer = FVector(Pad.X - PointCm.X, Pad.Y - PointCm.Y, 0.0).GetSafeNormal();
	const FVector Forward = Mesh ? MeshForward(*Mesh) : FVector::ForwardVector;
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToPlayer.Y, ToPlayer.X) - FMath::Atan2(Forward.Y, Forward.X)) + YawOffsetDeg;
	Actor.SetActorLocationAndRotation(PointCm, FRotator(0.0, Yaw, 0.0));
}

TArray<ASkeletalMeshActor*> SpawnScene(UWorld& World, const FArenaLayout& Layout)
{
	TArray<ASkeletalMeshActor*> Spawned;
	const double Distance = OpponentDistanceM();
	if (ASkeletalMeshActor* Brennic = SpawnCreature(World, Layout, TEXT("Brennic"), StandPointCm(Layout, Distance, -SceneHalfSpacingM)))
	{
		Spawned.Add(Brennic);
	}
	if (ASkeletalMeshActor* Hound = SpawnCreature(World, Layout, TEXT("CinderHound"), StandPointCm(Layout, Distance, SceneHalfSpacingM)))
	{
		Spawned.Add(Hound);
	}
	return Spawned;
}
}
