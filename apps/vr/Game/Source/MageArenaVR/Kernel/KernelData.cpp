#include "Kernel/KernelData.h"
#include "Kernel/KernelDataLoad.h"
#include "Kernel/VrRules.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "MageArenaVR.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

namespace KernelDataLoad
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
	// Chain: hash = SHA1(previous hash text + this file's text), so the order of reads is part of the result.
	const FTCHARToUTF8 Utf8(*(Data.PinHash + Out));
	FSHAHash Hash;
	FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash.Hash);
	Data.PinHash = Hash.ToString();
	++Data.PinFileCount;
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

// Lifted from KernelDataAir.cpp (T44) unchanged, so Air reads its cells exactly as before.
bool StrictNumber(const FString& Cell, double& Out)
{
	FString Text;
	if (!MatchGroup(Cell, TEXT("^(\\d+(?:\\.\\d+)?)$"), Text))
	{
		return false;
	}
	Out = FCString::Atod(*Text);
	return FMath::IsFinite(Out);
}

bool CellNumber(const FString& Cell, const TCHAR* Sheet, const TCHAR* Column, const FString& Id, double& Out, FString& Error)
{
	if (!StrictNumber(Cell, Out))
	{
		Error = FString::Printf(TEXT("%s row %s: %s cell '%s' is not a number"), Sheet, *Id, Column, *Cell);
		return false;
	}
	return true;
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

// T23. The VR-owned air rows. Read without the pin chain (the file is not pinned), but through the same test override
// map, so a test can load a bad cell and see the kernel refuse it.
bool LoadAirSpells(FKernelData& Data)
{
	const FString Path = KernelDataAirSpellsPath();
	FString Text;
	const FString* Override = GTextOverrides ? GTextOverrides->Find(Path) : nullptr;
	if (Override)
	{
		Text = *Override;
	}
	else if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: cannot read %s"), *Path));
	}
	FString Error;
	if (!ParseAirSpellsCsv(Text, Data.AirSpells, Data.AirCatalog, Error))
	{
		return Fail(Data, Path + TEXT(": ") + Error);
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

// Names that one file uses to point at another. Games.cpp and Enemies.cpp resolve them with checkf when a bout spawns,
// so a rename in a data bump used to surface there, mid-session, instead of as a named load error.
bool CheckReferences(FKernelData& Data)
{
	auto HasPreset = [&Data](const FString& Name)
	{
		for (const FComposition& Preset : Data.Presets)
		{
			if (Preset.Name == Name)
			{
				return true;
			}
		}
		return false;
	};
	if (!HasPreset(Data.ReferencePreset))
	{
		return Fail(Data, FString::Printf(TEXT("kernel data: games.referencePreset names no water preset: %s"), *Data.ReferencePreset));
	}
	for (const FString& Name : Data.OpponentPresets)
	{
		if (!HasPreset(Name))
		{
			return Fail(Data, FString::Printf(TEXT("kernel data: games.opponentPresets names no water preset: %s"), *Name));
		}
	}
	for (const FArenaTier& Tier : Data.ArenaTiers)
	{
		for (const FArenaWave& Wave : Tier.Waves)
		{
			for (const FWaveSpawn& Spawn : Wave.Spawns)
			{
				if (!Spawn.bEnemy)
				{
					continue;
				}
				bool bKnown = false;
				for (const FEnemySpec& Enemy : Data.Enemies)
				{
					bKnown |= Enemy.Id == Spawn.EnemyId;
				}
				if (!bKnown)
				{
					return Fail(Data, FString::Printf(TEXT("kernel data: arena-tiers %s wave %d spawns an enemy that enemies.json does not define: %s"),
						*Tier.Id, Wave.N, *Spawn.EnemyId));
				}
			}
		}
	}
	return true;
}

bool LoadAbsorb(FKernelData& Data)
{
	FString AbsorbError;
	if (!Data.Absorb.LoadFromPinnedFile(AbsorbError))
	{
		return Fail(Data, AbsorbError);
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
		return Fail(Data, TEXT("kernel data: perfect.notAgainst must be exactly physical and unblockable"));
	}
	return true;
}

bool LoadCombat(FKernelData& Data)
{
	const FString CombatPath = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/combat.json"));
	TSharedPtr<FJsonObject> Combat;
	if (!LoadJson(CombatPath, Combat, Data))
	{
		return false;
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
		return false;
	}
	if (Data.TierClockAdvanceS != Data.Absorb.TierClockAdvanceS)
	{
		return Fail(Data, TEXT("kernel data: clock reward contradiction"));
	}
	for (int32 Tier = 1; Tier <= 4; ++Tier)
	{
		if (!NeedNumber(*Unlock, *FString::FromInt(Tier), Data.UnlockAtSeconds[Tier], CombatPath, Data))
		{
			return false;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* BranchTiers = nullptr;
	if (!Lines->TryGetArrayField(TEXT("branchAtTiers"), BranchTiers))
	{
		return Fail(Data, TEXT("kernel data: lines.branchAtTiers missing"));
	}
	for (const TSharedPtr<FJsonValue>& Value : *BranchTiers)
	{
		Data.BranchAtTiers.Add(static_cast<int32>(std::llround(Value->AsNumber())));
	}
	return true;
}

// The absorb file owns the nerve formulas. stats.csv must state the same numbers.
bool CheckNerveAuthority(const FCsvTable& Stats, FKernelData& Data)
{
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
		return Fail(Data, TEXT("kernel data: nerve authority contradiction"));
	}
	{
		const FRegexPattern RefundPattern(TEXT("^(\\d+) \\* incomingTier \\* \\(1 \\+ ([\\d.]+) \\* nerveRank\\)"));
		FRegexMatcher RefundMatcher(RefundPattern, Data.Absorb.ManaFormula);
		if (!RefundMatcher.FindNext())
		{
			return Fail(Data, TEXT("kernel data: nerve authority contradiction"));
		}
		DrainB = [&]() { FString Second; MatchGroup(Data.Absorb.DrainFormula, TEXT("^\\d+ - (\\d+) \\* nerveRank$"), Second); return Second; }();
		RefundPer = RefundMatcher.GetCaptureGroup(2);
	}
	const FString Compact = Cell(*Nerve, TEXT("arena_effect"))->Replace(TEXT(" "), TEXT(""));
	if (!Compact.Contains(FString::Printf(TEXT("absorbDrain=%s-%s*rank"), *DrainA, *DrainB))
		|| !Compact.Contains(FString::Printf(TEXT("perfectReturnMult=1+%s*rank"), *RefundPer)))
	{
		return Fail(Data, TEXT("kernel data: nerve authority contradiction"));
	}
	return true;
}

bool LoadStats(FKernelData& Data)
{
	const FString StatsPath = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/stats.csv"));
	FCsvTable Stats;
	if (!LoadCsv(StatsPath, Stats, Data)
		|| !LinearStat(Stats, TEXT("vigor"), TEXT("maxHp"), Data.HpBase, Data.HpPerRank, Data)
		|| !LinearStat(Stats, TEXT("vigor"), TEXT("maxStamina"), Data.StaminaBase, Data.StaminaPerRank, Data)
		|| !LinearStat(Stats, TEXT("focus"), TEXT("maxMana"), Data.ManaBase, Data.ManaPerRank, Data)
		|| !LinearStat(Stats, TEXT("focus"), TEXT("manaRegen"), Data.ManaRegenBase, Data.ManaRegenPerRank, Data))
	{
		return false;
	}
	return CheckNerveAuthority(Stats, Data);
}

bool LoadScaleContract(FKernelData& Data)
{
	const FString ScalePath = PinnedPath(TEXT("art/scale-contract-v1.json"));
	TSharedPtr<FJsonObject> Scale;
	if (!LoadJson(ScalePath, Scale, Data))
	{
		return false;
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
		return false;
	}
	Data.ArenaWidthM = (*Metres)[0]->AsNumber();
	Data.ArenaHeightM = (*Metres)[1]->AsNumber();
	if (ScaleArc != Data.Absorb.ArcDeg)
	{
		return Fail(Data, TEXT("kernel data: absorb contract contradiction"));
	}
	return true;
}

bool LoadTrainingTuning(const FJsonObject& Training, FKernelData& Data, const FString& RuntimePath)
{
	const FJsonObject* Ranks = nullptr;
	const FJsonObject* Magic = nullptr;
	const FJsonObject* Physical = nullptr;
	const FJsonObject* Charge = nullptr;
	const FJsonObject* Field = nullptr;
	if (!NeedObject(Training, TEXT("defaultRanks"), Ranks, RuntimePath, Data)
		|| !NeedInt(*Ranks, TEXT("vigor"), Data.DefaultRanks.Vigor, RuntimePath, Data)
		|| !NeedInt(*Ranks, TEXT("focus"), Data.DefaultRanks.Focus, RuntimePath, Data)
		|| !NeedInt(*Ranks, TEXT("nerve"), Data.DefaultRanks.Nerve, RuntimePath, Data)
		|| !NeedVec(Training, TEXT("player"), Data.TrainingPlayer, RuntimePath, Data)
		|| !NeedNumber(Training, TEXT("dummyHp"), Data.DummyHp, RuntimePath, Data)
		|| !NeedVec(Training, TEXT("front"), Data.TrainingFront, RuntimePath, Data)
		|| !NeedVec(Training, TEXT("side"), Data.TrainingSide, RuntimePath, Data)
		|| !NeedNumber(Training, TEXT("attackIntervalS"), Data.AttackIntervalS, RuntimePath, Data)
		|| !NeedNumber(Training, TEXT("firstAttackS"), Data.FirstAttackS, RuntimePath, Data)
		|| !NeedObject(Training, TEXT("magic"), Magic, RuntimePath, Data)
		|| !NeedNumber(*Magic, TEXT("damage"), Data.MagicDamage, RuntimePath, Data)
		|| !NeedInt(*Magic, TEXT("tier"), Data.MagicTier, RuntimePath, Data)
		|| !NeedNumber(*Magic, TEXT("windupS"), Data.MagicWindupS, RuntimePath, Data)
		|| !NeedNumber(*Magic, TEXT("speedMps"), Data.MagicSpeedMps, RuntimePath, Data)
		|| !NeedNumber(*Magic, TEXT("rangeM"), Data.MagicRangeM, RuntimePath, Data)
		|| !NeedObject(Training, TEXT("physical"), Physical, RuntimePath, Data)
		|| !NeedNumber(*Physical, TEXT("damage"), Data.PhysicalDamage, RuntimePath, Data)
		|| !NeedInt(*Physical, TEXT("tier"), Data.PhysicalTier, RuntimePath, Data)
		|| !NeedNumber(*Physical, TEXT("windupS"), Data.PhysicalWindupS, RuntimePath, Data)
		|| !NeedNumber(*Physical, TEXT("speedMps"), Data.PhysicalSpeedMps, RuntimePath, Data)
		|| !NeedNumber(*Physical, TEXT("rangeM"), Data.PhysicalRangeM, RuntimePath, Data)
		|| !NeedObject(Training, TEXT("charge"), Charge, RuntimePath, Data)
		|| !NeedNumber(*Charge, TEXT("damage"), Data.ChargeDamage, RuntimePath, Data)
		|| !NeedNumber(*Charge, TEXT("windupS"), Data.ChargeWindupS, RuntimePath, Data)
		|| !NeedNumber(*Charge, TEXT("rangeM"), Data.ChargeRangeM, RuntimePath, Data)
		|| !NeedNumber(*Charge, TEXT("widthM"), Data.ChargeWidthM, RuntimePath, Data)
		|| !NeedInt(Training, TEXT("streamCount"), Data.StreamCount, RuntimePath, Data)
		|| !NeedNumber(Training, TEXT("streamIntervalS"), Data.StreamIntervalS, RuntimePath, Data)
		|| !NeedNumber(Training, TEXT("perfectBotLeadS"), Data.PerfectBotLeadS, RuntimePath, Data)
		|| !NeedNumber(Training, TEXT("lateBotLeadS"), Data.LateBotLeadS, RuntimePath, Data)
		|| !NeedNumber(Training, TEXT("reportDurationS"), Data.ReportDurationS, RuntimePath, Data)
		|| !NeedInt(Training, TEXT("performanceProjectiles"), Data.PerformanceProjectiles, RuntimePath, Data)
		|| !NeedObject(Training, TEXT("performanceField"), Field, RuntimePath, Data)
		|| !NeedNumber(*Field, TEXT("widthM"), Data.FieldWidthM, RuntimePath, Data)
		|| !NeedNumber(*Field, TEXT("heightM"), Data.FieldHeightM, RuntimePath, Data)
		|| !NeedNumber(*Field, TEXT("speedMps"), Data.FieldSpeedMps, RuntimePath, Data)
		|| !NeedNumber(*Field, TEXT("rangeM"), Data.FieldRangeM, RuntimePath, Data))
	{
		return false;
	}
	return true;
}

bool LoadWaterPresets(const FJsonObject& Water, FKernelData& Data, const FString& RuntimePath)
{
	const TArray<TSharedPtr<FJsonValue>>* Presets = nullptr;
	if (!Water.TryGetArrayField(TEXT("presets"), Presets) || Presets->Num() == 0)
	{
		return Fail(Data, TEXT("kernel data: runtime water.presets missing"));
	}
	for (const TSharedPtr<FJsonValue>& Value : *Presets)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
		{
			return Fail(Data, TEXT("kernel data: preset is not an object"));
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
			return false;
		}
		for (const TSharedPtr<FJsonValue>& LineValue : *LineValues)
		{
			FString Line;
			if (!LineValue.IsValid() || !LineValue->TryGetString(Line))
			{
				return Fail(Data, TEXT("kernel data: preset line is not a string"));
			}
			Composition.Lines.Add(Line);
		}
		Data.Presets.Add(MoveTemp(Composition));
	}
	return true;
}

bool LoadRuntime(FKernelData& Data)
{
	const FString RuntimePath = PinnedPath(TEXT("packages/core/src/arena/data/runtime.json"));
	TSharedPtr<FJsonObject> Runtime;
	if (!LoadJson(RuntimePath, Runtime, Data))
	{
		return false;
	}
	const FJsonObject* Geometry = nullptr;
	const FJsonObject* Training = nullptr;
	const FJsonObject* Water = nullptr;
	const FJsonObject* Games = nullptr;
	const FJsonObject* Presentation = nullptr;
	if (!NeedObject(*Runtime, TEXT("geometry"), Geometry, RuntimePath, Data)
		|| !NeedVec(*Geometry, TEXT("arenaCentre"), Data.ArenaCentre, RuntimePath, Data)
		|| !NeedNumber(*Geometry, TEXT("mageRadiusM"), Data.MageRadiusM, RuntimePath, Data)
		|| !NeedNumber(*Geometry, TEXT("projectileRadiusM"), Data.ProjectileRadiusM, RuntimePath, Data)
		|| !NeedNumber(*Geometry, TEXT("staffArcDeg"), Data.StaffArcDeg, RuntimePath, Data)
		|| !NeedObject(*Runtime, TEXT("training"), Training, RuntimePath, Data)
		|| !LoadTrainingTuning(*Training, Data, RuntimePath)
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
		return false;
	}
	return LoadGamesTuning(*Games, Data, RuntimePath) && LoadWaterPresets(*Water, Data, RuntimePath);
}

FKernelData Load()
{
	FKernelData Data;
	const FString SpellsPath = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/spells-water.csv"));
	const FString EnemyPath = PinnedPath(TEXT("docs/design/baseline-fourteen-nights/design/data/enemies.json"));

	// LoadText chains every file it reads into the pin hash, so the order of these steps is part of the result.
	if (!LoadAbsorb(Data) || !LoadCombat(Data) || !LoadStats(Data) || !LoadScaleContract(Data) || !LoadRuntime(Data))
	{
		return Data;
	}
	FCsvTable SpellRows;
	if (!LoadCsv(SpellsPath, SpellRows, Data) || !BuildSpells(SpellRows, Data))
	{
		return Data;
	}
	TSharedPtr<FJsonObject> Enemies;
	if (!LoadJson(EnemyPath, Enemies, Data) || !LoadEnemies(*Enemies, Data, EnemyPath) || !LoadArenaTiers(Data) || !LoadFireSpells(Data) || !LoadHeat(Data)
		|| !LoadAirSpells(Data) || !CheckReferences(Data))
	{
		return Data;
	}
	Data.bReady = true;
	return Data;
}
}
using namespace KernelDataLoad;

const FKernelData& KernelData()
{
	static const FKernelData Loaded = []()
	{
		FKernelData Data = Load();
		const FString PinnedDir = PinnedPath(TEXT(""));
		if (Data.bReady)
		{
			UE_LOG(LogMageArena, Log, TEXT("kernel data: pinned dir %s, %d files, SHA1 %s"), *PinnedDir, Data.PinFileCount, *Data.PinHash);
			// T54 (review A3b): which overlay rides on that pin, as check-overlay.mjs names it.
			UE_LOG(LogMageArena, Log, TEXT("kernel data: pin SHA1 %s, vr overlay sha256 %s, variant %s"), *Data.PinHash, *VrOverlaySha256(),
			    VrOverlayVariant());
		}
		else
		{
			UE_LOG(LogMageArena, Error, TEXT("kernel data: not loaded (%s); expected pinned data in %s"), *Data.Error, *PinnedDir);
		}
		return Data;
	}();
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
