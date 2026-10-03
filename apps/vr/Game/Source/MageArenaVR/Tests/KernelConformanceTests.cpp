#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Enemies.h"
#include "Kernel/Games.h"
#include "Kernel/MageAI.h"
#include "Kernel/Training.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

namespace
{
FString ConformanceDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/conformance")));
}

struct FCheck
{
	FAutomationTestBase& Test;
	double Rel = 1.0e-6;
	double Abs = 1.0e-9;
	int32 Tick = 0;
	FString When;
	bool bFailed = false;

	bool Fail(const FString& Field, const FString& Detail)
	{
		if (!bFailed)
		{
			bFailed = true;
			Test.AddError(FString::Printf(TEXT("tick %d %s %s: %s"), Tick, *When, *Field, *Detail));
		}
		return false;
	}

	bool Close(double Actual, double Expected) const
	{
		if (Actual == Expected)
		{
			return true;
		}
		const double Diff = std::abs(Actual - Expected);
		const double Scale = std::max(std::abs(Actual), std::abs(Expected));
		return Diff <= Abs || Diff <= Rel * Scale;
	}

	bool WantDouble(const FJsonObject& Object, const TCHAR* Field, double Actual, const FString& Path)
	{
		if (bFailed)
		{
			return false;
		}
		double Expected = 0.0;
		if (!Object.TryGetNumberField(Field, Expected))
		{
			return Fail(Path + TEXT(".") + Field, TEXT("missing number"));
		}
		if (!Close(Actual, Expected))
		{
			const double Diff = std::abs(Actual - Expected);
			const double Scale = std::max(std::abs(Actual), std::abs(Expected));
			return Fail(Path + TEXT(".") + Field, FString::Printf(TEXT("expected %.17g got %.17g abs %.3g rel %.3g"), Expected, Actual, Diff, Scale > 0.0 ? Diff / Scale : Diff));
		}
		return true;
	}

	bool WantInt(const FJsonObject& Object, const TCHAR* Field, int32 Actual, const FString& Path)
	{
		if (bFailed)
		{
			return false;
		}
		double Number = 0.0;
		if (!Object.TryGetNumberField(Field, Number))
		{
			return Fail(Path + TEXT(".") + Field, TEXT("missing number"));
		}
		const int32 Expected = static_cast<int32>(std::llround(Number));
		if (std::abs(Number - static_cast<double>(Expected)) > 1.0e-6)
		{
			return Fail(Path + TEXT(".") + Field, TEXT("not an integer"));
		}
		if (Actual != Expected)
		{
			return Fail(Path + TEXT(".") + Field, FString::Printf(TEXT("expected %d got %d"), Expected, Actual));
		}
		return true;
	}

	bool WantUInt(const FJsonObject& Object, const TCHAR* Field, uint32 Actual, const FString& Path)
	{
		if (bFailed)
		{
			return false;
		}
		double Number = 0.0;
		if (!Object.TryGetNumberField(Field, Number))
		{
			return Fail(Path + TEXT(".") + Field, TEXT("missing number"));
		}
		const int64 Rounded = std::llround(Number);
		if (Rounded < 0 || Rounded > 4294967295LL || std::abs(Number - static_cast<double>(Rounded)) > 1.0e-6)
		{
			return Fail(Path + TEXT(".") + Field, TEXT("not a uint32"));
		}
		if (Actual != static_cast<uint32>(Rounded))
		{
			return Fail(Path + TEXT(".") + Field, FString::Printf(TEXT("expected %u got %u"), static_cast<uint32>(Rounded), Actual));
		}
		return true;
	}

	bool WantBool(const FJsonObject& Object, const TCHAR* Field, bool Actual, const FString& Path)
	{
		if (bFailed)
		{
			return false;
		}
		bool Expected = false;
		if (!Object.TryGetBoolField(Field, Expected))
		{
			return Fail(Path + TEXT(".") + Field, TEXT("missing bool"));
		}
		if (Expected != Actual)
		{
			return Fail(Path + TEXT(".") + Field, FString::Printf(TEXT("expected %s got %s"), Expected ? TEXT("true") : TEXT("false"), Actual ? TEXT("true") : TEXT("false")));
		}
		return true;
	}

	bool WantString(const FJsonObject& Object, const TCHAR* Field, const FString& Actual, const FString& Path)
	{
		if (bFailed)
		{
			return false;
		}
		FString Expected;
		if (!Object.TryGetStringField(Field, Expected))
		{
			return Fail(Path + TEXT(".") + Field, TEXT("missing string"));
		}
		if (Expected != Actual)
		{
			return Fail(Path + TEXT(".") + Field, FString::Printf(TEXT("expected %s got %s"), *Expected, *Actual));
		}
		return true;
	}
};

bool ReadInput(FCheck& Check, const FJsonObject& Object, FInputFrame& Out)
{
	const TSharedPtr<FJsonObject>* Move = nullptr;
	const TSharedPtr<FJsonObject>* Aim = nullptr;
	if (!Object.TryGetObjectField(TEXT("move"), Move) || !Move || !Move->IsValid()
		|| !Object.TryGetObjectField(TEXT("aim"), Aim) || !Aim || !Aim->IsValid())
	{
		return Check.Fail(TEXT("input"), TEXT("missing move or aim"));
	}
	double MoveX = 0.0;
	double MoveY = 0.0;
	double AimX = 0.0;
	double AimY = 0.0;
	double Slot = 0.0;
	if (!(*Move)->TryGetNumberField(TEXT("x"), MoveX) || !(*Move)->TryGetNumberField(TEXT("y"), MoveY)
		|| !(*Aim)->TryGetNumberField(TEXT("x"), AimX) || !(*Aim)->TryGetNumberField(TEXT("y"), AimY)
		|| !Object.TryGetNumberField(TEXT("slot"), Slot))
	{
		return Check.Fail(TEXT("input"), TEXT("aim, move, or slot is not a number"));
	}
	Out.Move = FSimVec{MoveX, MoveY};
	Out.Aim = FSimVec{AimX, AimY};
	Out.Slot = static_cast<int32>(std::llround(Slot));
	if (!Object.TryGetBoolField(TEXT("cast"), Out.bCast) || !Object.TryGetBoolField(TEXT("absorb"), Out.bAbsorb)
		|| !Object.TryGetBoolField(TEXT("roll"), Out.bRoll) || !Object.TryGetBoolField(TEXT("sprint"), Out.bSprint))
	{
		return Check.Fail(TEXT("input"), TEXT("missing cast, absorb, roll, or sprint"));
	}
	return true;
}

