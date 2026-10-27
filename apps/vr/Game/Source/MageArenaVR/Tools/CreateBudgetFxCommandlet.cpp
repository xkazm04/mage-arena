#include "Tools/CreateBudgetFxCommandlet.h"

#include "Budget/BudgetFx.h"
#include "MageArenaVR.h"
#include "Session/SpellVfx.h"

#if WITH_EDITOR
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "EdGraph/EdGraphPin.h"
#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "ShaderCompiler.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

UCreateBudgetFxCommandlet::UCreateBudgetFxCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace
{
FString ParameterValueText(const FNiagaraParameterStore& Store, const FNiagaraVariableBase& Variable)
{
	const FNiagaraTypeDefinition Type = Variable.GetType();
	const int32 Offset = Store.IndexOf(Variable);
	if (Offset == INDEX_NONE || Type.IsDataInterface() || Type.IsUObject())
	{
		return TEXT("-");
	}
	const int32 Size = Type.GetSize();
	const uint8* Data = Store.GetParameterData(Offset, Size);
	if (Type == FNiagaraTypeDefinition::GetBoolDef())
	{
		return *reinterpret_cast<const int32*>(Data) ? TEXT("true") : TEXT("false");
	}
	if (Type == FNiagaraTypeDefinition::GetIntDef())
	{
		return FString::FromInt(*reinterpret_cast<const int32*>(Data));
	}
	FString Text;
	for (int32 Word = 0; Word + 4 <= Size; Word += 4)
	{
		Text += FString::Printf(TEXT("%s%g"), Word ? TEXT(",") : TEXT(""), *reinterpret_cast<const float*>(Data + Word));
	}
	return Text;
}

void DumpEmitter(const FString& Name, UNiagaraEmitter& Emitter, const FGuid& Version)
{
	FVersionedNiagaraEmitterData* Data = Emitter.GetEmitterData(Version);
	if (!Data)
	{
		return;
	}
	TArray<UNiagaraScript*> Scripts;
	Data->GetScripts(Scripts, false);
	for (UNiagaraScript* Script : Scripts)
	{
		TArray<FNiagaraVariable> Variables;
		Script->RapidIterationParameters.GetParameters(Variables);
		UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_DUMP %s script=%s params=%d"), *Name,
			*StaticEnum<ENiagaraScriptUsage>()->GetNameStringByValue(static_cast<int64>(Script->GetUsage())), Variables.Num());
		for (const FNiagaraVariable& Variable : Variables)
		{
			UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_DUMP %s param %s : %s = %s"), *Name, *Variable.GetName().ToString(),
				*Variable.GetType().GetName(), *ParameterValueText(Script->RapidIterationParameters, Variable));
		}
	}
	if (const UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource))
	{
		TArray<UNiagaraNodeFunctionCall*> Calls;
		Source->NodeGraph->GetNodesOfClass<UNiagaraNodeFunctionCall>(Calls);
		for (const UNiagaraNodeFunctionCall* Call : Calls)
		{
			if (Call->GetFunctionName().StartsWith(TEXT("EmitterState")))
			{
				for (const UEdGraphPin* Pin : Call->Pins)
				{
					if (Pin->Direction == EGPD_Input && Pin->PinName.ToString().Contains(TEXT("Loop")))
					{
						UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_DUMP %s pin %s = '%s' (%s)"), *Name, *Pin->PinName.ToString(), *Pin->DefaultValue, *Pin->PinType.PinCategory.ToString());
					}
				}
			}
		}
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_DUMP %s emitter sim=%d alloc=%d prealloc=%d bounds=%d local=%d"), *Name,
		static_cast<int32>(Data->SimTarget), static_cast<int32>(Data->AllocationMode), Data->PreAllocationCount,
		static_cast<int32>(Data->CalculateBoundsMode), Data->bLocalSpace ? 1 : 0);
}

const TCHAR* SimText(const FVersionedNiagaraEmitterData& Data)
{
	return Data.SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("gpu") : TEXT("cpu");
}

