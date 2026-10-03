#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandClipPlayer.h"
#include "Hands/HandInputSubsystem.h"
#include "MageArenaVR.h"

namespace
{
bool PlayClip(UHandInputSubsystem* Hands, const TCHAR* Action, EClipVariant Variant, FBlinkDetector& Detector, int32 Pad)
{
	Detector.Reset();
	Detector.SetActivePad(Pad);
	Hands->PlayQuickAction(Action, Variant);
	UHandClipPlayer* Player = Hands->GetClipPlayer();
	if (!Player || !Player->IsPlaying())
	{
		return false;
	}
	int32 Guard = 0;
	while (Player->IsPlaying() && Guard < 2000)
	{
		Hands->Step(1.0 / 72.0);
		++Guard;
	}
	Detector.Flush();
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBlinkClips, "MageArena.Blink.Clips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBlinkClips::RunTest(const FString& Parameters)
{
	bool bPass = true;
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	UHandInputSubsystem* Hands = NewObject<UHandInputSubsystem>(Instance);
	UBlinkDetectorSubsystem* Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
	Blinks->BindToHands(Hands);
	FBlinkDetector& Detector = Blinks->GetDetector();

	const EClipVariant Variants[] = {EClipVariant::Normal, EClipVariant::Slow, EClipVariant::Sloppy};
	const TCHAR* VariantNames[] = {TEXT("normal"), TEXT("slow"), TEXT("sloppy")};

	struct FExpect
	{
		const TCHAR* Action;
		int32 FromPad;
		int32 BlinkCount;
		int32 BoltCount;
		int32 Pad;
	};
	const FExpect Cases[] = {
		{TEXT("blink-left"), 1, 1, 0, 0},
		{TEXT("blink-right"), 1, 1, 0, 2},
		{TEXT("blink-back"), 1, 1, 0, 1},
		{TEXT("blink-left"), 0, 1, 0, 0},
		{TEXT("blink-right"), 0, 1, 0, 1},
		{TEXT("blink-back"), 0, 1, 0, 1},
		{TEXT("blink-left"), 2, 1, 0, 1},
		{TEXT("blink-right"), 2, 1, 0, 2},
		{TEXT("blink-back"), 2, 1, 0, 1},
		{TEXT("sigil-line1"), 1, 0, 0, 1},
		{TEXT("sigil-line2"), 1, 0, 0, 1},
		{TEXT("sigil-line3"), 1, 0, 0, 1},
		{TEXT("mudra"), 1, 0, 0, 1},
		{TEXT("ward-raise"), 1, 0, 0, 1},
		{TEXT("bolt"), 1, 0, 1, 1},
	};

	for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
	{
		for (const FExpect& Case : Cases)
		{
			const FString Label = FString::Printf(TEXT("%s.%s from %d"), Case.Action, VariantNames[VariantIndex], Case.FromPad);
			if (!PlayClip(Hands, Case.Action, Variants[VariantIndex], Detector, Case.FromPad))
			{
				AddError(Label + TEXT(" did not play"));
				bPass = false;
				continue;
			}
			if (Detector.GetBlinkCount() != Case.BlinkCount || Detector.GetBoltCount() != Case.BoltCount)
			{
				AddError(FString::Printf(TEXT("%s blinks=%d bolts=%d hottest=%.2f yaw=%.1f"),
					*Label, Detector.GetBlinkCount(), Detector.GetBoltCount(), Detector.GetHottestSpeed(),
					Detector.GetBlinkCount() > 0 ? Detector.GetLastBlink().YawDegrees : Detector.GetLastBolt().YawDegrees));
				bPass = false;
				continue;
			}
			if (Case.BlinkCount == 1)
			{
				bPass &= TestEqual(*Label, Detector.GetLastBlink().PadIndex, Case.Pad);
			}
		}
	}

	bPass &= TestEqual(TEXT("left from centre"), FBlinkDetector::ResolvePad(EBlinkDirection::Left, 1), 0);
	bPass &= TestEqual(TEXT("right from centre"), FBlinkDetector::ResolvePad(EBlinkDirection::Right, 1), 2);
	bPass &= TestEqual(TEXT("back from side"), FBlinkDetector::ResolvePad(EBlinkDirection::Back, 2), 1);

	Blinks->BindToHands(nullptr);
	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();
	return bPass;
}

#endif
