#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/AbsorbResolver.h"
#include "Engine/GameInstance.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/HandClip.h"
#include "Hands/HandClipPlayer.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPlayerController.h"
#include "HeadMountedDisplayTypes.h"
#include "InputCoreTypes.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
struct FLatencyRow
{
	double LatencyS = 0.0;
	double ConfirmS = -1.0;
	double OnsetS = -1.0;
	double ErrorS = 0.0;
	double ErrorFrames = 0.0;
	double ConfirmShiftS = 0.0;
	double HitS = 0.0;
	bool bRaised = false;
	bool bPerfect = false;
	bool bWithinOneFrame = false;
	bool bConfirmTracked = false;
};

struct FWardReport
{
	int32 Onset = -1;
	int32 Held = -1;
	int32 Kinds = -1;
	int32 Key = -1;
	int32 OnsetAfter = -1;
	int32 Settle = -1;
	int32 Already = -1;
	int32 Resume = -1;
	int32 Chord = -1;
	double RapidGapS = -1.0;
	double RapidOnsetS = -1.0;
	double RapidLoweredS = -1.0;
	int32 RapidFresh = -1;
	int32 AlreadyPerfects = -1;
	int32 AlreadyFresh = -1;
	double ResumeTimeS = -1.0;
	FString ResumeVariant;
	int32 VerticalOutside = -1;
	int32 ZeroOutside = -1;
	double SettleGap60S = -1.0;
	int32 SpaceConsumes = -1;
	bool bHasRules = false;
	bool bHasAuthored = false;
	bool bHasFresh = false;
	FAbsorbRules Rules;
	double AuthoredOnsetS = 0.0;
	double AuthoredConfirmS = 0.0;
	double GeometricOnsetS = 0.0;
	double FrameS = 0.0;
	double ShortGapS = 0.0;
	double LongGapS = 0.0;
	bool bShortFresh = false;
	bool bLongFresh = false;
	bool bShortNotPerfect = false;
	bool bLongPerfect = false;
	TArray<FLatencyRow> Rows;
	TArray<FLatencyRow> SettleRows;

	static FWardReport& Get()
	{
		static FWardReport Report;
		return Report;
	}

	void Set(const TCHAR* Name, bool bPass)
	{
		const int32 Value = bPass ? 1 : 0;
		const FString Label(Name);
		if (Label == TEXT("MageArena.Ward.OnsetCompensation"))
		{
			Onset = Value;
		}
		else if (Label == TEXT("MageArena.Ward.HeldNeverPerfect"))
		{
			Held = Value;
		}
		else if (Label == TEXT("MageArena.Ward.KindsAndArc"))
		{
			Kinds = Value;
		}
		else if (Label == TEXT("MageArena.Ward.KeyHoldPath"))
		{
			Key = Value;
		}
		else if (Label == TEXT("MageArena.Ward.OnsetAfterLower"))
		{
			OnsetAfter = Value;
		}
		else if (Label == TEXT("MageArena.Ward.SettleOnset"))
		{
			Settle = Value;
		}
		else if (Label == TEXT("MageArena.Ward.AlreadyRaised"))
		{
			Already = Value;
		}
		else if (Label == TEXT("MageArena.Ward.ResumeAfterRelease"))
		{
			Resume = Value;
		}
		else if (Label == TEXT("MageArena.Ward.HoldChord"))
		{
			Chord = Value;
		}
	}

	struct FPublish
	{
		explicit FPublish(const TCHAR* InName, bool& InPass)
			: Name(InName)
			, Pass(InPass)
		{
		}

		~FPublish()
		{
			FWardReport::Get().Set(Name, Pass);
			FString Error;
			FWardReport::Get().Write(Error);
		}

		const TCHAR* Name;
		bool& Pass;
	};

	static FString Num(double Value)
	{
		return FString::Printf(TEXT("%.6f"), Value);
	}

	static FString Bool(bool Value)
	{
		return Value ? TEXT("true") : TEXT("false");
	}

	static FString Tri(int32 Value)
	{
		if (Value < 0)
		{
			return TEXT("null");
		}
		return Value > 0 ? TEXT("true") : TEXT("false");
	}

	static FString Escape(const FString& Text)
	{
		FString Out;
		Out.Reserve(Text.Len());
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Char = Text[Index];
			if (Char == TEXT('\\') || Char == TEXT('"'))
			{
				Out.AppendChar(TEXT('\\'));
			}
			Out.AppendChar(Char);
		}
		return Out;
	}

	static void AppendRows(FString& Json, const TCHAR* Key, const TArray<FLatencyRow>& InRows)
	{
		if (InRows.Num() == 0)
		{
			Json += FString::Printf(TEXT("  \"%s\": null,\n"), Key);
			return;
		}
		Json += FString::Printf(TEXT("  \"%s\": [\n"), Key);
		for (int32 Index = 0; Index < InRows.Num(); ++Index)
		{
			const FLatencyRow& Row = InRows[Index];
			Json += FString::Printf(TEXT("    {\"latencyMs\": %s, \"confirmS\": %s, \"onsetS\": %s, \"errorS\": %s, \"errorFrames\": %s, \"confirmShiftS\": %s, \"hitS\": %s, \"bRaised\": %s, \"bPerfect\": %s, \"withinOneFrame\": %s, \"bConfirmTracked\": %s}%s\n"),
				*Num(Row.LatencyS * 1000.0), *Num(Row.ConfirmS), *Num(Row.OnsetS), *Num(Row.ErrorS), *Num(Row.ErrorFrames),
				*Num(Row.ConfirmShiftS), *Num(Row.HitS), *Bool(Row.bRaised), *Bool(Row.bPerfect), *Bool(Row.bWithinOneFrame),
				*Bool(Row.bConfirmTracked), Index + 1 < InRows.Num() ? TEXT(",") : TEXT(""));
		}
		Json += TEXT("  ],\n");
	}

	bool Write(FString& OutError) const
	{
		FString Json;
		Json += TEXT("{\n");
		Json += FString::Printf(TEXT("  \"thresholds\": {\"raiseAngleDeg\": %s, \"lowerAngleDeg\": %s, \"raiseHeightM\": %s, \"lowerHeightM\": %s, \"onsetSpeedMps\": %s, \"onsetTailS\": %s, \"historySeconds\": %s},\n"),
			*Num(FWardThresholds::RaiseAngleDeg), *Num(FWardThresholds::LowerAngleDeg), *Num(FWardThresholds::RaiseHeightM),
			*Num(FWardThresholds::LowerHeightM), *Num(FWardThresholds::OnsetSpeedMps), *Num(FWardThresholds::OnsetTailS),
			*Num(FWardThresholds::HistorySeconds));
		if (!bHasRules)
		{
			Json += TEXT("  \"pinned\": null,\n");
		}
		else
		{
			Json += TEXT("  \"pinned\": {\n");
			Json += FString::Printf(TEXT("    \"path\": \"%s\",\n"), *Escape(Rules.SourcePath));
			Json += FString::Printf(TEXT("    \"arcDeg\": %s,\n"), *Num(Rules.ArcDeg));
			Json += FString::Printf(TEXT("    \"windowS\": %s,\n"), *Num(Rules.WindowS));
			Json += FString::Printf(TEXT("    \"minReleaseBeforeReRaiseS\": %s,\n"), *Num(Rules.MinReleaseS));
			Json += FString::Printf(TEXT("    \"raiseCostMana\": %s,\n"), *Num(Rules.RaiseCostMana));
			Json += FString::Printf(TEXT("    \"drainPerSecond\": \"%s\",\n"), *Escape(Rules.DrainFormula));
			Json += FString::Printf(TEXT("    \"manaReturned\": \"%s\",\n"), *Escape(Rules.ManaFormula));
			Json += FString::Printf(TEXT("    \"tierClockAdvanceS\": %s,\n"), *Num(Rules.TierClockAdvanceS));
			Json += FString::Printf(TEXT("    \"perfectReduction\": %s,\n"), *Num(Rules.PerfectReduction));
			Json += FString::Printf(TEXT("    \"reduction\": {\"magic\": %s, \"physical\": %s, \"unblockable\": %s, \"outsideArc\": %s},\n"),
				*Num(Rules.ReductionMagic), *Num(Rules.ReductionPhysical), *Num(Rules.ReductionUnblockable), *Num(Rules.ReductionOutsideArc));
			Json += TEXT("    \"notAgainst\": [");
			for (int32 Index = 0; Index < Rules.NotAgainst.Num(); ++Index)
			{
				Json += FString::Printf(TEXT("%s\"%s\""), Index > 0 ? TEXT(", ") : TEXT(""), *Escape(Rules.NotAgainst[Index]));
			}
			Json += FString::Printf(TEXT("],\n    \"nerveRank\": %s,\n"), *Num(FAbsorbResolver::NerveRank));
			Json += FString::Printf(TEXT("    \"drainBase\": %s,\n    \"drainSubtractPerRank\": %s,\n    \"manaPerTier\": %s,\n    \"manaPerRank\": %s,\n    \"tierZeroReturn\": %s\n"),
				*Num(Rules.DrainBase), *Num(Rules.DrainSubtractPerRank), *Num(Rules.ManaPerTier), *Num(Rules.ManaPerRank), *Num(Rules.TierZeroReturn));
			Json += TEXT("  },\n");
		}
		if (!bHasAuthored)
		{
			Json += TEXT("  \"authored\": null,\n");
		}
		else
		{
			Json += FString::Printf(TEXT("  \"authored\": {\"onsetS\": %s, \"confirmS\": %s, \"geometricOnsetS\": %s, \"frameS\": %s},\n"),
				*Num(AuthoredOnsetS), *Num(AuthoredConfirmS), *Num(GeometricOnsetS), *Num(FrameS));
		}
		AppendRows(Json, TEXT("latency"), Rows);
		AppendRows(Json, TEXT("settleLatency"), SettleRows);
		Json += TEXT("  \"tests\": {\n");
		Json += FString::Printf(TEXT("    \"MageArena.Ward.OnsetCompensation\": %s,\n"), *Tri(Onset));
		Json += FString::Printf(TEXT("    \"MageArena.Ward.HeldNeverPerfect\": %s,\n"), *Tri(Held));
		Json += FString::Printf(TEXT("    \"MageArena.Ward.KindsAndArc\": %s,\n"), *Tri(Kinds));
		Json += FString::Printf(TEXT("    \"MageArena.Ward.KeyHoldPath\": %s,\n"), *Tri(Key));
		Json += FString::Printf(TEXT("    \"MageArena.Ward.OnsetAfterLower\": %s,\n"), *Tri(OnsetAfter));
		Json += FString::Printf(TEXT("    \"MageArena.Ward.SettleOnset\": %s,\n"), *Tri(Settle));
		Json += FString::Printf(TEXT("    \"MageArena.Ward.AlreadyRaised\": %s,\n"), *Tri(Already));
		Json += FString::Printf(TEXT("    \"MageArena.Ward.ResumeAfterRelease\": %s,\n"), *Tri(Resume));
		Json += FString::Printf(TEXT("    \"MageArena.Ward.HoldChord\": %s\n"), *Tri(Chord));
		Json += TEXT("  },\n");
		if (!bHasFresh)
		{
			Json += TEXT("  \"fresh\": null,\n");
		}
		else
		{
			Json += FString::Printf(TEXT("  \"fresh\": {\"shortGapS\": %s, \"shortGapFresh\": %s, \"shortGapNotPerfect\": %s, \"longGapS\": %s, \"longGapFresh\": %s, \"longGapPerfect\": %s},\n"),
				*Num(ShortGapS), *Bool(bShortFresh), *Bool(bShortNotPerfect), *Num(LongGapS), *Bool(bLongFresh), *Bool(bLongPerfect));
		}
		Json += TEXT("  \"retry\": {\n");
		Json += FString::Printf(TEXT("    \"rapidGapS\": %s,\n"), *Num(RapidGapS));
		Json += FString::Printf(TEXT("    \"rapidOnsetS\": %s,\n"), *Num(RapidOnsetS));
		Json += FString::Printf(TEXT("    \"rapidLoweredS\": %s,\n"), *Num(RapidLoweredS));
		Json += FString::Printf(TEXT("    \"rapidFresh\": %s,\n"), *Tri(RapidFresh));
		Json += FString::Printf(TEXT("    \"alreadyRaisedPerfects\": %s,\n"), *Num(static_cast<double>(AlreadyPerfects)));
		Json += FString::Printf(TEXT("    \"alreadyFresh\": %s,\n"), *Tri(AlreadyFresh));
		Json += FString::Printf(TEXT("    \"resumeTimeS\": %s,\n"), *Num(ResumeTimeS));
		Json += FString::Printf(TEXT("    \"resumeVariant\": \"%s\",\n"), *Escape(ResumeVariant));
		Json += FString::Printf(TEXT("    \"verticalOutside\": %s,\n"), *Tri(VerticalOutside));
		Json += FString::Printf(TEXT("    \"zeroOutside\": %s,\n"), *Tri(ZeroOutside));
		Json += FString::Printf(TEXT("    \"settleGap60S\": %s,\n"), *Num(SettleGap60S));
		Json += FString::Printf(TEXT("    \"onsetTailS\": %s,\n"), *Num(FWardThresholds::OnsetTailS));
		Json += FString::Printf(TEXT("    \"spaceConsumes\": %s\n"), *Tri(SpaceConsumes));
		Json += TEXT("  }\n");
		Json += TEXT("}\n");

		const FString ReportDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Reports"));
		IFileManager::Get().MakeDirectory(*ReportDir, true);
		const FString ReportPath = FPaths::Combine(ReportDir, TEXT("ward-timing.json"));
		if (!FFileHelper::SaveStringToFile(Json, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("failed to write %s"), *ReportPath);
			UE_LOG(LogMageArena, Error, TEXT("%s"), *OutError);
			return false;
		}
		return true;
	}
};

