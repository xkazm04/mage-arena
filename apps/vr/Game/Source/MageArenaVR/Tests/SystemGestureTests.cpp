#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandClip.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageSettings.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// VRC.Quest.Input.8: the system gesture is reserved and must not trigger any other action.
// These tests play clips through the real pipeline (clip player, hand source, the four detectors).

namespace
{
struct FGestureCounts
{
	int32 WardRaised = 0;
	int32 WardLowered = 0;
	int32 Blinks = 0;
	int32 Bolts = 0;
	int32 SigilCasts = 0;
	int32 SigilRejects = 0;
	int32 Plants = 0;
	int32 Lifts = 0;
	int32 SystemFrames = 0;
	int32 SystemPinchFrames = 0;

	int32 Total() const
	{
		return WardRaised + WardLowered + Blinks + Bolts + SigilCasts + SigilRejects + Plants + Lifts;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("ward=%d/%d blink=%d bolt=%d sigil=%d/%d staff=%d/%d"),
			WardRaised, WardLowered, Blinks, Bolts, SigilCasts, SigilRejects, Plants, Lifts);
	}
};

struct FSystemRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;
	FGestureCounts Counts;
	FDelegateHandle FrameHandle;
	FMageSettingsState SavedSettings;

	bool Open(FAutomationTestBase& Test)
	{
		SavedSettings = FMageSettings::Get();
		FMageSettings::Restore(FMageSettingsState());
		Instance = NewObject<UGameInstance>(GetTransientPackage());
		Instance->AddToRoot();
		Hands = NewObject<UHandInputSubsystem>(Instance);
		Wards = NewObject<UWardDetectorSubsystem>(Instance);
		FString Error;
		if (!Wards->InitDetector(Error))
		{
			Test.AddError(Error);
			return false;
		}
		Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
		Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
		Sigils->LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
		Staff = NewObject<UStaffDetectorSubsystem>(Instance);

		Wards->OnWardRaised.AddLambda([this](double, FVector) { ++Counts.WardRaised; });
		Wards->OnWardLowered.AddLambda([this]() { ++Counts.WardLowered; });
		Blinks->OnBlink.AddLambda([this](const FBlinkEvent&) { ++Counts.Blinks; });
		Blinks->OnBolt.AddLambda([this](const FBoltFlickEvent&) { ++Counts.Bolts; });
		Sigils->OnSigilCast.AddLambda([this](FName, float, double) { ++Counts.SigilCasts; });
		Sigils->OnSigilRejected.AddLambda([this](double) { ++Counts.SigilRejects; });
		Staff->OnPlant.AddLambda([this]() { ++Counts.Plants; });
		Staff->OnLift.AddLambda([this]() { ++Counts.Lifts; });

		Bind();
		// Counts the frames that carry the bit, so a test can prove the bit reached the detectors' input.
		FrameHandle = Hands->OnHandFrame().AddLambda([this](const FHandFrame& Frame)
		{
			if (Frame.bSystemGesture)
			{
				++Counts.SystemFrames;
				Counts.SystemPinchFrames += Frame.Pinch >= 0.5f ? 1 : 0;
			}
		});
		return true;
	}

	/** A fresh stream for every detector: no history, no latched flick, no stroke, no grip window. */
	void Bind()
	{
		Wards->BindToHands(Hands);
		Blinks->BindToHands(Hands);
		Sigils->BindToHands(Hands);
		Staff->BindToHands(Hands);
	}

	void Close()
	{
		if (Hands)
		{
			Hands->OnHandFrame().Remove(FrameHandle);
			Hands->StopAll();
		}
		if (Wards)
		{
			Wards->BindToHands(nullptr);
		}
		if (Blinks)
		{
			Blinks->BindToHands(nullptr);
		}
		if (Sigils)
		{
			Sigils->BindToHands(nullptr);
		}
		if (Staff)
		{
			Staff->BindToHands(nullptr);
		}
		if (Instance)
		{
			Instance->RemoveFromRoot();
		}
		FMageSettings::Restore(SavedSettings);
	}

	/** Plays the clip to its end through the action lane and flushes a flick that was still pending. */
	bool Play(const FHandClip& Clip)
	{
		Bind();
		Counts = FGestureCounts();
		Hands->PlayLoadedClip(Clip);
		UHandClipPlayer* Player = Hands->GetClipPlayer();
		if (!Player || !Player->IsPlaying())
		{
			return false;
		}
		int32 Guard = 0;
		while (Player->IsPlaying() && Guard < 4000)
		{
			Hands->Step(1.0 / 72.0);
			++Guard;
		}
		Blinks->FlushPending();
		return Guard < 4000;
	}
};

