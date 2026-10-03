#include "Kernel/Training.h"

#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"

#include "Algo/Sort.h"

#include <cmath>
#include <limits>

namespace
{
const TCHAR* MagicFamily = TEXT("magic");
}

FTraining CreateTraining(const FString& Kind, uint32 Seed)
{
	const FKernelData& Data = KernelData();
	FTraining Training;
	Training.State = CreateArena(Seed);
	Training.Kind = Kind;
	Training.PlayerId = AddMage(Training.State, 0, Data.TrainingPlayer, TEXT("Cassia"), &Data.DefaultRanks).Id;
	const FSimVec DummyPos = Kind == TEXT("physical") ? Data.TrainingSide : Data.TrainingFront;
	Training.DummyId = AddMage(Training.State, 1, DummyPos, TEXT("Training target"), &Data.DefaultRanks).Id;
	FActor& Dummy = *SimFindActor(Training.State, Training.DummyId);
	Dummy.bDummy = true;
	Dummy.Hp = Data.DummyHp;
	Dummy.MaxHp = Data.DummyHp;
	Training.NextAttack = SimTicks(Data.FirstAttackS);
	return Training;
}

void ScheduleTraining(FTraining& Training)
{
	FArenaState& State = Training.State;
	FActor* Dummy = SimFindActor(State, Training.DummyId);
	FActor* Player = SimFindActor(State, Training.PlayerId);
	if (!Dummy || !Player || Dummy->bDown || Player->bDown)
	{
		return;
	}
	const FKernelData& Data = KernelData();
	if (Training.Kind == TEXT("performance"))
	{
		while (State.Projectiles.Num() < Data.PerformanceProjectiles)
		{
			const FSimVec Origin{
				Player->Pos.X + (ArenaRandom(State, TEXT("performance x")) - 0.5) * Data.FieldWidthM,
				Player->Pos.Y + (ArenaRandom(State, TEXT("performance y")) - 0.5) * Data.FieldHeightM};
			FHit Hit;
			Hit.OwnerId = Dummy->Id;
			Hit.ActivationId = State.NextId++;
			Hit.Damage = 0.0;
			Hit.Family = MagicFamily;
			Hit.Tier = 0;
			Hit.Source = Origin;
			SpawnProjectile(State, Hit, Origin, FSimVec{1.0, 0.0}, Data.FieldSpeedMps, Data.FieldRangeM);
		}
		return;
	}
	if (State.Tick + 1 < Training.NextAttack)
	{
		return;
	}
	Training.NextAttack += SimTicks(Data.AttackIntervalS);
	Training.Attacks++;
	if (Training.Kind == TEXT("flanker"))
	{
		Dummy->Pos.X = (Training.Attacks % 2) ? Data.TrainingFront.X : 2.0 * Data.TrainingPlayer.X - Data.TrainingFront.X;
		Dummy->Pos.Y = Data.TrainingFront.Y;
	}
	const bool bPhysical = Training.Kind == TEXT("physical");
	const double SpecDamage = bPhysical ? Data.PhysicalDamage : Data.MagicDamage;
	const int32 SpecTier = bPhysical ? Data.PhysicalTier : Data.MagicTier;
	const double SpecWindup = bPhysical ? Data.PhysicalWindupS : Data.MagicWindupS;
	const double SpecSpeed = bPhysical ? Data.PhysicalSpeedMps : Data.MagicSpeedMps;
	const double SpecRange = bPhysical ? Data.PhysicalRangeM : Data.MagicRangeM;
	const int32 Count = Training.Kind == TEXT("stream") ? Data.StreamCount : 1;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Id = State.NextId++;
		FTelegraph Telegraph;
		Telegraph.Id = Id;
		Telegraph.ActivationId = Id;
		Telegraph.OwnerId = Dummy->Id;
		Telegraph.Family = Training.Kind == TEXT("charge") ? TEXT("unblockable") : bPhysical ? TEXT("physical") : TEXT("magic");
		Telegraph.Damage = Training.Kind == TEXT("charge") ? Data.ChargeDamage : SpecDamage;
		Telegraph.Tier = SpecTier;
		Telegraph.Source = Dummy->Pos;
		Telegraph.Origin = Dummy->Pos;
		Telegraph.Target = Player->Pos;
		Telegraph.StartTick = State.Tick + 1;
		Telegraph.ResolveTick = State.Tick + 1 + SimTicks(Training.Kind == TEXT("charge") ? Data.ChargeWindupS : SpecWindup) + Index * SimTicks(Data.StreamIntervalS);
		Telegraph.Kind = Training.Kind == TEXT("charge") ? TEXT("lane") : TEXT("projectile");
		Telegraph.SpeedMps = SpecSpeed;
		Telegraph.RangeM = Training.Kind == TEXT("charge") ? Data.ChargeRangeM : SpecRange;
		Telegraph.WidthM = Data.ChargeWidthM;
		State.Telegraphs.Add(Telegraph);
	}
}

