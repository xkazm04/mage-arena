#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/Games.h"
#include "Kernel/KernelData.h"
#include "MageArenaVR.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelEnemyStepCost, "MageArena.Kernel.EnemyStepCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelEnemyStepCost::RunTest(const FString& Parameters)
{
	// The kernel step budget is 1 ms (docs/PROJECT-PLAN.md section on CPU). The existing step-cost tests have no enemies in
	// them, so this one steps the two waves whose enemies read their behaviour text every tick: wave 0 (4 conscripts and 2
	// slingers) and wave 1 (3 cinder hounds and a mire maw). A reference-mage player keeps the bout alive while it is timed.
	if (!KernelData().bReady)
	{
		AddError(KernelData().Error);
		return false;
	}
	constexpr int32 Warmup = 20;
	constexpr int32 Samples = 300;
	bool bPass = true;
	for (int32 Wave = 0; Wave < 2; ++Wave)
	{
		FGames Games;
		if (!TestTrue(*FString::Printf(TEXT("wave %d creates"), Wave), TryCreateGames(Games, 40000, nullptr, Wave, true)))
		{
			return false;
		}
		for (int32 Index = 0; Index < Warmup && Games.Phase == TEXT("active"); ++Index)
		{
			StepGames(Games);
		}
		double SumUs = 0.0;
		double MaxUs = 0.0;
		int32 Measured = 0;
		for (int32 Index = 0; Index < Samples && Games.Phase == TEXT("active"); ++Index)
		{
			const double Start = FPlatformTime::Seconds();
			StepGames(Games);
			const double Us = (FPlatformTime::Seconds() - Start) * 1.0e6;
			SumUs += Us;
			MaxUs = FMath::Max(MaxUs, Us);
			++Measured;
		}
		bPass &= TestTrue(*FString::Printf(TEXT("wave %d stayed active for %d samples (got %d)"), Wave, Samples, Measured), Measured == Samples);
		const double MeanUs = Measured > 0 ? SumUs / static_cast<double>(Measured) : 0.0;
		UE_LOG(LogMageArena, Display, TEXT("EnemyStepUs wave=%d mean=%.3f max=%.3f samples=%d"), Wave, MeanUs, MaxUs, Measured);
		AddInfo(FString::Printf(TEXT("EnemyStepUs wave=%d mean=%.3f max=%.3f"), Wave, MeanUs, MaxUs));
		// 60 us is about 9 times the cost with Extract cached (6.4 us wave 0, 4.7 us wave 1, 2026-10-05) and well under the
		// 152 us wave 0 cost when every tick compiled a regex, so that regression is red here while a slower machine is not.
		bPass &= TestTrue(*FString::Printf(TEXT("wave %d mean step is within 60 us (%.1f us)"), Wave, MeanUs), MeanUs <= 60.0);
	}
	return bPass;
}

#endif
