#pragma once

#include "Kernel/KernelData.h"

#include "CoreMinimal.h"

class FJsonObject;

// Module-private. The loader readers and steps that cross a KernelData*.cpp seam; defined in KernelData.cpp unless a
// step's own file says otherwise. Not part of the public kernel data API (that is KernelData.h).
namespace KernelDataLoad
{
struct FCsvTable
{
	TArray<FString> Keys;
	TArray<TMap<FString, FString>> Rows;
};

FString PinnedPath(const TCHAR* Relative);
bool Fail(FKernelData& Data, const FString& Message);
bool LoadJson(const FString& Path, TSharedPtr<FJsonObject>& Out, FKernelData& Data);
bool NeedNumber(const FJsonObject& Object, const TCHAR* Field, double& Out, const FString& Path, FKernelData& Data);
bool NeedInt(const FJsonObject& Object, const TCHAR* Field, int32& Out, const FString& Path, FKernelData& Data);
bool NeedString(const FJsonObject& Object, const TCHAR* Field, FString& Out, const FString& Path, FKernelData& Data);
bool NeedObject(const FJsonObject& Object, const TCHAR* Field, const FJsonObject*& Out, const FString& Path, FKernelData& Data);
bool MatchDouble(const FString& Source, const FString& Pattern, double& Out, const FString& What, FKernelData& Data);
const FString* Cell(const TMap<FString, FString>& Row, const TCHAR* Key);

bool BuildSpells(const FCsvTable& Rows, FKernelData& Data);
bool LoadEnemies(const FJsonObject& Root, FKernelData& Data, const FString& Path);
bool LoadArenaTiers(FKernelData& Data);
}
