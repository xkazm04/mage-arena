#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/AbsorbResolver.h"
#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "HeadMountedDisplayTypes.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandClip.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageSettings.h"
#include "MageArenaVR.h"
#include "Misc/Paths.h"

// F7(b) of docs/research/STACK-OPPORTUNITIES-2026-10.md: do the ward, blink and sigil gestures still work when the hands
// update at about 30 Hz? The worst case is modelled in memory: every pose is held for 1/30 s, still emitted at 72 Hz,
// and nothing predicts between updates. No clip file is written. Every test plays the held copy through the real
// pipeline (PlayLoadedClip, the hand source, the detectors) and compares it with the unmodified clip.
//
// G1 of docs/research/FEASIBILITY-2026-10.md: the runtime extrapolates poses to the display time by default, so the held
// stream is the floor, not the real case. A second stream model extrapolates from the last two camera samples, and both
// models also run at 25 Hz, a bound: whether default mode drops to 25 Hz on 50 Hz mains is unconfirmed.

namespace HeldSampleTestsImpl
{
/** The modelled camera rate. The mutation proof sets this to 72, which turns the hold off. */
constexpr double GCameraHz = 30.0;
/** The rate the pose-count bound is written for. Not the same constant on purpose: TransformIsReal must notice a change. */
constexpr double GBoundHz = 30.0;
/** The lower camera rate (G1). The mutation proof sets this to 72, which turns both models into the identity. */
constexpr double GLowCameraHz = 25.0;
/** The pose-count bound for GLowCameraHz. Separate for the same reason as GBoundHz. */
constexpr double GLowBoundHz = 25.0;
/** Three alignments of the camera grid against the 72 Hz emit grid, so one lucky alignment cannot pass. */
constexpr double GPhases[] = {0.0, 1.0 / 90.0, 2.0 / 90.0};
constexpr int32 GPhaseCount = UE_ARRAY_COUNT(GPhases);

/**
 * RATCHET (F7(b)). A case whose held result differs from its 72 Hz result and that is listed here keeps the suite green.
 * A case that misses and is not listed fails. A listed case that now matches also fails, so a detector fix shrinks this
 * table on purpose. Key: "<action>.<variant>.p<phase index>".
 */
const TArray<FString>& KnownHeldMisses()
{
	// Empty since Gestures/HeldPointRate.h. blink-back.sloppy was listed in all three phases: the held stream released the
	// flick on the zero-speed repeat after its first stepped burst, a wind-up at yaw 6 to 10 degrees, so it read as a bolt.
	static const TArray<FString> Misses;
	return Misses;
}

/** The F7(b) follow-up bound on the held ward onset: one 30 Hz camera interval plus one 72 Hz frame, 47.2 ms. */
constexpr double GOnsetBoundMs = (1.0 / 30.0 + 1.0 / 72.0) * 1000.0;
/**
 * Hits after the 72 Hz onset that sit inside the 0.15 s perfect window with room for the onset bound on both sides. The
 * window edges themselves (onset and onset + 0.15 s) are logged only: a 30 Hz onset is quantized and cannot keep them.
 */
constexpr double GInteriorProbesS[] = {0.05, 0.10};

/**
 * RATCHET for the ward timing (F7(b) follow-up), with the rules of KnownHeldMisses. A ward case misses when its held onset is
 * more than GOnsetBoundMs from the 72 Hz onset, or when a hit at an interior probe is not perfect on held hands. A miss that
 * is not listed fails; a listed case that now holds fails too. Key: "ward-raise.<variant>.p<phase index>".
 */
const TArray<FString>& KnownHeldWardMisses()
{
	// Empty: with Gestures/HeldPointRate.h every case holds the bound and both interior probes.
	static const TArray<FString> Misses;
	return Misses;
}

/**
 * RATCHET for the action results on the G1 streams (extrapolated 30 Hz, held and extrapolated 25 Hz, and staff on every
 * stream), with the rules of KnownHeldMisses. Key: "<stream>.<action>.<variant>.p<phase index>", plus ".rearm" for the
 * blink re-arm check of HeldSample.BlinkModels.
 */
const TArray<FString>& KnownModelMisses()
{
	// Every entry is a blink re-arm (".rearm", HeldSample.BlinkModels); every blink result itself matches 72 Hz.
	// - held30 slow: the re-arm comes 167 to 236 ms late, while the flick fires 14 to 42 ms late. held25 does not miss, so
	//   this is a phase effect of the 30 Hz grid on the slow decay, not a bound that grows with the period.
	// - extrap30 slow and some sloppy: the extrapolated stream fires the flick early (55 to 69 ms on normal, up to 125 ms on
	//   slow), before the tip has peaked; that is consistent with a speed dip after a camera update falling under
	//   FallFraction of the peak so far. The tip is then still fast, and the re-arm comes 56 to 181 ms late.
	// - extrap25: the same early fire, larger: every case but two misses, normal 69 to 97 ms late, slow 167 to 194 ms.
	// A late re-arm can only lose a second flick that follows within about 0.2 s. Fix: a detector change (the fall test
	// and the re-arm on extrapolated and held samples), a separate run; W1 and W2 do not touch blink.
	static const TArray<FString> Misses = []()
	{
		const TCHAR* Cases[] = {
			TEXT("held30.blink-left.slow.p0"), TEXT("held30.blink-left.slow.p1"), TEXT("held30.blink-left.slow.p2"),
			TEXT("held30.blink-right.slow.p0"), TEXT("held30.blink-right.slow.p1"), TEXT("held30.blink-right.slow.p2"),
			TEXT("held30.blink-back.slow.p0"), TEXT("held30.blink-back.slow.p1"), TEXT("held30.blink-back.slow.p2"),
			TEXT("extrap30.blink-left.slow.p0"), TEXT("extrap30.blink-left.slow.p1"), TEXT("extrap30.blink-left.slow.p2"),
			TEXT("extrap30.blink-left.sloppy.p2"),
			TEXT("extrap30.blink-right.slow.p0"), TEXT("extrap30.blink-right.slow.p1"), TEXT("extrap30.blink-right.slow.p2"),
			TEXT("extrap30.blink-right.sloppy.p0"), TEXT("extrap30.blink-right.sloppy.p1"),
			TEXT("extrap30.blink-back.slow.p0"), TEXT("extrap30.blink-back.slow.p1"), TEXT("extrap30.blink-back.slow.p2"),
			TEXT("extrap30.blink-back.sloppy.p0"),
			TEXT("extrap25.blink-left.normal.p0"), TEXT("extrap25.blink-left.normal.p1"), TEXT("extrap25.blink-left.normal.p2"),
			TEXT("extrap25.blink-left.slow.p0"), TEXT("extrap25.blink-left.slow.p1"), TEXT("extrap25.blink-left.slow.p2"),
			TEXT("extrap25.blink-left.sloppy.p2"),
			TEXT("extrap25.blink-right.normal.p0"), TEXT("extrap25.blink-right.normal.p1"), TEXT("extrap25.blink-right.normal.p2"),
			TEXT("extrap25.blink-right.slow.p0"), TEXT("extrap25.blink-right.slow.p1"), TEXT("extrap25.blink-right.slow.p2"),
			TEXT("extrap25.blink-right.sloppy.p0"), TEXT("extrap25.blink-right.sloppy.p1"), TEXT("extrap25.blink-right.sloppy.p2"),
			TEXT("extrap25.blink-back.normal.p0"), TEXT("extrap25.blink-back.normal.p1"), TEXT("extrap25.blink-back.normal.p2"),
			TEXT("extrap25.blink-back.slow.p0"), TEXT("extrap25.blink-back.slow.p1"), TEXT("extrap25.blink-back.slow.p2"),
			TEXT("extrap25.blink-back.sloppy.p0"), TEXT("extrap25.blink-back.sloppy.p1"), TEXT("extrap25.blink-back.sloppy.p2")};
		TArray<FString> Keys;
		for (const TCHAR* Case : Cases)
		{
			Keys.Add(FString::Printf(TEXT("%s.rearm"), Case));
		}
		return Keys;
	}();
	return Misses;
}

/** RATCHET for the ward timing on the G1 streams, with the rules of KnownHeldWardMisses. Key as in KnownModelMisses. */
const TArray<FString>& KnownModelWardMisses()
{
	static const TArray<FString> Misses;
	return Misses;
}

/**
 * Held copy of a clip. Every frame keeps its time, so hz stays 72 and the 1/72 spacing holds. Pose, pinch and confidence
 * come from the source at the latest camera time Phase + k / CameraHz that is <= the frame time, clamped to the clip start.
 * Every other frame field, bSystemGesture included, is copied from the source frame.
 */
FHandClip MakeHeldClip(const FHandClip& Source, double CameraHz, double Phase)
{
	FHandClip Held = Source;
	for (FHandClipTrack& Track : Held.Tracks)
	{
		for (FHandFrame& Frame : Track.Frames)
		{
			const double Ticks = FMath::FloorToDouble((Frame.TimeSeconds - Phase) * CameraHz + 1.0e-9);
			const double CameraTime = FMath::Max(0.0, Phase + Ticks / CameraHz);
			FHandFrame Sampled;
			if (Source.Sample(Track.Hand, CameraTime, Sampled))
			{
				Frame.Joints = Sampled.Joints;
				Frame.Pinch = Sampled.Pinch;
				Frame.Confidence = Sampled.Confidence;
			}
		}
	}
	return Held;
}

/**
 * MODEL of an extrapolating runtime (G1). Meta states that poses are extrapolated to the display time, not how, so this is
 * a model and not Meta's method. Every frame keeps its time. The latest camera sample c1 = Phase + k / CameraHz <= the frame
 * time and the one before it, c0 = c1 - 1 / CameraHz, are extrapolated linearly to the frame time, capped at one camera
 * period: locations by the fraction (t - c1) * CameraHz of the step c0 to c1, rotations by the same fraction of the rotation
 * c0 to c1. Pinch, confidence and every other field are held as in MakeHeldClip: G1 extrapolates joint locations and
 * rotations only, and whether the runtime extrapolates pinch strength is unknown. (Measured once with pinch extrapolated too: the extrapolated
 * 30 Hz corpus then gave 30 zigzag impostor casts instead of 9.) Until c0 exists (the first camera period) the pose is held.
 */
FHandClip MakeExtrapolatedClip(const FHandClip& Source, double CameraHz, double Phase)
{
	FHandClip Extrapolated = Source;
	const double Period = 1.0 / CameraHz;
	for (FHandClipTrack& Track : Extrapolated.Tracks)
	{
		for (FHandFrame& Frame : Track.Frames)
		{
			const double Ticks = FMath::FloorToDouble((Frame.TimeSeconds - Phase) * CameraHz + 1.0e-9);
			const double Latest = Phase + Ticks / CameraHz;
			FHandFrame Now;
			if (!Source.Sample(Track.Hand, FMath::Max(0.0, Latest), Now))
			{
				continue;
			}
			Frame.Joints = Now.Joints;
			Frame.Pinch = Now.Pinch;
			Frame.Confidence = Now.Confidence;
			const double Previous = Latest - Period;
			FHandFrame Before;
			if (Previous < -1.0e-9 || !Source.Sample(Track.Hand, Previous, Before) || Before.Joints.Num() != Now.Joints.Num())
			{
				continue;
			}
			const double Fraction = FMath::Clamp((Frame.TimeSeconds - Latest) * CameraHz, 0.0, 1.0);
			for (int32 Index = 0; Index < Now.Joints.Num(); ++Index)
			{
				const FHandJointPose& From = Before.Joints[Index];
				const FHandJointPose& To = Now.Joints[Index];
				Frame.Joints[Index].Location = To.Location + (To.Location - From.Location) * Fraction;
				// The world-frame step that takes c0 to c1, applied again for the fraction of a period.
				const FQuat Step = To.Rotation * From.Rotation.Inverse();
				Frame.Joints[Index].Rotation = (FQuat::Slerp(FQuat::Identity, Step, Fraction) * To.Rotation).GetNormalized();
			}
		}
	}
	return Extrapolated;
}

enum class EStreamModel : uint8
{
	Held,
	Extrapolated
};

/** One modelled hand stream: how poses reach the 72 Hz frames, and the camera rate. */
struct FStream
{
	const TCHAR* Name;
	EStreamModel Model;
	double CameraHz;
};

const FStream GHeld30{TEXT("held30"), EStreamModel::Held, GCameraHz};
const FStream GExtrapolated30{TEXT("extrap30"), EStreamModel::Extrapolated, GCameraHz};
const FStream GHeld25{TEXT("held25"), EStreamModel::Held, GLowCameraHz};
const FStream GExtrapolated25{TEXT("extrap25"), EStreamModel::Extrapolated, GLowCameraHz};

/** The G1 streams the model tests run, beside the F7(b) held 30 Hz stream. */
const FStream* const GModelStreams[] = {&GExtrapolated30, &GHeld25, &GExtrapolated25};

FHandClip MakeStream(const FHandClip& Source, const FStream& Stream, double Phase)
{
	return Stream.Model == EStreamModel::Held ? MakeHeldClip(Source, Stream.CameraHz, Phase) : MakeExtrapolatedClip(Source, Stream.CameraHz, Phase);
}

/** The ward onset bound of a stream: one camera interval plus one 72 Hz frame. 47.2 ms at 30 Hz, 53.9 ms at 25 Hz. */
double OnsetBoundMs(double CameraHz)
{
	return (1.0 / CameraHz + 1.0 / 72.0) * 1000.0;
}

bool SamePose(const FHandFrame& A, const FHandFrame& B)
{
	// Tolerances absorb the slerp rounding that a sample of two equal poses can add.
	if (!FMath::IsNearlyEqual(A.Pinch, B.Pinch, 1.0e-6f) || !FMath::IsNearlyEqual(A.Confidence, B.Confidence, 1.0e-6f) || A.Joints.Num() != B.Joints.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < A.Joints.Num(); ++Index)
	{
		if (!A.Joints[Index].Location.Equals(B.Joints[Index].Location, 1.0e-4) || !A.Joints[Index].Rotation.Equals(B.Joints[Index].Rotation, 1.0e-5))
		{
			return false;
		}
	}
	return true;
}

int32 DistinctPoses(const FHandClipTrack& Track)
{
	int32 Count = Track.Frames.Num() > 0 ? 1 : 0;
	for (int32 Index = 1; Index < Track.Frames.Num(); ++Index)
	{
		Count += SamePose(Track.Frames[Index], Track.Frames[Index - 1]) ? 0 : 1;
	}
	return Count;
}

/** The clip with its last pose held still for StillS more, so a latched flick has room to re-arm before the clip ends. */
FHandClip MakeStillTailClip(const FHandClip& Source, double StillS)
{
	FHandClip Out = Source;
	const double Frame = 1.0 / 72.0;
	const int32 Count = FMath::CeilToInt(StillS / Frame);
	for (FHandClipTrack& Track : Out.Tracks)
	{
		if (Track.Frames.Num() == 0)
		{
			continue;
		}
		const FHandFrame Last = Track.Frames.Last();
		for (int32 Index = 1; Index <= Count; ++Index)
		{
			FHandFrame Still = Last;
			Still.TimeSeconds = Last.TimeSeconds + Index * Frame;
			Track.Frames.Add(Still);
		}
	}
	Out.Duration = Source.GetDuration() + Count * Frame;
	return Out;
}

/** The first flick of a stream and its re-arm, read from a bare blink detector on the casting-hand track. */
struct FRearm
{
	int32 Fires = 0;
	double FireFrameS = -1.0;
	double RearmS = -1.0;
};

/**
 * The detector latches a fired flick until the tip has been quiet for RearmQuietS. HottestSpeed stays at or above the arming
 * speed while it is latched and drops below it on the re-arm, so the first frame after the fire with a hottest speed under
 * MinTipSpeedMps is the re-arm.
 */
FRearm MeasureRearm(const FHandClip& Clip)
{
	FRearm Out;
	FBlinkDetector Detector;
	Detector.Reset();
	for (const FHandClipTrack& Track : Clip.Tracks)
	{
		if (Track.Hand != FMageSettings::CastingHand())
		{
			continue;
		}
		for (const FHandFrame& Frame : Track.Frames)
		{
			Detector.Ingest(Frame);
			const int32 Fires = Detector.GetBlinkCount() + Detector.GetBoltCount();
			if (Fires > Out.Fires && Out.FireFrameS < 0.0)
			{
				Out.FireFrameS = Frame.TimeSeconds;
			}
			else if (Out.FireFrameS >= 0.0 && Out.RearmS < 0.0 && Detector.GetHottestSpeed() < FBlinkThresholds::MinTipSpeedMps)
			{
				Out.RearmS = Frame.TimeSeconds;
			}
			Out.Fires = Fires;
		}
	}
	return Out;
}

struct FRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;
	FMageSettingsState SavedSettings;