bool ApplySetup(FCheck& Check, FArenaState& State, const FJsonObject& Setup)
{
	const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
	if (!Setup.TryGetArrayField(TEXT("actors"), Actors) || !Actors)
	{
		return Check.Fail(TEXT("setup.actors"), TEXT("missing"));
	}
	const FKernelData& Data = KernelData();
	for (int32 Index = 0; Index < Actors->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* ActorJson = nullptr;
		if (!(*Actors)[Index].IsValid() || !(*Actors)[Index]->TryGetObject(ActorJson) || !ActorJson || !ActorJson->IsValid())
		{
			return Check.Fail(TEXT("setup.actors"), TEXT("entry is not an object"));
		}
		const FJsonObject& Spec = **ActorJson;
		double X = 0.0;
		double Y = 0.0;
		double Team = 0.0;
		if (!Spec.TryGetNumberField(TEXT("x"), X) || !Spec.TryGetNumberField(TEXT("y"), Y) || !Spec.TryGetNumberField(TEXT("team"), Team))
		{
			return Check.Fail(TEXT("setup.actor"), TEXT("missing position or team"));
		}
		const TSharedPtr<FJsonObject>* RanksJson = nullptr;
		if (!Spec.TryGetObjectField(TEXT("ranks"), RanksJson) || !RanksJson || !RanksJson->IsValid())
		{
			return Check.Fail(TEXT("setup.ranks"), TEXT("missing"));
		}
		double Vigor = 0.0;
		double Focus = 0.0;
		double Nerve = 0.0;
		if (!(*RanksJson)->TryGetNumberField(TEXT("vigor"), Vigor) || !(*RanksJson)->TryGetNumberField(TEXT("focus"), Focus) || !(*RanksJson)->TryGetNumberField(TEXT("nerve"), Nerve))
		{
			return Check.Fail(TEXT("setup.ranks"), TEXT("incomplete"));
		}
		FSimRanks Ranks;
		Ranks.Vigor = static_cast<int32>(std::llround(Vigor));
		Ranks.Focus = static_cast<int32>(std::llround(Focus));
		Ranks.Nerve = static_cast<int32>(std::llround(Nerve));
		FString Label;
		if (!Spec.TryGetStringField(TEXT("label"), Label))
		{
			return Check.Fail(TEXT("setup.label"), TEXT("missing"));
		}
		FActor* Actor = nullptr;
		const TSharedPtr<FJsonValue> EnemyField = Spec.TryGetField(TEXT("enemy"));
		if (EnemyField.IsValid() && !EnemyField->IsNull())
		{
			FString EnemyId;
			if (!EnemyField->TryGetString(EnemyId))
			{
				return Check.Fail(TEXT("setup.enemy"), TEXT("not a string"));
			}
			Actor = &AddEnemy(State, EnemyId, FSimVec{X, Y});
		}
		else
		{
			Actor = &AddMage(State, static_cast<int32>(std::llround(Team)), FSimVec{X, Y}, Label, &Ranks);
		}
		double Id = 0.0;
		if (!Spec.TryGetNumberField(TEXT("id"), Id) || Actor->Id != static_cast<int32>(std::llround(Id)))
		{
			return Check.Fail(TEXT("setup.id"), FString::Printf(TEXT("expected %d got %d"), static_cast<int32>(std::llround(Id)), Actor->Id));
		}
		Actor->Team = static_cast<int32>(std::llround(Team));
		Actor->Label = Label;
		Actor->Ranks = Ranks;
		double MaxHp = 0.0;
		double Hp = 0.0;
		double Mana = 0.0;
		if (!Spec.TryGetNumberField(TEXT("maxHp"), MaxHp) || !Spec.TryGetNumberField(TEXT("hp"), Hp) || !Spec.TryGetNumberField(TEXT("mana"), Mana))
		{
			return Check.Fail(TEXT("setup.resources"), TEXT("missing"));
		}
		Actor->MaxHp = MaxHp;
		Actor->Hp = Hp;
		Actor->MaxMana = Data.ManaBase + Data.ManaPerRank * static_cast<double>(Ranks.Focus);
		Actor->Mana = Mana;
		Actor->MaxStamina = Data.StaminaBase + Data.StaminaPerRank * static_cast<double>(Ranks.Vigor);
		Actor->Stamina = Actor->MaxStamina;
		const TSharedPtr<FJsonObject>* CompositionJson = nullptr;
		if (!Spec.TryGetObjectField(TEXT("composition"), CompositionJson) || !CompositionJson || !CompositionJson->IsValid())
		{
			return Check.Fail(TEXT("setup.composition"), TEXT("missing"));
		}
		FComposition Composition;
		if (!(*CompositionJson)->TryGetStringField(TEXT("name"), Composition.Name))
		{
			return Check.Fail(TEXT("setup.composition.name"), TEXT("missing"));
		}
		const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
		if (!(*CompositionJson)->TryGetArrayField(TEXT("lines"), Lines) || !Lines)
		{
			return Check.Fail(TEXT("setup.composition.lines"), TEXT("missing"));
		}
		for (const TSharedPtr<FJsonValue>& Line : *Lines)
		{
			FString Text;
			if (!Line.IsValid() || !Line->TryGetString(Text))
			{
				return Check.Fail(TEXT("setup.composition.lines"), TEXT("not a string"));
			}
			Composition.Lines.Add(Text);
		}
		const TSharedPtr<FJsonObject>* Branches = nullptr;
		if (!(*CompositionJson)->TryGetObjectField(TEXT("branches"), Branches) || !Branches || !Branches->IsValid()
			|| !(*Branches)->TryGetStringField(TEXT("lash"), Composition.Branches.Lash)
			|| !(*Branches)->TryGetStringField(TEXT("mirror"), Composition.Branches.Mirror)
			|| !(*Branches)->TryGetStringField(TEXT("tide_orb"), Composition.Branches.TideOrb))
		{
			return Check.Fail(TEXT("setup.composition.branches"), TEXT("missing"));
		}
		Actor->Water = NewWaterState(&Composition);
		double Flow = 0.0;
		if (!Spec.TryGetNumberField(TEXT("flow"), Flow))
		{
			return Check.Fail(TEXT("setup.flow"), TEXT("missing"));
		}
		Actor->Water.Flow = static_cast<int32>(std::llround(Flow));
		double Tier = 0.0;
		double LastUnlock = 0.0;
		double Clock = 0.0;
		double Fresh = 0.0;
		double Release = 0.0;
		if (!Spec.TryGetNumberField(TEXT("tier"), Tier) || !Spec.TryGetNumberField(TEXT("lastUnlockTick"), LastUnlock)
			|| !Spec.TryGetNumberField(TEXT("clockAdvanceTicks"), Clock)
			|| !Spec.TryGetNumberField(TEXT("absorbFreshTick"), Fresh)
			|| !Spec.TryGetNumberField(TEXT("releaseTick"), Release))
		{
			return Check.Fail(TEXT("setup.clock"), TEXT("missing"));
		}
		Actor->Tier = static_cast<int32>(std::llround(Tier));
		Actor->LastUnlockTick = static_cast<int32>(std::llround(LastUnlock));
		Actor->ClockAdvanceTicks = static_cast<int32>(std::llround(Clock));
		Actor->AbsorbFreshTick = static_cast<int32>(std::llround(Fresh));
		Actor->ReleaseTick = static_cast<int32>(std::llround(Release));
		if (!Spec.TryGetBoolField(TEXT("dummy"), Actor->bDummy) || !Spec.TryGetBoolField(TEXT("absorb"), Actor->bAbsorb)
			|| !Spec.TryGetBoolField(TEXT("absorbExhausted"), Actor->bAbsorbExhausted))
		{
			return Check.Fail(TEXT("setup.flags"), TEXT("missing"));
		}
		const TSharedPtr<FJsonObject>* Facing = nullptr;
		double FaceX = 0.0;
		double FaceY = 0.0;
		if (!Spec.TryGetObjectField(TEXT("facing"), Facing) || !Facing || !Facing->IsValid()
			|| !(*Facing)->TryGetNumberField(TEXT("x"), FaceX) || !(*Facing)->TryGetNumberField(TEXT("y"), FaceY))
		{
			return Check.Fail(TEXT("setup.facing"), TEXT("missing"));
		}
		Actor->Facing = FSimVec{FaceX, FaceY};
	}
	return !Check.bFailed;
}

