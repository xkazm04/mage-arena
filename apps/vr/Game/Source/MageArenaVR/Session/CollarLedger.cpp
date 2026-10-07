#include "Session/CollarLedger.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <cmath>

namespace
{
constexpr double MaxWeight = 100.0;

bool ReadWeight(const TSharedPtr<FJsonObject>& Section, const TCHAR* SectionName, const TCHAR* Key, int32& Out, FString& Error)
{
	double Value = 0.0;
	if (!Section->TryGetNumberField(Key, Value))
	{
		Error = FString::Printf(TEXT("collar.json %s.%s must be a number"), SectionName, Key);
		return false;
	}
	if (!(Value >= 0.0 && Value <= MaxWeight) || std::floor(Value) != Value)
	{
		Error = FString::Printf(TEXT("collar.json %s.%s must be a whole number in [0, %.0f]"), SectionName, Key, MaxWeight);
		return false;
	}
	Out = static_cast<int32>(Value);
	return true;
}

// Every key is either a known number, a reserved number (accepted, ignored), or a "_" note. Every known number needs
// its _<key> note.
bool CheckKeys(const TSharedPtr<FJsonObject>& Section, const TCHAR* SectionName, const TArray<FString>& Known,
	const TArray<FString>& Reserved, FString& Error)
{
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Section->Values)
	{
		if (Pair.Key.StartsWith(TEXT("_")))
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String || Pair.Value->AsString().TrimStartAndEnd().IsEmpty())
			{
				Error = FString::Printf(TEXT("collar.json %s.%s must be a non-empty string"), SectionName, *Pair.Key);
				return false;
			}
			continue;
		}
		if (Reserved.Contains(Pair.Key))
		{
			double Ignored = 0.0;
			if (!Section->TryGetNumberField(Pair.Key, Ignored) || !(Ignored >= 0.0))
			{
				Error = FString::Printf(TEXT("collar.json %s.%s is reserved and must be a non-negative number if present"), SectionName, *Pair.Key);
				return false;
			}
			continue;
		}
		if (!Known.Contains(Pair.Key))
		{
			Error = FString::Printf(TEXT("collar.json %s has an unknown key %s"), SectionName, *Pair.Key);
			return false;
		}
	}
	for (const FString& Key : Known)
	{
		FString Why;
		if (!Section->TryGetStringField(TEXT("_") + Key, Why) || Why.TrimStartAndEnd().IsEmpty())
		{
			Error = FString::Printf(TEXT("collar.json %s.%s has no _%s line"), SectionName, *Key, *Key);
			return false;
		}
	}
	return true;
}
}

FString CollarWeightsPath()
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/collar.json"));
	FPaths::CollapseRelativeDirectories(Path);
	return Path;
}

