#include "Kernel/Catalog.h"

TArray<FString> ValidateComposition(const FComposition& Composition)
{
	TArray<FString> Errors;
	TSet<FString> Seen;
	for (const FString& Line : Composition.Lines)
	{
		Seen.Add(Line);
	}
	bool bKnown = true;
	for (const FString& Line : Composition.Lines)
	{
		if (!WaterLines().Contains(Line))
		{
			bKnown = false;
		}
	}
	if (Composition.Lines.Num() != KernelData().LineSlots || Seen.Num() != Composition.Lines.Num() || !bKnown)
	{
		Errors.Add(TEXT("Choose exactly three distinct Water lines."));
	}
	const FString Branches[] = {Composition.Branches.Lash, Composition.Branches.Mirror, Composition.Branches.TideOrb};
	const TCHAR* Names[] = {TEXT("lash"), TEXT("mirror"), TEXT("tide_orb")};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Branches[Index] != TEXT("A") && Branches[Index] != TEXT("B"))
		{
			Errors.Add(FString::Printf(TEXT("Choose branch A or B for %s."), Names[Index]));
		}
	}
	return Errors;
}

FWaterState NewWaterState(const FComposition* Composition)
{
	const FComposition& Source = Composition ? *Composition : KernelData().Presets[0];
	const TArray<FString> Errors = ValidateComposition(Source);
	checkf(Errors.Num() == 0, TEXT("%s"), Errors.Num() ? *Errors[0] : TEXT("composition"));
	FWaterState Water;
	Water.Composition = Source;
	return Water;
}

const FSpell* SpellFor(const FActor& Actor, int32 Slot)
{
	FString Line;
	if (Slot == 0)
	{
		Line = TEXT("bolt");
	}
	else if (Slot - 1 >= 0 && Slot - 1 < Actor.Water.Composition.Lines.Num())
	{
		Line = Actor.Water.Composition.Lines[Slot - 1];
	}
	else
	{
		return nullptr;
	}
	const int32 Tier = Slot == 0 ? 0 : Actor.Tier;
	FString Branch;
	if (Line == TEXT("lash"))
	{
		Branch = Actor.Water.Composition.Branches.Lash;
	}
	else if (Line == TEXT("mirror"))
	{
		Branch = Actor.Water.Composition.Branches.Mirror;
	}
	else if (Line == TEXT("tide_orb"))
	{
		Branch = Actor.Water.Composition.Branches.TideOrb;
	}
	for (const FSpell& Spell : KernelData().Spells)
	{
		if (Spell.Line == Line && Spell.Tier == Tier && (Spell.Branch.IsEmpty() || Spell.Branch == Branch))
		{
			return &Spell;
		}
	}
	return nullptr;
}

const FSpell* FindSpellById(const FString& Id)
{
	for (const FSpell& Spell : KernelData().Spells)
	{
		if (Spell.Id == Id)
		{
			return &Spell;
		}
	}
	for (const FSpell& Spell : KernelData().FireCatalog)
	{
		if (Spell.Id == Id)
		{
			return &Spell;
		}
	}
	return nullptr;
}

const FSpell* FindSpellByEffect(const FString& Effect)
{
	for (const FSpell& Spell : KernelData().Spells)
	{
		if (Spell.Effect == Effect)
		{
			return &Spell;
		}
	}
	return nullptr;
}

TArray<FString> LintSpells(const TArray<FSpell>* Catalog)
{
	const TArray<FSpell>& Spells = Catalog ? *Catalog : KernelData().Spells;
	const FKernelData& Data = KernelData();
	TArray<FString> Errors;
	for (const FString& Line : WaterLines())
	{
		for (int32 Tier = 1; Tier <= 4; ++Tier)
		{
			bool bFound = false;
			for (const FSpell& Spell : Spells)
			{
				if (Spell.Line == Line && Spell.Tier == Tier)
				{
					bFound = true;
				}
			}
			if (!bFound)
			{
				Errors.Add(FString::Printf(TEXT("Missing %s tier %d"), *Line, Tier));
			}
		}
	}
	for (const FSpell& Spell : Spells)
	{
		if (!Spell.Branch.IsEmpty() && !Data.BranchAtTiers.Contains(Spell.Tier))
		{
			Errors.Add(FString::Printf(TEXT("%s: illegal branch tier"), *Spell.Id));
		}
		if (Spell.Family == TEXT("unblockable") && Spell.TelegraphS < Data.MinimumTelegraphUnblockableS)
		{
			Errors.Add(FString::Printf(TEXT("%s: short unblockable warning"), *Spell.Id));
		}
		if (Spell.Kind == TEXT("zone") && Spell.TelegraphS < Data.MinimumTelegraphPositionalS)
		{
			Errors.Add(FString::Printf(TEXT("%s: short positional warning"), *Spell.Id));
		}
		const double Magnitudes[] = {Spell.CastS, Spell.CooldownS, Spell.Mana, Spell.Damage, Spell.TelegraphS, Spell.RangeM, Spell.DurationS, Spell.Amount};
		for (double Magnitude : Magnitudes)
		{
			if (!FMath::IsFinite(Magnitude) || Magnitude < 0.0)
			{
				Errors.Add(FString::Printf(TEXT("%s: invalid magnitude/unit"), *Spell.Id));
				break;
			}
		}
		int32 Copies = 0;
		for (const FSpell& Other : Spells)
		{
			if (Other.Id == Spell.Id)
			{
				++Copies;
			}
		}
		if (Copies != 1)
		{
			Errors.Add(FString::Printf(TEXT("%s: duplicate id"), *Spell.Id));
		}
	}
	return Errors;
}
