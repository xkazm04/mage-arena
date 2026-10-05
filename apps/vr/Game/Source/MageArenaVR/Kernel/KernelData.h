#pragma once

#include "Combat/AbsorbResolver.h"
#include "Kernel/SimTypes.h"

#include "CoreMinimal.h"

// Spell row after the catalog parse. catalog.ts Spell.
struct FSpell
{
	FString Id;
	FString Line;
	int32 Tier = 0;
	FString Branch;
	FString Name;
	double CastS = 0.0;
	double CooldownS = 0.0;
	double Mana = 0.0;
	double Damage = 0.0;
	FString Family;
	double TelegraphS = 0.0;
	double RangeM = 0.0;
	FString Kind;
	double SpeedMps = 0.0;
	double RadiusM = 0.0;
	int32 Count = 1;
	double SpreadDeg = 0.0;
	double ArcDeg = 360.0;
	FString Effect;
	double DurationS = 0.0;
	double Amount = 0.0;
	double BurstRadiusM = 0.0;
};

struct FAttackSpec
{
	FString Id;
	FString Family;
	double Damage = 0.0;
	double WindupS = 0.0;
	TOptional<double> RecoveryS;
	TOptional<double> CooldownS;
	TOptional<double> RangeM;
	TOptional<double> ProjectileMps;
	TOptional<int32> Tier;
	FString Effect;
	FString Telegraph;
	FString OnWallHit;
};

struct FDeathSpec
{
	FString Id;
	FString Family;
	int32 Tier = 0;
	double Damage = 0.0;
	double RadiusM = 0.0;
	double DelayS = 0.0;
};

struct FEnemySpec
{
	FString Id;
	FString Name;
	double Hp = 0.0;
	double SpeedMps = 0.0;
	TArray<FAttackSpec> Attacks;
	FString Behaviour;
	TOptional<double> FrontBlockDeg;
	TOptional<double> FrontBlockMult;
	TOptional<double> ArmourMultVsBolt;
	TOptional<double> KeepMinM;
	TOptional<double> KeepMaxM;
	TOptional<int32> PackSize;
	TOptional<FDeathSpec> OnDeath;
};

// enemies.json mages.competence. The dial is interpolated; caps clamp the result.
struct FMageCompetenceRow
{
	double Level = 0.0;
	double ReactionDelayS = 0.0;
	double AbsorbChance = 0.0;
	double PerfectChance = 0.0;
	double AimErrorDeg = 0.0;
	double DecisionCadenceS = 0.0;
};

struct FMageCaps
{
	double ReactionDelayMinS = 0.0;
	double PerfectAbsorbChanceMax = 0.0;
};

// arena-tiers.json spawn. Either an enemy pack or one mage.
struct FWaveSpawn
{
	bool bEnemy = false;
	FString EnemyId;
	FString MageId;
	double Competence = 0.0;
	int32 Count = 1;
	TOptional<int32> EchoFromLoop;
};

struct FArenaWave
{
	int32 N = 0;
	FString Kind;
	TArray<FWaveSpawn> Spawns;
	TOptional<double> TargetMinS;
	TOptional<double> TargetMaxS;
};

struct FArenaTier
{
	FString Id;
	FString Name;
	TArray<FArenaWave> Waves;
	TArray<int32> PayoutGold;
	TArray<int32> Renown;
	int32 RequiresMastery = 0;
};

// How a fire row's heat_gain cell is paid. Parsed, not a second copy of the numbers.
enum class EFireHeatMode : uint8
{
	None,
	Hit,
	Target,
	Tick,
	Instant,
	Lock
};

// spells-fire.csv row. Shape, damage text, heat_gain and notes are parsed into the fields below.
struct FFireSpell
{
	FString Id;
	FString Name;
	int32 Tier = 0;
	FString Shape;
	double CastS = 0.0;
	double CooldownS = 0.0;
	double Mana = 0.0;
	double Damage = 0.0;
	FString DamageText;
	FString Blockable;
	FString Family;
	double TelegraphS = 0.0;
	double RangeM = 0.0;
	FString HeatGain;
	FString Notes;

