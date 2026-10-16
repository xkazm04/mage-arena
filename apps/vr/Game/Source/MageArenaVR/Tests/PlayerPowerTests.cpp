#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Sweep 7 (architecture review P3 / F1): the one player power method, FVrRuleset::PlayerPowerMult, and the three kernel
// call sites that use it. The conformance vectors run with Rules == nullptr, so they do not guard this rule.

#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/KernelData.h"
#include "Kernel/VrRules.h"

namespace
{
const double kBoltPower = 1.7;
const double kLinePower = 2.3;

FVrRuleset PowerRules(bool bActive)
{
	FVrRuleset Rules;
	Rules.bActive = bActive;
	Rules.Split.bEnabled = true;
	Rules.Power.BoltDamage = kBoltPower;
	Rules.Power.LineDamage = kLinePower;
	return Rules;
}

enum class EPowerPath
{
	LegacyBoltRelease,
	NormalCast,
	SplitCast,
};

// Damage of the first projectile the player's Rain Needle (tier 0 bolt) puts in the arena, or -1 when none appears.
double FirstProjectileDamage(EPowerPath Path, FVrRuleset* Rules)
{
	FArenaState State = CreateArena(5);
	FActor& Player = AddMage(State, 0, FSimVec{10.0, 10.0});
	Player.Facing = FSimVec{1.0, 0.0};
	const int32 PlayerId = Player.Id;
	AddMage(State, 1, FSimVec{18.0, 10.0});
	const FSimVec Aim{18.0, 10.0};
	if (Path == EPowerPath::LegacyBoltRelease)
	{
		FPendingCast Pending;
		Pending.Kind = TEXT("bolt");
		Pending.StartTick = 0;
		Pending.ReleaseTick = 0;
		Pending.Aim = Aim;
		Pending.ActivationId = State.NextId++;
		SimFindActor(State, PlayerId)->Pending = Pending;
	}
	for (int32 Tick = 0; Tick < 12; ++Tick)
	{
		FInputFrame Input = SimIdleInput(Aim);
		Input.Slot = 0;
		if (Path == EPowerPath::SplitCast)
		{
			Input.bAbsorb = true;
		}
		Input.bCast = Path != EPowerPath::LegacyBoltRelease && Tick == 3;
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(PlayerId, Input);
		StepArena(State, Inputs, Rules);
		for (const FProjectile& Projectile : State.Projectiles)
		{
			if (Projectile.OwnerId == PlayerId)
			{
				return Projectile.Damage;
			}
		}
	}
	return -1.0;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPlayerPowerMethod, "MageArena.PlayerPower.Method",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPlayerPowerMethod::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!KernelData().bReady)
	{
		AddError(TEXT("kernel data failed to load"));
		return false;
	}
	FArenaState State = CreateArena(5);
	const FActor& Player = AddMage(State, 0, FSimVec{10.0, 10.0});
	const FActor& Foe = AddMage(State, 1, FSimVec{14.0, 10.0});
	// No shipped Water spell is a "line" kind (those are Fire and Air), so the line branch is covered with a stub.
	FSpell Line;
	Line.Kind = TEXT("line");
	FSpell Projectile;
	Projectile.Kind = TEXT("projectile");
	FSpell Cone;
	Cone.Kind = TEXT("cone");
	const FVrRuleset Active = PowerRules(true);
	const FVrRuleset Inactive = PowerRules(false);
	bool bPass = TestEqual(TEXT("active line"), Active.PlayerPowerMult(Player, &Line), kLinePower);
	bPass &= TestEqual(TEXT("active projectile"), Active.PlayerPowerMult(Player, &Projectile), kBoltPower);
	bPass &= TestEqual(TEXT("active null spell is the legacy bolt"), Active.PlayerPowerMult(Player, nullptr), kBoltPower);
	bPass &= TestEqual(TEXT("active cone is 1"), Active.PlayerPowerMult(Player, &Cone), 1.0);
	bPass &= TestEqual(TEXT("opponent team is 1"), Active.PlayerPowerMult(Foe, &Projectile), 1.0);
	bPass &= TestEqual(TEXT("inactive normal path is 1"), Inactive.PlayerPowerMult(Player, &Projectile), 1.0);
	bPass &= TestEqual(TEXT("inactive legacy bolt is 1"), Inactive.PlayerPowerMult(Player, nullptr), 1.0);
	bPass &= TestEqual(TEXT("inactive split line (no bActive check, F1)"), Inactive.PlayerPowerMult(Player, &Line, false), kLinePower);
	bPass &= TestEqual(TEXT("inactive split projectile (no bActive check, F1)"), Inactive.PlayerPowerMult(Player, &Projectile, false), kBoltPower);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPlayerPowerPaths, "MageArena.PlayerPower.Paths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPlayerPowerPaths::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!KernelData().bReady)
	{
		AddError(TEXT("kernel data failed to load"));
		return false;
	}
	const double Tol = 1.0e-9;
	bool bPass = true;
	{
		const double Base = FirstProjectileDamage(EPowerPath::LegacyBoltRelease, nullptr);
		FVrRuleset Active = PowerRules(true);
		FVrRuleset Inactive = PowerRules(false);
		bPass &= TestTrue(TEXT("legacy bolt spawns"), Base > 0.0);
		bPass &= TestEqual(TEXT("legacy bolt, active"), FirstProjectileDamage(EPowerPath::LegacyBoltRelease, &Active), Base * kBoltPower, Tol);
		bPass &= TestEqual(TEXT("legacy bolt, inactive"), FirstProjectileDamage(EPowerPath::LegacyBoltRelease, &Inactive), Base, Tol);
	}
	{
		const double Base = FirstProjectileDamage(EPowerPath::NormalCast, nullptr);
		FVrRuleset Active = PowerRules(true);
		FVrRuleset Inactive = PowerRules(false);
		bPass &= TestTrue(TEXT("normal cast spawns"), Base > 0.0);
		bPass &= TestEqual(TEXT("normal cast, active"), FirstProjectileDamage(EPowerPath::NormalCast, &Active), Base * kBoltPower, Tol);
		bPass &= TestEqual(TEXT("normal cast, inactive"), FirstProjectileDamage(EPowerPath::NormalCast, &Inactive), Base, Tol);
	}
	{
		const double Base = FirstProjectileDamage(EPowerPath::NormalCast, nullptr);
		FVrRuleset Active = PowerRules(true);
		FVrRuleset Inactive = PowerRules(false);
		const double OneHand = Active.Split.OneHandPower;
		const double Split = FirstProjectileDamage(EPowerPath::SplitCast, &Active);
		bPass &= TestTrue(TEXT("split cast spawns"), Split > 0.0);
		bPass &= TestEqual(TEXT("split cast, active"), Split, Base * OneHand * kBoltPower, Tol);
		// Deliberate for now (review F1): the split path has no bActive check, so an inactive ruleset still scales.
		bPass &= TestEqual(TEXT("split cast, inactive"), FirstProjectileDamage(EPowerPath::SplitCast, &Inactive), Base * OneHand * kBoltPower, Tol);
	}
	return bPass;
}

#endif
