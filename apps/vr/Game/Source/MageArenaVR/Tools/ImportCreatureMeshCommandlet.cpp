#include "Tools/ImportCreatureMeshCommandlet.h"

#include "Greybox/CreatureImport.h"
#include "MageArenaVR.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/SecureHash.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#endif

UImportCreatureMeshCommandlet::UImportCreatureMeshCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace
{
template <typename T>
T* AddExpression(UMaterial& Material, int32 X, int32 Y)
{
	T* Expression = NewObject<T>(&Material);
	Expression->MaterialExpressionEditorX = X;
	Expression->MaterialExpressionEditorY = Y;
	Material.GetExpressionCollection().AddExpression(Expression);
	return Expression;
}

UMaterialExpressionScalarParameter* AddScalar(UMaterial& Material, const TCHAR* Name, double Value, int32 Y)
{
	UMaterialExpressionScalarParameter* Parameter = AddExpression<UMaterialExpressionScalarParameter>(Material, -700, Y);
	Parameter->ParameterName = Name;
	Parameter->DefaultValue = static_cast<float>(Value);
	return Parameter;
}

UMaterialExpressionVectorParameter* AddVector(UMaterial& Material, const TCHAR* Name, const FLinearColor& Value, int32 Y)
{
	UMaterialExpressionVectorParameter* Parameter = AddExpression<UMaterialExpressionVectorParameter>(Material, -700, Y);
	Parameter->ParameterName = Name;
	Parameter->DefaultValue = Value;
	return Parameter;
}

void AddCustomInput(UMaterialExpressionCustom& Custom, const TCHAR* Name, UMaterialExpression* From, int32 OutputIndex = 0)
{
	FCustomInput& Input = Custom.Inputs.AddDefaulted_GetRef();
	Input.InputName = Name;
	Input.Input.Connect(OutputIndex, From);
}

/**
 * The plain material (Brennic): base colour from the texture, roughness 0.8, metallic 0, as the Blender M_Brennic.
 * The ember material (the hound): the same texture through the crack key (CreatureImport::EmberHlsl) gives the shaded
 * body colour and a mask; the mask x EmberColour x EmberIntensity is the emissive.
 */
UMaterial* BuildMaterial(const FString& Name, UTexture2D& Texture, bool bEmber)
{
	const FString PackageName = CreatureImport::FolderPath(Name) / (TEXT("M_") + Name);
	UPackage* Package = CreatePackage(*PackageName);
	UMaterial* Material = NewObject<UMaterial>(Package, FName(*(TEXT("M_") + Name)), RF_Public | RF_Standalone | RF_Transactional);
	Material->SetUsageByFlag(MATUSAGE_SkeletalMesh, true);
	UMaterialEditorOnlyData* Editor = Material->GetEditorOnlyData();

	UMaterialExpressionTextureSampleParameter2D* Sample = AddExpression<UMaterialExpressionTextureSampleParameter2D>(*Material, -1000, 0);
	Sample->ParameterName = TEXT("BaseColor");
	Sample->Texture = &Texture;
	UMaterialExpressionConstant* Roughness = AddExpression<UMaterialExpressionConstant>(*Material, -300, 200);
	Roughness->R = 0.8f;
	UMaterialExpressionConstant* Metallic = AddExpression<UMaterialExpressionConstant>(*Material, -300, 300);
	Metallic->R = 0.0f;
	Editor->Roughness.Connect(0, Roughness);
	Editor->Metallic.Connect(0, Metallic);
	if (!bEmber)
	{
		Editor->BaseColor.Connect(0, Sample);
	}
	else
	{
		const CreatureImport::FEmberParams& Ember = CreatureImport::DefaultEmber();
		UMaterialExpressionCustom* Key = AddExpression<UMaterialExpressionCustom>(*Material, -400, 0);
		Key->Description = TEXT("EmberKey");
		Key->OutputType = CMOT_Float4;
		Key->Code = CreatureImport::EmberHlsl();
		Key->Inputs.Reset();
		AddCustomInput(*Key, TEXT("C"), Sample, 0);
		AddCustomInput(*Key, TEXT("HueCentreDeg"), AddScalar(*Material, TEXT("HueCentreDeg"), Ember.HueCentreDeg, 100));
		AddCustomInput(*Key, TEXT("HueWidthDeg"), AddScalar(*Material, TEXT("HueWidthDeg"), Ember.HueWidthDeg, 160));
		AddCustomInput(*Key, TEXT("SatLow"), AddScalar(*Material, TEXT("SatLow"), Ember.SatLow, 220));
		AddCustomInput(*Key, TEXT("SatHigh"), AddScalar(*Material, TEXT("SatHigh"), Ember.SatHigh, 280));
		AddCustomInput(*Key, TEXT("ValLow"), AddScalar(*Material, TEXT("ValLow"), Ember.ValLow, 340));
		AddCustomInput(*Key, TEXT("ValHigh"), AddScalar(*Material, TEXT("ValHigh"), Ember.ValHigh, 400));
		AddCustomInput(*Key, TEXT("BodyTint"), AddVector(*Material, TEXT("BodyTint"), Ember.BodyTint, 460), 0);
		AddCustomInput(*Key, TEXT("BodyTintAmount"), AddScalar(*Material, TEXT("BodyTintAmount"), Ember.BodyTintAmount, 560));
		AddCustomInput(*Key, TEXT("BodyGain"), AddScalar(*Material, TEXT("BodyGain"), Ember.BodyGain, 620));

		UMaterialExpressionComponentMask* Body = AddExpression<UMaterialExpressionComponentMask>(*Material, -150, 0);
		Body->Input.Connect(0, Key);
		Body->R = Body->G = Body->B = 1;
		Body->A = 0;
		UMaterialExpressionComponentMask* Mask = AddExpression<UMaterialExpressionComponentMask>(*Material, -150, 100);
		Mask->Input.Connect(0, Key);
		Mask->R = Mask->G = Mask->B = 0;
		Mask->A = 1;
		UMaterialExpressionMultiply* Tinted = AddExpression<UMaterialExpressionMultiply>(*Material, 0, 100);
		Tinted->A.Connect(0, Mask);
		Tinted->B.Connect(0, AddVector(*Material, TEXT("EmberColour"), Ember.EmberColour, 700));
		UMaterialExpressionMultiply* Emissive = AddExpression<UMaterialExpressionMultiply>(*Material, 150, 100);
		Emissive->A.Connect(0, Tinted);
		Emissive->B.Connect(0, AddScalar(*Material, TEXT("EmberIntensity"), Ember.EmberIntensity, 800));
		Editor->BaseColor.Connect(0, Body);
		Editor->EmissiveColor.Connect(0, Emissive);
	}
	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();
	return Material;
}

bool SavePackagesUnder(const FString& Folder, int32& OutSaved)
{
	TArray<UPackage*> Packages;
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		if (It->GetName().StartsWith(Folder + TEXT("/")) && It->IsDirty())
		{
			Packages.Add(*It);
		}
	}
	bool bOk = true;
	for (UPackage* Package : Packages)
	{
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (UPackage::SavePackage(Package, Package->FindAssetInPackage(), *File, SaveArgs))
		{
			++OutSaved;
			UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CREATURE_IMPORT saved %s"), *File);
		}
		else
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CREATURE_IMPORT fail save %s"), *File);
			bOk = false;
		}
	}
	return bOk;
}

