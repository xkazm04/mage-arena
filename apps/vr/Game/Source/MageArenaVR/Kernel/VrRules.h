#pragma once

#include "Kernel/SimTypes.h"

struct FAttackSpec;
struct FSpell;

// VR combat overlay. Absent means the pinned kernel. Session creation is the only caller that passes one.

struct FVrAabb
{
	double MinX = 0.0;
	double MaxX = 0.0;
	double MinY = 0.0;
	double MaxY = 0.0;

	bool Contains(const FSimVec& Point) const;
	FVrAabb Expanded(double Margin) const;
};

struct FVrAttackMode
{
	FString EnemyId;
	// A ranged overlay attack that holds at the dais hold line (CR-003 steering). True for a throw and for a spit.
	bool bThrow = false;
	double ProjectileMps = 0.0;
	double RangeM = 0.0;
	double ArcApexM = 0.0;
	double SpearLengthM = 0.0;
	double SpearRadiusM = 0.0;
	// DF-004 option A. A spit replaces the melee row's family, tier, damage and windup as well as speed and range.
	// Recovery stays on the pinned row.
	bool bSpit = false;
	FString Family;
	int32 Tier = 0;
	double Damage = 0.0;
	double WindupS = 0.0;
	// A dying spitter throws one ember with the spit numbers after the pinned onDeath delay.
	bool bDeathEmber = false;
};

// Copies an overlay mode onto a local attack row. The pinned row is never written. A throw sets speed and range only.
void VrApplyAttackMode(const FVrAttackMode& Mode, FAttackSpec& Attack);

struct FVrSplitHands
{
	bool bEnabled = false;
	double OneHandPower = 0.6;
	int32 MaxTier = 2;
	// DECISIONS 2026-10-07 (split hands). Negative: the latch holds until the ward drops, the rule before T17.
	double LatchClearIdleS = -1.0;
	// DECISIONS 2026-10-07 (split hands). False: every split cast at full Flow is refused for "crest".
	bool bBoltAtFullFlow = false;
};

struct FVrPlantedStaff
{
	bool bEnabled = false;
	double DurationS = 6.0;
	double ManaUpFront = 8.0;
	double DrainPerSecond = 4.0;
	double MagicReduction = 0.85;
	double PhysicalReduction = 0.6;
};

// Runtime for one bout. Not part of the pinned state hash. A copied ruleset starts empty.
struct FVrStaffRuntime
{
	bool bPlanted = false;
	int32 ActorId = -1;
	int32 UntilTick = 0;
};

// SCHOOL-DEFENCES C. Proposal values live in combat.vr.json. Calibration may override them.
struct FVrFireWall
{
	bool bEnabled = false;
	double DistanceM = 4.0;
	double ArcDeg = 90.0;
	double DurationS = 3.0;
	double ManaCost = 12.0;
	double HeatPerStop = 4.0;
	double BurnDamage = 12.0;
};

// One raised curtain. Centre and facing are the caster's at the raise tick, before that tick's move.
struct FVrWall
{
	int32 Id = 0;
	int32 OwnerId = -1;
	FSimVec Centre;
	FSimVec Facing{1.0, 0.0};
	double DistanceM = 4.0;
	double ArcDeg = 90.0;
	int32 UntilTick = 0;
};

// Identity leaves the pinned wave. Counts below zero mean "use the pinned count".
struct FVrWaveTune
{
	int32 ConscriptCount = -1;
	int32 SlingerCount = -1;
	double SpawnDelayS = 0.0;
};

// Identity is 1. Cadence scales recovery, backoff, and cooldown, not the telegraph windup.
struct FVrPressure
{
	double AttackCadence = 1.0;
	double EnemyDamage = 1.0;
	double FireMageDamage = 1.0;
};

// Identity is 1. Applied to the seated player (team 0) only.
struct FVrPower
{
	double BoltDamage = 1.0;
	double LineDamage = 1.0;
	double ManaRegen = 1.0;
};

