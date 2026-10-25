#include "Greybox/CreatureImport.h"

#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/World.h"
#include "Greybox/ArenaLayout.h"
#include "MageArenaVR.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"

namespace CreatureImport
{
namespace
{
// Scene spots at 8 m: the near-centre spawn marker's post stands on the centre line, so nobody stands there. Brennic
// (1.03 m across) 1.4 m left, the hound (2.59 m long, toward the player) 1.4 m right, and the maw (2.38 x 2.23 m, turned
// about 27 deg to face the player, so 3.1 m across) 4.0 m left: 0.5 m clear of Brennic, 1.3 m of the near-left post.
const FCreatureSpec GSpecs[] = {
    // docs/research/BRENNIC-MESH-REWORK-2026-10-24.md: 23 bones, 1.7992 m, 23,990 tris.
    {TEXT("Brennic"), 1.7992, false, -1.4},
    // docs/research/CREATURE-MESH-2026-10-24.md: 26 bones, 1.3569 m, 11,796 tris after parts and rig.
    {TEXT("CinderHound"), 1.3569, true, 1.4},
    // docs/research/MAW-MV-2026-10-24.md and CREATURE-CAST-UE-2026-10-24.md: 11 bones, 1.2509 m, 17,752 tris after the
    // tongue part. Slots M_MireMaw (the seam-fixed bake) and M_MireMaw_Tongue (a capsule, flat dark red). The merge points
    // the tongue at texel (571, 658) of T_MireMaw_BaseColor.png, sRGB (112, 44, 46): every texel within 8 px of it is within
    // 0.102 of the flat colour's sRGB (124, 29, 29), the closest such patch on the bake (DRAWCALL-BUDGET-2026-10-25 3.1).
    {TEXT("MireMaw"), 1.2509, false, -4.0, TEXT("Tongue"), FLinearColor(0.20f, 0.012f, 0.012f), FVector2f(0.279053f, 0.321533f)},
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

FString FlatMaterialObjectPath(const FString& Name)
{
	return FString::Printf(TEXT("%s/M_%s_Flat.M_%s_Flat"), *FolderPath(Name), *Name, *Name);
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

const TCHAR* EmberSoftHlsl()
{
	// Tex is the base colour texture object (its sampler is TexSampler), UV the mesh's UV. The key is EmberHlsl's, on
	// pow(C, 1/2.2), over 12 taps: 6 on the ring SoftRadiusUv and 6 on half of it, offset by 30 deg. Returns the mean.
	return TEXT(
	    "float Acc = 0.0;\n"
	    "for (int I = 0; I < 12; I++)\n"
	    "{\n"
	    "    float A = I * 0.5235988;\n"
	    "    float R = (I % 2 == 0) ? SoftRadiusUv : SoftRadiusUv * 0.5;\n"
	    "    float3 S = pow(saturate(Texture2DSampleLevel(Tex, TexSampler, UV + R * float2(cos(A), sin(A)), 0).rgb), 1.0 / 2.2);\n"
	    "    float Mx = max(S.r, max(S.g, S.b));\n"
	    "    float Dl = Mx - min(S.r, min(S.g, S.b));\n"
	    "    float Sat = Mx > 0.0 ? Dl / Mx : 0.0;\n"
	    "    float Hue = 0.0;\n"
	    "    if (Dl > 1e-5)\n"
	    "    {\n"
	    "        if (Mx == S.r) { Hue = fmod((S.g - S.b) / Dl + 6.0, 6.0); }\n"
	    "        else if (Mx == S.g) { Hue = (S.b - S.r) / Dl + 2.0; }\n"
	    "        else { Hue = (S.r - S.g) / Dl + 4.0; }\n"
	    "    }\n"
	    "    Hue *= 60.0;\n"
	    "    float HueKey = 1.0 - smoothstep(HueWidthDeg * 0.5, HueWidthDeg, abs(Hue - HueCentreDeg));\n"
	    "    Acc += HueKey * smoothstep(SatLow, SatHigh, Sat) * smoothstep(ValLow, ValHigh, Mx);\n"
	    "}\n"
	    "return Acc / 12.0;\n");
}

double EmberSoftMask(TConstArrayView<FLinearColor> SrgbTaps, const FEmberParams& Params)
{
	double Sum = 0.0;
	for (const FLinearColor& Tap : SrgbTaps)
	{
		Sum += EmberMask(Tap, Params);
	}
	return SrgbTaps.Num() > 0 ? Sum / SrgbTaps.Num() : 0.0;
}

const FCreatureBudget& DefaultBudget()
{
	static const FCreatureBudget Budget;
	return Budget;
}

FCreatureBudget UncutBudget()
{
	FCreatureBudget Budget;
	Budget.bMergeSections = false;
	Budget.bPerBoneVelocity = true;
	Budget.Shadow = ECreatureShadow::Cascades;
	Budget.ShadowMaxDistanceM = 0.0;
	Budget.bGenerateLods = false;
	return Budget;
}

FCreatureBudget BudgetFromCommandLine()
{
	const TCHAR* Line = FCommandLine::Get();
	FCreatureBudget Budget = FParse::Param(Line, TEXT("MageArenaCreatureUncut")) ? UncutBudget() : DefaultBudget();
	int32 Switch = 0;
	if (FParse::Value(Line, TEXT("MageArenaCreatureMerge="), Switch))
	{
		Budget.bMergeSections = Switch != 0;
	}
	if (FParse::Value(Line, TEXT("MageArenaCreatureVelocity="), Switch))
	{
		Budget.bPerBoneVelocity = Switch != 0;
	}
	if (FParse::Value(Line, TEXT("MageArenaCreatureLods="), Switch))
	{
		Budget.bGenerateLods = Switch != 0;
	}
	FParse::Value(Line, TEXT("MageArenaCreatureForceLod="), Budget.ForcedLod);
	FParse::Value(Line, TEXT("MageArenaCreatureShadowMaxM="), Budget.ShadowMaxDistanceM);
	FString Shadow;
	if (FParse::Value(Line, TEXT("MageArenaCreatureShadow="), Shadow))
	{
		Budget.Shadow = Shadow == TEXT("off") ? ECreatureShadow::Off : Shadow == TEXT("inset") ? ECreatureShadow::Inset
		                                                                                       : ECreatureShadow::Cascades;
	}
	return Budget;
}

double ScreenSizeAt(double SphereRadiusCm, double DistanceM, double ScreenMultiple)
{
	return 2.0 * ScreenMultiple * SphereRadiusCm / FMath::Max(1.0, DistanceM * 100.0);
}

bool CastsShadowAt(const FCreatureBudget& Budget, double DistanceM)
{
	return Budget.Shadow != ECreatureShadow::Off && (Budget.ShadowMaxDistanceM <= 0.0 || DistanceM <= Budget.ShadowMaxDistanceM);
}

FSectionMerge PlanSectionMerge(TConstArrayView<FName> SlotNames, TConstArrayView<int32> TriangleGroups, TConstArrayView<int32> TriangleCorners, const FCreatureSpec& Spec)
{
	FSectionMerge Merge;
	TArray<int32> Counts;
	Counts.Init(0, SlotNames.Num());
	for (const int32 Group : TriangleGroups)
	{
		if (Counts.IsValidIndex(Group))
		{
			++Counts[Group];
		}
	}
	TSet<FName> Used;
	for (int32 Group = 0; Group < SlotNames.Num(); ++Group)
	{
		if (Counts[Group] > 0)
		{
			Used.Add(SlotNames[Group]);
			if (Merge.TargetGroup == INDEX_NONE || Counts[Group] > Counts[Merge.TargetGroup])
			{
				Merge.TargetGroup = Group;
			}
		}
	}
	Merge.SlotsBefore = Used.Num();
	Merge.SlotsAfter = Used.Num();
	if (Used.Num() < 2)
	{
		return Merge;
	}
	Merge.SlotsAfter = 1;
	Merge.TrianglesMoved = TriangleGroups.Num() - Counts[Merge.TargetGroup];
	if (!Spec.FlatSlot)
	{
		return Merge;
	}
	// A corner is the flat slot's own when no body triangle uses it too.
	TSet<int32> InFlat;
	TSet<int32> InBody;
	for (int32 Triangle = 0; Triangle < TriangleGroups.Num(); ++Triangle)
	{
		const int32 Group = TriangleGroups[Triangle];
		const bool bFlat = SlotNames.IsValidIndex(Group) && Group != Merge.TargetGroup && SlotNames[Group].ToString().Contains(Spec.FlatSlot);
		for (int32 Corner = 0; Corner < 3 && 3 * Triangle + Corner < TriangleCorners.Num(); ++Corner)
		{
			(bFlat ? InFlat : InBody).Add(TriangleCorners[3 * Triangle + Corner]);
		}
	}
	for (const int32 Corner : InFlat)
	{
		if (InBody.Contains(Corner))
		{
			++Merge.FlatCornersShared;
		}
		else
		{
			Merge.FlatCorners.Add(Corner);
		}
	}
	Merge.FlatCorners.Sort();
	return Merge;
}

FSectionMerge MergeSections(FMeshDescription& Description, const FCreatureSpec& Spec)
{
	// The attribute names of MeshAttribute::PolygonGroup::ImportedMaterialSlotName and VertexInstance::TextureCoordinate,
	// spelled out: the module links Engine, not MeshDescription, so only the description's inline accessors are used.
	TMeshAttributesRef<FPolygonGroupID, FName> SlotNames = Description.PolygonGroupAttributes().GetAttributesRef<FName>(FName(TEXT("ImportedMaterialSlotName")));
	TMeshAttributesRef<FVertexInstanceID, FVector2f> Uvs = Description.VertexInstanceAttributes().GetAttributesRef<FVector2f>(FName(TEXT("TextureCoordinate")));
	if (!SlotNames.IsValid())
	{
		return FSectionMerge();
	}
	TArray<FPolygonGroupID> Groups;
	TArray<FName> Names;
	TMap<int32, int32> GroupIndex;
	for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
	{
		GroupIndex.Add(Group.GetValue(), Groups.Num());
		Groups.Add(Group);
		Names.Add(SlotNames[Group]);
	}
	TArray<int32> TriangleGroups;
	TArray<int32> TriangleCorners;
	for (const FTriangleID Triangle : Description.Triangles().GetElementIDs())
	{
		const int32* Index = GroupIndex.Find(Description.GetTrianglePolygonGroup(Triangle).GetValue());
		TriangleGroups.Add(Index ? *Index : INDEX_NONE);
		const TArrayView<const FVertexInstanceID> Corners = Description.GetTriangleVertexInstances(Triangle);
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			TriangleCorners.Add(Corners.IsValidIndex(Corner) ? Corners[Corner].GetValue() : INDEX_NONE);
		}
	}
	const FSectionMerge Merge = PlanSectionMerge(Names, TriangleGroups, TriangleCorners, Spec);
	if (Merge.SlotsAfter >= Merge.SlotsBefore)
	{
		return Merge;
	}
	if (Uvs.IsValid())
	{
		for (const int32 Corner : Merge.FlatCorners)
		{
			for (int32 Channel = 0; Channel < Uvs.GetNumChannels(); ++Channel)
			{
				Uvs.Set(FVertexInstanceID(Corner), Channel, Spec.FlatSlotUv);
			}
		}
	}
	const FName Target = Names[Merge.TargetGroup];
	for (const FPolygonGroupID Group : Groups)
	{
		SlotNames.Set(Group, Target);
	}
	return Merge;
}

#if WITH_EDITOR
namespace
{
/** Sections and triangles per LOD of the render data, for the log. */
FString DescribeLods(const USkeletalMesh& Mesh)
{
	FString Out;
	const FSkeletalMeshRenderData* RenderData = Mesh.GetResourceForRendering();
	for (int32 Lod = 0; RenderData && Lod < RenderData->LODRenderData.Num(); ++Lod)
	{
		const FSkeletalMeshLODInfo* Info = Mesh.GetLODInfo(Lod);
		Out += FString::Printf(TEXT(" lod%d=%d tris/%d sections@%.3f"), Lod, RenderData->LODRenderData[Lod].GetTotalFaces(), RenderData->LODRenderData[Lod].RenderSections.Num(),
		    Info ? Info->ScreenSize.Default : 0.0f);
	}
	return Out;
}
}
#endif

void PrepareMesh(USkeletalMesh& Mesh, const FCreatureSpec& Spec, const FCreatureBudget& Budget)
{
#if WITH_EDITOR
	// Once per asset: the capture and the budget scene spawn each creature once, and a second pass would merge nothing
	// and find its LODs already there.
	static TSet<FString> Prepared;
	const FString Path = Mesh.GetPathName();
	if (Prepared.Contains(Path))
	{
		return;
	}
	Prepared.Add(Path);
	bool bRebuild = false;
	FSectionMerge Merge;
	if (Budget.bMergeSections && Mesh.HasMeshDescription(0))
	{
		if (FMeshDescription* Description = Mesh.GetMeshDescription(0))
		{
			Merge = MergeSections(*Description, Spec);
			if (Merge.SlotsAfter < Merge.SlotsBefore)
			{
				USkeletalMesh::FCommitMeshDescriptionParams Params;
				Params.bMarkPackageDirty = false;
				Mesh.CommitMeshDescription(0, Params);
				bRebuild = true;
			}
		}
	}
	if (Budget.bGenerateLods && Mesh.GetLODNum() == 1 && Mesh.GetImportedModel())
	{
		const double Radius = Mesh.GetBounds().SphereRadius;
		const double Distances[] = {Budget.Lod1DistanceM, Budget.Lod2DistanceM};
		const double Shares[] = {Budget.Lod1TriangleShare, Budget.Lod2TriangleShare};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Distances); ++Index)
		{
			FSkeletalMeshLODInfo& Info = Mesh.AddLODInfo();
			Info.ScreenSize = FPerPlatformFloat(static_cast<float>(ScreenSizeAt(Radius, Distances[Index])));
			Info.ReductionSettings.TerminationCriterion = SMTC_NumOfTriangles;
			Info.ReductionSettings.NumOfTrianglesPercentage = static_cast<float>(Shares[Index]);
			Info.ReductionSettings.BaseLOD = 0;
			Info.bHasBeenSimplified = true;
			// The reduction fills the LOD's model; the builder walks the models, so each generated LOD needs its slot.
			if (Mesh.GetImportedModel()->LODModels.Num() < Mesh.GetLODNum())
			{
				Mesh.GetImportedModel()->LODModels.Add(new FSkeletalMeshLODModel());
			}
		}
		bRebuild = true;
	}
	if (bRebuild)
	{
		Mesh.Build();
		FAssetCompilingManager::Get().FinishCompilationForObjects({&Mesh});
	}
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CREATURE_BUDGET mesh %s merge=%d slots=%d->%d moved=%d flatUvs=%d shared=%d lods=%d%s"), Spec.Name, Budget.bMergeSections ? 1 : 0,
	    Merge.SlotsBefore, Merge.SlotsAfter, Merge.TrianglesMoved, Merge.FlatCorners.Num(), Merge.FlatCornersShared, Mesh.GetLODNum(), *DescribeLods(Mesh));
#endif
}