	int32 WardRaised = 0;
	double FirstOnset = -1.0;
	TArray<int32> BlinkDirections;
	TArray<double> BlinkTimes;
	int32 Bolts = 0;
	TArray<FName> Casts;
	int32 Rejects = 0;
	int32 Plants = 0;
	int32 Lifts = 0;

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

		Wards->OnWardRaised.AddLambda([this](double Onset, FVector)
		{
			FirstOnset = WardRaised == 0 ? Onset : FirstOnset;
			++WardRaised;
		});
		Blinks->OnBlink.AddLambda([this](const FBlinkEvent& Event)
		{
			BlinkDirections.Add(static_cast<int32>(Event.Direction));
			BlinkTimes.Add(Event.TimeSeconds);
		});
		Blinks->OnBolt.AddLambda([this](const FBoltFlickEvent&) { ++Bolts; });
		Sigils->OnSigilCast.AddLambda([this](FName Line, float, double) { Casts.Add(Line); });
		Sigils->OnSigilRejected.AddLambda([this](double) { ++Rejects; });
		Staff->OnPlant.AddLambda([this]() { ++Plants; });
		Staff->OnLift.AddLambda([this]() { ++Lifts; });
		return true;
	}

	void Close()
	{
		if (Hands)
		{
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

	/** Plays the clip to its end through the action lane with a fresh stream for every detector. */
	bool Play(const FHandClip& Clip)
	{
		Wards->BindToHands(Hands);
		Blinks->BindToHands(Hands);
		Sigils->BindToHands(Hands);
		Staff->BindToHands(Hands);
		WardRaised = 0;
		FirstOnset = -1.0;
		BlinkDirections.Reset();
		BlinkTimes.Reset();
		Bolts = 0;
		Casts.Reset();
		Rejects = 0;
		Plants = 0;
		Lifts = 0;
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

	FString Result() const
	{
		FString Out = FString::Printf(TEXT("ward=%d"), WardRaised);
		Out += TEXT(" blink=");
		for (const int32 Direction : BlinkDirections)
		{
			Out += FString::Printf(TEXT("%d,"), Direction);
		}
		Out += FString::Printf(TEXT(" bolt=%d sigil="), Bolts);
		for (const FName Line : Casts)
		{
			Out += Line.ToString() + TEXT(",");
		}
		Out += FString::Printf(TEXT(" reject=%d"), Rejects);
		return Out;
	}
};

const TCHAR* VariantNames[] = {TEXT("normal"), TEXT("slow"), TEXT("sloppy")};
const EClipVariant Variants[] = {EClipVariant::Normal, EClipVariant::Slow, EClipVariant::Sloppy};

FString CaseKey(const TCHAR* Action, int32 VariantIndex, int32 PhaseIndex)
{
	return FString::Printf(TEXT("%s.%s.p%d"), Action, VariantNames[VariantIndex], PhaseIndex);
}

/** Shared by the three action tests: the match is judged on the part of the result that belongs to the action. */
enum class EKind : uint8
{
	Ward,
	Blink,
	Sigil,
	Staff
};

FString Relevant(const FRig& Rig, EKind Kind)
{
	switch (Kind)
	{
	case EKind::Ward:
		return FString::Printf(TEXT("ward x%d"), Rig.WardRaised);
	case EKind::Blink:
	{
		FString Out = FString::Printf(TEXT("blink x%d"), Rig.BlinkDirections.Num());
		for (const int32 Direction : Rig.BlinkDirections)
		{
			Out += FString::Printf(TEXT(" dir%d"), Direction);
		}
		return Out + FString::Printf(TEXT(" bolt x%d"), Rig.Bolts);
	}
	case EKind::Staff:
		return FString::Printf(TEXT("plant x%d lift x%d"), Rig.Plants, Rig.Lifts);
	default:
	{
		FString Out = FString::Printf(TEXT("sigil x%d"), Rig.Casts.Num());
		for (const FName Line : Rig.Casts)
		{
			Out += TEXT(" ") + Line.ToString();
		}
		return Out + FString::Printf(TEXT(" reject x%d"), Rig.Rejects);
	}
	}
}

bool LoadActionClip(FAutomationTestBase& Test, const TCHAR* Action, int32 VariantIndex, FHandClip& Out)
{
	FString Error;
	if (!FHandClip::LoadFromFile(FHandClip::MakeFilePath(Action, Variants[VariantIndex]), Out, Error))
	{
		Test.AddError(FString::Printf(TEXT("%s.%s: %s"), Action, VariantNames[VariantIndex], *Error));
		return false;
	}
	return true;
}

/** What the ward cases measured, for the summary line of HeldSample.Ward. */
struct FWardFigures
{
	TArray<double> OnsetDeltasMs;
	int32 Cases = 0;
	int32 InteriorHeld = 0;
	int32 WithinBound = 0;
};

/**
 * Runs one action over every variant and phase of a stream against the ratchet. Returns false on a failure. The F7(b)
 * cases use KnownHeldMisses and KnownHeldWardMisses with plain keys; bModelTables selects the G1 tables and stream-prefixed
 * keys.
 */
bool RunRatchet(FAutomationTestBase& Test, const TCHAR* Action, EKind Kind, const FStream& Stream, FWardFigures* OutWard,
	bool bModelTables = false)
{
	const double BoundMs = OnsetBoundMs(Stream.CameraHz);
	const TArray<FString>& Misses = bModelTables ? KnownModelMisses() : KnownHeldMisses();
	const TArray<FString>& WardMisses = bModelTables ? KnownModelWardMisses() : KnownHeldWardMisses();
	const TCHAR* MissesName = bModelTables ? TEXT("KnownModelMisses") : TEXT("KnownHeldMisses");
	const TCHAR* WardMissesName = bModelTables ? TEXT("KnownModelWardMisses") : TEXT("KnownHeldWardMisses");
	FRig Rig;
	if (!Rig.Open(Test))
	{
		Rig.Close();
		return false;
	}
	FAbsorbResolver Resolver;
	FString ResolverError;
	if (Kind == EKind::Ward && !Resolver.Init(ResolverError))
	{
		Test.AddError(ResolverError);
		Rig.Close();
		return false;
	}

	bool bPass = true;
	int32 Matches = 0;
	int32 Cases = 0;
	for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
	{
		FHandClip Source;
		if (!LoadActionClip(Test, Action, VariantIndex, Source))
		{
			bPass = false;
			continue;
		}
		if (!Rig.Play(Source))
		{
			Test.AddError(FString::Printf(TEXT("%s.%s at 72 Hz did not play"), Action, VariantNames[VariantIndex]));
			bPass = false;
			continue;
		}
		const FString Reference = Relevant(Rig, Kind);
		const int32 ReferenceWards = Rig.WardRaised;
		const double ReferenceOnset = Rig.FirstOnset;
		const FWardEvent ReferenceRaise = Rig.Wards->GetLastRaise();
		const TArray<double> ReferenceBlinkTimes = Rig.BlinkTimes;
		for (int32 PhaseIndex = 0; PhaseIndex < GPhaseCount; ++PhaseIndex)
		{
			const FString Key = bModelTables
				? FString::Printf(TEXT("%s.%s"), Stream.Name, *CaseKey(Action, VariantIndex, PhaseIndex))
				: CaseKey(Action, VariantIndex, PhaseIndex);
			const FHandClip Held = MakeStream(Source, Stream, GPhases[PhaseIndex]);
			if (!Rig.Play(Held))
			{
				Test.AddError(Key + TEXT(" held did not play"));
				bPass = false;
				continue;
			}
			const FString HeldResult = Relevant(Rig, Kind);
			const bool bMatch = HeldResult == Reference;
			FString Extra;
			if (Kind == EKind::Blink)
			{
				// How late each blink fires against 72 Hz, when the counts agree.
				for (int32 Index = 0; Index < Rig.BlinkTimes.Num() && Index < ReferenceBlinkTimes.Num(); ++Index)
				{
					Extra += FString::Printf(TEXT(" blink%dDeltaMs=%.3f"), Index, (Rig.BlinkTimes[Index] - ReferenceBlinkTimes[Index]) * 1000.0);
				}
			}
			bool bWardHolds = false;
			if (Kind == EKind::Ward)
			{
				if (OutWard)
				{
					++OutWard->Cases;
				}
				if (ReferenceWards < 1)
				{
					Test.AddError(Key + TEXT(" raised no ward at 72 Hz: the reference is broken"));
					bPass = false;
				}
				if (ReferenceWards >= 1 && Rig.WardRaised >= 1)
				{
					const double DeltaMs = (Rig.FirstOnset - ReferenceOnset) * 1000.0;
					// The perfect window of Ward.OnsetCompensation: a hit at the 72 Hz onset and one at its window end.
					// These two boundary probes are information only; the interior probes below are asserted.
					const double Window = Resolver.Rules().WindowS;
					auto Perfect = [&Resolver](const FWardEvent& Raise, double Tick)
					{
						FWardState State;
						State.bRaised = true;
						State.bFresh = Raise.bFresh;
						State.OnsetTime = Raise.OnsetTime;
						State.Facing = Raise.Facing;
						return Resolver.Resolve(Tick, EAbsorbHitKind::Magic, 3, State.Facing, State).bPerfect;
					};
					const FWardEvent HeldRaise = Rig.Wards->GetLastRaise();
					const bool bStart72 = Perfect(ReferenceRaise, ReferenceOnset);
					const bool bEnd72 = Perfect(ReferenceRaise, ReferenceOnset + Window);
					const bool bStartHeld = Perfect(HeldRaise, ReferenceOnset);
					const bool bEndHeld = Perfect(HeldRaise, ReferenceOnset + Window);
					Extra = FString::Printf(TEXT(" onset72=%.4f onsetHeld=%.4f onsetDeltaMs=%.3f perfect(start,end) 72Hz=%d,%d held=%d,%d perfectChanged=%d"),
						ReferenceOnset, Rig.FirstOnset, DeltaMs, bStart72, bEnd72, bStartHeld, bEndHeld, (bStart72 != bStartHeld || bEnd72 != bEndHeld) ? 1 : 0);
					bool bInteriorHeld = true;
					for (const double Probe : GInteriorProbesS)
					{
						const bool bProbe72 = Perfect(ReferenceRaise, ReferenceOnset + Probe);
						const bool bProbeHeld = Perfect(HeldRaise, ReferenceOnset + Probe);
						bInteriorHeld &= bProbeHeld;
						Extra += FString::Printf(TEXT(" perfect(+%.2f) 72Hz=%d held=%d"), Probe, bProbe72 ? 1 : 0, bProbeHeld ? 1 : 0);
						if (!bProbe72)
						{
							Test.AddError(FString::Printf(TEXT("%s: a hit at the 72 Hz onset + %.2f s is not perfect at 72 Hz: the reference is broken"), *Key, Probe));
							bPass = false;
						}
					}
					const bool bWithin = FMath::Abs(DeltaMs) <= BoundMs;
					bWardHolds = bWithin && bInteriorHeld;
					Extra += FString::Printf(TEXT(" onsetWithin%.1fms=%d interiorHeld=%d"), BoundMs, bWithin ? 1 : 0, bInteriorHeld ? 1 : 0);
					if (OutWard)
					{
						OutWard->OnsetDeltasMs.Add(DeltaMs);
						OutWard->WithinBound += bWithin ? 1 : 0;
						OutWard->InteriorHeld += bInteriorHeld ? 1 : 0;
					}
				}
				const bool bKnownWard = WardMisses.Contains(Key);
				Extra += FString::Printf(TEXT(" wardHolds=%d knownWard=%d"), bWardHolds ? 1 : 0, bKnownWard ? 1 : 0);
				if (!bWardHolds && !bKnownWard)
				{
					Test.AddError(FString::Printf(TEXT("%s onset is not within %.1f ms of the 72 Hz onset, or an interior hit is not perfect, and it is not in %s"),
						*Key, BoundMs, WardMissesName));
					bPass = false;
				}
				if (bWardHolds && bKnownWard)
				{
					Test.AddError(FString::Printf(TEXT("%s is in %s but now holds: remove it"), *Key, WardMissesName));
					bPass = false;
				}
			}
			const bool bKnown = Misses.Contains(Key);
			const FString Line = FString::Printf(TEXT("HeldSample case %s 72Hz=[%s] held=[%s] match=%d known=%d%s"),
				*Key, *Reference, *HeldResult, bMatch ? 1 : 0, bKnown ? 1 : 0, *Extra);
			UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
			Test.AddInfo(Line);
			++Cases;
			Matches += bMatch ? 1 : 0;
			if (!bMatch && !bKnown)
			{
				Test.AddError(FString::Printf(TEXT("%s misses its 72 Hz result and is not in %s"), *Key, MissesName));
				bPass = false;
			}
			if (bMatch && bKnown)
			{
				Test.AddError(FString::Printf(TEXT("%s is in %s but now matches: remove it"), *Key, MissesName));
				bPass = false;
			}
		}
	}
	const FString Summary = bModelTables
		? FString::Printf(TEXT("HeldSample %s %s matches %d/%d"), Stream.Name, Action, Matches, Cases)
		: FString::Printf(TEXT("HeldSample %s matches %d/%d"), Action, Matches, Cases);
	UE_LOG(LogMageArena, Log, TEXT("%s"), *Summary);
	Test.AddInfo(Summary);
	Rig.Close();
	return bPass;
}

struct FCorpusFigures
{
	int32 Clips = 0;
	int32 Correct = 0;
	int32 Impostors = 0;
	int32 ImpostorCasts = 0;
	int32 Noise = 0;
	int32 NoiseCasts = 0;

	int32 AccuracyPercentFloor() const
	{
		return Clips > 0 ? FMath::FloorToInt(100.0 * static_cast<double>(Correct) / static_cast<double>(Clips)) : 0;
	}
	int32 FalseCasts() const { return ImpostorCasts + NoiseCasts; }
};

void CollectJsonl(const TCHAR* Sub, TArray<FString>& Out)
{
	IFileManager::Get().FindFilesRecursive(Out, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), Sub), TEXT("*.jsonl"), true, false);
	Out.Sort();
}