FString QuickClipPath(const TCHAR* Action)
{
	return FHandClip::MakeFilePath(Action, EClipVariant::Normal);
}

FString SystemClipPath()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("system"), TEXT("system-gesture.normal.jsonl"));
}

void ForceSystemGesture(FHandClip& Clip)
{
	for (FHandClipTrack& Track : Clip.Tracks)
	{
		for (FHandFrame& Frame : Track.Frames)
		{
			Frame.bSystemGesture = true;
		}
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSystemGestureZeroEvents, "MageArena.Hands.SystemGesture.ZeroEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSystemGestureZeroEvents::RunTest(const FString& Parameters)
{
	FHandClip Clip;
	FString Error;
	if (!FHandClip::LoadFromFile(SystemClipPath(), Clip, Error))
	{
		AddError(Error);
		return false;
	}
	FSystemRig Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	bool bPass = TestTrue(TEXT("system gesture clip plays to its end"), Rig.Play(Clip));
	// Not vacuous: the bit and a pinch both reached the detectors, on the casting hand that a pinch would start a stroke on.
	bPass &= TestTrue(TEXT("frames reached the pipeline with the bit set"), Rig.Counts.SystemFrames > 0);
	bPass &= TestTrue(TEXT("a pinch happened while the bit was set"), Rig.Counts.SystemPinchFrames > 0);
	bPass &= TestTrue(TEXT("the clip's hand is the casting hand"), Clip.GetHands().Contains(FMageSettings::CastingHand()));
	bPass &= TestEqual(*FString::Printf(TEXT("no ward, blink, sigil or staff event (%s)"), *Rig.Counts.ToString()), Rig.Counts.Total(), 0);
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSystemGestureGateIsTheCause, "MageArena.Hands.SystemGesture.GateIsTheCause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSystemGestureGateIsTheCause::RunTest(const FString& Parameters)
{
	// One clip per detector. Each fires its event as recorded, and none fires with the bit forced on every frame.
	struct FCase
	{
		const TCHAR* Action;
		const TCHAR* Detector;
		int32 FGestureCounts::*Events;
	};
	const FCase Cases[] = {
		{TEXT("ward-raise"), TEXT("ward"), &FGestureCounts::WardRaised},
		{TEXT("blink-left"), TEXT("blink"), &FGestureCounts::Blinks},
		{TEXT("sigil-line1"), TEXT("sigil"), &FGestureCounts::SigilCasts},
		{TEXT("staff-plant"), TEXT("staff"), &FGestureCounts::Plants},
	};

	FSystemRig Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	bool bPass = true;
	for (const FCase& Case : Cases)
	{
		FHandClip Clip;
		FString Error;
		if (!FHandClip::LoadFromFile(QuickClipPath(Case.Action), Clip, Error))
		{
			AddError(Error);
			bPass = false;
			continue;
		}
		bPass &= TestTrue(FString::Printf(TEXT("%s plays"), Case.Action), Rig.Play(Clip));
		bPass &= TestTrue(FString::Printf(TEXT("%s unchanged fires the %s event (%s)"), Case.Action, Case.Detector, *Rig.Counts.ToString()),
			Rig.Counts.*Case.Events >= 1);
		bPass &= TestEqual(FString::Printf(TEXT("%s unchanged carries no system frame"), Case.Action), Rig.Counts.SystemFrames, 0);

		ForceSystemGesture(Clip);
		bPass &= TestTrue(FString::Printf(TEXT("%s with the bit forced plays"), Case.Action), Rig.Play(Clip));
		bPass &= TestTrue(FString::Printf(TEXT("%s forced frames carry the bit"), Case.Action), Rig.Counts.SystemFrames > 0);
		bPass &= TestEqual(FString::Printf(TEXT("%s with the bit forced yields no event (%s)"), Case.Action, *Rig.Counts.ToString()),
			Rig.Counts.Total(), 0);
	}
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSystemGestureSchemaV1Closed, "MageArena.Hands.SystemGesture.SchemaV1Closed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSystemGestureSchemaV1Closed::RunTest(const FString& Parameters)
{
	bool bPass = true;
	// An @1 clip loads unchanged and never carries the bit.
	FHandClip V1;
	FString Error;
	if (!FHandClip::LoadFromFile(QuickClipPath(TEXT("ward-raise")), V1, Error))
	{
		AddError(Error);
		return false;
	}
	bool bAnySystem = false;
	for (const FHandClipTrack& Track : V1.GetTracks())
	{
		for (const FHandFrame& Frame : Track.Frames)
		{
			bAnySystem |= Frame.bSystemGesture;
		}
	}
	bPass &= TestFalse(TEXT("an @1 clip never carries the bit"), bAnySystem);
	bPass &= TestTrue(TEXT("an @1 clip keeps its samples"), V1.GetSampleCount() > 1);

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *QuickClipPath(TEXT("ward-raise"))))
	{
		AddError(TEXT("cannot read ward-raise.normal.jsonl"));
		return false;
	}
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("system-gesture"));
	const FString WithSys = Text.Replace(TEXT(",\"joints\":"), TEXT(",\"sys\":true,\"joints\":"), ESearchCase::CaseSensitive);
	bPass &= TestTrue(TEXT("the sys edit changed the clip"), WithSys.Len() > Text.Len());

	auto LoadVariant = [&Dir](const TCHAR* Name, const FString& Body, FHandClip& Out, FString& OutError)
	{
		const FString Path = FPaths::Combine(Dir, Name);
		FFileHelper::SaveStringToFile(Body, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		const bool bLoaded = FHandClip::LoadFromFile(Path, Out, OutError);
		IFileManager::Get().Delete(*Path);
		return bLoaded;
	};

	FHandClip Rejected;
	FString RejectedError;
	bPass &= TestFalse(TEXT("an @1 clip that carries sys is rejected"), LoadVariant(TEXT("v1-with-sys.jsonl"), WithSys, Rejected, RejectedError));
	bPass &= TestTrue(TEXT("the rejection names sys"), RejectedError.Contains(TEXT("sys")));

	// The same bytes under @2 load, and the bit arrives on the frames. That pins the rejection above on the schema.
	const FString AsV2 = WithSys.Replace(TEXT("hand-clip@1"), TEXT("hand-clip@2"), ESearchCase::CaseSensitive);
	FHandClip V2;
	FString V2Error;
	if (LoadVariant(TEXT("v2-with-sys.jsonl"), AsV2, V2, V2Error))
	{
		bool bAllSystem = V2.GetSampleCount() > 0;
		for (const FHandClipTrack& Track : V2.GetTracks())
		{
			for (const FHandFrame& Frame : Track.Frames)
			{
				bAllSystem &= Frame.bSystemGesture;
			}
		}
		bPass &= TestTrue(TEXT("@2 sys true reaches every frame"), bAllSystem);
	}
	else
	{
		AddError(FString::Printf(TEXT("the same clip as @2 must load: %s"), *V2Error));
		bPass = false;
	}

	// An @2 sys must be a boolean.
	const FString NotBool = AsV2.Replace(TEXT("\"sys\":true"), TEXT("\"sys\":1"), ESearchCase::CaseSensitive);
	FHandClip BadBool;
	FString BadBoolError;
	bPass &= TestFalse(TEXT("an @2 sys that is not a boolean is rejected"), LoadVariant(TEXT("v2-sys-number.jsonl"), NotBool, BadBool, BadBoolError));

	// An @2 clip with no sys field is an @1 clip: absent means false.
	const FString V2NoSys = Text.Replace(TEXT("hand-clip@1"), TEXT("hand-clip@2"), ESearchCase::CaseSensitive);
	FHandClip NoSys;
	FString NoSysError;
	if (!LoadVariant(TEXT("v2-no-sys.jsonl"), V2NoSys, NoSys, NoSysError))
	{
		AddError(FString::Printf(TEXT("an @2 clip without sys must load: %s"), *NoSysError));
		bPass = false;
	}
	return bPass;
}

#endif