// Multiplier on ward drain while a split cast is latched. Identity is 1.
struct FVrDefenceTune
{
	double WardDrainSplit = 1.0;
};

// Plan D3. Read from combat.vr.json. The runtime flags below decide whether a bout uses them.
struct FVrNarrowFov
{
	double SpawnArcDeg = 40.0;
	double OfferBelowDeg = 90.0;
	double CameraFovDeg = 70.0;
};

struct FVrGentle
{
	double TelegraphMult = 2.0;
	double Competence = 1.0;
};

// DECISIONS 2026-10-07 (DF-003 answered): a named rival fights in HP-gated phases. Read from combat.vr.json rivals.
struct FVrRivalPhase
{
	double AtHpFraction = 0.0;
	// Empty: the break only surges. Otherwise the rival's next action is this fire spell, granted once per duel.
	FString OpensWith;
};

// appliesTo picks the spawn: the mage of this arena tier's wave n (and its kind), in this school.
struct FVrRival
{
	FString Id;
	FString School;
	FString TierId;
	int32 WaveN = 0;
	FString WaveKind;
	TArray<FVrRivalPhase> Phases;
	int32 SurgeTiers = 1;
};

// T21: who fights the Tiro final (the last Tiro wave). combat.vr.json tiroFinal. Rival empty: a mage of School with no
// phases (DECISIONS 2026-10-07 cut rule: Corvo, a second Ember entrant on the Fire kernel). Rival set: the named entry
// in rivals, which must apply to that final in that school.
struct FVrTiroFinal
{
	FString School;
	FString Rival;
};

// Runtime for one bout. Not part of the pinned state hash. A copied ruleset starts empty (ClearRuntime),
// and every wave spawn clears it before binding.
struct FVrRivalRuntime
{
	int32 ActorId = -1;
	int32 RivalIndex = -1;
	int32 PhasesBroken = 0;
	TArray<int32> BreakTicks;
	bool bGrantPending = false;
	bool bGrantUsed = false;
	FString GrantSpell;
	// Tick the granted cast started. -1 until it does.
	int32 GrantTick = -1;
};

// True when the segment crosses the arc. OutT is along From -> To. A graze that stays outside does not count.
bool VrWallSegmentCrosses(const FVrWall& Wall, const FSimVec& From, const FSimVec& To, double& OutT);

struct FVrRuleset
{
	bool bActive = false;
	double StandoffM = 0.0;
	FVrAabb Dais;
	TArray<FVrAttackMode> Throws;
	FVrSplitHands Split;
	FVrPlantedStaff Staff;
	FVrFireWall FireWall;
	FVrWaveTune Wave;
	FVrPressure Pressure;
	FVrPower Power;
	FVrDefenceTune Defence;
	FVrNarrowFov Narrow;
	FVrGentle Gentle;
	TArray<FVrRival> Rivals;
	FVrTiroFinal TiroFinal;
	// Harness seam for the census and the design tests: every opponent mage spawns at this competence.
	// Below zero keeps the pinned spawn competence. Not read from JSON. Gentle still wins.
	double MageCompetenceOverride = -1.0;
	// Runtime comfort. Not part of the pinned state hash. Off leaves positions and windups untouched.
	bool bNarrow = false;
	bool bGentle = false;
	// Kernel position of the pad the player is on, and that pad's flat forward. The session writes these.
	FSimVec NarrowOrigin{0.0, 0.0};
	FSimVec NarrowForward{1.0, 0.0};
	FVrStaffRuntime StaffRuntime;
	TArray<FVrWall> Walls;
	int32 WallsRaised = 0;
	TArray<int32> SplitCasting;
	// Ticks the casting hand has been idle while latched, by actor id. Runtime only, like SplitCasting.
	TMap<int32, int32> SplitIdleTicks;
	TArray<FString> Refusals;
	FVrRivalRuntime RivalRuntime;