/** The corpus, impostors and noise through the pipeline, at 72 Hz (no stream) or on a stream at phase 0. */
bool RunCorpus(FAutomationTestBase& Test, const FStream* Stream, FCorpusFigures& Out)
{
	FRig Rig;
	if (!Rig.Open(Test))
	{
		Rig.Close();
		return false;
	}
	bool bPass = true;
	auto Run = [&](const TCHAR* Sub, auto&& Consume)
	{
		TArray<FString> Files;
		CollectJsonl(Sub, Files);
		for (const FString& Path : Files)
		{
			FHandClip Clip;
			FString Error;
			if (!FHandClip::LoadFromFile(Path, Clip, Error))
			{
				Test.AddError(FString::Printf(TEXT("%s: %s"), *Path, *Error));
				bPass = false;
				continue;
			}
			const FHandClip Played = Stream ? MakeStream(Clip, *Stream, GPhases[0]) : Clip;
			if (!Rig.Play(Played))
			{
				Test.AddError(FString::Printf(TEXT("%s did not play"), *Path));
				bPass = false;
				continue;
			}
			Consume(Clip);
		}
	};
	const TCHAR* StreamName = Stream ? Stream->Name : TEXT("72hz");
	// Every wrong corpus clip and every false cast is logged with its file, so a finding names its clips.
	auto Note = [&](const TCHAR* What, const FHandClip& Clip)
	{
		FString Lines;
		for (const FName Line : Rig.Casts)
		{
			Lines += Line.ToString() + TEXT(" ");
		}
		UE_LOG(LogMageArena, Log, TEXT("HeldSample corpus %s %s %s casts=[%s] rejects=%d"), StreamName, What, *Clip.Name, *Lines.TrimEnd(), Rig.Rejects);
	};
	Run(TEXT("corpus"), [&](const FHandClip& Clip)
	{
		++Out.Clips;
		const FString Expected = Clip.Action.Right(5);
		const bool bCorrect = Rig.Casts.Num() >= 1 && Rig.Casts[0].ToString() == Expected;
		Out.Correct += bCorrect ? 1 : 0;
		if (!bCorrect)
		{
			Note(TEXT("wrong"), Clip);
		}
	});
	Run(TEXT("impostors"), [&](const FHandClip& Clip)
	{
		++Out.Impostors;
		Out.ImpostorCasts += Rig.Casts.Num();
		if (Rig.Casts.Num() > 0)
		{
			Note(TEXT("impostor-cast"), Clip);
		}
	});
	Run(TEXT("noise"), [&](const FHandClip& Clip)
	{
		++Out.Noise;
		Out.NoiseCasts += Rig.Casts.Num();
		if (Rig.Casts.Num() > 0)
		{
			Note(TEXT("noise-cast"), Clip);
		}
	});
	Rig.Close();
	return bPass;
}

