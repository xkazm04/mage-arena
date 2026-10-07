#include "Kernel/VrRules.h"

#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"
#include "MageArenaVR.h"

#include "Kernel/SimConstants.h"
#include "Kernel/SimMath.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>
#include <initializer_list>

namespace
{
FString GVrDataDirOverride;

FString VrFile(const TCHAR* Name)
{
	if (!GVrDataDirOverride.IsEmpty())
	{
		return FPaths::Combine(GVrDataDirOverride, Name);
	}
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr"), Name));
}

bool Fail(FString& Error, const FString& Message)
{
	Error = Message;
	return false;
}

bool LoadJsonFile(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& Error)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: cannot read %s"), *Path));
	}
	if (Text.Len() > 0 && Text[0] == 0xFEFF)
	{
		Text.RemoveAt(0, 1, EAllowShrinking::No);
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: %s is not JSON"), *Path));
	}
	return true;
}

bool NeedNumber(const FJsonObject& Object, const TCHAR* Field, double& Out, FString& Error)
{
	if (!Object.TryGetNumberField(Field, Out))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: missing number %s"), Field));
	}
	return true;
}

bool NeedBool(const FJsonObject& Object, const TCHAR* Field, bool& Out, FString& Error)
{
	if (!Object.TryGetBoolField(Field, Out))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: missing bool %s"), Field));
	}
	return true;
}

bool NeedString(const FJsonObject& Object, const TCHAR* Field, FString& Out, FString& Error)
{
	if (!Object.TryGetStringField(Field, Out))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: missing string %s"), Field));
	}
	return true;
}

bool NeedObject(const FJsonObject& Object, const TCHAR* Field, const FJsonObject*& Out, FString& Error)
{
	const TSharedPtr<FJsonObject>* Found = nullptr;
	if (!Object.TryGetObjectField(Field, Found) || !Found || !Found->IsValid())
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: missing object %s"), Field));
	}
	Out = Found->Get();
	return true;
}

bool NeedArray2(const FJsonObject& Object, const TCHAR* Field, double& X, double& Y, FString& Error)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.TryGetArrayField(Field, Values) || !Values || Values->Num() < 2
		|| !(*Values)[0].IsValid() || !(*Values)[1].IsValid()
		|| (*Values)[0]->Type != EJson::Number || (*Values)[1]->Type != EJson::Number)
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: %s needs two numbers"), Field));
	}
	X = (*Values)[0]->AsNumber();
	Y = (*Values)[1]->AsNumber();
	return true;
}

bool Near(double Left, double Right)
{
	return std::abs(Left - Right) <= 1.0e-6;
}

const FEnemySpec* FindEnemy(const FString& Id)
{
	for (const FEnemySpec& Spec : KernelData().Enemies)
	{
		if (Spec.Id == Id)
		{
			return &Spec;
		}
	}
	return nullptr;
}

const FAttackSpec* FindAttack(const FEnemySpec& Spec, const FString& Id)
{
	for (const FAttackSpec& Attack : Spec.Attacks)
	{
		if (Attack.Id == Id)
		{
			return &Attack;
		}
	}
	return nullptr;
}

bool ReadLayoutMeasure(const FJsonObject& Layout, double& CentreX, double& CentreY, double& SizeX, double& SizeY,
	double& HeightM, double& PadX, double& PadY, double& ChestM, double& SteelLengthM, double& SteelRadiusM, FString& Error)
{
	const FJsonObject* Dais = nullptr;
	if (!NeedObject(Layout, TEXT("dais"), Dais, Error)
		|| !NeedArray2(*Dais, TEXT("centreM"), CentreX, CentreY, Error)
		|| !NeedArray2(*Dais, TEXT("sizeM"), SizeX, SizeY, Error)
		|| !NeedNumber(*Dais, TEXT("heightM"), HeightM, Error))
	{
		return false;
	}
	const FJsonObject* Pads = nullptr;
	if (!NeedObject(Layout, TEXT("pads"), Pads, Error))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Pads->TryGetArrayField(TEXT("items"), Items) || !Items)
	{
		return Fail(Error, TEXT("vr rules: layout pads.items is missing"));
	}
	bool bFoundPad = false;
	for (const TSharedPtr<FJsonValue>& Value : *Items)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Item.IsValid())
		{
			continue;
		}
		FString Id;
		if (!Item->TryGetStringField(TEXT("id"), Id) || Id != TEXT("centre"))
		{
			continue;
		}
		if (!NeedArray2(*Item, TEXT("positionM"), PadX, PadY, Error))
		{
			return false;
		}
		bFoundPad = true;
		break;
	}
	if (!bFoundPad)
	{
		return Fail(Error, TEXT("vr rules: layout has no centre pad"));
	}
	const FJsonObject* Threats = nullptr;
	if (!NeedObject(Layout, TEXT("threats"), Threats, Error) || !NeedNumber(*Threats, TEXT("chestHeightAboveSandM"), ChestM, Error))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Order = nullptr;
	if (!Threats->TryGetArrayField(TEXT("order"), Order) || !Order)
	{
		return Fail(Error, TEXT("vr rules: layout threats.order is missing"));
	}
	bool bSteel = false;
	for (const TSharedPtr<FJsonValue>& Value : *Order)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Item.IsValid())
		{
			continue;
		}
		FString Kind;
		if (!Item->TryGetStringField(TEXT("kind"), Kind) || Kind != TEXT("steel"))
		{
			continue;
		}
		if (!NeedNumber(*Item, TEXT("lengthM"), SteelLengthM, Error) || !NeedNumber(*Item, TEXT("radiusM"), SteelRadiusM, Error))
		{
			return false;
		}
		bSteel = true;
		break;
	}
	if (!bSteel)
	{
		return Fail(Error, TEXT("vr rules: layout has no steel threat"));
	}
	return true;
}

