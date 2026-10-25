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
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSubtract.h"
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
 *
 * T65, the tint (custom floats 4..6, linear RGB) and the flat flag (float 7):
 *   shade   = saturate(luminance(atlas) / ClayShadeRef), so a tunic never exceeds its tint
 *   weight  = saturate((fraction down the figure - TunicStartFraction) / TunicEdgeFraction), 0 on the head and raised arms
 *   colour  = lerp(lerp(atlas, tint * shade, weight), tint, flat)
 *   opacity = lerp(atlas alpha, 1, flat)
 * A flat instance is a banner: the tint at full opacity, the atlas never shows. The figure fraction comes from the sampled
 * v between the rect's two v floats, whichever of them is the top.
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
		// The tint defaults to white and the flat flag to off.
		Data[Index]->ConstDefaultValue = (Index < 2 || Index == ArenaCrowd::FlatFloatIndex) ? 0.f : 1.f;
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
	auto Scalar = [&Add, Material](float Value, int32 X, int32 Y)
	{
		UMaterialExpressionConstant* Constant = NewObject<UMaterialExpressionConstant>(Material);
		Constant->R = Value;
		Add(Constant, X, Y);
		return Constant;
	};
	auto Binary = [&Add](auto* Expression, UMaterialExpression* A, UMaterialExpression* B, int32 X, int32 Y)
	{
		Expression->A.Expression = A;
		Expression->B.Expression = B;
		Add(Expression, X, Y);
		return Expression;
	};
	// The tint: floats 4..6 as one vector.
	UMaterialExpressionAppendVector* TintRg = NewObject<UMaterialExpressionAppendVector>(Material);
	TintRg->A.Expression = Data[ArenaCrowd::TintFloatIndex];
	TintRg->B.Expression = Data[ArenaCrowd::TintFloatIndex + 1];
	Add(TintRg, -800, 350);
	UMaterialExpressionAppendVector* Tint = NewObject<UMaterialExpressionAppendVector>(Material);
	Tint->A.Expression = TintRg;
	Tint->B.Expression = Data[ArenaCrowd::TintFloatIndex + 2];
	Add(Tint, -600, 350);
	// The shade: the atlas luminance over the clay reference, saturated.
	UMaterialExpressionConstant3Vector* LumaWeights = NewObject<UMaterialExpressionConstant3Vector>(Material);
	LumaWeights->Constant = FLinearColor(0.2126f, 0.7152f, 0.0722f);
	Add(LumaWeights, -400, 450);
	UMaterialExpressionDotProduct* Luma = NewObject<UMaterialExpressionDotProduct>(Material);
	Luma->A.Connect(0, Sample);
	Luma->B.Expression = LumaWeights;
	Add(Luma, -200, 450);
	UMaterialExpressionMultiply* ShadeScaled = Binary(NewObject<UMaterialExpressionMultiply>(Material), Luma, Scalar(1.f / ArenaCrowd::ClayShadeRef, -200, 550), 0, 450);
	UMaterialExpressionSaturate* Shade = NewObject<UMaterialExpressionSaturate>(Material);
	Shade->Input.Expression = ShadeScaled;
	Add(Shade, 200, 450);
	UMaterialExpressionMultiply* Tunic = Binary(NewObject<UMaterialExpressionMultiply>(Material), Tint, Shade, 400, 400);
	// The weight: how far down the figure the sampled v is, between the rect's top and bottom v (either order).
	UMaterialExpressionComponentMask* SampledV = NewObject<UMaterialExpressionComponentMask>(Material);
	SampledV->Input.Expression = Rect;
	SampledV->R = false;
	SampledV->G = true;
	SampledV->B = false;
	SampledV->A = false;
	Add(SampledV, -400, 650);
	UMaterialExpressionMin* RectTop = Binary(NewObject<UMaterialExpressionMin>(Material), Data[1], Data[3], -400, 750);
	UMaterialExpressionMax* RectBottom = Binary(NewObject<UMaterialExpressionMax>(Material), Data[1], Data[3], -400, 850);
	UMaterialExpressionSubtract* Down = Binary(NewObject<UMaterialExpressionSubtract>(Material), SampledV, RectTop, -200, 650);
	UMaterialExpressionSubtract* Span = Binary(NewObject<UMaterialExpressionSubtract>(Material), RectBottom, RectTop, -200, 850);
	UMaterialExpressionDivide* Fraction = Binary(NewObject<UMaterialExpressionDivide>(Material), Down, Span, 0, 650);
	UMaterialExpressionSubtract* PastStart = Binary(NewObject<UMaterialExpressionSubtract>(Material), Fraction, Scalar(ArenaCrowd::TunicStartFraction, 0, 750), 200, 650);
	UMaterialExpressionMultiply* Ramp = Binary(NewObject<UMaterialExpressionMultiply>(Material), PastStart, Scalar(1.f / ArenaCrowd::TunicEdgeFraction, 200, 750), 400, 650);
	UMaterialExpressionSaturate* Weight = NewObject<UMaterialExpressionSaturate>(Material);
	Weight->Input.Expression = Ramp;
	Add(Weight, 600, 650);
	// Clay or tunic by the weight, then the flat tint by the flag.
	UMaterialExpressionLinearInterpolate* ClayOrTunic = NewObject<UMaterialExpressionLinearInterpolate>(Material);
	ClayOrTunic->A.Connect(0, Sample);
	ClayOrTunic->B.Expression = Tunic;
	ClayOrTunic->Alpha.Expression = Weight;
	Add(ClayOrTunic, 800, 400);
	UMaterialExpressionLinearInterpolate* Colour = NewObject<UMaterialExpressionLinearInterpolate>(Material);
	Colour->A.Expression = ClayOrTunic;
	Colour->B.Expression = Tint;
	Colour->Alpha.Expression = Data[ArenaCrowd::FlatFloatIndex];
	Add(Colour, 1000, 400);
	UMaterialExpressionLinearInterpolate* Opacity = NewObject<UMaterialExpressionLinearInterpolate>(Material);
	Opacity->A.Connect(4, Sample);
	Opacity->B.Expression = Scalar(1.f, 800, 550);
	Opacity->Alpha.Expression = Data[ArenaCrowd::FlatFloatIndex];
	Add(Opacity, 1000, 550);
	UMaterialExpressionConstant* Roughness = NewObject<UMaterialExpressionConstant>(Material);
	Roughness->R = 1.f;
	Add(Roughness, -200, 200);
	UMaterialExpressionConstant* Specular = NewObject<UMaterialExpressionConstant>(Material);
	Specular->R = 0.f;
	Add(Specular, -200, 300);
	// TextureSample outputs: 0 RGB, 1 R, 2 G, 3 B, 4 A.
	Material->GetEditorOnlyData()->BaseColor.Expression = Colour;
	Material->GetEditorOnlyData()->OpacityMask.Expression = Opacity;
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
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CROWD_ASSETS %s blend=masked clip=0.5 shading=defaultlit twosided=0 instanced=1 customfloats=%d"), ArenaCrowd::MaterialName, ArenaCrowd::CustomDataFloats);
	return Material;
}

}
#endif

int32 UCreateCrowdAssetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bForce = FParse::Param(*Params, TEXT("force"));
	// -material: rebuild M_Crowd_Atlas against the atlas on disk, which stays as it is.
	const bool bForceMaterial = bForce || FParse::Param(*Params, TEXT("material"));
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
	if (bForceMaterial && !bForce)
	{
		IFileManager::Get().Delete(*FPackageName::LongPackageNameToFilename(PackagePath(ArenaCrowd::MaterialName), FPackageName::GetAssetPackageExtension()), false, true, true);
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
	UMaterialInterface* Material = bForceMaterial ? nullptr : LoadExisting<UMaterialInterface>(ArenaCrowd::MaterialName);
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
