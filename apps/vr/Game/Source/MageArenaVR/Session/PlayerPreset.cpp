#include "Session/PlayerPreset.h"

#include "Dom/JsonObject.h"
#include "Kernel/Catalog.h"
#include "Kernel/KernelData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool Fail(FString& Error, const FString& Message)
{
	Error = FString::Printf(TEXT("presets: %s"), *Message);
	return false;
}

bool NeedString(const FJsonObject& Object, const TCHAR* Key, FString& Out, const TCHAR* Where, FString& Error)
{
	const TSharedPtr<FJsonValue> Value = Object.TryGetField(Key);
	if (!Value.IsValid() || Value->Type != EJson::String)
	{
		return Fail(Error, FString::Printf(TEXT("%s.%s must be a string"), Where, Key));
	}
	Out = Value->AsString();
	return true;
}

bool NeedBranch(const FJsonObject& Object, const TCHAR* Key, FString& Out, const TCHAR* Where, FString& Error)
{
	if (!NeedString(Object, Key, Out, Where, Error))
	{
		return false;
	}
	if (Out != TEXT("A") && Out != TEXT("B"))
	{
		return Fail(Error, FString::Printf(TEXT("%s.%s must be \"A\" or \"B\", not \"%s\""), Where, Key, *Out));
	}
	return true;
}

bool NeedNumber(const FJsonObject& Object, const TCHAR* Key, double& Out, const TCHAR* Where, FString& Error)
{
	const TSharedPtr<FJsonValue> Value = Object.TryGetField(Key);
	if (!Value.IsValid() || Value->Type != EJson::Number)
	{
		return Fail(Error, FString::Printf(TEXT("%s.%s must be a number"), Where, Key));
	}
	Out = Value->AsNumber();
	return true;
}

const FComposition* FindPinnedPreset(const FString& Name)
{
	for (const FComposition& Preset : KernelData().Presets)
	{
		if (Preset.Name == Name)
		{
			return &Preset;
		}
	}
	return nullptr;
}

bool ParsePick(const FJsonObject& Object, FStonePick& Pick, FString& Error)
{
	const TCHAR* Where = TEXT("stonePick");
	double AfterBout = 0.0;
	if (!NeedString(Object, TEXT("branch"), Pick.Branch, Where, Error)
		|| !NeedNumber(Object, TEXT("afterBout"), AfterBout, Where, Error)
		|| !NeedBranch(Object, TEXT("left"), Pick.Left, Where, Error)
		|| !NeedBranch(Object, TEXT("right"), Pick.Right, Where, Error)
		|| !NeedBranch(Object, TEXT("default"), Pick.Default, Where, Error)
		|| !NeedNumber(Object, TEXT("timeoutS"), Pick.TimeoutS, Where, Error))
	{
		return false;
	}
	if (Pick.Branch != TEXT("tide_orb"))
	{
		return Fail(Error, FString::Printf(TEXT("stonePick.branch \"%s\" is not supported (tide_orb only)"), *Pick.Branch));
	}
	if (AfterBout != FMath::RoundToDouble(AfterBout) || AfterBout < 1.0)
	{
		return Fail(Error, TEXT("stonePick.afterBout must be a whole bout number from 1"));
	}
	if (Pick.Left == Pick.Right)
	{
		return Fail(Error, TEXT("stonePick.left and stonePick.right must be different branches"));
	}
	if (!(Pick.TimeoutS > 0.0))
	{
		return Fail(Error, TEXT("stonePick.timeoutS must be above 0"));
	}
	Pick.AfterBout = static_cast<int32>(AfterBout);
	Pick.bEnabled = true;
	return true;
}
}

FComposition FPlayerPreset::WithPick(const FString& Picked) const
{
	FComposition Out = Composition;
	if (Pick.bEnabled && (Picked == TEXT("A") || Picked == TEXT("B")))
	{
		Out.Branches.TideOrb = Picked;
	}
	return Out;
}

FString PlayerPresetPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/presets.json")));
}