/** Sets every rapid-iteration parameter whose name ends in ".<Suffix>" and whose type is Type. Returns how many it set. */
template <typename T>
int32 SetByName(TConstArrayView<UNiagaraScript*> Scripts, const FNiagaraTypeDefinition& Type, const TCHAR* Suffix, const T& Value)
{
	const FString Tail = FString(TEXT(".")) + Suffix;
	int32 Set = 0;
	for (UNiagaraScript* Script : Scripts)
	{
		TArray<FNiagaraVariable> Variables;
		Script->RapidIterationParameters.GetParameters(Variables);
		for (const FNiagaraVariable& Variable : Variables)
		{
			if (Variable.GetType() == Type && Variable.GetName().ToString().EndsWith(Tail, ESearchCase::CaseSensitive))
			{
				Script->RapidIterationParameters.SetParameterValue(Value, Variable);
				++Set;
			}
		}
	}
	return Set;
}

/** The emitter's own properties: sim target, allocation, bounds. The caller compiles before ApplyInputs. */
void ApplyEmitterData(const FBudgetFxRow& Row, FVersionedNiagaraEmitterData& Data)
{
	Data.SimTarget = Row.bGpu ? ENiagaraSimTarget::GPUComputeSim : ENiagaraSimTarget::CPUSim;
	Data.AllocationMode = EParticleAllocationMode::FixedCount;
	Data.PreAllocationCount = Row.PeakLive;
	if (Row.bGpu)
	{
		// A GPU emitter has no per-frame bounds: a fixed 4 m box (NIAGARA-PROXY 1.2).
		Data.CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
		Data.FixedBounds = FBox(FVector(-200.0), FVector(200.0));
	}
}

/**
 * A burst template loops once and then completes, and its component with it. The stress scene needs a burst every
 * period for the whole window, so the emitter-state module's "Loop Behavior" static switch is set to the value the
 * continuous templates carry (NewEnumerator0, Infinite). Returns whether the switch was found.
 */
bool SetLoopForever(FVersionedNiagaraEmitterData& Data)
{
	const UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data.GraphSource);
	if (!Source)
	{
		return false;
	}
	TArray<UNiagaraNodeFunctionCall*> Calls;
	Source->NodeGraph->GetNodesOfClass<UNiagaraNodeFunctionCall>(Calls);
	bool bSet = false;
	for (UNiagaraNodeFunctionCall* Call : Calls)
	{
		if (!Call->GetFunctionName().StartsWith(TEXT("EmitterState")))
		{
			continue;
		}
		for (UEdGraphPin* Pin : Call->Pins)
		{
			if (Pin->Direction == EGPD_Input && Pin->PinName == TEXT("Loop Behavior"))
			{
				Pin->DefaultValue = TEXT("NewEnumerator0");
				Call->PinDefaultValueChanged(Pin);
				bSet = true;
			}
		}
	}
	if (bSet)
	{
		Source->NodeGraph->NotifyGraphChanged();
	}
	return bSet;
}