/**
 * RATCHET for D-G3 under injected latency on modelled streams (G1), with the rules of KnownHeldMisses. Asserted for latency
 * <= 60 ms only. Key: "<stream>.ward-raise.<variant>.p<phase index>.l<latency ms>"; the 72 Hz reference stream is "72hz" and
 * has phase 0 only.
 */
const TArray<FString>& KnownLatencyMisses()
{
	// Each case below misses at 0, 20, 40 and 60 ms alike: the onset error does not grow with latency, so onset
	// compensation holds. The miss is the camera grid. The detector can stamp the onset no earlier than the first frame
	// that shows the motion, and on a 30 Hz stream that frame comes 13.9 to 41.7 ms after the 72 Hz onset, by phase. Over
	// 13.9 ms (one 72 Hz frame) the go predicate fails, though the hit at the authored tick and both interior probes stay
	// perfect: the window moves late, it is not lost. The extrapolated model misses the same cases by the same amount,
	// because a raise from rest gives nothing to extrapolate until the first moving camera sample. What would fix it:
	// W2, an onset stamped earlier by the camera lag (needs the camera rate, G2), or a detector that stamps the onset at
	// the last still sample. Not W1: a wider window does not move the onset.
	static const TArray<FString> Misses = []()
	{
		const TCHAR* Cases[] = {
			TEXT("held30.ward-raise.normal.p1"), TEXT("held30.ward-raise.normal.p2"),
			TEXT("held30.ward-raise.slow.p0"), TEXT("held30.ward-raise.slow.p1"),
			TEXT("held30.ward-raise.sloppy.p0"), TEXT("held30.ward-raise.sloppy.p1"),
			TEXT("extrap30.ward-raise.normal.p1"), TEXT("extrap30.ward-raise.normal.p2"),
			TEXT("extrap30.ward-raise.slow.p0"), TEXT("extrap30.ward-raise.slow.p1"),
			TEXT("extrap30.ward-raise.sloppy.p0"), TEXT("extrap30.ward-raise.sloppy.p1")};
		TArray<FString> Keys;
		for (const TCHAR* Case : Cases)
		{
			for (const int32 LatencyMs : {0, 20, 40, 60})
			{
				Keys.Add(FString::Printf(TEXT("%s.l%d"), Case, LatencyMs));
			}
		}
		return Keys;
	}();
	return Misses;
}

