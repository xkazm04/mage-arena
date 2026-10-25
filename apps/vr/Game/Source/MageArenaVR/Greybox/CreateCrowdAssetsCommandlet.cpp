#include "Greybox/CreateCrowdAssetsCommandlet.h"

#include "Greybox/ArenaStands.h"
#include "MageArenaVR.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

UCreateCrowdAssetsCommandlet::UCreateCrowdAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace
{
FString PackagePath(const TCHAR* Name)
{
	return FString::Printf(TEXT("%s/%s"), ArenaCrowd::Folder, Name);
}

template <typename T>
T* LoadExisting(const TCHAR* Name)
{
	return LoadObject<T>(nullptr, *ArenaCrowd::ObjectPath(Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

bool Save(UPackage* Package, UObject* Asset, const TCHAR* Name)
{
	const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Asset, *File, SaveArgs))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CROWD_ASSETS fail %s save %s"), Name, *File);
		return false;
	}
	return true;
}

/** The atlas through the editor's own texture import: sRGB colour, mips from the World group, the default compression. */
UTexture2D* ImportAtlas(const FString& Png)
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = Png;
	Task->DestinationPath = ArenaCrowd::Folder;
	Task->DestinationName = ArenaCrowd::AtlasName;
	Task->bReplaceExisting = true;
	Task->bReplaceExistingSettings = true;
	Task->bAutomated = true;
	Task->bSave = false;
	AssetTools.ImportAssetTasks({Task});
	UTexture2D* Texture = nullptr;
	for (UObject* Object : Task->GetObjects())
	{
		Texture = Texture ? Texture : Cast<UTexture2D>(Object);
	}
	if (!Texture)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CROWD_ASSETS fail %s import %s"), ArenaCrowd::AtlasName, *Png);
		return nullptr;
	}
	Texture->SRGB = true;
	Texture->MipGenSettings = TMGS_FromTextureGroup;
	Texture->LODGroup = TEXTUREGROUP_World;
	Texture->CompressionSettings = TC_Default;
	Texture->PostEditChange();
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (!Save(Texture->GetOutermost(), Texture, ArenaCrowd::AtlasName))
	{
		return nullptr;
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CROWD_ASSETS %s %dx%d srgb=%d mipgen=%d group=%d md5=%s"), ArenaCrowd::AtlasName, Texture->Source.GetSizeX(),
	    Texture->Source.GetSizeY(), Texture->SRGB ? 1 : 0, static_cast<int32>(Texture->MipGenSettings), static_cast<int32>(Texture->LODGroup), *LexToString(FMD5Hash::HashFile(*Png)));
	return Texture;
}

/**
 * Lit like the greybox (default lit, matte), masked at 0.5, one-sided, usable on instanced meshes. The UV is the quad's
 * 0..1 mapped into the instance's atlas rect: lerp((u0, top v), (u1, bottom v), uv).
 */
