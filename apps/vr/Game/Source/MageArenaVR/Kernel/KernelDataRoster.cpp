#include "Kernel/KernelData.h"
#include "Kernel/KernelDataLoad.h"

#include "Dom/JsonObject.h"

#include <cmath>

namespace KernelDataLoad
{
bool OptionalNumber(const FJsonObject& Object, const TCHAR* Field, TOptional<double>& Out, const FString& Path, FKernelData& Data)
{
	if (!Object.HasField(Field))
	{
		return true;
	}
	double Value = 0.0;
	if (!NeedNumber(Object, Field, Value, Path, Data))
	{
		return false;
	}
	Out = Value;
	return true;
}

bool ParseAttack(const FJsonObject& Object, FAttackSpec& Attack, const FString& Path, FKernelData& Data)
{
	if (!NeedString(Object, TEXT("id"), Attack.Id, Path, Data)
		|| !NeedString(Object, TEXT("family"), Attack.Family, Path, Data)
		|| !NeedNumber(Object, TEXT("damage"), Attack.Damage, Path, Data)
		|| !NeedNumber(Object, TEXT("windupS"), Attack.WindupS, Path, Data))
	{
		return false;
	}
	if (!OptionalNumber(Object, TEXT("recoveryS"), Attack.RecoveryS, Path, Data)
		|| !OptionalNumber(Object, TEXT("cooldownS"), Attack.CooldownS, Path, Data)
		|| !OptionalNumber(Object, TEXT("rangeM"), Attack.RangeM, Path, Data)
		|| !OptionalNumber(Object, TEXT("projectileMps"), Attack.ProjectileMps, Path, Data))
	{
		return false;
	}
	if (Object.HasField(TEXT("tier")))
	{
		int32 Tier = 0;
		if (!NeedInt(Object, TEXT("tier"), Tier, Path, Data))
		{
			return false;
		}
		Attack.Tier = Tier;
	}
	Object.TryGetStringField(TEXT("effect"), Attack.Effect);
	Object.TryGetStringField(TEXT("telegraph"), Attack.Telegraph);
	Object.TryGetStringField(TEXT("onWallHit"), Attack.OnWallHit);
	return true;
}

bool ParseEnemy(const FJsonObject& Object, FEnemySpec& Spec, const FString& Path, FKernelData& Data)
{
	if (!NeedString(Object, TEXT("id"), Spec.Id, Path, Data)
		|| !NeedString(Object, TEXT("name"), Spec.Name, Path, Data)
		|| !NeedNumber(Object, TEXT("hp"), Spec.Hp, Path, Data)
		|| !NeedNumber(Object, TEXT("speedMps"), Spec.SpeedMps, Path, Data)
		|| !NeedString(Object, TEXT("behaviour"), Spec.Behaviour, Path, Data))
	{
		return false;
	}
	if (!OptionalNumber(Object, TEXT("frontBlockDeg"), Spec.FrontBlockDeg, Path, Data)
		|| !OptionalNumber(Object, TEXT("frontBlockMult"), Spec.FrontBlockMult, Path, Data)
		|| !OptionalNumber(Object, TEXT("armourMultVsBolt"), Spec.ArmourMultVsBolt, Path, Data))
	{
		return false;
	}
	if (Object.HasField(TEXT("packSize")))
	{
		int32 Pack = 0;
		if (!NeedInt(Object, TEXT("packSize"), Pack, Path, Data))
		{
			return false;
		}
		Spec.PackSize = Pack;
	}
	const TArray<TSharedPtr<FJsonValue>>* Keep = nullptr;
	if (Object.TryGetArrayField(TEXT("keepDistanceM"), Keep))
	{
		if (Keep->Num() != 2)
		{
			return Fail(Data, Path + TEXT(" keepDistanceM needs two numbers"));
		}
		Spec.KeepMinM = (*Keep)[0]->AsNumber();
		Spec.KeepMaxM = (*Keep)[1]->AsNumber();
	}
	const TArray<TSharedPtr<FJsonValue>>* Attacks = nullptr;
	if (!Object.TryGetArrayField(TEXT("attacks"), Attacks) || Attacks == nullptr || Attacks->Num() == 0)
	{
		return Fail(Data, Path + TEXT(" has no attacks"));
	}
	for (const TSharedPtr<FJsonValue>& Value : *Attacks)
	{
		const TSharedPtr<FJsonObject>* AttackObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(AttackObject) || AttackObject == nullptr)
		{
			return Fail(Data, Path + TEXT(" attack is not an object"));
		}
		// The id is read here because Attack.Id is not filled until ParseAttack runs.
		FString AttackId;
		(*AttackObject)->TryGetStringField(TEXT("id"), AttackId);
		FAttackSpec Attack;
		if (!ParseAttack(**AttackObject, Attack, Path + TEXT(".") + AttackId, Data))
		{
			return false;
		}
		Spec.Attacks.Add(MoveTemp(Attack));
	}
	const TSharedPtr<FJsonObject>* Death = nullptr;
	if (Object.TryGetObjectField(TEXT("onDeath"), Death) && Death && Death->IsValid())
	{
		FDeathSpec SpecDeath;
		if (!NeedString(**Death, TEXT("id"), SpecDeath.Id, Path, Data)
			|| !NeedString(**Death, TEXT("family"), SpecDeath.Family, Path, Data)
			|| !NeedInt(**Death, TEXT("tier"), SpecDeath.Tier, Path, Data)
			|| !NeedNumber(**Death, TEXT("damage"), SpecDeath.Damage, Path, Data)
			|| !NeedNumber(**Death, TEXT("radiusM"), SpecDeath.RadiusM, Path, Data)
			|| !NeedNumber(**Death, TEXT("delayS"), SpecDeath.DelayS, Path, Data))
		{
			return false;
		}
		Spec.OnDeath = SpecDeath;
	}
	return true;
}

