#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Session/PresentationLook.h"
#include "Session/SessionPresentation.h"

// T49 follow-up: the enemy HP fill was the back's exact Y/Z section and sat centred in it, so no view ever showed it
// (T49's sampled bar pixel was the back colour before and after). The fill must stand proud of the back in Y and Z.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHpBarFillProud, "MageArena.Presentation.HpBarFillProud",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHpBarFillProud::RunTest(const FString& Parameters)
{
	const double MinFraction = PresentationLook::HpFillMinCm / PresentationLook::HpBarLengthCm;
	for (const double Fraction : {1.0, 0.5, MinFraction, 0.0})
	{
		const FHpBarParts Parts = SessionHpBar::Parts(Fraction);
		const FString At = FString::Printf(TEXT("fraction %.3f"), Fraction);
		TestTrue(At + TEXT(": fill Y half-extent exceeds the back's"), Parts.FillSizeCm.Y * 0.5 > Parts.BackSizeCm.Y * 0.5);
		TestTrue(At + TEXT(": fill Z half-extent exceeds the back's"), Parts.FillSizeCm.Z * 0.5 > Parts.BackSizeCm.Z * 0.5);
		TestEqual(At + TEXT(": the back keeps the bar length"), Parts.BackSizeCm.X, PresentationLook::HpBarLengthCm);
		TestEqual(At + TEXT(": the fill is centred in Y and Z"), FVector(0.0, Parts.FillOffsetCm.Y, Parts.FillOffsetCm.Z), FVector::ZeroVector);
		// Left-anchored: the fill's left edge sits on the back's left edge.
		const double FillLeft = Parts.FillOffsetCm.X - Parts.FillSizeCm.X * 0.5;
		const double BackLeft = -Parts.BackSizeCm.X * 0.5;
		if (Parts.FillSizeCm.X > PresentationLook::HpFillMinCm)
		{
			TestEqual(At + TEXT(": the fill is left-anchored"), FillLeft, BackLeft, 1e-6);
		}
		TestTrue(At + TEXT(": the fill is at least the minimum length"), Parts.FillSizeCm.X >= PresentationLook::HpFillMinCm);
	}
	const FHpBarParts Full = SessionHpBar::Parts(1.0);
	TestEqual(TEXT("a full bar's fill spans the back"), Full.FillSizeCm.X, Full.BackSizeCm.X);
	const FHpBarParts Half = SessionHpBar::Parts(0.5);
	TestEqual(TEXT("a half bar's fill is half the length"), Half.FillSizeCm.X, PresentationLook::HpBarLengthCm * 0.5);
	return true;
}

#endif
