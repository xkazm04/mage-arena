#pragma once

#include "Kernel/KernelData.h"

#include "CoreMinimal.h"

// Module-private. The loader readers and steps that cross a KernelData*.cpp seam; defined in KernelData.cpp unless a
// step's own file says otherwise. Not part of the public kernel data API (that is KernelData.h).
namespace KernelDataLoad
{
struct FCsvTable
{
	TArray<FString> Keys;
	TArray<TMap<FString, FString>> Rows;
};

bool Fail(FKernelData& Data, const FString& Message);
bool MatchDouble(const FString& Source, const FString& Pattern, double& Out, const FString& What, FKernelData& Data);
const FString* Cell(const TMap<FString, FString>& Row, const TCHAR* Key);

bool BuildSpells(const FCsvTable& Rows, FKernelData& Data);
}
