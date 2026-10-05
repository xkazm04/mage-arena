#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/KernelData.h"

namespace
{
const TCHAR* const EnemiesFile = TEXT("docs/design/baseline-fourteen-nights/design/data/enemies.json");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelLoadSeam, "MageArena.Kernel.LoadSeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelLoadSeam::RunTest(const FString& Parameters)
{
	// With no override the seam reads the same files as the process-wide load.
	const TMap<FString, FString> None;
	const FKernelData Plain = LoadKernelDataWithOverrides(None);
	if (!TestTrue(*FString::Printf(TEXT("loads with no override (%s)"), *Plain.Error), Plain.bReady))
	{
		return false;
	}
	TestEqual(TEXT("same water spell count as KernelData()"), Plain.Spells.Num(), KernelData().Spells.Num());
	TestEqual(TEXT("same enemy count as KernelData()"), Plain.Enemies.Num(), KernelData().Enemies.Num());
	TestEqual(TEXT("same fire spell count as KernelData()"), Plain.FireSpells.Num(), KernelData().FireSpells.Num());

	// Control: an override that is not JSON must fail the load, so a green run above is not the seam doing nothing.
	TMap<FString, FString> Broken;
	Broken.Add(KernelDataPinnedPath(EnemiesFile), TEXT("not json"));
	const FKernelData Bad = LoadKernelDataWithOverrides(Broken);
	TestFalse(TEXT("an override that is not JSON fails the load"), Bad.bReady);
	TestTrue(*FString::Printf(TEXT("the error names the overridden file (%s)"), *Bad.Error), Bad.Error.Contains(TEXT("enemies.json")));
	return true;
}

#endif
