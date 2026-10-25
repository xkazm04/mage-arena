#include "Session/StorySlice.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/ArenaFlags.h"

namespace
{
bool SliceFail(FString& Error, const FString& Message)
{
	Error = FString::Printf(TEXT("slice.json: %s"), *Message);
	return false;
}

bool SliceNumber(const FJsonObject& Object, const TCHAR* Key, double& Out, const FString& Where, FString& Error)
{
	const TSharedPtr<FJsonValue> Value = Object.TryGetField(Key);
	if (!Value.IsValid() || Value->Type != EJson::Number)
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.%s must be a number"), *Where, Key));
	}
	Out = Value->AsNumber();
	return true;
}

bool SliceString(const FJsonObject& Object, const TCHAR* Key, FString& Out, const FString& Where, FString& Error)
{
	const TSharedPtr<FJsonValue> Value = Object.TryGetField(Key);
	if (!Value.IsValid() || Value->Type != EJson::String || Value->AsString().IsEmpty())
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.%s must be a non-empty string"), *Where, Key));
	}
	Out = Value->AsString();
	return true;
}

// An array of objects under Key. A missing key is an empty array when bOptional.
bool SliceObjects(const FJsonObject& Object, const TCHAR* Key, bool bOptional, TArray<TSharedPtr<FJsonObject>>& Out, const FString& Where,
    FString& Error)
{
	Out.Reset();
	const TSharedPtr<FJsonValue> Value = Object.TryGetField(Key);
	if (!Value.IsValid())
	{
		return bOptional ? true : SliceFail(Error, FString::Printf(TEXT("%s.%s is missing"), *Where, Key));
	}
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Object.TryGetArrayField(Key, Items) || !Items)
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.%s must be an array"), *Where, Key));
	}
	for (int32 Index = 0; Index < Items->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Item = (*Items)[Index];
		if (!Item.IsValid() || Item->Type != EJson::Object || !Item->AsObject().IsValid())
		{
			return SliceFail(Error, FString::Printf(TEXT("%s.%s[%d] must be an object"), *Where, Key, Index));
		}
		Out.Add(Item->AsObject());
	}
	return true;
}

bool SliceLines(const FJsonObject& Object, const TCHAR* Key, bool bOptional, TArray<FStoryLine>& Out, const FString& Where, FString& Error)
{
	TArray<TSharedPtr<FJsonObject>> Items;
	if (!SliceObjects(Object, Key, bOptional, Items, Where, Error))
	{
		return false;
	}
	Out.Reset();
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FString At = FString::Printf(TEXT("%s.%s[%d]"), *Where, Key, Index);
		FStoryLine Line;
		if (!SliceString(*Items[Index], TEXT("text"), Line.Text, At, Error) || !SliceNumber(*Items[Index], TEXT("dwellS"), Line.DwellS, At, Error))
		{
			return false;
		}
		if (!(Line.DwellS > 0.0))
		{
			return SliceFail(Error, FString::Printf(TEXT("%s.dwellS must be above 0"), *At));
		}
		Out.Add(Line);
	}
	return true;
}

// null (or absent) adds no clause; otherwise one { flag, op, value } clause.
bool SliceWhen(const FJsonObject& Object, const TCHAR* Key, TArray<FStoryWhen>& Out, const FString& Where, FString& Error)
{
	const TSharedPtr<FJsonValue> Value = Object.TryGetField(Key);
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		return true;
	}
	const FString At = FString::Printf(TEXT("%s.%s"), *Where, Key);
	if (Value->Type != EJson::Object || !Value->AsObject().IsValid())
	{
		return SliceFail(Error, FString::Printf(TEXT("%s must be null or an object"), *At));
	}
	const TSharedPtr<FJsonObject> Clause = Value->AsObject();
	FStoryWhen When;
	if (!SliceString(*Clause, TEXT("flag"), When.Flag, At, Error) || !SliceString(*Clause, TEXT("op"), When.Op, At, Error))
	{
		return false;
	}
	if (When.Op != TEXT("eq") && When.Op != TEXT("gte") && When.Op != TEXT("lte"))
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.op must be eq, gte or lte, not %s"), *At, *When.Op));
	}
	const TSharedPtr<FJsonValue> Compare = Clause->TryGetField(TEXT("value"));
	if (Compare.IsValid() && Compare->Type == EJson::Boolean)
	{
		if (When.Op != TEXT("eq"))
		{
			return SliceFail(Error, FString::Printf(TEXT("%s: a bool value takes op eq"), *At));
		}
		When.bIsBool = true;
		When.bBool = Compare->AsBool();
	}
	else if (Compare.IsValid() && Compare->Type == EJson::Number)
	{
		When.Number = Compare->AsNumber();
	}
	else
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.value must be a bool or a number"), *At));
	}
	Out.Add(When);
	return true;
}