	FString Kind;
	double SpeedMps = 0.0;
	double RadiusM = 0.0;
	double ArcDeg = 360.0;
	double DurationS = 0.0;
	double TickS = 0.0;
	double KnockbackM = 0.0;
	double DashM = 0.0;
	EFireHeatMode HeatMode = EFireHeatMode::None;
	double InstantHeat = 0.0;
	double HeatAmount = 0.0;
	double HeatLockValue = 0.0;
	double HeatLockS = 0.0;
	// "doubled" in the notes is read as 2. Stacks with the Blazing threshold multiplier.
	double AbsorbDrainMult = 1.0;
	bool bPierceShields = false;
	bool bRootDuringCast = false;
	bool bSequentialTelegraph = false;
	bool bPerfectOnlyFirstTick = false;
	bool bBolt = false;
};

// schools.json fire.combatIdentity. One effect per threshold; a higher band replaces a lower one.
struct FHeatThreshold
{
	double At = 0.0;
	FString Name;
	bool bSpellDamage = false;
	double SpellDamageMult = 1.0;
	bool bAbsorbDrain = false;
	double AbsorbDrainMult = 1.0;
	bool bStaminaRegen = false;
	double StaminaRegenMult = 1.0;
};

struct FHeatRules
{
	double Min = 0.0;
	double Max = 0.0;
	double PerBoltHit = 0.0;
	double PerFlickTarget = 0.0;
	double PerHitTakenUnabsorbed = 0.0;
	double PerSecondPerEnemyWithin = 0.0;
	double EnemyWithinM = 0.0;
	double DecayPerSecond = 0.0;
	double DecayAfterNoGainS = 0.0;
	TArray<FHeatThreshold> Thresholds;
};

// Pinned combat, stats, spells, enemies, runtime and the scale contract. Loaded once.
struct FKernelData
{
	bool bReady = false;
	FString Error;

	double SimStepHz = 0.0;
	double WalkMps = 0.0;
	double SprintMps = 0.0;
	double SprintStaminaPerSecond = 0.0;
	double RollDistanceM = 0.0;
	double RollDurationS = 0.0;
	double RollIFramesS = 0.0;
	double RollStaminaCost = 0.0;
	double RollRecoveryS = 0.0;
	double StaffRangeM = 0.0;
	double StaffWindupS = 0.0;
	double StaffRecoveryS = 0.0;
	double StaffStaminaCost = 0.0;
	double StaffDamage = 0.0;
	bool bStaffInterrupts = false;
	double StaminaRegenPerSecond = 0.0;
	double StaminaRegenDelayS = 0.0;
	double UnlockAtSeconds[5] = {};
	double TierClockAdvanceS = 0.0;
	double MinimumSecondsBetweenUnlocks = 0.0;
	FAbsorbRules Absorb;
	int32 LineSlots = 0;
	TArray<int32> BranchAtTiers;
	double FlowWithinS = 0.0;
	double FlowMax = 0.0;
	double FlowPerStack = 0.0;
	bool bFlowResetSameLine = false;
	double FlowIdleS = 0.0;
	double CrestManaCost = 0.0;
	double CrestDamageMult = 0.0;
	double PerfectAbsorbGives = 0.0;
	double MinimumTelegraphPositionalS = 0.0;
	double MinimumTelegraphUnblockableS = 0.0;
	double HealFraction = 0.0;
	double ManaRefill = 0.0;
	double StaminaRefill = 0.0;

	double HpBase = 0.0;
	double HpPerRank = 0.0;
	double StaminaBase = 0.0;
	double StaminaPerRank = 0.0;
	double ManaBase = 0.0;
	double ManaPerRank = 0.0;
	double ManaRegenBase = 0.0;
	double ManaRegenPerRank = 0.0;

	FString BoltName;
	double BoltCastS = 0.0;
	double BoltCooldownS = 0.0;
	double BoltMana = 0.0;
	double BoltDamage = 0.0;
	double BoltRangeM = 0.0;
	double BoltSpeedMps = 0.0;

