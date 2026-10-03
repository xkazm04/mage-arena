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

	double MaxFrameDeltaS = 0.0;

	TArray<FEnemySpec> Enemies;
};

const FKernelData& KernelData();

// catalog.ts:53. Line identity, not a tuned magnitude.
const TArray<FString>& WaterLines();