UObject* ImportOne(IAssetTools& AssetTools, const FString& File, const FString& Folder, const FString& DestinationName, UFactory* Factory, UObject* Options)
{
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = File;
	Task->DestinationPath = Folder;
	Task->DestinationName = DestinationName;
	Task->bReplaceExisting = true;
	Task->bReplaceExistingSettings = true;
	Task->bAutomated = true;
	Task->bSave = false;
	Task->Factory = Factory;
	Task->Options = Options;
	AssetTools.ImportAssetTasks({Task});
	const TArray<UObject*> Objects = Task->GetObjects();
	for (UObject* Object : Objects)
	{
		UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CREATURE_IMPORT object %s (%s)"), *Object->GetPathName(), *Object->GetClass()->GetName());
	}
	return Objects.Num() > 0 ? Objects[0] : nullptr;
}
}
#endif

int32 UImportCreatureMeshCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString Name;
	FString Fbx;
	FString TexturePath;
	FParse::Value(*Params, TEXT("name="), Name);
	FParse::Value(*Params, TEXT("fbx="), Fbx);
	FParse::Value(*Params, TEXT("texture="), TexturePath);
	const CreatureImport::FCreatureSpec* Spec = CreatureImport::FindSpec(Name);
	if (!Spec || !FPaths::FileExists(Fbx) || !FPaths::FileExists(TexturePath))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CREATURE_IMPORT fail usage: -name=<Brennic|CinderHound> -fbx=<existing file> -texture=<existing file> (got name=%s fbx=%s texture=%s)"),
		    *Name, *Fbx, *TexturePath);
		return 1;
	}
	const FString Folder = CreatureImport::FolderPath(Name);
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CREATURE_IMPORT start %s fbx=%s md5=%s texture=%s md5=%s -> %s"), *Name, *Fbx, *LexToString(FMD5Hash::HashFile(*Fbx)),
	    *TexturePath, *LexToString(FMD5Hash::HashFile(*TexturePath)), *Folder);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UTexture2D* Texture = Cast<UTexture2D>(ImportOne(AssetTools, TexturePath, Folder, FString::Printf(TEXT("T_%s_BaseColor"), *Name), nullptr, nullptr));
	if (!Texture)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CREATURE_IMPORT fail texture %s"), *TexturePath);
		return 1;
	}
	Texture->SRGB = true;
	Texture->MarkPackageDirty();

	// The legacy FBX factory, named so AssetTools skips Interchange. Mesh, skeleton and the baked clips; no materials or
	// textures from the FBX (rig_anim.py strips the paths), no physics asset. Scale 100: rig_anim.py's export
	// (apply_scale_options FBX_SCALE_ALL) writes metres into a file the importer reads as centimetres, so at scale 1
	// Brennic stood 1.8 cm tall (the first run, refused by the height check below).
	constexpr float ImportScale = 100.0f;
	UFbxFactory* Factory = NewObject<UFbxFactory>();
	Factory->AddToRoot();
	UFbxImportUI* Options = NewObject<UFbxImportUI>();
	Options->bAutomatedImportShouldDetectType = false;
	Options->MeshTypeToImport = FBXIT_SkeletalMesh;
	Options->OriginalImportType = FBXIT_SkeletalMesh;
	Options->bImportAsSkeletal = true;
	Options->bImportMesh = true;
	Options->bImportAnimations = true;
	Options->bCreatePhysicsAsset = false;
	Options->bImportMaterials = false;
	Options->bImportTextures = false;
	Options->SkeletalMeshImportData->ImportUniformScale = ImportScale;
	Options->SkeletalMeshImportData->bConvertScene = true;
	Options->SkeletalMeshImportData->bForceFrontXAxis = false;
	Options->SkeletalMeshImportData->bConvertSceneUnit = false;
	Options->AnimSequenceImportData->ImportUniformScale = ImportScale;
	Options->AnimSequenceImportData->bConvertScene = true;
	Options->AnimSequenceImportData->bForceFrontXAxis = false;
	Options->AnimSequenceImportData->bConvertSceneUnit = false;
	UObject* Imported = ImportOne(AssetTools, Fbx, Folder, TEXT("SK_") + Name, Factory, Options);
	Factory->RemoveFromRoot();
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(Imported);
	if (!Mesh)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CREATURE_IMPORT fail fbx %s gave %s"), *Fbx, Imported ? *Imported->GetClass()->GetName() : TEXT("nothing"));
		return 1;
	}

	UMaterial* Material = BuildMaterial(Name, *Texture, Spec->bEmber);
	TArray<FSkeletalMaterial>& Slots = Mesh->GetMaterials();
	for (FSkeletalMaterial& Slot : Slots)
	{
		Slot.MaterialInterface = Material;
	}
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	const FBoxSphereBounds Bounds = Mesh->GetImportedBounds();
	const double HeightM = Bounds.BoxExtent.Z * 2.0 / 100.0;
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	const FSkeletalMeshRenderData* Render = Mesh->GetResourceForRendering();
	const int32 Triangles = Render && Render->LODRenderData.Num() > 0 ? Render->LODRenderData[0].GetTotalFaces() : 0;
	int32 Clips = 0;
	for (TObjectIterator<UAnimSequence> It; It; ++It)
	{
		if (It->GetPackage()->GetName().StartsWith(Folder + TEXT("/")))
		{
			++Clips;
			UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CREATURE_IMPORT clip %s %.3f s keys=%d"), *It->GetName(), It->GetPlayLength(), It->GetNumberOfSampledKeys());
		}
	}
	for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
	{
		const FString BoneName = Skeleton.GetBoneName(Bone).ToString();
		if (BoneName.Equals(TEXT("head"), ESearchCase::IgnoreCase) || BoneName.StartsWith(TEXT("ball_")) || Bone == 0)
		{
			FTransform Pose = FTransform::Identity;
			for (int32 Walk = Bone; Walk != INDEX_NONE; Walk = Skeleton.GetParentIndex(Walk))
			{
				Pose = Pose * Skeleton.GetRefBonePose()[Walk];
			}
			UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CREATURE_IMPORT bone %s component=(%.1f, %.1f, %.1f) cm"), *BoneName, Pose.GetLocation().X, Pose.GetLocation().Y,
			    Pose.GetLocation().Z);
		}
	}
	UE_LOG(LogMageArena, Display,
	    TEXT("MAGEVR_CREATURE_IMPORT mesh %s bones=%d lod0tris=%d lods=%d slots=%d clips=%d bounds origin=(%.1f, %.1f, %.1f) extent=(%.1f, %.1f, %.1f) cm height=%.4f m (expect %.4f)"),
	    *Mesh->GetPathName(), Skeleton.GetNum(), Triangles, Render ? Render->LODRenderData.Num() : 0, Slots.Num(), Clips, Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z,
	    Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z, HeightM, Spec->HeightM);
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CREATURE_IMPORT texture %s %dx%d srgb=%d compression=%d material %s ember=%d"), *Texture->GetPathName(), Texture->Source.GetSizeX(),
	    Texture->Source.GetSizeY(), Texture->SRGB ? 1 : 0, static_cast<int32>(Texture->CompressionSettings), *Material->GetPathName(), Spec->bEmber ? 1 : 0);
	if (FMath::Abs(HeightM - Spec->HeightM) > 0.05 * Spec->HeightM)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CREATURE_IMPORT fail %s height %.4f m is more than 5 %% off %.4f m (scale)"), *Name, HeightM, Spec->HeightM);
		return 1;
	}

	// Compile the texture and the material's shaders here, so the derived data is in the DDC before the -game capture
	// loads them (the first capture run spent 120 s at about 1 fps compiling and never took a shot).
	Material->ForceRecompileForRendering();
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CREATURE_IMPORT compiled %s texture %dx%d %s"), *Name, Texture->GetSizeX(), Texture->GetSizeY(),
	    GetPixelFormatString(Texture->GetPixelFormat()));

	int32 Saved = 0;
	if (!SavePackagesUnder(Folder, Saved))
	{
		return 1;
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CREATURE_IMPORT done %s saved=%d"), *Name, Saved);
	return 0;
#else
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CREATURE_IMPORT needs an editor build"));
	return 1;
#endif
}
