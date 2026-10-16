#include "Tools/CreateBudgetFxCommandlet.h"

#include "Budget/BudgetFx.h"
#include "MageArenaVR.h"

#if WITH_EDITOR
#include "Misc/PackageName.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
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
		System->RequestCompile(true);
		System->WaitForCompilationComplete(true, false);

		System->MarkPackageDirty();
		const FString PackageFile = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
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
