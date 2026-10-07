#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Kernel/KernelData.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
const TCHAR* const EnemiesFile = TEXT("docs/design/baseline-fourteen-nights/design/data/enemies.json");

// Reads a pinned JSON file, lets Mutate edit the parsed object, and returns the edited text.
bool MutatedJson(const TCHAR* Relative, TFunctionRef<bool(FJsonObject&)> Mutate, FString& OutText)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *KernelDataPinnedPath(Relative)))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Mutate(*Root))
	{
		return false;
	}
	OutText.Reset();
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutText);
	return FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
}

FKernelData LoadWith(const TCHAR* Relative, const FString& Text)
{
	TMap<FString, FString> Overrides;
	Overrides.Add(KernelDataPinnedPath(Relative), Text);
	return LoadKernelDataWithOverrides(Overrides);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelLoadSeam, "MageArena.Kernel.LoadSeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelLoadSeam::RunTest(const FString& Parameters)
{
	// With no override the seam reads the same files as the process-wide load.
	const TMap<FString, FString> None;
	const FKernelData Plain = LoadKernelDataWithOverrides(None);
	if (!TestTrue(*FString::Printf(TEXT("loads with no override (%s)"), *Plain.Error), Plain.bReady))
	{
		return false;
	}
	TestEqual(TEXT("same water spell count as KernelData()"), Plain.Spells.Num(), KernelData().Spells.Num());
	TestEqual(TEXT("same enemy count as KernelData()"), Plain.Enemies.Num(), KernelData().Enemies.Num());
	TestEqual(TEXT("same fire spell count as KernelData()"), Plain.FireSpells.Num(), KernelData().FireSpells.Num());

	// Control: an override that is not JSON must fail the load, so a green run above is not the seam doing nothing.
	const FKernelData Bad = LoadWith(EnemiesFile, TEXT("not json"));
	TestFalse(TEXT("an override that is not JSON fails the load"), Bad.bReady);
	TestTrue(*FString::Printf(TEXT("the error names the overridden file (%s)"), *Bad.Error), Bad.Error.Contains(TEXT("enemies.json")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelLoadNamesEnemy, "MageArena.Kernel.LoadNamesEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelLoadNamesEnemy::RunTest(const FString& Parameters)
{
	// A roster entry that is missing a required field must be named in the load error, so a data bump points at it.
	FString EnemyId;
	FString AttackId;
	FString NoHp;
	const bool bEnemyEdit = MutatedJson(EnemiesFile, [&](FJsonObject& Root)
	{
		const TArray<TSharedPtr<FJsonValue>>* Soldiers = nullptr;
		if (!Root.TryGetArrayField(TEXT("soldiers"), Soldiers) || Soldiers->Num() == 0)
		{
			return false;
		}
		const TSharedPtr<FJsonObject> First = (*Soldiers)[0]->AsObject();
		First->TryGetStringField(TEXT("id"), EnemyId);
		First->RemoveField(TEXT("hp"));
		return !EnemyId.IsEmpty();
	}, NoHp);
	if (!TestTrue(TEXT("built the enemy without hp"), bEnemyEdit))
	{
		return false;
	}
	const FKernelData MissingHp = LoadWith(EnemiesFile, NoHp);
	TestFalse(TEXT("an enemy without hp fails the load"), MissingHp.bReady);
	TestTrue(*FString::Printf(TEXT("the error names enemy '%s' (%s)"), *EnemyId, *MissingHp.Error), MissingHp.Error.Contains(EnemyId));

	FString NoDamage;
	const bool bAttackEdit = MutatedJson(EnemiesFile, [&](FJsonObject& Root)
	{
		const TArray<TSharedPtr<FJsonValue>>* Soldiers = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Attacks = nullptr;
		if (!Root.TryGetArrayField(TEXT("soldiers"), Soldiers) || Soldiers->Num() == 0)
		{
			return false;
		}
		const TSharedPtr<FJsonObject> First = (*Soldiers)[0]->AsObject();
		if (!First->TryGetArrayField(TEXT("attacks"), Attacks) || Attacks->Num() == 0)
		{
			return false;
		}
		const TSharedPtr<FJsonObject> Attack = (*Attacks)[0]->AsObject();
		Attack->TryGetStringField(TEXT("id"), AttackId);
		Attack->RemoveField(TEXT("damage"));
		return !AttackId.IsEmpty();
	}, NoDamage);
	if (!TestTrue(TEXT("built the attack without damage"), bAttackEdit))
	{
		return false;
	}
	const FKernelData MissingDamage = LoadWith(EnemiesFile, NoDamage);
	TestFalse(TEXT("an attack without damage fails the load"), MissingDamage.bReady);
	TestTrue(*FString::Printf(TEXT("the error names attack '%s' (%s)"), *AttackId, *MissingDamage.Error), MissingDamage.Error.Contains(AttackId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelLoadRejectsUnknownReference, "MageArena.Kernel.LoadRejectsUnknownReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelLoadRejectsUnknownReference::RunTest(const FString& Parameters)
{
	// A preset or enemy name that nothing loaded must fail the load, named. Otherwise it first shows up as a checkf abort
	// when a bout spawns (Games.cpp SpawnWave and RunFight, Enemies.cpp AddEnemy).
	const TCHAR* RuntimeFile = TEXT("packages/core/src/arena/data/runtime.json");
	const TCHAR* TiersFile = TEXT("docs/design/baseline-fourteen-nights/design/data/arena-tiers.json");

	FString BadReference;
	const bool bReference = MutatedJson(RuntimeFile, [](FJsonObject& Root)
	{
		const TSharedPtr<FJsonObject>* Games = nullptr;
		if (!Root.TryGetObjectField(TEXT("games"), Games))
		{
			return false;
		}
		(*Games)->SetStringField(TEXT("referencePreset"), TEXT("no_such_reference_preset"));
		return true;
	}, BadReference);
	FString BadOpponent;
	const bool bOpponent = MutatedJson(RuntimeFile, [](FJsonObject& Root)
	{
		const TSharedPtr<FJsonObject>* Games = nullptr;
		if (!Root.TryGetObjectField(TEXT("games"), Games))
		{
			return false;
		}
		TArray<TSharedPtr<FJsonValue>> Names = (*Games)->GetArrayField(TEXT("opponentPresets"));
		if (Names.Num() == 0)
		{
			return false;
		}
		Names[0] = MakeShared<FJsonValueString>(TEXT("no_such_opponent_preset"));
		(*Games)->SetArrayField(TEXT("opponentPresets"), Names);
		return true;
	}, BadOpponent);
	FString BadEnemy;
	const bool bEnemy = MutatedJson(TiersFile, [](FJsonObject& Root)
	{
		const TArray<TSharedPtr<FJsonValue>>* Tiers = nullptr;
		if (!Root.TryGetArrayField(TEXT("tiers"), Tiers))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Tier : *Tiers)
		{
			const TArray<TSharedPtr<FJsonValue>>* Waves = nullptr;
			if (!Tier->AsObject()->TryGetArrayField(TEXT("waves"), Waves))
			{
				continue;
			}
			for (const TSharedPtr<FJsonValue>& Wave : *Waves)
			{
				const TArray<TSharedPtr<FJsonValue>>* Spawns = nullptr;
				if (!Wave->AsObject()->TryGetArrayField(TEXT("spawns"), Spawns))
				{
					continue;
				}
				for (const TSharedPtr<FJsonValue>& Spawn : *Spawns)
				{
					FString Existing;
					if (Spawn->AsObject()->TryGetStringField(TEXT("enemy"), Existing))
					{
						Spawn->AsObject()->SetStringField(TEXT("enemy"), TEXT("no_such_enemy"));
						return true;
					}
				}
			}
		}
		return false;
	}, BadEnemy);
	if (!TestTrue(TEXT("built the three broken inputs"), bReference && bOpponent && bEnemy))
	{
		return false;
	}

	const FKernelData Reference = LoadWith(RuntimeFile, BadReference);
	TestFalse(TEXT("an unknown reference preset fails the load"), Reference.bReady);
	TestTrue(*FString::Printf(TEXT("the error names the reference preset (%s)"), *Reference.Error), Reference.Error.Contains(TEXT("no_such_reference_preset")));

	const FKernelData Opponent = LoadWith(RuntimeFile, BadOpponent);
	TestFalse(TEXT("an unknown opponent preset fails the load"), Opponent.bReady);
	TestTrue(*FString::Printf(TEXT("the error names the opponent preset (%s)"), *Opponent.Error), Opponent.Error.Contains(TEXT("no_such_opponent_preset")));

	const FKernelData Enemy = LoadWith(TiersFile, BadEnemy);
	TestFalse(TEXT("an unknown wave enemy fails the load"), Enemy.bReady);
	TestTrue(*FString::Printf(TEXT("the error names the enemy (%s)"), *Enemy.Error), Enemy.Error.Contains(TEXT("no_such_enemy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelLoadNamesColumn, "MageArena.Kernel.LoadNamesColumn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelLoadNamesColumn::RunTest(const FString& Parameters)
{
	// A data bump that renames a spells-water.csv column must fail the load with the column named. It used to
	// dereference a null cell and take the editor down.
	const TCHAR* SpellsFile = TEXT("docs/design/baseline-fourteen-nights/design/data/spells-water.csv");
	FString Csv;
	if (!TestTrue(TEXT("read spells-water.csv"), FFileHelper::LoadFileToString(Csv, *KernelDataPinnedPath(SpellsFile))))
	{
		return false;
	}
	const FString Renamed = Csv.Replace(TEXT(",mana,"), TEXT(",mana_renamed,"), ESearchCase::CaseSensitive);
	if (!TestTrue(TEXT("the header had a mana column to rename"), Renamed != Csv))
	{
		return false;
	}
	const FKernelData Loaded = LoadWith(SpellsFile, Renamed);
	TestFalse(TEXT("a spells file without a mana column fails the load"), Loaded.bReady);
	TestTrue(*FString::Printf(TEXT("the error names the mana column (%s)"), *Loaded.Error), Loaded.Error.Contains(TEXT("mana")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelPinHash, "MageArena.Kernel.PinHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelPinHash::RunTest(const FString& Parameters)
{
	const TMap<FString, FString> None;
	const FKernelData First = LoadKernelDataWithOverrides(None);
	if (!TestTrue(*FString::Printf(TEXT("loads with no override (%s)"), *First.Error), First.bReady))
	{
		return false;
	}
	TestTrue(TEXT("at least one pinned file was read"), First.PinFileCount > 0);
	TestFalse(TEXT("the pin hash is not empty"), First.PinHash.IsEmpty());
	const FKernelData Second = LoadKernelDataWithOverrides(None);
	TestEqual(TEXT("a second load reads the same number of files"), Second.PinFileCount, First.PinFileCount);
	TestEqual(TEXT("a second load gives the same hash"), Second.PinHash, First.PinHash);
	return true;
}

#endif