	const FVrAttackMode* FindThrow(const FString& EnemyId) const;
	// The spit mode for this enemy id, or null. A spit is also returned by FindThrow.
	const FVrAttackMode* FindSpit(const FString& EnemyId) const;
	FVrAabb HoldBox() const;
	bool InsideDais(const FSimVec& Point) const;
	bool InsideHold(const FSimVec& Point) const;
	// Negative when inside the hold box, zero on the boundary, positive outside.
	double OutsideGap(const FSimVec& Point) const;
	FSimVec HoldPoint(const FSimVec& From, const FSimVec& Goal) const;
	FSimVec Outward(const FSimVec& Point) const;
	FSimVec PushOutsideHold(const FSimVec& Point) const;
	void KeepOut(FArenaState& State) const;
	// Bearing around NarrowOrigin. Points already inside the arc are returned unchanged.
	FSimVec CompressToArc(const FSimVec& Point) const;
	void KeepInView(FArenaState& State) const;

	bool IsPlanted(int32 ActorId) const;
	void Lift(const TCHAR* Why);
	bool IsSplitCasting(int32 ActorId) const;
	void SetSplitCasting(int32 ActorId, bool bCasting);
	// Ticks of casting-hand idle that clear the latch. -1 when the latch only clears on a ward drop.
	int32 SplitLatchIdleTicks() const;
	// Once per actor tick, after the ward update. Counts idle ticks and clears the latch at SplitLatchIdleTicks.
	void TickSplitLatch(const FActor& Actor);
	// Null when a split cast of Spell is legal now, else the refusal reason ("tier" or "crest").
	// The kernel and the session both ask this, so they cannot disagree.
	const TCHAR* SplitRefusal(const FActor& Actor, const FSpell& Spell) const;
	// 0 for unblockable and for any other family. The dome never perfects.
	double DomeReduction(const FString& Family) const;
	void TickStaff(FArenaState& State, FActor& Actor, const FInputFrame& Input);
	void Refuse(const FActor& Actor, const TCHAR* Reason);

	const FVrWall* FindWall(int32 OwnerId) const;
	void ExpireWalls(const FArenaState& State);
	void TickWalls(FArenaState& State, FActor& Actor, const FInputFrame& Input);
	void BurnCrossers(FArenaState& State);
	void ClearRuntime();

	// Index into Rivals, or INDEX_NONE.
	int32 FindRival(const FString& TierId, int32 WaveN, const FString& WaveKind, const FString& School) const;
	const FVrRival* BoundRival() const;
	bool IsRival(int32 ActorId) const;
	// Phase breaks for the bound rival. StepArena calls it once per tick, after hits resolve and before the tier clock.
	void TickRivalPhases(FArenaState& State);
	// Starts the granted signature when the bound rival is free. True when it started.
	bool TryGrantedCast(FArenaState& State, FActor& Actor, const FSimVec& Aim);
};

// Reads combat.vr.json. With bHonorProposalSwitch, -MageArenaProposal or MageArenaProposal=1
// overlays apps/vr/data/vr/calibration-proposal.json. The default bout does not.
bool LoadVrRuleset(FVrRuleset& Out, FString& Error, bool bHonorProposalSwitch = true);

// Overlays calibration-proposal.json onto an already loaded ruleset, whatever the command line says. The census uses it.
bool ApplyVrCalibrationProposal(FVrRuleset& Out, FString& Error);

// Test seam: read the overlay files from this directory instead of apps/vr/data/vr. Empty restores the default.
void SetVrDataDirForTest(const FString& Dir);

// Opponent telegraph seconds. Identity when gentle is off, including when the multiplier is not 1.
inline double VrOpponentTelegraph(const FVrRuleset* Rules, int32 Team, double Seconds)
{
	if (Rules && Rules->bGentle && Team != 0)
	{
		return Seconds * Rules->Gentle.TelegraphMult;
	}
	return Seconds;
}
