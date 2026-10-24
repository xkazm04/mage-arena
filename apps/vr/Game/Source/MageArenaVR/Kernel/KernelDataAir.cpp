#include "Kernel/KernelData.h"
#include "Kernel/KernelDataLoad.h"

#include "Internationalization/Regex.h"
#include "Misc/Paths.h"

#include <cmath>

using KernelDataLoad::CellNumber;
using KernelDataLoad::MatchGroup;
using KernelDataLoad::StrictNumber;

const FAirSpell* FindAirSpell(const FString& Id)
{
	for (const FAirSpell& Spell : KernelData().AirSpells)
	{
		if (Spell.Id == Id)
		{
			return &Spell;
		}
	}
	return nullptr;
}

FString KernelDataAirSpellsPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/schools/spells-air.csv")));
}

namespace
{
const TCHAR* AirHeader = TEXT("id,name,tier,shape,cast_s,cooldown_s,mana,damage,blockable,telegraph_s,range_m,momentum_gain,notes");

bool AirFail(FString& Error, const FString& Message)
{
	Error = Message;
	return false;
}

// Matches the whole cell against Pattern and returns up to three capture groups.
bool MatchCell(const FString& Cell, const TCHAR* Pattern, TArray<FString>& Groups)
{
	const FRegexPattern Compiled(Pattern);
	FRegexMatcher Matcher(Compiled, Cell);
	if (!Matcher.FindNext() || Matcher.GetMatchBeginning() != 0 || Matcher.GetMatchEnding() != Cell.Len())
	{
		return false;
	}
	Groups.Reset();
	for (int32 Group = 1; Group <= 3; ++Group)
	{
		Groups.Add(Matcher.GetCaptureGroup(Group));
	}
	return true;
}

bool CountWord(const FString& Word, int32& Out)
{
	static const TCHAR* Words[] = {TEXT("zero"), TEXT("one"), TEXT("two"), TEXT("three"), TEXT("four"), TEXT("five"), TEXT("six")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Words); ++Index)
	{
		if (Word == Words[Index])
		{
			Out = Index;
			return true;
		}
	}
	double Number = 0.0;
	if (StrictNumber(Word, Number) && Number == std::floor(Number))
	{
		Out = static_cast<int32>(Number);
		return true;
	}
	return false;
}

bool ParseAirShape(FAirSpell& Spell, FString& Error)
{
	const FString& Shape = Spell.Shape;
	TArray<FString> G;
	auto Reach = [&](double Value) -> bool
	{
		if (std::abs(Value - Spell.RangeM) <= 1.0e-9)
		{
			return true;
		}
		return AirFail(Error, FString::Printf(TEXT("air row %s: shape reach %g disagrees with range_m %g"), *Spell.Id, Value, Spell.RangeM));
	};
	if (MatchCell(Shape, TEXT("^projectile (\\d+(?:\\.\\d+)?) m/s$"), G))
	{
		Spell.Kind = TEXT("projectile");
		Spell.SpeedMps = FCString::Atod(*G[0]);
	}
	else if (MatchCell(Shape, TEXT("^two projectiles (\\d+(?:\\.\\d+)?) m/s at \\+-(\\d+(?:\\.\\d+)?) deg$"), G))
	{
		Spell.Kind = TEXT("twin");
		Spell.Count = 2;
		Spell.SpeedMps = FCString::Atod(*G[0]);
		Spell.SpreadDeg = 2.0 * FCString::Atod(*G[1]);
	}
	else if (MatchCell(Shape, TEXT("^dash (\\d+(?:\\.\\d+)?) m$"), G))
	{
		Spell.Kind = TEXT("dash");
		Spell.DashM = FCString::Atod(*G[0]);
		if (!Reach(Spell.DashM))
		{
			return false;
		}
	}
	else if (MatchCell(Shape, TEXT("^curving projectile (\\d+(?:\\.\\d+)?) m/s entering (\\d+(?:\\.\\d+)?) deg off the line of sight$"), G))
	{
		Spell.Kind = TEXT("veer");
		Spell.SpeedMps = FCString::Atod(*G[0]);
		Spell.EntryDeg = FCString::Atod(*G[1]);
		if (!(Spell.EntryDeg > 0.0 && Spell.EntryDeg < 90.0))
		{
			return AirFail(Error, FString::Printf(TEXT("air row %s: entry angle must be in (0, 90) degrees"), *Spell.Id));
		}
	}
	else if (MatchCell(Shape, TEXT("^ground circle r (\\d+(?:\\.\\d+)?) m at a point$"), G))
	{
		Spell.Kind = TEXT("zone");
		Spell.RadiusM = FCString::Atod(*G[0]);
	}
	else if (MatchCell(Shape, TEXT("^self (\\d+(?:\\.\\d+)?) s$"), G))
	{
		Spell.Kind = TEXT("form");
		Spell.DurationS = FCString::Atod(*G[0]);
	}
	else if (MatchCell(Shape, TEXT("^([a-z]+|\\d+) projectiles (\\d+(?:\\.\\d+)?) m/s released (\\d+(?:\\.\\d+)?) s apart$"), G))
	{
		Spell.Kind = TEXT("squall");
		if (!CountWord(G[0], Spell.Count) || Spell.Count < 2)
		{
			return AirFail(Error, FString::Printf(TEXT("air row %s: volley count '%s' is not a number of two or more"), *Spell.Id, *G[0]));
		}
		Spell.SpeedMps = FCString::Atod(*G[1]);
		Spell.IntervalS = FCString::Atod(*G[2]);
	}
	else if (Shape == TEXT("self"))
	{
		Spell.Kind = TEXT("self");
	}
	else if (MatchCell(Shape, TEXT("^line (\\d+(?:\\.\\d+)?) m(?: lightning)?$"), G))
	{
		Spell.Kind = TEXT("line");
		if (!Reach(FCString::Atod(*G[0])))
		{
			return false;
		}
	}
	else if (MatchCell(Shape, TEXT("^channelled beam (\\d+(?:\\.\\d+)?) m for (\\d+(?:\\.\\d+)?) s$"), G))
	{
		Spell.Kind = TEXT("beam");
		Spell.DurationS = FCString::Atod(*G[1]);
		if (!Reach(FCString::Atod(*G[0])))
		{
			return false;
		}
	}
	else
	{
		return AirFail(Error, FString::Printf(TEXT("air row %s: shape not understood: %s"), *Spell.Id, *Shape));
	}
	return true;
}