FInputFrame TimingBot(const FTraining& Training, const FString& Kind, TOptional<int32> LeadTicks)
{
	const FKernelData& Data = KernelData();
	const FActor& Player = *SimFindActor(Training.State, Training.PlayerId);
	const FActor& Dummy = *SimFindActor(Training.State, Training.DummyId);
	FInputFrame Input = SimIdleInput(Dummy.Pos);
	if (Kind == TEXT("never"))
	{
		return Input;
	}
	if (Kind == TEXT("holder"))
	{
		Input.bAbsorb = true;
		return Input;
	}
	const int32 Lead = LeadTicks.Get(SimTicks(Kind == TEXT("perfect") ? Data.PerfectBotLeadS : Data.LateBotLeadS));
	struct FIncoming
	{
		int32 Index = 0;
		double Impact = 0.0;
	};
	TArray<FIncoming> Incoming;
	for (int32 Index = 0; Index < Training.State.Projectiles.Num(); ++Index)
	{
		const FProjectile& Projectile = Training.State.Projectiles[Index];
		if (Projectile.OwnerId == Player.Id || Projectile.Family != TEXT("magic"))
		{
			continue;
		}
		const double Gap = SimDistance(Projectile.Pos, Player.Pos) - Projectile.Radius - Player.Radius;
		const FSimVec Toward = SimUnit(SimSub(Player.Pos, Projectile.Pos));
		const double Speed = Projectile.Velocity.X * Toward.X + Projectile.Velocity.Y * Toward.Y;
		FIncoming Row;
		Row.Index = Index;
		Row.Impact = Speed > 0.0
			? std::max(1.0, std::ceil(Gap / (Speed / Data.SimStepHz) - TickCeilBias))
			: std::numeric_limits<double>::infinity();
		Incoming.Add(Row);
	}
	Algo::Sort(Incoming, [](const FIncoming& Left, const FIncoming& Right)
	{
		if (Left.Impact != Right.Impact)
		{
			return Left.Impact < Right.Impact;
		}
		return Left.Index < Right.Index;
	});
	if (Incoming.Num() > 0)
	{
		const FProjectile& Projectile = Training.State.Projectiles[Incoming[0].Index];
		Input.Aim = Projectile.Pos;
		Input.bAbsorb = Incoming[0].Impact <= static_cast<double>(Lead) + 1.0;
	}
	return Input;
}

void StepTraining(FTraining& Training, const FInputFrame& Input)
{
	ScheduleTraining(Training);
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(Training.PlayerId, Input);
	StepArena(Training.State, Inputs);
	if (Training.Kind == TEXT("performance"))
	{
		ScheduleTraining(Training);
	}
}

FTimingReport RunTimingBot(const FString& Kind, TOptional<double> DurationS, TOptional<int32> LeadTicks)
{
	const FKernelData& Data = KernelData();
	const double Duration = DurationS.Get(Data.ReportDurationS);
	FTraining Training = CreateTraining(TEXT("magic"));
	FTimingReport Report;
	Report.Kind = Kind;
	Report.DurationS = Duration;
	int32 Downs = 0;
	const int32 Steps = SimTicks(Duration);
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		StepTraining(Training, TimingBot(Training, Kind, LeadTicks));
		FActor& Player = *SimFindActor(Training.State, Training.PlayerId);
		if (Player.bDown)
		{
			++Downs;
			Player.bDown = false;
			Player.Hp = Player.MaxHp;
		}
	}
	const FActor& Player = *SimFindActor(Training.State, Training.PlayerId);
	Report.Attacks = Training.Attacks;
	Report.Perfects = Player.Metrics.Perfects;
	Report.Hits = Player.Metrics.Hits;
	Report.Blocks = Player.Metrics.Blocks;
	Report.ManaReturned = Player.Metrics.ManaReturned;
	Report.ManaDrained = Player.Metrics.ManaDrained;
	Report.DamageTaken = Player.Metrics.DamageTaken;
	Report.Downs = Downs;
	Report.FinalMana = Player.Mana;
	Report.PerfectRate = Player.Metrics.Hits == 0 ? 0.0 : static_cast<double>(Player.Metrics.Perfects) / static_cast<double>(Player.Metrics.Hits);
	return Report;
}