/** The rapid-iteration inputs (spawn rate, burst, lifetime, size, colour). Returns how many parameters it set. */
int32 ApplyInputs(const FBudgetFxRow& Row, FVersionedNiagaraEmitterData& Data)
{
	TArray<UNiagaraScript*> Scripts;
	Data.GetScripts(Scripts, false);
	const FNiagaraTypeDefinition Float = FNiagaraTypeDefinition::GetFloatDef();
	const FNiagaraTypeDefinition Int = FNiagaraTypeDefinition::GetIntDef();
	const FNiagaraTypeDefinition Colour = FNiagaraTypeDefinition::GetColorDef();
	const FNiagaraTypeDefinition Vector = FNiagaraTypeDefinition::GetVec3Def();
	int32 Set = 0;
	Set += SetByName(Scripts, Colour, TEXT("InitializeParticle.Color"), Row.Colour);
	const float Life = static_cast<float>(Row.LifeS);
	Set += SetByName(Scripts, Float, TEXT("InitializeParticle.Lifetime"), Life);
	Set += SetByName(Scripts, Float, TEXT("InitializeParticle.Lifetime Min"), Life);
	Set += SetByName(Scripts, Float, TEXT("InitializeParticle.Lifetime Max"), Life);
	Set += SetByName(Scripts, Float, TEXT("InitializeParticle.Uniform Sprite Size"), static_cast<float>(Row.SizeMaxCm));
	Set += SetByName(Scripts, Float, TEXT("InitializeParticle.Uniform Sprite Size Min"), static_cast<float>(Row.SizeMinCm));
	Set += SetByName(Scripts, Float, TEXT("InitializeParticle.Uniform Sprite Size Max"), static_cast<float>(Row.SizeMaxCm));
	if (Row.SpawnPerS > 0.0)
	{
		Set += SetByName(Scripts, Float, TEXT("SpawnRate.SpawnRate"), static_cast<float>(Row.SpawnPerS));
	}
	if (Row.BurstCount > 0)
	{
		Set += SetByName(Scripts, Int, TEXT("SpawnBurst_Instantaneous.Spawn Count"), Row.BurstCount);
		// The emitter loops, one burst per loop: the loop duration is the burst period.
		Set += SetByName(Scripts, Float, TEXT("EmitterState.Loop Duration"), static_cast<float>(Row.BurstPeriodS));
	}
	if (!Row.bGpu)
	{
		// Short-lived effect particles stay near their spawn point: no gravity.
		Set += SetByName(Scripts, Vector, TEXT("GravityForce.Gravity"), FVector3f::ZeroVector);
	}
	if (Row.TemplatePath.EndsWith(TEXT("/Fountain")))
	{
		// Fountain's launch speed is a random range (cm/s); a trail or a telegraph ring barely moves.
		const float Speed = Row.Name.Contains(TEXT("Telegraph")) ? 0.0f : 40.0f;
		Set += SetByName(Scripts, Float, TEXT("RandomRangeFloat.Minimum"), 0.0f);
		Set += SetByName(Scripts, Float, TEXT("RandomRangeFloat.Maximum"), Speed);
	}
	return Set;
}

/**
 * T63: T_SpellFlipbook, the spell flipbook atlas, imported from apps/vr/art/vfx/spell-flipbook.png (written by
 * apps/vr/tools/vfx/spell-flipbook.mjs). A mask, not a picture (R = G = B = shade, A = opacity), so linear, not sRGB;
 * mips from the Effects group; 512x512 at most. Null when the import or the save fails.
 */
UTexture2D* ImportSpellFlipbook()
{
	const FString Png = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../art/vfx/spell-flipbook.png")));
	if (!FPaths::FileExists(Png))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FX fail flipbook missing %s"), *Png);
		return nullptr;
	}
	const FString PackagePath = SpellVfx::FlipbookTexturePackage;
	const FString Name = FPackageName::GetShortName(PackagePath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = Png;
	Task->DestinationPath = FPackageName::GetLongPackagePath(PackagePath);
	Task->DestinationName = Name;
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
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FX fail %s import %s"), *Name, *Png);
		return nullptr;
	}
	Texture->SRGB = false;
	Texture->MipGenSettings = TMGS_FromTextureGroup;
	Texture->LODGroup = TEXTUREGROUP_Effects;
	Texture->CompressionSettings = TC_Default;
	Texture->MaxTextureSize = 512;
	Texture->PostEditChange();
	FAssetCompilingManager::Get().FinishAllCompilation();
	const FString PackageFile = FPackageName::LongPackageNameToFilename(Texture->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Texture->GetOutermost(), Texture, *PackageFile, SaveArgs))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FX fail %s save %s"), *Name, *PackageFile);
		return nullptr;
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_FX %s %dx%d srgb=%d mipgen=%d group=%d md5=%s -> %s"), *Name, Texture->Source.GetSizeX(), Texture->Source.GetSizeY(),
	    Texture->SRGB ? 1 : 0, static_cast<int32>(Texture->MipGenSettings), static_cast<int32>(Texture->LODGroup), *LexToString(FMD5Hash::HashFile(*Png)), *PackageFile);
	return Texture;
}