bool ParseAirDamage(FAirSpell& Spell, FString& Error)
{
	TArray<FString> G;
	if (MatchCell(Spell.DamageText, TEXT("^(\\d+(?:\\.\\d+)?)(?: each)?$"), G))
	{
		Spell.Damage = FCString::Atod(*G[0]);
	}
	else if (MatchCell(Spell.DamageText, TEXT("^(\\d+(?:\\.\\d+)?) per (\\d+(?:\\.\\d+)?) s tick$"), G))
	{
		Spell.Damage = FCString::Atod(*G[0]);
		Spell.TickS = FCString::Atod(*G[1]);
	}
	else
	{
		return AirFail(Error, FString::Printf(TEXT("air row %s: damage cell not understood: %s"), *Spell.Id, *Spell.DamageText));
	}
	if (Spell.Kind == TEXT("beam") && !(Spell.TickS > 0.0))
	{
		return AirFail(Error, FString::Printf(TEXT("air row %s: a beam needs a tick interval"), *Spell.Id));
	}
	if (Spell.Kind != TEXT("beam") && Spell.TickS > 0.0)
	{
		return AirFail(Error, FString::Printf(TEXT("air row %s: only a beam ticks"), *Spell.Id));
	}
	return true;
}

bool ParseAirMomentum(FAirSpell& Spell, FString& Error)
{
	TArray<FString> G;
	const FString& Cell = Spell.MomentumGain;
	if (MatchCell(Cell, TEXT("^(\\d+(?:\\.\\d+)?) per hit$"), G))
	{
		Spell.MomentumMode = EAirMomentumMode::Hit;
		Spell.MomentumAmount = FCString::Atod(*G[0]);
	}
	else if (MatchCell(Cell, TEXT("^(\\d+(?:\\.\\d+)?) per target$"), G))
	{
		Spell.MomentumMode = EAirMomentumMode::Target;
		Spell.MomentumAmount = FCString::Atod(*G[0]);
	}
	else if (MatchCell(Cell, TEXT("^(\\d+(?:\\.\\d+)?) per tick$"), G))
	{
		Spell.MomentumMode = EAirMomentumMode::Tick;
		Spell.MomentumAmount = FCString::Atod(*G[0]);
	}
	else if (MatchCell(Cell, TEXT("^\\+(\\d+(?:\\.\\d+)?) instant$"), G))
	{
		Spell.MomentumMode = EAirMomentumMode::Instant;
		Spell.MomentumAmount = FCString::Atod(*G[0]);
	}
	else if (MatchCell(Cell, TEXT("^spends (\\d+(?:\\.\\d+)?)$"), G))
	{
		Spell.MomentumMode = EAirMomentumMode::Spend;
		Spell.MomentumAmount = FCString::Atod(*G[0]);
	}
	else if (MatchCell(Cell, TEXT("^Momentum locked at (\\d+(?:\\.\\d+)?) for (\\d+(?:\\.\\d+)?) s$"), G))
	{
		Spell.MomentumMode = EAirMomentumMode::Lock;
		Spell.MomentumLockValue = FCString::Atod(*G[0]);
		Spell.MomentumLockS = FCString::Atod(*G[1]);
	}
	else
	{
		return AirFail(Error, FString::Printf(TEXT("air row %s: momentum_gain not understood: %s"), *Spell.Id, *Cell));
	}
	// Each mode belongs to the shapes that can pay it.
	const bool bTick = Spell.MomentumMode == EAirMomentumMode::Tick;
	const bool bSpend = Spell.MomentumMode == EAirMomentumMode::Spend;
	if (bTick != (Spell.Kind == TEXT("beam")) || bSpend != (Spell.Kind == TEXT("form"))
		|| (Spell.MomentumMode == EAirMomentumMode::Lock && Spell.Kind != TEXT("self")))
	{
		return AirFail(Error, FString::Printf(TEXT("air row %s: momentum_gain '%s' does not fit shape '%s'"), *Spell.Id, *Cell, *Spell.Shape));
	}
	return true;
}