bool ParseCollarWeights(const FString& Text, FCollarWeights& Out, FString& Error)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		Error = TEXT("collar.json is not JSON");
		return false;
	}
	double Version = 0.0;
	if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1.0)
	{
		Error = TEXT("collar.json version must be 1");
		return false;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		if (!Pair.Key.StartsWith(TEXT("_")) && Pair.Key != TEXT("version") && Pair.Key != TEXT("cracks") && Pair.Key != TEXT("tithe")
			&& Pair.Key != TEXT("wardstones"))
		{
			Error = FString::Printf(TEXT("collar.json has an unknown key %s"), *Pair.Key);
			return false;
		}
	}
	const TSharedPtr<FJsonObject>* Cracks = nullptr;
	const TSharedPtr<FJsonObject>* Tithe = nullptr;
	const TSharedPtr<FJsonObject>* Stones = nullptr;
	if (!Root->TryGetObjectField(TEXT("cracks"), Cracks) || !Cracks || !Cracks->IsValid()
		|| !Root->TryGetObjectField(TEXT("tithe"), Tithe) || !Tithe || !Tithe->IsValid()
		|| !Root->TryGetObjectField(TEXT("wardstones"), Stones) || !Stones || !Stones->IsValid())
	{
		Error = TEXT("collar.json needs the objects cracks, tithe and wardstones");
		return false;
	}
	// spare (T25) and interrupt (T29) have no source yet: accepted, ignored.
	const TArray<FString> Reserved = {TEXT("spare"), TEXT("interrupt")};
	if (!CheckKeys(*Cracks, TEXT("cracks"), {TEXT("perfect"), TEXT("reflect"), TEXT("reflectHitsCaster"), TEXT("unblockableBlinked")}, Reserved, Error)
		|| !CheckKeys(*Tithe, TEXT("tithe"), {TEXT("areaCastTier3Plus"), TEXT("missedCast"), TEXT("crest")}, Reserved, Error)
		|| !CheckKeys(*Stones, TEXT("wardstones"), {TEXT("fullGlowTithe")}, {}, Error))
	{
		return false;
	}
	FCollarWeights Next;
	if (!ReadWeight(*Cracks, TEXT("cracks"), TEXT("perfect"), Next.Perfect, Error)
		|| !ReadWeight(*Cracks, TEXT("cracks"), TEXT("reflect"), Next.Reflect, Error)
		|| !ReadWeight(*Cracks, TEXT("cracks"), TEXT("reflectHitsCaster"), Next.ReflectHitsCaster, Error)
		|| !ReadWeight(*Cracks, TEXT("cracks"), TEXT("unblockableBlinked"), Next.UnblockableBlinked, Error)
		|| !ReadWeight(*Tithe, TEXT("tithe"), TEXT("areaCastTier3Plus"), Next.AreaCastTier3Plus, Error)
		|| !ReadWeight(*Tithe, TEXT("tithe"), TEXT("missedCast"), Next.MissedCast, Error)
		|| !ReadWeight(*Tithe, TEXT("tithe"), TEXT("crest"), Next.Crest, Error))
	{
		return false;
	}
	double Full = 0.0;
	if (!(*Stones)->TryGetNumberField(TEXT("fullGlowTithe"), Full) || !(Full > 0.0 && Full <= 1000.0))
	{
		Error = TEXT("collar.json wardstones.fullGlowTithe must be a number in (0, 1000]");
		return false;
	}
	Next.WardstoneFullTithe = Full;
	Out = Next;
	return true;
}

bool LoadCollarWeights(FCollarWeights& Out, FString& Error)
{
	const FString Path = CollarWeightsPath();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		Error = FString::Printf(TEXT("collar.json missing: %s"), *Path);
		return false;
	}
	return ParseCollarWeights(Text, Out, Error);
}

bool FCollarLedger::IsAreaCastTier3Plus(const FSpell& Spell)
{
	if (Spell.Tier < 3)
	{
		return false;
	}
	if (Spell.Kind == TEXT("projectile"))
	{
		return Spell.Count > 1 || Spell.BurstRadiusM > 0.0;
	}
	if (Spell.Kind == TEXT("cone") || Spell.Kind == TEXT("ring") || Spell.Kind == TEXT("wave"))
	{
		return true;
	}
	// A zone is an area when it can damage (Freeze Field). Puddle and Fog Bank are utility and spill nothing.
	return Spell.Kind == TEXT("zone") && Spell.Damage > 0.0;
}

bool FCollarLedger::IsCrack(ECollarSource Source)
{
	return Source == ECollarSource::Perfect || Source == ECollarSource::Reflect || Source == ECollarSource::ReflectHitsCaster
		|| Source == ECollarSource::UnblockableBlinked;
}

const TCHAR* FCollarLedger::SourceName(ECollarSource Source)
{
	switch (Source)
	{
	case ECollarSource::Perfect: return TEXT("perfect");
	case ECollarSource::Reflect: return TEXT("reflect");
	case ECollarSource::ReflectHitsCaster: return TEXT("reflectHitsCaster");
	case ECollarSource::UnblockableBlinked: return TEXT("unblockableBlinked");
	case ECollarSource::AreaCastTier3Plus: return TEXT("areaCastTier3Plus");
	case ECollarSource::MissedCast: return TEXT("missedCast");
	case ECollarSource::Crest: return TEXT("crest");
	default: return TEXT("?");
	}
}

void FCollarLedger::BeginBout(int32 CrestsNow)
{
	for (int32& Count : BoutCounts)
	{
		Count = 0;
	}
	BoutCracks = 0;
	BoutTithe = 0;
	LastCrests = CrestsNow;
	LastCrackSim = -1.0;
	LastTitheSim = -1.0;
}