bool LoadEnemies(const FJsonObject& Root, FKernelData& Data, const FString& Path)
{
	const TArray<TSharedPtr<FJsonValue>>* Soldiers = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Creatures = nullptr;
	if (!Root.TryGetArrayField(TEXT("soldiers"), Soldiers) || !Root.TryGetArrayField(TEXT("creatures"), Creatures))
	{
		return Fail(Data, Path + TEXT(" is missing soldiers or creatures"));
	}
	auto Consume = [&](const TArray<TSharedPtr<FJsonValue>>& List) -> bool
	{
		for (const TSharedPtr<FJsonValue>& Value : List)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
			{
				return Fail(Data, Path + TEXT(" roster entry is not an object"));
			}
			// The id is read here because Spec.Id is not filled until ParseEnemy runs.
			FString EnemyId;
			(*Object)->TryGetStringField(TEXT("id"), EnemyId);
			FEnemySpec Spec;
			if (!ParseEnemy(**Object, Spec, Path + TEXT("#") + EnemyId, Data))
			{
				return false;
			}
			Data.Enemies.Add(MoveTemp(Spec));
		}
		return true;
	};
	if (!Consume(*Soldiers) || !Consume(*Creatures))
	{
		return false;
	}
	const FJsonObject* Mages = nullptr;
	const FJsonObject* Caps = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!NeedObject(Root, TEXT("mages"), Mages, Path, Data) || !Mages->TryGetArrayField(TEXT("competence"), Rows) || Rows->Num() == 0
		|| !NeedObject(*Mages, TEXT("caps"), Caps, Path, Data)
		|| !NeedNumber(*Caps, TEXT("reactionDelayMinS"), Data.MageCaps.ReactionDelayMinS, Path, Data)
		|| !NeedNumber(*Caps, TEXT("perfectAbsorbChanceMax"), Data.MageCaps.PerfectAbsorbChanceMax, Path, Data))
	{
		if (Data.Error.IsEmpty())
		{
			Fail(Data, Path + TEXT(" is missing mages.competence"));
		}
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Rows)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
		{
			return Fail(Data, Path + TEXT(" competence row is not an object"));
		}
		FMageCompetenceRow Row;
		if (!NeedNumber(**Object, TEXT("level"), Row.Level, Path, Data)
			|| !NeedNumber(**Object, TEXT("reactionDelayS"), Row.ReactionDelayS, Path, Data)
			|| !NeedNumber(**Object, TEXT("absorbChanceVsAbsorbable"), Row.AbsorbChance, Path, Data)
			|| !NeedNumber(**Object, TEXT("perfectAbsorbChance"), Row.PerfectChance, Path, Data)
			|| !NeedNumber(**Object, TEXT("aimErrorDeg"), Row.AimErrorDeg, Path, Data)
			|| !NeedNumber(**Object, TEXT("decisionCadenceS"), Row.DecisionCadenceS, Path, Data))
		{
			return false;
		}
		Data.MageCompetence.Add(Row);
	}
	return true;
}

bool ParseWaveSpawn(const FJsonObject& SpawnObject, FWaveSpawn& Spawn, const FString& Path, FKernelData& Data)
{
	double Count = 0.0;
	if (SpawnObject.TryGetNumberField(TEXT("count"), Count))
	{
		Spawn.Count = static_cast<int32>(std::llround(Count));
	}
	double Echo = 0.0;
	if (SpawnObject.TryGetNumberField(TEXT("orEchoFromLoop"), Echo))
	{
		Spawn.EchoFromLoop = static_cast<int32>(std::llround(Echo));
	}
	if (SpawnObject.TryGetStringField(TEXT("enemy"), Spawn.EnemyId))
	{
		Spawn.bEnemy = true;
	}
	else if (SpawnObject.TryGetStringField(TEXT("mage"), Spawn.MageId))
	{
		Spawn.bEnemy = false;
		if (!NeedNumber(SpawnObject, TEXT("competence"), Spawn.Competence, Path, Data))
		{
			return false;
		}
	}
	else
	{
		return Fail(Data, Path + TEXT(" spawn is neither an enemy nor a mage"));
	}
	return true;
}

