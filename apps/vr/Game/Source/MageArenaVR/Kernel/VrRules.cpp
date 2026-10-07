#include "Kernel/VrRules.h"

#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"
#include "MageArenaVR.h"

#include "Kernel/SimMath.h"

#include <cmath>

bool FVrAabb::Contains(const FSimVec& Point) const
{
	return Point.X >= MinX && Point.X <= MaxX && Point.Y >= MinY && Point.Y <= MaxY;
}

FVrAabb FVrAabb::Expanded(double Margin) const
{
	FVrAabb Box;
	Box.MinX = MinX - Margin;
	Box.MaxX = MaxX + Margin;
	Box.MinY = MinY - Margin;
	Box.MaxY = MaxY + Margin;
	return Box;
}

const FVrAttackMode* FVrRuleset::FindThrow(const FString& EnemyId) const
{
	for (const FVrAttackMode& Mode : Throws)
	{
		if (Mode.bThrow && Mode.EnemyId == EnemyId)
		{
			return &Mode;
		}
	}
	return nullptr;
}

const FVrAttackMode* FVrRuleset::FindSpit(const FString& EnemyId) const
{
	const FVrAttackMode* Mode = FindThrow(EnemyId);
	return Mode && Mode->bSpit ? Mode : nullptr;
}

void VrApplyAttackMode(const FVrAttackMode& Mode, FAttackSpec& Attack)
{
	Attack.ProjectileMps = Mode.ProjectileMps;
	Attack.RangeM = Mode.RangeM;
	if (Mode.bSpit)
	{
		Attack.Family = Mode.Family;
		Attack.Tier = Mode.Tier;
		Attack.Damage = Mode.Damage;
		Attack.WindupS = Mode.WindupS;
	}
}

FVrAabb FVrRuleset::HoldBox() const
{
	return Dais.Expanded(StandoffM);
}

bool FVrRuleset::InsideDais(const FSimVec& Point) const
{
	return Dais.Contains(Point);
}

bool FVrRuleset::InsideHold(const FSimVec& Point) const
{
	return HoldBox().Contains(Point);
}

double FVrRuleset::OutsideGap(const FSimVec& Point) const
{
	const FVrAabb Box = HoldBox();
	const double Dx = std::max(Box.MinX - Point.X, Point.X - Box.MaxX);
	const double Dy = std::max(Box.MinY - Point.Y, Point.Y - Box.MaxY);
	if (Dx <= 0.0 && Dy <= 0.0)
	{
		return std::max(Dx, Dy);
	}
	return std::hypot(std::max(Dx, 0.0), std::max(Dy, 0.0));
}

FSimVec FVrRuleset::Outward(const FSimVec& Point) const
{
	const FVrAabb Box = HoldBox();
	const FSimVec Centre{(Box.MinX + Box.MaxX) * 0.5, (Box.MinY + Box.MaxY) * 0.5};
	return SimUnit(SimSub(Point, Centre));
}

FSimVec FVrRuleset::PushOutsideHold(const FSimVec& Point) const
{
	const FVrAabb Box = HoldBox();
	if (!Box.Contains(Point))
	{
		return Point;
	}
	const double Left = Point.X - Box.MinX;
	const double Right = Box.MaxX - Point.X;
	const double Down = Point.Y - Box.MinY;
	const double Up = Box.MaxY - Point.Y;
	const double Eps = 1.0e-4;
	FSimVec Out = Point;
	if (Left <= Right && Left <= Down && Left <= Up)
	{
		Out.X = Box.MinX - Eps;
	}
	else if (Right <= Down && Right <= Up)
	{
		Out.X = Box.MaxX + Eps;
	}
	else if (Down <= Up)
	{
		Out.Y = Box.MinY - Eps;
	}
	else
	{
		Out.Y = Box.MaxY + Eps;
	}
	return Out;
}

