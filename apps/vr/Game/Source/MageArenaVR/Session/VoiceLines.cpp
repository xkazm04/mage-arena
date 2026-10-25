#include "Session/VoiceLines.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
// The rail prompt stands 220 cm ahead of the seat and about 40 cm above the seated eye (SessionPresentation, the rail);
// Septima and Ophel stand 150 cm to its left and right. The grille is above the gate at the far end of the oval
// (arena-layout.json floor radiusXM 16, wall heightM 3). A rival speaks from about head height.
const FVector RailAheadCm(220.0, 0.0, 40.0);
const double RailSideCm = 150.0;
const FVector GrilleCm(1600.0, 0.0, 450.0);
const double MouthAboveFloorCm = 170.0;
}

bool FVoiceManifest::LoadFromFile(const FString& Path, FString& OutError)
{
	Rows.Reset();
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		OutError = FString::Printf(TEXT("voice manifest missing or not JSON (%s)"), *Path);
		return false;
	}
	double Version = 0.0;
	const TArray<TSharedPtr<FJsonValue>>* Cues = nullptr;
	if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1.0 || !Root->TryGetArrayField(TEXT("cues"), Cues) || !Cues)
	{
		OutError = FString::Printf(TEXT("voice manifest needs version 1 and a cues array (%s)"), *Path);
		return false;
	}
	TArray<FVoiceManifestRow> Read;
	for (int32 Index = 0; Index < Cues->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Cue = (*Cues)[Index].IsValid() ? (*Cues)[Index]->AsObject() : nullptr;
		FVoiceManifestRow Row;
		double Words = 0.0;
		if (!Cue.IsValid() || !Cue->TryGetStringField(TEXT("id"), Row.Id) || !Cue->TryGetStringField(TEXT("text"), Row.Text) || !Cue->TryGetStringField(TEXT("file"), Row.File) || !Cue->TryGetNumberField(TEXT("words"), Words) || Row.Id.IsEmpty() || Row.Text.IsEmpty() || Row.File.IsEmpty())
		{
			OutError = FString::Printf(TEXT("voice manifest cues[%d] needs id, text, words and file"), Index);
			return false;
		}
		// speaker is null for the unvoiced prologue tableau.
		Cue->TryGetStringField(TEXT("speaker"), Row.Speaker);
		Row.Words = FMath::RoundToInt(Words);
		if (Read.ContainsByPredicate([&Row](const FVoiceManifestRow& Other)
		{ return Other.Id == Row.Id; }))
		{
			OutError = FString::Printf(TEXT("voice manifest lists %s twice"), *Row.Id);
			return false;
		}
		Read.Add(MoveTemp(Row));
	}
	Rows = MoveTemp(Read);
	return true;
}

const FVoiceManifestRow* FVoiceManifest::Find(const FString& Id) const
{
	return Rows.FindByPredicate([&Id](const FVoiceManifestRow& Row)
	{ return Row.Id == Id; });
}

FString FVoiceManifest::VoiceDir()
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../art/audio/voice"));
	FPaths::CollapseRelativeDirectories(Path);
	return Path;
}

FString FVoiceManifest::DefaultPath()
{
	return FPaths::Combine(VoiceDir(), TEXT("manifest.json"));
}

FString VoiceLines::AssetPath(const FString& Cue)
{
	return FString::Printf(TEXT("/Game/Audio/Voice/%s.%s"), *Cue, *Cue);
}

bool VoiceLines::SpeakerLocationCm(const FString& Speaker, const FVector& EyeCm, const FVector& RivalCm, FVector& OutCm)
{
	if (Speaker == TEXT("septima") || Speaker == TEXT("ophel"))
	{
		// +Y is the seated player's right (arena-layout.json axes).
		OutCm = EyeCm + RailAheadCm + FVector(0.0, Speaker == TEXT("septima") ? -RailSideCm : RailSideCm, 0.0);
		return true;
	}
	if (Speaker == TEXT("venno"))
	{
		OutCm = GrilleCm;
		return true;
	}
	if (Speaker == TEXT("brennic") || Speaker == TEXT("lio"))
	{
		OutCm = RivalCm + FVector(0.0, 0.0, MouthAboveFloorCm);
		return true;
	}
	return false;
}