bool ParseAirNotes(FAirSpell& Spell, FString& Error)
{
	const FString& Notes = Spell.Notes;
	FString Text;
	Spell.bRootDuringCast = Notes.Contains(TEXT("rooted"), ESearchCase::IgnoreCase);
	if (MatchGroup(Notes, TEXT("Absorb drain x(\\d+(?:\\.\\d+)?)"), Text))
	{
		Spell.AbsorbDrainMult = FCString::Atod(*Text);
	}
	if (Spell.Kind == TEXT("form"))
	{
		FString Physical;
		FString Magic;
		FString Need;
		if (!MatchGroup(Notes, TEXT("evade (\\d+(?:\\.\\d+)?) % of physical"), Physical)
			|| !MatchGroup(Notes, TEXT("(\\d+(?:\\.\\d+)?) % of magic"), Magic)
			|| !MatchGroup(Notes, TEXT("Needs Momentum (\\d+(?:\\.\\d+)?) or more"), Need))
		{
			return AirFail(Error, FString::Printf(TEXT("air row %s: the form notes need the evade chances and the Momentum it needs"), *Spell.Id));
		}
		Spell.EvadePhysical = FCString::Atod(*Physical) / 100.0;
		Spell.EvadeMagic = FCString::Atod(*Magic) / 100.0;
		Spell.NeedMomentum = FCString::Atod(*Need);
		if (Spell.EvadePhysical > 1.0 || Spell.EvadeMagic > 1.0)
		{
			return AirFail(Error, FString::Printf(TEXT("air row %s: an evade chance is above 100 %%"), *Spell.Id));
		}
		if (!Notes.Contains(TEXT("unblockables are not evaded")))
		{
			return AirFail(Error, FString::Printf(TEXT("air row %s: the form notes no longer say unblockables are not evaded"), *Spell.Id));
		}
	}
	return true;
}

void PublishAirCatalog(const FAirSpell& Spell, TArray<FSpell>& Catalog)
{
	FSpell View;
	View.Id = Spell.Id;
	View.Line = Spell.Id;
	View.Tier = Spell.Tier;
	View.Name = Spell.Name;
	View.CastS = Spell.CastS;
	View.CooldownS = Spell.CooldownS;
	View.Mana = Spell.Mana;
	View.Damage = Spell.Damage;
	View.Family = Spell.Family;
	View.TelegraphS = Spell.TelegraphS;
	View.RangeM = Spell.RangeM;
	if (Spell.Kind == TEXT("twin") || Spell.Kind == TEXT("veer") || Spell.Kind == TEXT("squall"))
	{
		View.Kind = TEXT("projectile");
	}
	else if (Spell.Kind == TEXT("form"))
	{
		View.Kind = TEXT("self");
	}
	else
	{
		View.Kind = Spell.Kind;
	}
	View.SpeedMps = Spell.SpeedMps;
	View.RadiusM = Spell.RadiusM;
	View.Count = Spell.Count;
	View.SpreadDeg = Spell.SpreadDeg;
	View.DurationS = Spell.DurationS;
	View.Effect = Spell.Kind;
	Catalog.Add(MoveTemp(View));
}
}

