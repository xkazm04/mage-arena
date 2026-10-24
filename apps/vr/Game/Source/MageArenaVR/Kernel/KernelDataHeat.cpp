#include "Kernel/KernelData.h"
#include "Kernel/KernelDataLoad.h"

#include "Dom/JsonObject.h"

#include <cmath>

namespace KernelDataLoad
{
bool SameNumber(double Left, double Right)
{
	return std::abs(Left - Right) <= 1.0e-9;
}

bool RequireReach(const FFireSpell& Spell, double Reach, FKernelData& Data)
{
	if (SameNumber(Reach, Spell.RangeM))
	{
		return true;
	}
	return Fail(Data, FString::Printf(TEXT("kernel data: %s shape reach disagrees with range_m"), *Spell.Id));
}

void PublishFireCatalog(const FFireSpell& Spell, FKernelData& Data)
{
	FSpell View;
	View.Id = Spell.Id;
	View.Line = Spell.Id;
	View.Tier = Spell.Tier;
	View.Name = Spell.Name;
	View.CastS = Spell.CastS;
	View.CooldownS = Spell.CooldownS;
	View.Mana = Spell.Mana;
	View.Damage = Spell.Damage;
	View.Family = Spell.Family;
	View.TelegraphS = Spell.TelegraphS;
	View.RangeM = Spell.RangeM;
	View.Kind = Spell.Kind == TEXT("meteor") ? TEXT("zone") : Spell.Kind;
	View.SpeedMps = Spell.SpeedMps;
	View.RadiusM = Spell.RadiusM;
	View.ArcDeg = Spell.ArcDeg;
	View.DurationS = Spell.DurationS;
	View.Effect = Spell.Kind;
	Data.FireCatalog.Add(MoveTemp(View));
}

bool ParseFireSpell(FFireSpell& Spell, FKernelData& Data)
{
	const FString& Shape = Spell.Shape;
	const FString& Heat = Spell.HeatGain;
	const FString& Notes = Spell.Notes;
	if (Shape.StartsWith(TEXT("projectile")))
	{
		Spell.Kind = TEXT("projectile");
		if (!MatchDouble(Shape, TEXT("([\\d.]+) m/s"), Spell.SpeedMps, Spell.Id, Data))
		{
			return false;
		}
	}
	else if (Shape.StartsWith(TEXT("cone")))
	{
		Spell.Kind = TEXT("cone");
		double Reach = 0.0;
		if (!MatchDouble(Shape, TEXT("([\\d.]+) deg"), Spell.ArcDeg, Spell.Id, Data)
			|| !MatchDouble(Shape, TEXT("x ([\\d.]+) m"), Reach, Spell.Id, Data)
			|| !RequireReach(Spell, Reach, Data))
		{
			return false;
		}
	}
	else if (Shape.StartsWith(TEXT("dash")))
	{
		Spell.Kind = TEXT("dash");
		if (!MatchDouble(Shape, TEXT("dash ([\\d.]+) m"), Spell.DashM, Spell.Id, Data)
			|| !MatchDouble(Shape, TEXT("trail ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data)
			|| !RequireReach(Spell, Spell.DashM, Data))
		{
			return false;
		}
	}
	else if (Shape == TEXT("self"))
	{
		Spell.Kind = TEXT("self");
	}
	else if (Shape.StartsWith(TEXT("line")))
	{
		Spell.Kind = TEXT("line");
		double Reach = 0.0;
		if (!MatchDouble(Shape, TEXT("line ([\\d.]+) m"), Reach, Spell.Id, Data) || !RequireReach(Spell, Reach, Data))
		{
			return false;
		}
	}
	else if (Shape.StartsWith(TEXT("ring")))
	{
		Spell.Kind = TEXT("ring");
		if (!MatchDouble(Shape, TEXT("r ([\\d.]+) m"), Spell.RadiusM, Spell.Id, Data)
			|| !MatchDouble(Shape, TEXT("knockback ([\\d.]+) m"), Spell.KnockbackM, Spell.Id, Data)
			|| !RequireReach(Spell, Spell.RadiusM, Data))
		{
			return false;
		}
		Spell.ArcDeg = 360.0;
	}
	else if (Shape.StartsWith(TEXT("ground circle")))
	{
		Spell.Kind = TEXT("zone");
		if (!MatchDouble(Shape, TEXT("r ([\\d.]+) m"), Spell.RadiusM, Spell.Id, Data))
		{
			return false;
		}
	}
	else if (Shape.StartsWith(TEXT("meteor circle")))
	{
		Spell.Kind = TEXT("meteor");
		if (!MatchDouble(Shape, TEXT("r ([\\d.]+) m"), Spell.RadiusM, Spell.Id, Data))
		{
			return false;
		}
	}
	else if (Shape.StartsWith(TEXT("channelled beam")))
	{
		Spell.Kind = TEXT("beam");
		double Reach = 0.0;
		if (!MatchDouble(Shape, TEXT("beam ([\\d.]+) m"), Reach, Spell.Id, Data)
			|| !MatchDouble(Shape, TEXT("for ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data)
			|| !RequireReach(Spell, Reach, Data))
		{
			return false;
		}
	}
	else
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: fire shape not understood: %s"), *Shape));
	}

	if (Spell.DamageText.Contains(TEXT("tick")))
	{
		if (!MatchDouble(Spell.DamageText, TEXT("per ([\\d.]+) s tick"), Spell.TickS, Spell.Id, Data))
		{
			return false;
		}
	}
	if ((Spell.Kind == TEXT("dash") || Spell.Kind == TEXT("beam")) && !(Spell.TickS > 0.0))
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: %s has no tick interval"), *Spell.Id));
	}

	if (Heat.Contains(TEXT("locked")))
	{
		Spell.HeatMode = EFireHeatMode::Lock;
		if (!MatchDouble(Heat, TEXT("locked at ([\\d.]+)"), Spell.HeatLockValue, Spell.Id, Data)
			|| !MatchDouble(Heat, TEXT("for ([\\d.]+) s"), Spell.HeatLockS, Spell.Id, Data))
		{
			return false;
		}
	}
	else if (Heat.Contains(TEXT("instant")))
	{
		Spell.HeatMode = EFireHeatMode::Instant;
		if (!MatchDouble(Heat, TEXT("([\\d.]+) instant"), Spell.InstantHeat, Spell.Id, Data))
		{
			return false;
		}
	}
	else if (Heat.Contains(TEXT("per tick")))
	{
		Spell.HeatMode = EFireHeatMode::Tick;
		if (!MatchDouble(Heat, TEXT("([\\d.]+) per tick"), Spell.HeatAmount, Spell.Id, Data))
		{
			return false;
		}
	}
	else if (Heat.Contains(TEXT("per target")))
	{
		Spell.HeatMode = EFireHeatMode::Target;
		if (!MatchDouble(Heat, TEXT("([\\d.]+) per target"), Spell.HeatAmount, Spell.Id, Data))
		{
			return false;
		}
	}
	else if (Heat.Contains(TEXT("per hit")))
	{
		Spell.HeatMode = EFireHeatMode::Hit;
		if (!MatchDouble(Heat, TEXT("([\\d.]+) per hit"), Spell.HeatAmount, Spell.Id, Data))
		{
			return false;
		}
	}
	else
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: fire heat gain not understood: %s"), *Heat));
	}

	Spell.bRootDuringCast = Notes.Contains(TEXT("rooted"), ESearchCase::IgnoreCase);
	Spell.bSequentialTelegraph = Notes.Contains(TEXT("shadow grows"), ESearchCase::IgnoreCase);
	Spell.bPerfectOnlyFirstTick = Notes.Contains(TEXT("only catch the first tick"), ESearchCase::IgnoreCase);
	Spell.bPierceShields = Shape.Contains(TEXT("piercing"), ESearchCase::IgnoreCase) || Notes.Contains(TEXT("Pierces"), ESearchCase::IgnoreCase);
	Spell.bBolt = Spell.Id == TEXT("fire_bolt");
	// The notes say the drain "is doubled". That word is the number 2, stacked on the threshold later.
	if (Notes.Contains(TEXT("doubled"), ESearchCase::IgnoreCase))
	{
		Spell.AbsorbDrainMult = 2.0;
	}
	if (Spell.Kind.IsEmpty())
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: %s has no kind"), *Spell.Id));
	}
	PublishFireCatalog(Spell, Data);
	return true;
}

bool LoadNumberField(const TSharedPtr<FJsonValue>& Value, double& Out)
{
	return Value.IsValid() && Value->TryGetNumber(Out) && FMath::IsFinite(Out);
}

bool LoadHeatGain(const FJsonObject& Gain, FKernelData& Data, const FString& Path)
{
	bool bBolt = false;
	bool bFlick = false;
	bool bTaken = false;
	bool bNear = false;
	// Values is keyed by UE::FSharedString. A const TPair<FString, ...>& loop variable bound a converted temporary
	// (key and shared pointer copied) for every field. The explicit FString below holds the same key text.
	for (const auto& Field : Gain.Values)
	{
		const FString Key(Field.Key);
		double Number = 0.0;
		if (!LoadNumberField(Field.Value, Number))
		{
			return Fail(Data, Path + TEXT(" fire gain is not a number: ") + Key);
		}
		if (Key == TEXT("perBoltHit"))
		{
			Data.Heat.PerBoltHit = Number;
			bBolt = true;
		}
		else if (Key == TEXT("perFlickTarget"))
		{
			Data.Heat.PerFlickTarget = Number;
			bFlick = true;
		}
		else if (Key == TEXT("perHitTakenUnabsorbed"))
		{
			Data.Heat.PerHitTakenUnabsorbed = Number;
			bTaken = true;
		}
		else if (Key.StartsWith(TEXT("perSecondEnemyWithin")) && Key.EndsWith(TEXT("m")))
		{
			FString Metres = Key;
			Metres.RemoveFromStart(TEXT("perSecondEnemyWithin"));
			Metres.RemoveFromEnd(TEXT("m"));
			Data.Heat.PerSecondPerEnemyWithin = Number;
			Data.Heat.EnemyWithinM = FCString::Atod(*Metres);
			bNear = Data.Heat.EnemyWithinM > 0.0;
		}
		else
		{
			return Fail(Data, Path + TEXT(" unknown fire gain ") + Key);
		}
	}
	if (!bBolt || !bFlick || !bTaken || !bNear)
	{
		return Fail(Data, Path + TEXT(" fire gain is missing a rule"));
	}
	return true;
}

bool LoadHeatDecay(const FJsonObject& Decay, FKernelData& Data, const FString& Path)
{
	bool bDecayRate = false;
	bool bDecayDelay = false;
	for (const auto& Field : Decay.Values)
	{
		const FString Key(Field.Key);
		double Number = 0.0;
		if (!LoadNumberField(Field.Value, Number))
		{
			return Fail(Data, Path + TEXT(" fire decay is not a number: ") + Key);
		}
		if (Key == TEXT("perSecond"))
		{
			Data.Heat.DecayPerSecond = Number;
			bDecayRate = true;
		}
		else if (Key == TEXT("afterNoGainSeconds"))
		{
			Data.Heat.DecayAfterNoGainS = Number;
			bDecayDelay = true;
		}
		else
		{
			return Fail(Data, Path + TEXT(" unknown fire decay ") + Key);
		}
	}
	if (!bDecayRate || !bDecayDelay)
	{
		return Fail(Data, Path + TEXT(" fire decay is incomplete"));
	}
	return true;
}

bool ParseHeatThreshold(const FJsonObject& Object, FHeatThreshold& Threshold, const FString& Path, FKernelData& Data)
{
	bool bSawAt = false;
	for (const auto& Field : Object.Values)
	{
		const FString Key(Field.Key);
		if (Key == TEXT("at"))
		{
			bSawAt = LoadNumberField(Field.Value, Threshold.At);
			if (!bSawAt)
			{
				return Fail(Data, Path + TEXT(" fire threshold at is invalid"));
			}
		}
		else if (Key == TEXT("name"))
		{
			if (!Field.Value.IsValid() || !Field.Value->TryGetString(Threshold.Name))
			{
				return Fail(Data, Path + TEXT(" fire threshold name is invalid"));
			}
		}
		else if (Key == TEXT("spellDamageMult"))
		{
			Threshold.bSpellDamage = LoadNumberField(Field.Value, Threshold.SpellDamageMult);
			if (!Threshold.bSpellDamage)
			{
				return Fail(Data, Path + TEXT(" fire spellDamageMult is invalid"));
			}
		}
		else if (Key == TEXT("absorbDrainMult"))
		{
			Threshold.bAbsorbDrain = LoadNumberField(Field.Value, Threshold.AbsorbDrainMult);
			if (!Threshold.bAbsorbDrain)
			{
				return Fail(Data, Path + TEXT(" fire absorbDrainMult is invalid"));
			}
		}
		else if (Key == TEXT("staminaRegenMult"))
		{
			Threshold.bStaminaRegen = LoadNumberField(Field.Value, Threshold.StaminaRegenMult);
			if (!Threshold.bStaminaRegen)
			{
				return Fail(Data, Path + TEXT(" fire staminaRegenMult is invalid"));
			}
		}
		else
		{
			return Fail(Data, Path + TEXT(" unknown fire threshold field ") + Key);
		}
	}
	if (!bSawAt)
	{
		return Fail(Data, Path + TEXT(" fire threshold has no at"));
	}
	return true;
}

bool LoadHeatThresholds(const FJsonObject& Identity, FKernelData& Data, const FString& Path)
{
	const TArray<TSharedPtr<FJsonValue>>* Thresholds = nullptr;
	if (!Identity.TryGetArrayField(TEXT("thresholds"), Thresholds) || Thresholds == nullptr || Thresholds->Num() == 0)
	{
		return Fail(Data, Path + TEXT(" fire thresholds are missing"));
	}
	for (const TSharedPtr<FJsonValue>& Value : *Thresholds)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
		{
			return Fail(Data, Path + TEXT(" fire threshold is not an object"));
		}
		FHeatThreshold Threshold;
		if (!ParseHeatThreshold(**Object, Threshold, Path, Data))
		{
			return false;
		}
		Data.Heat.Thresholds.Add(Threshold);
	}
	Data.Heat.Thresholds.Sort([](const FHeatThreshold& Left, const FHeatThreshold& Right)
	{
		return Left.At < Right.At;
	});
	return true;
}