	TArray<FSpell> Spells;
	double StoredFraction = 0.0;

	double ArenaWidthM = 0.0;
	double ArenaHeightM = 0.0;
	double OpeningSeparationM = 0.0;
	FSimVec ArenaCentre;
	double MageRadiusM = 0.0;
	double ProjectileRadiusM = 0.0;
	double StaffArcDeg = 0.0;

	FSimRanks DefaultRanks;
	FSimVec TrainingPlayer;
	double DummyHp = 0.0;
	FSimVec TrainingFront;
	FSimVec TrainingSide;
	double AttackIntervalS = 0.0;
	double FirstAttackS = 0.0;
	double MagicDamage = 0.0;
	int32 MagicTier = 0;
	double MagicWindupS = 0.0;
	double MagicSpeedMps = 0.0;
	double MagicRangeM = 0.0;
	double PhysicalDamage = 0.0;
	int32 PhysicalTier = 0;
	double PhysicalWindupS = 0.0;
	double PhysicalSpeedMps = 0.0;
	double PhysicalRangeM = 0.0;
	double ChargeDamage = 0.0;
	double ChargeWindupS = 0.0;
	double ChargeRangeM = 0.0;
	double ChargeWidthM = 0.0;
	int32 StreamCount = 0;
	double StreamIntervalS = 0.0;
	double PerfectBotLeadS = 0.0;
	double LateBotLeadS = 0.0;
	double ReportDurationS = 0.0;
	int32 PerformanceProjectiles = 0;
	double FieldWidthM = 0.0;
	double FieldHeightM = 0.0;
	double FieldSpeedMps = 0.0;
	double FieldRangeM = 0.0;

	double FanSpeedMps = 0.0;
	double ReturnWaveSpeedMps = 0.0;
	double ReturnWaveRadiusM = 0.0;
	double TombCursorRadiusM = 0.0;
	TArray<FComposition> Presets;

	FSimVec EnemySpawn;
	double SpawnJitterM = 0.0;
	double SpawnMarginM = 0.0;
	double DefaultMeleeRangeM = 0.0;
	double MeleeArcDeg = 0.0;
	double DefaultRecoveryS = 0.0;
	double RangedRangeM = 0.0;
	double NetRangeM = 0.0;
	double TongueRangeM = 0.0;
	double TongueWidthM = 0.0;
	double MawArtilleryCooldownS = 0.0;
	double ChargeCooldownS = 0.0;
	double ContactRangeM = 0.0;
	double HoundOrbitM = 0.0;
	double ConscriptBackoffM = 0.0;
	double SeparationM = 0.0;
	double SeparationWeight = 0.0;
	FSimVec PlayerSpawn;
	double MagePreferredMinM = 0.0;
	double MagePreferredMaxM = 0.0;
	double MageStrafePeriodS = 0.0;
	double MageAimLeadFraction = 0.0;
	double MageDefenceHoldS = 0.0;
	double ReferenceCompetence = 0.0;
	FString ReferencePreset;
	TArray<FString> OpponentPresets;
	double FightTimeoutS = 0.0;
	double DeadAirBucketS = 0.0;

	double MaxFrameDeltaS = 0.0;

	TArray<FEnemySpec> Enemies;
	TArray<FMageCompetenceRow> MageCompetence;
	FMageCaps MageCaps;
	TArray<FArenaTier> ArenaTiers;
	TArray<FFireSpell> FireSpells;
	// Same FSpell records the threat scan looks up by id. Not part of the water spell list.
	TArray<FSpell> FireCatalog;
	FHeatRules Heat;
};

const FFireSpell* FindFireSpell(const FString& Id);

const FKernelData& KernelData();

// Test seam. Loads a fresh FKernelData from the pinned files, except that a file whose full path is a key in
// Overrides is read from the mapped text instead. KernelData() itself never sees an override.
FString KernelDataPinnedPath(const TCHAR* Relative);
FKernelData LoadKernelDataWithOverrides(const TMap<FString, FString>& Overrides);

// catalog.ts:53. Line identity, not a tuned magnitude.
const TArray<FString>& WaterLines();