bool LoadPlayerPreset(FPlayerPreset& Out, FString& Error, const FString& InPath)
{
	Out = FPlayerPreset();
	const FString Path = InPath.IsEmpty() ? PlayerPresetPath() : InPath;
	if (!KernelData().bReady)
	{
		return Fail(Error, TEXT("kernel data is not ready"));
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return Fail(Error, FString::Printf(TEXT("cannot read %s"), *Path));
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Fail(Error, FString::Printf(TEXT("%s is not JSON"), *Path));
	}
	double Version = 0.0;
	if (!NeedNumber(*Root, TEXT("version"), Version, TEXT("root"), Error))
	{
		return false;
	}
	if (Version != 1.0)
	{
		return Fail(Error, TEXT("version must be 1"));
	}
	FString PlayerId;
	if (!NeedString(*Root, TEXT("player"), PlayerId, TEXT("root"), Error))
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* Presets = nullptr;
	if (!Root->TryGetObjectField(TEXT("presets"), Presets) || !Presets || !Presets->IsValid())
	{
		return Fail(Error, TEXT("presets must be an object"));
	}
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!(*Presets)->TryGetObjectField(PlayerId, Entry) || !Entry || !Entry->IsValid())
	{
		return Fail(Error, FString::Printf(TEXT("player preset \"%s\" is not in presets"), *PlayerId));
	}
	const FJsonObject& Preset = **Entry;
	const FString Where = FString::Printf(TEXT("presets.%s"), *PlayerId);
	FPlayerPreset Parsed;
	Parsed.Id = PlayerId;
	if (!NeedString(Preset, TEXT("basedOn"), Parsed.BasedOn, *Where, Error))
	{
		return false;
	}
	if (!FindPinnedPreset(Parsed.BasedOn))
	{
		return Fail(Error, FString::Printf(TEXT("%s.basedOn \"%s\" is not a pinned runtime.json water preset"), *Where, *Parsed.BasedOn));
	}
	const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
	if (!Preset.TryGetArrayField(TEXT("lines"), Lines) || !Lines)
	{
		return Fail(Error, FString::Printf(TEXT("%s.lines must be an array"), *Where));
	}
	for (const TSharedPtr<FJsonValue>& Line : *Lines)
	{
		if (!Line.IsValid() || Line->Type != EJson::String)
		{
			return Fail(Error, FString::Printf(TEXT("%s.lines must hold strings"), *Where));
		}
		Parsed.Composition.Lines.Add(Line->AsString());
	}
	const TSharedPtr<FJsonObject>* Branches = nullptr;
	if (!Preset.TryGetObjectField(TEXT("branches"), Branches) || !Branches || !Branches->IsValid())
	{
		return Fail(Error, FString::Printf(TEXT("%s.branches must be an object"), *Where));
	}
	const FString BranchWhere = Where + TEXT(".branches");
	if (!NeedBranch(**Branches, TEXT("lash"), Parsed.Composition.Branches.Lash, *BranchWhere, Error)
		|| !NeedBranch(**Branches, TEXT("mirror"), Parsed.Composition.Branches.Mirror, *BranchWhere, Error)
		|| !NeedBranch(**Branches, TEXT("tide_orb"), Parsed.Composition.Branches.TideOrb, *BranchWhere, Error))
	{
		return false;
	}
	Parsed.Composition.Name = PlayerId;
	const TArray<FString> Problems = ValidateComposition(Parsed.Composition);
	if (Problems.Num() > 0)
	{
		return Fail(Error, FString::Printf(TEXT("%s: %s"), *Where, *Problems[0]));
	}
	const TSharedPtr<FJsonObject>* Pick = nullptr;
	if (Preset.HasField(TEXT("stonePick")))
	{
		if (!Preset.TryGetObjectField(TEXT("stonePick"), Pick) || !Pick || !Pick->IsValid())
		{
			return Fail(Error, FString::Printf(TEXT("%s.stonePick must be an object"), *Where));
		}
		if (!ParsePick(**Pick, Parsed.Pick, Error))
		{
			return false;
		}
		if (Parsed.Pick.Default != Parsed.Composition.Branches.TideOrb)
		{
			return Fail(Error, FString::Printf(TEXT("%s.stonePick.default must equal branches.tide_orb"), *Where));
		}
	}
	Out = MoveTemp(Parsed);
	return true;
}