// T23. schools.json air.combatIdentity, read strictly: an unknown key is a load failure, so a new pinned rule cannot be
// skipped silently.
bool ParseMomentumThreshold(const FJsonObject& Object, FMomentumThreshold& Threshold, const FString& Path, FKernelData& Data)
{
	bool bAt = false;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object.Values)
	{
		double Number = 0.0;
		if (!LoadNumberField(Pair.Value, Number))
		{
			return Fail(Data, Path + TEXT(" air threshold field is not a number: ") + Pair.Key);
		}
		if (Pair.Key == TEXT("at"))
		{
			Threshold.At = Number;
			bAt = true;
		}
		else if (Pair.Key == TEXT("spellsPierce"))
		{
			Threshold.bPierce = true;
			Threshold.SpellsPierce = static_cast<int32>(std::llround(Number));
			if (static_cast<double>(Threshold.SpellsPierce) != Number || Threshold.SpellsPierce < 0)
			{
				return Fail(Data, Path + TEXT(" air spellsPierce is not a whole count"));
			}
		}
		else if (Pair.Key == TEXT("rollStaminaMult"))
		{
			Threshold.bRollStamina = true;
			Threshold.RollStaminaMult = Number;
		}
		else if (Pair.Key == TEXT("spellRangeMult"))
		{
			Threshold.bRange = true;
			Threshold.SpellRangeMult = Number;
		}
		else
		{
			return Fail(Data, Path + TEXT(" unknown air threshold field ") + Pair.Key);
		}
	}
	if (!bAt)
	{
		return Fail(Data, Path + TEXT(" air threshold has no at"));
	}
	return true;
}

