#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/AbsorbResolver.h"
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
#include "Misc/Paths.h"

// F7(b) of docs/research/STACK-OPPORTUNITIES-2026-10.md: do the ward, blink and sigil gestures still work when the hands
// update at about 30 Hz? The worst case is modelled in memory: every pose is held for 1/30 s, still emitted at 72 Hz,
// and nothing predicts between updates. No clip file is written. Every test plays the held copy through the real
// pipeline (PlayLoadedClip, the hand source, the detectors) and compares it with the unmodified clip.

namespace HeldSampleTestsImpl
{
/** The modelled camera rate. The mutation proof sets this to 72, which turns the hold off. */
constexpr double GCameraHz = 30.0;
/** The rate the pose-count bound is written for. Not the same constant on purpose: TransformIsReal must notice a change. */
constexpr double GBoundHz = 30.0;
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
	int32 Bolts = 0;
	TArray<FName> Casts;
	int32 Rejects = 0;

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
		Blinks->OnBlink.AddLambda([this](const FBlinkEvent& Event) { BlinkDirections.Add(static_cast<int32>(Event.Direction)); });
		Blinks->OnBolt.AddLambda([this](const FBoltFlickEvent&) { ++Bolts; });
		Sigils->OnSigilCast.AddLambda([this](FName Line, float, double) { Casts.Add(Line); });
		Sigils->OnSigilRejected.AddLambda([this](double) { ++Rejects; });
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
		Bolts = 0;
		Casts.Reset();
		Rejects = 0;
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
	Sigil
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

/** Runs one action over every variant and phase against the ratchet. Returns false on a failure. */
bool RunRatchet(FAutomationTestBase& Test, const TCHAR* Action, EKind Kind, FWardFigures* OutWard)
{
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
		for (int32 PhaseIndex = 0; PhaseIndex < GPhaseCount; ++PhaseIndex)
		{
			const FString Key = CaseKey(Action, VariantIndex, PhaseIndex);
			const FHandClip Held = MakeHeldClip(Source, GCameraHz, GPhases[PhaseIndex]);
			if (!Rig.Play(Held))
			{
				Test.AddError(Key + TEXT(" held did not play"));
				bPass = false;
				continue;
			}
			const FString HeldResult = Relevant(Rig, Kind);
			const bool bMatch = HeldResult == Reference;
			FString Extra;
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
					const bool bWithin = FMath::Abs(DeltaMs) <= GOnsetBoundMs;
					bWardHolds = bWithin && bInteriorHeld;
					Extra += FString::Printf(TEXT(" onsetWithin%.1fms=%d interiorHeld=%d"), GOnsetBoundMs, bWithin ? 1 : 0, bInteriorHeld ? 1 : 0);
					if (OutWard)
					{
						OutWard->OnsetDeltasMs.Add(DeltaMs);
						OutWard->WithinBound += bWithin ? 1 : 0;
						OutWard->InteriorHeld += bInteriorHeld ? 1 : 0;
					}
				}
				const bool bKnownWard = KnownHeldWardMisses().Contains(Key);
				Extra += FString::Printf(TEXT(" wardHolds=%d knownWard=%d"), bWardHolds ? 1 : 0, bKnownWard ? 1 : 0);
				if (!bWardHolds && !bKnownWard)
				{
					Test.AddError(FString::Printf(TEXT("%s held onset is not within %.1f ms of the 72 Hz onset, or an interior hit is not perfect, and it is not in KnownHeldWardMisses"),
						*Key, GOnsetBoundMs));
					bPass = false;
				}
				if (bWardHolds && bKnownWard)
				{
					Test.AddError(FString::Printf(TEXT("%s is in KnownHeldWardMisses but now holds: remove it"), *Key));
					bPass = false;
				}
			}
			const bool bKnown = KnownHeldMisses().Contains(Key);
			const FString Line = FString::Printf(TEXT("HeldSample case %s 72Hz=[%s] held=[%s] match=%d known=%d%s"),
				*Key, *Reference, *HeldResult, bMatch ? 1 : 0, bKnown ? 1 : 0, *Extra);
			UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
			Test.AddInfo(Line);
			++Cases;
			Matches += bMatch ? 1 : 0;
			if (!bMatch && !bKnown)
			{
				Test.AddError(FString::Printf(TEXT("%s held misses its 72 Hz result and is not in KnownHeldMisses"), *Key));
				bPass = false;
			}
			if (bMatch && bKnown)
			{
				Test.AddError(FString::Printf(TEXT("%s is in KnownHeldMisses but now matches: remove it"), *Key));
				bPass = false;
			}
		}
	}
	const FString Summary = FString::Printf(TEXT("HeldSample %s matches %d/%d"), Action, Matches, Cases);
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

/** The corpus, impostors and noise through the pipeline, at 72 Hz (bHeld false) or held at phase 0. */
bool RunCorpus(FAutomationTestBase& Test, bool bHeld, FCorpusFigures& Out)
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
			const FHandClip Played = bHeld ? MakeHeldClip(Clip, GCameraHz, GPhases[0]) : Clip;
			if (!Rig.Play(Played))
			{
				Test.AddError(FString::Printf(TEXT("%s did not play"), *Path));
				bPass = false;
				continue;
			}
			Consume(Clip);
		}
	};
	Run(TEXT("corpus"), [&](const FHandClip& Clip)
	{
		++Out.Clips;
		const FString Expected = Clip.Action.Right(5);
		Out.Correct += (Rig.Casts.Num() >= 1 && Rig.Casts[0].ToString() == Expected) ? 1 : 0;
	});
	Run(TEXT("impostors"), [&](const FHandClip&)
	{
		++Out.Impostors;
		Out.ImpostorCasts += Rig.Casts.Num();
	});
	Run(TEXT("noise"), [&](const FHandClip&)
	{
		++Out.Noise;
		Out.NoiseCasts += Rig.Casts.Num();
	});
	Rig.Close();
	return bPass;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHeldSampleWard, "MageArena.Hands.HeldSample.Ward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHeldSampleWard::RunTest(const FString& Parameters)
{
	FWardFigures Figures;
	const bool bPass = RunRatchet(*this, TEXT("ward-raise"), EKind::Ward, &Figures);
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
		bPass &= RunRatchet(*this, Action, EKind::Blink, nullptr);
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
		bPass &= RunRatchet(*this, Action, EKind::Sigil, nullptr);
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
	bool bPass = RunCorpus(*this, false, Normal);
	bPass &= RunCorpus(*this, true, Held);
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

#endif