bool CompareComposition(FCheck& Check, const FJsonObject& Object, const FComposition& Composition, const FString& Path)
{
	const TSharedPtr<FJsonObject>* Json = nullptr;
	if (!Object.TryGetObjectField(TEXT("composition"), Json) || !Json || !Json->IsValid())
	{
		return Check.Fail(Path + TEXT(".composition"), TEXT("missing"));
	}
	if (!Check.WantString(**Json, TEXT("name"), Composition.Name, Path + TEXT(".composition")))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
	if (!(*Json)->TryGetArrayField(TEXT("lines"), Lines) || !Lines)
	{
		return Check.Fail(Path + TEXT(".composition.lines"), TEXT("missing"));
	}
	if (Lines->Num() != Composition.Lines.Num())
	{
		return Check.Fail(Path + TEXT(".composition.lines"), FString::Printf(TEXT("expected %d got %d"), Lines->Num(), Composition.Lines.Num()));
	}
	for (int32 Index = 0; Index < Lines->Num(); ++Index)
	{
		FString Line;
		if (!(*Lines)[Index].IsValid() || !(*Lines)[Index]->TryGetString(Line) || Line != Composition.Lines[Index])
		{
			return Check.Fail(Path + TEXT(".composition.lines"), FString::Printf(TEXT("index %d"), Index));
		}
	}
	const TSharedPtr<FJsonObject>* Branches = nullptr;
	if (!(*Json)->TryGetObjectField(TEXT("branches"), Branches) || !Branches || !Branches->IsValid())
	{
		return Check.Fail(Path + TEXT(".composition.branches"), TEXT("missing"));
	}
	return Check.WantString(**Branches, TEXT("lash"), Composition.Branches.Lash, Path + TEXT(".branches"))
		&& Check.WantString(**Branches, TEXT("mirror"), Composition.Branches.Mirror, Path + TEXT(".branches"))
		&& Check.WantString(**Branches, TEXT("tide_orb"), Composition.Branches.TideOrb, Path + TEXT(".branches"));
}

bool CompareCooldowns(FCheck& Check, const FJsonObject& Object, const FWaterState& Water, const FString& Path)
{
	const TSharedPtr<FJsonObject>* Json = nullptr;
	if (!Object.TryGetObjectField(TEXT("cooldowns"), Json) || !Json || !Json->IsValid())
	{
		return Check.Fail(Path + TEXT(".cooldowns"), TEXT("missing"));
	}
	if ((*Json)->Values.Num() != Water.Cooldowns.Num())
	{
		return Check.Fail(Path + TEXT(".cooldowns"), FString::Printf(TEXT("count expected %d got %d"), (*Json)->Values.Num(), Water.Cooldowns.Num()));
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Json)->Values)
	{
		double Number = 0.0;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(Number))
		{
			return Check.Fail(Path + TEXT(".cooldowns.") + Pair.Key, TEXT("not a number"));
		}
		const int32 Expected = static_cast<int32>(std::llround(Number));
		bool bFound = false;
		for (const FCooldownEntry& Entry : Water.Cooldowns)
		{
			if (Entry.Line == Pair.Key)
			{
				bFound = true;
				if (Entry.Until != Expected)
				{
					return Check.Fail(Path + TEXT(".cooldowns.") + Pair.Key, FString::Printf(TEXT("expected %d got %d"), Expected, Entry.Until));
				}
			}
		}
		if (!bFound)
		{
			return Check.Fail(Path + TEXT(".cooldowns.") + Pair.Key, TEXT("missing on the actor"));
		}
	}
	return !Check.bFailed;
}