bool SliceScene(const FJsonObject& Object, int32 Index, FStoryScene& Scene, FString& Error)
{
	const FString Where = FString::Printf(TEXT("scenes[%d]"), Index);
	double Intermission = 0.0;
	double AfterWave = 0.0;
	if (!SliceString(Object, TEXT("id"), Scene.Id, Where, Error) ||
	    !SliceNumber(Object, TEXT("intermission"), Intermission, Where, Error) ||
	    !SliceNumber(Object, TEXT("afterWave"), AfterWave, Where, Error) ||
	    !SliceNumber(Object, TEXT("floorS"), Scene.FloorS, Where, Error) ||
	    !SliceNumber(Object, TEXT("naturalS"), Scene.NaturalS, Where, Error) ||
	    !SliceNumber(Object, TEXT("maxS"), Scene.MaxS, Where, Error))
	{
		return false;
	}
	Scene.Intermission = FMath::RoundToInt(Intermission);
	Scene.AfterWave = FMath::RoundToInt(AfterWave);
	if (Scene.Intermission < 1 ||
	    static_cast<double>(Scene.Intermission) != Intermission ||
	    Scene.AfterWave < 1 ||
	    static_cast<double>(Scene.AfterWave) != AfterWave)
	{
		return SliceFail(Error, FString::Printf(TEXT("%s: intermission and afterWave must be whole numbers from 1"), *Where));
	}
	if (!(Scene.FloorS >= 0.0 &&
	        Scene.FloorS <= Scene.NaturalS &&
	        Scene.NaturalS <= Scene.MaxS &&
	        Scene.MaxS > 0.0 &&
	        Scene.MaxS <= FStorySlice::MaxSceneS))
	{
		return SliceFail(Error, FString::Printf(TEXT("%s: needs 0 <= floorS <= naturalS <= maxS <= %.0f"), *Where, FStorySlice::MaxSceneS));
	}
	TArray<TSharedPtr<FJsonObject>> Variants;
	if (!SliceObjects(Object, TEXT("variants"), false, Variants, Where, Error))
	{
		return false;
	}
	for (int32 VariantIndex = 0; VariantIndex < Variants.Num(); ++VariantIndex)
	{
		const FString At = FString::Printf(TEXT("%s.variants[%d]"), *Where, VariantIndex);
		FStoryVariant Variant;
		if (!SliceString(*Variants[VariantIndex], TEXT("id"), Variant.Id, At, Error) ||
		    !SliceWhen(*Variants[VariantIndex], TEXT("when"), Variant.When, At, Error) ||
		    !SliceWhen(*Variants[VariantIndex], TEXT("andWhen"), Variant.When, At, Error) ||
		    !SliceLines(*Variants[VariantIndex], TEXT("lines"), false, Variant.Lines, At, Error))
		{
			return false;
		}
		Scene.Variants.Add(MoveTemp(Variant));
	}
	if (Scene.Variants.Num() == 0)
	{
		return SliceFail(Error, FString::Printf(TEXT("%s has no variant"), *Where));
	}
	if (!SliceLines(Object, TEXT("always"), true, Scene.Always, Where, Error))
	{
		return false;
	}
	const TSharedPtr<FJsonValue> ChoiceValue = Object.TryGetField(TEXT("choice"));
	if (!ChoiceValue.IsValid() || ChoiceValue->Type == EJson::Null)
	{
		return true;
	}
	if (ChoiceValue->Type != EJson::Object || !ChoiceValue->AsObject().IsValid())
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.choice must be null or an object"), *Where));
	}
	const TSharedPtr<FJsonObject> Choice = ChoiceValue->AsObject();
	const FString At = Where + TEXT(".choice");
	if (!SliceString(*Choice, TEXT("writes"), Scene.Writes, At, Error))
	{
		return false;
	}
	if (Scene.Writes != FArenaFlags::IntermissionKey(Scene.Intermission))
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.writes must be %s"), *At, *FArenaFlags::IntermissionKey(Scene.Intermission)));
	}
	const TSharedPtr<FJsonValue> Keeps = Choice->TryGetField(TEXT("keepsExistingPick"));
	if (Keeps.IsValid() && Keeps->Type != EJson::Boolean)
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.keepsExistingPick must be a bool"), *At));
	}
	Scene.bKeepsExistingPick = Keeps.IsValid() && Keeps->AsBool();
	TArray<TSharedPtr<FJsonObject>> Stones;
	if (!SliceObjects(*Choice, TEXT("stones"), false, Stones, At, Error))
	{
		return false;
	}
	if (Stones.Num() != 3)
	{
		return SliceFail(Error, FString::Printf(TEXT("%s needs three stones (left, centre, right)"), *At));
	}
	Scene.Stones.Init(FString(), 3);
	for (int32 StoneIndex = 0; StoneIndex < Stones.Num(); ++StoneIndex)
	{
		const FString StoneAt = FString::Printf(TEXT("%s.stones[%d]"), *At, StoneIndex);
		double Stone = -1.0;
		FString Label;
		if (!SliceNumber(*Stones[StoneIndex], TEXT("index"), Stone, StoneAt, Error) ||
		    !SliceString(*Stones[StoneIndex], TEXT("label"), Label, StoneAt, Error))
		{
			return false;
		}
		if (Stone != static_cast<double>(StoneIndex))
		{
			return SliceFail(Error, FString::Printf(TEXT("%s.index must be %d"), *StoneAt, StoneIndex));
		}
		if (FStorySlice::CountWords(Label) > FStorySlice::MaxLabelWords)
		{
			return SliceFail(Error, FString::Printf(TEXT("%s.label has more than %d words"), *StoneAt, FStorySlice::MaxLabelWords));
		}
		Scene.Stones[StoneIndex] = Label;
	}
	return true;
}
}

