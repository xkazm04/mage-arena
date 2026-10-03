#include "Kernel/VrRules.h"

#include "Kernel/KernelData.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

namespace
{
FString VrFile(const TCHAR* Name)
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr"), Name));
}

bool Fail(FString& Error, const FString& Message)
{
	Error = Message;
	return false;
}

bool LoadJsonFile(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& Error)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: cannot read %s"), *Path));
	}
	if (Text.Len() > 0 && Text[0] == 0xFEFF)
	{
		Text.RemoveAt(0, 1, EAllowShrinking::No);
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: %s is not JSON"), *Path));
	}
	return true;
}

bool NeedNumber(const FJsonObject& Object, const TCHAR* Field, double& Out, FString& Error)
{
	if (!Object.TryGetNumberField(Field, Out))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: missing number %s"), Field));
	}
	return true;
}

bool NeedBool(const FJsonObject& Object, const TCHAR* Field, bool& Out, FString& Error)
{
	if (!Object.TryGetBoolField(Field, Out))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: missing bool %s"), Field));
	}
	return true;
}

bool NeedString(const FJsonObject& Object, const TCHAR* Field, FString& Out, FString& Error)
{
	if (!Object.TryGetStringField(Field, Out))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: missing string %s"), Field));
	}
	return true;
}

bool NeedObject(const FJsonObject& Object, const TCHAR* Field, const FJsonObject*& Out, FString& Error)
{
	const TSharedPtr<FJsonObject>* Found = nullptr;
	if (!Object.TryGetObjectField(Field, Found) || !Found || !Found->IsValid())
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: missing object %s"), Field));
	}
	Out = Found->Get();
	return true;
}

bool NeedArray2(const FJsonObject& Object, const TCHAR* Field, double& X, double& Y, FString& Error)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.TryGetArrayField(Field, Values) || !Values || Values->Num() < 2
		|| !(*Values)[0].IsValid() || !(*Values)[1].IsValid()
		|| (*Values)[0]->Type != EJson::Number || (*Values)[1]->Type != EJson::Number)
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: %s needs two numbers"), Field));
	}
	X = (*Values)[0]->AsNumber();
	Y = (*Values)[1]->AsNumber();
	return true;
}

bool Near(double Left, double Right)
{
	return std::abs(Left - Right) <= 1.0e-6;
}

const FEnemySpec* FindEnemy(const FString& Id)
{
	for (const FEnemySpec& Spec : KernelData().Enemies)
	{
		if (Spec.Id == Id)
		{
			return &Spec;
		}
	}
	return nullptr;
}

const FAttackSpec* FindAttack(const FEnemySpec& Spec, const FString& Id)
{
	for (const FAttackSpec& Attack : Spec.Attacks)
	{
		if (Attack.Id == Id)
		{
			return &Attack;
		}
	}
	return nullptr;
}

bool ReadLayoutMeasure(const FJsonObject& Layout, double& CentreX, double& CentreY, double& SizeX, double& SizeY,
	double& HeightM, double& PadX, double& PadY, double& ChestM, double& SteelLengthM, double& SteelRadiusM, FString& Error)
{
	const FJsonObject* Dais = nullptr;
	if (!NeedObject(Layout, TEXT("dais"), Dais, Error)
		|| !NeedArray2(*Dais, TEXT("centreM"), CentreX, CentreY, Error)
		|| !NeedArray2(*Dais, TEXT("sizeM"), SizeX, SizeY, Error)
		|| !NeedNumber(*Dais, TEXT("heightM"), HeightM, Error))
	{
		return false;
	}
	const FJsonObject* Pads = nullptr;
	if (!NeedObject(Layout, TEXT("pads"), Pads, Error))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Pads->TryGetArrayField(TEXT("items"), Items) || !Items)
	{
		return Fail(Error, TEXT("vr rules: layout pads.items is missing"));
	}
	bool bFoundPad = false;
	for (const TSharedPtr<FJsonValue>& Value : *Items)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Item.IsValid())
		{
			continue;
		}
		FString Id;
		if (!Item->TryGetStringField(TEXT("id"), Id) || Id != TEXT("centre"))
		{
			continue;
		}
		if (!NeedArray2(*Item, TEXT("positionM"), PadX, PadY, Error))
		{
			return false;
		}
		bFoundPad = true;
		break;
	}
	if (!bFoundPad)
	{
		return Fail(Error, TEXT("vr rules: layout has no centre pad"));
	}
	const FJsonObject* Threats = nullptr;
	if (!NeedObject(Layout, TEXT("threats"), Threats, Error) || !NeedNumber(*Threats, TEXT("chestHeightAboveSandM"), ChestM, Error))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Order = nullptr;
	if (!Threats->TryGetArrayField(TEXT("order"), Order) || !Order)
	{
		return Fail(Error, TEXT("vr rules: layout threats.order is missing"));
	}
	bool bSteel = false;
	for (const TSharedPtr<FJsonValue>& Value : *Order)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Item.IsValid())
		{
			continue;
		}
		FString Kind;
		if (!Item->TryGetStringField(TEXT("kind"), Kind) || Kind != TEXT("steel"))
		{
			continue;
		}
		if (!NeedNumber(*Item, TEXT("lengthM"), SteelLengthM, Error) || !NeedNumber(*Item, TEXT("radiusM"), SteelRadiusM, Error))
		{
			return false;
		}
		bSteel = true;
		break;
	}
	if (!bSteel)
	{
		return Fail(Error, TEXT("vr rules: layout has no steel threat"));
	}
	return true;
}

