#pragma once

#include "Kernel/KernelData.h"

TArray<FString> ValidateComposition(const FComposition& Composition);
FWaterState NewWaterState(const FComposition* Composition = nullptr);
const FSpell* SpellFor(const FActor& Actor, int32 Slot);
const FSpell* FindSpellById(const FString& Id);
const FSpell* FindSpellByEffect(const FString& Effect);
TArray<FString> LintSpells(const TArray<FSpell>* Catalog = nullptr);