void ApplyComponentBudget(USkeletalMeshComponent& Component, double DistanceM, const FCreatureBudget& Budget)
{
	const bool bCasts = CastsShadowAt(Budget, DistanceM);
	Component.SetCastShadow(bCasts);
	Component.bCastInsetShadow = bCasts && Budget.Shadow == ECreatureShadow::Inset;
	Component.bPerBoneMotionBlur = Budget.bPerBoneVelocity;
	// SetForcedLOD counts from 1 (0 picks by screen size).
	Component.SetForcedLOD(Budget.ForcedLod >= 0 ? Budget.ForcedLod + 1 : 0);
	Component.MarkRenderStateDirty();
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
	const FCreatureBudget Budget = BudgetFromCommandLine();
	if (const FCreatureSpec* Spec = FindSpec(Name))
	{
		PrepareMesh(*Mesh, *Spec, Budget);
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
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ApplyComponentBudget(*Component, DistanceFromViewM(Layout, PointCm), Budget);
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
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CREATURE_BUDGET spawned %s distance=%.2f m shadow=%d inset=%d perBoneVelocity=%d forcedLod=%d sphere=%.1f cm"), *Name,
	    DistanceFromViewM(Layout, PointCm), Component->CastShadow ? 1 : 0, Component->bCastInsetShadow ? 1 : 0, Component->bPerBoneMotionBlur ? 1 : 0, Budget.ForcedLod,
	    Bounds.SphereRadius);
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

double DistanceFromViewM(const FArenaLayout& Layout, const FVector& PointCm)
{
	const FRunePad* Centre = Layout.FindPad(1);
	const FVector Pad = (Centre ? Centre->PositionM : FVector(-12.0, 0.0, 0.0)) * 100.0;
	return FVector2D(PointCm.X - Pad.X, PointCm.Y - Pad.Y).Size() / 100.0;
}

TArray<ASkeletalMeshActor*> SpawnScene(UWorld& World, const FArenaLayout& Layout)
{
	TArray<ASkeletalMeshActor*> Spawned;
	const double Distance = OpponentDistanceM();
	FString Only;
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaCreatureOnly="), Only);
	for (const FCreatureSpec& Spec : GSpecs)
	{
		if (!Only.IsEmpty() && Only != Spec.Name)
		{
			continue;
		}
		if (ASkeletalMeshActor* Actor = SpawnCreature(World, Layout, Spec.Name, StandPointCm(Layout, Distance, Spec.SceneLateralM)))
		{
			Spawned.Add(Actor);
		}
	}
	return Spawned;
}
}