bool ReadThrow(const FJsonObject& Attack, const FEnemySpec& Conscript, const FAttackSpec& Spear, const FAttackSpec& Stone,
	double HeightM, double ChestM, double SteelLengthM, double SteelRadiusM, FVrAttackMode& Mode, FString& Error)
{
	FString ModeName;
	FString AttackId;
	if (!NeedString(Attack, TEXT("mode"), ModeName, Error) || !NeedString(Attack, TEXT("attackId"), AttackId, Error))
	{
		return false;
	}
	if (ModeName != TEXT("throw") || AttackId != Spear.Id)
	{
		return Fail(Error, TEXT("vr rules: conscript mode must be throw of spear_lunge"));
	}
	if (Attack.HasField(TEXT("recoveryS")) || Attack.HasField(TEXT("cooldownS")))
	{
		return Fail(Error, TEXT("vr rules: conscript recovery and cooldown stay on the pinned row"));
	}
	double Damage = 0.0;
	double Windup = 0.0;
	if (!NeedNumber(Attack, TEXT("damage"), Damage, Error) || !NeedNumber(Attack, TEXT("windupS"), Windup, Error)
		|| !NeedNumber(Attack, TEXT("projectileMps"), Mode.ProjectileMps, Error)
		|| !NeedNumber(Attack, TEXT("rangeM"), Mode.RangeM, Error)
		|| !NeedNumber(Attack, TEXT("arcApexM"), Mode.ArcApexM, Error)
		|| !NeedNumber(Attack, TEXT("spearLengthM"), Mode.SpearLengthM, Error)
		|| !NeedNumber(Attack, TEXT("spearRadiusM"), Mode.SpearRadiusM, Error))
	{
		return false;
	}
	if (!Near(Damage, Spear.Damage) || !Near(Windup, Spear.WindupS))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: conscript damage/windup must stay pinned (%.4f / %.4f)"), Spear.Damage, Spear.WindupS));
	}
	if (!Spear.RangeM.IsSet() || !Near(Spear.RangeM.GetValue(), 2.0))
	{
		return Fail(Error, TEXT("vr rules: pinned spear_lunge rangeM is not 2"));
	}
	if (Stone.ProjectileMps.IsSet() == false || !Near(Mode.ProjectileMps, Stone.ProjectileMps.GetValue()))
	{
		return Fail(Error, TEXT("vr rules: thrown spear speed must match the pinned sling stone"));
	}
	if (!Near(Mode.ArcApexM, ChestM - HeightM))
	{
		return Fail(Error, TEXT("vr rules: arcApexM must be the layout chest height minus the dais height"));
	}
	if (!Near(Mode.SpearLengthM, SteelLengthM) || !Near(Mode.SpearRadiusM, SteelRadiusM))
	{
		return Fail(Error, TEXT("vr rules: spear mesh must match the layout steel threat"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Replaces = nullptr;
	if (!Attack.TryGetArrayField(TEXT("replaces"), Replaces) || !Replaces)
	{
		return Fail(Error, TEXT("vr rules: conscript throw must list replaces"));
	}
	bool bRange = false;
	bool bSpeed = false;
	for (const TSharedPtr<FJsonValue>& Value : *Replaces)
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Row.IsValid())
		{
			return Fail(Error, TEXT("vr rules: replaces row is not an object"));
		}
		FString Field;
		double Overlay = 0.0;
		if (!NeedString(*Row, TEXT("field"), Field, Error) || !NeedNumber(*Row, TEXT("overlay"), Overlay, Error))
		{
			return false;
		}
		if (Field == TEXT("rangeM"))
		{
			double Pinned = 0.0;
			if (!NeedNumber(*Row, TEXT("pinned"), Pinned, Error) || !Near(Pinned, Spear.RangeM.GetValue()) || !Near(Overlay, Mode.RangeM))
			{
				return Fail(Error, TEXT("vr rules: rangeM replace does not match the pinned lunge and the overlay range"));
			}
			bRange = true;
		}
		else if (Field == TEXT("projectileMps"))
		{
			const TSharedPtr<FJsonValue> PinnedField = Row->TryGetField(TEXT("pinned"));
			if (!PinnedField.IsValid() || PinnedField->Type != EJson::Null || !Near(Overlay, Mode.ProjectileMps))
			{
				return Fail(Error, TEXT("vr rules: projectileMps replace must name a null pinned speed and the overlay speed"));
			}
			bSpeed = true;
		}
		else
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: unknown replace %s"), *Field));
		}
	}
	if (!bRange || !bSpeed)
	{
		return Fail(Error, TEXT("vr rules: conscript replaces must name rangeM and projectileMps"));
	}
	Mode.bThrow = true;
	Mode.EnemyId = Conscript.Id;
	return true;
}