bool ParseArenaWave(const FJsonObject& WaveObject, FArenaWave& Wave, const FString& Path, FKernelData& Data)
{
	const TArray<TSharedPtr<FJsonValue>>* Spawns = nullptr;
	if (!NeedInt(WaveObject, TEXT("n"), Wave.N, Path, Data) || !NeedString(WaveObject, TEXT("kind"), Wave.Kind, Path, Data)
		|| !WaveObject.TryGetArrayField(TEXT("spawns"), Spawns) || Spawns->Num() == 0)
	{
		if (Data.Error.IsEmpty())
		{
			Fail(Data, Path + TEXT(" wave is missing spawns"));
		}
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Band = nullptr;
	if (WaveObject.TryGetArrayField(TEXT("targetDurationS"), Band) && Band && Band->Num() == 2)
	{
		double Min = 0.0;
		double Max = 0.0;
		if (!(*Band)[0].IsValid() || !(*Band)[0]->TryGetNumber(Min) || !(*Band)[1].IsValid() || !(*Band)[1]->TryGetNumber(Max))
		{
			return Fail(Data, Path + TEXT(" targetDurationS is not a pair of numbers"));
		}
		Wave.TargetMinS = Min;
		Wave.TargetMaxS = Max;
	}
	for (const TSharedPtr<FJsonValue>& SpawnValue : *Spawns)
	{
		const TSharedPtr<FJsonObject>* SpawnObject = nullptr;
		if (!SpawnValue.IsValid() || !SpawnValue->TryGetObject(SpawnObject) || SpawnObject == nullptr)
		{
			return Fail(Data, Path + TEXT(" spawn is not an object"));
		}
		FWaveSpawn Spawn;
		if (!ParseWaveSpawn(**SpawnObject, Spawn, Path, Data))
		{
			return false;
		}
		Wave.Spawns.Add(Spawn);
	}
	return true;
}

// payoutGold and renown: lists of whole numbers.
bool ReadIntList(const TArray<TSharedPtr<FJsonValue>>& Values, const TCHAR* Field, TArray<int32>& Out, const FString& Path, FKernelData& Data)
{
	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		double Number = 0.0;
		if (!Value.IsValid() || !Value->TryGetNumber(Number))
		{
			return Fail(Data, Path + TEXT(" ") + Field + TEXT(" entry is not a number"));
		}
		Out.Add(static_cast<int32>(std::llround(Number)));
	}
	return true;
}

bool ParseArenaTier(const FJsonObject& TierObject, FArenaTier& Tier, const FString& Path, FKernelData& Data)
{
	const TArray<TSharedPtr<FJsonValue>>* Waves = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Gold = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Renown = nullptr;
	if (!NeedString(TierObject, TEXT("id"), Tier.Id, Path, Data)
		|| !NeedString(TierObject, TEXT("name"), Tier.Name, Path, Data)
		|| !NeedInt(TierObject, TEXT("requiresMastery"), Tier.RequiresMastery, Path, Data)
		|| !TierObject.TryGetArrayField(TEXT("waves"), Waves) || Waves->Num() == 0
		|| !TierObject.TryGetArrayField(TEXT("payoutGold"), Gold)
		|| !TierObject.TryGetArrayField(TEXT("renown"), Renown))
	{
		if (Data.Error.IsEmpty())
		{
			Fail(Data, Path + TEXT(" tier is missing waves or payouts"));
		}
		return false;
	}
	for (const TSharedPtr<FJsonValue>& WaveValue : *Waves)
	{
		const TSharedPtr<FJsonObject>* WaveObject = nullptr;
		if (!WaveValue.IsValid() || !WaveValue->TryGetObject(WaveObject) || WaveObject == nullptr)
		{
			return Fail(Data, Path + TEXT(" wave is not an object"));
		}
		FArenaWave Wave;
		if (!ParseArenaWave(**WaveObject, Wave, Path, Data))
		{
			return false;
		}
		Tier.Waves.Add(MoveTemp(Wave));
	}
	return ReadIntList(*Gold, TEXT("payoutGold"), Tier.PayoutGold, Path, Data)
		&& ReadIntList(*Renown, TEXT("renown"), Tier.Renown, Path, Data);
}

bool LoadArenaTiers(FKernelData& Data)
{
	const FString Path = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/arena-tiers.json"));
	TSharedPtr<FJsonObject> Root;
	if (!LoadJson(Path, Root, Data))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Tiers = nullptr;
	if (!Root->TryGetArrayField(TEXT("tiers"), Tiers) || Tiers->Num() == 0)
	{
		return Fail(Data, Path + TEXT(" is missing tiers"));
	}
	for (const TSharedPtr<FJsonValue>& TierValue : *Tiers)
	{
		const TSharedPtr<FJsonObject>* TierObject = nullptr;
		if (!TierValue.IsValid() || !TierValue->TryGetObject(TierObject) || TierObject == nullptr)
		{
			return Fail(Data, Path + TEXT(" tier is not an object"));
		}
		FArenaTier Tier;
		if (!ParseArenaTier(**TierObject, Tier, Path, Data))
		{
			return false;
		}
		Data.ArenaTiers.Add(MoveTemp(Tier));
	}
	return true;
}
}
