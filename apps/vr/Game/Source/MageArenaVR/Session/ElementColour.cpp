#include "Session/ElementColour.h"

#include "Dom/JsonObject.h"
#include "Kernel/SimTypes.h"
#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FElementPalette& MutablePalette()
{
	static FElementPalette Palette = FElementPalette::Greybox();
	return Palette;
}

bool ReadColour(const FJsonObject& Object, const TCHAR* Field, FLinearColor& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.TryGetArrayField(Field, Values) || !Values || Values->Num() < 3)
	{
		return false;
	}
	Out = FLinearColor(
		static_cast<float>((*Values)[0]->AsNumber()),
		static_cast<float>((*Values)[1]->AsNumber()),
		static_cast<float>((*Values)[2]->AsNumber()),
		1.f);
	return true;
}
}

FElementPalette FElementPalette::Greybox()
{
	FElementPalette Palette;
	// arena-layout.json threats.order[water]: the turquoise every Water threat and cast has drawn in since A02.
	Palette.Water = FLinearColor(0.009721f, 0.745404f, 0.745404f);
	// T26 ember orange (arena-layout.json threats.order[fire]): linear 0.90, 0.26, 0.02, about sRGB 243, 139, 39. The
	// old vermilion (sRGB about 226, 66, 30) sat next to the unblockable rim red (about 224, 31, 24); an absorb cue must
	// not borrow the leave cue's colour, so fire moved toward orange, away from the rim.
	Palette.Fire = FLinearColor(0.90f, 0.26f, 0.02f);
	// T23 wind green: linear 0.20, 0.85, 0.22, about sRGB 124, 238, 130. Not cyan (Water), not white (steel).
	Palette.Air = FLinearColor(0.20f, 0.85f, 0.22f);
	// Reserved for the Earth school (no Earth caster in the slice): ochre, about sRGB 196, 140, 52.
	Palette.Earth = FLinearColor(0.55f, 0.26f, 0.035f);
	Palette.Steel = FLinearColor(0.68f, 0.68f, 0.68f);
	Palette.UnblockableBody = FLinearColor(0.006995f, 0.006995f, 0.008023f);
	Palette.UnblockableRim = FLinearColor(0.745404f, 0.014444f, 0.009134f);
	return Palette;
}

FLinearColor FElementPalette::ElementColour(EElement Element) const
{
	switch (Element)
	{
	case EElement::Fire:
		return Fire;
	case EElement::Air:
		return Air;
	case EElement::Earth:
		return Earth;
	case EElement::Water:
	default:
		return Water;
	}
}

const FElementPalette& ActiveElementPalette()
{
	return MutablePalette();
}

void SetActiveElementPalette(const FElementPalette& Palette)
{
	MutablePalette() = Palette;
}

void ResetActiveElementPalette()
{
	MutablePalette() = FElementPalette::Greybox();
}

int32 ApplyStylePalette(FElementPalette& Palette, const FJsonObject& Style)
{
	const TSharedPtr<FJsonObject>* Projectiles = nullptr;
	if (!Style.TryGetObjectField(TEXT("projectiles"), Projectiles) || !Projectiles || !Projectiles->IsValid())
	{
		return 0;
	}
	const FJsonObject& Object = **Projectiles;
	int32 Applied = 0;
	Applied += ReadColour(Object, TEXT("magic"), Palette.Water) ? 1 : 0;
	Applied += ReadColour(Object, TEXT("water"), Palette.Water) ? 1 : 0;
	Applied += ReadColour(Object, TEXT("fire"), Palette.Fire) ? 1 : 0;
	Applied += ReadColour(Object, TEXT("air"), Palette.Air) ? 1 : 0;
	Applied += ReadColour(Object, TEXT("earth"), Palette.Earth) ? 1 : 0;
	Applied += ReadColour(Object, TEXT("steel"), Palette.Steel) ? 1 : 0;
	Applied += ReadColour(Object, TEXT("unblockableBody"), Palette.UnblockableBody) ? 1 : 0;
	Applied += ReadColour(Object, TEXT("unblockableRim"), Palette.UnblockableRim) ? 1 : 0;
	return Applied;
}