bool FStoryWhen::Matches(const FArenaFlags& Flags) const
{
	if (bIsBool)
	{
		bool bValue = false;
		return Flags.GetBool(Flag, bValue) && bValue == bBool;
	}
	double Value = 0.0;
	if (!Flags.GetNumber(Flag, Value))
	{
		return false;
	}
	if (Op == TEXT("gte"))
	{
		return Value + 1.0e-9 >= Number;
	}
	if (Op == TEXT("lte"))
	{
		return Value <= Number + 1.0e-9;
	}
	return FMath::IsNearlyEqual(Value, Number, 1.0e-9);
}

int32 FStoryScene::SelectVariant(const FArenaFlags& Flags) const
{
	for (int32 Index = 0; Index < Variants.Num(); ++Index)
	{
		bool bAll = true;
		for (const FStoryWhen& When : Variants[Index].When)
		{
			bAll &= When.Matches(Flags);
		}
		if (bAll)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FString FStorySlice::DefaultFilePath()
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/story/slice.json"));
	FPaths::CollapseRelativeDirectories(Path);
	return Path;
}

int32 FStorySlice::CountWords(const FString& Text)
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	return Words.Num();
}

void FStorySlice::Reset()
{
	bLoaded = false;
	Scenes.Reset();
	BeatFloors.Reset();
}

bool FStorySlice::Load(const FString& Path, FString& OutError)
{
	Reset();
	FString Text;
	if (!IFileManager::Get().FileExists(*Path))
	{
		return SliceFail(OutError, FString::Printf(TEXT("missing (%s)"), *Path));
	}
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return SliceFail(OutError, FString::Printf(TEXT("unreadable (%s)"), *Path));
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		return SliceFail(OutError, FString::Printf(TEXT("not JSON (%s)"), *Path));
	}
	double Version = 0.0;
	if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1.0)
	{
		return SliceFail(OutError, FString::Printf(TEXT("version must be 1 (%s)"), *Path));
	}
	TArray<TSharedPtr<FJsonObject>> Beats;
	if (!SliceObjects(*Root, TEXT("beats"), false, Beats, TEXT("root"), OutError))
	{
		return false;
	}
	TMap<FString, double> Floors;
	for (int32 Index = 0; Index < Beats.Num(); ++Index)
	{
		const FString Where = FString::Printf(TEXT("beats[%d]"), Index);
		FString Id;
		double Floor = 0.0;
		double Max = 0.0;
		if (!SliceString(*Beats[Index], TEXT("id"), Id, Where, OutError) ||
		    !SliceNumber(*Beats[Index], TEXT("floorS"), Floor, Where, OutError) ||
		    !SliceNumber(*Beats[Index], TEXT("maxS"), Max, Where, OutError))
		{
			return false;
		}
		if (!(Floor >= 0.0 && Floor <= Max) || Floors.Contains(Id))
		{
			return SliceFail(OutError, FString::Printf(TEXT("%s (%s): needs a unique id and 0 <= floorS <= maxS"), *Where, *Id));
		}
		Floors.Add(Id, Floor);
	}
	TArray<TSharedPtr<FJsonObject>> SceneObjects;
	if (!SliceObjects(*Root, TEXT("scenes"), false, SceneObjects, TEXT("root"), OutError))
	{
		return false;
	}
	TArray<FStoryScene> Read;
	for (int32 Index = 0; Index < SceneObjects.Num(); ++Index)
	{
		FStoryScene Scene;
		if (!SliceScene(*SceneObjects[Index], Index, Scene, OutError))
		{
			return false;
		}
		for (const FStoryScene& Other : Read)
		{
			if (Other.Intermission == Scene.Intermission || Other.AfterWave == Scene.AfterWave)
			{
				return SliceFail(OutError, FString::Printf(TEXT("scenes[%d]: two scenes for intermission %d or after wave %d"), Index, Scene.Intermission, Scene.AfterWave));
			}
		}
		Read.Add(MoveTemp(Scene));
	}
	Scenes = MoveTemp(Read);
	BeatFloors = MoveTemp(Floors);
	bLoaded = true;
	return true;
}

const FStoryScene* FStorySlice::FindSceneAfterWave(int32 WaveN) const
{
	if (!bLoaded)
	{
		return nullptr;
	}
	for (const FStoryScene& Scene : Scenes)
	{
		if (Scene.AfterWave == WaveN)
		{
			return &Scene;
		}
	}
	return nullptr;
}

double FStorySlice::BeatFloorS(const TCHAR* BeatId) const
{
	const double* Found = bLoaded ? BeatFloors.Find(BeatId) : nullptr;
	return Found ? *Found : 0.0;
}
