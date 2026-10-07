#include "Hands/HandClip.h"

#include "Dom/JsonObject.h"
#include "HeadMountedDisplayTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static_assert(static_cast<int32>(EHandKeypoint::LittleTip) + 1 == MageHandJointCount,
	"Hand clips use EHandKeypoint order.");

namespace
{
const TCHAR* GKeypointNames[MageHandJointCount] = {
	TEXT("Palm"), TEXT("Wrist"),
	TEXT("ThumbMetacarpal"), TEXT("ThumbProximal"), TEXT("ThumbDistal"), TEXT("ThumbTip"),
	TEXT("IndexMetacarpal"), TEXT("IndexProximal"), TEXT("IndexIntermediate"), TEXT("IndexDistal"), TEXT("IndexTip"),
	TEXT("MiddleMetacarpal"), TEXT("MiddleProximal"), TEXT("MiddleIntermediate"), TEXT("MiddleDistal"), TEXT("MiddleTip"),
	TEXT("RingMetacarpal"), TEXT("RingProximal"), TEXT("RingIntermediate"), TEXT("RingDistal"), TEXT("RingTip"),
	TEXT("LittleMetacarpal"), TEXT("LittleProximal"), TEXT("LittleIntermediate"), TEXT("LittleDistal"), TEXT("LittleTip"),
};

const TCHAR* GSchemaV1 = TEXT("mage-arena/hand-clip@1");
const TCHAR* GSchemaV2 = TEXT("mage-arena/hand-clip@2");

const TCHAR* GSpace = TEXT("seated-origin, metres, +X forward, +Y right, +Z up (Unreal axes, cm converted to m)");

bool ParseObject(const FString& Line, TSharedPtr<FJsonObject>& Out, FString& OutError, int32 LineNumber)
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
	if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
	{
		OutError = FString::Printf(TEXT("line %d: invalid JSON"), LineNumber);
		return false;
	}
	return true;
}

bool ExpectFieldCount(const TSharedPtr<FJsonObject>& Object, int32 Count, const TCHAR* What, int32 LineNumber, FString& OutError)
{
	if (Object->Values.Num() != Count)
	{
		OutError = FString::Printf(TEXT("line %d: %s has %d fields, expected %d"), LineNumber, What, Object->Values.Num(), Count);
		return false;
	}
	return true;
}

bool ReadHandLetter(const FString& Letter, EControllerHand& OutHand)
{
	if (Letter == TEXT("L"))
	{
		OutHand = EControllerHand::Left;
		return true;
	}
	if (Letter == TEXT("R"))
	{
		OutHand = EControllerHand::Right;
		return true;
	}
	return false;
}
}

const TCHAR* FHandClip::KeypointName(int32 Index)
{
	return (Index >= 0 && Index < MageHandJointCount) ? GKeypointNames[Index] : TEXT("");
}

const TCHAR* FHandClip::VariantToString(EClipVariant Variant)
{
	switch (Variant)
	{
	case EClipVariant::Slow:
		return TEXT("slow");
	case EClipVariant::Sloppy:
		return TEXT("sloppy");
	default:
		return TEXT("normal");
	}
}

FString FHandClip::MakeFilePath(FName Action, EClipVariant Variant, bool bMirror)
{
	const FString Name = FString::Printf(TEXT("%s.%s.jsonl"), *Action.ToString(), VariantToString(Variant));
	if (bMirror)
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("mirror"), Name);
	}
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), Name);
}

int32 FHandClip::GetSampleCount() const
{
	int32 Count = 0;
	for (const FHandClipTrack& Track : Tracks)
	{
		Count += Track.Frames.Num();
	}
	return Count;
}

