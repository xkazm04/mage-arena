#include "Kernel/KernelData.h"
#include "Kernel/KernelDataLoad.h"

#include <cmath>

namespace KernelDataLoad
{
bool RequireSpellColumns(const FCsvTable& Rows, FKernelData& Data)
{
	// Every column below is dereferenced without a null check, so a renamed column must stop here, named.
	static const TCHAR* const Required[] = {
		TEXT("line"), TEXT("tier"), TEXT("branch"), TEXT("name"), TEXT("shape"), TEXT("cast_s"), TEXT("cooldown_s"),
		TEXT("mana"), TEXT("damage"), TEXT("blockable"), TEXT("telegraph_s"), TEXT("range_m")};
	for (const TCHAR* Column : Required)
	{
		if (!Rows.Keys.Contains(FString(Column)))
		{
			return Fail(Data, FString::Printf(TEXT("kernel data: spells-water.csv has no %s column"), Column));
		}
	}
	return true;
}

// Every lash tier that is a cone uses the tier 1 arc.
bool FindLashArc(const FCsvTable& Rows, double& LashArc, FKernelData& Data)
{
	const FString* LashShape = nullptr;
	for (const TMap<FString, FString>& Row : Rows.Rows)
	{
		if (Cell(Row, TEXT("line")) && *Cell(Row, TEXT("line")) == TEXT("lash") && Cell(Row, TEXT("tier")) && *Cell(Row, TEXT("tier")) == TEXT("1"))
		{
			LashShape = Cell(Row, TEXT("shape"));
		}
	}
	if (!LashShape)
	{
		return Fail(Data, TEXT("kernel data: missing lash tier 1 shape"));
	}
	return MatchDouble(*LashShape, TEXT("cone ([\\d.]+) deg"), LashArc, TEXT("lash arc"), Data);
}

// The cells every Water line has. RequireSpellColumns has already checked that each one exists.
FSpell SpellFromRow(const TMap<FString, FString>& Row)
{
	const FString& Line = *Cell(Row, TEXT("line"));
	const FString& TierText = *Cell(Row, TEXT("tier"));
	const FString& Branch = *Cell(Row, TEXT("branch"));
	const FString& DamageText = *Cell(Row, TEXT("damage"));
	const int32 Tier = FCString::Atoi(*TierText);
	FSpell Spell;
	Spell.Id = FString::Printf(TEXT("%s:%d:%s"), *Line, Tier, Branch.IsEmpty() ? TEXT("base") : *Branch);
	Spell.Line = Line;
	Spell.Tier = Tier;
	Spell.Branch = Branch;
	Spell.Name = *Cell(Row, TEXT("name"));
	Spell.CastS = FCString::Atod(**Cell(Row, TEXT("cast_s")));
	Spell.CooldownS = FCString::Atod(**Cell(Row, TEXT("cooldown_s")));
	Spell.Mana = FCString::Atod(**Cell(Row, TEXT("mana")));
	Spell.Damage = (!DamageText.IsEmpty() && FChar::IsDigit(DamageText[0])) ? FCString::Atod(*DamageText) : 0.0;
	Spell.Family = *Cell(Row, TEXT("blockable")) == TEXT("UNBLOCKABLE") ? TEXT("unblockable") : TEXT("magic");
	Spell.TelegraphS = FCString::Atod(**Cell(Row, TEXT("telegraph_s")));
	Spell.RangeM = FCString::Atod(**Cell(Row, TEXT("range_m")));
	Spell.Kind = TEXT("self");
	return Spell;
}

// bolt and tide_orb. The bolt row is also published as the Data.Bolt* fields.
bool ParseWaterProjectile(const FString& Shape, FSpell& Spell, FKernelData& Data)
{
	Spell.Kind = TEXT("projectile");
	if (Shape.Contains(TEXT("m/s")))
	{
		if (!MatchDouble(Shape, TEXT("([\\d.]+) m/s"), Spell.SpeedMps, Spell.Id, Data))
		{
			return false;
		}
	}
	else
	{
		Spell.SpeedMps = Data.FanSpeedMps;
	}
	if (Shape.Contains(TEXT("two projectiles")))
	{
		Spell.Count = 2;
		double Half = 0.0;
		if (!MatchDouble(Shape, TEXT("\\+-([\\d.]+) deg"), Half, Spell.Id, Data))
		{
			return false;
		}
		Spell.SpreadDeg = 2.0 * Half;
	}
	if (Shape.Contains(TEXT("-orb fan")))
	{
		double Count = 0.0;
		if (!MatchDouble(Shape, TEXT("(\\d+)-orb"), Count, Spell.Id, Data)
			|| !MatchDouble(Shape, TEXT("fan ([\\d.]+) deg"), Spell.SpreadDeg, Spell.Id, Data))
		{
			return false;
		}
		Spell.Count = static_cast<int32>(std::llround(Count));
	}
	if (Shape.Contains(TEXT("bursts")))
	{
		if (!MatchDouble(Shape, TEXT("bursts r ([\\d.]+) m"), Spell.BurstRadiusM, Spell.Id, Data))
		{
			return false;
		}
	}
	else if (Shape.Contains(TEXT(" r ")))
	{
		if (!MatchDouble(Shape, TEXT("r ([\\d.]+) m"), Spell.RadiusM, Spell.Id, Data))
		{
			return false;
		}
	}
	if (Spell.Line == TEXT("bolt"))
	{
		Data.BoltName = Spell.Name;
		Data.BoltCastS = Spell.CastS;
		Data.BoltCooldownS = Spell.CooldownS;
		Data.BoltMana = Spell.Mana;
		Data.BoltDamage = Spell.Damage;
		Data.BoltRangeM = Spell.RangeM;
		Data.BoltSpeedMps = Spell.SpeedMps;
	}
	return true;
}

bool ParseWaterLash(const FString& Shape, double LashArc, FSpell& Spell, FKernelData& Data)
{
	Spell.Kind = Spell.Tier == 3 ? TEXT("ring") : TEXT("cone");
	Spell.ArcDeg = Spell.Kind == TEXT("ring") ? 360.0 : LashArc;
	if (Shape.Contains(TEXT("pull")) || Shape.Contains(TEXT("push")))
	{
		Spell.Effect = Shape.Contains(TEXT("pull")) ? TEXT("pull") : TEXT("push");
		const FString Pattern = Shape.Contains(TEXT("pull")) ? TEXT("pull ([\\d.]+) m") : TEXT("push ([\\d.]+) m");
		if (!MatchDouble(Shape, Pattern, Spell.Amount, Spell.Id, Data))
		{
			return false;
		}
	}
	if (Shape.Contains(TEXT("root")))
	{
		Spell.Effect = TEXT("root");
		if (!MatchDouble(Shape, TEXT("root ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
		{
			return false;
		}
	}
	return true;
}

bool ParseWaterMire(const FString& Shape, FSpell& Spell, FKernelData& Data)
{
	const int32 Tier = Spell.Tier;
	Spell.Kind = Tier == 4 ? TEXT("target") : TEXT("zone");
	if (Spell.Kind == TEXT("zone") && !MatchDouble(Shape, TEXT("r ([\\d.]+) m"), Spell.RadiusM, Spell.Id, Data))
	{
		return false;
	}
	if (Tier == 1)
	{
		Spell.Effect = TEXT("slow");
		double Percent = 0.0;
		if (!MatchDouble(Shape, TEXT("slow ([\\d.]+)%"), Percent, Spell.Id, Data)
			|| !MatchDouble(Shape, TEXT("for ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
		{
			return false;
		}
		Spell.Amount = Percent / 100.0;
	}
	if (Tier == 2)
	{
		Spell.Effect = TEXT("fog");
		if (!MatchDouble(Shape, TEXT("for ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
		{
			return false;
		}
	}
	if (Tier == 3)
	{
		Spell.Effect = TEXT("root");
		if (!MatchDouble(Shape, TEXT("root ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
		{
			return false;
		}
	}
	if (Tier == 4)
	{
		Spell.Effect = TEXT("encase");
		if (!MatchDouble(Shape, TEXT("target ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
		{
			return false;
		}
	}
	return true;
}

bool ParseWaterMend(const FString& Shape, const FString& DamageText, FSpell& Spell, FKernelData& Data)
{
	const int32 Tier = Spell.Tier;
	if (Tier == 1)
	{
		Spell.Effect = TEXT("heal");
		if (!MatchDouble(DamageText, TEXT("heal ([\\d.]+)"), Spell.Amount, Spell.Id, Data))
		{
			return false;
		}
	}
	if (Tier == 2)
	{
		Spell.Effect = TEXT("ward");
		double Percent = 0.0;
		if (!MatchDouble(Shape, TEXT("-([\\d.]+)%"), Percent, Spell.Id, Data)
			|| !MatchDouble(Shape, TEXT("for ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
		{
			return false;
		}
		Spell.Amount = Percent / 100.0;
	}
	if (Tier == 3)
	{
		Spell.Effect = TEXT("hot");
		if (!MatchDouble(DamageText, TEXT("heal ([\\d.]+)"), Spell.Amount, Spell.Id, Data)
			|| !MatchDouble(DamageText, TEXT("over ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
		{
			return false;
		}
	}
	if (Tier == 4)
	{
		Spell.Effect = TEXT("font");
		if (!MatchDouble(Shape, TEXT("restore ([\\d.]+) mana"), Spell.Amount, Spell.Id, Data))
		{
			return false;
		}
	}
	return true;
}

// Mirror tier 4 also publishes Data.StoredFraction, read from the notes cell.
bool ParseWaterMirror(const TMap<FString, FString>& Row, const FString& Shape, const FString& DamageText, FSpell& Spell, FKernelData& Data)
{
	const int32 Tier = Spell.Tier;
	if (Tier == 1)
	{
		Spell.Effect = TEXT("sheen");
		double Percent = 0.0;
		if (!MatchDouble(Shape, TEXT("\\+([\\d.]+)%"), Percent, Spell.Id, Data)
			|| !MatchDouble(Shape, TEXT("self ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
		{
			return false;
		}
		Spell.Amount = Percent / 100.0;
	}
	if (Tier == 2)
	{
		Spell.Kind = TEXT("passive");
		Spell.Effect = Spell.Branch == TEXT("A") ? TEXT("reflection") : TEXT("ripple");
	}
	if (Tier == 3)
	{
		Spell.Effect = TEXT("decoy");
		if (!MatchDouble(Shape, TEXT("for ([\\d.]+) s"), Spell.DurationS, Spell.Id, Data))
		{
			return false;
		}
	}
	if (Tier == 4)
	{
		Spell.Kind = TEXT("wave");
		Spell.Effect = TEXT("return");
		if (!MatchDouble(DamageText, TEXT("max ([\\d.]+)"), Spell.Amount, Spell.Id, Data))
		{
			return false;
		}
		Spell.Damage = Spell.Amount;
		const FString* Notes = Cell(Row, TEXT("notes"));
		double Percent = 0.0;
		if (!Notes || !MatchDouble(*Notes, TEXT("stores ([\\d.]+)%"), Percent, Spell.Id, Data))
		{
			return false;
		}
		Data.StoredFraction = Percent / 100.0;
	}
	return true;
}

// The shape (and for mend and mirror the damage text) of one row, read by the rules of its line.
bool ParseWaterLine(const TMap<FString, FString>& Row, double LashArc, FSpell& Spell, FKernelData& Data)
{
	const FString& Line = Spell.Line;
	const FString& Shape = *Cell(Row, TEXT("shape"));
	const FString& DamageText = *Cell(Row, TEXT("damage"));
	if (Line == TEXT("bolt") || Line == TEXT("tide_orb"))
	{
		return ParseWaterProjectile(Shape, Spell, Data);
	}
	if (Line == TEXT("lash"))
	{
		return ParseWaterLash(Shape, LashArc, Spell, Data);
	}
	if (Line == TEXT("mire"))
	{
		return ParseWaterMire(Shape, Spell, Data);
	}
	if (Line == TEXT("mend"))
	{
		return ParseWaterMend(Shape, DamageText, Spell, Data);
	}
	if (Line == TEXT("mirror"))
	{
		return ParseWaterMirror(Row, Shape, DamageText, Spell, Data);
	}
	return Fail(Data, FString::Printf(TEXT("kernel data: unrecognized Water line %s"), *Line));
}

bool BuildSpells(const FCsvTable& Rows, FKernelData& Data)
{
	double LashArc = 0.0;
	if (!RequireSpellColumns(Rows, Data) || !FindLashArc(Rows, LashArc, Data))
	{
		return false;
	}
	for (const TMap<FString, FString>& Row : Rows.Rows)
	{
		FSpell Spell = SpellFromRow(Row);
		if (!ParseWaterLine(Row, LashArc, Spell, Data))
		{
			return false;
		}
		Data.Spells.Add(MoveTemp(Spell));
	}
	if (!(Data.StoredFraction > 0.0))
	{
		return Fail(Data, TEXT("kernel data: mirror tier 4 did not publish a stored fraction"));
	}
	return true;
}
}