FSimVec FVrRuleset::HoldPoint(const FSimVec& From, const FSimVec& Goal) const
{
	auto Finish = [this](const FSimVec& Point) -> FSimVec
	{
		if (!bNarrow)
		{
			return Point;
		}
		FSimVec Held = CompressToArc(Point);
		if (bActive && InsideHold(Held))
		{
			Held = CompressToArc(PushOutsideHold(Held));
		}
		return Held;
	};
	const FVrAabb Box = HoldBox();
	if (!Box.Contains(Goal))
	{
		return Finish(Goal);
	}
	if (Box.Contains(From))
	{
		return Finish(PushOutsideHold(From));
	}
	const FSimVec Delta = SimSub(Goal, From);
	double Enter = -1.0e9;
	double Exit = 1.0e9;
	auto Clip = [&Enter, &Exit](double Origin, double Dir, double Min, double Max) -> bool
	{
		if (std::abs(Dir) < 1.0e-12)
		{
			return Origin >= Min && Origin <= Max;
		}
		double Low = (Min - Origin) / Dir;
		double High = (Max - Origin) / Dir;
		if (Low > High)
		{
			const double Swap = Low;
			Low = High;
			High = Swap;
		}
		Enter = std::max(Enter, Low);
		Exit = std::min(Exit, High);
		return Enter <= Exit;
	};
	if (!Clip(From.X, Delta.X, Box.MinX, Box.MaxX) || !Clip(From.Y, Delta.Y, Box.MinY, Box.MaxY))
	{
		return Finish(PushOutsideHold(From));
	}
	const double T = SimClamp(Enter, 0.0, 1.0);
	return Finish(SimAdd(From, SimScale(Delta, T)));
}

FSimVec FVrRuleset::CompressToArc(const FSimVec& Point) const
{
	if (!bNarrow || !(Narrow.SpawnArcDeg > 0.0) || Narrow.SpawnArcDeg >= 180.0)
	{
		return Point;
	}
	const FSimVec Forward = SimUnit(NarrowForward, FSimVec{1.0, 0.0});
	const FSimVec Right{-Forward.Y, Forward.X};
	const FSimVec Offset = SimSub(Point, NarrowOrigin);
	const double Ahead = SimDot(Offset, Forward);
	const double Side = SimDot(Offset, Right);
	const double Radius = std::hypot(Ahead, Side);
	if (!(Radius > 1.0e-8))
	{
		return Point;
	}
	const double Bearing = std::atan2(Side, Ahead);
	const double Limit = Narrow.SpawnArcDeg * SimPi / 180.0;
	if (std::abs(Bearing) <= Limit)
	{
		return Point;
	}
	const double Clamped = std::copysign(Limit, Bearing);
	return SimAdd(NarrowOrigin, SimAdd(SimScale(Forward, Radius * std::cos(Clamped)), SimScale(Right, Radius * std::sin(Clamped))));
}

void FVrRuleset::KeepInView(FArenaState& State) const
{
	// Narrow view: an opponent outside the arc of the active pad walks back toward the nearest arc point at no more
	// than its own walk speed per tick. Positions are never snapped, so a blink to another pad does not teleport anyone.
	const FKernelData& Data = KernelData();
	for (FActor& Actor : State.Actors)
	{
		if (Actor.bDown || Actor.Team == 0)
		{
			continue;
		}
		const FSimVec Goal = CompressToArc(Actor.Pos);
		const FSimVec Delta = SimSub(Goal, Actor.Pos);
		const double Distance = SimLength(Delta);
		if (!(Distance > 1.0e-9))
		{
			continue;
		}
		const double Step = Actor.SpeedMps.Get(Data.WalkMps) * SimDt();
		Actor.Pos = Distance <= Step ? Goal : SimAdd(Actor.Pos, SimScale(Delta, Step / Distance));
	}
}

void FVrRuleset::KeepOut(FArenaState& State) const
{
	if (!bActive)
	{
		return;
	}
	for (FActor& Actor : State.Actors)
	{
		if (!Actor.Enemy.IsSet())
		{
			continue;
		}
		if (InsideHold(Actor.Pos))
		{
			Actor.Pos = PushOutsideHold(Actor.Pos);
		}
	}
}