bool CompareActor(FCheck& Check, const FJsonObject& Json, const FActor& Actor, const FString& Path)
{
	if (!Check.WantInt(Json, TEXT("id"), Actor.Id, Path) || !Check.WantInt(Json, TEXT("team"), Actor.Team, Path) || !Check.WantString(Json, TEXT("label"), Actor.Label, Path))
	{
		return false;
	}
	if (!Check.WantDouble(Json, TEXT("x"), Actor.Pos.X, Path) || !Check.WantDouble(Json, TEXT("y"), Actor.Pos.Y, Path)
		|| !Check.WantDouble(Json, TEXT("px"), Actor.PreviousPos.X, Path) || !Check.WantDouble(Json, TEXT("py"), Actor.PreviousPos.Y, Path)
		|| !Check.WantDouble(Json, TEXT("fx"), Actor.Facing.X, Path) || !Check.WantDouble(Json, TEXT("fy"), Actor.Facing.Y, Path)
		|| !Check.WantDouble(Json, TEXT("hp"), Actor.Hp, Path) || !Check.WantDouble(Json, TEXT("mana"), Actor.Mana, Path)
		|| !Check.WantDouble(Json, TEXT("stamina"), Actor.Stamina, Path))
	{
		return false;
	}
	if (!Check.WantBool(Json, TEXT("down"), Actor.bDown, Path) || !Check.WantBool(Json, TEXT("dummy"), Actor.bDummy, Path)
		|| !Check.WantBool(Json, TEXT("absorb"), Actor.bAbsorb, Path))
	{
		return false;
	}
	if (!Check.WantInt(Json, TEXT("absorbFreshTick"), Actor.AbsorbFreshTick, Path) || !Check.WantInt(Json, TEXT("releaseTick"), Actor.ReleaseTick, Path)
		|| !Check.WantInt(Json, TEXT("tier"), Actor.Tier, Path) || !Check.WantInt(Json, TEXT("lastUnlockTick"), Actor.LastUnlockTick, Path)
		|| !Check.WantInt(Json, TEXT("clockAdvanceTicks"), Actor.ClockAdvanceTicks, Path))
	{
		return false;
	}
	const FWaterState& Water = Actor.Water;
	if (!Check.WantInt(Json, TEXT("flow"), Water.Flow, Path) || !Check.WantDouble(Json, TEXT("stored"), Water.Stored, Path)
		|| !Check.WantInt(Json, TEXT("crests"), Water.Crests, Path) || !Check.WantDouble(Json, TEXT("healing"), Water.Healing, Path)
		|| !Check.WantInt(Json, TEXT("controlTicks"), Water.ControlTicks, Path)
		|| !Check.WantInt(Json, TEXT("rootUntil"), Water.RootUntil, Path) || !Check.WantInt(Json, TEXT("encasedUntil"), Water.EncasedUntil, Path)
		|| !Check.WantInt(Json, TEXT("slowUntil"), Water.SlowUntil, Path) || !Check.WantDouble(Json, TEXT("slowMult"), Water.SlowMult, Path)
		|| !Check.WantInt(Json, TEXT("wardUntil"), Water.WardUntil, Path) || !Check.WantInt(Json, TEXT("sheenUntil"), Water.SheenUntil, Path)
		|| !Check.WantInt(Json, TEXT("hotUntil"), Water.HotUntil, Path) || !Check.WantDouble(Json, TEXT("hotPerTick"), Water.HotPerTick, Path))
	{
		return false;
	}
	if (!CompareComposition(Check, Json, Water.Composition, Path) || !CompareCooldowns(Check, Json, Water, Path))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Unlocks = nullptr;
	if (!Json.TryGetArrayField(TEXT("unlocks"), Unlocks) || !Unlocks)
	{
		return Check.Fail(Path + TEXT(".unlocks"), TEXT("missing"));
	}
	if (Unlocks->Num() != Actor.UnlockTicks.Num())
	{
		return Check.Fail(Path + TEXT(".unlocks"), FString::Printf(TEXT("expected %d got %d"), Unlocks->Num(), Actor.UnlockTicks.Num()));
	}
	for (int32 Index = 0; Index < Unlocks->Num(); ++Index)
	{
		double Number = 0.0;
		if (!(*Unlocks)[Index].IsValid() || !(*Unlocks)[Index]->TryGetNumber(Number)
			|| static_cast<int32>(std::llround(Number)) != Actor.UnlockTicks[Index])
		{
			return Check.Fail(Path + TEXT(".unlocks"), FString::Printf(TEXT("index %d"), Index));
		}
	}
	const TSharedPtr<FJsonValue> PendingKind = Json.TryGetField(TEXT("pendingKind"));
	if (!PendingKind.IsValid())
	{
		return Check.Fail(Path + TEXT(".pendingKind"), TEXT("missing"));
	}
	if (PendingKind->IsNull())
	{
		if (Actor.Pending.IsSet())
		{
			return Check.Fail(Path + TEXT(".pendingKind"), TEXT("expected null"));
		}
	}
	else
	{
		FString Kind;
		if (!PendingKind->TryGetString(Kind) || !Actor.Pending.IsSet() || Actor.Pending->Kind != Kind)
		{
			return Check.Fail(Path + TEXT(".pendingKind"), FString::Printf(TEXT("expected %s"), *Kind));
		}
		const TSharedPtr<FJsonValue> Spell = Json.TryGetField(TEXT("pendingSpell"));
		FString SpellId;
		const bool bSpell = Spell.IsValid() && !Spell->IsNull();
		if (bSpell && !Spell->TryGetString(SpellId))
		{
			return Check.Fail(Path + TEXT(".pendingSpell"), TEXT("not a string"));
		}
		if (bSpell != Actor.Pending->SpellId.IsSet() || (bSpell && Actor.Pending->SpellId.GetValue() != SpellId))
		{
			return Check.Fail(Path + TEXT(".pendingSpell"), TEXT("mismatch"));
		}
		if (!Check.WantInt(Json, TEXT("pendingRelease"), Actor.Pending->ReleaseTick, Path))
		{
			return false;
		}
		const TSharedPtr<FJsonValue> Mult = Json.TryGetField(TEXT("pendingDamageMult"));
		const bool bMult = Mult.IsValid() && !Mult->IsNull();
		double MultValue = 0.0;
		if (bMult && !Mult->TryGetNumber(MultValue))
		{
			return Check.Fail(Path + TEXT(".pendingDamageMult"), TEXT("not a number"));
		}
		if (bMult != Actor.Pending->DamageMult.IsSet() || (bMult && !Check.Close(Actor.Pending->DamageMult.GetValue(), MultValue)))
		{
			return Check.Fail(Path + TEXT(".pendingDamageMult"), TEXT("mismatch"));
		}
	}
	const TSharedPtr<FJsonObject>* Metrics = nullptr;
	if (!Json.TryGetObjectField(TEXT("metrics"), Metrics) || !Metrics || !Metrics->IsValid())
	{
		return Check.Fail(Path + TEXT(".metrics"), TEXT("missing"));
	}
	const FString MetricsPath = Path + TEXT(".metrics");
	if (!Check.WantInt(**Metrics, TEXT("perfects"), Actor.Metrics.Perfects, MetricsPath)
		|| !Check.WantInt(**Metrics, TEXT("blocks"), Actor.Metrics.Blocks, MetricsPath)
		|| !Check.WantInt(**Metrics, TEXT("hits"), Actor.Metrics.Hits, MetricsPath)
		|| !Check.WantDouble(**Metrics, TEXT("damageDealt"), Actor.Metrics.DamageDealt, MetricsPath)
		|| !Check.WantDouble(**Metrics, TEXT("damageTaken"), Actor.Metrics.DamageTaken, MetricsPath)
		|| !Check.WantDouble(**Metrics, TEXT("manaDrained"), Actor.Metrics.ManaDrained, MetricsPath)
		|| !Check.WantDouble(**Metrics, TEXT("manaRaised"), Actor.Metrics.ManaRaised, MetricsPath)
		|| !Check.WantDouble(**Metrics, TEXT("manaReturned"), Actor.Metrics.ManaReturned, MetricsPath)
		|| !Check.WantInt(**Metrics, TEXT("casts"), Actor.Metrics.Casts, MetricsPath)
		|| !Check.WantInt(**Metrics, TEXT("rolls"), Actor.Metrics.Rolls, MetricsPath))
	{
		return false;
	}
	const TSharedPtr<FJsonValue> Enemy = Json.TryGetField(TEXT("enemy"));
	if (!Enemy.IsValid())
	{
		return Check.Fail(Path + TEXT(".enemy"), TEXT("missing"));
	}
	if (Enemy->IsNull())
	{
		if (Actor.Enemy.IsSet())
		{
			return Check.Fail(Path + TEXT(".enemy"), TEXT("expected null"));
		}
	}
	else
	{
		const TSharedPtr<FJsonObject>* EnemyJson = nullptr;
		if (!Enemy->TryGetObject(EnemyJson) || !EnemyJson || !EnemyJson->IsValid() || !Actor.Enemy.IsSet())
		{
			return Check.Fail(Path + TEXT(".enemy"), TEXT("mismatch"));
		}
		const FString EnemyPath = Path + TEXT(".enemy");
		if (!Check.WantString(**EnemyJson, TEXT("id"), Actor.Enemy->Id, EnemyPath)
			|| !Check.WantInt(**EnemyJson, TEXT("readyTick"), Actor.Enemy->ReadyTick, EnemyPath)
			|| !Check.WantInt(**EnemyJson, TEXT("backoffUntil"), Actor.Enemy->BackoffUntil, EnemyPath)
			|| !Check.WantInt(**EnemyJson, TEXT("stunnedUntil"), Actor.Enemy->StunnedUntil, EnemyPath)
			|| !Check.WantInt(**EnemyJson, TEXT("attackIndex"), Actor.Enemy->AttackIndex, EnemyPath)
			|| !Check.WantBool(**EnemyJson, TEXT("deathQueued"), Actor.Enemy->bDeathQueued, EnemyPath))
		{
			return false;
		}
	}
	const TSharedPtr<FJsonValue> Speed = Json.TryGetField(TEXT("speedMps"));
	if (!Speed.IsValid())
	{
		return Check.Fail(Path + TEXT(".speedMps"), TEXT("missing"));
	}
	if (Speed->IsNull())
	{
		if (Actor.SpeedMps.IsSet())
		{
			return Check.Fail(Path + TEXT(".speedMps"), TEXT("expected null"));
		}
	}
	else
	{
		double Number = 0.0;
		if (!Speed->TryGetNumber(Number) || !Actor.SpeedMps.IsSet() || !Check.Close(Actor.SpeedMps.GetValue(), Number))
		{
			return Check.Fail(Path + TEXT(".speedMps"), TEXT("mismatch"));
		}
	}
	const TSharedPtr<FJsonValue> Decoy = Json.TryGetField(TEXT("decoy"));
	if (!Decoy.IsValid())
	{
		return Check.Fail(Path + TEXT(".decoy"), TEXT("missing"));
	}
	if (Decoy->IsNull())
	{
		if (Actor.Water.Decoy.IsSet())
		{
			return Check.Fail(Path + TEXT(".decoy"), TEXT("expected null"));
		}
	}
	else
	{
		const TSharedPtr<FJsonObject>* DecoyJson = nullptr;
		if (!Decoy->TryGetObject(DecoyJson) || !DecoyJson || !DecoyJson->IsValid() || !Actor.Water.Decoy.IsSet())
		{
			return Check.Fail(Path + TEXT(".decoy"), TEXT("mismatch"));
		}
		if (!Check.WantDouble(**DecoyJson, TEXT("x"), Actor.Water.Decoy->Pos.X, Path + TEXT(".decoy"))
			|| !Check.WantDouble(**DecoyJson, TEXT("y"), Actor.Water.Decoy->Pos.Y, Path + TEXT(".decoy"))
			|| !Check.WantInt(**DecoyJson, TEXT("until"), Actor.Water.Decoy->Until, Path + TEXT(".decoy")))
		{
			return false;
		}
	}
	return !Check.bFailed;
}