void LoadStylePaletteFromCommandLine()
{
	static bool bDone = false;
	if (bDone)
	{
		return;
	}
	bDone = true;
	FString StyleId;
	if (!FParse::Value(FCommandLine::Get(), TEXT("MageArenaStyle="), StyleId) || StyleId.IsEmpty() || StyleId == TEXT("greybox"))
	{
		return;
	}
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/styles/style-") + StyleId + TEXT(".json"));
	FPaths::CollapseRelativeDirectories(Path);
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogMageArena, Warning, TEXT("colour style %s: %s missing or not JSON, greybox palette kept"), *StyleId, *Path);
		return;
	}
	FElementPalette Palette = FElementPalette::Greybox();
	const int32 Applied = ApplyStylePalette(Palette, *Root);
	SetActiveElementPalette(Palette);
	UE_LOG(LogMageArena, Log, TEXT("colour style %s: %d colours applied from %s"), *StyleId, Applied, *Path);
}

TOptional<EElement> CreatureElement(const FString& EnemyId)
{
	// enemies.json: cinder_hound onDeath ember_burst is magic, and the DF-004 ember spit copies it; a hound is a creature
	// of embers ("A creature made of embers", cues.json hound-snarl), so its magic is Fire.
	if (EnemyId == TEXT("cinder_hound"))
	{
		return EElement::Fire;
	}
	// enemies.json: mire_maw bog_glob is magic. A bog of water; Mire is also a Water line (spells-water.csv).
	if (EnemyId == TEXT("mire_maw"))
	{
		return EElement::Water;
	}
	return {};
}

TOptional<EElement> ActorElement(const FArenaState& State, int32 ActorId)
{
	const FActor* Actor = SimFindActor(State, ActorId);
	if (!Actor)
	{
		return {};
	}
	if (Actor->Enemy.IsSet())
	{
		return CreatureElement(Actor->Enemy->Id);
	}
	if (Actor->Fire.bSchool)
	{
		return EElement::Fire;
	}
	if (Actor->Air.bSchool)
	{
		return EElement::Air;
	}
	// Not an enemy, not Fire, not Air: the kernel's own spellbook, which is Water (the player, Water mages, dummies).
	return EElement::Water;
}

FThreatLook ThreatLook(const FElementPalette& Palette, EElement Element, const FString& Family)
{
	FThreatLook Look;
	if (Family == TEXT("unblockable"))
	{
		Look.Body = Palette.UnblockableBody;
		Look.Rim = Palette.UnblockableRim;
		Look.bRim = true;
		return Look;
	}
	if (Family == TEXT("physical"))
	{
		Look.Body = Palette.Steel;
		Look.Rim = Palette.Steel;
		return Look;
	}
	Look.Body = Palette.ElementColour(Element);
	Look.Rim = Look.Body;
	return Look;
}

FThreatLook ThreatLookFor(const FElementPalette& Palette, const FArenaState& State, int32 OwnerId, const FString& Family)
{
	const TOptional<EElement> Element = ActorElement(State, OwnerId);
	if (!Element.IsSet() && Family != TEXT("unblockable") && Family != TEXT("physical"))
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogMageArena, Warning, TEXT("colour: magic from owner %d has no school (unknown actor or creature); drawn Water"), OwnerId);
		}
	}
	return ThreatLook(Palette, Element.Get(EElement::Water), Family);
}

FThreatLook ElementLook(const FElementPalette& Palette, const FArenaState& State, int32 PlayerId, int32 OwnerId, const FString& Family)
{
	if (OwnerId == PlayerId)
	{
		FThreatLook Look;
		Look.Body = Palette.ElementColour(ActorElement(State, PlayerId).Get(EElement::Water));
		Look.Rim = Look.Body;
		return Look;
	}
	return ThreatLookFor(Palette, State, OwnerId, Family);
}