bool VrWallSegmentCrosses(const FVrWall& Wall, const FSimVec& From, const FSimVec& To, double& OutT)
{
	// The curtain is the arc, not the disk. A chord that enters and leaves still crosses it,
	// which is how the mage sees a bolt whose whole flight starts and ends outside the ring.
	if (!(Wall.DistanceM > 0.0))
	{
		return false;
	}
	const FSimVec Delta = SimSub(To, From);
	const FSimVec Offset = SimSub(From, Wall.Centre);
	const double A = SimDot(Delta, Delta);
	if (!(A > 0.0))
	{
		return false;
	}
	const double B = 2.0 * SimDot(Offset, Delta);
	const double C = SimDot(Offset, Offset) - Wall.DistanceM * Wall.DistanceM;
	const double Disc = B * B - 4.0 * A * C;
	if (Disc < 0.0)
	{
		return false;
	}
	const double Root = std::sqrt(Disc);
	const double Inv = 1.0 / (2.0 * A);
	const double Hits[] = {(-B - Root) * Inv, (-B + Root) * Inv};
	for (const double T : Hits)
	{
		if (T < 0.0 || T > 1.0)
		{
			continue;
		}
		const FSimVec Point = SimAdd(From, SimScale(Delta, T));
		if (!SimInArc(Wall.Facing, SimSub(Point, Wall.Centre), Wall.ArcDeg))
		{
			continue;
		}
		OutT = T;
		return true;
	}
	return false;
}

const FVrWall* FVrRuleset::FindWall(int32 OwnerId) const
{
	for (const FVrWall& Wall : Walls)
	{
		if (Wall.OwnerId == OwnerId)
		{
			return &Wall;
		}
	}
	return nullptr;
}

void FVrRuleset::ExpireWalls(const FArenaState& State)
{
	for (int32 Index = Walls.Num() - 1; Index >= 0; --Index)
	{
		if (State.Tick >= Walls[Index].UntilTick)
		{
			UE_LOG(LogMageArena, Log, TEXT("defence firewall end id=%d owner=%d tick=%d"), Walls[Index].Id, Walls[Index].OwnerId, State.Tick);
			Walls.RemoveAt(Index);
		}
	}
}

void FVrRuleset::TickWalls(FArenaState& State, FActor& Actor, const FInputFrame& Input)
{
	if (!bActive || !FireWall.bEnabled || !Actor.Fire.bSchool || !Input.bRaiseFireWall)
	{
		return;
	}
	if (FindWall(Actor.Id))
	{
		return;
	}
	if (Actor.Mana + 1.0e-9 < FireWall.ManaCost)
	{
		Refuse(Actor, TEXT("firewall-mana"));
		return;
	}
	Actor.Mana -= FireWall.ManaCost;
	FVrWall Wall;
	Wall.Id = State.NextId++;
	Wall.OwnerId = Actor.Id;
	Wall.Centre = Actor.Pos;
	Wall.Facing = Actor.Facing;
	Wall.DistanceM = FireWall.DistanceM;
	Wall.ArcDeg = FireWall.ArcDeg;
	Wall.UntilTick = State.Tick + SimTicks(FireWall.DurationS);
	Walls.Add(Wall);
	++WallsRaised;
	UE_LOG(LogMageArena, Log, TEXT("defence firewall raise actor=%d until=%d mana=%.3f"), Actor.Id, Wall.UntilTick, Actor.Mana);
}

void FVrRuleset::BurnCrossers(FArenaState& State)
{
	if (!bActive || !FireWall.bEnabled || Walls.Num() == 0)
	{
		return;
	}
	for (FActor& Actor : State.Actors)
	{
		if (Actor.bDown)
		{
			continue;
		}
		for (const FVrWall& Wall : Walls)
		{
			if (Wall.OwnerId == Actor.Id)
			{
				continue;
			}
			const FActor* Owner = SimFindActor(State, Wall.OwnerId);
			if (Owner && Owner->Team == Actor.Team)
			{
				continue;
			}
			double CrossT = 0.0;
			if (!VrWallSegmentCrosses(Wall, Actor.PreviousPos, Actor.Pos, CrossT))
			{
				continue;
			}
			FHit Hit;
			Hit.OwnerId = Wall.OwnerId;
			Hit.ActivationId = Wall.Id;
			Hit.Damage = FireWall.BurnDamage;
			Hit.Family = TEXT("magic");
			Hit.Tier = 0;
			Hit.Source = Wall.Centre;
			Hit.Delivery = TEXT("area");
			UE_LOG(LogMageArena, Log, TEXT("defence firewall burn wall=%d actor=%d damage=%.3f"), Wall.Id, Actor.Id, FireWall.BurnDamage);
			ResolveHit(State, Actor, Hit, this);
		}
	}
}