void FCollarLedger::Add(ECollarSource Source, double SimSeconds)
{
	int32 Weight = 0;
	switch (Source)
	{
	case ECollarSource::Perfect: Weight = Weights.Perfect; break;
	case ECollarSource::Reflect: Weight = Weights.Reflect; break;
	case ECollarSource::ReflectHitsCaster: Weight = Weights.ReflectHitsCaster; break;
	case ECollarSource::UnblockableBlinked: Weight = Weights.UnblockableBlinked; break;
	case ECollarSource::AreaCastTier3Plus: Weight = Weights.AreaCastTier3Plus; break;
	case ECollarSource::MissedCast: Weight = Weights.MissedCast; break;
	case ECollarSource::Crest: Weight = Weights.Crest; break;
	default: return;
	}
	++BoutCounts[static_cast<int32>(Source)];
	if (Weight <= 0)
	{
		return;
	}
	if (IsCrack(Source))
	{
		BoutCracks += Weight;
		LastCrackSim = SimSeconds;
	}
	else
	{
		BoutTithe += Weight;
		LastTitheSim = SimSeconds;
	}
}

void FCollarLedger::Observe(const FArenaEvent& Event, int32 PlayerId, const FSpell* CastSpell, double SimSeconds)
{
	if (Event.ActorId != PlayerId)
	{
		return;
	}
	if (Event.Kind == TEXT("perfect"))
	{
		Add(ECollarSource::Perfect, SimSeconds);
	}
	else if (Event.Kind == TEXT("reflect"))
	{
		Add(ECollarSource::Reflect, SimSeconds);
	}
	else if (Event.Kind == TEXT("reflectHit"))
	{
		Add(ECollarSource::ReflectHitsCaster, SimSeconds);
	}
	else if (Event.Kind == TEXT("dodge"))
	{
		Add(ECollarSource::UnblockableBlinked, SimSeconds);
	}
	else if (Event.Kind == TEXT("miss"))
	{
		Add(ECollarSource::MissedCast, SimSeconds);
	}
	else if (Event.Kind == TEXT("cast") && CastSpell && IsAreaCastTier3Plus(*CastSpell))
	{
		Add(ECollarSource::AreaCastTier3Plus, SimSeconds);
	}
}

void FCollarLedger::ObserveCrests(int32 Crests, double SimSeconds)
{
	// The counter restarts at 0 when a chained wave resets the Water state; only a rise is a Crest spent.
	for (int32 Spent = LastCrests; Spent < Crests; ++Spent)
	{
		Add(ECollarSource::Crest, SimSeconds);
	}
	LastCrests = Crests;
}

FString FCollarLedger::Breakdown() const
{
	TArray<FString> Parts;
	for (int32 Index = 0; Index < static_cast<int32>(ECollarSource::Count); ++Index)
	{
		Parts.Add(FString::Printf(TEXT("%s=%d"), SourceName(static_cast<ECollarSource>(Index)), BoutCounts[Index]));
	}
	return FString::Join(Parts, TEXT(" "));
}

void FCollarLedger::CommitBout()
{
	LifetimeCracks += BoutCracks;
	LifetimeTithe += BoutTithe;
}

void FCollarLedger::SetLifetime(int32 Cracks, int32 Tithe)
{
	LifetimeCracks = Cracks;
	LifetimeTithe = Tithe;
}

bool FCollarLedger::SaveLifetime(const FString& Path) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetNumberField(TEXT("lifetimeCracks"), LifetimeCracks);
	Root->SetNumberField(TEXT("lifetimeTithe"), LifetimeTithe);
	FString Text;
	if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text)))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveStringToFile(Text, *Path);
}

bool FCollarLedger::LoadLifetime(const FString& Path)
{
	LifetimeCracks = 0;
	LifetimeTithe = 0;
	if (!IFileManager::Get().FileExists(*Path))
	{
		return true;
	}
	FString Text;
	TSharedPtr<FJsonObject> Root;
	double Version = 0.0;
	double Cracks = 0.0;
	double Tithe = 0.0;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root)
		|| !Root.IsValid() || !Root->TryGetNumberField(TEXT("version"), Version) || Version != 1.0
		|| !Root->TryGetNumberField(TEXT("lifetimeCracks"), Cracks) || !Root->TryGetNumberField(TEXT("lifetimeTithe"), Tithe)
		|| !(Cracks >= 0.0 && Cracks <= 1.0e9) || !(Tithe >= 0.0 && Tithe <= 1.0e9) || std::floor(Cracks) != Cracks
		|| std::floor(Tithe) != Tithe)
	{
		UE_LOG(LogMageArena, Warning, TEXT("Collar ledger is corrupt (%s); the collar starts at 0 Cracks, 0 Tithe"), *Path);
		return false;
	}
	LifetimeCracks = static_cast<int32>(Cracks);
	LifetimeTithe = static_cast<int32>(Tithe);
	return true;
}