struct FWardRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;

	bool Start(FAutomationTestBase& Test)
	{
		Instance = NewObject<UGameInstance>(GetTransientPackage());
		Instance->AddToRoot();
		Hands = NewObject<UHandInputSubsystem>(Instance);
		Hands->AddToRoot();
		Wards = NewObject<UWardDetectorSubsystem>(Instance);
		Wards->AddToRoot();
		FString Error;
		if (!Wards->InitDetector(Error))
		{
			Test.AddError(Error);
			return false;
		}
		Wards->BindToHands(Hands);
		return true;
	}

	void Finish()
	{
		if (Wards)
		{
			Wards->BindToHands(nullptr);
			Wards->RemoveFromRoot();
			Wards->MarkAsGarbage();
			Wards = nullptr;
		}
		if (Hands)
		{
			if (UHandClipPlayer* Player = Hands->GetClipPlayer())
			{
				Player->Stop();
			}
			Hands->RemoveFromRoot();
			Hands->MarkAsGarbage();
			Hands = nullptr;
		}
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
			Instance = nullptr;
		}
	}
};

const FHandClipTrack* LeftTrack(const FHandClip& Clip)
{
	for (const FHandClipTrack& Track : Clip.GetTracks())
	{
		if (Track.Hand == EControllerHand::Left && Track.Frames.Num() > 1)
		{
			return &Track;
		}
	}
	return nullptr;
}

bool LoadWard(FHandClip& OutClip, FString& OutError)
{
	return FHandClip::LoadFromFile(FHandClip::MakeFilePath(TEXT("ward-raise"), EClipVariant::Normal), OutClip, OutError);
}