/**
 * T58: M_SpellGlow, the one material of every spell VFX puff (Session/SpellVfx.h). Unlit, translucent (alpha blend),
 * two-sided, usable on instanced meshes. A blend moves a pixel from the background toward the glow colour and never past
 * it, so a glow cannot clip into another hue over the bright sand and sky (an additive glow read fire as yellow-gold and
 * air as lime to a blind critic). Greybox::Tint writes Color.
 *
 * T63: the puff is a card showing one cell of T_SpellFlipbook, picked per instance by three custom data floats
 * (SpellVfx::CardCustomData): U = u x d0 + d1, V = v / 4 + d2. Emissive = Color x the cell's shade (R, at most 1) and
 * Opacity = saturate(A x GlowGain): every pixel is the glow colour or a darker tone of the same chroma, never brighter
 * than its own threat and never another hue. The far-hemisphere falloff of T58 lives on as the SoftGlow cell. Every
 * colour stays one material instance and one instanced draw. Returns false when the import or the save fails.
 *
 * T73: one exception to "the same chroma": Fire's tint sets RimMix 1 and RimColor (SpellVfx::FireRimColour, a deeper
 * red-orange of Fire's family, never brighter than Fire), and its darkest tones take that hue (SpellVfx::GlowEmissive).
 */