bool ReadThrow(const FJsonObject& Attack, const FEnemySpec& Conscript, const FAttackSpec& Spear, const FAttackSpec& Stone,
	double HeightM, double ChestM, double SteelLengthM, double SteelRadiusM, FVrAttackMode& Mode, FString& Error)
{
	FString ModeName;
	FString AttackId;
	if (!NeedString(Attack, TEXT("mode"), ModeName, Error) || !NeedString(Attack, TEXT("attackId"), AttackId, Error))
	{
		return false;
	}
	if (ModeName != TEXT("throw") || AttackId != Spear.Id)
	{
		return Fail(Error, TEXT("vr rules: conscript mode must be throw of spear_lunge"));
	}
	if (Attack.HasField(TEXT("recoveryS")) || Attack.HasField(TEXT("cooldownS")))
	{
		return Fail(Error, TEXT("vr rules: conscript recovery and cooldown stay on the pinned row"));
	}
	double Damage = 0.0;
	double Windup = 0.0;
	if (!NeedNumber(Attack, TEXT("damage"), Damage, Error) || !NeedNumber(Attack, TEXT("windupS"), Windup, Error)
		|| !NeedNumber(Attack, TEXT("projectileMps"), Mode.ProjectileMps, Error)
		|| !NeedNumber(Attack, TEXT("rangeM"), Mode.RangeM, Error)
		|| !NeedNumber(Attack, TEXT("arcApexM"), Mode.ArcApexM, Error)
		|| !NeedNumber(Attack, TEXT("spearLengthM"), Mode.SpearLengthM, Error)
		|| !NeedNumber(Attack, TEXT("spearRadiusM"), Mode.SpearRadiusM, Error))
	{
		return false;
	}
	if (!Near(Damage, Spear.Damage) || !Near(Windup, Spear.WindupS))
	{
		return Fail(Error, FString::Printf(TEXT("vr rules: conscript damage/windup must stay pinned (%.4f / %.4f)"), Spear.Damage, Spear.WindupS));
	}
	if (!Spear.RangeM.IsSet() || !Near(Spear.RangeM.GetValue(), 2.0))
	{
		return Fail(Error, TEXT("vr rules: pinned spear_lunge rangeM is not 2"));
	}
	if (Stone.ProjectileMps.IsSet() == false || !Near(Mode.ProjectileMps, Stone.ProjectileMps.GetValue()))
	{
		return Fail(Error, TEXT("vr rules: thrown spear speed must match the pinned sling stone"));
	}
	if (!Near(Mode.ArcApexM, ChestM - HeightM))
	{
		return Fail(Error, TEXT("vr rules: arcApexM must be the layout chest height minus the dais height"));
	}
	if (!Near(Mode.SpearLengthM, SteelLengthM) || !Near(Mode.SpearRadiusM, SteelRadiusM))
	{
		return Fail(Error, TEXT("vr rules: spear mesh must match the layout steel threat"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Replaces = nullptr;
	if (!Attack.TryGetArrayField(TEXT("replaces"), Replaces) || !Replaces)
	{
		return Fail(Error, TEXT("vr rules: conscript throw must list replaces"));
	}
	bool bRange = false;
	bool bSpeed = false;
	for (const TSharedPtr<FJsonValue>& Value : *Replaces)
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Row.IsValid())
		{
			return Fail(Error, TEXT("vr rules: replaces row is not an object"));
		}
		FString Field;
		double Overlay = 0.0;
		if (!NeedString(*Row, TEXT("field"), Field, Error) || !NeedNumber(*Row, TEXT("overlay"), Overlay, Error))
		{
			return false;
		}
		if (Field == TEXT("rangeM"))
		{
			double Pinned = 0.0;
			if (!NeedNumber(*Row, TEXT("pinned"), Pinned, Error) || !Near(Pinned, Spear.RangeM.GetValue()) || !Near(Overlay, Mode.RangeM))
			{
				return Fail(Error, TEXT("vr rules: rangeM replace does not match the pinned lunge and the overlay range"));
			}
			bRange = true;
		}
		else if (Field == TEXT("projectileMps"))
		{
			const TSharedPtr<FJsonValue> PinnedField = Row->TryGetField(TEXT("pinned"));
			if (!PinnedField.IsValid() || PinnedField->Type != EJson::Null || !Near(Overlay, Mode.ProjectileMps))
			{
				return Fail(Error, TEXT("vr rules: projectileMps replace must name a null pinned speed and the overlay speed"));
			}
			bSpeed = true;
		}
		else
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: unknown replace %s"), *Field));
		}
	}
	if (!bRange || !bSpeed)
	{
		return Fail(Error, TEXT("vr rules: conscript replaces must name rangeM and projectileMps"));
	}
	Mode.bThrow = true;
	Mode.EnemyId = Conscript.Id;
	return true;
}
}

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
	const FVrAabb Box = HoldBox();
	if (!Box.Contains(Goal))
	{
		return Goal;
	}
	if (Box.Contains(From))
	{
		return PushOutsideHold(From);
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
		return PushOutsideHold(From);
	}
	const double T = SimClamp(Enter, 0.0, 1.0);
	return SimAdd(From, SimScale(Delta, T));
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

