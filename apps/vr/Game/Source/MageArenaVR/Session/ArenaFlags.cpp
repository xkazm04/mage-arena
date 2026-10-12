#include "Session/ArenaFlags.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void FArenaFlags::Reset()
{
	Values.Reset();
}

void FArenaFlags::SetBool(const FString& Key, bool bValue)
{
	FValue& Value = Values.FindOrAdd(Key);
	Value.bIsBool = true;
	Value.bBool = bValue;
	Value.Number = 0.0;
}

void FArenaFlags::SetNumber(const FString& Key, double Value)
{
	FValue& Slot = Values.FindOrAdd(Key);
	Slot.bIsBool = false;
	Slot.bBool = false;
	Slot.Number = Value;
}

bool FArenaFlags::GetBool(const FString& Key, bool& bOut) const
{
	const FValue* Found = Values.Find(Key);
	if (!Found || !Found->bIsBool)
	{
		return false;
	}
	bOut = Found->bBool;
	return true;
}

bool FArenaFlags::GetNumber(const FString& Key, double& Out) const
{
	const FValue* Found = Values.Find(Key);
	if (!Found || Found->bIsBool)
	{
		return false;
	}
	Out = Found->Number;
	return true;
}

bool FArenaFlags::Has(const FString& Key) const
{
	return Values.Contains(Key);
}

TArray<FString> FArenaFlags::Keys() const
{
	TArray<FString> Out;
	Values.GetKeys(Out);
	Out.Sort();
	return Out;
}

FString FArenaFlags::TiroKey(int32 WaveN, const TCHAR* Field)
{
	return FString::Printf(TEXT("arena.tiro.1.w%d.%s"), WaveN, Field);
}

bool FArenaFlags::Save(const FString& Path) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	TSharedRef<FJsonObject> Flags = MakeShared<FJsonObject>();
	for (const FString& Key : Keys())
	{
		const FValue& Value = Values.FindChecked(Key);
		if (Value.bIsBool)
		{
			Flags->SetBoolField(Key, Value.bBool);
		}
		else
		{
			Flags->SetNumberField(Key, Value.Number);
		}
	}
	Root->SetObjectField(TEXT("flags"), Flags);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveStringToFile(Text, *Path);
}

bool FArenaFlags::Load(const FString& Path)
{
	Values.Reset();
	FString Text;
	if (!IFileManager::Get().FileExists(*Path))
	{
		return true;
	}
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogMageArena, Warning, TEXT("Arena flags unreadable (%s); starting with none"), *Path);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	double Version = 0.0;
	const TSharedPtr<FJsonObject>* Flags = nullptr;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->TryGetNumberField(TEXT("version"), Version)
		|| Version != 1.0 || !Root->TryGetObjectField(TEXT("flags"), Flags) || !Flags || !Flags->IsValid())
	{
		UE_LOG(LogMageArena, Warning, TEXT("Arena flags are not a version 1 flag file (%s); starting with none"), *Path);
		return false;
	}
	TMap<FString, FValue> Read;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Flags)->Values)
	{
		FValue Value;
		if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Boolean)
		{
			Value.bIsBool = true;
			Value.bBool = Pair.Value->AsBool();
		}
		else if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Number)
		{
			Value.Number = Pair.Value->AsNumber();
		}
		else
		{
			UE_LOG(LogMageArena, Warning, TEXT("Arena flag %s is not a bool or a number (%s); starting with none"), *Pair.Key, *Path);
			return false;
		}
		Read.Add(Pair.Key, Value);
	}
	Values = MoveTemp(Read);
	return true;
}
