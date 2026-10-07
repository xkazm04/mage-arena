#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/SigilStrokeBuilder.h"
#include "HAL/FileManager.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandClip.h"
#include "Hands/HandClipPlayer.h"
#include "Hands/HandInputSubsystem.h"
#include "HeadMountedDisplayTypes.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
const FName GLines[] = { TEXT("line1"), TEXT("line2"), TEXT("line3") };

FString ClipsDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"));
}

/** The generated corpus, impostors, noise and mouse clips (clipgen). Not under Clips, which the package stages whole. */
FString TestClipsDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("TestClips"));
}

void CollectJsonl(const FString& Directory, TArray<FString>& OutFiles)
{
	IFileManager::Get().FindFilesRecursive(OutFiles, *Directory, TEXT("*.jsonl"), true, false);
	OutFiles.Sort();
}

int32 LineIndex(FName Label)
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(GLines); ++Index)
	{
		if (GLines[Index] == Label)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FName LineFromAction(const FString& Action)
{
	for (const FName Line : GLines)
	{
		if (Action.EndsWith(Line.ToString()))
		{
			return Line;
		}
	}
	return NAME_None;
}

struct FAccuracyBucket
{
	int32 Count = 0;
	int32 Correct = 0;
	int32 Rejected = 0;
	int32 Confusion[3][4] = {};
	double CorrectMax = 0.0;

	double Top1() const
	{
		return Count > 0 ? static_cast<double>(Correct) / static_cast<double>(Count) : 0.0;
	}

	double RejectRate() const
	{
		return Count > 0 ? static_cast<double>(Rejected) / static_cast<double>(Count) : 0.0;
	}

	void Add(FName Expected, bool bGesture, const FQClassifyResult& Result)
	{
		const int32 Row = LineIndex(Expected);
		if (Row == INDEX_NONE)
		{
			return;
		}
		++Count;
		int32 Column = 3;
		if (bGesture && !Result.bReject)
		{
			const int32 Predicted = LineIndex(Result.Label);
			if (Predicted != INDEX_NONE)
			{
				Column = Predicted;
			}
		}
		++Confusion[Row][Column];
		if (Column == 3)
		{
			++Rejected;
			return;
		}
		if (Column == Row)
		{
			++Correct;
			CorrectMax = FMath::Max(CorrectMax, Result.Distance);
		}
	}
};

double Percentile(TArray<double> Values, double P)
{
	if (Values.Num() == 0)
	{
		return 0.0;
	}
	Values.Sort();
	const int32 Index = FMath::Clamp(FMath::CeilToInt(P * static_cast<double>(Values.Num())) - 1, 0, Values.Num() - 1);
	return Values[Index];
}

FString ConfusionJson(const FAccuracyBucket& Bucket)
{
	FString Out = TEXT("{");
	for (int32 Row = 0; Row < 3; ++Row)
	{
		if (Row > 0)
		{
			Out += TEXT(",");
		}
		Out += FString::Printf(TEXT("\"%s\":{\"line1\":%d,\"line2\":%d,\"line3\":%d,\"reject\":%d}"),
			*GLines[Row].ToString(),
			Bucket.Confusion[Row][0], Bucket.Confusion[Row][1], Bucket.Confusion[Row][2], Bucket.Confusion[Row][3]);
	}
	Out += TEXT("}");
	return Out;
}

FString BucketJson(const FAccuracyBucket& Bucket)
{
	return FString::Printf(
		TEXT("{\"count\":%d,\"top1\":%.6f,\"rejectRate\":%.6f,\"correctMaxDistance\":%.3f,\"confusion\":%s}"),
		Bucket.Count, Bucket.Top1(), Bucket.RejectRate(), Bucket.CorrectMax, *ConfusionJson(Bucket));
}

USigilRecognizerSubsystem* NewSigils(UGameInstance* Instance)
{
	USigilRecognizerSubsystem* Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
	Sigils->LoadTemplatesFromDirectory(FPaths::Combine(ClipsDir(), TEXT("templates")));
	return Sigils;
}

bool LoadClip(FAutomationTestBase& Test, const FString& Path, FHandClip& OutClip)
{
	FString Error;
	if (!FHandClip::LoadFromFile(Path, OutClip, Error))
	{
		Test.AddError(FString::Printf(TEXT("%s: %s"), *Path, *Error));
		return false;
	}
	return true;
}

void LogBucket(const TCHAR* Name, const FAccuracyBucket& Bucket)
{
	UE_LOG(LogMageArena, Log, TEXT("Sigil accuracy %s top1=%.2f%% (%d/%d) reject=%.2f%% correctMax=%.0f"),
		Name, Bucket.Top1() * 100.0, Bucket.Correct, Bucket.Count, Bucket.RejectRate() * 100.0, Bucket.CorrectMax);
}

struct FFedGesture
{
	ESigilStrokeEvent Event = ESigilStrokeEvent::None;
	TArray<FQPoint> Points;
};

FFedGesture FeedClip(const FHandClip& Clip)
{
	FFedGesture Out;
	FSigilStrokeBuilder Builder;
	Builder.SetStyle(ESigilDrawStyle::StyleA);
	const int32 TipIndex = static_cast<int32>(EHandKeypoint::IndexTip);
	for (const FHandClipTrack& Track : Clip.GetTracks())
	{
		if (Track.Hand != EControllerHand::Right)
		{
			continue;
		}
		for (const FHandFrame& Frame : Track.Frames)
		{
			if (!Frame.Joints.IsValidIndex(TipIndex))
			{
				continue;
			}
			TArray<FQPoint> Points;
			const ESigilStrokeEvent Event = Builder.AddTip(Frame.Joints[TipIndex].Location, Frame.Pinch, Frame.TimeSeconds, Points);
			if (Event == ESigilStrokeEvent::Gesture)
			{
				Out.Event = Event;
				Out.Points = MoveTemp(Points);
				return Out;
			}
			if (Event == ESigilStrokeEvent::Rejected)
			{
				Out.Event = Event;
				if (Points.Num() > 0)
				{
					Out.Points = MoveTemp(Points);
				}
				return Out;
			}
		}
	}
	TArray<FQPoint> Flushed;
	const ESigilStrokeEvent FlushEvent = Builder.Flush(Flushed);
	if (Out.Event == ESigilStrokeEvent::None)
	{
		Out.Event = FlushEvent;
	}
	if (Flushed.Num() > 0)
	{
		Out.Points = MoveTemp(Flushed);
	}
	return Out;
}

struct FImpostorBucket
{
	int32 Count = 0;
	int32 Rejected = 0;
	TArray<double> Distances;

	double RejectRate() const
	{
		return Count > 0 ? static_cast<double>(Rejected) / static_cast<double>(Count) : 0.0;
	}
};

struct FDistanceSpan
{
	double Min = 0.0;
	double P50 = 0.0;
	double P95 = 0.0;
	double Max = 0.0;
	int32 Count = 0;

	static FDistanceSpan From(TArray<double> Values)
	{
		FDistanceSpan Span;
		Span.Count = Values.Num();
		if (Values.Num() == 0)
		{
			return Span;
		}
		Values.Sort();
		Span.Min = Values[0];
		Span.Max = Values.Last();
		Span.P50 = Percentile(Values, 0.50);
		Span.P95 = Percentile(Values, 0.95);
		return Span;
	}

	FString Json() const
	{
		return FString::Printf(TEXT("{\"count\":%d,\"min\":%.3f,\"p50\":%.3f,\"p95\":%.3f,\"max\":%.3f}"), Count, Min, P50, P95, Max);
	}
};

FString ImpostorKind(const FString& Action)
{
	const FString Prefix = TEXT("impostor-");
	if (Action.StartsWith(Prefix))
	{
		return Action.Mid(Prefix.Len());
	}
	return Action;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSigilsTemplates, "MageArena.Sigils.Templates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSigilsTemplates::RunTest(const FString& Parameters)
{
	bool bPass = true;
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	USigilRecognizerSubsystem* Sigils = NewSigils(Instance);
	const FQPointCloudRecognizer& Recognizer = Sigils->GetRecognizer();

	for (const FName Line : GLines)
	{
		bPass &= TestTrue(*FString::Printf(TEXT("at least one %s template"), *Line.ToString()), Recognizer.CountTemplates(Line) >= 1);
	}

	TArray<FString> Files;
	CollectJsonl(FPaths::Combine(ClipsDir(), TEXT("templates")), Files);
	bPass &= TestTrue(TEXT("template files exist"), Files.Num() > 0);
	bPass &= TestEqual(TEXT("every template file loaded"), Recognizer.GetTemplateCount(), Files.Num());
	for (const FString& Path : Files)
	{
		FHandClip Clip;
		if (!LoadClip(*this, Path, Clip))
		{
			bPass = false;
			continue;
		}
		const FName Expected = LineFromAction(Clip.Action);
		TArray<FQPoint> Points;
		if (!ExtractFirstGesture(Clip, ESigilDrawStyle::StyleA, Points))
		{
			AddError(FString::Printf(TEXT("%s: template gesture did not extract"), *Path));
			bPass = false;
			continue;
		}
		const FQClassifyResult Result = Recognizer.Classify(Points);
		bPass &= TestTrue(*FString::Printf(TEXT("%s classifies as %s, got %s"), *Clip.Action, *Expected.ToString(), *Result.Label.ToString()), Result.Label == Expected);
		bPass &= TestFalse(*FString::Printf(TEXT("%s rejected"), *Path), Result.bReject);
		bPass &= TestTrue(*FString::Printf(TEXT("%s self-distance %.1f"), *Path, Result.Distance), Result.Distance < 1.0);

		// Style B is one stroke. The inner stroke must not unwind the circle check.
		TArray<FQPoint> StyleBPoints;
		if (!ExtractFirstGesture(Clip, ESigilDrawStyle::StyleB, StyleBPoints))
		{
			AddError(FString::Printf(TEXT("%s: style B gesture did not extract"), *Path));
			bPass = false;
		}
	}

	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSigilsAccuracy, "MageArena.Sigils.Accuracy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSigilsAccuracy::RunTest(const FString& Parameters)
{
	bool bPass = true;
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	USigilRecognizerSubsystem* Sigils = NewSigils(Instance);
	const FQPointCloudRecognizer& Recognizer = Sigils->GetRecognizer();

	FAccuracyBucket ByVariant[3];
	FAccuracyBucket Synthetic;
	FAccuracyBucket Mouse;
	TArray<double> Times;
	TArray<double> CorrectDistances;

	auto Consume = [&](const FHandClip& Clip, bool bMouse)
	{
		const FName Expected = LineFromAction(Clip.Action);
		TArray<FQPoint> Points;
		const bool bGesture = ExtractFirstGesture(Clip, ESigilDrawStyle::StyleA, Points);
		FQClassifyResult Result;
		if (bGesture)
		{
			Result = Recognizer.Classify(Points);
			Times.Add(Result.Milliseconds);
			if (Result.Label == Expected)
			{
				CorrectDistances.Add(Result.Distance);
			}
		}
		if (bMouse)
		{
			Mouse.Add(Expected, bGesture, Result);
			return;
		}
		Synthetic.Add(Expected, bGesture, Result);
		const int32 VariantIndex = Clip.Variant == TEXT("slow") ? 1 : Clip.Variant == TEXT("sloppy") ? 2 : 0;
		ByVariant[VariantIndex].Add(Expected, bGesture, Result);
	};

	TArray<FString> Corpus;
	CollectJsonl(FPaths::Combine(TestClipsDir(), TEXT("corpus")), Corpus);
	for (const FString& Path : Corpus)
	{
		FHandClip Clip;
		if (!LoadClip(*this, Path, Clip))
		{
			bPass = false;
			continue;
		}
		Consume(Clip, false);
	}
	TArray<FString> MouseFiles;
	CollectJsonl(FPaths::Combine(TestClipsDir(), TEXT("mouse")), MouseFiles);
	for (const FString& Path : MouseFiles)
	{
		FHandClip Clip;
		if (!LoadClip(*this, Path, Clip))
		{
			bPass = false;
			continue;
		}
		Consume(Clip, Clip.Source == TEXT("mouse"));
	}

	const TCHAR* ImpostorNames[] = { TEXT("bare"), TEXT("dot"), TEXT("zigzag"), TEXT("hook"), TEXT("arc"), TEXT("swipe") };
	FImpostorBucket Impostors[UE_ARRAY_COUNT(ImpostorNames)];
	TArray<double> ImpostorDistances;
	TArray<FString> ImpostorFiles;
	CollectJsonl(FPaths::Combine(TestClipsDir(), TEXT("impostors")), ImpostorFiles);
	for (const FString& Path : ImpostorFiles)
	{
		FHandClip Clip;
		if (!LoadClip(*this, Path, Clip))
		{
			bPass = false;
			continue;
		}
		const FString Kind = ImpostorKind(Clip.Action);
		int32 KindIndex = INDEX_NONE;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ImpostorNames); ++Index)
		{
			if (Kind == ImpostorNames[Index])
			{
				KindIndex = Index;
				break;
			}
		}
		if (KindIndex == INDEX_NONE)
		{
			AddError(FString::Printf(TEXT("unknown impostor %s"), *Clip.Action));
			bPass = false;
			continue;
		}
		FImpostorBucket& Bucket = Impostors[KindIndex];
		++Bucket.Count;
		const FFedGesture Fed = FeedClip(Clip);
		FQClassifyResult Result;
		if (Fed.Points.Num() > 0)
		{
			Result = Recognizer.Classify(Fed.Points);
			Times.Add(Result.Milliseconds);
			Bucket.Distances.Add(Result.Distance);
			ImpostorDistances.Add(Result.Distance);
		}
		const bool bRejected = Fed.Event != ESigilStrokeEvent::Gesture || Result.bReject;
		if (bRejected)
		{
			++Bucket.Rejected;
		}
		else
		{
			UE_LOG(LogMageArena, Warning, TEXT("Impostor accepted %s as %s distance %.1f"), *Clip.Action, *Result.Label.ToString(), Result.Distance);
		}
	}

	const double P50 = Percentile(Times, 0.50);
	const double P95 = Percentile(Times, 0.95);
	const TCHAR* VariantNames[] = { TEXT("normal"), TEXT("slow"), TEXT("sloppy") };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		LogBucket(VariantNames[Index], ByVariant[Index]);
		AddInfo(FString::Printf(TEXT("%s top1 %.2f%% (%d/%d)"), VariantNames[Index], ByVariant[Index].Top1() * 100.0, ByVariant[Index].Correct, ByVariant[Index].Count));
	}
	LogBucket(TEXT("synthetic"), Synthetic);
	LogBucket(TEXT("mouse"), Mouse);
	const FDistanceSpan CorrectSpan = FDistanceSpan::From(CorrectDistances);
	const FDistanceSpan ImpostorSpan = FDistanceSpan::From(ImpostorDistances);
	UE_LOG(LogMageArena, Log, TEXT("Sigil distance correct min=%.0f p50=%.0f p95=%.0f max=%.0f n=%d"),
		CorrectSpan.Min, CorrectSpan.P50, CorrectSpan.P95, CorrectSpan.Max, CorrectSpan.Count);
	UE_LOG(LogMageArena, Log, TEXT("Sigil distance impostor min=%.0f p50=%.0f p95=%.0f max=%.0f n=%d"),
		ImpostorSpan.Min, ImpostorSpan.P50, ImpostorSpan.P95, ImpostorSpan.Max, ImpostorSpan.Count);
	UE_LOG(LogMageArena, Log, TEXT("Sigil threshold %.0f margin above correct max %.0f below impostor min %.0f"),
		Recognizer.GetRejectDistance(),
		Recognizer.GetRejectDistance() - CorrectSpan.Max,
		ImpostorSpan.Count > 0 ? ImpostorSpan.Min - Recognizer.GetRejectDistance() : 0.0);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ImpostorNames); ++Index)
	{
		const FImpostorBucket& Bucket = Impostors[Index];
		const FDistanceSpan KindSpan = FDistanceSpan::From(Bucket.Distances);
		UE_LOG(LogMageArena, Log, TEXT("Sigil impostor %s rejected %d/%d (%.2f%%) distance min=%.0f p50=%.0f max=%.0f n=%d"),
			ImpostorNames[Index], Bucket.Rejected, Bucket.Count, Bucket.RejectRate() * 100.0,
			KindSpan.Min, KindSpan.P50, KindSpan.Max, KindSpan.Count);
		AddInfo(FString::Printf(TEXT("impostor %s reject %.2f%% (%d/%d)"), ImpostorNames[Index], Bucket.RejectRate() * 100.0, Bucket.Rejected, Bucket.Count));
	}
	UE_LOG(LogMageArena, Log, TEXT("Sigil classification ms p50=%.4f p95=%.4f n=%d"), P50, P95, Times.Num());
	AddInfo(FString::Printf(TEXT("mouse top1 %.2f%% (%d/%d); classify p50 %.4f ms p95 %.4f ms"), Mouse.Top1() * 100.0, Mouse.Correct, Mouse.Count, P50, P95));

	FString Json = FString::Printf(TEXT("{\n  \"pointCount\": %d,\n  \"lutSize\": %d,\n  \"rejectDistance\": %.1f,\n  \"innerTailRatio\": %.2f,\n  \"minInnerStrokeRatio\": %.2f,\n  \"byVariant\": {\n"),
		FQPointCloudRecognizer::PointCount, FQPointCloudRecognizer::LutSize, Recognizer.GetRejectDistance(), FSigilStrokeBuilder::InnerTailRatio, FSigilStrokeBuilder::MinInnerStrokeRatio);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Json += FString::Printf(TEXT("    \"%s\": %s%s\n"), VariantNames[Index], *BucketJson(ByVariant[Index]), Index < 2 ? TEXT(",") : TEXT(""));
	}
	Json += FString::Printf(TEXT("  },\n  \"bySource\": {\n    \"synthetic\": %s,\n    \"mouse\": %s\n  },\n  \"impostors\": {\n"),
		*BucketJson(Synthetic), *BucketJson(Mouse));
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ImpostorNames); ++Index)
	{
		const FImpostorBucket& Bucket = Impostors[Index];
		const FDistanceSpan KindSpan = FDistanceSpan::From(Bucket.Distances);
		Json += FString::Printf(TEXT("    \"%s\": {\"count\":%d,\"rejected\":%d,\"rejectRate\":%.6f,\"distance\":%s}%s\n"),
			ImpostorNames[Index], Bucket.Count, Bucket.Rejected, Bucket.RejectRate(), *KindSpan.Json(),
			Index + 1 < UE_ARRAY_COUNT(ImpostorNames) ? TEXT(",") : TEXT(""));
	}
	Json += FString::Printf(TEXT("  },\n  \"distances\": {\n    \"correct\": %s,\n    \"impostor\": %s\n  },\n  \"timingMs\": {\"p50\": %.6f, \"p95\": %.6f, \"count\": %d}\n}\n"),
		*CorrectSpan.Json(), *ImpostorSpan.Json(), P50, P95, Times.Num());

	const FString ReportDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Reports"));
	IFileManager::Get().MakeDirectory(*ReportDir, true);
	const FString ReportPath = FPaths::Combine(ReportDir, TEXT("sigil-accuracy.json"));
	bPass &= TestTrue(TEXT("wrote sigil-accuracy.json"), FFileHelper::SaveStringToFile(Json, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	bPass &= TestEqual(TEXT("synthetic drawings"), Synthetic.Count, 270);
	bPass &= TestEqual(TEXT("mouse drawings"), Mouse.Count, 90);
	bPass &= TestTrue(*FString::Printf(TEXT("normal top1 %.4f"), ByVariant[0].Top1()), ByVariant[0].Top1() >= 0.95);
	bPass &= TestTrue(*FString::Printf(TEXT("slow top1 %.4f"), ByVariant[1].Top1()), ByVariant[1].Top1() >= 0.95);
	bPass &= TestTrue(*FString::Printf(TEXT("sloppy top1 %.4f"), ByVariant[2].Top1()), ByVariant[2].Top1() >= 0.90);
	bPass &= TestTrue(*FString::Printf(TEXT("mouse top1 %.4f"), Mouse.Top1()), Mouse.Top1() >= 0.90);
	bPass &= TestTrue(*FString::Printf(TEXT("classify p95 %.4f ms"), P95), P95 <= 0.5);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ImpostorNames); ++Index)
	{
		bPass &= TestTrue(*FString::Printf(TEXT("impostor %s count %d"), ImpostorNames[Index], Impostors[Index].Count), Impostors[Index].Count >= 30);
		bPass &= TestTrue(*FString::Printf(TEXT("impostor %s reject %.4f"), ImpostorNames[Index], Impostors[Index].RejectRate()), Impostors[Index].RejectRate() >= 0.90);
	}

	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSigilsNoFalseCasts, "MageArena.Sigils.NoFalseCasts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSigilsNoFalseCasts::RunTest(const FString& Parameters)
{
	bool bPass = true;
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	USigilRecognizerSubsystem* Sigils = NewSigils(Instance);
	const FQPointCloudRecognizer& Recognizer = Sigils->GetRecognizer();

	TArray<FString> Files;
	CollectJsonl(FPaths::Combine(TestClipsDir(), TEXT("noise")), Files);
	bPass &= TestTrue(TEXT("noise clips exist"), Files.Num() > 0);

	int32 Casts = 0;
	double FedSeconds = 0.0;
	const int32 TipIndex = static_cast<int32>(EHandKeypoint::IndexTip);
	for (const FString& Path : Files)
	{
		FHandClip Clip;
		if (!LoadClip(*this, Path, Clip))
		{
			bPass = false;
			continue;
		}
		FSigilStrokeBuilder Builder;
		Builder.SetStyle(ESigilDrawStyle::StyleA);
		auto Consider = [&](ESigilStrokeEvent Event, const TArray<FQPoint>& Points)
		{
			if (Event != ESigilStrokeEvent::Gesture)
			{
				return;
			}
			const FQClassifyResult Result = Recognizer.Classify(Points);
			if (!Result.bReject)
			{
				++Casts;
				UE_LOG(LogMageArena, Warning, TEXT("False cast in %s as %s distance %.1f"), *Clip.Action, *Result.Label.ToString(), Result.Distance);
			}
		};
		bool bFed = false;
		double FedBegin = 0.0;
		double FedEnd = 0.0;
		for (const FHandClipTrack& Track : Clip.GetTracks())
		{
			if (Track.Hand != EControllerHand::Right)
			{
				continue;
			}
			for (const FHandFrame& Frame : Track.Frames)
			{
				if (!Frame.Joints.IsValidIndex(TipIndex))
				{
					continue;
				}
				if (!bFed)
				{
					bFed = true;
					FedBegin = Frame.TimeSeconds;
				}
				FedEnd = Frame.TimeSeconds;
				TArray<FQPoint> Points;
				const ESigilStrokeEvent Event = Builder.AddTip(Frame.Joints[TipIndex].Location, Frame.Pinch, Frame.TimeSeconds, Points);
				Consider(Event, Points);
			}
		}
		TArray<FQPoint> Flushed;
		Consider(Builder.Flush(Flushed), Flushed);
		if (bFed)
		{
			FedSeconds += FedEnd - FedBegin;
		}
	}

	const double Minutes = FedSeconds / 60.0;
	const double Rate = Minutes > 0.0 ? static_cast<double>(Casts) / Minutes : static_cast<double>(Casts);
	UE_LOG(LogMageArena, Log, TEXT("Sigil false casts %d over %.3fs fed (%.3f per minute)"), Casts, FedSeconds, Rate);
	AddInfo(FString::Printf(TEXT("false casts %d over %.1fs fed = %.3f per minute"), Casts, FedSeconds, Rate));
	bPass &= TestTrue(*FString::Printf(TEXT("fed noise duration %.3fs"), FedSeconds), FedSeconds >= 180.0 - 1.0e-3);
	bPass &= TestTrue(*FString::Printf(TEXT("false-cast rate %.3f per minute"), Rate), Rate <= 1.0);

	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSigilsEndToEnd, "MageArena.Sigils.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSigilsEndToEnd::RunTest(const FString& Parameters)
{
	bool bPass = true;
	// Not GameInstance::Init(): that boots every game-instance subsystem.
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	UHandInputSubsystem* Hands = NewObject<UHandInputSubsystem>(Instance);
	USigilRecognizerSubsystem* Sigils = NewSigils(Instance);

	int32 Casts = 0;
	FName Line = NAME_None;
	Sigils->OnSigilCast.AddLambda([&Casts, &Line](FName InLine, float, double)
	{
		++Casts;
		Line = InLine;
	});
	// Bind before Play. Play emits the t=0 frame immediately.
	Sigils->BindToHands(Hands);
	Hands->PlayQuickAction(TEXT("sigil-line2"), EClipVariant::Normal);

	UHandClipPlayer* Player = Hands->GetClipPlayer();
	if (!Player || Player->GetClip().Action != TEXT("sigil-line2"))
	{
		AddError(TEXT("PlayQuickAction did not start sigil-line2.normal"));
		Instance->RemoveFromRoot();
		return false;
	}

	const FHandClipTrack* Right = nullptr;
	for (const FHandClipTrack& Track : Player->GetClip().GetTracks())
	{
		if (Track.Hand == EControllerHand::Right)
		{
			Right = &Track;
			break;
		}
	}
	if (!Right || Right->Frames.Num() < 2)
	{
		AddError(TEXT("sigil-line2.normal has no right-hand frames"));
		Instance->RemoveFromRoot();
		return false;
	}

	// Step on the clip's own sample times and stop at Duration.
	// Stepping past the end no longer emits (MageArena.Clips.NoEmitAfterEnd).
	const double Duration = Player->GetDuration();
	for (int32 Index = 1; Index < Right->Frames.Num(); ++Index)
	{
		if (Player->GetTime() + 1.0e-9 >= Duration)
		{
			break;
		}
		const double Dt = Right->Frames[Index].TimeSeconds - Player->GetTime();
		if (Dt > 1.0e-9)
		{
			Hands->Step(Dt);
		}
	}

	bPass &= TestEqual(TEXT("one sigil cast"), Casts, 1);
	bPass &= TestTrue(*FString::Printf(TEXT("cast line %s"), *Line.ToString()), Line == FName(TEXT("line2")));
	UE_LOG(LogMageArena, Log, TEXT("Sigil end-to-end casts=%d line=%s"), Casts, *Line.ToString());

	Player->Stop();
	Sigils->BindToHands(nullptr);
	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();
	return bPass;
}

#endif