bool LoadMomentum(const TArray<TSharedPtr<FJsonValue>>& Schools, FKernelData& Data, const FString& Path)
{
	const FJsonObject* Air = nullptr;
	for (const TSharedPtr<FJsonValue>& Value : Schools)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FString Id;
		if (Value.IsValid() && Value->TryGetObject(Object) && Object && (*Object)->TryGetStringField(TEXT("id"), Id) && Id == TEXT("air"))
		{
			Air = Object->Get();
		}
	}
	if (!Air)
	{
		return Fail(Data, Path + TEXT(" has no air school"));
	}
	const FJsonObject* Identity = nullptr;
	FString Resource;
	FString Extra;
	if (!NeedObject(*Air, TEXT("combatIdentity"), Identity, Path, Data)
		|| !NeedString(*Identity, TEXT("resource"), Resource, Path, Data)
		|| !NeedString(*Identity, TEXT("perfectAbsorbExtra"), Extra, Path, Data)
		|| !NeedNumber(*Identity, TEXT("absorbDrainMult"), Data.Momentum.AbsorbDrainMult, Path, Data))
	{
		return false;
	}
	if (Resource != TEXT("momentum"))
	{
		return Fail(Data, Path + TEXT(" air resource is not momentum"));
	}
	// The kernel implements the deflect reading only. Anything else in the pin is a rule this kernel does not have.
	if (!Extra.StartsWith(TEXT("deflect")) || !Extra.Contains(TEXT("tier <= own unlocked tier")) || !Extra.Contains(TEXT("aim direction")))
	{
		return Fail(Data, Path + TEXT(" air perfectAbsorbExtra is not the deflect reading this kernel implements"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Range = nullptr;
	if (!Identity->TryGetArrayField(TEXT("range"), Range) || Range == nullptr || Range->Num() != 2
		|| !LoadNumberField((*Range)[0], Data.Momentum.Min) || !LoadNumberField((*Range)[1], Data.Momentum.Max)
		|| Data.Momentum.Max < Data.Momentum.Min)
	{
		return Fail(Data, Path + TEXT(" air momentum range is invalid"));
	}
	const FJsonObject* Gain = nullptr;
	const FJsonObject* Decay = nullptr;
	if (!NeedObject(*Identity, TEXT("gain"), Gain, Path, Data) || !NeedObject(*Identity, TEXT("decay"), Decay, Path, Data))
	{
		return false;
	}
	bool bMoving = false;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Gain->Values)
	{
		double Number = 0.0;
		FString Speed;
		if (!LoadNumberField(Pair.Value, Number))
		{
			return Fail(Data, Path + TEXT(" air gain is not a number: ") + Pair.Key);
		}
		// "perSecondMovingAbove4Mps": the speed is parsed from the key, like Heat's "Within3m".
		if (MatchGroup(Pair.Key, TEXT("^perSecondMovingAbove([\\d.]+)Mps$"), Speed))
		{
			Data.Momentum.GainPerSecondMoving = Number;
			Data.Momentum.MovingAboveMps = FCString::Atod(*Speed);
			bMoving = true;
		}
		else
		{
			return Fail(Data, Path + TEXT(" unknown air gain ") + Pair.Key);
		}
	}
	bool bStill = false;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Decay->Values)
	{
		double Number = 0.0;
		if (!LoadNumberField(Pair.Value, Number))
		{
			return Fail(Data, Path + TEXT(" air decay is not a number: ") + Pair.Key);
		}
		if (Pair.Key == TEXT("perSecondStill"))
		{
			Data.Momentum.DecayPerSecondStill = Number;
			bStill = true;
		}
		else
		{
			return Fail(Data, Path + TEXT(" unknown air decay ") + Pair.Key);
		}
	}
	if (!bMoving || !bStill)
	{
		return Fail(Data, Path + TEXT(" air gain or decay is missing a rule"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Thresholds = nullptr;
	if (!Identity->TryGetArrayField(TEXT("thresholds"), Thresholds) || Thresholds == nullptr)
	{
		return Fail(Data, Path + TEXT(" air thresholds are missing"));
	}
	for (const TSharedPtr<FJsonValue>& Value : *Thresholds)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object)
		{
			return Fail(Data, Path + TEXT(" air threshold is not an object"));
		}
		FMomentumThreshold Threshold;
		if (!ParseMomentumThreshold(**Object, Threshold, Path, Data))
		{
			return false;
		}
		Data.Momentum.Thresholds.Add(Threshold);
	}
	Data.Momentum.Thresholds.Sort([](const FMomentumThreshold& Left, const FMomentumThreshold& Right)
	{
		return Left.At < Right.At;
	});
	return true;
}

bool LoadHeat(FKernelData& Data)
{
	const FString Path = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/schools.json"));
	TSharedPtr<FJsonObject> Root;
	if (!LoadJson(Path, Root, Data))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Schools = nullptr;
	if (!Root->TryGetArrayField(TEXT("schools"), Schools) || Schools == nullptr)
	{
		return Fail(Data, Path + TEXT(" has no schools"));
	}
	const FJsonObject* Fire = nullptr;
	for (const TSharedPtr<FJsonValue>& Value : *Schools)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FString Id;
		if (Value.IsValid() && Value->TryGetObject(Object) && Object && (*Object)->TryGetStringField(TEXT("id"), Id) && Id == TEXT("fire"))
		{
			Fire = Object->Get();
		}
	}
	if (!Fire)
	{
		return Fail(Data, Path + TEXT(" has no fire school"));
	}
	const FJsonObject* Identity = nullptr;
	FString Resource;
	FString Extra;
	if (!NeedObject(*Fire, TEXT("combatIdentity"), Identity, Path, Data)
		|| !NeedString(*Identity, TEXT("resource"), Resource, Path, Data)
		|| !NeedString(*Identity, TEXT("perfectAbsorbExtra"), Extra, Path, Data))
	{
		return false;
	}
	if (Resource != TEXT("heat"))
	{
		return Fail(Data, Path + TEXT(" fire resource is not heat"));
	}
	if (!Extra.Contains(TEXT("none"), ESearchCase::IgnoreCase))
	{
		return Fail(Data, Path + TEXT(" fire perfectAbsorbExtra is not the none-reading this kernel implements"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Range = nullptr;
	if (!Identity->TryGetArrayField(TEXT("range"), Range) || Range == nullptr || Range->Num() != 2
		|| !LoadNumberField((*Range)[0], Data.Heat.Min) || !LoadNumberField((*Range)[1], Data.Heat.Max)
		|| Data.Heat.Max < Data.Heat.Min)
	{
		return Fail(Data, Path + TEXT(" fire heat range is invalid"));
	}
	const FJsonObject* Gain = nullptr;
	const FJsonObject* Decay = nullptr;
	if (!NeedObject(*Identity, TEXT("gain"), Gain, Path, Data) || !NeedObject(*Identity, TEXT("decay"), Decay, Path, Data))
	{
		return false;
	}
	// T23: the air identity sits in the same pinned file. It is read from this parse, so PinHash does not change.
	return LoadHeatGain(*Gain, Data, Path) && LoadHeatDecay(*Decay, Data, Path) && LoadHeatThresholds(*Identity, Data, Path)
		&& LoadMomentum(*Schools, Data, Path);
}

bool LoadFireSpells(FKernelData& Data)
{
	// The notes column contains commas and is not quoted. Split the first twelve fields and keep the tail.
	const FString Path = PinnedPath(TEXT("docs/design/reference-the-ledger/design/data/spells-fire.csv"));
	FString Text;
	if (!LoadText(Path, Text, Data))
	{
		return false;
	}
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, false);
	if (Lines.Num() < 2)
	{
		return Fail(Data, Path + TEXT(" has no fire spells"));
	}
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		if (Lines[Index].TrimStartAndEnd().IsEmpty())
		{
			continue;
		}
		TArray<FString> Values;
		Lines[Index].ParseIntoArray(Values, TEXT(","), false);
		if (Values.Num() < 13)
		{
			return Fail(Data, FString::Printf(TEXT("kernel data: short fire-spell row in %s: %s"), *Path, *Lines[Index]));
		}
		FString Notes = Values[12];
		for (int32 Extra = 13; Extra < Values.Num(); ++Extra)
		{
			Notes += TEXT(",");
			Notes += Values[Extra];
		}
		const FString& Damage = Values[7];
		FFireSpell Spell;
		Spell.Id = Values[0];
		Spell.Name = Values[1];
		// T44: a number cell that is not a number, or a blockable cell the pin does not use, refuses the load. The raw
		// blockable cell no longer passes through as the family.
		const TCHAR* const Sheet = TEXT("spells-fire.csv");
		FString Error;
		double Tier = 0.0;
		if (!CellNumber(Values[2], Sheet, TEXT("tier"), Spell.Id, Tier, Error)
			|| !CellNumber(Values[4], Sheet, TEXT("cast_s"), Spell.Id, Spell.CastS, Error)
			|| !CellNumber(Values[5], Sheet, TEXT("cooldown_s"), Spell.Id, Spell.CooldownS, Error)
			|| !CellNumber(Values[6], Sheet, TEXT("mana"), Spell.Id, Spell.Mana, Error)
			|| !CellNumber(Values[9], Sheet, TEXT("telegraph_s"), Spell.Id, Spell.TelegraphS, Error)
			|| !CellNumber(Values[10], Sheet, TEXT("range_m"), Spell.Id, Spell.RangeM, Error))
		{
			return Fail(Data, TEXT("kernel data: ") + Error);
		}
		if (Tier != std::floor(Tier))
		{
			return Fail(Data, FString::Printf(TEXT("kernel data: %s row %s: tier cell '%s' is not a whole number"), Sheet, *Spell.Id, *Values[2]));
		}
		EThreatFamily Family = EThreatFamily::None;
		if (!ParseBlockable(Values[8], Family, Error))
		{
			return Fail(Data, FString::Printf(TEXT("kernel data: %s row %s: %s"), Sheet, *Spell.Id, *Error));
		}
		Spell.Tier = static_cast<int32>(Tier);
		Spell.Shape = Values[3];
		Spell.DamageText = Damage;
		Spell.Damage = (!Damage.IsEmpty() && FChar::IsDigit(Damage[0])) ? FCString::Atod(*Damage) : 0.0;
		Spell.Blockable = Values[8];
		// Fire keeps n/a as its family; HitFamily (Fire.cpp) reads it as magic at the hit.
		Spell.Family = FamilyName(Family);
		Spell.HeatGain = Values[11];
		Spell.Notes = Notes;
		if (!ParseFireSpell(Spell, Data))
		{
			return false;
		}
		Data.FireSpells.Add(MoveTemp(Spell));
	}
	if (Data.FireSpells.Num() == 0)
	{
		return Fail(Data, Path + TEXT(" has no fire spells"));
	}
	return true;
}
}

const FFireSpell* FindFireSpell(const FString& Id)
{
	for (const FFireSpell& Spell : KernelData().FireSpells)
	{
		if (Spell.Id == Id)
		{
			return &Spell;
		}
	}
	return nullptr;
}
