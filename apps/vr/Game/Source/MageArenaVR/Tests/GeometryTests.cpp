#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "Kernel/Enemies.h"
#include "Kernel/Games.h"
#include "Kernel/Geometry.h"

#include <cmath>

namespace
{
bool Ready(FAutomationTestBase& Test)
{
	if (KernelData().bReady)
	{
		return true;
	}
	Test.AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
	return false;
}

void StepInput(FArenaState& State, int32 ActorId, const FInputFrame& Input)
{
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(ActorId, Input);
	StepArena(State, Inputs);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelGeometry, "MageArena.Kernel.Geometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelGeometry::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	const FKernelData& Data = KernelData();
	bPass &= TestEqual(TEXT("centre x"), Data.ArenaCentre.X, 16.0);
	bPass &= TestEqual(TEXT("centre y"), Data.ArenaCentre.Y, 62.0);
	bPass &= TestEqual(TEXT("width"), Data.ArenaWidthM, 192.0);
	bPass &= TestEqual(TEXT("height"), Data.ArenaHeightM, 144.0);
	bPass &= TestTrue(TEXT("west rim"), ArenaContains(FSimVec{-80.0, 62.0}));
	bPass &= TestTrue(TEXT("south rim"), ArenaContains(FSimVec{16.0, -10.0}));
	bPass &= TestFalse(TEXT("corner"), ArenaContains(FSimVec{-80.0, -10.0}));
	for (int32 Index = 0; Index < 72; ++Index)
	{
		for (const double Radius : {0.38, 0.7, 2.0})
		{
			const double Angle = static_cast<double>(Index) * SimPi / 36.0;
			const FSimVec Point = ConstrainToArena(FSimVec{16.0 + 200.0 * std::cos(Angle), 62.0 + 200.0 * std::sin(Angle)}, Radius);
			bPass &= TestTrue(TEXT("disk inside"), ArenaContains(Point, Radius));
			for (int32 Sample = 0; Sample < 36; ++Sample)
			{
				const double SampleAngle = static_cast<double>(Sample) * SimPi / 18.0;
				const FSimVec Rim{Point.X + Radius * std::cos(SampleAngle), Point.Y + Radius * std::sin(SampleAngle)};
				bPass &= TestTrue(TEXT("rim sample"), ArenaContains(Rim));
			}
		}
	}
	const FSimVec Interior{13.2345, 10.111};
	const FSimVec Kept = ConstrainToArena(Interior, 0.38);
	bPass &= TestEqual(TEXT("interior x"), Kept.X, Interior.X);
	bPass &= TestEqual(TEXT("interior y"), Kept.Y, Interior.Y);

	{
		FArenaState State = CreateArena();
		const int32 PlayerId = AddMage(State, 0, FSimVec{31.0, 20.0}).Id;
		for (int32 Index = 0; Index < SimTicks(2.0); ++Index)
		{
			FInputFrame Input = SimIdleInput(FSimVec{100.0, 20.0});
			Input.Move = FSimVec{1.0, 0.0};
			StepInput(State, PlayerId, Input);
		}
		bPass &= TestTrue(TEXT("past the old court"), SimFindActor(State, PlayerId)->Pos.X > 39.0);
		FActor& Player = *SimFindActor(State, PlayerId);
		Player.Pos = FSimVec{110.0, 62.0};
		Player.PreviousPos = Player.Pos;
		for (int32 Index = 0; Index < SimTicks(2.0); ++Index)
		{
			FInputFrame Input = SimIdleInput(FSimVec{120.0, 62.0});
			Input.Move = FSimVec{1.0, 1.0};
			Input.bRoll = true;
			StepInput(State, PlayerId, Input);
		}
		const FActor& Rolled = *SimFindActor(State, PlayerId);
		bPass &= TestTrue(TEXT("roll stays inside"), ArenaContains(Rolled.Pos, Rolled.Radius));
		const int32 ThornId = AddEnemy(State, TEXT("thornback"), FSimVec{108.0, 64.0}).Id;
		const FActor& Thorn = *SimFindActor(State, ThornId);
		FTelegraph Telegraph;
		Telegraph.Id = 100;
		Telegraph.ActivationId = 100;
		Telegraph.OwnerId = ThornId;
		Telegraph.Source = Thorn.Pos;
		Telegraph.Origin = Thorn.Pos;
		Telegraph.Target = FSimVec{200.0, 100.0};
		Telegraph.Kind = TEXT("charge");
		Telegraph.StartTick = State.Tick;
		Telegraph.ResolveTick = State.Tick + 1;
		Telegraph.Damage = 0.0;
		Telegraph.Family = TEXT("unblockable");
		Telegraph.Tier = 1;
		Telegraph.SpeedMps = 0.0;
		Telegraph.RangeM = 40.0;
		Telegraph.WidthM = 2.0;
		Telegraph.WallStunS = 1.0;
		State.Telegraphs.Add(Telegraph);
		StepArena(State);
		const FActor& Charged = *SimFindActor(State, ThornId);
		bPass &= TestTrue(TEXT("charge stays inside"), ArenaContains(Charged.Pos, Charged.Radius));
		bPass &= TestTrue(TEXT("charge stuns"), Charged.Enemy.IsSet() && Charged.Enemy->StunnedUntil > State.Tick);
	}

	for (int32 Seed = 0; Seed < 100; ++Seed)
	{
		for (int32 Wave = 0; Wave < 4; ++Wave)
		{
			FGames Games;
			if (!TryCreateGames(Games, static_cast<uint32>(Seed), nullptr, Wave, false))
			{
				AddError(FString::Printf(TEXT("create failed seed %d wave %d"), Seed, Wave));
				return false;
			}
			for (const FActor& Actor : Games.State.Actors)
			{
				if (!ArenaContains(Actor.Pos, Actor.Radius))
				{
					AddError(FString::Printf(TEXT("outside seed %d wave %d id %d"), Seed, Wave, Actor.Id));
					bPass = false;
				}
				for (const FActor& Other : Games.State.Actors)
				{
					if (Actor.Id != Other.Id && SimDistance(Actor.Pos, Other.Pos) < Data.OpeningSeparationM)
					{
						AddError(FString::Printf(TEXT("spacing seed %d wave %d %d vs %d"), Seed, Wave, Actor.Id, Other.Id));
						bPass = false;
					}
				}
			}
		}
	}
	return bPass;
}

#endif
