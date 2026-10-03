#pragma once

#include "Kernel/SimTypes.h"

struct FTraining
{
	FArenaState State;
	int32 PlayerId = 0;
	int32 DummyId = 0;
	FString Kind;
	int32 NextAttack = 0;
	int32 Attacks = 0;
};

struct FTimingReport
{
	FString Kind;
	double DurationS = 0.0;
	int32 Attacks = 0;
	int32 Perfects = 0;
	int32 Hits = 0;
	int32 Blocks = 0;
	double ManaReturned = 0.0;
	double ManaDrained = 0.0;
	double DamageTaken = 0.0;
	int32 Downs = 0;
	double FinalMana = 0.0;
	double PerfectRate = 0.0;
};

FTraining CreateTraining(const FString& Kind = TEXT("magic"), uint32 Seed = 1);
void ScheduleTraining(FTraining& Training);
FInputFrame TimingBot(const FTraining& Training, const FString& Kind, TOptional<int32> LeadTicks = {});
void StepTraining(FTraining& Training, const FInputFrame& Input);
FTimingReport RunTimingBot(const FString& Kind, TOptional<double> DurationS = {}, TOptional<int32> LeadTicks = {});
