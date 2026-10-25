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

// T27: a string or null (or absent, read as null). Null leaves Out empty.
bool SliceOptionalString(const FJsonObject& Object, const TCHAR* Key, FString& Out, const FString& Where, FString& Error)
{
	Out.Reset();
	const TSharedPtr<FJsonValue> Value = Object.TryGetField(Key);
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		return true;
	}
	if (Value->Type != EJson::String || Value->AsString().IsEmpty())
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.%s must be null or a non-empty string"), *Where, Key));
	}
	Out = Value->AsString();
	return true;
}

// T27: a line's cue id and speaker, both string or null.
bool SliceVoice(const FJsonObject& Object, FStoryLine& Line, const FString& Where, FString& Error)
{
	return SliceOptionalString(Object, TEXT("cue"), Line.Cue, Where, Error) && SliceOptionalString(Object, TEXT("speaker"), Line.Speaker, Where, Error);
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
		if (!SliceString(*Items[Index], TEXT("text"), Line.Text, At, Error) || !SliceNumber(*Items[Index], TEXT("dwellS"), Line.DwellS, At, Error) ||
		    !SliceVoice(*Items[Index], Line, At, Error))
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

// T27: one beat. Its lines are top-level (one entry with no key) or per variant (intro-w4 by rival, closing by result).
bool SliceBeat(const FJsonObject& Object, int32 Index, FStoryBeat& Beat, FString& Error)
{
	const FString Where = FString::Printf(TEXT("beats[%d]"), Index);
	double Wave = 0.0;
	if (!SliceString(Object, TEXT("id"), Beat.Id, Where, Error) ||
	    !SliceString(Object, TEXT("phase"), Beat.Phase, Where, Error) ||
	    !SliceNumber(Object, TEXT("floorS"), Beat.FloorS, Where, Error) ||
	    !SliceNumber(Object, TEXT("naturalS"), Beat.NaturalS, Where, Error) ||
	    !SliceNumber(Object, TEXT("maxS"), Beat.MaxS, Where, Error))
	{
		return false;
	}
	if (!(Beat.FloorS >= 0.0 && Beat.FloorS <= Beat.NaturalS && Beat.NaturalS <= Beat.MaxS))
	{
		return SliceFail(Error, FString::Printf(TEXT("%s (%s): needs 0 <= floorS <= naturalS <= maxS"), *Where, *Beat.Id));
	}
	if (Object.HasField(TEXT("wave")) && (!SliceNumber(Object, TEXT("wave"), Wave, Where, Error) || Wave < 1.0 || FMath::RoundToDouble(Wave) != Wave))
	{
		return SliceFail(Error, FString::Printf(TEXT("%s.wave must be a whole number from 1"), *Where));
	}
	Beat.Wave = FMath::RoundToInt(Wave);
	if (Object.HasField(TEXT("lines")))
	{
		FStoryBeatLines Lines;
		if (!SliceLines(Object, TEXT("lines"), false, Lines.Lines, Where, Error))
		{
			return false;
		}
		Beat.Variants.Add(MoveTemp(Lines));
		return true;
	}
	TArray<TSharedPtr<FJsonObject>> Variants;
	if (!SliceObjects(Object, TEXT("variants"), false, Variants, Where, Error))
	{
		return false;
	}
	for (int32 VariantIndex = 0; VariantIndex < Variants.Num(); ++VariantIndex)
	{
		const FString At = FString::Printf(TEXT("%s.variants[%d]"), *Where, VariantIndex);
		FStoryBeatLines Lines;
		if (!SliceString(*Variants[VariantIndex], TEXT("id"), Lines.Id, At, Error) ||
		    !SliceOptionalString(*Variants[VariantIndex], TEXT("rival"), Lines.Rival, At, Error) ||
		    !SliceLines(*Variants[VariantIndex], TEXT("lines"), false, Lines.Lines, At, Error))
		{
			return false;
		}
		const TSharedPtr<FJsonValue> Won = Variants[VariantIndex]->TryGetField(TEXT("requiresFinalWon"));
		if (Won.IsValid() && Won->Type != EJson::Boolean)
		{
			return SliceFail(Error, FString::Printf(TEXT("%s.requiresFinalWon must be a bool"), *At));
		}
		Lines.FinalWon = Won.IsValid() ? (Won->AsBool() ? 1 : 0) : -1;
		Beat.Variants.Add(MoveTemp(Lines));
	}
	if (Beat.Variants.Num() == 0)
	{
		return SliceFail(Error, FString::Printf(TEXT("%s has no lines"), *Where));
	}
	return true;
}

// T27: taunts[] { rival, lines[] { cue, speaker, atHpFraction, text } }. Optional: a slice with no taunts has none.
bool SliceTaunts(const FJsonObject& Root, TArray<FStoryTaunt>& Out, FString& Error)
{
	TArray<TSharedPtr<FJsonObject>> Rivals;
	if (!SliceObjects(Root, TEXT("taunts"), true, Rivals, TEXT("root"), Error))
	{
		return false;
	}
	for (int32 Index = 0; Index < Rivals.Num(); ++Index)
	{
		const FString Where = FString::Printf(TEXT("taunts[%d]"), Index);
		FString Rival;
		TArray<TSharedPtr<FJsonObject>> Lines;
		if (!SliceString(*Rivals[Index], TEXT("rival"), Rival, Where, Error) || !SliceObjects(*Rivals[Index], TEXT("lines"), false, Lines, Where, Error))
		{
			return false;
		}
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString At = FString::Printf(TEXT("%s.lines[%d]"), *Where, LineIndex);
			FStoryTaunt Taunt;
			Taunt.Rival = Rival;
			if (!SliceString(*Lines[LineIndex], TEXT("text"), Taunt.Line.Text, At, Error) ||
			    !SliceNumber(*Lines[LineIndex], TEXT("atHpFraction"), Taunt.AtHpFraction, At, Error) ||
			    !SliceVoice(*Lines[LineIndex], Taunt.Line, At, Error))
			{
				return false;
			}
			if (!(Taunt.AtHpFraction > 0.0 && Taunt.AtHpFraction < 1.0) || Taunt.Line.Cue.IsEmpty())
			{
				return SliceFail(Error, FString::Printf(TEXT("%s needs a cue and 0 < atHpFraction < 1"), *At));
			}
			Out.Add(MoveTemp(Taunt));
		}
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
	Beats.Reset();
	Taunts.Reset();
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
	TArray<TSharedPtr<FJsonObject>> BeatObjects;
	if (!SliceObjects(*Root, TEXT("beats"), false, BeatObjects, TEXT("root"), OutError))
	{
		return false;
	}
	TArray<FStoryBeat> ReadBeats;
	for (int32 Index = 0; Index < BeatObjects.Num(); ++Index)
	{
		FStoryBeat Beat;
		if (!SliceBeat(*BeatObjects[Index], Index, Beat, OutError))
		{
			return false;
		}
		if (ReadBeats.ContainsByPredicate([&Beat](const FStoryBeat& Other)
		{ return Other.Id == Beat.Id; }))
		{
			return SliceFail(OutError, FString::Printf(TEXT("beats[%d]: the id %s is used twice"), Index, *Beat.Id));
		}
		ReadBeats.Add(MoveTemp(Beat));
	}
	TArray<FStoryTaunt> ReadTaunts;
	if (!SliceTaunts(*Root, ReadTaunts, OutError))
	{
		return false;
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
	Beats = MoveTemp(ReadBeats);
	Taunts = MoveTemp(ReadTaunts);
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

const FStoryBeat* FStorySlice::FindBeat(const TCHAR* BeatId) const
{
	return bLoaded ? Beats.FindByPredicate([BeatId](const FStoryBeat& Beat)
	{ return Beat.Id == BeatId; })
	               : nullptr;
}

const FStoryBeat* FStorySlice::FindIntroBeat(int32 WaveN) const
{
	return bLoaded ? Beats.FindByPredicate([WaveN](const FStoryBeat& Beat)
	{ return Beat.Phase == TEXT("intro") && Beat.Wave == WaveN; })
	               : nullptr;
}

double FStorySlice::BeatFloorS(const TCHAR* BeatId) const
{
	const FStoryBeat* Beat = FindBeat(BeatId);
	return Beat ? Beat->FloorS : 0.0;
}

double FStorySlice::BeatNaturalS(const TCHAR* BeatId) const
{
	const FStoryBeat* Beat = FindBeat(BeatId);
	return Beat ? Beat->NaturalS : 0.0;
}

const FStoryTaunt* FStorySlice::FindTaunt(const FString& Rival, double AtHpFraction) const
{
	if (!bLoaded)
	{
		return nullptr;
	}
	return Taunts.FindByPredicate([&Rival, AtHpFraction](const FStoryTaunt& Taunt)
	{ return Taunt.Rival == Rival && FMath::IsNearlyEqual(Taunt.AtHpFraction, AtHpFraction, 1.0e-6); });
}

const TArray<FStoryLine>* FStoryBeat::SelectLines(const FString& Rival, bool bFinalWon) const
{
	for (const FStoryBeatLines& Entry : Variants)
	{
		const bool bRival = Entry.Rival.IsEmpty() || Entry.Rival == Rival;
		const bool bResult = Entry.FinalWon < 0 || (Entry.FinalWon == 1) == bFinalWon;
		if (bRival && bResult)
		{
			return &Entry.Lines;
		}
	}
	return Variants.Num() > 0 ? &Variants[0].Lines : nullptr;
}

FString FStorySlice::LineEvent(const FString& Id, int32 Index)
{
	return FString::Printf(TEXT("%s#%d"), *Id, Index + 1);
}

TArray<const FStoryLine*> FStorySlice::SceneLines(const FStoryScene& Scene, int32 Variant)
{
	TArray<const FStoryLine*> Lines;
	if (Scene.Variants.IsValidIndex(Variant))
	{
		for (const FStoryLine& Line : Scene.Variants[Variant].Lines)
		{
			Lines.Add(&Line);
		}
	}
	for (const FStoryLine& Line : Scene.Always)
	{
		Lines.Add(&Line);
	}
	return Lines;
}

TMap<FString, TArray<FStoryVoiceTrigger>> FStorySlice::VoiceTriggers() const
{
	TMap<FString, TArray<FStoryVoiceTrigger>> Out;
	auto Add = [&Out](const FStoryLine& Line, const FString& Event)
	{
		if (Line.Cue.IsEmpty())
		{
			return;
		}
		TArray<FStoryVoiceTrigger>& Triggers = Out.FindOrAdd(Line.Cue);
		if (!Triggers.ContainsByPredicate([&Event](const FStoryVoiceTrigger& Trigger)
		{ return Trigger.Event == Event; }))
		{
			Triggers.Add({Event, Line.Text, Line.Speaker});
		}
	};
	for (const FStoryBeat& Beat : Beats)
	{
		for (const FStoryBeatLines& Entry : Beat.Variants)
		{
			for (int32 Index = 0; Index < Entry.Lines.Num(); ++Index)
			{
				Add(Entry.Lines[Index], LineEvent(Beat.Id, Index));
			}
		}
	}
	for (const FStoryScene& Scene : Scenes)
	{
		for (int32 Variant = 0; Variant < Scene.Variants.Num(); ++Variant)
		{
			const TArray<const FStoryLine*> Lines = SceneLines(Scene, Variant);
			for (int32 Index = 0; Index < Lines.Num(); ++Index)
			{
				Add(*Lines[Index], LineEvent(Scene.Id, Index));
			}
		}
	}
	TMap<FString, int32> TauntIndex;
	for (const FStoryTaunt& Taunt : Taunts)
	{
		int32& Index = TauntIndex.FindOrAdd(Taunt.Rival);
		Add(Taunt.Line, LineEvent(TEXT("taunt.") + Taunt.Rival, Index++));
	}
	return Out;
}
