#include "Kernel/KernelData.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

namespace
{
// Set only for the duration of LoadKernelDataWithOverrides, on the game thread.
const TMap<FString, FString>* GTextOverrides = nullptr;

FString PinnedPath(const TCHAR* Relative)
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/pinned"), Relative));
}

bool Fail(FKernelData& Data, const FString& Message)
{
	Data.bReady = false;
	Data.Error = Message;
	return false;
}

bool LoadText(const FString& Path, FString& Out, FKernelData& Data)
{
	const FString* Override = GTextOverrides ? GTextOverrides->Find(Path) : nullptr;
	if (Override)
	{
		Out = *Override;
	}
	else if (!FFileHelper::LoadFileToString(Out, *Path))
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: cannot read %s"), *Path));
	}
	if (Out.Len() > 0 && Out[0] == 0xFEFF)
	{
		Out.RemoveAt(0, 1, EAllowShrinking::No);
	}
	return true;
}

bool LoadJson(const FString& Path, TSharedPtr<FJsonObject>& Out, FKernelData& Data)
{
	FString Text;
	if (!LoadText(Path, Text, Data))
	{
		return false;
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: %s is not JSON"), *Path));
	}
	return true;
}

bool NeedNumber(const FJsonObject& Object, const TCHAR* Field, double& Out, const FString& Path, FKernelData& Data)
{
	if (!Object.TryGetNumberField(Field, Out) || !FMath::IsFinite(Out))
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: %s is missing finite %s"), *Path, Field));
	}
	return true;
}

bool NeedInt(const FJsonObject& Object, const TCHAR* Field, int32& Out, const FString& Path, FKernelData& Data)
{
	double Value = 0.0;
	if (!NeedNumber(Object, Field, Value, Path, Data))
	{
		return false;
	}
	Out = static_cast<int32>(std::llround(Value));
	return true;
}

bool NeedBool(const FJsonObject& Object, const TCHAR* Field, bool& Out, const FString& Path, FKernelData& Data)
{
	if (!Object.TryGetBoolField(Field, Out))
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: %s is missing bool %s"), *Path, Field));
	}
	return true;
}

bool NeedString(const FJsonObject& Object, const TCHAR* Field, FString& Out, const FString& Path, FKernelData& Data)
{
	if (!Object.TryGetStringField(Field, Out))
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: %s is missing string %s"), *Path, Field));
	}
	return true;
}

bool NeedObject(const FJsonObject& Object, const TCHAR* Field, const FJsonObject*& Out, const FString& Path, FKernelData& Data)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (!Object.TryGetObjectField(Field, Child) || Child == nullptr || !Child->IsValid())
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: %s is missing object %s"), *Path, Field));
	}
	Out = Child->Get();
	return true;
}

bool NeedVec(const FJsonObject& Object, const TCHAR* Field, FSimVec& Out, const FString& Path, FKernelData& Data)
{
	const FJsonObject* Child = nullptr;
	if (!NeedObject(Object, Field, Child, Path, Data))
	{
		return false;
	}
	return NeedNumber(*Child, TEXT("x"), Out.X, Path + TEXT(".") + Field, Data)
		&& NeedNumber(*Child, TEXT("y"), Out.Y, Path + TEXT(".") + Field, Data);
}

bool MatchGroup(const FString& Source, const FString& Pattern, FString& Out)
{
	const FRegexPattern Compiled(Pattern);
	FRegexMatcher Matcher(Compiled, Source);
	if (!Matcher.FindNext())
	{
		return false;
	}
	Out = Matcher.GetCaptureGroup(1);
	return true;
}

bool MatchDouble(const FString& Source, const FString& Pattern, double& Out, const FString& What, FKernelData& Data)
{
	FString Text;
	if (!MatchGroup(Source, Pattern, Text))
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: shape not understood: %s / %s (%s)"), *Source, *Pattern, *What));
	}
	Out = FCString::Atod(*Text);
	return true;
}

struct FCsvTable
{
	TArray<FString> Keys;
	TArray<TMap<FString, FString>> Rows;
};

bool LoadCsv(const FString& Path, FCsvTable& Out, FKernelData& Data)
{
	FString Text;
	if (!LoadText(Path, Text, Data))
	{
		return false;
	}
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, false);
	if (Lines.Num() == 0)
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: empty CSV %s"), *Path));
	}
	Lines[0].ParseIntoArray(Out.Keys, TEXT(","), false);
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		if (Lines[Index].TrimStartAndEnd().IsEmpty())
		{
			continue;
		}
		TArray<FString> Values;
		Lines[Index].ParseIntoArray(Values, TEXT(","), false);
		if (Values.Num() != Out.Keys.Num())
		{
			return Fail(Data, FString::Printf(TEXT("kernel data: ambiguous CSV row in %s: %s"), *Path, *Lines[Index]));
		}
		TMap<FString, FString> Row;
		for (int32 Column = 0; Column < Out.Keys.Num(); ++Column)
		{
			Row.Add(Out.Keys[Column], Values[Column]);
		}
		Out.Rows.Add(MoveTemp(Row));
	}
	return true;
}

const FString* Cell(const TMap<FString, FString>& Row, const TCHAR* Key)
{
	return Row.Find(FString(Key));
}

bool LinearStat(const FCsvTable& Stats, const TCHAR* Stat, const TCHAR* Variable, double& Base, double& PerRank, FKernelData& Data)
{
	const TMap<FString, FString>* Found = nullptr;
	for (const TMap<FString, FString>& Row : Stats.Rows)
	{
		const FString* Name = Cell(Row, TEXT("stat"));
		if (Name && *Name == Stat)
		{
			Found = &Row;
			break;
		}
	}
	if (!Found)
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: missing stat %s"), Stat));
	}
	const FString* Effect = Cell(*Found, TEXT("arena_effect"));
	if (!Effect)
	{
		return Fail(Data, TEXT("kernel data: stat row has no arena_effect"));
	}
	const FString Pattern = FString::Printf(TEXT("%s = ([\\d.]+) \\+ ([\\d.]+)\\*rank"), Variable);
	const FRegexPattern Compiled(Pattern);
	FRegexMatcher Matcher(Compiled, *Effect);
	if (!Matcher.FindNext())
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: missing stat formula %s"), Variable));
	}
	Base = FCString::Atod(*Matcher.GetCaptureGroup(1));
	PerRank = FCString::Atod(*Matcher.GetCaptureGroup(2));
	return true;
}

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
		FAttackSpec Attack;
		if (!ParseAttack(**AttackObject, Attack, Path + TEXT(".") + Attack.Id, Data))
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