constexpr double GLatenciesS[] = {0.0, 0.020, 0.040, 0.060, 0.080, 0.100};
/** The D-G3 gate: the perfect event at the authored tick +-1 frame for injected latency of 60 ms or less. */
constexpr double GLatencyGateS = 0.060;

/** The first frame whose palm moves at the ward onset speed: the authored onset, as in Ward.OnsetCompensation. */
double PalmSpeedOnsetS(const FHandClipTrack& Track)
{
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	for (int32 Index = 1; Index < Track.Frames.Num(); ++Index)
	{
		const FHandFrame& Prev = Track.Frames[Index - 1];
		const FHandFrame& Frame = Track.Frames[Index];
		const double Dt = Frame.TimeSeconds - Prev.TimeSeconds;
		if (!Frame.Joints.IsValidIndex(Palm) || !Prev.Joints.IsValidIndex(Palm) || Dt < 1.0e-6)
		{
			continue;
		}
		const double Speed = FVector::Distance(Frame.Joints[Palm].Location, Prev.Joints[Palm].Location) / MageClipMetresToCentimetres / Dt;
		if (Speed >= FWardThresholds::OnsetSpeedMps)
		{
			return Frame.TimeSeconds;
		}
	}
	return -1.0;
}

const FHandClipTrack* WardTrack(const FHandClip& Clip)
{
	for (const FHandClipTrack& Track : Clip.Tracks)
	{
		if (Track.Hand == FMageSettings::WardHand() && Track.Frames.Num() > 1)
		{
			return &Track;
		}
	}
	return nullptr;
}

/**
 * The latency model of Ward.OnsetCompensation as a clip: 72 Hz output frames up to EndS, and after Anchor the source time
 * runs at Scale, so the pose that confirms the raise arrives the injected latency late while the onset is unchanged.
 */
FHandClip MakeLatencyClip(const FHandClip& Source, double Anchor, double Scale, double EndS)
{
	FHandClip Out = Source;
	const double Frame = 1.0 / 72.0;
	const int32 Count = FMath::CeilToInt(EndS / Frame) + 1;
	for (FHandClipTrack& Track : Out.Tracks)
	{
		Track.Frames.Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Output = Index * Frame;
			const double SourceTime = FMath::Clamp(Output > Anchor ? Anchor + (Output - Anchor) * Scale : Output, 0.0, Source.GetDuration());
			FHandFrame Sampled;
			if (Source.Sample(Track.Hand, SourceTime, Sampled))
			{
				Sampled.TimeSeconds = Output;
				Sampled.Hand = Track.Hand;
				Track.Frames.Add(Sampled);
			}
		}
	}
	Out.Duration = (Count - 1) * Frame;
	return Out;
}

/** Feeds the ward-hand track to a fresh detector until it raises. Returns false when it never raises. */
bool RaiseOn(const FHandClip& Clip, FWardDetector& Detector, double& OutConfirmS)
{
	OutConfirmS = -1.0;
	const FHandClipTrack* Track = WardTrack(Clip);
	if (Track == nullptr)
	{
		return false;
	}
	for (const FHandFrame& Frame : Track->Frames)
	{
		Detector.Ingest(Frame);
		if (Detector.GetState().bRaised)
		{
			OutConfirmS = Frame.TimeSeconds;
			return true;
		}
	}
	return false;
}
}