bool OnlyKnownKeys(const FJsonObject& Object, std::initializer_list<const TCHAR*> Known, const TCHAR* Where, FString& Error)
{
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object.Values)
	{
		if (Pair.Key.StartsWith(TEXT("_")))
		{
			continue;
		}
		bool bKnown = false;
		for (const TCHAR* Name : Known)
		{
			bKnown |= Pair.Key == Name;
		}
		if (!bKnown)
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s has an unknown key %s"), Where, *Pair.Key));
		}
	}
	return true;
}

bool WholeNumber(double Value, double Min, double Max, int32& Out)
{
	const double Whole = std::round(Value);
	if (std::abs(Value - Whole) > 1.0e-9 || Whole < Min || Whole > Max)
	{
		return false;
	}
	Out = static_cast<int32>(Whole);
	return true;
}

// DECISIONS 2026-10-07 (DF-003 answered). Absent block: no rival, today's duel.
bool ReadRivals(const FJsonObject& Overlay, TArray<FVrRival>& Out, FString& Error)
{
	Out.Reset();
	if (!Overlay.HasField(TEXT("rivals")))
	{
		return true;
	}
	const FJsonObject* Block = nullptr;
	if (!NeedObject(Overlay, TEXT("rivals"), Block, Error))
	{
		return Fail(Error, TEXT("vr rules: rivals must be an object"));
	}
	const FKernelData& Data = KernelData();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Block->Values)
	{
		if (Pair.Key.StartsWith(TEXT("_")))
		{
			continue;
		}
		const TSharedPtr<FJsonObject> Entry = Pair.Value.IsValid() && Pair.Value->Type == EJson::Object ? Pair.Value->AsObject() : nullptr;
		if (!Entry.IsValid())
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: rival %s is not an object"), *Pair.Key));
		}
		const FString Where = FString::Printf(TEXT("rival %s"), *Pair.Key);
		if (!OnlyKnownKeys(*Entry, {TEXT("school"), TEXT("appliesTo"), TEXT("phases"), TEXT("surgeTiers")}, *Where, Error))
		{
			return false;
		}
		FVrRival Rival;
		Rival.Id = Pair.Key;
		const FJsonObject* Applies = nullptr;
		double Surge = 0.0;
		if (!NeedString(*Entry, TEXT("school"), Rival.School, Error) || !NeedObject(*Entry, TEXT("appliesTo"), Applies, Error)
			|| !NeedNumber(*Entry, TEXT("surgeTiers"), Surge, Error))
		{
			return false;
		}
		if (Rival.School != TEXT("fire") && Rival.School != TEXT("water"))
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s school must be fire or water (the kernel schools)"), *Where));
		}
		if (!WholeNumber(Surge, 0.0, static_cast<double>(TierCap - 1), Rival.SurgeTiers))
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s surgeTiers must be an integer from 0 to %d"), *Where, TierCap - 1));
		}
		double WaveN = 0.0;
		if (!OnlyKnownKeys(*Applies, {TEXT("tier"), TEXT("waveN"), TEXT("kind")}, *Where, Error)
			|| !NeedString(*Applies, TEXT("tier"), Rival.TierId, Error) || !NeedNumber(*Applies, TEXT("waveN"), WaveN, Error)
			|| !NeedString(*Applies, TEXT("kind"), Rival.WaveKind, Error))
		{
			return false;
		}
		const FArenaTier* Tier = Data.ArenaTiers.FindByPredicate([&](const FArenaTier& Candidate) { return Candidate.Id == Rival.TierId; });
		if (!Tier)
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s appliesTo.tier %s is not a pinned arena tier"), *Where, *Rival.TierId));
		}
		if (!WholeNumber(WaveN, 1.0, static_cast<double>(Tier->Waves.Num()), Rival.WaveN))
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s appliesTo.waveN must be a wave of %s"), *Where, *Rival.TierId));
		}
		const FArenaWave& Wave = Tier->Waves[Rival.WaveN - 1];
		if (Wave.N != Rival.WaveN || Wave.Kind != Rival.WaveKind)
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s appliesTo.kind %s is not wave %d's kind"), *Where, *Rival.WaveKind, Rival.WaveN));
		}
		if (!Wave.Spawns.ContainsByPredicate([](const FWaveSpawn& Spawn) { return !Spawn.bEnemy; }))
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s appliesTo names a wave with no mage"), *Where));
		}
		const TArray<TSharedPtr<FJsonValue>>* Phases = nullptr;
		if (!Entry->TryGetArrayField(TEXT("phases"), Phases) || !Phases || Phases->Num() < 1 || Phases->Num() > 2)
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s phases must list one or two breaks (two or three phases)"), *Where));
		}
		double Previous = 1.0;
		int32 Signatures = 0;
		for (const TSharedPtr<FJsonValue>& Value : *Phases)
		{
			const TSharedPtr<FJsonObject> PhaseJson = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
			if (!PhaseJson.IsValid())
			{
				return Fail(Error, FString::Printf(TEXT("vr rules: %s phase is not an object"), *Where));
			}
			FVrRivalPhase Phase;
			if (!OnlyKnownKeys(*PhaseJson, {TEXT("atHpFraction"), TEXT("opensWith")}, *Where, Error)
				|| !NeedNumber(*PhaseJson, TEXT("atHpFraction"), Phase.AtHpFraction, Error))
			{
				return false;
			}
			if (!(Phase.AtHpFraction > 0.0 && Phase.AtHpFraction < Previous))
			{
				return Fail(Error, FString::Printf(TEXT("vr rules: %s atHpFraction must be in (0, 1) and fall phase by phase"), *Where));
			}
			Previous = Phase.AtHpFraction;
			if (PhaseJson->HasField(TEXT("opensWith")))
			{
				if (!NeedString(*PhaseJson, TEXT("opensWith"), Phase.OpensWith, Error))
				{
					return false;
				}
				if (Rival.School != TEXT("fire") || !FindFireSpell(Phase.OpensWith))
				{
					return Fail(Error, FString::Printf(TEXT("vr rules: %s opensWith %s is not a pinned fire spell"), *Where, *Phase.OpensWith));
				}
				++Signatures;
			}
			Rival.Phases.Add(Phase);
		}
		if (Signatures > 1)
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s may open only one phase with a signature"), *Where));
		}
		for (const FVrRival& Other : Out)
		{
			if (Other.TierId == Rival.TierId && Other.WaveN == Rival.WaveN && Other.School == Rival.School)
			{
				return Fail(Error, FString::Printf(TEXT("vr rules: %s and %s apply to the same mage"), *Other.Id, *Rival.Id));
			}
		}
		Out.Add(Rival);
	}
	return true;
}