bool BuildSpells(const FCsvTable& Rows, FKernelData& Data)
{
	const FString* LashShape = nullptr;
	for (const TMap<FString, FString>& Row : Rows.Rows)
	{
		if (Cell(Row, TEXT("line")) && *Cell(Row, TEXT("line")) == TEXT("lash") && Cell(Row, TEXT("tier")) && *Cell(Row, TEXT("tier")) == TEXT("1"))
		{
			LashShape = Cell(Row, TEXT("shape"));
		}
	}
	if (!LashShape)
	{
		return Fail(Data, TEXT("kernel data: missing lash tier 1 shape"));
	}
	double LashArc = 0.0;
	if (!MatchDouble(*LashShape, TEXT("cone ([\\d.]+) deg"), LashArc, TEXT("lash arc"), Data))
	{
		return false;
	}

	for (const TMap<FString, FString>& Row : Rows.Rows)
	{
		const FString& Line = *Cell(Row, TEXT("line"));
		const FString& TierText = *Cell(Row, TEXT("tier"));
		const FString& Branch = *Cell(Row, TEXT("branch"));
		const FString& Shape = *Cell(Row, TEXT("shape"));
		const FString& DamageText = *Cell(Row, TEXT("damage"));
		const int32 Tier = FCString::Atoi(*TierText);
		FSpell Spell;
		Spell.Id = FString::Printf(TEXT("%s:%d:%s"), *Line, Tier, Branch.IsEmpty() ? TEXT("base") : *Branch);
		Spell.Line = Line;
		Spell.Tier = Tier;
		Spell.Branch = Branch;
		Spell.Name = *Cell(Row, TEXT("name"));
		Spell.CastS = FCString::Atod(**Cell(Row, TEXT("cast_s")));
		Spell.CooldownS = FCString::Atod(**Cell(Row, TEXT("cooldown_s")));
		Spell.Mana = FCString::Atod(**Cell(Row, TEXT("mana")));
		Spell.Damage = (!DamageText.IsEmpty() && FChar::IsDigit(DamageText[0])) ? FCString::Atod(*DamageText) : 0.0;
		Spell.Family = *Cell(Row, TEXT("blockable")) == TEXT("UNBLOCKABLE") ? TEXT("unblockable") : TEXT("magic");
		Spell.TelegraphS = FCString::Atod(**Cell(Row, TEXT("telegraph_s")));
		Spell.RangeM = FCString::Atod(**Cell(Row, TEXT("range_m")));
		Spell.Kind = TEXT("self");

		if (Line == TEXT("bolt") || Line == TEXT("tide_orb"))
		{
			Spell.Kind = TEXT("projectile");
			if (Shape.Contains(TEXT("m/s")))
			{
				if (!MatchDouble(Shape, TEXT("([\\d.]+) m/s"), Spell.SpeedMps, Spell.Id, Data))
				{
					return false;
				}
			}
			else
			{
				Spell.SpeedMps = Data.FanSpeedMps;
			}
			if (Shape.Contains(TEXT("two projectiles")))
			{
				Spell.Count = 2;
				double Half = 0.0;
				if (!MatchDouble(Shape, TEXT("\\+-([\\d.]+) deg"), Half, Spell.Id, Data))
				{
					return false;
				}
				Spell.SpreadDeg = 2.0 * Half;
			}
			if (Shape.Contains(TEXT("-orb fan")))
			{
				double Count = 0.0;
				if (!MatchDouble(Shape, TEXT("(\\d+)-orb"), Count, Spell.Id, Data)
					|| !MatchDouble(Shape, TEXT("fan ([\\d.]+) deg"), Spell.SpreadDeg, Spell.Id, Data))
				{
					return false;
				}
				Spell.Count = static_cast<int32>(std::llround(Count));
			}
			if (Shape.Contains(TEXT("bursts")))
			{
				if (!MatchDouble(Shape, TEXT("bursts r ([\\d.]+) m"), Spell.BurstRadiusM, Spell.Id, Data))
				{
					return false;
				}
			}
			else if (Shape.Contains(TEXT(" r ")))
			{
				if (!MatchDouble(Shape, TEXT("r ([\\d.]+) m"), Spell.RadiusM, Spell.Id, Data))
				{
					return false;
				}
			}
			if (Line == TEXT("bolt"))
			{
				Data.BoltName = Spell.Name;
				Data.BoltCastS = Spell.CastS;
				Data.BoltCooldownS = Spell.CooldownS;
				Data.BoltMana = Spell.Mana;
				Data.BoltDamage = Spell.Damage;
				Data.BoltRangeM = Spell.RangeM;
				Data.BoltSpeedMps = Spell.SpeedMps;
			}
		}
		else if (Line == TEXT("lash"))
		{
			Spell.Kind = Tier == 3 ? TEXT("ring") : TEXT("cone");
			Spell.ArcDeg = Spell.Kind == TEXT("ring") ? 360.0 : LashArc;
			if (Shape.Contains(TEXT("pull")) || Shape.Contains(TEXT("push")))
			{
				Spell.Effect = Shape.Contains(TEXT("pull")) ? TEXT("pull") : TEXT("push");
				const FString Pattern = Shape.Contains(TEXT("pull")) ? TEXT("pull ([\\d.]+) m") : TEXT("push ([\\d.]+) m");
				if (!MatchDouble(Shape, Pattern, Spell.Amount, Spell.Id, Data))
				{
					return false;
				}
			}
			if (Shape.Contains(TEXT("root")))
			{
				Spell.Effect = TEXT("root");
				if (!MatchDouble(Shape, TEXT("root ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
				{
					return false;
				}
			}
		}
		else if (Line == TEXT("mire"))
		{
			Spell.Kind = Tier == 4 ? TEXT("target") : TEXT("zone");
			if (Spell.Kind == TEXT("zone") && !MatchDouble(Shape, TEXT("r ([\\d.]+) m"), Spell.RadiusM, Spell.Id, Data))
			{
				return false;
			}
			if (Tier == 1)
			{
				Spell.Effect = TEXT("slow");
				double Percent = 0.0;
				if (!MatchDouble(Shape, TEXT("slow ([\\d.]+)%"), Percent, Spell.Id, Data)
					|| !MatchDouble(Shape, TEXT("for ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
				{
					return false;
				}
				Spell.Amount = Percent / 100.0;
			}
			if (Tier == 2)
			{
				Spell.Effect = TEXT("fog");
				if (!MatchDouble(Shape, TEXT("for ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
				{
					return false;
				}
			}
			if (Tier == 3)
			{
				Spell.Effect = TEXT("root");
				if (!MatchDouble(Shape, TEXT("root ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
				{
					return false;
				}
			}
			if (Tier == 4)
			{
				Spell.Effect = TEXT("encase");
				if (!MatchDouble(Shape, TEXT("target ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
				{
					return false;
				}
			}
		}
		else if (Line == TEXT("mend"))
		{
			if (Tier == 1)
			{
				Spell.Effect = TEXT("heal");
				if (!MatchDouble(DamageText, TEXT("heal ([\\d.]+)"), Spell.Amount, Spell.Id, Data))
				{
					return false;
				}
			}
			if (Tier == 2)
			{
				Spell.Effect = TEXT("ward");
				double Percent = 0.0;
				if (!MatchDouble(Shape, TEXT("-([\\d.]+)%"), Percent, Spell.Id, Data)
					|| !MatchDouble(Shape, TEXT("for ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
				{
					return false;
				}
				Spell.Amount = Percent / 100.0;
			}
			if (Tier == 3)
			{
				Spell.Effect = TEXT("hot");
				if (!MatchDouble(DamageText, TEXT("heal ([\\d.]+)"), Spell.Amount, Spell.Id, Data)
					|| !MatchDouble(DamageText, TEXT("over ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
				{
					return false;
				}
			}
			if (Tier == 4)
			{
				Spell.Effect = TEXT("font");
				if (!MatchDouble(Shape, TEXT("restore ([\\d.]+) mana"), Spell.Amount, Spell.Id, Data))
				{
					return false;
				}
			}
		}
		else if (Line == TEXT("mirror"))
		{
			if (Tier == 1)
			{
				Spell.Effect = TEXT("sheen");
				double Percent = 0.0;
				if (!MatchDouble(Shape, TEXT("\\+([\\d.]+)%"), Percent, Spell.Id, Data)
					|| !MatchDouble(Shape, TEXT("self ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
				{
					return false;
				}
				Spell.Amount = Percent / 100.0;
			}
			if (Tier == 2)
			{
				Spell.Kind = TEXT("passive");
				Spell.Effect = Branch == TEXT("A") ? TEXT("reflection") : TEXT("ripple");
			}
			if (Tier == 3)
			{
				Spell.Effect = TEXT("decoy");
				if (!MatchDouble(Shape, TEXT("for ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
				{
					return false;
				}
			}
			if (Tier == 4)
			{
				Spell.Kind = TEXT("wave");
				Spell.Effect = TEXT("return");
				if (!MatchDouble(DamageText, TEXT("max ([\\d.]+)"), Spell.Amount, Spell.Id, Data))
				{
					return false;
				}
				Spell.Damage = Spell.Amount;
				const FString* Notes = Cell(Row, TEXT("notes"));
				double Percent = 0.0;
				if (!Notes || !MatchDouble(*Notes, TEXT("stores ([\\d.]+)%"), Percent, Spell.Id, Data))
				{
					return false;
				}
				Data.StoredFraction = Percent / 100.0;
			}
		}
		else
		{
			return Fail(Data, FString::Printf(TEXT("kernel data: unrecognized Water line %s"), *Line));
		}
		Data.Spells.Add(MoveTemp(Spell));
	}
	if (!(Data.StoredFraction > 0.0))
	{
		return Fail(Data, TEXT("kernel data: mirror tier 4 did not publish a stored fraction"));
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
			FEnemySpec Spec;
			if (!ParseEnemy(**Object, Spec, Path + TEXT("#") + Spec.Id, Data))
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
		const TArray<TSharedPtr<FJsonValue>>* Waves = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Gold = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Renown = nullptr;
		if (!NeedString(**TierObject, TEXT("id"), Tier.Id, Path, Data)
			|| !NeedString(**TierObject, TEXT("name"), Tier.Name, Path, Data)
			|| !NeedInt(**TierObject, TEXT("requiresMastery"), Tier.RequiresMastery, Path, Data)
			|| !(*TierObject)->TryGetArrayField(TEXT("waves"), Waves) || Waves->Num() == 0
			|| !(*TierObject)->TryGetArrayField(TEXT("payoutGold"), Gold)
			|| !(*TierObject)->TryGetArrayField(TEXT("renown"), Renown))
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
			const TArray<TSharedPtr<FJsonValue>>* Spawns = nullptr;
			if (!NeedInt(**WaveObject, TEXT("n"), Wave.N, Path, Data) || !NeedString(**WaveObject, TEXT("kind"), Wave.Kind, Path, Data)
				|| !(*WaveObject)->TryGetArrayField(TEXT("spawns"), Spawns) || Spawns->Num() == 0)
			{
				if (Data.Error.IsEmpty())
				{
					Fail(Data, Path + TEXT(" wave is missing spawns"));
				}
				return false;
			}
			const TArray<TSharedPtr<FJsonValue>>* Band = nullptr;
			if ((*WaveObject)->TryGetArrayField(TEXT("targetDurationS"), Band) && Band && Band->Num() == 2)
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
				double Count = 0.0;
				if ((*SpawnObject)->TryGetNumberField(TEXT("count"), Count))
				{
					Spawn.Count = static_cast<int32>(std::llround(Count));
				}
				double Echo = 0.0;
				if ((*SpawnObject)->TryGetNumberField(TEXT("orEchoFromLoop"), Echo))
				{
					Spawn.EchoFromLoop = static_cast<int32>(std::llround(Echo));
				}
				if ((*SpawnObject)->TryGetStringField(TEXT("enemy"), Spawn.EnemyId))
				{
					Spawn.bEnemy = true;
				}
				else if ((*SpawnObject)->TryGetStringField(TEXT("mage"), Spawn.MageId))
				{
					Spawn.bEnemy = false;
					if (!NeedNumber(**SpawnObject, TEXT("competence"), Spawn.Competence, Path, Data))
					{
						return false;
					}
				}
				else
				{
					return Fail(Data, Path + TEXT(" spawn is neither an enemy nor a mage"));
				}
				Wave.Spawns.Add(Spawn);
			}
			Tier.Waves.Add(MoveTemp(Wave));
		}
		for (const TSharedPtr<FJsonValue>& Value : *Gold)
		{
			double Number = 0.0;
			if (!Value.IsValid() || !Value->TryGetNumber(Number))
			{
				return Fail(Data, Path + TEXT(" payoutGold entry is not a number"));
			}
			Tier.PayoutGold.Add(static_cast<int32>(std::llround(Number)));
		}
		for (const TSharedPtr<FJsonValue>& Value : *Renown)
		{
			double Number = 0.0;
			if (!Value.IsValid() || !Value->TryGetNumber(Number))
			{
				return Fail(Data, Path + TEXT(" renown entry is not a number"));
			}
			Tier.Renown.Add(static_cast<int32>(std::llround(Number)));
		}
		Data.ArenaTiers.Add(MoveTemp(Tier));
	}
	return true;
}

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
	bool bBolt = false;
	bool bFlick = false;
	bool bTaken = false;
	bool bNear = false;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Gain->Values)
	{
		double Number = 0.0;
		if (!LoadNumberField(Field.Value, Number))
		{
			return Fail(Data, Path + TEXT(" fire gain is not a number: ") + Field.Key);
		}
		if (Field.Key == TEXT("perBoltHit"))
		{
			Data.Heat.PerBoltHit = Number;
			bBolt = true;
		}
		else if (Field.Key == TEXT("perFlickTarget"))
		{
			Data.Heat.PerFlickTarget = Number;
			bFlick = true;
		}
		else if (Field.Key == TEXT("perHitTakenUnabsorbed"))
		{
			Data.Heat.PerHitTakenUnabsorbed = Number;
			bTaken = true;
		}
		else if (Field.Key.StartsWith(TEXT("perSecondEnemyWithin")) && Field.Key.EndsWith(TEXT("m")))
		{
			FString Metres = Field.Key;
			Metres.RemoveFromStart(TEXT("perSecondEnemyWithin"));
			Metres.RemoveFromEnd(TEXT("m"));
			Data.Heat.PerSecondPerEnemyWithin = Number;
			Data.Heat.EnemyWithinM = FCString::Atod(*Metres);
			bNear = Data.Heat.EnemyWithinM > 0.0;
		}
		else
		{
			return Fail(Data, Path + TEXT(" unknown fire gain ") + Field.Key);
		}
	}
	if (!bBolt || !bFlick || !bTaken || !bNear)
	{
		return Fail(Data, Path + TEXT(" fire gain is missing a rule"));
	}
	bool bDecayRate = false;
	bool bDecayDelay = false;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Decay->Values)
	{
		double Number = 0.0;
		if (!LoadNumberField(Field.Value, Number))
		{
			return Fail(Data, Path + TEXT(" fire decay is not a number: ") + Field.Key);
		}
		if (Field.Key == TEXT("perSecond"))
		{
			Data.Heat.DecayPerSecond = Number;
			bDecayRate = true;
		}
		else if (Field.Key == TEXT("afterNoGainSeconds"))
		{
			Data.Heat.DecayAfterNoGainS = Number;
			bDecayDelay = true;
		}
		else
		{
			return Fail(Data, Path + TEXT(" unknown fire decay ") + Field.Key);
		}
	}
	if (!bDecayRate || !bDecayDelay)
	{
		return Fail(Data, Path + TEXT(" fire decay is incomplete"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Thresholds = nullptr;
	if (!Identity->TryGetArrayField(TEXT("thresholds"), Thresholds) || Thresholds == nullptr || Thresholds->Num() == 0)
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
		bool bSawAt = false;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*Object)->Values)
		{
			if (Field.Key == TEXT("at"))
			{
				bSawAt = LoadNumberField(Field.Value, Threshold.At);
				if (!bSawAt)
				{
					return Fail(Data, Path + TEXT(" fire threshold at is invalid"));
				}
			}
			else if (Field.Key == TEXT("name"))
			{
				if (!Field.Value.IsValid() || !Field.Value->TryGetString(Threshold.Name))
				{
					return Fail(Data, Path + TEXT(" fire threshold name is invalid"));
				}
			}
			else if (Field.Key == TEXT("spellDamageMult"))
			{
				Threshold.bSpellDamage = LoadNumberField(Field.Value, Threshold.SpellDamageMult);
				if (!Threshold.bSpellDamage)
				{
					return Fail(Data, Path + TEXT(" fire spellDamageMult is invalid"));
				}
			}
			else if (Field.Key == TEXT("absorbDrainMult"))
			{
				Threshold.bAbsorbDrain = LoadNumberField(Field.Value, Threshold.AbsorbDrainMult);
				if (!Threshold.bAbsorbDrain)
				{
					return Fail(Data, Path + TEXT(" fire absorbDrainMult is invalid"));
				}
			}
			else if (Field.Key == TEXT("staminaRegenMult"))
			{
				Threshold.bStaminaRegen = LoadNumberField(Field.Value, Threshold.StaminaRegenMult);
				if (!Threshold.bStaminaRegen)
				{
					return Fail(Data, Path + TEXT(" fire staminaRegenMult is invalid"));
				}
			}
			else
			{
				return Fail(Data, Path + TEXT(" unknown fire threshold field ") + Field.Key);
			}
		}
		if (!bSawAt)
		{
			return Fail(Data, Path + TEXT(" fire threshold has no at"));
		}
		Data.Heat.Thresholds.Add(Threshold);
	}
	Data.Heat.Thresholds.Sort([](const FHeatThreshold& Left, const FHeatThreshold& Right)
	{
		return Left.At < Right.At;
	});
	return true;
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
		Spell.Tier = FCString::Atoi(*Values[2]);
		Spell.Shape = Values[3];
		Spell.CastS = FCString::Atod(*Values[4]);
		Spell.CooldownS = FCString::Atod(*Values[5]);
		Spell.Mana = FCString::Atod(*Values[6]);
		Spell.DamageText = Damage;
		Spell.Damage = (!Damage.IsEmpty() && FChar::IsDigit(Damage[0])) ? FCString::Atod(*Damage) : 0.0;
		Spell.Blockable = Values[8];
		Spell.Family = Spell.Blockable == TEXT("UNBLOCKABLE") ? TEXT("unblockable") : Spell.Blockable == TEXT("absorbable") ? TEXT("magic") : Spell.Blockable;
		Spell.TelegraphS = FCString::Atod(*Values[9]);
		Spell.RangeM = FCString::Atod(*Values[10]);
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

bool LoadGamesTuning(const FJsonObject& Games, FKernelData& Data, const FString& Path)
{
	const TArray<TSharedPtr<FJsonValue>>* Preferred = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Opponents = nullptr;
	if (!NeedVec(Games, TEXT("playerSpawn"), Data.PlayerSpawn, Path, Data)
		|| !Games.TryGetArrayField(TEXT("magePreferredDistanceM"), Preferred) || Preferred->Num() != 2
		|| !(*Preferred)[0].IsValid() || !(*Preferred)[0]->TryGetNumber(Data.MagePreferredMinM)
		|| !(*Preferred)[1].IsValid() || !(*Preferred)[1]->TryGetNumber(Data.MagePreferredMaxM)
		|| !NeedNumber(Games, TEXT("mageStrafePeriodS"), Data.MageStrafePeriodS, Path, Data)
		|| !NeedNumber(Games, TEXT("mageAimLeadFraction"), Data.MageAimLeadFraction, Path, Data)
		|| !NeedNumber(Games, TEXT("mageDefenceHoldS"), Data.MageDefenceHoldS, Path, Data)
		|| !NeedNumber(Games, TEXT("referenceCompetence"), Data.ReferenceCompetence, Path, Data)
		|| !NeedString(Games, TEXT("referencePreset"), Data.ReferencePreset, Path, Data)
		|| !Games.TryGetArrayField(TEXT("opponentPresets"), Opponents) || Opponents->Num() == 0
		|| !NeedNumber(Games, TEXT("fightTimeoutS"), Data.FightTimeoutS, Path, Data))
	{
		if (Data.Error.IsEmpty())
		{
			Fail(Data, Path + TEXT(" games tuning is incomplete"));
		}
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Opponents)
	{
		FString Name;
		if (!Value.IsValid() || !Value->TryGetString(Name))
		{
			return Fail(Data, Path + TEXT(" opponent preset is not a string"));
		}
		Data.OpponentPresets.Add(Name);
	}
	return true;
}

FKernelData Load()
{
	FKernelData Data;
	const FString CombatPath = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/combat.json"));
	const FString StatsPath = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/stats.csv"));
	const FString SpellsPath = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/spells-water.csv"));
	const FString EnemyPath = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/enemies.json"));
	const FString ScalePath = PinnedPath(TEXT("art/scale-contract-v1.json"));
	const FString RuntimePath = PinnedPath(TEXT("packages/core/src/arena/data/runtime.json"));

	FString AbsorbError;
	if (!Data.Absorb.LoadFromPinnedFile(AbsorbError))
	{
		Fail(Data, AbsorbError);
		return Data;
	}
	bool bPhysical = false;
	bool bUnblockable = false;
	bool bMagic = false;
	for (const FString& Entry : Data.Absorb.NotAgainst)
	{
		bPhysical |= Entry == TEXT("physical");
		bUnblockable |= Entry == TEXT("unblockable");
		bMagic |= Entry == TEXT("magic");
	}
	// kernel.ts:57 hard-codes family === 'magic'. BlocksPerfect matches only while notAgainst is the other two families.
	if (Data.Absorb.NotAgainst.Num() != AbsorbNotAgainstCount || !bPhysical || !bUnblockable || bMagic)
	{
		Fail(Data, TEXT("kernel data: perfect.notAgainst must be exactly physical and unblockable"));
		return Data;
	}

	TSharedPtr<FJsonObject> Combat;
	if (!LoadJson(CombatPath, Combat, Data))
	{
		return Data;
	}
	const FJsonObject* Movement = nullptr;
	const FJsonObject* Roll = nullptr;
	const FJsonObject* Staff = nullptr;
	const FJsonObject* Stamina = nullptr;
	const FJsonObject* Clock = nullptr;
	const FJsonObject* Unlock = nullptr;
	const FJsonObject* Lines = nullptr;
	const FJsonObject* Flow = nullptr;
	const FJsonObject* Crest = nullptr;
	const FJsonObject* Bands = nullptr;
	const FJsonObject* Waves = nullptr;
	const FJsonObject* Pacing = nullptr;
	if (!NeedNumber(*Combat, TEXT("simStepHz"), Data.SimStepHz, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("movement"), Movement, CombatPath, Data)
		|| !NeedNumber(*Movement, TEXT("walkMps"), Data.WalkMps, CombatPath, Data)
		|| !NeedNumber(*Movement, TEXT("sprintMps"), Data.SprintMps, CombatPath, Data)
		|| !NeedNumber(*Movement, TEXT("sprintStaminaPerSecond"), Data.SprintStaminaPerSecond, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("roll"), Roll, CombatPath, Data)
		|| !NeedNumber(*Roll, TEXT("distanceM"), Data.RollDistanceM, CombatPath, Data)
		|| !NeedNumber(*Roll, TEXT("durationS"), Data.RollDurationS, CombatPath, Data)
		|| !NeedNumber(*Roll, TEXT("iFramesS"), Data.RollIFramesS, CombatPath, Data)
		|| !NeedNumber(*Roll, TEXT("staminaCost"), Data.RollStaminaCost, CombatPath, Data)
		|| !NeedNumber(*Roll, TEXT("recoveryS"), Data.RollRecoveryS, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("staffStrike"), Staff, CombatPath, Data)
		|| !NeedNumber(*Staff, TEXT("rangeM"), Data.StaffRangeM, CombatPath, Data)
		|| !NeedNumber(*Staff, TEXT("windupS"), Data.StaffWindupS, CombatPath, Data)
		|| !NeedNumber(*Staff, TEXT("recoveryS"), Data.StaffRecoveryS, CombatPath, Data)
		|| !NeedNumber(*Staff, TEXT("staminaCost"), Data.StaffStaminaCost, CombatPath, Data)
		|| !NeedNumber(*Staff, TEXT("damage"), Data.StaffDamage, CombatPath, Data)
		|| !NeedBool(*Staff, TEXT("interruptsCasts"), Data.bStaffInterrupts, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("stamina"), Stamina, CombatPath, Data)
		|| !NeedNumber(*Stamina, TEXT("regenPerSecond"), Data.StaminaRegenPerSecond, CombatPath, Data)
		|| !NeedNumber(*Stamina, TEXT("regenDelayS"), Data.StaminaRegenDelayS, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("tierClock"), Clock, CombatPath, Data)
		|| !NeedObject(*Clock, TEXT("unlockAtSeconds"), Unlock, CombatPath, Data)
		|| !NeedNumber(*Clock, TEXT("perfectAbsorbAdvanceS"), Data.TierClockAdvanceS, CombatPath, Data)
		|| !NeedNumber(*Clock, TEXT("minimumSecondsBetweenUnlocks"), Data.MinimumSecondsBetweenUnlocks, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("lines"), Lines, CombatPath, Data)
		|| !NeedInt(*Lines, TEXT("slots"), Data.LineSlots, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("flow"), Flow, CombatPath, Data)
		|| !NeedNumber(*Flow, TEXT("differentLineWithinS"), Data.FlowWithinS, CombatPath, Data)
		|| !NeedNumber(*Flow, TEXT("max"), Data.FlowMax, CombatPath, Data)
		|| !NeedNumber(*Flow, TEXT("perStackDamage"), Data.FlowPerStack, CombatPath, Data)
		|| !NeedBool(*Flow, TEXT("resetOnSameLineTwice"), Data.bFlowResetSameLine, CombatPath, Data)
		|| !NeedNumber(*Flow, TEXT("resetAfterIdleS"), Data.FlowIdleS, CombatPath, Data)
		|| !NeedObject(*Flow, TEXT("crest"), Crest, CombatPath, Data)
		|| !NeedNumber(*Crest, TEXT("manaCost"), Data.CrestManaCost, CombatPath, Data)
		|| !NeedNumber(*Crest, TEXT("damageMult"), Data.CrestDamageMult, CombatPath, Data)
		|| !NeedNumber(*Flow, TEXT("perfectAbsorbGives"), Data.PerfectAbsorbGives, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("reactionBands"), Bands, CombatPath, Data)
		|| !NeedNumber(*Bands, TEXT("minimumTelegraphPositionalS"), Data.MinimumTelegraphPositionalS, CombatPath, Data)
		|| !NeedNumber(*Bands, TEXT("minimumTelegraphUnblockableS"), Data.MinimumTelegraphUnblockableS, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("betweenWaves"), Waves, CombatPath, Data)
		|| !NeedNumber(*Waves, TEXT("healFractionOfMissingHp"), Data.HealFraction, CombatPath, Data)
		|| !NeedNumber(*Waves, TEXT("manaRefill"), Data.ManaRefill, CombatPath, Data)
		|| !NeedNumber(*Waves, TEXT("staminaRefill"), Data.StaminaRefill, CombatPath, Data)
		|| !NeedObject(*Combat, TEXT("pacingTargets"), Pacing, CombatPath, Data)
		|| !NeedNumber(*Pacing, TEXT("deadAirBucketS"), Data.DeadAirBucketS, CombatPath, Data))
	{
		return Data;
	}
	if (Data.TierClockAdvanceS != Data.Absorb.TierClockAdvanceS)
	{
		Fail(Data, TEXT("kernel data: clock reward contradiction"));
		return Data;
	}
	for (int32 Tier = 1; Tier <= 4; ++Tier)
	{
		if (!NeedNumber(*Unlock, *FString::FromInt(Tier), Data.UnlockAtSeconds[Tier], CombatPath, Data))
		{
			return Data;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* BranchTiers = nullptr;
	if (!Lines->TryGetArrayField(TEXT("branchAtTiers"), BranchTiers))
	{
		Fail(Data, TEXT("kernel data: lines.branchAtTiers missing"));
		return Data;
	}
	for (const TSharedPtr<FJsonValue>& Value : *BranchTiers)
	{
		Data.BranchAtTiers.Add(static_cast<int32>(std::llround(Value->AsNumber())));
	}

	FCsvTable Stats;
	if (!LoadCsv(StatsPath, Stats, Data)
		|| !LinearStat(Stats, TEXT("vigor"), TEXT("maxHp"), Data.HpBase, Data.HpPerRank, Data)
		|| !LinearStat(Stats, TEXT("vigor"), TEXT("maxStamina"), Data.StaminaBase, Data.StaminaPerRank, Data)
		|| !LinearStat(Stats, TEXT("focus"), TEXT("maxMana"), Data.ManaBase, Data.ManaPerRank, Data)
		|| !LinearStat(Stats, TEXT("focus"), TEXT("manaRegen"), Data.ManaRegenBase, Data.ManaRegenPerRank, Data))
	{
		return Data;
	}
	const TMap<FString, FString>* Nerve = nullptr;
	for (const TMap<FString, FString>& Row : Stats.Rows)
	{
		const FString* Name = Cell(Row, TEXT("stat"));
		if (Name && *Name == TEXT("nerve"))
		{
			Nerve = &Row;
		}
	}
	FString DrainA;
	FString DrainB;
	FString RefundPer;
	if (!Nerve || !MatchGroup(Data.Absorb.DrainFormula, TEXT("^(\\d+) - (\\d+) \\* nerveRank$"), DrainA))
	{
		Fail(Data, TEXT("kernel data: nerve authority contradiction"));
		return Data;
	}
	{
		const FRegexPattern RefundPattern(TEXT("^(\\d+) \\* incomingTier \\* \\(1 \\+ ([\\d.]+) \\* nerveRank\\)"));
		FRegexMatcher RefundMatcher(RefundPattern, Data.Absorb.ManaFormula);
		if (!RefundMatcher.FindNext())
		{
			Fail(Data, TEXT("kernel data: nerve authority contradiction"));
			return Data;
		}
		DrainB = [&]() { FString Second; MatchGroup(Data.Absorb.DrainFormula, TEXT("^\\d+ - (\\d+) \\* nerveRank$"), Second); return Second; }();
		RefundPer = RefundMatcher.GetCaptureGroup(2);
	}
	const FString Compact = Cell(*Nerve, TEXT("arena_effect"))->Replace(TEXT(" "), TEXT(""));
	if (!Compact.Contains(FString::Printf(TEXT("absorbDrain=%s-%s*rank"), *DrainA, *DrainB))
		|| !Compact.Contains(FString::Printf(TEXT("perfectReturnMult=1+%s*rank"), *RefundPer)))
	{
		Fail(Data, TEXT("kernel data: nerve authority contradiction"));
		return Data;
	}

	TSharedPtr<FJsonObject> Scale;
	if (!LoadJson(ScalePath, Scale, Data))
	{
		return Data;
	}
	const TArray<TSharedPtr<FJsonValue>>* Metres = nullptr;
	const FJsonObject* ScaleAbsorb = nullptr;
	double ScaleArc = 0.0;
	if (!Scale->TryGetArrayField(TEXT("arena_metres"), Metres) || Metres->Num() < 2
		|| !NeedNumber(*Scale, TEXT("minimum_combatant_centre_separation_metres"), Data.OpeningSeparationM, ScalePath, Data)
		|| !NeedObject(*Scale, TEXT("absorb"), ScaleAbsorb, ScalePath, Data)
		|| !NeedNumber(*ScaleAbsorb, TEXT("angle_degrees"), ScaleArc, ScalePath, Data))
	{
		if (Data.Error.IsEmpty())
		{
			Fail(Data, TEXT("kernel data: scale contract is missing arena_metres"));
		}
		return Data;
	}
	Data.ArenaWidthM = (*Metres)[0]->AsNumber();
	Data.ArenaHeightM = (*Metres)[1]->AsNumber();
	if (ScaleArc != Data.Absorb.ArcDeg)
	{
		Fail(Data, TEXT("kernel data: absorb contract contradiction"));
		return Data;
	}

	TSharedPtr<FJsonObject> Runtime;
	if (!LoadJson(RuntimePath, Runtime, Data))
	{
		return Data;
	}
	const FJsonObject* Geometry = nullptr;
	const FJsonObject* Training = nullptr;
	const FJsonObject* Magic = nullptr;
	const FJsonObject* Physical = nullptr;
	const FJsonObject* Charge = nullptr;
	const FJsonObject* Field = nullptr;
	const FJsonObject* Ranks = nullptr;
	const FJsonObject* Water = nullptr;
	const FJsonObject* Games = nullptr;
	const FJsonObject* Presentation = nullptr;
	if (!NeedObject(*Runtime, TEXT("geometry"), Geometry, RuntimePath, Data)
		|| !NeedVec(*Geometry, TEXT("arenaCentre"), Data.ArenaCentre, RuntimePath, Data)
		|| !NeedNumber(*Geometry, TEXT("mageRadiusM"), Data.MageRadiusM, RuntimePath, Data)
		|| !NeedNumber(*Geometry, TEXT("projectileRadiusM"), Data.ProjectileRadiusM, RuntimePath, Data)
		|| !NeedNumber(*Geometry, TEXT("staffArcDeg"), Data.StaffArcDeg, RuntimePath, Data)
		|| !NeedObject(*Runtime, TEXT("training"), Training, RuntimePath, Data)
		|| !NeedObject(*Training, TEXT("defaultRanks"), Ranks, RuntimePath, Data)
		|| !NeedInt(*Ranks, TEXT("vigor"), Data.DefaultRanks.Vigor, RuntimePath, Data)
		|| !NeedInt(*Ranks, TEXT("focus"), Data.DefaultRanks.Focus, RuntimePath, Data)
		|| !NeedInt(*Ranks, TEXT("nerve"), Data.DefaultRanks.Nerve, RuntimePath, Data)
		|| !NeedVec(*Training, TEXT("player"), Data.TrainingPlayer, RuntimePath, Data)
		|| !NeedNumber(*Training, TEXT("dummyHp"), Data.DummyHp, RuntimePath, Data)
		|| !NeedVec(*Training, TEXT("front"), Data.TrainingFront, RuntimePath, Data)
		|| !NeedVec(*Training, TEXT("side"), Data.TrainingSide, RuntimePath, Data)
		|| !NeedNumber(*Training, TEXT("attackIntervalS"), Data.AttackIntervalS, RuntimePath, Data)
		|| !NeedNumber(*Training, TEXT("firstAttackS"), Data.FirstAttackS, RuntimePath, Data)
		|| !NeedObject(*Training, TEXT("magic"), Magic, RuntimePath, Data)
		|| !NeedNumber(*Magic, TEXT("damage"), Data.MagicDamage, RuntimePath, Data)
		|| !NeedInt(*Magic, TEXT("tier"), Data.MagicTier, RuntimePath, Data)
		|| !NeedNumber(*Magic, TEXT("windupS"), Data.MagicWindupS, RuntimePath, Data)
		|| !NeedNumber(*Magic, TEXT("speedMps"), Data.MagicSpeedMps, RuntimePath, Data)
		|| !NeedNumber(*Magic, TEXT("rangeM"), Data.MagicRangeM, RuntimePath, Data)
		|| !NeedObject(*Training, TEXT("physical"), Physical, RuntimePath, Data)
		|| !NeedNumber(*Physical, TEXT("damage"), Data.PhysicalDamage, RuntimePath, Data)
		|| !NeedInt(*Physical, TEXT("tier"), Data.PhysicalTier, RuntimePath, Data)
		|| !NeedNumber(*Physical, TEXT("windupS"), Data.PhysicalWindupS, RuntimePath, Data)
		|| !NeedNumber(*Physical, TEXT("speedMps"), Data.PhysicalSpeedMps, RuntimePath, Data)
		|| !NeedNumber(*Physical, TEXT("rangeM"), Data.PhysicalRangeM, RuntimePath, Data)
		|| !NeedObject(*Training, TEXT("charge"), Charge, RuntimePath, Data)
		|| !NeedNumber(*Charge, TEXT("damage"), Data.ChargeDamage, RuntimePath, Data)
		|| !NeedNumber(*Charge, TEXT("windupS"), Data.ChargeWindupS, RuntimePath, Data)
		|| !NeedNumber(*Charge, TEXT("rangeM"), Data.ChargeRangeM, RuntimePath, Data)
		|| !NeedNumber(*Charge, TEXT("widthM"), Data.ChargeWidthM, RuntimePath, Data)
		|| !NeedInt(*Training, TEXT("streamCount"), Data.StreamCount, RuntimePath, Data)
		|| !NeedNumber(*Training, TEXT("streamIntervalS"), Data.StreamIntervalS, RuntimePath, Data)
		|| !NeedNumber(*Training, TEXT("perfectBotLeadS"), Data.PerfectBotLeadS, RuntimePath, Data)
		|| !NeedNumber(*Training, TEXT("lateBotLeadS"), Data.LateBotLeadS, RuntimePath, Data)
		|| !NeedNumber(*Training, TEXT("reportDurationS"), Data.ReportDurationS, RuntimePath, Data)
		|| !NeedInt(*Training, TEXT("performanceProjectiles"), Data.PerformanceProjectiles, RuntimePath, Data)
		|| !NeedObject(*Training, TEXT("performanceField"), Field, RuntimePath, Data)
		|| !NeedNumber(*Field, TEXT("widthM"), Data.FieldWidthM, RuntimePath, Data)
		|| !NeedNumber(*Field, TEXT("heightM"), Data.FieldHeightM, RuntimePath, Data)
		|| !NeedNumber(*Field, TEXT("speedMps"), Data.FieldSpeedMps, RuntimePath, Data)
		|| !NeedNumber(*Field, TEXT("rangeM"), Data.FieldRangeM, RuntimePath, Data)
		|| !NeedObject(*Runtime, TEXT("water"), Water, RuntimePath, Data)
		|| !NeedNumber(*Water, TEXT("fanSpeedMps"), Data.FanSpeedMps, RuntimePath, Data)
		|| !NeedNumber(*Water, TEXT("returnWaveSpeedMps"), Data.ReturnWaveSpeedMps, RuntimePath, Data)
		|| !NeedNumber(*Water, TEXT("returnWaveRadiusM"), Data.ReturnWaveRadiusM, RuntimePath, Data)
		|| !NeedNumber(*Water, TEXT("tombCursorRadiusM"), Data.TombCursorRadiusM, RuntimePath, Data)
		|| !NeedObject(*Runtime, TEXT("games"), Games, RuntimePath, Data)
		|| !NeedVec(*Games, TEXT("enemySpawn"), Data.EnemySpawn, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("spawnJitterM"), Data.SpawnJitterM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("spawnMarginM"), Data.SpawnMarginM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("defaultMeleeRangeM"), Data.DefaultMeleeRangeM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("meleeArcDeg"), Data.MeleeArcDeg, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("defaultRecoveryS"), Data.DefaultRecoveryS, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("rangedRangeM"), Data.RangedRangeM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("netRangeM"), Data.NetRangeM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("tongueRangeM"), Data.TongueRangeM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("tongueWidthM"), Data.TongueWidthM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("mawArtilleryCooldownS"), Data.MawArtilleryCooldownS, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("chargeCooldownS"), Data.ChargeCooldownS, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("contactRangeM"), Data.ContactRangeM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("houndOrbitM"), Data.HoundOrbitM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("conscriptBackoffM"), Data.ConscriptBackoffM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("separationM"), Data.SeparationM, RuntimePath, Data)
		|| !NeedNumber(*Games, TEXT("separationWeight"), Data.SeparationWeight, RuntimePath, Data)
		|| !NeedObject(*Runtime, TEXT("presentation"), Presentation, RuntimePath, Data)
		|| !NeedNumber(*Presentation, TEXT("maxFrameDeltaS"), Data.MaxFrameDeltaS, RuntimePath, Data))
	{
		return Data;
	}
	if (!LoadGamesTuning(*Games, Data, RuntimePath))
	{
		return Data;
	}
	const TArray<TSharedPtr<FJsonValue>>* Presets = nullptr;
	if (!Water->TryGetArrayField(TEXT("presets"), Presets) || Presets->Num() == 0)
	{
		Fail(Data, TEXT("kernel data: runtime water.presets missing"));
		return Data;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Presets)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
		{
			Fail(Data, TEXT("kernel data: preset is not an object"));
			return Data;
		}
		FComposition Composition;
		const FJsonObject* Branches = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* LineValues = nullptr;
		if (!NeedString(**Object, TEXT("name"), Composition.Name, RuntimePath, Data)
			|| !(*Object)->TryGetArrayField(TEXT("lines"), LineValues)
			|| !NeedObject(**Object, TEXT("branches"), Branches, RuntimePath, Data)
			|| !NeedString(*Branches, TEXT("lash"), Composition.Branches.Lash, RuntimePath, Data)
			|| !NeedString(*Branches, TEXT("mirror"), Composition.Branches.Mirror, RuntimePath, Data)
			|| !NeedString(*Branches, TEXT("tide_orb"), Composition.Branches.TideOrb, RuntimePath, Data))
		{
			if (Data.Error.IsEmpty())
			{
				Fail(Data, TEXT("kernel data: preset lines missing"));
			}
			return Data;
		}
		for (const TSharedPtr<FJsonValue>& LineValue : *LineValues)
		{
			FString Line;
			if (!LineValue.IsValid() || !LineValue->TryGetString(Line))
			{
				Fail(Data, TEXT("kernel data: preset line is not a string"));
				return Data;
			}
			Composition.Lines.Add(Line);
		}
		Data.Presets.Add(MoveTemp(Composition));
	}

	FCsvTable SpellRows;
	if (!LoadCsv(SpellsPath, SpellRows, Data) || !BuildSpells(SpellRows, Data))
	{
		return Data;
	}
	TSharedPtr<FJsonObject> Enemies;
	if (!LoadJson(EnemyPath, Enemies, Data) || !LoadEnemies(*Enemies, Data, EnemyPath) || !LoadArenaTiers(Data) || !LoadFireSpells(Data) || !LoadHeat(Data))
	{
		return Data;
	}
	Data.bReady = true;
	return Data;
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

const FKernelData& KernelData()
{
	static const FKernelData Loaded = Load();
	return Loaded;
}

FString KernelDataPinnedPath(const TCHAR* Relative)
{
	return PinnedPath(Relative);
}

FKernelData LoadKernelDataWithOverrides(const TMap<FString, FString>& Overrides)
{
	GTextOverrides = &Overrides;
	FKernelData Data = Load();
	GTextOverrides = nullptr;
	return Data;
}

const TArray<FString>& WaterLines()
{
	static const TArray<FString> Lines = {TEXT("tide_orb"), TEXT("lash"), TEXT("mire"), TEXT("mend"), TEXT("mirror")};
	return Lines;
}