bool CreateSpellGlowMaterial(bool bForce)
{
	const FString PackagePath = SpellVfx::GlowMaterialPackage;
	const FString Name = FPackageName::GetShortName(PackagePath);
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *Name);
	const FString TextureObject = FString::Printf(TEXT("%s.%s"), SpellVfx::FlipbookTexturePackage, *FPackageName::GetShortName(SpellVfx::FlipbookTexturePackage));
	if (!bForce && LoadObject<UMaterial>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet) && LoadObject<UTexture2D>(nullptr, *TextureObject, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_FX %s unchanged"), *Name);
		return true;
	}
	const FString PackageFile = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	// A rebuild starts from nothing on disk: the reimported atlas would otherwise load the old material, and the new one
	// could not be saved over it.
	IFileManager::Get().Delete(*PackageFile, false, true, true);
	IFileManager::Get().Delete(*FPackageName::LongPackageNameToFilename(SpellVfx::FlipbookTexturePackage, FPackageName::GetAssetPackageExtension()), false, true, true);
	UTexture2D* Flipbook = ImportSpellFlipbook();
	if (!Flipbook)
	{
		return false;
	}
	UPackage* Package = CreatePackage(*PackagePath);
	UMaterial* Material = NewObject<UMaterial>(Package, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
	Material->MaterialDomain = MD_Surface;
	Material->BlendMode = BLEND_Translucent;
	Material->SetShadingModel(MSM_Unlit);
	Material->bUsedWithInstancedStaticMeshes = true;
	Material->TwoSided = true;
	auto Add = [Material](UMaterialExpression* Expression, int32 X, int32 Y)
	{
		Expression->MaterialExpressionEditorX = X;
		Expression->MaterialExpressionEditorY = Y;
		Material->GetExpressionCollection().AddExpression(Expression);
	};
	UMaterialExpressionVectorParameter* Colour = NewObject<UMaterialExpressionVectorParameter>(Material);
	Colour->ParameterName = TEXT("Color");
	Colour->DefaultValue = FLinearColor::White;
	Add(Colour, -400, -300);
	UMaterialExpressionScalarParameter* Gain = NewObject<UMaterialExpressionScalarParameter>(Material);
	Gain->ParameterName = TEXT("GlowGain");
	Gain->DefaultValue = 1.0f;
	Add(Gain, -400, 250);
	// Without instance data (a plain mesh in the editor) the card shows the SoftGlow cell.
	const float Defaults[SpellVfx::CardCustomFloats] = {0.25f, 0.f, 0.75f};
	UMaterialExpressionPerInstanceCustomData* Data[SpellVfx::CardCustomFloats];
	for (int32 Index = 0; Index < SpellVfx::CardCustomFloats; ++Index)
	{
		Data[Index] = NewObject<UMaterialExpressionPerInstanceCustomData>(Material);
		Data[Index]->DataIndex = Index;
		Data[Index]->ConstDefaultValue = Defaults[Index];
		Add(Data[Index], -1200, -100 + Index * 100);
	}
	UMaterialExpressionTextureCoordinate* Uv = NewObject<UMaterialExpressionTextureCoordinate>(Material);
	Add(Uv, -1200, 250);
	UMaterialExpressionComponentMask* UvU = NewObject<UMaterialExpressionComponentMask>(Material);
	UvU->Input.Expression = Uv;
	UvU->R = true;
	Add(UvU, -1050, 200);
	UMaterialExpressionComponentMask* UvV = NewObject<UMaterialExpressionComponentMask>(Material);
	UvV->Input.Expression = Uv;
	UvV->G = true;
	Add(UvV, -1050, 300);
	UMaterialExpressionMultiply* UScaled = NewObject<UMaterialExpressionMultiply>(Material);
	UScaled->A.Expression = UvU;
	UScaled->B.Expression = Data[0];
	Add(UScaled, -900, 100);
	UMaterialExpressionAdd* U = NewObject<UMaterialExpressionAdd>(Material);
	U->A.Expression = UScaled;
	U->B.Expression = Data[1];
	Add(U, -800, 100);
	UMaterialExpressionMultiply* VScaled = NewObject<UMaterialExpressionMultiply>(Material);
	VScaled->A.Expression = UvV;
	VScaled->ConstB = 1.f / SpellVfx::FlipbookGrid;
	Add(VScaled, -900, 250);
	UMaterialExpressionAdd* V = NewObject<UMaterialExpressionAdd>(Material);
	V->A.Expression = VScaled;
	V->B.Expression = Data[2];
	Add(V, -800, 250);
	UMaterialExpressionAppendVector* Cell = NewObject<UMaterialExpressionAppendVector>(Material);
	Cell->A.Expression = U;
	Cell->B.Expression = V;
	Add(Cell, -650, 150);
	UMaterialExpressionTextureSample* Sample = NewObject<UMaterialExpressionTextureSample>(Material);
	Sample->Texture = Flipbook;
	Sample->SamplerType = SAMPLERTYPE_LinearColor;
	Sample->Coordinates.Expression = Cell;
	Add(Sample, -500, 100);
	// T73: the rim term. Weight = RimMix x saturate((RimShadeTop - shade) / (RimShadeTop - RimShadeBottom)), and the hue is
	// lerp(Color, RimColor, Weight): a cell's darkest tones take RimColor. RimMix is 0 unless the presentation sets it (only
	// Fire's tint, SpellVfx::ApplyGlowRim), and lerp(a, b, 0) is a, so every other tint draws Color x shade as before.
	UMaterialExpressionVectorParameter* RimColour = NewObject<UMaterialExpressionVectorParameter>(Material);
	RimColour->ParameterName = TEXT("RimColor");
	RimColour->DefaultValue = FLinearColor::White;
	Add(RimColour, -400, -450);
	UMaterialExpressionScalarParameter* RimMix = NewObject<UMaterialExpressionScalarParameter>(Material);
	RimMix->ParameterName = TEXT("RimMix");
	RimMix->DefaultValue = 0.f;
	Add(RimMix, -400, -600);
	UMaterialExpressionScalarParameter* RimTop = NewObject<UMaterialExpressionScalarParameter>(Material);
	RimTop->ParameterName = TEXT("RimShadeTop");
	RimTop->DefaultValue = SpellVfx::FireRimShadeTop;
	Add(RimTop, -650, -600);
	UMaterialExpressionScalarParameter* RimBottom = NewObject<UMaterialExpressionScalarParameter>(Material);
	RimBottom->ParameterName = TEXT("RimShadeBottom");
	RimBottom->DefaultValue = SpellVfx::FireRimShadeBottom;
	Add(RimBottom, -650, -500);
	// TextureSample outputs: 0 RGB, 1 R, 2 G, 3 B, 4 A.
	UMaterialExpressionSubtract* RimDepth = NewObject<UMaterialExpressionSubtract>(Material);
	RimDepth->A.Expression = RimTop;
	RimDepth->B.Connect(1, Sample);
	Add(RimDepth, -500, -650);
	UMaterialExpressionSubtract* RimBand = NewObject<UMaterialExpressionSubtract>(Material);
	RimBand->A.Expression = RimTop;
	RimBand->B.Expression = RimBottom;
	Add(RimBand, -500, -550);
	UMaterialExpressionDivide* RimRatio = NewObject<UMaterialExpressionDivide>(Material);
	RimRatio->A.Expression = RimDepth;
	RimRatio->B.Expression = RimBand;
	Add(RimRatio, -380, -650);
	UMaterialExpressionSaturate* RimRamp = NewObject<UMaterialExpressionSaturate>(Material);
	RimRamp->Input.Expression = RimRatio;
	Add(RimRamp, -300, -650);
	UMaterialExpressionMultiply* RimWeight = NewObject<UMaterialExpressionMultiply>(Material);
	RimWeight->A.Expression = RimRamp;
	RimWeight->B.Expression = RimMix;
	Add(RimWeight, -220, -600);
	UMaterialExpressionLinearInterpolate* Hue = NewObject<UMaterialExpressionLinearInterpolate>(Material);
	Hue->A.Expression = Colour;
	Hue->B.Expression = RimColour;
	Hue->Alpha.Expression = RimWeight;
	Add(Hue, -200, -350);
	UMaterialExpressionMultiply* Shaded = NewObject<UMaterialExpressionMultiply>(Material);
	Shaded->A.Expression = Hue;
	Shaded->B.Connect(1, Sample);
	Add(Shaded, -250, -200);
	UMaterialExpressionMultiply* Dense = NewObject<UMaterialExpressionMultiply>(Material);
	Dense->A.Connect(4, Sample);
	Dense->B.Expression = Gain;
	Add(Dense, -250, 150);
	UMaterialExpressionSaturate* Opacity = NewObject<UMaterialExpressionSaturate>(Material);
	Opacity->Input.Expression = Dense;
	Add(Opacity, -120, 150);
	Material->GetEditorOnlyData()->EmissiveColor.Expression = Shaded;
	Material->GetEditorOnlyData()->Opacity.Expression = Opacity;
	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Material, *PackageFile, SaveArgs))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FX fail %s save %s"), *Name, *PackageFile);
		return false;
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_FX %s blend=translucent shading=unlit twosided=1 instanced=1 flipbook=%s customdata=%d rim=RimColor,RimMix,RimShadeTop=%.2f,RimShadeBottom=%.2f -> %s"), *Name,
	    *Flipbook->GetName(), SpellVfx::CardCustomFloats, SpellVfx::FireRimShadeTop, SpellVfx::FireRimShadeBottom, *PackageFile);
	return true;
}