bool ProposalRequested()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("MageArenaProposal")))
	{
		return true;
	}
	const FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("MageArenaProposal"));
	return Env == TEXT("1") || Env.Equals(TEXT("true"), ESearchCase::IgnoreCase);
}

bool InRange(double Value, double Min, double Max, bool bExclusiveMin)
{
	if (bExclusiveMin)
	{
		return Value > Min && Value <= Max;
	}
	return Value >= Min && Value <= Max;
}

bool ReadOptional(const FJsonObject& Object, const TCHAR* Field, double& Slot, FString& Error, double Min, double Max, bool bExclusiveMin)
{
	if (!Object.HasField(Field))
	{
		return true;
	}
	double Value = 0.0;
	if (!Object.TryGetNumberField(Field, Value) || !InRange(Value, Min, Max, bExclusiveMin))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: proposal %s is missing or out of range"), Field));
	}
	Slot = Value;
	return true;
}

bool ReadCount(const FJsonObject& Object, const TCHAR* Field, int32& Slot, FString& Error)
{
	if (!Object.HasField(Field))
	{
		return true;
	}
	double Value = 0.0;
	if (!Object.TryGetNumberField(Field, Value))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: proposal %s is not a number"), Field));
	}
	const double Whole = std::round(Value);
	if (std::abs(Value - Whole) > 1.0e-6 || Whole < -1.0 || Whole > 8.0)
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: proposal %s must be an integer from -1 to 8"), Field));
	}
	Slot = static_cast<int32>(Whole);
	return true;
}