bool LoadVrRuleset(FVrRuleset& Out, FString& Error)
{
	Out = FVrRuleset();
	if (!KernelData().bReady)
	{
		return Fail(Error, TEXT("vr rules: kernel data is not ready"));
	}
	TSharedPtr<FJsonObject> Overlay;
	TSharedPtr<FJsonObject> Layout;
	if (!LoadJsonFile(VrFile(TEXT("combat.vr.json")), Overlay, Error) || !LoadJsonFile(VrFile(TEXT("arena-layout.json")), Layout, Error))
	{
		return false;
	}
	double Version = 0.0;
	if (!NeedNumber(*Overlay, TEXT("version"), Version, Error) || !Near(Version, 1.0))
	{
		return Fail(Error, TEXT("vr rules: combat.vr.json version must be 1"));
	}
	const FJsonObject* Dais = nullptr;
	const FJsonObject* Movement = nullptr;
	if (!NeedObject(*Overlay, TEXT("daisNoEntry"), Dais, Error) || !NeedObject(*Overlay, TEXT("movement"), Movement, Error))
	{
		return false;
	}
	double CentreX = 0.0;
	double CentreY = 0.0;
	double HalfX = 0.0;
	double HalfY = 0.0;
	double Standoff = 0.0;
	if (!NeedArray2(*Dais, TEXT("centreM"), CentreX, CentreY, Error)
		|| !NeedArray2(*Dais, TEXT("halfExtentM"), HalfX, HalfY, Error)
		|| !NeedNumber(*Dais, TEXT("standoffM"), Standoff, Error))
	{
		return false;
	}
	double LayoutCentreX = 0.0;
	double LayoutCentreY = 0.0;
	double SizeX = 0.0;
	double SizeY = 0.0;
	double HeightM = 0.0;
	double PadX = 0.0;
	double PadY = 0.0;
	double ChestM = 0.0;
	double SteelLengthM = 0.0;
	double SteelRadiusM = 0.0;
	if (!ReadLayoutMeasure(*Layout, LayoutCentreX, LayoutCentreY, SizeX, SizeY, HeightM, PadX, PadY, ChestM, SteelLengthM, SteelRadiusM, Error))
	{
		return false;
	}
	if (!Near(CentreX, LayoutCentreX) || !Near(CentreY, LayoutCentreY)
		|| !Near(HalfX * 2.0, SizeX) || !Near(HalfY * 2.0, SizeY)
		|| !Near(Standoff, HeightM))
	{
		return Fail(Error, TEXT("vr rules: dais box or standoff does not match arena-layout.json"));
	}
	bool bCloses = false;
	if (!NeedBool(*Movement, TEXT("throwerClosesToLip"), bCloses, Error) || !bCloses)
	{
		return Fail(Error, TEXT("vr rules: throwerClosesToLip must be true"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Attacks = nullptr;
	if (!Overlay->TryGetArrayField(TEXT("attacks"), Attacks) || !Attacks)
	{
		return Fail(Error, TEXT("vr rules: attacks array is missing"));
	}
	const FEnemySpec* Conscript = FindEnemy(TEXT("conscript"));
	const FEnemySpec* Slinger = FindEnemy(TEXT("slinger"));
	if (!Conscript || !Slinger)
	{
		return Fail(Error, TEXT("vr rules: pinned roster is missing conscript or slinger"));
	}
	const FAttackSpec* Spear = FindAttack(*Conscript, TEXT("spear_lunge"));
	const FAttackSpec* Stone = FindAttack(*Slinger, TEXT("sling_stone"));
	if (!Spear || !Stone)
	{
		return Fail(Error, TEXT("vr rules: pinned spear_lunge or sling_stone is missing"));
	}
	bool bConscript = false;
	bool bSlinger = false;
	FVrAttackMode Throw;
	for (const TSharedPtr<FJsonValue>& Value : *Attacks)
	{
		const TSharedPtr<FJsonObject> Attack = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Attack.IsValid())
		{
			return Fail(Error, TEXT("vr rules: attack entry is not an object"));
		}
		FString EnemyId;
		FString ModeName;
		if (!NeedString(*Attack, TEXT("enemyId"), EnemyId, Error) || !NeedString(*Attack, TEXT("mode"), ModeName, Error))
		{
			return false;
		}
		if (EnemyId == TEXT("conscript"))
		{
			if (bConscript || !ReadThrow(*Attack, *Conscript, *Spear, *Stone, HeightM, ChestM, SteelLengthM, SteelRadiusM, Throw, Error))
			{
				return bConscript ? Fail(Error, TEXT("vr rules: conscript listed twice")) : false;
			}
			bConscript = true;
		}
		else if (EnemyId == TEXT("slinger"))
		{
			if (bSlinger)
			{
				return Fail(Error, TEXT("vr rules: slinger listed twice"));
			}
			FString AttackId;
			if (!NeedString(*Attack, TEXT("attackId"), AttackId, Error) || ModeName != TEXT("unchanged") || AttackId != Stone->Id)
			{
				return Fail(Error, TEXT("vr rules: slinger mode must be unchanged sling_stone"));
			}
			if (Attack->HasField(TEXT("projectileMps")) || Attack->HasField(TEXT("damage")) || Attack->HasField(TEXT("rangeM"))
				|| Attack->HasField(TEXT("windupS")) || Attack->HasField(TEXT("cooldownS")) || Attack->HasField(TEXT("replaces")))
			{
				return Fail(Error, TEXT("vr rules: slinger entry must not override a pinned field"));
			}
			bSlinger = true;
		}
		else if (ModeName == TEXT("throw"))
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s throw is not a wave-1 mode this overlay knows"), *EnemyId));
		}
		else if (ModeName != TEXT("unchanged"))
		{
			return Fail(Error, FString::Printf(TEXT("vr rules: %s has an unknown mode"), *EnemyId));
		}
	}
	if (!bConscript || !bSlinger)
	{
		return Fail(Error, TEXT("vr rules: attacks must name conscript and slinger"));
	}
	const FSimVec Spawn = KernelData().PlayerSpawn;
	const FSimVec Centre{
		Spawn.X + (CentreX - PadX),
		Spawn.Y + (CentreY - PadY)};
	Out.bActive = true;
	Out.StandoffM = Standoff;
	Out.Dais.MinX = Centre.X - HalfX;
	Out.Dais.MaxX = Centre.X + HalfX;
	Out.Dais.MinY = Centre.Y - HalfY;
	Out.Dais.MaxY = Centre.Y + HalfY;
	Out.Throws.Add(Throw);
	return true;
}