bool CompareEvents(FCheck& Check, const TArray<TSharedPtr<FJsonValue>>& Expected, const TArray<FArenaEvent>& Actual, int32 Count, const TCHAR* Label)
{
	if (Actual.Num() < Count || Expected.Num() < Count)
	{
		return Check.Fail(Label, FString::Printf(TEXT("count expected at least %d, vector %d, sim %d"), Count, Expected.Num(), Actual.Num()));
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const TSharedPtr<FJsonObject>* Json = nullptr;
		if (!Expected[Index].IsValid() || !Expected[Index]->TryGetObject(Json) || !Json || !Json->IsValid())
		{
			return Check.Fail(Label, FString::Printf(TEXT("index %d is not an object"), Index));
		}
		const FString Path = FString::Printf(TEXT("%s[%d]"), Label, Index);
		if (!Check.WantInt(**Json, TEXT("tick"), Actual[Index].Tick, Path) || !Check.WantString(**Json, TEXT("kind"), Actual[Index].Kind, Path)
			|| !Check.WantInt(**Json, TEXT("actorId"), Actual[Index].ActorId, Path) || !Check.WantDouble(**Json, TEXT("value"), Actual[Index].Value, Path))
		{
			return false;
		}
		const TSharedPtr<FJsonValue> Target = (*Json)->TryGetField(TEXT("targetId"));
		if (!Target.IsValid())
		{
			return Check.Fail(Path + TEXT(".targetId"), TEXT("missing"));
		}
		if (Target->IsNull())
		{
			if (Actual[Index].TargetId.IsSet())
			{
				return Check.Fail(Path + TEXT(".targetId"), TEXT("expected null"));
			}
		}
		else
		{
			double Number = 0.0;
			if (!Target->TryGetNumber(Number) || !Actual[Index].TargetId.IsSet()
				|| Actual[Index].TargetId.GetValue() != static_cast<int32>(std::llround(Number)))
			{
				return Check.Fail(Path + TEXT(".targetId"), TEXT("mismatch"));
			}
		}
	}
	return !Check.bFailed;
}

bool CompareGames(FCheck& Check, const FJsonObject& Json, const FGames& Games);