/** Builds the (unsaved) system of one row from its emitter template. Null when the template is missing. */
UNiagaraSystem* BuildSystem(const FBudgetFxRow& Row, UPackage* Package)
{
	const FString TemplateObject = FString::Printf(TEXT("%s.%s"), *Row.TemplatePath, *FPackageName::GetShortName(Row.TemplatePath));
	UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr, *TemplateObject);
	if (!Template)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FX fail %s template %s not found"), *Row.Name, *TemplateObject);
		return nullptr;
	}
	UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, FName(*Row.Name), RF_Public | RF_Standalone | RF_Transactional);
	UNiagaraSystemFactoryNew::InitializeSystem(System, true);
	FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid);
	return System;
}
}
#endif

int32 UCreateBudgetFxCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bDump = FParse::Param(*Params, TEXT("dump"));
	const bool bForce = FParse::Param(*Params, TEXT("force"));
	const bool bVerify = FParse::Param(*Params, TEXT("verify"));
	int32 Created = 0;
	int32 Unchanged = 0;
	int32 Failed = 0;
	// T58: the spell VFX material (T63: and its flipbook atlas). -spellvfx rebuilds them alone, without touching the Niagara systems.
	if (!bDump && !CreateSpellGlowMaterial(FParse::Param(*Params, TEXT("spellvfx"))))
	{
		++Failed;
	}
	for (const FBudgetFxRow& Row : BudgetStress::FxRows())
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *Row.AssetPath, *Row.Name);
		if (!bDump && !bForce)
		{
			if (UNiagaraSystem* Existing = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				if (bVerify && !Existing->GetEmitterHandles().IsEmpty())
				{
					const FVersionedNiagaraEmitter Saved = Existing->GetEmitterHandle(0).GetInstance();
					DumpEmitter(Row.Name, *Saved.Emitter, Saved.Version);
				}
				const FVersionedNiagaraEmitterData* Data = Existing->GetEmitterHandles().IsEmpty() ? nullptr : Existing->GetEmitterHandle(0).GetEmitterData();
				UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_FX %s emitters=%d sim=%s peak=%d unchanged"), *Row.Name,
					Existing->GetEmitterHandles().Num(), Data ? SimText(*Data) : TEXT("none"), Data ? Data->PreAllocationCount : 0);
				++Unchanged;
				continue;
			}
		}

		UPackage* Package = CreatePackage(*Row.AssetPath);
		UNiagaraSystem* System = BuildSystem(Row, Package);
		if (!System || System->GetEmitterHandles().Num() != 1)
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FX fail %s system not built"), *Row.Name);
			++Failed;
			continue;
		}
		const FVersionedNiagaraEmitter Instance = System->GetEmitterHandle(0).GetInstance();
		FVersionedNiagaraEmitterData* Data = Instance.GetEmitterData();
		if (bDump)
		{
			DumpEmitter(Row.Name, *Instance.Emitter, Instance.Version);
			continue;
		}
		// The sim target first, then a compile so the scripts exist for it, then the inputs, then the real compile.
		ApplyEmitterData(Row, *Data);
		FProperty* SimTargetProperty = FindFProperty<FProperty>(FVersionedNiagaraEmitterData::StaticStruct(), GET_MEMBER_NAME_CHECKED(FVersionedNiagaraEmitterData, SimTarget));
		FPropertyChangedEvent SimTargetChanged(SimTargetProperty);
		Instance.Emitter->PostEditChangeVersionedProperty(SimTargetChanged, Instance.Version);
		System->RequestCompile(true);
		System->WaitForCompilationComplete(true, false);
		const int32 InputsSet = ApplyInputs(Row, *Data);
		if (Row.BurstCount > 0 && !SetLoopForever(*Data))
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FX fail %s loop behavior switch not found"), *Row.Name);
			++Failed;
			continue;
		}
		System->RequestCompile(true);
		System->WaitForCompilationComplete(true, false);

		System->MarkPackageDirty();
		const FString PackageFile = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		if (bForce)
		{
			// Saving over an asset this process never loaded is refused; -force rebuilds, so the old file goes first.
			IFileManager::Get().Delete(*PackageFile, false, true, true);
		}
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, System, *PackageFile, SaveArgs))
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FX fail %s save %s"), *Row.Name, *PackageFile);
			++Failed;
			continue;
		}
		UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_FX %s emitters=%d sim=%s peak=%d inputs=%d -> %s"), *Row.Name, System->GetEmitterHandles().Num(),
			SimText(*Data), Data->PreAllocationCount, InputsSet, *PackageFile);
		++Created;
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_BUDGET_FX done created=%d unchanged=%d failed=%d"), Created, Unchanged, Failed);
	return Failed == 0 ? 0 : 1;
#else
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FX needs an editor build"));
	return 1;
#endif
}