bool ApplyProposal(FVrRuleset& Out, FString& Error)
{
	const FString Path = VrFile(TEXT("calibration-proposal.json"));
	TSharedPtr<FJsonObject> Root;
	if (!LoadJsonFile(Path, Root, Error))
	{
		return Fail(Error, TEXT("vr rules: -MageArenaProposal is set and calibration-proposal.json could not be read"));
	}
	const FJsonObject* Knobs = nullptr;
	if (!NeedObject(*Root, TEXT("knobs"), Knobs, Error))
	{
		return false;
	}
	const FJsonObject* Wave = nullptr;
	const FJsonObject* Pressure = nullptr;
	const FJsonObject* Power = nullptr;
	const FJsonObject* Defence = nullptr;
	if (!NeedObject(*Knobs, TEXT("wave"), Wave, Error) || !NeedObject(*Knobs, TEXT("pressure"), Pressure, Error)
		|| !NeedObject(*Knobs, TEXT("power"), Power, Error) || !NeedObject(*Knobs, TEXT("defence"), Defence, Error))
	{
		return false;
	}
	if (!ReadCount(*Wave, TEXT("conscriptCount"), Out.Wave.ConscriptCount, Error)
		|| !ReadCount(*Wave, TEXT("slingerCount"), Out.Wave.SlingerCount, Error)
		|| !ReadOptional(*Wave, TEXT("spawnDelayS"), Out.Wave.SpawnDelayS, Error, 0.0, 20.0, false))
	{
		return false;
	}
	if (!ReadOptional(*Pressure, TEXT("attackCadence"), Out.Pressure.AttackCadence, Error, 0.0, 3.0, true)
		|| !ReadOptional(*Pressure, TEXT("enemyDamage"), Out.Pressure.EnemyDamage, Error, 0.0, 3.0, true)
		|| !ReadOptional(*Pressure, TEXT("fireMageDamage"), Out.Pressure.FireMageDamage, Error, 0.0, 3.0, true))
	{
		return false;
	}
	if (!ReadOptional(*Power, TEXT("boltDamage"), Out.Power.BoltDamage, Error, 0.0, 3.0, true)
		|| !ReadOptional(*Power, TEXT("lineDamage"), Out.Power.LineDamage, Error, 0.0, 3.0, true)
		|| !ReadOptional(*Power, TEXT("manaRegen"), Out.Power.ManaRegen, Error, 0.0, 3.0, true))
	{
		return false;
	}
	if (!ReadOptional(*Defence, TEXT("wardDrainSplit"), Out.Defence.WardDrainSplit, Error, 0.0, 3.0, true)
		|| !ReadOptional(*Defence, TEXT("oneHandPower"), Out.Split.OneHandPower, Error, 0.0, 1.0, true)
		|| !ReadOptional(*Defence, TEXT("staffDurationS"), Out.Staff.DurationS, Error, 0.0, 30.0, true)
		|| !ReadOptional(*Defence, TEXT("staffManaUpFront"), Out.Staff.ManaUpFront, Error, 0.0, 80.0, false)
		|| !ReadOptional(*Defence, TEXT("staffDrainPerSecond"), Out.Staff.DrainPerSecond, Error, 0.0, 40.0, false)
		|| !ReadOptional(*Defence, TEXT("staffMagicReduction"), Out.Staff.MagicReduction, Error, 0.0, 1.0, false)
		|| !ReadOptional(*Defence, TEXT("staffPhysicalReduction"), Out.Staff.PhysicalReduction, Error, 0.0, 1.0, false)
		|| !ReadOptional(*Defence, TEXT("fireWallDistanceM"), Out.FireWall.DistanceM, Error, 1.0, 12.0, false)
		|| !ReadOptional(*Defence, TEXT("fireWallArcDeg"), Out.FireWall.ArcDeg, Error, 1.0, 180.0, false)
		|| !ReadOptional(*Defence, TEXT("fireWallDurationS"), Out.FireWall.DurationS, Error, 0.0, 30.0, true)
		|| !ReadOptional(*Defence, TEXT("fireWallManaCost"), Out.FireWall.ManaCost, Error, 0.0, 80.0, false)
		|| !ReadOptional(*Defence, TEXT("fireWallHeatPerStop"), Out.FireWall.HeatPerStop, Error, 0.0, 40.0, false)
		|| !ReadOptional(*Defence, TEXT("fireWallBurnDamage"), Out.FireWall.BurnDamage, Error, 0.0, 80.0, false))
	{
		return false;
	}
	return true;
}
}