bool CompareCheckpoint(FCheck& Check, const FJsonObject& Json, const FArenaState& State, const TArray<TSharedPtr<FJsonValue>>& Events, const FGames* Games)
{
	if (!Check.WantInt(Json, TEXT("tick"), State.Tick, TEXT("state")) || !Check.WantUInt(Json, TEXT("rng"), State.Rng, TEXT("state"))
		|| !Check.WantInt(Json, TEXT("nextId"), State.NextId, TEXT("state")) || !Check.WantInt(Json, TEXT("eventCount"), State.Events.Num(), TEXT("state"))
		|| !Check.WantInt(Json, TEXT("randomDraws"), State.RandomLog.Num(), TEXT("state")))
	{
		return false;
	}
	double EventCount = 0.0;
	if (!Json.TryGetNumberField(TEXT("eventCount"), EventCount))
	{
		return Check.Fail(TEXT("eventCount"), TEXT("missing"));
	}
	if (!CompareEvents(Check, Events, State.Events, static_cast<int32>(std::llround(EventCount)), TEXT("events")))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
	if (!Json.TryGetArrayField(TEXT("actors"), Actors) || !Actors)
	{
		return Check.Fail(TEXT("actors"), TEXT("missing"));
	}
	if (Actors->Num() != State.Actors.Num())
	{
		return Check.Fail(TEXT("actors"), FString::Printf(TEXT("count expected %d got %d"), Actors->Num(), State.Actors.Num()));
	}
	for (int32 Index = 0; Index < Actors->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* ActorJson = nullptr;
		if (!(*Actors)[Index].IsValid() || !(*Actors)[Index]->TryGetObject(ActorJson) || !ActorJson || !ActorJson->IsValid())
		{
			return Check.Fail(TEXT("actors"), TEXT("entry is not an object"));
		}
		if (!CompareActor(Check, **ActorJson, State.Actors[Index], FString::Printf(TEXT("actors[%d]"), Index)))
		{
			return false;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Projectiles = nullptr;
	if (!Json.TryGetArrayField(TEXT("projectiles"), Projectiles) || !Projectiles)
	{
		return Check.Fail(TEXT("projectiles"), TEXT("missing"));
	}
	if (Projectiles->Num() != State.Projectiles.Num())
	{
		return Check.Fail(TEXT("projectiles"), FString::Printf(TEXT("count expected %d got %d"), Projectiles->Num(), State.Projectiles.Num()));
	}
	for (int32 Index = 0; Index < Projectiles->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* ProjectileJson = nullptr;
		if (!(*Projectiles)[Index].IsValid() || !(*Projectiles)[Index]->TryGetObject(ProjectileJson) || !ProjectileJson || !ProjectileJson->IsValid())
		{
			return Check.Fail(TEXT("projectiles"), TEXT("entry is not an object"));
		}
		const FString Path = FString::Printf(TEXT("projectiles[%d]"), Index);
		const FProjectile& Projectile = State.Projectiles[Index];
		const FJsonObject& Row = **ProjectileJson;
		if (!Check.WantInt(Row, TEXT("id"), Projectile.Id, Path) || !Check.WantInt(Row, TEXT("ownerId"), Projectile.OwnerId, Path)
			|| !Check.WantInt(Row, TEXT("activationId"), Projectile.ActivationId, Path)
			|| !Check.WantDouble(Row, TEXT("x"), Projectile.Pos.X, Path) || !Check.WantDouble(Row, TEXT("y"), Projectile.Pos.Y, Path)
			|| !Check.WantDouble(Row, TEXT("px"), Projectile.PreviousPos.X, Path) || !Check.WantDouble(Row, TEXT("py"), Projectile.PreviousPos.Y, Path)
			|| !Check.WantDouble(Row, TEXT("vx"), Projectile.Velocity.X, Path) || !Check.WantDouble(Row, TEXT("vy"), Projectile.Velocity.Y, Path)
			|| !Check.WantDouble(Row, TEXT("damage"), Projectile.Damage, Path) || !Check.WantDouble(Row, TEXT("remainingM"), Projectile.RemainingM, Path)
			|| !Check.WantDouble(Row, TEXT("radius"), Projectile.Radius, Path)
			|| !Check.WantString(Row, TEXT("family"), Projectile.Family, Path) || !Check.WantInt(Row, TEXT("tier"), Projectile.Tier, Path)
			|| !Check.WantBool(Row, TEXT("bolt"), Projectile.bBolt, Path) || !Check.WantBool(Row, TEXT("reflected"), Projectile.bReflected, Path)
			|| !Check.WantBool(Row, TEXT("piercing"), Projectile.bPiercing, Path) || !Check.WantDouble(Row, TEXT("burst"), Projectile.BurstRadiusM, Path))
		{
			return false;
		}
		const TSharedPtr<FJsonValue> Delivery = Row.TryGetField(TEXT("delivery"));
		if (!Delivery.IsValid())
		{
			return Check.Fail(Path + TEXT(".delivery"), TEXT("missing"));
		}
		if (Delivery->IsNull())
		{
			if (!Projectile.Delivery.IsEmpty())
			{
				return Check.Fail(Path + TEXT(".delivery"), TEXT("expected null"));
			}
		}
		else if (!Check.WantString(Row, TEXT("delivery"), Projectile.Delivery, Path))
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* HitIds = nullptr;
		if (!Row.TryGetArrayField(TEXT("hitIds"), HitIds) || !HitIds || HitIds->Num() != Projectile.HitIds.Num())
		{
			return Check.Fail(Path + TEXT(".hitIds"), TEXT("count mismatch"));
		}
		for (int32 HitIndex = 0; HitIndex < HitIds->Num(); ++HitIndex)
		{
			double Number = 0.0;
			if (!(*HitIds)[HitIndex].IsValid() || !(*HitIds)[HitIndex]->TryGetNumber(Number)
				|| static_cast<int32>(std::llround(Number)) != Projectile.HitIds[HitIndex])
			{
				return Check.Fail(Path + TEXT(".hitIds"), FString::Printf(TEXT("index %d"), HitIndex));
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Telegraphs = nullptr;
	if (!Json.TryGetArrayField(TEXT("telegraphs"), Telegraphs) || !Telegraphs)
	{
		return Check.Fail(TEXT("telegraphs"), TEXT("missing"));
	}
	if (Telegraphs->Num() != State.Telegraphs.Num())
	{
		return Check.Fail(TEXT("telegraphs"), FString::Printf(TEXT("count expected %d got %d"), Telegraphs->Num(), State.Telegraphs.Num()));
	}
	for (int32 Index = 0; Index < Telegraphs->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* TelegraphJson = nullptr;
		if (!(*Telegraphs)[Index].IsValid() || !(*Telegraphs)[Index]->TryGetObject(TelegraphJson) || !TelegraphJson || !TelegraphJson->IsValid())
		{
			return Check.Fail(TEXT("telegraphs"), TEXT("entry is not an object"));
		}
		const FString Path = FString::Printf(TEXT("telegraphs[%d]"), Index);
		const FTelegraph& Telegraph = State.Telegraphs[Index];
		const FJsonObject& Row = **TelegraphJson;
		if (!Check.WantInt(Row, TEXT("id"), Telegraph.Id, Path) || !Check.WantInt(Row, TEXT("ownerId"), Telegraph.OwnerId, Path)
			|| !Check.WantString(Row, TEXT("kind"), Telegraph.Kind, Path) || !Check.WantString(Row, TEXT("family"), Telegraph.Family, Path)
			|| !Check.WantInt(Row, TEXT("tier"), Telegraph.Tier, Path) || !Check.WantDouble(Row, TEXT("damage"), Telegraph.Damage, Path)
			|| !Check.WantInt(Row, TEXT("resolveTick"), Telegraph.ResolveTick, Path) || !Check.WantInt(Row, TEXT("startTick"), Telegraph.StartTick, Path)
			|| !Check.WantDouble(Row, TEXT("ox"), Telegraph.Origin.X, Path) || !Check.WantDouble(Row, TEXT("oy"), Telegraph.Origin.Y, Path)
			|| !Check.WantDouble(Row, TEXT("tx"), Telegraph.Target.X, Path) || !Check.WantDouble(Row, TEXT("ty"), Telegraph.Target.Y, Path)
			|| !Check.WantDouble(Row, TEXT("speedMps"), Telegraph.SpeedMps, Path) || !Check.WantDouble(Row, TEXT("rangeM"), Telegraph.RangeM, Path)
			|| !Check.WantDouble(Row, TEXT("widthM"), Telegraph.WidthM, Path) || !Check.WantBool(Row, TEXT("survives"), Telegraph.bSurvivesOwner, Path))
		{
			return false;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Zones = nullptr;
	if (!Json.TryGetArrayField(TEXT("zones"), Zones) || !Zones)
	{
		return Check.Fail(TEXT("zones"), TEXT("missing"));
	}
	if (Zones->Num() != State.Zones.Num())
	{
		return Check.Fail(TEXT("zones"), FString::Printf(TEXT("count expected %d got %d"), Zones->Num(), State.Zones.Num()));
	}
	for (int32 Index = 0; Index < Zones->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* ZoneJson = nullptr;
		if (!(*Zones)[Index].IsValid() || !(*Zones)[Index]->TryGetObject(ZoneJson) || !ZoneJson || !ZoneJson->IsValid())
		{
			return Check.Fail(TEXT("zones"), TEXT("entry is not an object"));
		}
		const FString Path = FString::Printf(TEXT("zones[%d]"), Index);
		const FZone& Zone = State.Zones[Index];
		const FJsonObject& Row = **ZoneJson;
		if (!Check.WantInt(Row, TEXT("id"), Zone.Id, Path) || !Check.WantInt(Row, TEXT("ownerId"), Zone.OwnerId, Path)
			|| !Check.WantString(Row, TEXT("kind"), Zone.Kind, Path)
			|| !Check.WantDouble(Row, TEXT("x"), Zone.Pos.X, Path) || !Check.WantDouble(Row, TEXT("y"), Zone.Pos.Y, Path)
			|| !Check.WantDouble(Row, TEXT("radiusM"), Zone.RadiusM, Path) || !Check.WantInt(Row, TEXT("until"), Zone.Until, Path)
			|| !Check.WantDouble(Row, TEXT("slowMult"), Zone.SlowMult, Path))
		{
			return false;
		}
	}
	const TSharedPtr<FJsonObject>* GamesJson = nullptr;
	if (Json.TryGetObjectField(TEXT("games"), GamesJson) && GamesJson && GamesJson->IsValid())
	{
		if (!Games)
		{
			return Check.Fail(TEXT("games"), TEXT("replay has no games state"));
		}
		if (!CompareGames(Check, **GamesJson, *Games))
		{
			return false;
		}
	}
	return !Check.bFailed;
}

bool CompareGames(FCheck& Check, const FJsonObject& Json, const FGames& Games)
{
	if (!Check.WantInt(Json, TEXT("wave"), Games.Wave, TEXT("games"))
		|| !Check.WantString(Json, TEXT("phase"), Games.Phase, TEXT("games"))
		|| !Check.WantInt(Json, TEXT("wavesCleared"), Games.WavesCleared, TEXT("games"))
		|| !Check.WantInt(Json, TEXT("playerId"), Games.PlayerId, TEXT("games")))
	{
		return false;
	}
	FString Kind;
	if (!Json.TryGetStringField(TEXT("resultKind"), Kind))
	{
		if (Games.Result.IsSet())
		{
			return Check.Fail(TEXT("games.result"), TEXT("expected none"));
		}
		return true;
	}
	if (!Games.Result.IsSet())
	{
		return Check.Fail(TEXT("games.result"), TEXT("missing"));
	}
	if (Games.Result->Kind != Kind)
	{
		return Check.Fail(TEXT("games.resultKind"), FString::Printf(TEXT("expected %s got %s"), *Kind, *Games.Result->Kind));
	}
	if (!Check.WantInt(Json, TEXT("gold"), Games.Result->Gold, TEXT("games"))
		|| !Check.WantInt(Json, TEXT("renown"), Games.Result->Renown, TEXT("games"))
		|| !Check.WantBool(Json, TEXT("finalReached"), Games.Result->bFinalReached, TEXT("games"))
		|| !Check.WantBool(Json, TEXT("finalWon"), Games.Result->bFinalWon, TEXT("games")))
	{
		return false;
	}
	return true;
}

bool ApplyOp(FCheck& Check, FArenaState& State, FGames* Games, const FJsonObject& Op)
{
	FString Name;
	if (!Op.TryGetStringField(TEXT("op"), Name))
	{
		return Check.Fail(TEXT("op"), TEXT("missing op"));
	}
	if (Name == TEXT("advanceGames"))
	{
		if (!Games || !TryAdvanceGames(*Games))
		{
			return Check.Fail(TEXT("op.advanceGames"), TEXT("not an intermission"));
		}
		return true;
	}
	if (Name == TEXT("clearWave"))
	{
		if (!Games)
		{
			return Check.Fail(TEXT("op.clearWave"), TEXT("no games state"));
		}
		for (FActor& Actor : Games->State.Actors)
		{
			if (Actor.Id == Games->PlayerId)
			{
				continue;
			}
			Actor.bDown = true;
			Actor.Hp = 0.0;
			if (Actor.Enemy.IsSet())
			{
				Actor.Enemy->bDeathQueued = true;
			}
		}
		Games->State.Projectiles.Reset();
		Games->State.Telegraphs.Reset();
		return true;
	}
	double ActorId = 0.0;
	if (!Op.TryGetNumberField(TEXT("actorId"), ActorId))
	{
		return Check.Fail(TEXT("op"), TEXT("missing op or actor"));
	}
	FActor* Actor = SimFindActor(State, static_cast<int32>(std::llround(ActorId)));
	if (!Actor)
	{
		return Check.Fail(TEXT("op"), FString::Printf(TEXT("missing actor %d"), static_cast<int32>(std::llround(ActorId))));
	}
	if (Name == TEXT("resetWave"))
	{
		ResetWave(State, *Actor);
		return true;
	}
	if (Name == TEXT("setHp"))
	{
		double Hp = 0.0;
		if (!Op.TryGetNumberField(TEXT("hp"), Hp))
		{
			return Check.Fail(TEXT("op.setHp"), TEXT("missing hp"));
		}
		Actor->Hp = Hp;
		return true;
	}
	if (Name == TEXT("setMana"))
	{
		double Mana = 0.0;
		if (!Op.TryGetNumberField(TEXT("mana"), Mana))
		{
			return Check.Fail(TEXT("op.setMana"), TEXT("missing mana"));
		}
		Actor->Mana = Mana;
		return true;
	}
	if (Name == TEXT("setClockAdvanceTicks"))
	{
		double Ticks = 0.0;
		if (!Op.TryGetNumberField(TEXT("ticks"), Ticks))
		{
			return Check.Fail(TEXT("op.setClockAdvanceTicks"), TEXT("missing ticks"));
		}
		Actor->ClockAdvanceTicks = static_cast<int32>(std::llround(Ticks));
		return true;
	}
	return Check.Fail(TEXT("op"), Name + TEXT(" is unknown"));
}

void Drive(const FString& Driver, FArenaState& State, FTraining* Training, FGames* Games, bool bAutoAdvance, const TMap<int32, FInputFrame>& Held)
{
	if (Driver == TEXT("training") && Training)
	{
		const FInputFrame* Found = Held.Find(Training->PlayerId);
		const FInputFrame Input = Found ? *Found : SimIdleInput(SimFindActor(Training->State, Training->DummyId)->Pos);
		StepTraining(*Training, Input);
		return;
	}
	if (Driver == TEXT("games") && Games)
	{
		const FInputFrame* Overlay = Held.Find(Games->PlayerId);
		StepGames(*Games, Overlay);
		if (bAutoAdvance && Games->Phase == TEXT("intermission"))
		{
			TryAdvanceGames(*Games);
		}
		return;
	}
	if (Driver == TEXT("mage"))
	{
		TMap<int32, FInputFrame> Merged;
		for (FActor& Actor : State.Actors)
		{
			if (Actor.MageAI.IsSet() && !Actor.bDown)
			{
				Merged.Add(Actor.Id, MageInput(State, Actor));
			}
		}
		for (const TPair<int32, FInputFrame>& Pair : Held)
		{
			Merged.FindOrAdd(Pair.Key) = Pair.Value;
		}
		StepArena(State, Merged);
		return;
	}
	if (Driver == TEXT("enemies"))
	{
		TMap<int32, FInputFrame> Merged = EnemyInputs(State);
		for (const TPair<int32, FInputFrame>& Pair : Held)
		{
			Merged.FindOrAdd(Pair.Key) = Pair.Value;
		}
		StepArena(State, Merged);
		QueueDeathEffects(State);
		return;
	}
	StepArena(State, Held);
}

bool Replay(FAutomationTestBase& Test, const FString& Name)
{
	if (!KernelData().bReady)
	{
		Test.AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
		return false;
	}
	FString Text;
	const FString Path = FPaths::Combine(ConformanceDir(), Name + TEXT(".json"));
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		Test.AddError(TEXT("Cannot read ") + Path);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Test.AddError(TEXT("Invalid conformance JSON ") + Path);
		return false;
	}
	FCheck Check{Test};
	const TSharedPtr<FJsonObject>* Tolerance = nullptr;
	if (!Root->TryGetObjectField(TEXT("tolerance"), Tolerance) || !Tolerance || !Tolerance->IsValid()
		|| !(*Tolerance)->TryGetNumberField(TEXT("relative"), Check.Rel) || !(*Tolerance)->TryGetNumberField(TEXT("absolute"), Check.Abs))
	{
		Test.AddError(TEXT("tolerance is missing"));
		return false;
	}
	if (!(Check.Rel <= 1.0e-6) || !(Check.Abs <= 1.0e-9))
	{
		Test.AddError(FString::Printf(TEXT("tolerance %g relative / %g absolute is looser than 1e-6 / 1e-9"), Check.Rel, Check.Abs));
		return false;
	}
	FString Driver;
	double SeedNumber = 0.0;
	double TickNumber = 0.0;
	bool bReplaySetup = false;
	if (!Root->TryGetStringField(TEXT("driver"), Driver) || !Root->TryGetNumberField(TEXT("seed"), SeedNumber)
		|| !Root->TryGetNumberField(TEXT("ticks"), TickNumber) || !Root->TryGetBoolField(TEXT("replaySetup"), bReplaySetup))
	{
		Test.AddError(TEXT("vector is missing driver, seed, ticks, or replaySetup"));
		return false;
	}
	const uint32 Seed = static_cast<uint32>(std::llround(SeedNumber));
	const int32 StopAt = static_cast<int32>(std::llround(TickNumber));
	FTraining Training;
	FGames Games;
	FArenaState Arena;
	const bool bTraining = Driver == TEXT("training");
	const bool bGames = Driver == TEXT("games");
	bool bAutoAdvance = false;
	if (bTraining)
	{
		FString Kind;
		if (!Root->TryGetStringField(TEXT("trainingKind"), Kind))
		{
			Test.AddError(TEXT("training vector is missing trainingKind"));
			return false;
		}
		Training = CreateTraining(Kind, Seed);
	}
	else if (bGames)
	{
		const TSharedPtr<FJsonObject>* GamesJson = nullptr;
		if (!Root->TryGetObjectField(TEXT("games"), GamesJson) || !GamesJson || !GamesJson->IsValid())
		{
			Test.AddError(TEXT("games vector is missing games"));
			return false;
		}
		double StartWave = 0.0;
		bool bReference = false;
		(*GamesJson)->TryGetNumberField(TEXT("startWave"), StartWave);
		(*GamesJson)->TryGetBoolField(TEXT("referencePlayer"), bReference);
		(*GamesJson)->TryGetBoolField(TEXT("autoAdvance"), bAutoAdvance);
		const FComposition* Preset = nullptr;
		FString PresetName;
		if ((*GamesJson)->TryGetStringField(TEXT("preset"), PresetName))
		{
			for (const FComposition& Candidate : KernelData().Presets)
			{
				if (Candidate.Name == PresetName)
				{
					Preset = &Candidate;
					break;
				}
			}
			if (!Preset)
			{
				Test.AddError(TEXT("unknown preset ") + PresetName);
				return false;
			}
		}
		if (!TryCreateGames(Games, Seed, Preset, static_cast<int32>(std::llround(StartWave)), bReference))
		{
			Test.AddError(TEXT("TryCreateGames failed"));
			return false;
		}
		FActor* Player = SimFindActor(Games.State, Games.PlayerId);
		double PlayerHp = 0.0;
		double PlayerMana = 0.0;
		if (Player && (*GamesJson)->TryGetNumberField(TEXT("playerHp"), PlayerHp))
		{
			Player->Hp = PlayerHp;
		}
		if (Player && (*GamesJson)->TryGetNumberField(TEXT("playerMana"), PlayerMana))
		{
			Player->Mana = PlayerMana;
		}
	}
	else
	{
		Arena = CreateArena(Seed);
		if (bReplaySetup)
		{
			const TSharedPtr<FJsonObject>* Setup = nullptr;
			if (!Root->TryGetObjectField(TEXT("setup"), Setup) || !Setup || !Setup->IsValid() || !ApplySetup(Check, Arena, **Setup))
			{
				if (!Check.bFailed)
				{
					Test.AddError(TEXT("setup failed"));
				}
				return false;
			}
		}
		if (Driver == TEXT("mage"))
		{
			const TArray<TSharedPtr<FJsonValue>>* Attachments = nullptr;
			if (!Root->TryGetArrayField(TEXT("mageAttachments"), Attachments) || !Attachments)
			{
				Test.AddError(TEXT("mage vector is missing mageAttachments"));
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Attachments)
			{
				const TSharedPtr<FJsonObject>* Attachment = nullptr;
				double ActorId = 0.0;
				double Competence = 0.0;
				if (!Value.IsValid() || !Value->TryGetObject(Attachment) || !Attachment || !Attachment->IsValid()
					|| !(*Attachment)->TryGetNumberField(TEXT("actorId"), ActorId)
					|| !(*Attachment)->TryGetNumberField(TEXT("competence"), Competence))
				{
					Test.AddError(TEXT("mage attachment is incomplete"));
					return false;
				}
				FActor* Actor = SimFindActor(Arena, static_cast<int32>(std::llround(ActorId)));
				if (!Actor)
				{
					Test.AddError(TEXT("mage attachment actor is missing"));
					return false;
				}
				AttachMageAI(*Actor, Competence, Arena.Tick);
			}
		}
	}
	auto StateOf = [&]() -> FArenaState&
	{
		if (bTraining)
		{
			return Training.State;
		}
		if (bGames)
		{
			return Games.State;
		}
		return Arena;
	};
	const TArray<TSharedPtr<FJsonValue>>* Frames = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Checkpoints = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	if (!Root->TryGetArrayField(TEXT("frames"), Frames) || !Frames || !Root->TryGetArrayField(TEXT("ops"), Ops) || !Ops
		|| !Root->TryGetArrayField(TEXT("checkpoints"), Checkpoints) || !Checkpoints
		|| !Root->TryGetArrayField(TEXT("events"), Events) || !Events)
	{
		Test.AddError(TEXT("vector is missing frames, ops, checkpoints, or events"));
		return false;
	}
	int32 Cursor = 0;
	auto Consume = [&](int32 Tick, const TCHAR* When) -> bool
	{
		Check.Tick = Tick;
		Check.When = When;
		if (Cursor >= Checkpoints->Num())
		{
			return true;
		}
		const TSharedPtr<FJsonObject>* Checkpoint = nullptr;
		if (!(*Checkpoints)[Cursor].IsValid() || !(*Checkpoints)[Cursor]->TryGetObject(Checkpoint) || !Checkpoint || !Checkpoint->IsValid())
		{
			return Check.Fail(TEXT("checkpoint"), TEXT("not an object"));
		}
		double CheckpointTick = 0.0;
		FString CheckpointWhen;
		if (!(*Checkpoint)->TryGetNumberField(TEXT("tick"), CheckpointTick) || !(*Checkpoint)->TryGetStringField(TEXT("when"), CheckpointWhen))
		{
			return Check.Fail(TEXT("checkpoint"), TEXT("missing tick or when"));
		}
		const int32 ExpectedTick = static_cast<int32>(std::llround(CheckpointTick));
		if (ExpectedTick > Tick)
		{
			return true;
		}
		if (ExpectedTick != Tick || CheckpointWhen != When)
		{
			return Check.Fail(TEXT("checkpoint"), FString::Printf(TEXT("next vector row is tick %d %s"), ExpectedTick, *CheckpointWhen));
		}
		if (!CompareCheckpoint(Check, **Checkpoint, StateOf(), *Events, bGames ? &Games : nullptr))
		{
			return false;
		}
		++Cursor;
		return true;
	};
	if (!Consume(0, TEXT("pre-ops")))
	{
		return false;
	}
	TMap<int32, FInputFrame> Held;
	for (int32 Tick = 1; Tick <= StopAt; ++Tick)
	{
		for (const TSharedPtr<FJsonValue>& FrameValue : *Frames)
		{
			const TSharedPtr<FJsonObject>* Frame = nullptr;
			double FrameTick = 0.0;
			if (!FrameValue.IsValid() || !FrameValue->TryGetObject(Frame) || !Frame || !Frame->IsValid()
				|| !(*Frame)->TryGetNumberField(TEXT("tick"), FrameTick))
			{
				Test.AddError(TEXT("bad frame"));
				return false;
			}
			if (static_cast<int32>(std::llround(FrameTick)) != Tick)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>* Inputs = nullptr;
			if (!(*Frame)->TryGetObjectField(TEXT("inputs"), Inputs) || !Inputs || !Inputs->IsValid())
			{
				Test.AddError(TEXT("frame inputs missing"));
				return false;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Inputs)->Values)
			{
				const TSharedPtr<FJsonObject>* InputJson = nullptr;
				if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(InputJson) || !InputJson || !InputJson->IsValid())
				{
					Test.AddError(TEXT("frame input is not an object"));
					return false;
				}
				FInputFrame Input;
				if (!ReadInput(Check, **InputJson, Input))
				{
					return false;
				}
				Held.FindOrAdd(FCString::Atoi(*Pair.Key)) = Input;
			}
		}
		Check.Tick = Tick;
		Check.When = TEXT("step");
		Drive(Driver, StateOf(), bTraining ? &Training : nullptr, bGames ? &Games : nullptr, bAutoAdvance, Held);
		if (!Consume(Tick, TEXT("pre-ops")))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& OpValue : *Ops)
		{
			const TSharedPtr<FJsonObject>* Op = nullptr;
			double After = 0.0;
			if (!OpValue.IsValid() || !OpValue->TryGetObject(Op) || !Op || !Op->IsValid() || !(*Op)->TryGetNumberField(TEXT("afterTick"), After))
			{
				Test.AddError(TEXT("bad op"));
				return false;
			}
			if (static_cast<int32>(std::llround(After)) == Tick && !ApplyOp(Check, StateOf(), bGames ? &Games : nullptr, **Op))
			{
				return false;
			}
		}
		bool bHadOp = false;
		for (const TSharedPtr<FJsonValue>& OpValue : *Ops)
		{
			const TSharedPtr<FJsonObject>* Op = nullptr;
			double After = 0.0;
			if (OpValue.IsValid() && OpValue->TryGetObject(Op) && Op && Op->IsValid() && (*Op)->TryGetNumberField(TEXT("afterTick"), After)
				&& static_cast<int32>(std::llround(After)) == Tick)
			{
				bHadOp = true;
			}
		}
		if (bHadOp && !Consume(Tick, TEXT("post-ops")))
		{
			return false;
		}
	}
	if (Cursor != Checkpoints->Num())
	{
		Test.AddError(FString::Printf(TEXT("unconsumed checkpoints: %d of %d"), Cursor, Checkpoints->Num()));
		return false;
	}
	Check.Tick = StopAt;
	Check.When = TEXT("events");
	if (StateOf().Events.Num() != Events->Num())
	{
		return Check.Fail(TEXT("events"), FString::Printf(TEXT("count expected %d got %d"), Events->Num(), StateOf().Events.Num()));
	}
	return CompareEvents(Check, *Events, StateOf().Events, Events->Num(), TEXT("events"));
}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMageArenaKernelConformance, "MageArena.Kernel.Conformance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

void FMageArenaKernelConformance::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(ConformanceDir(), TEXT("*.json")), true, false);
	Files.Sort();
	for (const FString& File : Files)
	{
		const FString Name = FPaths::GetBaseFilename(File);
		OutBeautifiedNames.Add(Name);
		OutTestCommands.Add(Name);
	}
	if (Files.Num() == 0)
	{
		OutBeautifiedNames.Add(TEXT("NoVectors"));
		OutTestCommands.Add(TEXT("NoVectors"));
	}
}

bool FMageArenaKernelConformance::RunTest(const FString& Parameters)
{
	if (Parameters == TEXT("NoVectors"))
	{
		AddError(TEXT("No conformance vectors in ") + ConformanceDir());
		return false;
	}
	return Replay(*this, Parameters);
}

#endif