using namespace HeldSampleTestsImpl;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleTransformIsReal, "MageArena.Hands.HeldSample.TransformIsReal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleTransformIsReal::RunTest(const FString& Parameters)
{
	const TCHAR* Actions[] = {TEXT("ward-raise"), TEXT("blink-left"), TEXT("blink-right"), TEXT("blink-back"),
		TEXT("sigil-line1"), TEXT("sigil-line2"), TEXT("sigil-line3")};
	bool bPass = true;
	for (const TCHAR* Action : Actions)
	{
		for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
		{
			FHandClip Source;
			if (!LoadActionClip(*this, Action, VariantIndex, Source))
			{
				bPass = false;
				continue;
			}
			const int32 Bound = FMath::CeilToInt(Source.GetDuration() * GBoundHz) + 1;
			for (int32 PhaseIndex = 0; PhaseIndex < GPhaseCount; ++PhaseIndex)
			{
				const FHandClip Held = MakeHeldClip(Source, GCameraHz, GPhases[PhaseIndex]);
				bPass &= TestEqual(*FString::Printf(TEXT("%s hz stays 72"), *CaseKey(Action, VariantIndex, PhaseIndex)), Held.Hz, 72.0);
				bPass &= TestEqual(TEXT("track count"), Held.Tracks.Num(), Source.Tracks.Num());
				for (int32 TrackIndex = 0; TrackIndex < Source.Tracks.Num(); ++TrackIndex)
				{
					const int32 Original = DistinctPoses(Source.Tracks[TrackIndex]);
					const int32 Distinct = DistinctPoses(Held.Tracks[TrackIndex]);
					bPass &= TestEqual(TEXT("frame count unchanged"), Held.Tracks[TrackIndex].Frames.Num(), Source.Tracks[TrackIndex].Frames.Num());
					bPass &= TestTrue(*FString::Printf(TEXT("%s track %d held has %d distinct poses, at most %d"),
						*CaseKey(Action, VariantIndex, PhaseIndex), TrackIndex, Distinct, Bound), Distinct <= Bound);
					bPass &= TestTrue(*FString::Printf(TEXT("%s track %d source has %d distinct poses, more than held %d"),
						*CaseKey(Action, VariantIndex, PhaseIndex), TrackIndex, Original, Distinct), Original > Distinct);
				}
			}
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleModelsAreReal, "MageArena.Hands.HeldSample.ModelsAreReal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleModelsAreReal::RunTest(const FString& Parameters)
{
	// G1: each new stream must differ from its source. The 25 Hz hold keeps the pose-count bound of TransformIsReal at its
	// own rate. An extrapolated stream must differ from its source and from the held stream of the same rate and phase, so
	// neither the identity nor a zero extrapolation fraction (which is the hold) passes.
	const TCHAR* Actions[] = {TEXT("ward-raise"), TEXT("blink-left"), TEXT("blink-right"), TEXT("blink-back"),
		TEXT("sigil-line1"), TEXT("sigil-line2"), TEXT("sigil-line3"), TEXT("staff-lift"), TEXT("staff-plant")};
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	bool bPass = true;
	double HeldErrorCm[2] = {0.0, 0.0};
	double ExtrapolatedErrorCm[2] = {0.0, 0.0};
	int32 ErrorFrames[2] = {0, 0};
	for (const TCHAR* Action : Actions)
	{
		for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
		{
			FHandClip Source;
			if (!LoadActionClip(*this, Action, VariantIndex, Source))
			{
				bPass = false;
				continue;
			}
			const int32 LowBound = FMath::CeilToInt(Source.GetDuration() * GLowBoundHz) + 1;
			for (int32 PhaseIndex = 0; PhaseIndex < GPhaseCount; ++PhaseIndex)
			{
				const FString Key = CaseKey(Action, VariantIndex, PhaseIndex);
				const FHandClip Held25 = MakeStream(Source, GHeld25, GPhases[PhaseIndex]);
				bPass &= TestEqual(*FString::Printf(TEXT("held25 %s hz stays 72"), *Key), Held25.Hz, 72.0);
				bPass &= TestEqual(TEXT("held25 track count"), Held25.Tracks.Num(), Source.Tracks.Num());
				for (int32 TrackIndex = 0; TrackIndex < Source.Tracks.Num() && TrackIndex < Held25.Tracks.Num(); ++TrackIndex)
				{
					const int32 Original = DistinctPoses(Source.Tracks[TrackIndex]);
					const int32 Distinct = DistinctPoses(Held25.Tracks[TrackIndex]);
					bPass &= TestEqual(TEXT("held25 frame count unchanged"), Held25.Tracks[TrackIndex].Frames.Num(), Source.Tracks[TrackIndex].Frames.Num());
					bPass &= TestTrue(*FString::Printf(TEXT("held25 %s track %d has %d distinct poses, at most %d"), *Key, TrackIndex, Distinct, LowBound),
						Distinct <= LowBound);
					bPass &= TestTrue(*FString::Printf(TEXT("held25 %s track %d source has %d distinct poses, more than held %d"), *Key, TrackIndex, Original, Distinct),
						Original > Distinct);
				}
				const FStream* Pairs[2][2] = {{&GExtrapolated30, &GHeld30}, {&GExtrapolated25, &GHeld25}};
				for (int32 RateIndex = 0; RateIndex < 2; ++RateIndex)
				{
					const FStream& Model = *Pairs[RateIndex][0];
					const FHandClip Extrapolated = MakeStream(Source, Model, GPhases[PhaseIndex]);
					const FHandClip Held = MakeStream(Source, *Pairs[RateIndex][1], GPhases[PhaseIndex]);
					bPass &= TestEqual(*FString::Printf(TEXT("%s %s hz stays 72"), Model.Name, *Key), Extrapolated.Hz, 72.0);
					bPass &= TestEqual(TEXT("extrapolated track count"), Extrapolated.Tracks.Num(), Source.Tracks.Num());
					for (int32 TrackIndex = 0; TrackIndex < Source.Tracks.Num() && TrackIndex < Extrapolated.Tracks.Num(); ++TrackIndex)
					{
						const TArray<FHandFrame>& From = Source.Tracks[TrackIndex].Frames;
						const TArray<FHandFrame>& Out = Extrapolated.Tracks[TrackIndex].Frames;
						const TArray<FHandFrame>& Hold = Held.Tracks[TrackIndex].Frames;
						if (!TestEqual(TEXT("extrapolated frame count unchanged"), Out.Num(), From.Num()) || Hold.Num() != From.Num())
						{
							bPass = false;
							continue;
						}
						int32 OffSource = 0;
						int32 OffHeld = 0;
						int32 Retimed = 0;
						for (int32 Index = 0; Index < From.Num(); ++Index)
						{
							OffSource += SamePose(Out[Index], From[Index]) ? 0 : 1;
							OffHeld += SamePose(Out[Index], Hold[Index]) ? 0 : 1;
							Retimed += Out[Index].TimeSeconds == From[Index].TimeSeconds ? 0 : 1;
							if (From[Index].Joints.IsValidIndex(Palm) && Out[Index].Joints.IsValidIndex(Palm) && Hold[Index].Joints.IsValidIndex(Palm))
							{
								HeldErrorCm[RateIndex] += FVector::Distance(Hold[Index].Joints[Palm].Location, From[Index].Joints[Palm].Location);
								ExtrapolatedErrorCm[RateIndex] += FVector::Distance(Out[Index].Joints[Palm].Location, From[Index].Joints[Palm].Location);
								++ErrorFrames[RateIndex];
							}
						}
						bPass &= TestEqual(TEXT("extrapolated frames keep their time"), Retimed, 0);
						bPass &= TestTrue(*FString::Printf(TEXT("%s %s track %d differs from its source in %d frames"), Model.Name, *Key, TrackIndex, OffSource), OffSource > 0);
						bPass &= TestTrue(*FString::Printf(TEXT("%s %s track %d differs from the held stream in %d frames"), Model.Name, *Key, TrackIndex, OffHeld), OffHeld > 0);
					}
				}
			}
		}
	}
	for (int32 RateIndex = 0; RateIndex < 2; ++RateIndex)
	{
		const double Frames = FMath::Max(1, ErrorFrames[RateIndex]);
		const FString Line = FString::Printf(TEXT("HeldSample %d Hz mean palm distance to the source: held %.3f cm, extrapolated %.3f cm over %d frames"),
			RateIndex == 0 ? 30 : 25, HeldErrorCm[RateIndex] / Frames, ExtrapolatedErrorCm[RateIndex] / Frames, ErrorFrames[RateIndex]);
		UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
		AddInfo(Line);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleWard, "MageArena.Hands.HeldSample.Ward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleWard::RunTest(const FString& Parameters)
{
	FWardFigures Figures;
	const bool bPass = RunRatchet(*this, TEXT("ward-raise"), EKind::Ward, GHeld30, &Figures);
	TArray<double>& Deltas = Figures.OnsetDeltasMs;
	if (Deltas.Num() > 0)
	{
		Deltas.Sort();
		const FString Line = FString::Printf(TEXT("HeldSample ward onset delta ms min=%.3f median=%.3f max=%.3f n=%d"),
			Deltas[0], Deltas[Deltas.Num() / 2], Deltas.Last(), Deltas.Num());
		UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
		AddInfo(Line);
	}
	const FString Bound = FString::Printf(TEXT("HeldSample ward onset within %.1f ms %d/%d, interior perfect (+0.05, +0.10) held %d/%d, known ward misses %d"),
		GOnsetBoundMs, Figures.WithinBound, Figures.Cases, Figures.InteriorHeld, Figures.Cases, KnownHeldWardMisses().Num());
	UE_LOG(LogMageArena, Log, TEXT("%s"), *Bound);
	AddInfo(Bound);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleBlink, "MageArena.Hands.HeldSample.Blink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleBlink::RunTest(const FString& Parameters)
{
	bool bPass = true;
	for (const TCHAR* Action : {TEXT("blink-left"), TEXT("blink-right"), TEXT("blink-back")})
	{
		bPass &= RunRatchet(*this, Action, EKind::Blink, GHeld30, nullptr);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleSigilLines, "MageArena.Hands.HeldSample.SigilLines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleSigilLines::RunTest(const FString& Parameters)
{
	bool bPass = true;
	for (const TCHAR* Action : {TEXT("sigil-line1"), TEXT("sigil-line2"), TEXT("sigil-line3")})
	{
		bPass &= RunRatchet(*this, Action, EKind::Sigil, GHeld30, nullptr);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleSigilCorpus, "MageArena.Hands.HeldSample.SigilCorpus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleSigilCorpus::RunTest(const FString& Parameters)
{
	// Floors set to what the first run measured (phase 0). Accuracy is rounded down to a whole percent.
	constexpr int32 HeldAccuracyFloorPercent = 94;
	constexpr int32 HeldFalseCastCeiling = 0;

	FCorpusFigures Normal;
	FCorpusFigures Held;
	bool bPass = RunCorpus(*this, nullptr, Normal);
	bPass &= RunCorpus(*this, &GHeld30, Held);
	const FString Line72 = FString::Printf(TEXT("HeldSample corpus 72Hz accuracy %d/%d (%d%% floor), impostor casts %d/%d, noise casts %d/%d"),
		Normal.Correct, Normal.Clips, Normal.AccuracyPercentFloor(), Normal.ImpostorCasts, Normal.Impostors, Normal.NoiseCasts, Normal.Noise);
	const FString LineHeld = FString::Printf(TEXT("HeldSample corpus held accuracy %d/%d (%d%% floor), impostor casts %d/%d, noise casts %d/%d"),
		Held.Correct, Held.Clips, Held.AccuracyPercentFloor(), Held.ImpostorCasts, Held.Impostors, Held.NoiseCasts, Held.Noise);
	UE_LOG(LogMageArena, Log, TEXT("%s"), *Line72);
	UE_LOG(LogMageArena, Log, TEXT("%s"), *LineHeld);
	AddInfo(Line72);
	AddInfo(LineHeld);
	bPass &= TestEqual(TEXT("corpus clips"), Held.Clips, 270);
	bPass &= TestEqual(TEXT("impostor clips"), Held.Impostors, 180);
	bPass &= TestTrue(*FString::Printf(TEXT("held accuracy %d%% at or above %d%%"), Held.AccuracyPercentFloor(), HeldAccuracyFloorPercent),
		Held.AccuracyPercentFloor() >= HeldAccuracyFloorPercent);
	bPass &= TestTrue(*FString::Printf(TEXT("held false casts %d at or below %d"), Held.FalseCasts(), HeldFalseCastCeiling),
		Held.FalseCasts() <= HeldFalseCastCeiling);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleLatency, "MageArena.Hands.HeldSample.Latency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleLatency::RunTest(const FString& Parameters)
{
	// D-G3 (docs/PROJECT-PLAN.md:204) on modelled streams: the latency model of Ward.OnsetCompensation applied to the three
	// ward-raise variants, then played through the held and the extrapolated 30 Hz streams at every phase. The go predicate
	// is the one Ward.OnsetCompensation asserts: the onset within one 72 Hz frame of the authored onset, and a hit at the
	// authored tick (authored onset + window) perfect, or the frame before it when the onset is early. The 72 Hz stream is
	// the reference. Also logged: the literal "perfect at the tick +-1 frame", and the interior probes with the stream bound.
	struct FLatencyStream
	{
		const TCHAR* Name;
		const FStream* Stream;
	};
	const FLatencyStream Streams[] = {{TEXT("72hz"), nullptr}, {GHeld30.Name, &GHeld30}, {GExtrapolated30.Name, &GExtrapolated30}};
	constexpr int32 StreamCount = UE_ARRAY_COUNT(Streams);

	const FMageSettingsState Saved = FMageSettings::Get();
	FMageSettings::Restore(FMageSettingsState());
	FAbsorbResolver Resolver;
	FString Error;
	if (!Resolver.Init(Error))
	{
		AddError(Error);
		FMageSettings::Restore(Saved);
		return false;
	}
	const double Frame = 1.0 / 72.0;
	const double Window = Resolver.Rules().WindowS;
	bool bPass = true;
	TArray<double> GateErrorsMs[StreamCount];
	int32 GateCases[StreamCount] = {};
	int32 GateGo[StreamCount] = {};
	int32 GateInterior[StreamCount] = {};
	for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
	{
		FHandClip Source;
		if (!LoadActionClip(*this, TEXT("ward-raise"), VariantIndex, Source))
		{
			bPass = false;
			continue;
		}
		const FHandClipTrack* Track = WardTrack(Source);
		const double GeoOnset = Track ? PalmSpeedOnsetS(*Track) : -1.0;
		FWardDetector Clean;
		double ConfirmStamp = -1.0;
		const bool bClean = Clean.Init(Error) && RaiseOn(Source, Clean, ConfirmStamp);
		const double Anchor = GeoOnset + 2.0 * Frame;
		if (GeoOnset < 0.0 || !bClean || !(ConfirmStamp > Anchor))
		{
			AddError(FString::Printf(TEXT("ward-raise.%s has no usable 72 Hz onset %.4f and confirm %.4f: the reference is broken"),
				VariantNames[VariantIndex], GeoOnset, ConfirmStamp));
			bPass = false;
			continue;
		}
		const double AuthoredTick = GeoOnset + Window;
		for (const double Latency : GLatenciesS)
		{
			const double Scale = (ConfirmStamp - Anchor) / (ConfirmStamp - Anchor + Latency);
			const FHandClip Delayed = MakeLatencyClip(Source, Anchor, Scale, ConfirmStamp + Latency + 0.5);
			const bool bGate = Latency <= GLatencyGateS + 1.0e-9;
			const int32 LatencyMs = FMath::RoundToInt(Latency * 1000.0);
			for (int32 StreamIndex = 0; StreamIndex < StreamCount; ++StreamIndex)
			{
				const FLatencyStream& Entry = Streams[StreamIndex];
				const int32 Phases = Entry.Stream ? GPhaseCount : 1;
				const double BoundMs = Entry.Stream ? OnsetBoundMs(Entry.Stream->CameraHz) : Frame * 1000.0;
				for (int32 PhaseIndex = 0; PhaseIndex < Phases; ++PhaseIndex)
				{
					const FHandClip Played = Entry.Stream ? MakeStream(Delayed, *Entry.Stream, GPhases[PhaseIndex]) : Delayed;
					FWardDetector Trial;
					double Confirm = -1.0;
					const bool bRaised = Trial.Init(Error) && RaiseOn(Played, Trial, Confirm);
					const FWardState State = Trial.GetState();
					auto Perfect = [&](double Tick)
					{
						return bRaised && Resolver.Resolve(Tick, EAbsorbHitKind::Magic, 3, State.Facing, State).bPerfect;
					};
					const double ErrorS = bRaised ? Trial.GetLastRaise().OnsetTime - GeoOnset : 0.0;
					const bool bWithinFrame = bRaised && FMath::Abs(ErrorS) <= Frame + 1.0e-6;
					const bool bAtTick = Perfect(AuthoredTick);
					const bool bBefore = Perfect(AuthoredTick - Frame);
					const bool bAfter = Perfect(AuthoredTick + Frame);
					const bool bEarly = ErrorS < -1.0e-9;
					const bool bGo = bWithinFrame && (bAtTick || (bEarly && bBefore));
					const bool bInterior = Perfect(GeoOnset + GInteriorProbesS[0]) && Perfect(GeoOnset + GInteriorProbesS[1]);
					const bool bWithinBound = bRaised && FMath::Abs(ErrorS) * 1000.0 <= BoundMs + 1.0e-6;
					const FString Key = FString::Printf(TEXT("%s.ward-raise.%s.p%d.l%d"), Entry.Name, VariantNames[VariantIndex], PhaseIndex, LatencyMs);
					const bool bKnown = KnownLatencyMisses().Contains(Key);
					const FString Line = FString::Printf(TEXT("HeldSample latency %s raised=%d onsetErrorMs=%.3f confirmShiftMs=%.3f go=%d withinFrame=%d perfect(tick-1,tick,tick+1)=%d,%d,%d interior=%d within%.1fms=%d gate=%d known=%d"),
						*Key, bRaised ? 1 : 0, ErrorS * 1000.0, bRaised ? (Confirm - ConfirmStamp) * 1000.0 : 0.0, bGo ? 1 : 0, bWithinFrame ? 1 : 0,
						bBefore ? 1 : 0, bAtTick ? 1 : 0, bAfter ? 1 : 0, bInterior ? 1 : 0, BoundMs, bWithinBound ? 1 : 0, bGate ? 1 : 0, bKnown ? 1 : 0);
					UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
					AddInfo(Line);
					if (!bGate)
					{
						continue;
					}
					if (!Entry.Stream)
					{
						// The latency transform is real: on the 72 Hz stream the confirm moves by the injected latency, to one
						// frame, as Ward.OnsetCompensation asserts.
						bPass &= TestTrue(*FString::Printf(TEXT("%s confirm tracks the injected latency"), *Key),
							bRaised && FMath::Abs(Confirm - ConfirmStamp - Latency) <= Frame + 1.0e-4);
					}
					++GateCases[StreamIndex];
					GateGo[StreamIndex] += bGo ? 1 : 0;
					GateInterior[StreamIndex] += bInterior ? 1 : 0;
					if (bRaised)
					{
						GateErrorsMs[StreamIndex].Add(ErrorS * 1000.0);
					}
					if (!bGo && !bKnown)
					{
						AddError(FString::Printf(TEXT("%s misses the D-G3 go and is not in KnownLatencyMisses"), *Key));
						bPass = false;
					}
					if (bGo && bKnown)
					{
						AddError(FString::Printf(TEXT("%s is in KnownLatencyMisses but now holds the D-G3 go: remove it"), *Key));
						bPass = false;
					}
				}
			}
		}
	}
	for (int32 StreamIndex = 0; StreamIndex < StreamCount; ++StreamIndex)
	{
		TArray<double>& Errors = GateErrorsMs[StreamIndex];
		Errors.Sort();
		const FString Line = FString::Printf(TEXT("HeldSample latency %s to 60 ms: go %d/%d, interior perfect %d/%d, onset error ms min=%.3f max=%.3f"),
			Streams[StreamIndex].Name, GateGo[StreamIndex], GateCases[StreamIndex], GateInterior[StreamIndex], GateCases[StreamIndex],
			Errors.Num() ? Errors[0] : 0.0, Errors.Num() ? Errors.Last() : 0.0);
		UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
		AddInfo(Line);
	}
	FMageSettings::Restore(Saved);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleWardModels, "MageArena.Hands.HeldSample.WardModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleWardModels::RunTest(const FString& Parameters)
{
	// G1: the ward onset bound (one camera interval plus one 72 Hz frame) and the interior perfect probes of
	// HeldSample.Ward, on every G1 stream.
	bool bPass = true;
	for (const FStream* Stream : GModelStreams)
	{
		FWardFigures Figures;
		bPass &= RunRatchet(*this, TEXT("ward-raise"), EKind::Ward, *Stream, &Figures, true);
		TArray<double>& Deltas = Figures.OnsetDeltasMs;
		Deltas.Sort();
		const FString Line = FString::Printf(TEXT("HeldSample %s ward onset delta ms min=%.3f median=%.3f max=%.3f n=%d, within %.1f ms %d/%d, interior perfect %d/%d"),
			Stream->Name, Deltas.Num() ? Deltas[0] : 0.0, Deltas.Num() ? Deltas[Deltas.Num() / 2] : 0.0, Deltas.Num() ? Deltas.Last() : 0.0, Deltas.Num(),
			OnsetBoundMs(Stream->CameraHz), Figures.WithinBound, Figures.Cases, Figures.InteriorHeld, Figures.Cases);
		UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
		AddInfo(Line);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleBlinkModels, "MageArena.Hands.HeldSample.BlinkModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleBlinkModels::RunTest(const FString& Parameters)
{
	// G1: blink left, right and back on every G1 stream against 72 Hz: a false blink, a bolt or a wrong direction changes
	// the result. Then the re-arm, on held30 too: every clip gets 0.5 s of still tail, and the time the detector lets go of
	// its first flick is compared with 72 Hz. A case misses when 72 Hz re-arms and the stream does not, or re-arms later than
	// the stream's onset bound (one camera interval plus one 72 Hz frame).
	bool bPass = true;
	const TCHAR* Actions[] = {TEXT("blink-left"), TEXT("blink-right"), TEXT("blink-back")};
	for (const TCHAR* Action : Actions)
	{
		for (const FStream* Stream : GModelStreams)
		{
			bPass &= RunRatchet(*this, Action, EKind::Blink, *Stream, nullptr, true);
		}
	}

	const FMageSettingsState Saved = FMageSettings::Get();
	FMageSettings::Restore(FMageSettingsState());
	TArray<const FStream*> Streams = {&GHeld30};
	Streams.Append(GModelStreams, UE_ARRAY_COUNT(GModelStreams));
	for (const FStream* Stream : Streams)
	{
		TArray<double> DelaysMs;
		int32 Cases = 0;
		int32 Holds = 0;
		for (const TCHAR* Action : Actions)
		{
			for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
			{
				FHandClip Source;
				if (!LoadActionClip(*this, Action, VariantIndex, Source))
				{
					bPass = false;
					continue;
				}
				const FHandClip Tailed = MakeStillTailClip(Source, 0.5);
				const FRearm Reference = MeasureRearm(Tailed);
				if (Reference.Fires < 1 || Reference.RearmS < 0.0)
				{
					AddError(FString::Printf(TEXT("%s.%s at 72 Hz did not fire and re-arm: the reference is broken"), Action, VariantNames[VariantIndex]));
					bPass = false;
					continue;
				}
				for (int32 PhaseIndex = 0; PhaseIndex < GPhaseCount; ++PhaseIndex)
				{
					const FString Key = FString::Printf(TEXT("%s.%s.rearm"), Stream->Name, *CaseKey(Action, VariantIndex, PhaseIndex));
					const FRearm Measured = MeasureRearm(MakeStream(Tailed, *Stream, GPhases[PhaseIndex]));
					const bool bRearmed = Measured.Fires >= 1 && Measured.RearmS >= 0.0;
					const double DelayMs = bRearmed ? (Measured.RearmS - Reference.RearmS) * 1000.0 : 0.0;
					const bool bHolds = bRearmed && DelayMs <= OnsetBoundMs(Stream->CameraHz) + 1.0e-6;
					const bool bKnown = KnownModelMisses().Contains(Key);
					const FString Line = FString::Printf(TEXT("HeldSample rearm %s fire72=%.4f rearm72=%.4f fire=%.4f rearm=%.4f rearmDelayMs=%.3f holds=%d known=%d"),
						*Key, Reference.FireFrameS, Reference.RearmS, Measured.FireFrameS, Measured.RearmS, DelayMs, bHolds ? 1 : 0, bKnown ? 1 : 0);
					UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
					AddInfo(Line);
					++Cases;
					Holds += bHolds ? 1 : 0;
					if (bRearmed)
					{
						DelaysMs.Add(DelayMs);
					}
					if (!bHolds && !bKnown)
					{
						AddError(FString::Printf(TEXT("%s re-arms late or not at all and is not in KnownModelMisses"), *Key));
						bPass = false;
					}
					if (bHolds && bKnown)
					{
						AddError(FString::Printf(TEXT("%s is in KnownModelMisses but now re-arms in time: remove it"), *Key));
						bPass = false;
					}
				}
			}
		}
		DelaysMs.Sort();
		const FString Summary = FString::Printf(TEXT("HeldSample rearm %s within %.1f ms %d/%d, delay ms min=%.3f max=%.3f"), Stream->Name,
			OnsetBoundMs(Stream->CameraHz), Holds, Cases, DelaysMs.Num() ? DelaysMs[0] : 0.0, DelaysMs.Num() ? DelaysMs.Last() : 0.0);
		UE_LOG(LogMageArena, Log, TEXT("%s"), *Summary);
		AddInfo(Summary);
	}
	FMageSettings::Restore(Saved);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleCorpusModels, "MageArena.Hands.HeldSample.CorpusModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleCorpusModels::RunTest(const FString& Parameters)
{
	// G1: the sigil corpus, impostors and noise on every G1 stream at phase 0. Two-sided: the correct count and the false
	// casts must equal what was measured, so a loss fails and a gain fails until this table is updated.
	struct FExpected
	{
		const TCHAR* Stream;
		int32 Correct;
		int32 FalseCasts;
	};
	// extrap30: 9 false casts, every one an impostor-zigzag (a small three-leg mark inside the circle) cast as a line. Pinch
	// is held in this model, so the joint extrapolation alone lets them pass the reject. Fix: a recognizer change (the reject on short jagged
	// inner strokes), a separate run; W1 and W2 do not touch sigils.
	// held25: 258/270 and no false cast, one more correct than 72 Hz. extrap25: 237/270 (87%, under the 90% sloppy floor
	// of D-G2) and 30 false casts, all 30 impostor-zigzag clips; sigil-line2 loses most (22 of 33 wrong). The longer
	// period lengthens the overshoot. The same recognizer fix applies.
	const FExpected Expected[] = {{TEXT("extrap30"), 253, 9}, {TEXT("held25"), 258, 0}, {TEXT("extrap25"), 237, 30}};
	bool bPass = true;
	for (const FStream* Stream : GModelStreams)
	{
		FCorpusFigures Figures;
		bPass &= RunCorpus(*this, Stream, Figures);
		const FString Line = FString::Printf(TEXT("HeldSample corpus %s accuracy %d/%d (%d%% floor), impostor casts %d/%d, noise casts %d/%d"),
			Stream->Name, Figures.Correct, Figures.Clips, Figures.AccuracyPercentFloor(), Figures.ImpostorCasts, Figures.Impostors, Figures.NoiseCasts, Figures.Noise);
		UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
		AddInfo(Line);
		bPass &= TestEqual(*FString::Printf(TEXT("%s corpus clips"), Stream->Name), Figures.Clips, 270);
		bPass &= TestEqual(*FString::Printf(TEXT("%s impostor clips"), Stream->Name), Figures.Impostors, 180);
		const FExpected* Found = nullptr;
		for (const FExpected& Entry : Expected)
		{
			Found = FCString::Strcmp(Entry.Stream, Stream->Name) == 0 ? &Entry : Found;
		}
		if (!Found)
		{
			AddError(FString::Printf(TEXT("%s has no expected corpus figures"), Stream->Name));
			bPass = false;
			continue;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s corpus correct (ratchet)"), Stream->Name), Figures.Correct, Found->Correct);
		bPass &= TestEqual(*FString::Printf(TEXT("%s false casts (ratchet)"), Stream->Name), Figures.FalseCasts(), Found->FalseCasts);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleStaff, "MageArena.Hands.HeldSample.Staff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleStaff::RunTest(const FString& Parameters)
{
	// G1, report only: staff-lift and staff-plant on held and extrapolated 30 Hz against 72 Hz, in KnownModelMisses.
	bool bPass = true;
	for (const TCHAR* Action : {TEXT("staff-lift"), TEXT("staff-plant")})
	{
		for (const FStream* Stream : {&GHeld30, &GExtrapolated30})
		{
			bPass &= RunRatchet(*this, Action, EKind::Staff, *Stream, nullptr, true);
		}
	}
	return bPass;
}

#endif