void FVrRuleset::ClearRuntime()
{
	Walls.Reset();
	WallsRaised = 0;
	StaffRuntime = FVrStaffRuntime();
	SplitCasting.Reset();
	Refusals.Reset();
}

bool FVrRuleset::IsPlanted(int32 ActorId) const
{
	return bActive && Staff.bEnabled && StaffRuntime.bPlanted && StaffRuntime.ActorId == ActorId;
}

void FVrRuleset::Lift(const TCHAR* Why)
{
	if (!StaffRuntime.bPlanted)
	{
		return;
	}
	StaffRuntime.bPlanted = false;
	StaffRuntime.ActorId = -1;
	StaffRuntime.UntilTick = 0;
	UE_LOG(LogMageArena, Log, TEXT("defence staff lift %s"), Why);
}

bool FVrRuleset::IsSplitCasting(int32 ActorId) const
{
	return SplitCasting.Contains(ActorId);
}

void FVrRuleset::SetSplitCasting(int32 ActorId, bool bCasting)
{
	if (bCasting)
	{
		if (!SplitCasting.Contains(ActorId))
		{
			SplitCasting.Add(ActorId);
		}
		return;
	}
	SplitCasting.Remove(ActorId);
}

double FVrRuleset::DomeReduction(const FString& Family) const
{
	if (!Staff.bEnabled)
	{
		return 0.0;
	}
	if (Family == TEXT("magic"))
	{
		return Staff.MagicReduction;
	}
	if (Family == TEXT("physical"))
	{
		return Staff.PhysicalReduction;
	}
	return 0.0;
}

void FVrRuleset::Refuse(const FActor& Actor, const TCHAR* Reason)
{
	Refusals.Add(Reason);
	UE_LOG(LogMageArena, Log, TEXT("defence refuse %s actor=%d tier=%d flow=%d"), Reason, Actor.Id, Actor.Tier, Actor.Water.Flow);
}

void FVrRuleset::TickStaff(FArenaState& State, FActor& Actor, const FInputFrame& Input)
{
	if (!bActive || !Staff.bEnabled || Actor.Enemy.IsSet())
	{
		return;
	}
	if (IsPlanted(Actor.Id) && State.Tick >= StaffRuntime.UntilTick)
	{
		Lift(TEXT("expiry"));
	}
	if (IsPlanted(Actor.Id) && Input.bLiftStaff)
	{
		Lift(TEXT("gesture"));
	}
	if (!IsPlanted(Actor.Id) && Input.bPlantStaff && !Input.bLiftStaff)
	{
		if (Actor.Mana + 1.0e-9 >= Staff.ManaUpFront)
		{
			Actor.Mana -= Staff.ManaUpFront;
			StaffRuntime.bPlanted = true;
			StaffRuntime.ActorId = Actor.Id;
			StaffRuntime.UntilTick = State.Tick + SimTicks(Staff.DurationS);
			UE_LOG(LogMageArena, Log, TEXT("defence staff plant actor=%d until=%d mana=%.3f"), Actor.Id, StaffRuntime.UntilTick, Actor.Mana);
		}
		else
		{
			Refuse(Actor, TEXT("staff-mana"));
		}
	}
	if (!IsPlanted(Actor.Id))
	{
		return;
	}
	const double Cost = Staff.DrainPerSecond * SimDt();
	const double Paid = std::min(Actor.Mana, Cost);
	Actor.Mana -= Paid;
	if (Paid + 1.0e-12 < Cost || Actor.Mana <= ManaExhaustedEpsilon)
	{
		Actor.Mana = std::max(0.0, Actor.Mana);
		Lift(TEXT("mana"));
	}
}