UMaterial* BuildMaterial(UTexture2D* Atlas)
{
	UPackage* Package = CreatePackage(*PackagePath(ArenaCrowd::MaterialName));
	UMaterial* Material = NewObject<UMaterial>(Package, ArenaCrowd::MaterialName, RF_Public | RF_Standalone | RF_Transactional);
	Material->MaterialDomain = MD_Surface;
	Material->BlendMode = BLEND_Masked;
	Material->OpacityMaskClipValue = 0.5f;
	Material->SetShadingModel(MSM_DefaultLit);
	Material->TwoSided = false;
	Material->SetUsageByFlag(MATUSAGE_InstancedStaticMeshes, true);
	auto Add = [Material](UMaterialExpression* Expression, int32 X, int32 Y)
	{
		Expression->MaterialExpressionEditorX = X;
		Expression->MaterialExpressionEditorY = Y;
		Material->GetExpressionCollection().AddExpression(Expression);
	};
	UMaterialExpressionPerInstanceCustomData* Data[ArenaCrowd::CustomDataFloats];
	for (int32 Index = 0; Index < ArenaCrowd::CustomDataFloats; ++Index)
	{
		Data[Index] = NewObject<UMaterialExpressionPerInstanceCustomData>(Material);
		Data[Index]->DataIndex = Index;
		// Without custom data (a plain mesh in the editor) the quad shows the whole atlas.
		Data[Index]->ConstDefaultValue = Index < 2 ? 0.f : 1.f;
		Add(Data[Index], -1000, -200 + Index * 100);
	}
	UMaterialExpressionAppendVector* TopLeft = NewObject<UMaterialExpressionAppendVector>(Material);
	TopLeft->A.Expression = Data[0];
	TopLeft->B.Expression = Data[1];
	Add(TopLeft, -800, -150);
	UMaterialExpressionAppendVector* BottomRight = NewObject<UMaterialExpressionAppendVector>(Material);
	BottomRight->A.Expression = Data[2];
	BottomRight->B.Expression = Data[3];
	Add(BottomRight, -800, 50);
	UMaterialExpressionTextureCoordinate* Uv = NewObject<UMaterialExpressionTextureCoordinate>(Material);
	Add(Uv, -800, 200);
	UMaterialExpressionLinearInterpolate* Rect = NewObject<UMaterialExpressionLinearInterpolate>(Material);
	Rect->A.Expression = TopLeft;
	Rect->B.Expression = BottomRight;
	Rect->Alpha.Expression = Uv;
	Add(Rect, -600, 0);
	UMaterialExpressionTextureSample* Sample = NewObject<UMaterialExpressionTextureSample>(Material);
	Sample->Texture = Atlas;
	Sample->SamplerType = SAMPLERTYPE_Color;
	Sample->Coordinates.Expression = Rect;
	Add(Sample, -400, 0);
	UMaterialExpressionConstant* Roughness = NewObject<UMaterialExpressionConstant>(Material);
	Roughness->R = 1.f;
	Add(Roughness, -200, 200);
	UMaterialExpressionConstant* Specular = NewObject<UMaterialExpressionConstant>(Material);
	Specular->R = 0.f;
	Add(Specular, -200, 300);
	// TextureSample outputs: 0 RGB, 1 R, 2 G, 3 B, 4 A.
	Material->GetEditorOnlyData()->BaseColor.Connect(0, Sample);
	Material->GetEditorOnlyData()->OpacityMask.Connect(4, Sample);
	Material->GetEditorOnlyData()->Roughness.Expression = Roughness;
	Material->GetEditorOnlyData()->Specular.Expression = Specular;
	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	if (!Save(Package, Material, ArenaCrowd::MaterialName))
	{
		return nullptr;
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CROWD_ASSETS %s blend=masked clip=0.5 shading=defaultlit twosided=0 instanced=1"), ArenaCrowd::MaterialName);
	return Material;
}

}
#endif

int32 UCreateCrowdAssetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bForce = FParse::Param(*Params, TEXT("force"));
	const FString Png = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../art/environment-kit/crowd/crowd_atlas.png")));
	if (!FPaths::FileExists(Png))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CROWD_ASSETS fail atlas missing %s"), *Png);
		return 1;
	}
	if (bForce)
	{
		// A rebuild starts from nothing on disk: the reimported atlas would otherwise load the old material, and the new
		// one could not be saved over it.
		for (const TCHAR* Name : {ArenaCrowd::AtlasName, ArenaCrowd::MaterialName})
		{
			IFileManager::Get().Delete(*FPackageName::LongPackageNameToFilename(PackagePath(Name), FPackageName::GetAssetPackageExtension()), false, true, true);
		}
	}
	UTexture2D* Atlas = bForce ? nullptr : LoadExisting<UTexture2D>(ArenaCrowd::AtlasName);
	if (Atlas)
	{
		UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CROWD_ASSETS %s unchanged"), ArenaCrowd::AtlasName);
	}
	else if (!(Atlas = ImportAtlas(Png)))
	{
		return 1;
	}
	UMaterialInterface* Material = bForce ? nullptr : LoadExisting<UMaterialInterface>(ArenaCrowd::MaterialName);
	if (Material)
	{
		UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CROWD_ASSETS %s unchanged"), ArenaCrowd::MaterialName);
	}
	else if (!(Material = BuildMaterial(Atlas)))
	{
		return 1;
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CROWD_ASSETS done"));
	return 0;
#else
	return 1;
#endif
}