double GeometricOnset(const FHandClipTrack& Track)
{
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	for (int32 Index = 1; Index < Track.Frames.Num(); ++Index)
	{
		const FHandFrame& Prev = Track.Frames[Index - 1];
		const FHandFrame& Frame = Track.Frames[Index];
		if (!Frame.Joints.IsValidIndex(Palm) || !Prev.Joints.IsValidIndex(Palm))
		{
			continue;
		}
		const double Dt = Frame.TimeSeconds - Prev.TimeSeconds;
		if (Dt < 1.0e-6)
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

double GeometricConfirm(const FHandClipTrack& Track)
{
	for (const FHandFrame& Frame : Track.Frames)
	{
		const FVector Normal = FWardDetector::PalmNormal(Frame);
		const double Angle = FWardDetector::AngleToFacingDeg(Normal, FVector::ForwardVector);
		const double Height = FWardDetector::PalmHeightMetres(Frame);
		if (Angle <= FWardThresholds::RaiseAngleDeg && Height >= FWardThresholds::RaiseHeightM)
		{
			return Frame.TimeSeconds;
		}
	}
	return -1.0;
}

void Feed(FWardDetector& Detector, const FHandClip& Clip, double SourceTime, double StampTime)
{
	FHandFrame Frame;
	if (!Clip.Sample(EControllerHand::Left, SourceTime, Frame))
	{
		return;
	}
	Frame.TimeSeconds = StampTime;
	Detector.Ingest(Frame);
}

bool PrepareRules(FAutomationTestBase& Test, FAbsorbResolver& Resolver, FWardDetector& Detector)
{
	FString Error;
	if (!Resolver.Init(Error))
	{
		Test.AddError(Error);
		return false;
	}
	if (!Detector.Init(Error))
	{
		Test.AddError(Error);
		return false;
	}
	FWardReport& Report = FWardReport::Get();
	Report.Rules = Resolver.Rules();
	Report.bHasRules = true;
	return true;
}

FHandFrame AtTime(const FHandFrame& Source, double Time)
{
	FHandFrame Out = Source;
	Out.TimeSeconds = Time;
	Out.Hand = EControllerHand::Left;
	return Out;
}

FHandFrame Moved(const FHandFrame& Source, double Time, const FVector& DeltaCm)
{
	FHandFrame Out = AtTime(Source, Time);
	for (FHandJointPose& Joint : Out.Joints)
	{
		Joint.Location += DeltaCm;
	}
	return Out;
}

struct FLapPrelude
{
	double LoweredTime = -1.0;
	double TwitchTime = -1.0;
	double Time = -1.0;
	bool bOk = false;
};

/** Raise on the hold, drop to the lap, twitch at lowered+0.06, then sit still for 0.25 s. */
bool PreludeLapTwitch(FAutomationTestBase& Test, FWardDetector& Detector, const FHandFrame& Hold, const FHandFrame& Lap, double Dt, FLapPrelude& Out)
{
	double T = 0.0;
	Detector.Ingest(AtTime(Hold, T));
	if (!Detector.GetState().bRaised)
	{
		Test.AddError(TEXT("hold pose did not raise"));
		return false;
	}
	T += Dt;
	Detector.Ingest(AtTime(Hold, T));
	T += Dt;
	Detector.Ingest(AtTime(Lap, T));
	if (Detector.GetState().bRaised)
	{
		Test.AddError(TEXT("lap pose did not lower the ward"));
		return false;
	}
	Out.LoweredTime = Detector.GetLoweredTime();
	const double TwitchAt = Out.LoweredTime + 0.06;
	while (T + Dt < TwitchAt - 1.0e-9)
	{
		T += Dt;
		Detector.Ingest(AtTime(Lap, T));
	}
	T += Dt;
	if (T + 1.0e-9 < TwitchAt)
	{
		T = TwitchAt;
	}
	Detector.Ingest(Moved(Lap, T, FVector(0.0, 4.0, 0.0)));
	Out.TwitchTime = T;
	T += Dt;
	Detector.Ingest(AtTime(Lap, T));
	const double StillUntil = Out.TwitchTime + 0.25;
	while (T + Dt < StillUntil - 1.0e-9)
	{
		T += Dt;
		Detector.Ingest(AtTime(Lap, T));
	}
	Out.Time = T;
	Out.bOk = !Detector.GetState().bRaised;
	if (!Out.bOk)
	{
		Test.AddError(TEXT("lap stillness raised the ward"));
	}
	return Out.bOk;
}

FHandFrame PalmUpFrame(double Time, double HeightM)
{
	FHandFrame Frame;
	Frame.TimeSeconds = Time;
	Frame.Hand = EControllerHand::Left;
	Frame.Confidence = 1.0f;
	Frame.Joints.SetNum(MageHandJointCount);
	const double Z = HeightM * MageClipMetresToCentimetres;
	const FVector Palm(22.0, -20.0, Z);
	for (FHandJointPose& Joint : Frame.Joints)
	{
		Joint.Location = Palm;
	}
	const int32 Wrist = static_cast<int32>(EHandKeypoint::Wrist);
	const int32 Index = static_cast<int32>(EHandKeypoint::IndexMetacarpal);
	const int32 Little = static_cast<int32>(EHandKeypoint::LittleMetacarpal);
	Frame.Joints[Wrist].Location = FVector(16.0, -20.0, Z);
	Frame.Joints[Index].Location = FVector(22.0, -24.0, Z);
	Frame.Joints[Little].Location = FVector(22.0, -16.0, Z);
	Frame.Joints[static_cast<int32>(EHandKeypoint::Palm)].Location = Palm;
	return Frame;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardOnset, "MageArena.Ward.OnsetCompensation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardOnset::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.OnsetCompensation"), bPass);

	FAbsorbResolver Resolver;
	FWardDetector Detector;
	if (!PrepareRules(*this, Resolver, Detector))
	{
		return false;
	}
	FHandClip Clip;
	FString Error;
	if (!LoadWard(Clip, Error))
	{
		AddError(Error);
		return false;
	}
	const FHandClipTrack* Track = LeftTrack(Clip);
	if (Track == nullptr || Clip.Hz <= 0.0)
	{
		AddError(TEXT("ward-raise.normal has no left-hand track"));
		return false;
	}

	const double Frame = 1.0 / Clip.Hz;
	const double Plateau = UHandClipPlayer::FindPlateauStart(Clip, EControllerHand::Left);
	FHandFrame Hold;
	bPass = true;
	if (!Clip.Sample(EControllerHand::Left, Plateau, Hold) || !Hold.Joints.IsValidIndex(static_cast<int32>(EHandKeypoint::Palm)))
	{
		AddError(TEXT("ward-raise.normal hold pose has no palm"));
		bPass = false;
		return false;
	}
	const FVector Normal = FWardDetector::PalmNormal(Hold);
	const double AxisDot = FVector::DotProduct(Normal, Hold.Joints[static_cast<int32>(EHandKeypoint::Palm)].Rotation.GetAxisX());
	bPass &= TestTrue(*FString::Printf(TEXT("palm normal matches +X (dot %.3f)"), AxisDot), AxisDot > 0.99);

	const double GeoOnset = GeometricOnset(*Track);
	const double GeoConfirm = GeometricConfirm(*Track);
	bPass &= TestTrue(TEXT("clip has a palm-speed onset"), GeoOnset >= 0.0);
	bPass &= TestTrue(TEXT("clip has a raised pose"), GeoConfirm > GeoOnset);

	Detector.ResetStream();
	for (const FHandFrame& FrameSample : Track->Frames)
	{
		Detector.Ingest(FrameSample);
		if (Detector.GetState().bRaised)
		{
			break;
		}
	}
	const double AuthoredOnset = Detector.GetLastRaise().OnsetTime;
	const double AuthoredConfirm = Detector.GetState().bRaised ? Track->Frames[FMath::Min(Track->Frames.Num() - 1, 0)].TimeSeconds : -1.0;
	double ConfirmStamp = -1.0;
	{
		FWardDetector Clean;
		FString Ignored;
		Clean.Init(Ignored);
		for (const FHandFrame& FrameSample : Track->Frames)
		{
			Clean.Ingest(FrameSample);
			if (Clean.GetState().bRaised)
			{
				ConfirmStamp = FrameSample.TimeSeconds;
				break;
			}
		}
		bPass &= TestTrue(TEXT("clean clip raises"), Clean.GetState().bRaised);
		bPass &= TestTrue(*FString::Printf(TEXT("clean onset %.4f matches speed crossing %.4f"), Clean.GetLastRaise().OnsetTime, GeoOnset),
			FMath::Abs(Clean.GetLastRaise().OnsetTime - GeoOnset) <= Frame + 1.0e-6);
		bPass &= TestTrue(*FString::Printf(TEXT("clean confirm %.4f matches pose crossing %.4f"), ConfirmStamp, GeoConfirm),
			FMath::Abs(ConfirmStamp - GeoConfirm) <= Frame + 1.0e-6);
	}
	if (!bPass || ConfirmStamp < 0.0 || GeoOnset < 0.0)
	{
		return false;
	}

	FWardReport& Report = FWardReport::Get();
	Report.bHasAuthored = true;
	Report.AuthoredOnsetS = GeoOnset;
	Report.AuthoredConfirmS = ConfirmStamp;
	Report.GeometricOnsetS = GeoOnset;
	Report.FrameS = Frame;
	(void)AuthoredOnset;
	(void)AuthoredConfirm;

	const double Anchor = GeoOnset + 2.0 * Frame;
	if (!(ConfirmStamp > Anchor))
	{
		AddError(TEXT("recognition is not after the onset anchor"));
		bPass = false;
		return false;
	}

	const double Window = Resolver.Rules().WindowS;
	const double AuthoredTick = GeoOnset + Window;
	const double Latencies[] = { 0.0, 0.020, 0.040, 0.060, 0.080, 0.100 };
	for (const double Latency : Latencies)
	{
		const double Scale = (ConfirmStamp - Anchor) / (ConfirmStamp - Anchor + Latency);
		FWardDetector Trial;
		FString Ignored;
		if (!Trial.Init(Ignored))
		{
			AddError(TEXT("detector lost the pinned rules"));
			bPass = false;
			return false;
		}
		double Output = 0.0;
		Feed(Trial, Clip, 0.0, 0.0);
		int32 Guard = 0;
		while (!Trial.GetState().bRaised && Output < ConfirmStamp + Latency + 0.5 && Guard < 500)
		{
			Output += Frame;
			double Source = Output;
			if (Output > Anchor)
			{
				Source = Anchor + (Output - Anchor) * Scale;
			}
			Source = FMath::Clamp(Source, 0.0, Clip.GetDuration());
			Feed(Trial, Clip, Source, Output);
			++Guard;
		}

		FLatencyRow Row;
		Row.LatencyS = Latency;
		Row.bRaised = Trial.GetState().bRaised;
		Row.ConfirmS = Row.bRaised ? Output : -1.0;
		Row.OnsetS = Row.bRaised ? Trial.GetLastRaise().OnsetTime : -1.0;
		Row.ErrorS = Row.bRaised ? Row.OnsetS - GeoOnset : 0.0;
		Row.ErrorFrames = Frame > 0.0 ? Row.ErrorS / Frame : 0.0;
		Row.ConfirmShiftS = Row.bRaised ? Row.ConfirmS - ConfirmStamp : 0.0;
		Row.HitS = AuthoredTick;
		Row.bWithinOneFrame = Row.bRaised && FMath::Abs(Row.ErrorS) <= Frame + 1.0e-6;
		Row.bConfirmTracked = Row.bRaised && FMath::Abs(Row.ConfirmShiftS - Latency) <= Frame + 1.0e-4;

		const FWardState State = Trial.GetState();
		const FAbsorbResult AtTick = Resolver.Resolve(AuthoredTick, EAbsorbHitKind::Magic, 3, State.Facing, State);
		FAbsorbResult Before;
		Before.bPerfect = false;
		if (Row.bRaised)
		{
			Before = Resolver.Resolve(AuthoredTick - Frame, EAbsorbHitKind::Magic, 3, State.Facing, State);
		}
		const bool bEarly = Row.ErrorS < -1.0e-9;
		const bool bTiming = Row.bWithinOneFrame && (AtTick.bPerfect || (bEarly && Before.bPerfect));
		Row.bPerfect = AtTick.bPerfect;
		Report.Rows.Add(Row);

		const FString Line = FString::Printf(TEXT("latency %5.1f ms confirm=%.4f onset=%.4f error=%.4f ms frames=%.2f shift=%.4f perfect=%d tracked=%d"),
			Latency * 1000.0, Row.ConfirmS, Row.OnsetS, Row.ErrorS * 1000.0, Row.ErrorFrames, Row.ConfirmShiftS, AtTick.bPerfect ? 1 : 0, Row.bConfirmTracked ? 1 : 0);
		UE_LOG(LogMageArena, Log, TEXT("Ward %s"), *Line);
		AddInfo(Line);

		const bool bGate = Latency <= 0.060 + 1.0e-9;
		if (bGate)
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s within one frame"), *Line), bTiming);
			bPass &= TestTrue(*FString::Printf(TEXT("%s confirm tracks the injected latency"), *Line), Row.bConfirmTracked);
		}
		else
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s still raises"), *Line), Row.bRaised);
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardHeld, "MageArena.Ward.HeldNeverPerfect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardHeld::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.HeldNeverPerfect"), bPass);
	FWardRig Rig;
	if (!Rig.Start(*this))
	{
		return false;
	}
	FAbsorbResolver Resolver;
	FString Error;
	if (!Resolver.Init(Error))
	{
		AddError(Error);
		Rig.Finish();
		return false;
	}
	FWardReport::Get().Rules = Resolver.Rules();
	FWardReport::Get().bHasRules = true;

	int32 Raises = 0;
	Rig.Wards->OnWardRaised.AddLambda([&Raises](double, FVector)
	{
		++Raises;
	});
	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	UHandClipPlayer* Player = Rig.Hands->GetClipPlayer();
	if (!Player || !Player->IsPlaying())
	{
		AddError(TEXT("ward hold did not start"));
		Rig.Finish();
		return false;
	}

	const double Frame = 1.0 / 72.0;
	int32 Guard = 0;
	while (Player->GetTime() < 10.0 && Guard < 2000)
	{
		Rig.Hands->Step(Frame);
		++Guard;
	}
	bPass = true;
	bPass &= TestTrue(TEXT("held for 10s"), Player->GetTime() + 1.0e-6 >= 10.0);
	bPass &= TestEqual(TEXT("one raise across the hold"), Raises, 1);
	bPass &= TestTrue(TEXT("still raised"), Rig.Wards->GetState().bRaised);
	const double Onset = Rig.Wards->GetState().OnsetTime;
	int32 Perfects = 0;
	double HitTime = Player->GetTime();
	for (int32 Bolt = 0; Bolt < 3; ++Bolt)
	{
		const FWardState State = Rig.Wards->GetState();
		const FAbsorbResult Hit = Resolver.Resolve(HitTime, EAbsorbHitKind::Magic, 2, State.Facing, State);
		if (Hit.bPerfect)
		{
			++Perfects;
		}
		bPass &= TestFalse(*FString::Printf(TEXT("bolt %d perfect"), Bolt), Hit.bPerfect);
		bPass &= TestEqual(*FString::Printf(TEXT("bolt %d magic reduction"), Bolt), Hit.Reduction, Resolver.Rules().ReductionFor(EAbsorbHitKind::Magic));
		bPass &= TestTrue(*FString::Printf(TEXT("bolt %d still the original onset"), Bolt), FMath::Abs(State.OnsetTime - Onset) <= 1.0e-6);
		HitTime += 0.2;
		Rig.Hands->Step(0.2);
	}
	bPass &= TestEqual(TEXT("zero perfects"), Perfects, 0);
	bPass &= TestEqual(TEXT("hold did not re-raise"), Raises, 1);
	UE_LOG(LogMageArena, Log, TEXT("Ward held 10s onset=%.4f perfects=%d raises=%d"), Onset, Perfects, Raises);
	AddInfo(FString::Printf(TEXT("held 10s onset %.4f perfects %d"), Onset, Perfects));
	Rig.Finish();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardKinds, "MageArena.Ward.KindsAndArc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardKinds::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.KindsAndArc"), bPass);
	FAbsorbResolver Resolver;
	FString Error;
	if (!Resolver.Init(Error))
	{
		AddError(Error);
		return false;
	}
	const FAbsorbRules& Rules = Resolver.Rules();
	FWardReport::Get().Rules = Rules;
	FWardReport::Get().bHasRules = true;

	FWardState Raised;
	Raised.bRaised = true;
	Raised.bFresh = true;
	Raised.OnsetTime = 1.0;
	Raised.Facing = FVector::ForwardVector;
	const double Mid = Raised.OnsetTime + Rules.WindowS * 0.5;
	const FVector Inside = FVector::ForwardVector;

	FAbsorbResolver Cold;
	const FAbsorbResult ColdHit = Cold.Resolve(Mid, EAbsorbHitKind::Magic, 1, Inside, Raised);
	bPass = TestFalse(TEXT("unloaded resolver is not perfect"), ColdHit.bPerfect);
	bPass &= TestEqual(TEXT("unloaded resolver reduces nothing"), ColdHit.Reduction, 0.0);

	const FAbsorbResult Magic = Resolver.Resolve(Mid, EAbsorbHitKind::Magic, 0, Inside, Raised);
	const FAbsorbResult MagicTier = Resolver.Resolve(Mid, EAbsorbHitKind::Magic, 3, Inside, Raised);
	bPass &= TestTrue(TEXT("fresh magic perfects"), Magic.bPerfect);
	bPass &= TestEqual(TEXT("perfect reduction"), Magic.Reduction, Rules.PerfectReduction);
	bPass &= TestEqual(TEXT("tier 0 mana"), Magic.ManaReturned, Rules.ManaForPerfect(0, FAbsorbResolver::NerveRank));
	bPass &= TestEqual(TEXT("tier 3 mana"), MagicTier.ManaReturned, Rules.ManaForPerfect(3, FAbsorbResolver::NerveRank));
	bPass &= TestEqual(TEXT("tier clock"), Magic.TierClockAdvanceS, Rules.TierClockAdvanceS);
	bPass &= TestTrue(TEXT("exact window is perfect"), Resolver.Resolve(Raised.OnsetTime + Rules.WindowS, EAbsorbHitKind::Magic, 1, Inside, Raised).bPerfect);
	bPass &= TestFalse(TEXT("past the window is not perfect"), Resolver.Resolve(Raised.OnsetTime + Rules.WindowS + 0.001, EAbsorbHitKind::Magic, 1, Inside, Raised).bPerfect);
	bPass &= TestFalse(TEXT("before onset is not perfect"), Resolver.Resolve(Raised.OnsetTime - 0.01, EAbsorbHitKind::Magic, 1, Inside, Raised).bPerfect);

	const FAbsorbResult Physical = Resolver.Resolve(Mid, EAbsorbHitKind::Physical, 3, Inside, Raised);
	bPass &= TestFalse(TEXT("physical never perfect"), Physical.bPerfect);
	bPass &= TestEqual(TEXT("physical reduction"), Physical.Reduction, Rules.ReductionFor(EAbsorbHitKind::Physical));
	bPass &= TestEqual(TEXT("physical mana"), Physical.ManaReturned, 0.0);
	bPass &= TestEqual(TEXT("physical clock"), Physical.TierClockAdvanceS, 0.0);
	bPass &= TestTrue(TEXT("physical is in notAgainst"), Rules.BlocksPerfect(EAbsorbHitKind::Physical));

	const FAbsorbResult Unblockable = Resolver.Resolve(Mid, EAbsorbHitKind::Unblockable, 3, Inside, Raised);
	bPass &= TestFalse(TEXT("unblockable never perfect"), Unblockable.bPerfect);
	bPass &= TestEqual(TEXT("unblockable reduction"), Unblockable.Reduction, Rules.ReductionFor(EAbsorbHitKind::Unblockable));
	bPass &= TestEqual(TEXT("unblockable mana"), Unblockable.ManaReturned, 0.0);
	bPass &= TestTrue(TEXT("unblockable is reduced less than a perfect"), Unblockable.Reduction + 1.0e-6 < Rules.PerfectReduction);
	bPass &= TestTrue(TEXT("unblockable is in notAgainst"), Rules.BlocksPerfect(EAbsorbHitKind::Unblockable));
	bPass &= TestFalse(TEXT("magic is not in notAgainst"), Rules.BlocksPerfect(EAbsorbHitKind::Magic));

	bPass &= TestNotEqual(TEXT("magic and outside differ"), Rules.ReductionFor(EAbsorbHitKind::Magic), Rules.ReductionOutsideArc);
	const double OutsideYaw = FMath::DegreesToRadians(Rules.ArcDeg * 0.5 + 5.0);
	const FVector Outside(FMath::Cos(OutsideYaw), FMath::Sin(OutsideYaw), 0.0);
	const FAbsorbResult OutsideHit = Resolver.Resolve(Mid, EAbsorbHitKind::Magic, 3, Outside, Raised);
	bPass &= TestFalse(TEXT("outside the arc is not perfect"), OutsideHit.bPerfect);
	bPass &= TestEqual(TEXT("outside the arc is not reduced"), OutsideHit.Reduction, Rules.ReductionOutsideArc);
	bPass &= TestNotEqual(TEXT("outside is not the magic reduction"), OutsideHit.Reduction, Rules.ReductionFor(EAbsorbHitKind::Magic));

	const FAbsorbResult Vertical = Resolver.Resolve(Mid, EAbsorbHitKind::Magic, 3, FVector(0.0, 0.0, -1.0), Raised);
	bPass &= TestFalse(TEXT("vertical hit is not perfect"), Vertical.bPerfect);
	bPass &= TestEqual(TEXT("vertical hit is outside the arc"), Vertical.Reduction, Rules.ReductionOutsideArc);
	const FAbsorbResult StraightUp = Resolver.Resolve(Mid, EAbsorbHitKind::Magic, 3, FVector(0.0, 0.0, 1.0), Raised);
	bPass &= TestFalse(TEXT("upward hit is not perfect"), StraightUp.bPerfect);
	bPass &= TestEqual(TEXT("upward hit is outside the arc"), StraightUp.Reduction, Rules.ReductionOutsideArc);
	const FAbsorbResult ZeroHit = Resolver.Resolve(Mid, EAbsorbHitKind::Magic, 3, FVector::ZeroVector, Raised);
	bPass &= TestFalse(TEXT("zero hit is not perfect"), ZeroHit.bPerfect);
	bPass &= TestEqual(TEXT("zero hit is outside the arc"), ZeroHit.Reduction, Rules.ReductionOutsideArc);
	FWardReport::Get().VerticalOutside = (!Vertical.bPerfect && Vertical.Reduction == Rules.ReductionOutsideArc && !StraightUp.bPerfect) ? 1 : 0;
	FWardReport::Get().ZeroOutside = (!ZeroHit.bPerfect && ZeroHit.Reduction == Rules.ReductionOutsideArc) ? 1 : 0;

	const double EdgeYaw = FMath::DegreesToRadians(Rules.ArcDeg * 0.5);
	const FVector Edge(FMath::Cos(EdgeYaw), FMath::Sin(EdgeYaw), 0.0);
	bPass &= TestTrue(TEXT("arc edge is inside"), Resolver.Resolve(Mid, EAbsorbHitKind::Magic, 1, Edge, Raised).bPerfect);

	FWardState Stale = Raised;
	Stale.bFresh = false;
	const FAbsorbResult StaleHit = Resolver.Resolve(Mid, EAbsorbHitKind::Magic, 3, Inside, Stale);
	bPass &= TestFalse(TEXT("stale raise is not perfect"), StaleHit.bPerfect);
	bPass &= TestEqual(TEXT("stale raise keeps the magic reduction"), StaleHit.Reduction, Rules.ReductionFor(EAbsorbHitKind::Magic));
	bPass &= TestEqual(TEXT("stale mana"), StaleHit.ManaReturned, 0.0);

	bPass &= TestTrue(TEXT("parsed mana grows with nerve"), Rules.ManaForPerfect(2, 2.0) > Rules.ManaForPerfect(2, 0.0));
	bPass &= TestTrue(TEXT("parsed drain falls with nerve"), Rules.DrainPerSecond(0.0) > Rules.DrainPerSecond(1.0));

	FWardRig Rig;
	if (!Rig.Start(*this))
	{
		bPass = false;
		return false;
	}
	double LoweredAt = -1.0;
	Rig.Wards->OnWardLowered.AddLambda([&Rig, &LoweredAt]()
	{
		if (Rig.Hands && Rig.Hands->GetClipPlayer())
		{
			LoweredAt = Rig.Hands->GetClipPlayer()->GetTime();
		}
	});
	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	UHandClipPlayer* Player = Rig.Hands->GetClipPlayer();
	if (!Player)
	{
		AddError(TEXT("short-gap ward did not start"));
		Rig.Finish();
		bPass = false;
		return false;
	}
	const double Step = Player->GetClip().Hz > 0.0 ? 1.0 / Player->GetClip().Hz : 1.0 / 72.0;
	int32 Guard = 0;
	while ((Player->GetTime() < 0.30 || !Rig.Wards->GetState().bRaised) && Guard < 200)
	{
		Rig.Hands->Step(Step);
		++Guard;
	}
	Rig.Hands->SetWardHeld(false, EClipVariant::Normal);
	while (Rig.Wards->GetState().bRaised && Guard < 400)
	{
		Rig.Hands->Step(Step);
		++Guard;
	}
	bPass &= TestFalse(TEXT("short reverse lowered the ward"), Rig.Wards->GetState().bRaised);
	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	while (!Rig.Wards->GetState().bRaised && Guard < 600)
	{
		Rig.Hands->Step(Step);
		++Guard;
	}
	const FWardEvent ShortRaise = Rig.Wards->GetLastRaise();
	const double DetectorLowered = Rig.Wards->GetDetector().GetLoweredTime();
	const double ShortGap = ShortRaise.OnsetTime - LoweredAt;
	const FAbsorbResult ShortHit = Resolver.Resolve(ShortRaise.OnsetTime + 0.02, EAbsorbHitKind::Magic, 1, Rig.Wards->GetState().Facing, Rig.Wards->GetState());
	bPass &= TestTrue(TEXT("short reverse raised again"), Rig.Wards->GetState().bRaised);
	bPass &= TestTrue(*FString::Printf(TEXT("short onset %.4f is at or after the lower %.4f"), ShortRaise.OnsetTime, DetectorLowered),
		ShortRaise.OnsetTime + 1.0e-9 >= DetectorLowered);
	bPass &= TestTrue(*FString::Printf(TEXT("short onset %.4f is at or after the lower callback %.4f"), ShortRaise.OnsetTime, LoweredAt),
		ShortRaise.OnsetTime + 1.0e-9 >= LoweredAt);
	bPass &= TestFalse(*FString::Printf(TEXT("short gap %.4f is not fresh"), ShortGap), ShortRaise.bFresh);
	bPass &= TestFalse(TEXT("short gap does not perfect"), ShortHit.bPerfect);
	bPass &= TestEqual(TEXT("short gap keeps magic reduction"), ShortHit.Reduction, Rules.ReductionFor(EAbsorbHitKind::Magic));
	UE_LOG(LogMageArena, Log, TEXT("Ward short re-raise gap=%.4f fresh=%d onset=%.4f lowered=%.4f"), ShortGap, ShortRaise.bFresh ? 1 : 0, ShortRaise.OnsetTime, LoweredAt);
	Rig.Finish();

	FWardRig LongRig;
	if (!LongRig.Start(*this))
	{
		bPass = false;
		return false;
	}
	double LongLoweredAt = -1.0;
	LongRig.Wards->OnWardLowered.AddLambda([&LongRig, &LongLoweredAt]()
	{
		if (LongRig.Hands && LongRig.Hands->GetClipPlayer())
		{
			LongLoweredAt = LongRig.Hands->GetClipPlayer()->GetTime();
		}
	});
	LongRig.Hands->SetWardHeld(true, EClipVariant::Normal);
	UHandClipPlayer* LongPlayer = LongRig.Hands->GetClipPlayer();
	if (!LongPlayer)
	{
		AddError(TEXT("long-gap ward did not start"));
		LongRig.Finish();
		bPass = false;
		return false;
	}
	const double LongStep = LongPlayer->GetClip().Hz > 0.0 ? 1.0 / LongPlayer->GetClip().Hz : 1.0 / 72.0;
	Guard = 0;
	while ((LongPlayer->GetTime() < 0.30 || !LongRig.Wards->GetState().bRaised) && Guard < 200)
	{
		LongRig.Hands->Step(LongStep);
		++Guard;
	}
	LongRig.Hands->SetWardHeld(false, EClipVariant::Normal);
	while (LongPlayer->IsPlaying() && Guard < 800)
	{
		LongRig.Hands->Step(LongStep);
		++Guard;
	}
	bPass &= TestFalse(TEXT("full lower finished"), LongPlayer->IsPlaying());
	bPass &= TestFalse(TEXT("full lower dropped the ward"), LongRig.Wards->GetState().bRaised);

	FHandClip Clip;
	if (!LoadWard(Clip, Error))
	{
		AddError(Error);
		LongRig.Finish();
		bPass = false;
		return false;
	}
	FHandFrame Lap;
	if (!Clip.Sample(EControllerHand::Left, 0.0, Lap))
	{
		AddError(TEXT("no lap frame"));
		LongRig.Finish();
		bPass = false;
		return false;
	}
	double Stamp = LongPlayer->GetTime();
	for (int32 Index = 0; Index < 18; ++Index)
	{
		Stamp += LongStep;
		Lap.TimeSeconds = Stamp;
		LongRig.Wards->GetDetector().Ingest(Lap);
	}
	bool bRose = false;
	double Source = 0.0;
	for (int32 Index = 0; Index < 40 && !bRose; ++Index)
	{
		Source += LongStep;
		Stamp += LongStep;
		FHandFrame Frame;
		if (!Clip.Sample(EControllerHand::Left, Source, Frame))
		{
			break;
		}
		Frame.TimeSeconds = Stamp;
		LongRig.Wards->GetDetector().Ingest(Frame);
		bRose = LongRig.Wards->GetState().bRaised;
	}
	const FWardEvent LongRaise = LongRig.Wards->GetLastRaise();
	const double LongGap = LongRaise.OnsetTime - LongLoweredAt;
	const FAbsorbResult LongHit = Resolver.Resolve(LongRaise.OnsetTime + 0.05, EAbsorbHitKind::Magic, 1, LongRig.Wards->GetState().Facing, LongRig.Wards->GetState());
	bPass &= TestTrue(TEXT("long gap raised again"), bRose);
	bPass &= TestTrue(*FString::Printf(TEXT("long gap %.4f is fresh"), LongGap), LongRaise.bFresh && LongGap + 1.0e-9 >= Rules.MinReleaseS);
	bPass &= TestTrue(TEXT("long gap perfects"), LongHit.bPerfect);
	UE_LOG(LogMageArena, Log, TEXT("Ward long re-raise gap=%.4f fresh=%d onset=%.4f lowered=%.4f"), LongGap, LongRaise.bFresh ? 1 : 0, LongRaise.OnsetTime, LongLoweredAt);

	FWardReport& Report = FWardReport::Get();
	Report.bHasFresh = true;
	Report.ShortGapS = ShortGap;
	Report.bShortFresh = ShortRaise.bFresh;
	Report.bShortNotPerfect = !ShortHit.bPerfect;
	Report.LongGapS = LongGap;
	Report.bLongFresh = LongRaise.bFresh;
	Report.bLongPerfect = LongHit.bPerfect;
	LongRig.Finish();
	AddInfo(FString::Printf(TEXT("fresh shortGap %.4f fresh=%d longGap %.4f fresh=%d"), ShortGap, ShortRaise.bFresh ? 1 : 0, LongGap, LongRaise.bFresh ? 1 : 0));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardKey, "MageArena.Ward.KeyHoldPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardKey::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.KeyHoldPath"), bPass);
	FWardRig Rig;
	if (!Rig.Start(*this))
	{
		return false;
	}
	int32 Raises = 0;
	int32 Lowers = 0;
	Rig.Wards->OnWardRaised.AddLambda([&Raises](double, FVector)
	{
		++Raises;
	});
	Rig.Wards->OnWardLowered.AddLambda([&Lowers]()
	{
		++Lowers;
	});

	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	UHandClipPlayer* Player = Rig.Hands->GetClipPlayer();
	if (!Player || !Player->IsPlaying())
	{
		AddError(TEXT("Space hold path did not start ward-raise"));
		Rig.Finish();
		return false;
	}
	const double Plateau = UHandClipPlayer::FindPlateauStart(Player->GetClip(), EControllerHand::Left);
	bPass = TestTrue(*FString::Printf(TEXT("plateau %.3f"), Plateau), Plateau > 0.20 && Plateau < 0.30);
	const double Duration = Player->GetDuration();
	const double Frame = Player->GetClip().Hz > 0.0 ? 1.0 / Player->GetClip().Hz : 1.0 / 72.0;
	int32 Guard = 0;
	while (Player->GetTime() <= Duration + 0.5 && Guard < 500)
	{
		Rig.Hands->Step(Frame);
		++Guard;
	}
	bPass &= TestEqual(TEXT("one OnWardRaised while held"), Raises, 1);
	bPass &= TestEqual(TEXT("no OnWardLowered while held"), Lowers, 0);
	bPass &= TestTrue(TEXT("hold streams past the clip duration"), Player->GetTime() > Duration);
	bPass &= TestTrue(TEXT("hold stays playing"), Player->IsPlaying());
	const uint64 HeldSequence = Player->GetFrameSequence();
	Rig.Hands->Step(Frame);
	bPass &= TestTrue(TEXT("hold stamps a new frame"), Player->GetFrameSequence() > HeldSequence);

	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	Rig.Hands->Step(Frame);
	bPass &= TestEqual(TEXT("a repeat hold is not a second raise"), Raises, 1);

	Rig.Hands->SetWardHeld(false, EClipVariant::Normal);
	Guard = 0;
	while (Player->IsPlaying() && Guard < 200)
	{
		Rig.Hands->Step(Frame);
		++Guard;
	}
	bPass &= TestFalse(TEXT("lower finished"), Player->IsPlaying());
	bPass &= TestEqual(TEXT("still one OnWardRaised"), Raises, 1);
	bPass &= TestEqual(TEXT("one OnWardLowered"), Lowers, 1);
	const uint64 Frozen = Player->GetFrameSequence();
	Rig.Hands->Step(0.1);
	Rig.Hands->Step(0.5);
	bPass &= TestTrue(TEXT("sequence frozen after the lower"), Player->GetFrameSequence() == Frozen);
	UE_LOG(LogMageArena, Log, TEXT("Ward key path raises=%d lowers=%d plateau=%.3f"), Raises, Lowers, Plateau);
	AddInfo(FString::Printf(TEXT("key path raises %d lowers %d plateau %.3f"), Raises, Lowers, Plateau));
	Rig.Finish();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardOnsetAfterLower, "MageArena.Ward.OnsetAfterLower",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardOnsetAfterLower::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.OnsetAfterLower"), bPass);
	FAbsorbResolver Resolver;
	FString Error;
	if (!Resolver.Init(Error))
	{
		AddError(Error);
		return false;
	}
	const FAbsorbRules& Rules = Resolver.Rules();

	FWardRig Rig;
	if (!Rig.Start(*this))
	{
		return false;
	}
	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	UHandClipPlayer* Player = Rig.Hands->GetClipPlayer();
	if (!Player)
	{
		AddError(TEXT("rapid re-raise did not start"));
		Rig.Finish();
		return false;
	}
	const double Step = Player->GetClip().Hz > 0.0 ? 1.0 / Player->GetClip().Hz : 1.0 / 72.0;
	int32 Guard = 0;
	while ((Player->GetTime() < 0.30 || !Rig.Wards->GetState().bRaised) && Guard < 200)
	{
		Rig.Hands->Step(Step);
		++Guard;
	}
	Rig.Hands->SetWardHeld(false, EClipVariant::Normal);
	while (Rig.Wards->GetState().bRaised && Guard < 400)
	{
		Rig.Hands->Step(Step);
		++Guard;
	}
	bPass = TestFalse(TEXT("rapid lower left the raised pose"), Rig.Wards->GetState().bRaised);
	const double Lowered = Rig.Wards->GetDetector().GetLoweredTime();
	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	while (!Rig.Wards->GetState().bRaised && Guard < 600)
	{
		Rig.Hands->Step(Step);
		++Guard;
	}
	const FWardEvent Rapid = Rig.Wards->GetLastRaise();
	const double RapidGap = Rapid.OnsetTime - Lowered;
	const bool bGapFresh = RapidGap + 1.0e-9 >= Rules.MinReleaseS;
	const FAbsorbResult RapidHit = Resolver.Resolve(Rapid.OnsetTime + 0.02, EAbsorbHitKind::Magic, 1, Rig.Wards->GetState().Facing, Rig.Wards->GetState());
	bPass &= TestTrue(TEXT("rapid re-raise reached the pose"), Rig.Wards->GetState().bRaised);
	bPass &= TestTrue(*FString::Printf(TEXT("rapid onset %.4f is at or after the lower %.4f"), Rapid.OnsetTime, Lowered),
		Rapid.OnsetTime + 1.0e-9 >= Lowered);
	bPass &= TestTrue(*FString::Printf(TEXT("rapid gap %.4f decides freshness (%d, rule %d)"), RapidGap, Rapid.bFresh ? 1 : 0, bGapFresh ? 1 : 0),
		Rapid.bFresh == bGapFresh);
	bPass &= TestTrue(*FString::Printf(TEXT("rapid gap %.4f is under the release minimum %.4f"), RapidGap, Rules.MinReleaseS),
		RapidGap + 1.0e-9 < Rules.MinReleaseS);
	bPass &= TestFalse(TEXT("rapid re-raise does not perfect"), RapidHit.bPerfect);
	FWardReport& Report = FWardReport::Get();
	Report.RapidGapS = RapidGap;
	Report.RapidOnsetS = Rapid.OnsetTime;
	Report.RapidLoweredS = Lowered;
	Report.RapidFresh = Rapid.bFresh ? 1 : 0;
	UE_LOG(LogMageArena, Log, TEXT("Ward rapid re-raise gap=%.4f onset=%.4f lowered=%.4f fresh=%d"),
		RapidGap, Rapid.OnsetTime, Lowered, Rapid.bFresh ? 1 : 0);
	AddInfo(FString::Printf(TEXT("rapid gap %.4f onset %.4f lowered %.4f fresh %d"), RapidGap, Rapid.OnsetTime, Lowered, Rapid.bFresh ? 1 : 0));
	Rig.Finish();

	FWardRig LongRig;
	if (!LongRig.Start(*this))
	{
		bPass = false;
		return false;
	}
	LongRig.Hands->SetWardHeld(true, EClipVariant::Normal);
	UHandClipPlayer* LongPlayer = LongRig.Hands->GetClipPlayer();
	if (!LongPlayer)
	{
		AddError(TEXT("released re-raise did not start"));
		LongRig.Finish();
		bPass = false;
		return false;
	}
	const double LongStep = LongPlayer->GetClip().Hz > 0.0 ? 1.0 / LongPlayer->GetClip().Hz : 1.0 / 72.0;
	Guard = 0;
	while ((LongPlayer->GetTime() < 0.30 || !LongRig.Wards->GetState().bRaised) && Guard < 200)
	{
		LongRig.Hands->Step(LongStep);
		++Guard;
	}
	LongRig.Hands->SetWardHeld(false, EClipVariant::Normal);
	while (LongPlayer->IsPlaying() && Guard < 800)
	{
		LongRig.Hands->Step(LongStep);
		++Guard;
	}
	bPass &= TestFalse(TEXT("full lower finished playing"), LongPlayer->IsPlaying());
	const double LongLowered = LongRig.Wards->GetDetector().GetLoweredTime();
	FHandClip Clip;
	if (!LoadWard(Clip, Error))
	{
		AddError(Error);
		LongRig.Finish();
		bPass = false;
		return false;
	}
	FHandFrame Lap;
	if (!Clip.Sample(EControllerHand::Left, 0.0, Lap))
	{
		AddError(TEXT("no lap frame for the released re-raise"));
		LongRig.Finish();
		bPass = false;
		return false;
	}
	double Stamp = LongPlayer->GetTime();
	for (int32 Index = 0; Index < 18; ++Index)
	{
		Stamp += LongStep;
		Lap.TimeSeconds = Stamp;
		LongRig.Wards->GetDetector().Ingest(Lap);
	}
	bool bRose = false;
	double Source = 0.0;
	for (int32 Index = 0; Index < 40 && !bRose; ++Index)
	{
		Source += LongStep;
		Stamp += LongStep;
		FHandFrame Frame;
		if (!Clip.Sample(EControllerHand::Left, Source, Frame))
		{
			break;
		}
		Frame.TimeSeconds = Stamp;
		LongRig.Wards->GetDetector().Ingest(Frame);
		bRose = LongRig.Wards->GetState().bRaised;
	}
	const FWardEvent Released = LongRig.Wards->GetLastRaise();
	const double ReleasedGap = Released.OnsetTime - LongLowered;
	const FAbsorbResult ReleasedHit = Resolver.Resolve(Released.OnsetTime + 0.05, EAbsorbHitKind::Magic, 1, LongRig.Wards->GetState().Facing, LongRig.Wards->GetState());
	bPass &= TestTrue(TEXT("released re-raise reached the pose"), bRose);
	bPass &= TestTrue(*FString::Printf(TEXT("released onset %.4f is at or after the lower %.4f"), Released.OnsetTime, LongLowered),
		Released.OnsetTime + 1.0e-9 >= LongLowered);
	bPass &= TestTrue(*FString::Printf(TEXT("released gap %.4f is at least the minimum"), ReleasedGap),
		Released.bFresh && ReleasedGap + 1.0e-9 >= Rules.MinReleaseS);
	bPass &= TestTrue(TEXT("released re-raise perfects"), ReleasedHit.bPerfect);
	UE_LOG(LogMageArena, Log, TEXT("Ward released re-raise gap=%.4f onset=%.4f lowered=%.4f fresh=%d"),
		ReleasedGap, Released.OnsetTime, LongLowered, Released.bFresh ? 1 : 0);
	AddInfo(FString::Printf(TEXT("released gap %.4f onset %.4f lowered %.4f"), ReleasedGap, Released.OnsetTime, LongLowered));
	LongRig.Finish();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardSettle, "MageArena.Ward.SettleOnset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardSettle::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.SettleOnset"), bPass);
	FAbsorbResolver Resolver;
	FWardDetector Detector;
	if (!PrepareRules(*this, Resolver, Detector))
	{
		return false;
	}
	FHandClip Clip;
	FString Error;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("ward"), TEXT("ward-raise.normal.settle.jsonl"));
	if (!FHandClip::LoadFromFile(Path, Clip, Error))
	{
		AddError(Error);
		return false;
	}
	const FHandClipTrack* Track = LeftTrack(Clip);
	if (Track == nullptr || Clip.Hz <= 0.0)
	{
		AddError(TEXT("ward-raise.normal.settle has no left-hand track"));
		return false;
	}

	const double Frame = 1.0 / Clip.Hz;
	const double GeoOnset = GeometricOnset(*Track);
	const double GeoConfirm = GeometricConfirm(*Track);
	bPass = true;
	bPass &= TestTrue(TEXT("settle clip has a palm-speed onset"), GeoOnset >= 0.0);
	bPass &= TestTrue(*FString::Printf(TEXT("settle confirm %.4f is after a long still from onset %.4f"), GeoConfirm, GeoOnset),
		GeoConfirm - GeoOnset > 0.40);
	if (!bPass)
	{
		return false;
	}

	double LastFast = GeoOnset;
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	for (int32 Index = 1; Index < Track->Frames.Num(); ++Index)
	{
		const FHandFrame& Prev = Track->Frames[Index - 1];
		const FHandFrame& FrameSample = Track->Frames[Index];
		if (!FrameSample.Joints.IsValidIndex(Palm) || !Prev.Joints.IsValidIndex(Palm))
		{
			continue;
		}
		if (FrameSample.TimeSeconds > GeoConfirm + 1.0e-9)
		{
			break;
		}
		const double Dt = FrameSample.TimeSeconds - Prev.TimeSeconds;
		if (Dt < 1.0e-6)
		{
			continue;
		}
		const double Speed = FVector::Distance(FrameSample.Joints[Palm].Location, Prev.Joints[Palm].Location) / MageClipMetresToCentimetres / Dt;
		if (Speed >= FWardThresholds::OnsetSpeedMps)
		{
			LastFast = FrameSample.TimeSeconds;
		}
	}

	const double Anchor = GeoOnset + 2.0 * Frame;
	if (!(GeoConfirm > Anchor))
	{
		AddError(TEXT("settle recognition is not after the onset anchor"));
		bPass = false;
		return false;
	}
	const double Latency60 = 0.060;
	const double Scale = (GeoConfirm - Anchor) / (GeoConfirm - Anchor + Latency60);
	const double WarpedConfirm = Anchor + (GeoConfirm - Anchor) / Scale;
	const double WarpedFast = LastFast <= Anchor ? LastFast : Anchor + (LastFast - Anchor) / Scale;
	const double Gap60 = WarpedConfirm - WarpedFast;
	FWardReport::Get().SettleGap60S = Gap60;
	bPass &= TestTrue(*FString::Printf(TEXT("onset tail %.6f is the 60 ms source gap %.6f rounded to a frame (last fast %.4f)"),
		FWardThresholds::OnsetTailS, Gap60, LastFast),
		FMath::Abs(FWardThresholds::OnsetTailS - Gap60) <= (0.5 * Frame) + 1.0e-4);
	bPass &= TestTrue(*FString::Printf(TEXT("60 ms source gap %.4f is longer than an 80 ms tail"), Gap60), Gap60 > 0.30);
	UE_LOG(LogMageArena, Log, TEXT("Ward settle tail=%.6f gap60=%.6f lastFast=%.4f confirm=%.4f onset=%.4f"),
		FWardThresholds::OnsetTailS, Gap60, LastFast, GeoConfirm, GeoOnset);
	AddInfo(FString::Printf(TEXT("settle tail %.6f gap60 %.6f lastFast %.4f confirm %.4f onset %.4f"),
		FWardThresholds::OnsetTailS, Gap60, LastFast, GeoConfirm, GeoOnset));

	const double Window = Resolver.Rules().WindowS;
	const double AuthoredTick = GeoOnset + Window;
	const double Latencies[] = { 0.0, 0.020, 0.040, 0.060, 0.080, 0.100 };
	for (const double Latency : Latencies)
	{
		const double TrialScale = (GeoConfirm - Anchor) / (GeoConfirm - Anchor + Latency);
		FWardDetector Trial;
		FString Ignored;
		if (!Trial.Init(Ignored))
		{
			AddError(TEXT("settle detector lost the pinned rules"));
			bPass = false;
			return false;
		}
		double Output = 0.0;
		Feed(Trial, Clip, 0.0, 0.0);
		int32 Guard = 0;
		while (!Trial.GetState().bRaised && Output < GeoConfirm + Latency + 0.5 && Guard < 500)
		{
			Output += Frame;
			double Source = Output;
			if (Output > Anchor)
			{
				Source = Anchor + (Output - Anchor) * TrialScale;
			}
			Source = FMath::Clamp(Source, 0.0, Clip.GetDuration());
			Feed(Trial, Clip, Source, Output);
			++Guard;
		}

		FLatencyRow Row;
		Row.LatencyS = Latency;
		Row.bRaised = Trial.GetState().bRaised;
		Row.ConfirmS = Row.bRaised ? Output : -1.0;
		Row.OnsetS = Row.bRaised ? Trial.GetLastRaise().OnsetTime : -1.0;
		Row.ErrorS = Row.bRaised ? Row.OnsetS - GeoOnset : 0.0;
		Row.ErrorFrames = Frame > 0.0 ? Row.ErrorS / Frame : 0.0;
		Row.ConfirmShiftS = Row.bRaised ? Row.ConfirmS - GeoConfirm : 0.0;
		Row.HitS = AuthoredTick;
		Row.bWithinOneFrame = Row.bRaised && FMath::Abs(Row.ErrorS) <= Frame + 1.0e-6;
		Row.bConfirmTracked = Row.bRaised && FMath::Abs(Row.ConfirmShiftS - Latency) <= Frame + 1.0e-4;

		const FWardState State = Trial.GetState();
		const FAbsorbResult AtTick = Resolver.Resolve(AuthoredTick, EAbsorbHitKind::Magic, 3, State.Facing, State);
		FAbsorbResult Before;
		Before.bPerfect = false;
		if (Row.bRaised)
		{
			Before = Resolver.Resolve(AuthoredTick - Frame, EAbsorbHitKind::Magic, 3, State.Facing, State);
		}
		const bool bEarly = Row.ErrorS < -1.0e-9;
		const bool bTiming = Row.bWithinOneFrame && (AtTick.bPerfect || (bEarly && Before.bPerfect));
		Row.bPerfect = AtTick.bPerfect;
		FWardReport::Get().SettleRows.Add(Row);

		const FString Line = FString::Printf(TEXT("settle latency %5.1f ms confirm=%.4f onset=%.4f error=%.4f ms frames=%.2f shift=%.4f perfect=%d tracked=%d fresh=%d"),
			Latency * 1000.0, Row.ConfirmS, Row.OnsetS, Row.ErrorS * 1000.0, Row.ErrorFrames, Row.ConfirmShiftS,
			AtTick.bPerfect ? 1 : 0, Row.bConfirmTracked ? 1 : 0, Trial.GetLastRaise().bFresh ? 1 : 0);
		UE_LOG(LogMageArena, Log, TEXT("Ward %s"), *Line);
		AddInfo(Line);

		const bool bGate = Latency <= 0.060 + 1.0e-9;
		if (bGate)
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s within one frame"), *Line), bTiming);
			bPass &= TestTrue(*FString::Printf(TEXT("%s confirm tracks the injected latency"), *Line), Row.bConfirmTracked);
			bPass &= TestTrue(*FString::Printf(TEXT("%s raise is fresh"), *Line), Trial.GetLastRaise().bFresh);
		}
		else
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s still raises"), *Line), Row.bRaised);
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardAlreadyRaised, "MageArena.Ward.AlreadyRaised",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardAlreadyRaised::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.AlreadyRaised"), bPass);
	FAbsorbResolver Resolver;
	FWardDetector Detector;
	if (!PrepareRules(*this, Resolver, Detector))
	{
		return false;
	}
	FHandClip Clip;
	FString Error;
	if (!LoadWard(Clip, Error))
	{
		AddError(Error);
		return false;
	}
	const double Plateau = UHandClipPlayer::FindPlateauStart(Clip, EControllerHand::Left);
	FHandFrame Pose;
	if (!Clip.Sample(EControllerHand::Left, Plateau, Pose))
	{
		AddError(TEXT("ward plateau has no pose"));
		return false;
	}
	const double Angle = FWardDetector::AngleToFacingDeg(FWardDetector::PalmNormal(Pose), FVector::ForwardVector);
	const double Height = FWardDetector::PalmHeightMetres(Pose);
	bPass = TestTrue(*FString::Printf(TEXT("plateau starts raised (angle %.2f height %.3f)"), Angle, Height),
		Angle <= FWardThresholds::RaiseAngleDeg && Height >= FWardThresholds::RaiseHeightM);
	if (!bPass)
	{
		return false;
	}

	const double Frame = Clip.Hz > 0.0 ? 1.0 / Clip.Hz : 1.0 / 72.0;
	int32 Raises = 0;
	Detector.OnRaised.AddLambda([&Raises](double, FVector)
	{
		++Raises;
	});
	// A few identical frames establish the pose. Bolt stamps stay after them:
	// a stamp behind the last sample is a rewind and would open a second raise.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Pose.TimeSeconds = Index * Frame;
		Detector.Ingest(Pose);
	}
	bPass &= TestEqual(TEXT("one raise from an already-raised stream"), Raises, 1);
	bPass &= TestTrue(TEXT("already-raised stream stays raised"), Detector.GetState().bRaised);
	bPass &= TestFalse(TEXT("already-raised stream was not seen lowered"), Detector.HasSeenLowered());
	bPass &= TestFalse(TEXT("already-raised stream is not fresh"), Detector.GetLastRaise().bFresh);

	const double Onset = Detector.GetState().OnsetTime;
	const double Hits[] = { Onset + 0.10, Onset + 0.22, Onset + 0.42 };
	bPass &= TestTrue(TEXT("the first bolt is inside the perfect window"), Hits[0] <= Onset + Resolver.Rules().WindowS + 1.0e-9);
	bPass &= TestTrue(TEXT("the first bolt is after the establishing frames"), Hits[0] > 3.0 * Frame);
	int32 Perfects = 0;
	for (int32 Bolt = 0; Bolt < UE_ARRAY_COUNT(Hits); ++Bolt)
	{
		Pose.TimeSeconds = Hits[Bolt];
		Detector.Ingest(Pose);
		const FWardState State = Detector.GetState();
		const FAbsorbResult Hit = Resolver.Resolve(Hits[Bolt], EAbsorbHitKind::Magic, 2, State.Facing, State);
		if (Hit.bPerfect)
		{
			++Perfects;
		}
		bPass &= TestFalse(*FString::Printf(TEXT("already-raised bolt %d perfect"), Bolt), Hit.bPerfect);
		bPass &= TestEqual(*FString::Printf(TEXT("already-raised bolt %d magic reduction"), Bolt), Hit.Reduction, Resolver.Rules().ReductionFor(EAbsorbHitKind::Magic));
		bPass &= TestTrue(*FString::Printf(TEXT("already-raised bolt %d keeps the original onset"), Bolt), FMath::Abs(State.OnsetTime - Onset) <= 1.0e-6);
	}
	bPass &= TestEqual(TEXT("already-raised stream perfects nothing"), Perfects, 0);
	bPass &= TestEqual(TEXT("bolts did not raise again"), Raises, 1);
	FWardReport::Get().AlreadyPerfects = Perfects;
	FWardReport::Get().AlreadyFresh = Detector.GetLastRaise().bFresh ? 1 : 0;
	UE_LOG(LogMageArena, Log, TEXT("Ward already raised onset=%.4f fresh=%d perfects=%d"), Onset, Detector.GetLastRaise().bFresh ? 1 : 0, Perfects);
	AddInfo(FString::Printf(TEXT("already raised onset %.4f perfects %d"), Onset, Perfects));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardResume, "MageArena.Ward.ResumeAfterRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardResume::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.ResumeAfterRelease"), bPass);
	FHandClip NormalClip;
	FHandClip SloppyClip;
	FString Error;
	if (!FHandClip::LoadFromFile(FHandClip::MakeFilePath(TEXT("ward-raise"), EClipVariant::Normal), NormalClip, Error)
		|| !FHandClip::LoadFromFile(FHandClip::MakeFilePath(TEXT("ward-raise"), EClipVariant::Sloppy), SloppyClip, Error))
	{
		AddError(Error);
		return false;
	}
	bPass = TestTrue(TEXT("sloppy ward-raise has its own seed"), NormalClip.Seed != SloppyClip.Seed);
	if (!bPass)
	{
		return false;
	}

	FWardRig Rig;
	if (!Rig.Start(*this))
	{
		return false;
	}
	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	UHandClipPlayer* Player = Rig.Hands->GetClipPlayer();
	if (!Player || !Player->IsPlaying())
	{
		AddError(TEXT("resume test hold did not start"));
		Rig.Finish();
		return false;
	}
	const double Frame = Player->GetClip().Hz > 0.0 ? 1.0 / Player->GetClip().Hz : 1.0 / 72.0;
	int32 Guard = 0;
	while ((Player->GetTime() < 0.30 || !Rig.Wards->GetState().bRaised) && Guard < 200)
	{
		Rig.Hands->Step(Frame);
		++Guard;
	}
	bPass &= TestTrue(TEXT("hold reached the raised pose"), Rig.Wards->GetState().bRaised);
	Rig.Hands->SetWardHeld(false, EClipVariant::Normal);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Rig.Hands->Step(Frame);
	}
	bPass &= TestTrue(TEXT("a mid-lower press can resume"), Player->CanResumeHold());
	const double MidTime = Player->GetTime();
	Rig.Hands->SetWardHeld(true, EClipVariant::Sloppy);
	bPass &= TestTrue(*FString::Printf(TEXT("mid-lower resume keeps the clock at %.4f"), Player->GetTime()),
		FMath::Abs(Player->GetTime() - MidTime) <= 1.0e-6);
	bPass &= TestTrue(TEXT("mid-lower resume keeps the normal clip"), Player->GetClip().Variant == TEXT("normal"));
	bPass &= TestTrue(TEXT("mid-lower resume keeps the normal seed"), Player->GetClip().Seed == NormalClip.Seed);
	Rig.Hands->Step(Frame);
	bPass &= TestTrue(TEXT("mid-lower resume is still the held ward"), Rig.Hands->IsWardHeld());

	Rig.Hands->SetWardHeld(false, EClipVariant::Normal);
	Guard = 0;
	while (Player->IsPlaying() && Guard < 400)
	{
		Rig.Hands->Step(Frame);
		++Guard;
	}
	bPass &= TestFalse(TEXT("the lower ran to the end"), Player->IsPlaying());
	bPass &= TestFalse(TEXT("a finished lower cannot resume"), Player->CanResumeHold());
	bPass &= TestTrue(TEXT("the finished lower left the clock away from zero"), Player->GetTime() > 0.20);
	Rig.Hands->SetWardHeld(true, EClipVariant::Sloppy);
	bPass &= TestTrue(*FString::Printf(TEXT("the next hold reloads at %.4f"), Player->GetTime()), Player->GetTime() <= 1.0e-6);
	bPass &= TestTrue(*FString::Printf(TEXT("reloaded variant is %s"), *Player->GetClip().Variant), Player->GetClip().Variant == TEXT("sloppy"));
	bPass &= TestTrue(TEXT("reloaded seed is the sloppy clip"), Player->GetClip().Seed == SloppyClip.Seed);
	bPass &= TestTrue(TEXT("reloaded clip is not the normal seed"), Player->GetClip().Seed != NormalClip.Seed);
	bPass &= TestTrue(TEXT("reloaded hold is playing"), Player->IsPlaying());
	FWardReport::Get().ResumeTimeS = Player->GetTime();
	FWardReport::Get().ResumeVariant = Player->GetClip().Variant;
	UE_LOG(LogMageArena, Log, TEXT("Ward resume variant=%s seed=%lld time=%.4f"),
		*Player->GetClip().Variant, Player->GetClip().Seed, Player->GetTime());
	AddInfo(FString::Printf(TEXT("resume variant %s time %.4f"), *Player->GetClip().Variant, Player->GetTime()));
	Rig.Finish();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardChord, "MageArena.Ward.HoldChord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardChord::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.HoldChord"), bPass);

	AMageArenaPlayerController* Controller = NewObject<AMageArenaPlayerController>(GetTransientPackage());
	if (!Controller)
	{
		AddError(TEXT("could not construct the player controller"));
		return false;
	}
	Controller->AddToRoot();
	Controller->EnsureMapping();
	const bool bSpaceConsumes = Controller->IsQuickActionConsuming(EKeys::SpaceBar);
	FWardReport::Get().SpaceConsumes = bSpaceConsumes ? 1 : 0;
	bPass = TestFalse(TEXT("the Space hold action does not consume the key"), bSpaceConsumes);
	bPass &= TestTrue(TEXT("Q still consumes its key"), Controller->IsQuickActionConsuming(EKeys::Q));
	bPass &= TestTrue(TEXT("1 still consumes its key"), Controller->IsQuickActionConsuming(EKeys::One));
	Controller->RemoveFromRoot();
	Controller->MarkAsGarbage();

	FWardHoldKeys None;
	FWardHoldKeys Space;
	Space.bSpaceDown = true;
	FWardHoldKeys Both;
	Both.bSpaceDown = true;
	Both.bRightDown = true;

	const FWardHoldStep Pressed = StepWardHold(None, Space);
	bPass &= TestTrue(TEXT("space down presses"), Pressed.bPress && Pressed.bHeld);
	bPass &= TestFalse(TEXT("space down does not aim yet"), Pressed.bAim);
	bPass &= TestFalse(TEXT("space aims from the view"), Pressed.bAimFromCursor);
	const FWardHoldStep Held = StepWardHold(Space, Space);
	bPass &= TestTrue(TEXT("space held aims without a second press"), Held.bAim && !Held.bPress && !Held.bRelease);
	bPass &= TestFalse(TEXT("space held aims from the view"), Held.bAimFromCursor);
	const FWardHoldStep Added = StepWardHold(Space, Both);
	bPass &= TestTrue(TEXT("adding the right button keeps the hold and aims at the cursor"),
		Added.bHeld && Added.bAim && Added.bAimFromCursor && !Added.bPress && !Added.bRelease);
	const FWardHoldStep DroppedRight = StepWardHold(Both, Space);
	bPass &= TestTrue(TEXT("releasing the right button leaves space holding"),
		DroppedRight.bHeld && !DroppedRight.bRelease && DroppedRight.bAim && !DroppedRight.bAimFromCursor);
	const FWardHoldStep Released = StepWardHold(Space, None);
	bPass &= TestTrue(TEXT("releasing space releases the chord"), Released.bRelease && !Released.bHeld && !Released.bAim);

	const FVector Forward = FVector::ForwardVector;
	const FVector Right = FVector::RightVector;
	bPass &= TestTrue(TEXT("space aim follows the view"), ResolveWardAim(false, FVector(0.0, 1.0, 1.0), Forward).Equals(Forward, 1.0e-4));
	bPass &= TestTrue(TEXT("a new view moves the aim"), ResolveWardAim(false, FVector(0.0, 1.0, 1.0), Right).Equals(Right, 1.0e-4));
	bPass &= TestTrue(TEXT("the cursor wins while the right button aims"), ResolveWardAim(true, FVector(0.0, 1.0, 0.5), Forward).Equals(Right, 1.0e-4));
	bPass &= TestTrue(TEXT("a vertical cursor falls back to the view"), ResolveWardAim(true, FVector(0.0, 0.0, -1.0), Right).Equals(Right, 1.0e-4));
	bPass &= TestTrue(TEXT("a vertical view falls back to +X"), ResolveWardAim(false, FVector::ZeroVector, FVector(0.0, 0.0, 1.0)).Equals(Forward, 1.0e-4));

	FWardRig Rig;
	if (!Rig.Start(*this))
	{
		bPass = false;
		return false;
	}
	int32 Raises = 0;
	int32 Lowers = 0;
	Rig.Wards->OnWardRaised.AddLambda([&Raises](double, FVector)
	{
		++Raises;
	});
	Rig.Wards->OnWardLowered.AddLambda([&Lowers]()
	{
		++Lowers;
	});
	auto Apply = [&Rig](const FWardHoldStep& Step)
	{
		if (Step.bPress)
		{
			Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
		}
		else if (Step.bRelease)
		{
			Rig.Hands->SetWardHeld(false, EClipVariant::Normal);
		}
	};

	Apply(Pressed);
	UHandClipPlayer* Player = Rig.Hands->GetClipPlayer();
	if (!Player || !Player->IsPlaying())
	{
		AddError(TEXT("chord press did not start the ward"));
		Rig.Finish();
		bPass = false;
		return false;
	}
	const double Frame = Player->GetClip().Hz > 0.0 ? 1.0 / Player->GetClip().Hz : 1.0 / 72.0;
	int32 Guard = 0;
	while ((Player->GetTime() < 0.30 || !Rig.Wards->GetState().bRaised) && Guard < 200)
	{
		Rig.Hands->Step(Frame);
		++Guard;
	}
	bPass &= TestEqual(TEXT("chord press raises once"), Raises, 1);
	bPass &= TestTrue(TEXT("chord press is raised"), Rig.Wards->GetState().bRaised);

	Apply(Added);
	Rig.Hands->Step(Frame);
	bPass &= TestEqual(TEXT("adding the right button does not raise again"), Raises, 1);
	bPass &= TestEqual(TEXT("adding the right button does not lower"), Lowers, 0);
	bPass &= TestTrue(TEXT("ward stays up with both buttons"), Rig.Wards->GetState().bRaised);

	Apply(DroppedRight);
	Rig.Hands->Step(Frame);
	bPass &= TestTrue(TEXT("releasing the right button does not drop the ward"), Rig.Wards->GetState().bRaised);
	bPass &= TestEqual(TEXT("releasing the right button is not a lower"), Lowers, 0);
	bPass &= TestTrue(TEXT("space is still the active hold"), Rig.Hands->IsWardHeld());

	Apply(Released);
	Guard = 0;
	while (Player->IsPlaying() && Guard < 400)
	{
		Rig.Hands->Step(Frame);
		++Guard;
	}
	bPass &= TestFalse(TEXT("releasing space lowers the ward"), Rig.Wards->GetState().bRaised);
	bPass &= TestEqual(TEXT("releasing space lowers once"), Lowers, 1);
	bPass &= TestEqual(TEXT("the chord raised once"), Raises, 1);
	UE_LOG(LogMageArena, Log, TEXT("Ward chord raises=%d lowers=%d spaceConsumes=%d"), Raises, Lowers, bSpaceConsumes ? 1 : 0);
	AddInfo(FString::Printf(TEXT("chord raises %d lowers %d spaceConsumes %d"), Raises, Lowers, bSpaceConsumes ? 1 : 0));
	Rig.Finish();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardLapStill, "MageArena.Ward.LapStillOnset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardLapStill::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.LapStillOnset"), bPass);
	FAbsorbResolver Resolver;
	FWardDetector Detector;
	if (!PrepareRules(*this, Resolver, Detector))
	{
		return false;
	}
	FHandClip Clip;
	FString Error;
	if (!LoadWard(Clip, Error))
	{
		AddError(Error);
		return false;
	}
	const FHandClipTrack* Track = LeftTrack(Clip);
	if (Track == nullptr || Clip.Hz <= 0.0)
	{
		AddError(TEXT("ward-raise.normal has no left-hand track"));
		return false;
	}
	const double Dt = 1.0 / Clip.Hz;
	const double Geo = GeometricOnset(*Track);
	const double Plateau = UHandClipPlayer::FindPlateauStart(Clip, EControllerHand::Left);
	FHandFrame Hold;
	FHandFrame Lap;
	if (!Clip.Sample(EControllerHand::Left, Plateau, Hold) || !Clip.Sample(EControllerHand::Left, 0.0, Lap))
	{
		AddError(TEXT("ward-raise.normal has no hold or lap pose"));
		return false;
	}
	bPass = TestTrue(TEXT("clip onset is a real speed crossing"), Geo > 0.0);
	if (!bPass)
	{
		return false;
	}

	FLapPrelude Prelude;
	if (!PreludeLapTwitch(*this, Detector, Hold, Lap, Dt, Prelude))
	{
		bPass = false;
		return false;
	}
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	double TwitchStepM = 0.0;
	{
		FHandFrame Twitch = Moved(Lap, Prelude.TwitchTime, FVector(0.0, 4.0, 0.0));
		TwitchStepM = FVector::Distance(Twitch.Joints[Palm].Location, Lap.Joints[Palm].Location) / MageClipMetresToCentimetres;
	}
	bPass &= TestTrue(*FString::Printf(TEXT("lap twitch speed %.2f m/s clears the onset"), TwitchStepM / Dt),
		TwitchStepM / Dt >= FWardThresholds::OnsetSpeedMps);

	double T = Prelude.Time;
	double Source = 0.0;
	double Stamp0 = -1.0;
	int32 Guard = 0;
	while (!Detector.GetState().bRaised && Source <= Clip.GetDuration() && Guard < 80)
	{
		T += Dt;
		FHandFrame Frame;
		if (!Clip.Sample(EControllerHand::Left, Source, Frame))
		{
			break;
		}
		Frame.TimeSeconds = T;
		Frame.Hand = EControllerHand::Left;
		if (Stamp0 < 0.0)
		{
			Stamp0 = T;
		}
		Detector.Ingest(Frame);
		if (Detector.GetState().bRaised)
		{
			break;
		}
		Source += Dt;
		++Guard;
	}
	const double Expected = Stamp0 + Geo;
	const FWardEvent Raised = Detector.GetLastRaise();
	const double Gap = Raised.OnsetTime - Prelude.LoweredTime;
	bPass &= TestTrue(TEXT("twitch-then-still raised again"), Detector.GetState().bRaised);
	bPass &= TestTrue(*FString::Printf(TEXT("onset %.4f is the raise (%.4f), not the twitch %.4f"), Raised.OnsetTime, Expected, Prelude.TwitchTime),
		Detector.GetState().bRaised && FMath::Abs(Raised.OnsetTime - Expected) <= Dt + 1.0e-6);
	bPass &= TestTrue(*FString::Printf(TEXT("onset %.4f is after the twitch %.4f"), Raised.OnsetTime, Prelude.TwitchTime),
		Raised.OnsetTime > Prelude.TwitchTime + 0.10);
	bPass &= TestTrue(*FString::Printf(TEXT("gap %.4f stays above the release minimum"), Gap), Gap > Resolver.Rules().MinReleaseS);
	bPass &= TestTrue(TEXT("the raise after the lap twitch is fresh"), Raised.bFresh);
	UE_LOG(LogMageArena, Log, TEXT("Ward lap still fast onset=%.4f expected=%.4f twitch=%.4f gap=%.4f fresh=%d"),
		Raised.OnsetTime, Expected, Prelude.TwitchTime, Gap, Raised.bFresh ? 1 : 0);
	AddInfo(FString::Printf(TEXT("lap still fast onset %.4f expected %.4f gap %.4f fresh %d"),
		Raised.OnsetTime, Expected, Gap, Raised.bFresh ? 1 : 0));

	// A later arrival whose average palm speed stays under the onset threshold. Bridging that
	// slow sample lands in the lap and expires the perfect window before the hand is up.
	FWardDetector Slow;
	if (!Slow.Init(Error))
	{
		AddError(Error);
		bPass = false;
		return false;
	}
	FLapPrelude SlowPrelude;
	if (!PreludeLapTwitch(*this, Slow, Hold, Lap, Dt, SlowPrelude))
	{
		bPass = false;
		return false;
	}
	const double GapS = 1.20;
	const double ArrivalT = SlowPrelude.Time + GapS;
	const double DistM = FVector::Distance(Hold.Joints[Palm].Location, Lap.Joints[Palm].Location) / MageClipMetresToCentimetres;
	const double ArrivalSpeed = DistM / GapS;
	bPass &= TestTrue(*FString::Printf(TEXT("slow arrival %.3f m/s stays under the onset"), ArrivalSpeed),
		ArrivalSpeed < FWardThresholds::OnsetSpeedMps);
	FHandFrame Arrival = AtTime(Hold, ArrivalT);
	Slow.Ingest(Arrival);
	const FWardEvent SlowRaise = Slow.GetLastRaise();
	const double SlowGap = SlowRaise.OnsetTime - SlowPrelude.LoweredTime;
	bPass &= TestTrue(TEXT("slow arrival reached the pose"), Slow.GetState().bRaised);
	bPass &= TestTrue(*FString::Printf(TEXT("slow onset %.4f is the arrival %.4f, not the lap"), SlowRaise.OnsetTime, ArrivalT),
		Slow.GetState().bRaised && FMath::Abs(SlowRaise.OnsetTime - ArrivalT) <= 1.0e-4);
	bPass &= TestTrue(*FString::Printf(TEXT("slow gap %.4f is past the release minimum"), SlowGap), SlowGap > Resolver.Rules().MinReleaseS);
	bPass &= TestTrue(TEXT("slow arrival is fresh"), SlowRaise.bFresh);
	UE_LOG(LogMageArena, Log, TEXT("Ward lap still slow onset=%.4f arrival=%.4f lap=%.4f gap=%.4f fresh=%d"),
		SlowRaise.OnsetTime, ArrivalT, SlowPrelude.Time, SlowGap, SlowRaise.bFresh ? 1 : 0);
	AddInfo(FString::Printf(TEXT("lap still slow onset %.4f arrival %.4f gap %.4f"), SlowRaise.OnsetTime, ArrivalT, SlowGap));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardFirstFidget, "MageArena.Ward.FirstRaiseFidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardFirstFidget::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.FirstRaiseFidget"), bPass);
	FAbsorbResolver Resolver;
	FWardDetector Detector;
	if (!PrepareRules(*this, Resolver, Detector))
	{
		return false;
	}
	FHandClip Clip;
	FString Error;
	if (!LoadWard(Clip, Error))
	{
		AddError(Error);
		return false;
	}
	const FHandClipTrack* Track = LeftTrack(Clip);
	if (Track == nullptr || Clip.Hz <= 0.0)
	{
		AddError(TEXT("ward-raise.normal has no left-hand track"));
		return false;
	}
	const double Dt = 1.0 / Clip.Hz;
	const double Geo = GeometricOnset(*Track);
	FHandFrame Lap;
	if (!Clip.Sample(EControllerHand::Left, 0.0, Lap) || Geo <= 0.0)
	{
		AddError(TEXT("ward-raise.normal has no lap onset"));
		return false;
	}

	double T = 0.0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Detector.Ingest(AtTime(Lap, T));
		T += Dt;
	}
	const double FidgetTime = T;
	Detector.Ingest(Moved(Lap, T, FVector(0.0, 4.0, 0.0)));
	T += Dt;
	Detector.Ingest(AtTime(Lap, T));
	const double StillUntil = FidgetTime + 0.20;
	while (T < StillUntil - 1.0e-9)
	{
		T += Dt;
		Detector.Ingest(AtTime(Lap, T));
	}
	bPass = TestFalse(TEXT("a lap fidget does not raise"), Detector.GetState().bRaised);
	bPass &= TestTrue(TEXT("the fidget was seen down"), Detector.HasSeenLowered());

	double Source = 0.0;
	double Stamp0 = -1.0;
	int32 Guard = 0;
	while (!Detector.GetState().bRaised && Source <= Clip.GetDuration() && Guard < 80)
	{
		T += Dt;
		FHandFrame Frame;
		if (!Clip.Sample(EControllerHand::Left, Source, Frame))
		{
			break;
		}
		Frame.TimeSeconds = T;
		Frame.Hand = EControllerHand::Left;
		if (Stamp0 < 0.0)
		{
			Stamp0 = T;
		}
		Detector.Ingest(Frame);
		if (Detector.GetState().bRaised)
		{
			break;
		}
		Source += Dt;
		++Guard;
	}
	const double Expected = Stamp0 + Geo;
	const FWardEvent First = Detector.GetLastRaise();
	bPass &= TestTrue(TEXT("first raise after the fidget reached the pose"), Detector.GetState().bRaised);
	bPass &= TestTrue(*FString::Printf(TEXT("first onset %.4f is the raise %.4f, not the fidget %.4f"), First.OnsetTime, Expected, FidgetTime),
		Detector.GetState().bRaised && FMath::Abs(First.OnsetTime - Expected) <= Dt + 1.0e-6);
	bPass &= TestTrue(*FString::Printf(TEXT("first onset %.4f is after the fidget"), First.OnsetTime), First.OnsetTime > FidgetTime + 0.05);
	bPass &= TestTrue(TEXT("first raise from an observed lap is fresh"), First.bFresh);
	bPass &= TestEqual(TEXT("the fidget was not a previous lower"), Detector.GetLowerCount(), 0);
	UE_LOG(LogMageArena, Log, TEXT("Ward first fidget onset=%.4f expected=%.4f fidget=%.4f fresh=%d"),
		First.OnsetTime, Expected, FidgetTime, First.bFresh ? 1 : 0);
	AddInfo(FString::Printf(TEXT("first fidget onset %.4f expected %.4f fresh %d"), First.OnsetTime, Expected, First.bFresh ? 1 : 0));

	// Motion before the hand is ever in the lap is outside the first-raise bound.
	// Kept fast the whole way, so a walk with no bound latches onto that fidget.
	FWardDetector Bound;
	if (!Bound.Init(Error))
	{
		AddError(Error);
		bPass = false;
		return false;
	}
	const double Plateau = UHandClipPlayer::FindPlateauStart(Clip, EControllerHand::Left);
	FHandFrame Hold;
	if (!Clip.Sample(EControllerHand::Left, Plateau, Hold))
	{
		AddError(TEXT("ward plateau missing for the bound case"));
		bPass = false;
		return false;
	}
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	FHandFrame Current = Moved(Lap, 0.0, FVector(0.0, 0.0, 20.0));
	T = 0.0;
	double FirstLow = -1.0;
	auto Feed = [&](FHandFrame Frame)
	{
		Frame.TimeSeconds = T;
		Frame.Hand = EControllerHand::Left;
		const double Height = FWardDetector::PalmHeightMetres(Frame);
		if (FirstLow < 0.0 && Height < FWardThresholds::LowerHeightM)
		{
			FirstLow = T;
		}
		Bound.Ingest(Frame);
		T += Dt;
	};
	Feed(Current);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Current = Moved(Current, T, FVector(0.0, 4.0, 0.0));
		Feed(Current);
	}
	bPass &= TestFalse(TEXT("the chest fidget is not already a ward"), Bound.GetState().bRaised);
	Guard = 0;
	while (FWardDetector::PalmHeightMetres(Current) > 0.12 && Guard < 12)
	{
		Current = Moved(Current, T, FVector(0.0, 1.0, -5.0));
		Feed(Current);
		++Guard;
	}
	bPass &= TestTrue(*FString::Printf(TEXT("pre-lap fidget ends before the lap sample %.4f"), FirstLow),
		FirstLow > 4.0 * Dt);
	Guard = 0;
	while (!Bound.GetState().bRaised && Guard < 40)
	{
		const double DistCm = FVector::Distance(Current.Joints[Palm].Location, Hold.Joints[Palm].Location);
		if (DistCm <= 5.0)
		{
			Feed(AtTime(Hold, T));
			break;
		}
		const double StepCm = FMath::Min(3.0, DistCm - 5.0);
		if (StepCm < 1.0)
		{
			Feed(AtTime(Hold, T));
			break;
		}
		Current = Moved(Current, T, (Hold.Joints[Palm].Location - Current.Joints[Palm].Location).GetSafeNormal() * StepCm);
		Feed(Current);
		++Guard;
	}
	const FWardEvent Bounded = Bound.GetLastRaise();
	bPass &= TestTrue(TEXT("continuous fidget still reaches a first raise"), Bound.GetState().bRaised);
	bPass &= TestTrue(*FString::Printf(TEXT("onset %.4f is at or after the first lap sample %.4f"), Bounded.OnsetTime, FirstLow),
		Bound.GetState().bRaised && FirstLow >= 0.0 && Bounded.OnsetTime + 1.0e-9 >= FirstLow);
	bPass &= TestTrue(TEXT("motion before the lap is not a fresh-window thief"), Bounded.bFresh);
	bPass &= TestEqual(TEXT("no lower before the first raise"), Bound.GetLowerCount(), 0);
	UE_LOG(LogMageArena, Log, TEXT("Ward first bound onset=%.4f lap=%.4f fresh=%d"),
		Bounded.OnsetTime, FirstLow, Bounded.bFresh ? 1 : 0);
	AddInfo(FString::Printf(TEXT("first bound onset %.4f lap %.4f"), Bounded.OnsetTime, FirstLow));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWardElevatedFacing, "MageArena.Ward.ElevatedThreatFacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWardElevatedFacing::RunTest(const FString& Parameters)
{
	bool bPass = false;
	FWardReport::FPublish Publish(TEXT("MageArena.Ward.ElevatedThreatFacing"), bPass);
	FString Error;
	FWardDetector Detector;
	if (!Detector.Init(Error))
	{
		AddError(Error);
		return false;
	}
	const FVector Threat(0.3, 0.4, 1.0);
	Detector.SetThreatFacing(Threat, true);
	const FHandFrame PalmUp = PalmUpFrame(0.0, 0.30);
	const FVector Normal = FWardDetector::PalmNormal(PalmUp);
	const double Angle = FWardDetector::AngleToFacingDeg(Normal, Threat.GetSafeNormal());
	const double Height = FWardDetector::PalmHeightMetres(PalmUp);
	bPass = TestTrue(*FString::Printf(TEXT("palm-up normal is vertical (%.3f, %.3f, %.3f)"), Normal.X, Normal.Y, Normal.Z),
		FVector(Normal.X, Normal.Y, 0.0).IsNearlyZero(1.0e-3) && Normal.Z > 0.9);
	bPass &= TestTrue(*FString::Printf(TEXT("palm-up faces the elevated threat (%.1f deg, height %.2f)"), Angle, Height),
		Angle <= FWardThresholds::RaiseAngleDeg && Height >= FWardThresholds::RaiseHeightM);
	if (!bPass)
	{
		return false;
	}

	const double Dt = 1.0 / 72.0;
	Detector.Ingest(PalmUp);
	FHandFrame Next = PalmUp;
	Next.TimeSeconds = Dt;
	Detector.Ingest(Next);
	const FVector Flat(Threat.X, Threat.Y, 0.0);
	const FVector FlatNormal = Flat.GetSafeNormal();
	const FVector Facing = Detector.GetLastRaise().Facing;
	bPass &= TestTrue(TEXT("elevated threat raised the palm-up ward"), Detector.GetState().bRaised);
	bPass &= TestTrue(*FString::Printf(TEXT("facing (%.3f, %.3f, %.3f) is flat"), Facing.X, Facing.Y, Facing.Z),
		FMath::Abs(Facing.Z) <= 1.0e-4);
	bPass &= TestTrue(*FString::Printf(TEXT("facing matches the threat yaw (%.3f, %.3f)"), FlatNormal.X, FlatNormal.Y),
		Facing.Equals(FlatNormal, 1.0e-3));
	bPass &= TestTrue(TEXT("held palm-up keeps the flat facing"), Detector.GetState().Facing.Equals(FlatNormal, 1.0e-3));

	Detector.ResetStream();
	Detector.SetThreatFacing(FVector(0.0, 0.0, 1.0), true);
	Detector.Ingest(PalmUpFrame(0.0, 0.30));
	const FVector StraightUp = Detector.GetLastRaise().Facing;
	bPass &= TestTrue(TEXT("a vertical threat still raises palm-up"), Detector.GetState().bRaised);
	bPass &= TestTrue(*FString::Printf(TEXT("a vertical threat flattens to +X, got (%.3f, %.3f, %.3f)"), StraightUp.X, StraightUp.Y, StraightUp.Z),
		StraightUp.Equals(FVector::ForwardVector, 1.0e-3));
	UE_LOG(LogMageArena, Log, TEXT("Ward elevated facing (%.3f, %.3f, %.3f) vertical (%.3f, %.3f, %.3f)"),
		Facing.X, Facing.Y, Facing.Z, StraightUp.X, StraightUp.Y, StraightUp.Z);
	AddInfo(FString::Printf(TEXT("elevated facing (%.3f, %.3f, %.3f)"), Facing.X, Facing.Y, Facing.Z));
	return bPass;
}

#endif