bool ApplyVrCalibrationProposal(FVrRuleset& Out, FString& Error)
{
	if (!Out.bActive)
	{
		return Fail(Error, TEXT("vr rules: the proposal needs a loaded overlay"));
	}
	return ApplyProposal(Out, Error);
}

void SetVrDataDirForTest(const FString& Dir)
{
	GVrDataDirOverride = Dir;
}

bool LoadVrRuleset(FVrRuleset& Out, FString& Error, bool bHonorProposalSwitch)
{
	Out = FVrRuleset();
	if (!KernelData().bReady)
	{
		return Fail(Error, TEXT("vr rules: kernel data is not ready"));
	}
	TSharedPtr<FJsonObject> Overlay;
	TSharedPtr<FJsonObject> Layout;
	if (!LoadJsonFile(VrFile(TEXT("combat.vr.json")), Overlay, Error) || !LoadJsonFile(VrFile(TEXT("arena-layout.json")), Layout, Error))
	{
		return false;
	}
	double Version = 0.0;
	if (!NeedNumber(*Overlay, TEXT("version"), Version, Error) || !Near(Version, 1.0))
	{
		return Fail(Error, TEXT("vr rules: combat.vr.json version must be 1"));
	}
	const FJsonObject* Dais = nullptr;
	const FJsonObject* Movement = nullptr;
	if (!NeedObject(*Overlay, TEXT("daisNoEntry"), Dais, Error) || !NeedObject(*Overlay, TEXT("movement"), Movement, Error))
	{
		return false;
	}
	double CentreX = 0.0;
	double CentreY = 0.0;
	double HalfX = 0.0;
	double HalfY = 0.0;
	double Standoff = 0.0;
	if (!NeedArray2(*Dais, TEXT("centreM"), CentreX, CentreY, Error)
		|| !NeedArray2(*Dais, TEXT("halfExtentM"), HalfX, HalfY, Error)
		|| !NeedNumber(*Dais, TEXT("standoffM"), Standoff, Error))
	{
		return false;
	}
	double LayoutCentreX = 0.0;
	double LayoutCentreY = 0.0;
	double SizeX = 0.0;
	double SizeY = 0.0;
	double HeightM = 0.0;
	double PadX = 0.0;
	double PadY = 0.0;
	double ChestM = 0.0;
	double SteelLengthM = 0.0;
	double SteelRadiusM = 0.0;
	if (!ReadLayoutMeasure(*Layout, LayoutCentreX, LayoutCentreY, SizeX, SizeY, HeightM, PadX, PadY, ChestM, SteelLengthM, SteelRadiusM, Error))
	{
		return false;
	}
	if (!Near(CentreX, LayoutCentreX) || !Near(CentreY, LayoutCentreY)
		|| !Near(HalfX * 2.0, SizeX) || !Near(HalfY * 2.0, SizeY)
		|| !Near(Standoff, HeightM))
	{
		return Fail(Error, TEXT("vr rules: dais box or standoff does not match arena-layout.json"));
	}
	bool bCloses = false;
	if (!NeedBool(*Movement, TEXT("throwerClosesToLip"), bCloses, Error) || !bCloses)
	{
		return Fail(Error, TEXT("vr rules: throwerClosesToLip must be true"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Attacks = nullptr;
	if (!Overlay->TryGetArrayField(TEXT("attacks"), Attacks) || !Attacks)
	{
		return Fail(Error, TEXT("vr rules: attacks array is missing"));
	}
	const FEnemySpec* Conscript = FindEnemy(TEXT("conscript"));
	const FEnemySpec* Slinger = FindEnemy(TEXT("slinger"));
	if (!Conscript || !Slinger)
	{
		return Fail(Error, TEXT("vr rules: pinned roster is missing conscript or slinger"));
	}
	const FAttackSpec* Spear = FindAttack(*Conscript, TEXT("spear_lunge"));
	const FAttackSpec* Stone = FindAttack(*Slinger, TEXT("sling_stone"));
	if (!Spear || !Stone)
	{
		return Fail(Error, TEXT("vr rules: pinned spear_lunge or sling_stone is missing"));
	}
	bool bConscript = false;
	bool bSlinger = false;
	FVrAttackMode Throw;
	for (const TSharedPtr<FJsonValue>& Value : *Attacks)
	{
		const TSharedPtr<FJsonObject> Attack = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Attack.IsValid())
		{
			return Fail(Error, TEXT("vr rules: attack entry is not an object"));
		}
		FString EnemyId;
		FString ModeName;
		if (!NeedString(*Attack, TEXT("enemyId"), EnemyId, Error) || !NeedString(*Attack, TEXT("mode"), ModeName, Error))
		{
			return false;
		}
		if (EnemyId == TEXT("conscript"))
		{
			if (bConscript || !ReadThrow(*Attack, *Conscript, *Spear, *Stone, HeightM, ChestM, SteelLengthM, SteelRadiusM, Throw, Error))
			{
				return bConscript ? Fail(Error, TEXT("vr rules: conscript listed twice")) : false;
			}
			bConscript = true;
		}
		else if (EnemyId == TEXT("slinger"))
		{
			if (bSlinger)
			{
				return Fail(Error, TEXT("vr rules: slinger listed twice"));
			}
			FString AttackId;
			if (!NeedString(*Attack, TEXT("attackId"), AttackId, Error) || ModeName != TEXT("unchanged") || AttackId != Stone->Id)
			{
				return Fail(Error, TEXT("vr rules: slinger mode must be unchanged sling_stone"));
			}
			if (Attack->HasField(TEXT("projectileMps")) || Attack->HasField(TEXT("damage")) || Attack->HasField(TEXT("rangeM"))
				|| Attack->HasField(TEXT("windupS")) || Attack->HasField(TEXT("cooldownS")) || Attack->HasField(TEXT("replaces")))
			{
				return Fail(Error, TEXT("vr rules: slinger entry must not override a pinned field"));
			}
			bSlinger = true;
		}
		else if (ModeName == TEXT("throw"))
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s throw is not a wave-1 mode this overlay knows"), *EnemyId));
		}
		else if (ModeName != TEXT("unchanged"))
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s has an unknown mode"), *EnemyId));
		}
	}
	if (!bConscript || !bSlinger)
	{
		return Fail(Error, TEXT("vr rules: attacks must name conscript and slinger"));
	}
	const FSimVec Spawn = KernelData().PlayerSpawn;
	const FSimVec Centre{
		Spawn.X + (CentreX - PadX),
		Spawn.Y + (CentreY - PadY)};

	const FJsonObject* Split = nullptr;
	const FJsonObject* Planted = nullptr;
	if (!NeedObject(*Overlay, TEXT("splitHands"), Split, Error) || !NeedObject(*Overlay, TEXT("plantedStaff"), Planted, Error))
	{
		return false;
	}
	double OneHandPower = 0.0;
	double MaxTier = 0.0;
	double DurationS = 0.0;
	double ManaUpFront = 0.0;
	double Drain = 0.0;
	double MagicReduction = 0.0;
	double PhysicalReduction = 0.0;
	if (!NeedNumber(*Split, TEXT("oneHandPower"), OneHandPower, Error)
		|| !NeedNumber(*Split, TEXT("maxTier"), MaxTier, Error)
		|| !NeedNumber(*Planted, TEXT("durationS"), DurationS, Error)
		|| !NeedNumber(*Planted, TEXT("manaUpFront"), ManaUpFront, Error)
		|| !NeedNumber(*Planted, TEXT("drainPerSecond"), Drain, Error)
		|| !NeedNumber(*Planted, TEXT("magicReduction"), MagicReduction, Error)
		|| !NeedNumber(*Planted, TEXT("physicalReduction"), PhysicalReduction, Error))
	{
		return false;
	}
	const double TierRounded = std::round(MaxTier);
	if (!(OneHandPower > 0.0 && OneHandPower <= 1.0) || std::abs(MaxTier - TierRounded) > 1.0e-6
		|| TierRounded < 0.0 || TierRounded > 4.0)
	{
		return Fail(Error, TEXT("vr rules: splitHands oneHandPower must be in (0, 1] and maxTier an integer 0..4"));
	}
	if (!(DurationS > 0.0 && DurationS <= 30.0) || ManaUpFront < 0.0 || Drain < 0.0
		|| MagicReduction < 0.0 || MagicReduction > 1.0 || PhysicalReduction < 0.0 || PhysicalReduction > 1.0)
	{
		return Fail(Error, TEXT("vr rules: plantedStaff duration, mana, or reduction is out of range"));
	}
	Out.Split.bEnabled = true;
	Out.Split.OneHandPower = OneHandPower;
	Out.Split.MaxTier = static_cast<int32>(TierRounded);
	Out.Staff.bEnabled = true;
	Out.Staff.DurationS = DurationS;
	Out.Staff.ManaUpFront = ManaUpFront;
	Out.Staff.DrainPerSecond = Drain;
	Out.Staff.MagicReduction = MagicReduction;
	Out.Staff.PhysicalReduction = PhysicalReduction;

	const FJsonObject* WallJson = nullptr;
	if (!NeedObject(*Overlay, TEXT("fireWall"), WallJson, Error))
	{
		return false;
	}
	double DistanceM = 0.0;
	double ArcDeg = 0.0;
	double WallDurationS = 0.0;
	double ManaCost = 0.0;
	double HeatPerStop = 0.0;
	double BurnDamage = 0.0;
	if (!NeedNumber(*WallJson, TEXT("distanceM"), DistanceM, Error)
		|| !NeedNumber(*WallJson, TEXT("arcDeg"), ArcDeg, Error)
		|| !NeedNumber(*WallJson, TEXT("durationS"), WallDurationS, Error)
		|| !NeedNumber(*WallJson, TEXT("manaCost"), ManaCost, Error)
		|| !NeedNumber(*WallJson, TEXT("heatPerStop"), HeatPerStop, Error)
		|| !NeedNumber(*WallJson, TEXT("burnDamage"), BurnDamage, Error))
	{
		return false;
	}
	if (!(DistanceM > 0.0 && DistanceM <= 16.0) || !(ArcDeg > 0.0 && ArcDeg <= 360.0)
		|| !(WallDurationS > 0.0 && WallDurationS <= 30.0) || ManaCost < 0.0 || HeatPerStop < 0.0 || BurnDamage < 0.0)
	{
		return Fail(Error, TEXT("vr rules: fireWall distance, arc, duration, mana, heat, or burn is out of range"));
	}
	Out.FireWall.bEnabled = true;
	Out.FireWall.DistanceM = DistanceM;
	Out.FireWall.ArcDeg = ArcDeg;
	Out.FireWall.DurationS = WallDurationS;
	Out.FireWall.ManaCost = ManaCost;
	Out.FireWall.HeatPerStop = HeatPerStop;
	Out.FireWall.BurnDamage = BurnDamage;

	const FJsonObject* NarrowJson = nullptr;
	const FJsonObject* GentleJson = nullptr;
	if (!NeedObject(*Overlay, TEXT("narrowFov"), NarrowJson, Error) || !NeedObject(*Overlay, TEXT("gentle"), GentleJson, Error))
	{
		return false;
	}
	double SpawnArcDeg = 0.0;
	double OfferBelowDeg = 0.0;
	double CameraFovDeg = 0.0;
	double TelegraphMult = 0.0;
	double Competence = 0.0;
	if (!NeedNumber(*NarrowJson, TEXT("spawnArcDeg"), SpawnArcDeg, Error)
		|| !NeedNumber(*NarrowJson, TEXT("offerBelowDeg"), OfferBelowDeg, Error)
		|| !NeedNumber(*NarrowJson, TEXT("cameraFovDeg"), CameraFovDeg, Error)
		|| !NeedNumber(*GentleJson, TEXT("telegraphMult"), TelegraphMult, Error)
		|| !NeedNumber(*GentleJson, TEXT("competence"), Competence, Error))
	{
		return false;
	}
	if (!(SpawnArcDeg > 0.0 && SpawnArcDeg <= 180.0) || !(OfferBelowDeg > 0.0 && OfferBelowDeg <= 180.0)
		|| !(CameraFovDeg >= 40.0 && CameraFovDeg <= 140.0))
	{
		return Fail(Error, TEXT("vr rules: narrowFov arc, offer, or camera is out of range"));
	}
	if (!(TelegraphMult >= 1.0 && TelegraphMult <= 8.0) || !(Competence >= 0.0 && Competence <= 5.0))
	{
		return Fail(Error, TEXT("vr rules: gentle multiplier or competence is out of range"));
	}
	Out.Narrow.SpawnArcDeg = SpawnArcDeg;
	Out.Narrow.OfferBelowDeg = OfferBelowDeg;
	Out.Narrow.CameraFovDeg = CameraFovDeg;
	Out.Gentle.TelegraphMult = TelegraphMult;
	Out.Gentle.Competence = Competence;
	Out.bNarrow = false;
	Out.bGentle = false;

	if (!ReadRivals(*Overlay, Out.Rivals, Error))
	{
		return false;
	}

	Out.bActive = true;
	Out.StandoffM = Standoff;
	Out.Dais.MinX = Centre.X - HalfX;
	Out.Dais.MaxX = Centre.X + HalfX;
	Out.Dais.MinY = Centre.Y - HalfY;
	Out.Dais.MaxY = Centre.Y + HalfY;
	Out.Throws.Add(Throw);
	if (bHonorProposalSwitch && ProposalRequested())
	{
		if (!ApplyProposal(Out, Error))
		{
			Out.bActive = false;
			return false;
		}
	}
	return true;
}