bool FHandClip::LoadFromFile(const FString& Path, FHandClip& OutClip, FString& OutError)
{
	OutClip = FHandClip();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("Cannot read clip %s"), *Path);
		return false;
	}
	Text.ReplaceInline(TEXT("\r"), TEXT(""));
	TArray<FString> Lines;
	Text.ParseIntoArray(Lines, TEXT("\n"), false);
	if (Lines.Num() > 0 && Lines.Last().IsEmpty())
	{
		Lines.Pop();
	}
	if (Lines.Num() < 2)
	{
		OutError = FString::Printf(TEXT("%s: need a header and at least one frame"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Header;
	if (!ParseObject(Lines[0], Header, OutError, 1) || !ExpectFieldCount(Header, 10, TEXT("header"), 1, OutError))
	{
		return false;
	}

	FString Schema;
	if (!Header->TryGetStringField(TEXT("schema"), Schema) || (Schema != GSchemaV1 && Schema != GSchemaV2))
	{
		OutError = FString::Printf(TEXT("%s: schema is not mage-arena/hand-clip@1 or @2"), *Path);
		return false;
	}
	// @2 is @1 plus one optional frame field, sys. Objects stay closed: an @1 frame may not carry it.
	const bool bSchemaV2 = Schema == GSchemaV2;
	if (!Header->TryGetStringField(TEXT("name"), OutClip.Name) || OutClip.Name.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s: missing name"), *Path);
		return false;
	}
	if (!Header->TryGetStringField(TEXT("action"), OutClip.Action) || OutClip.Action.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s: missing action"), *Path);
		return false;
	}
	if (!Header->TryGetStringField(TEXT("variant"), OutClip.Variant)
		|| (OutClip.Variant != TEXT("normal") && OutClip.Variant != TEXT("slow") && OutClip.Variant != TEXT("sloppy")))
	{
		OutError = FString::Printf(TEXT("%s: bad variant"), *Path);
		return false;
	}
	if (!Header->TryGetStringField(TEXT("source"), OutClip.Source)
		|| (OutClip.Source != TEXT("synthetic") && OutClip.Source != TEXT("recorded") && OutClip.Source != TEXT("mouse")))
	{
		OutError = FString::Printf(TEXT("%s: bad source"), *Path);
		return false;
	}
	if (!Header->TryGetStringField(TEXT("space"), OutClip.Space) || OutClip.Space != GSpace)
	{
		OutError = FString::Printf(TEXT("%s: space string mismatch"), *Path);
		return false;
	}
	double Hz = 0.0;
	if (!Header->TryGetNumberField(TEXT("hz"), Hz) || Hz != 72.0)
	{
		OutError = FString::Printf(TEXT("%s: hz must be 72"), *Path);
		return false;
	}
	OutClip.Hz = Hz;
	double SeedNumber = 0.0;
	if (!Header->TryGetNumberField(TEXT("seed"), SeedNumber) || SeedNumber < 0.0 || SeedNumber > 4294967295.0
		|| SeedNumber != FMath::FloorToDouble(SeedNumber))
	{
		OutError = FString::Printf(TEXT("%s: seed is not an integer in range"), *Path);
		return false;
	}
	OutClip.Seed = static_cast<int64>(SeedNumber);

	const TArray<TSharedPtr<FJsonValue>>* HandValues = nullptr;
	if (!Header->TryGetArrayField(TEXT("hands"), HandValues) || HandValues == nullptr || HandValues->Num() == 0)
	{
		OutError = FString::Printf(TEXT("%s: hands must be a non-empty array"), *Path);
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *HandValues)
	{
		FString Letter;
		EControllerHand Hand = EControllerHand::Left;
		if (!Value.IsValid() || !Value->TryGetString(Letter) || !ReadHandLetter(Letter, Hand))
		{
			OutError = FString::Printf(TEXT("%s: bad hand letter"), *Path);
			return false;
		}
		if (OutClip.Hands.Contains(Hand))
		{
			OutError = FString::Printf(TEXT("%s: duplicate hand"), *Path);
			return false;
		}
		OutClip.Hands.Add(Hand);
		FHandClipTrack Track;
		Track.Hand = Hand;
		OutClip.Tracks.Add(Track);
	}
	if (OutClip.Hands.Num() == 2 && OutClip.Hands[0] != EControllerHand::Left)
	{
		OutError = FString::Printf(TEXT("%s: hands must list L before R"), *Path);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* KeyValues = nullptr;
	if (!Header->TryGetArrayField(TEXT("keypoints"), KeyValues) || KeyValues == nullptr || KeyValues->Num() != MageHandJointCount)
	{
		OutError = FString::Printf(TEXT("%s: keypoints must be the 26 EHandKeypoint names"), *Path);
		return false;
	}
	for (int32 Index = 0; Index < MageHandJointCount; ++Index)
	{
		FString Name;
		if (!(*KeyValues)[Index].IsValid() || !(*KeyValues)[Index]->TryGetString(Name) || Name != GKeypointNames[Index])
		{
			OutError = FString::Printf(TEXT("%s: keypoint %d is not %s"), *Path, Index, GKeypointNames[Index]);
			return false;
		}
	}

	TMap<EControllerHand, int32> TrackIndex;
	for (int32 Index = 0; Index < OutClip.Tracks.Num(); ++Index)
	{
		TrackIndex.Add(OutClip.Tracks[Index].Hand, Index);
	}

	double PreviousLineTime = -1.0;
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		const int32 LineNumber = LineIndex + 1;
		if (Lines[LineIndex].IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s: blank line %d"), *Path, LineNumber);
			return false;
		}
		TSharedPtr<FJsonObject> FrameObject;
		if (!ParseObject(Lines[LineIndex], FrameObject, OutError, LineNumber))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *Path, *OutError);
			return false;
		}
		bool bSystemGesture = false;
		const TSharedPtr<FJsonValue> SysValue = FrameObject->TryGetField(TEXT("sys"));
		const bool bHasSys = SysValue.IsValid();
		// A JSON number would also convert to bool. The schema says boolean.
		if (bHasSys && (!bSchemaV2 || SysValue->Type != EJson::Boolean || !SysValue->TryGetBool(bSystemGesture)))
		{
			OutError = FString::Printf(TEXT("%s: line %d: sys needs schema @2 and a boolean"), *Path, LineNumber);
			return false;
		}
		if (!ExpectFieldCount(FrameObject, bHasSys ? 6 : 5, TEXT("frame"), LineNumber, OutError))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *Path, *OutError);
			return false;
		}
		FHandFrame Frame;
		double TimeSeconds = 0.0;
		double Confidence = 0.0;
		double Pinch = 0.0;
		FString Letter;
		if (!FrameObject->TryGetNumberField(TEXT("t"), TimeSeconds) || !FMath::IsFinite(TimeSeconds) || TimeSeconds < 0.0)
		{
			OutError = FString::Printf(TEXT("%s: line %d: bad t"), *Path, LineNumber);
			return false;
		}
		if (TimeSeconds < PreviousLineTime)
		{
			OutError = FString::Printf(TEXT("%s: line %d: t went backwards"), *Path, LineNumber);
			return false;
		}
		PreviousLineTime = TimeSeconds;
		if (!FrameObject->TryGetStringField(TEXT("hand"), Letter) || !ReadHandLetter(Letter, Frame.Hand) || !TrackIndex.Contains(Frame.Hand))
		{
			OutError = FString::Printf(TEXT("%s: line %d: bad hand"), *Path, LineNumber);
			return false;
		}
		if (!FrameObject->TryGetNumberField(TEXT("conf"), Confidence) || Confidence < 0.0 || Confidence > 1.0
			|| !FrameObject->TryGetNumberField(TEXT("pinch"), Pinch) || Pinch < 0.0 || Pinch > 1.0)
		{
			OutError = FString::Printf(TEXT("%s: line %d: conf or pinch out of range"), *Path, LineNumber);
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Joints = nullptr;
		if (!FrameObject->TryGetArrayField(TEXT("joints"), Joints) || Joints == nullptr || Joints->Num() != MageHandJointCount)
		{
			OutError = FString::Printf(TEXT("%s: line %d: expected 26 joints"), *Path, LineNumber);
			return false;
		}
		Frame.TimeSeconds = TimeSeconds;
		Frame.Confidence = static_cast<float>(Confidence);
		Frame.Pinch = static_cast<float>(Pinch);
		Frame.bSystemGesture = bSystemGesture;
		Frame.Joints.SetNum(MageHandJointCount);
		for (int32 JointIndex = 0; JointIndex < MageHandJointCount; ++JointIndex)
		{
			const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
			if (!(*Joints)[JointIndex].IsValid() || !(*Joints)[JointIndex]->TryGetArray(Components)
				|| Components == nullptr || Components->Num() != 7)
			{
				OutError = FString::Printf(TEXT("%s: line %d: joint %d is not 7 numbers"), *Path, LineNumber, JointIndex);
				return false;
			}
			double Number[7];
			for (int32 Component = 0; Component < 7; ++Component)
			{
				if (!(*Components)[Component].IsValid() || !(*Components)[Component]->TryGetNumber(Number[Component])
					|| !FMath::IsFinite(Number[Component]))
				{
					OutError = FString::Printf(TEXT("%s: line %d: joint %d component %d"), *Path, LineNumber, JointIndex, Component);
					return false;
				}
			}
			FHandJointPose& Pose = Frame.Joints[JointIndex];
			Pose.Location = FVector(Number[0], Number[1], Number[2]) * MageClipMetresToCentimetres;
			Pose.Rotation = FQuat(Number[3], Number[4], Number[5], Number[6]);
			const double Magnitude = Pose.Rotation.Size();
			if (!FMath::IsFinite(Magnitude) || FMath::Abs(Magnitude - 1.0) > 1.0e-3)
			{
				OutError = FString::Printf(TEXT("%s: line %d: joint %d quaternion norm %.6f"), *Path, LineNumber, JointIndex, Magnitude);
				return false;
			}
		}

		FHandClipTrack& Track = OutClip.Tracks[TrackIndex[Frame.Hand]];
		if (Track.Frames.Num() == 0)
		{
			if (Frame.TimeSeconds != 0.0)
			{
				OutError = FString::Printf(TEXT("%s: hand does not start at t=0"), *Path);
				return false;
			}
		}
		else if (!(Frame.TimeSeconds > Track.Frames.Last().TimeSeconds))
		{
			OutError = FString::Printf(TEXT("%s: line %d: hand time is not increasing"), *Path, LineNumber);
			return false;
		}
		else
		{
			const double Dt = Frame.TimeSeconds - Track.Frames.Last().TimeSeconds;
			const double Expected = 1.0 / OutClip.Hz;
			if (Dt < Expected * 0.9 || Dt > Expected * 1.1)
			{
				OutError = FString::Printf(TEXT("%s: line %d: sample spacing %.6f is outside 10%% of 1/hz"), *Path, LineNumber, Dt);
				return false;
			}
		}
		Track.Frames.Add(MoveTemp(Frame));
	}

	if (OutClip.Tracks.Num() == 0 || OutClip.Tracks[0].Frames.Num() < 2)
	{
		OutError = FString::Printf(TEXT("%s: each hand needs at least 2 samples"), *Path);
		return false;
	}
	const int32 SampleCount = OutClip.Tracks[0].Frames.Num();
	OutClip.Duration = OutClip.Tracks[0].Frames.Last().TimeSeconds;
	for (const FHandClipTrack& Track : OutClip.Tracks)
	{
		if (Track.Frames.Num() != SampleCount)
		{
			OutError = FString::Printf(TEXT("%s: hands do not share a time grid"), *Path);
			return false;
		}
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			if (Track.Frames[Index].TimeSeconds != OutClip.Tracks[0].Frames[Index].TimeSeconds)
			{
				OutError = FString::Printf(TEXT("%s: hand timestamps diverge"), *Path);
				return false;
			}
			if (Track.Frames[Index].Joints.Num() != MageHandJointCount)
			{
				OutError = FString::Printf(TEXT("%s: joint count"), *Path);
				return false;
			}
		}
		OutClip.Duration = FMath::Max(OutClip.Duration, Track.Frames.Last().TimeSeconds);
	}
	return true;
}