bool ParseAirSpellsCsv(const FString& Text, TArray<FAirSpell>& OutSpells, TArray<FSpell>& OutCatalog, FString& Error)
{
	FString Body = Text;
	if (Body.Len() > 0 && Body[0] == 0xFEFF)
	{
		Body.RemoveAt(0, 1, EAllowShrinking::No);
	}
	TArray<FString> Lines;
	Body.ParseIntoArrayLines(Lines, false);
	if (Lines.Num() < 2)
	{
		return AirFail(Error, TEXT("air spells: no rows"));
	}
	if (Lines[0].TrimEnd() != AirHeader)
	{
		return AirFail(Error, FString::Printf(TEXT("air spells: header is not '%s'"), AirHeader));
	}
	TArray<FAirSpell> Spells;
	TArray<FSpell> Catalog;
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		const FString Line = Lines[Index].TrimEnd();
		if (Line.TrimStart().IsEmpty())
		{
			continue;
		}
		// The notes column is unquoted and may hold commas (the Fire reading): split twelve fields and keep the tail.
		TArray<FString> Values;
		Line.ParseIntoArray(Values, TEXT(","), false);
		if (Values.Num() < 13)
		{
			return AirFail(Error, FString::Printf(TEXT("air spells: short row: %s"), *Line));
		}
		FString Notes = Values[12];
		for (int32 Extra = 13; Extra < Values.Num(); ++Extra)
		{
			Notes += TEXT(",");
			Notes += Values[Extra];
		}
		FAirSpell Spell;
		Spell.Id = Values[0];
		Spell.Name = Values[1];
		TArray<FString> IdGroups;
		if (!MatchCell(Spell.Id, TEXT("^air_[a-z]+$"), IdGroups) || Spell.Name.IsEmpty())
		{
			return AirFail(Error, FString::Printf(TEXT("air spells: bad id or name in row: %s"), *Line));
		}
		double Tier = 0.0;
		if (!CellNumber(Values[2], TEXT("air"), TEXT("tier"), Spell.Id, Tier, Error)
			|| !CellNumber(Values[4], TEXT("air"), TEXT("cast_s"), Spell.Id, Spell.CastS, Error)
			|| !CellNumber(Values[5], TEXT("air"), TEXT("cooldown_s"), Spell.Id, Spell.CooldownS, Error)
			|| !CellNumber(Values[6], TEXT("air"), TEXT("mana"), Spell.Id, Spell.Mana, Error)
			|| !CellNumber(Values[9], TEXT("air"), TEXT("telegraph_s"), Spell.Id, Spell.TelegraphS, Error)
			|| !CellNumber(Values[10], TEXT("air"), TEXT("range_m"), Spell.Id, Spell.RangeM, Error))
		{
			return false;
		}
		if (Tier != std::floor(Tier) || Tier > static_cast<double>(TierCap))
		{
			return AirFail(Error, FString::Printf(TEXT("air row %s: tier %s is not 0-%d"), *Spell.Id, *Values[2], TierCap));
		}
		Spell.Tier = static_cast<int32>(Tier);
		Spell.Shape = Values[3];
		Spell.DamageText = Values[7];
		Spell.Blockable = Values[8];
		Spell.MomentumGain = Values[11];
		Spell.Notes = Notes;
		if (Spell.Blockable == TEXT("UNBLOCKABLE"))
		{
			Spell.Family = TEXT("unblockable");
		}
		else if (Spell.Blockable == TEXT("absorbable"))
		{
			Spell.Family = TEXT("magic");
		}
		else if (Spell.Blockable == TEXT("n/a"))
		{
			Spell.Family = TEXT("n/a");
		}
		else
		{
			return AirFail(Error, FString::Printf(TEXT("air row %s: blockable cell not understood: %s"), *Spell.Id, *Spell.Blockable));
		}
		if (!ParseAirShape(Spell, Error) || !ParseAirDamage(Spell, Error) || !ParseAirMomentum(Spell, Error) || !ParseAirNotes(Spell, Error))
		{
			return false;
		}
		const bool bSelfShape = Spell.Kind == TEXT("dash") || Spell.Kind == TEXT("form") || Spell.Kind == TEXT("self");
		if (bSelfShape != (Spell.Family == TEXT("n/a")))
		{
			return AirFail(Error, FString::Printf(TEXT("air row %s: n/a belongs to the self and dash rows only"), *Spell.Id));
		}
		if (Spells.ContainsByPredicate([&](const FAirSpell& Other) { return Other.Id == Spell.Id; }))
		{
			return AirFail(Error, FString::Printf(TEXT("air spells: duplicate id %s"), *Spell.Id));
		}
		Spell.bBolt = Spell.Id == TEXT("air_bolt");
		PublishAirCatalog(Spell, Catalog);
		Spells.Add(MoveTemp(Spell));
	}
	if (Spells.Num() == 0)
	{
		return AirFail(Error, TEXT("air spells: no rows"));
	}
	OutSpells.Append(MoveTemp(Spells));
	OutCatalog.Append(MoveTemp(Catalog));
	return true;
}
