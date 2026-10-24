#include "Session/PresentationLook.h"

#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
// The soldiers' colours are the only ones a style file changes (LoadStyleFromCommandLine).
FLinearColor ConscriptColour(0.45f, 0.48f, 0.52f);
FLinearColor SlingerColour(0.55f, 0.28f, 0.05f);
// Opponent mage bodies (T20): dark tones of the school colours, so the body never reads as an absorb cue.
const FLinearColor MageFireBody(0.30f, 0.06f, 0.03f);
const FLinearColor MageWaterBody(0.03f, 0.22f, 0.24f);
// T23. The body is a dark tone of the air wind green, never an absorb cue.
const FLinearColor MageAirBody(0.05f, 0.20f, 0.06f);
// T26: the cinder hound body was the orange its embers now draw in (0.8, 0.3, 0.05). A body must not read as an absorb
// cue (the T20 rule for mage bodies), so the hound is a dark ember tone and its ember stands out at the mouth.
const FLinearColor HoundBody(0.28f, 0.07f, 0.02f);
const FLinearColor MawBody(0.15f, 0.35f, 0.1f);
const FLinearColor TrainingDummyColour(0.86f, 0.78f, 0.62f);
const FLinearColor HpBack(0.02f, 0.02f, 0.02f);
const FLinearColor HpFill(0.65f, 0.08f, 0.05f);
bool bStyleLoaded = false;
}

FBodyLook PresentationLook::Body(const FString& EnemyId)
{
	FBodyLook Look;
	Look.Colour = ConscriptColour;
	if (EnemyId == TEXT("slinger"))
	{
		Look.Colour = SlingerColour;
		Look.SizeCm = FVector(42.0, 42.0, 150.0);
	}
	else if (EnemyId == TEXT("cinder_hound"))
	{
		Look.Shape = TEXT("Cube");
		Look.Colour = HoundBody;
		Look.SizeCm = FVector(120.0, 40.0, 40.0);
	}
	else if (EnemyId == TEXT("mire_maw"))
	{
		Look.Colour = MawBody;
		Look.SizeCm = FVector(150.0, 150.0, 80.0);
	}
	else if (EnemyId == TEXT("mage-fire") || EnemyId == TEXT("mage-water") || EnemyId == TEXT("mage-air"))
	{
		Look.Colour = EnemyId == TEXT("mage-fire") ? MageFireBody : (EnemyId == TEXT("mage-water") ? MageWaterBody : MageAirBody);
		Look.SizeCm = FVector(56.0, 56.0, 180.0);
	}
	return Look;
}

FBodyLook PresentationLook::Dummy()
{
	FBodyLook Look;
	Look.Colour = TrainingDummyColour;
	return Look;
}

FLinearColor PresentationLook::HpBackColour()
{
	return HpBack;
}

FLinearColor PresentationLook::HpFillColour()
{
	return HpFill;
}

void PresentationLook::LoadStyleFromCommandLine()
{
	if (bStyleLoaded)
	{
		return;
	}
	bStyleLoaded = true;

	// The reader targets the 10-04 style schema ("soldiers"); a file without that key leaves the soldiers as they are.
	FString StyleArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("MageArenaStyle="), StyleArg) && StyleArg != TEXT("greybox") && !StyleArg.IsEmpty())
	{
		FString StylePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/styles/style-") + StyleArg + TEXT(".json"));
		FPaths::CollapseRelativeDirectories(StylePath);
		FString StyleJson;
		if (FFileHelper::LoadFileToString(StyleJson, *StylePath))
		{
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StyleJson);
			if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
			{
				auto ReadCol = [&](const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, FLinearColor& Out)
				{
					const TArray<TSharedPtr<FJsonValue>>* Arr;
					if (Obj->TryGetArrayField(Field, Arr) && Arr->Num() >= 3)
					{
						Out = FLinearColor((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber(), Out.A);
					}
				};
				ReadCol(Root, TEXT("soldiers"), ConscriptColour);
				ReadCol(Root, TEXT("soldiers"), SlingerColour);
			}
		}
	}
}