bool FHandClip::Sample(EControllerHand Hand, double TimeSeconds, FHandFrame& Out) const
{
	const FHandClipTrack* Track = nullptr;
	for (const FHandClipTrack& Candidate : Tracks)
	{
		if (Candidate.Hand == Hand)
		{
			Track = &Candidate;
			break;
		}
	}
	if (Track == nullptr || Track->Frames.Num() == 0)
	{
		return false;
	}
	const TArray<FHandFrame>& Frames = Track->Frames;
	if (TimeSeconds <= Frames[0].TimeSeconds)
	{
		Out = Frames[0];
		Out.TimeSeconds = TimeSeconds;
		return true;
	}
	if (TimeSeconds >= Frames.Last().TimeSeconds)
	{
		Out = Frames.Last();
		Out.TimeSeconds = TimeSeconds;
		return true;
	}

	int32 Lo = 0;
	int32 Hi = Frames.Num();
	while (Lo < Hi)
	{
		const int32 Mid = (Lo + Hi) / 2;
		if (Frames[Mid].TimeSeconds < TimeSeconds)
		{
			Lo = Mid + 1;
		}
		else
		{
			Hi = Mid;
		}
	}
	const FHandFrame& Before = Frames[Lo - 1];
	const FHandFrame& After = Frames[Lo];
	const double Span = After.TimeSeconds - Before.TimeSeconds;
	const double Alpha = Span > 0.0 ? (TimeSeconds - Before.TimeSeconds) / Span : 0.0;

	Out.TimeSeconds = TimeSeconds;
	Out.Hand = Hand;
	Out.Confidence = FMath::Lerp(Before.Confidence, After.Confidence, Alpha);
	Out.Pinch = FMath::Lerp(Before.Pinch, After.Pinch, Alpha);
	// A bit cannot be blended. Either neighbour raising it blocks the sample, the safe side of the rule.
	Out.bSystemGesture = Before.bSystemGesture || After.bSystemGesture;
	Out.Joints.SetNum(MageHandJointCount);
	for (int32 Index = 0; Index < MageHandJointCount; ++Index)
	{
		Out.Joints[Index].Location = FMath::Lerp(Before.Joints[Index].Location, After.Joints[Index].Location, Alpha);
		Out.Joints[Index].Rotation = FQuat::Slerp(Before.Joints[Index].Rotation, After.Joints[Index].Rotation, Alpha);
	}
	return true;
}
