#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Hands/HandClip.h"
#include "Hands/HandClipPlayer.h"
#include "Hands/HandInputSubsystem.h"
#include "HeadMountedDisplayTypes.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

namespace
{
const TCHAR* GActions[] = {
	TEXT("sigil-line1"),
	TEXT("sigil-line2"),
	TEXT("sigil-line3"),
	TEXT("mudra"),
	TEXT("bolt"),
	TEXT("ward-raise"),
	TEXT("blink-left"),
	TEXT("blink-right"),
	TEXT("blink-back"),
	TEXT("staff-plant"),
	TEXT("staff-lift"),
};

const TCHAR* GVariantNames[] = {
	TEXT("normal"),
	TEXT("slow"),
	TEXT("sloppy"),
};

const EClipVariant GVariantValues[] = {
	EClipVariant::Normal,
	EClipVariant::Slow,
	EClipVariant::Sloppy,
};

const EHandKeypoint GKeypointOrder[MageHandJointCount] = {
	EHandKeypoint::Palm,
	EHandKeypoint::Wrist,
	EHandKeypoint::ThumbMetacarpal,
	EHandKeypoint::ThumbProximal,
	EHandKeypoint::ThumbDistal,
	EHandKeypoint::ThumbTip,
	EHandKeypoint::IndexMetacarpal,
	EHandKeypoint::IndexProximal,
	EHandKeypoint::IndexIntermediate,
	EHandKeypoint::IndexDistal,
	EHandKeypoint::IndexTip,
	EHandKeypoint::MiddleMetacarpal,
	EHandKeypoint::MiddleProximal,
	EHandKeypoint::MiddleIntermediate,
	EHandKeypoint::MiddleDistal,
	EHandKeypoint::MiddleTip,
	EHandKeypoint::RingMetacarpal,
	EHandKeypoint::RingProximal,
	EHandKeypoint::RingIntermediate,
	EHandKeypoint::RingDistal,
	EHandKeypoint::RingTip,
	EHandKeypoint::LittleMetacarpal,
	EHandKeypoint::LittleProximal,
	EHandKeypoint::LittleIntermediate,
	EHandKeypoint::LittleDistal,
	EHandKeypoint::LittleTip,
};

static_assert(UE_ARRAY_COUNT(GActions) == 11, "Eleven quick actions.");
static_assert(UE_ARRAY_COUNT(GVariantNames) == 3, "Three clip variants.");
static_assert(UE_ARRAY_COUNT(GVariantValues) == 3, "Three clip variants.");
static_assert(UE_ARRAY_COUNT(GKeypointOrder) == MageHandJointCount, "26 keypoints.");
static_assert(EHandKeypointCount == MageHandJointCount, "EHandKeypoint count.");

FString ClipsDirectory()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"));
}

FString ExpectedFileName(const TCHAR* Action, const TCHAR* Variant)
{
	return FString::Printf(TEXT("%s.%s.jsonl"), Action, Variant);
}

void UpdateDouble(FSHA1& Sha, double Value)
{
	Sha.Update(reinterpret_cast<const uint8*>(&Value), sizeof(double));
}

/** SHA1 of the t=0 emit plus every Step(1/90) until the clip duration. Not the clip's own 72 Hz grid, so samples are interpolated. */
FString DigestPlayback(FAutomationTestBase& Test, UHandClipPlayer* Player, const TCHAR* Label)
{
	if (!Player)
	{
		Test.AddError(FString::Printf(TEXT("%s: no player"), Label));
		return FString();
	}
	const FHandClip& Clip = Player->GetClip();
	if (Clip.GetHands().Num() == 0 || Clip.GetDuration() <= 0.0)
	{
		Test.AddError(FString::Printf(TEXT("%s: clip has no hands or no duration"), Label));
		return FString();
	}

	FSHA1 Sha;
	auto Consume = [&Sha, &Test, Player, Label]() -> bool
	{
		const uint64 Sequence = Player->GetFrameSequence();
		Sha.Update(reinterpret_cast<const uint8*>(&Sequence), sizeof(Sequence));
		for (const EControllerHand Hand : Player->GetClip().GetHands())
		{
			FHandFrame Frame;
			if (!Player->GetLatest(Hand, Frame))
			{
				Test.AddError(FString::Printf(TEXT("%s: GetLatest failed"), Label));
				return false;
			}
			if (Frame.Joints.Num() != MageHandJointCount)
			{
				Test.AddError(FString::Printf(TEXT("%s: joint count %d"), Label, Frame.Joints.Num()));
				return false;
			}
			UpdateDouble(Sha, Frame.TimeSeconds);
			const uint8 HandByte = Hand == EControllerHand::Left ? 0 : 1;
			Sha.Update(&HandByte, 1);
			UpdateDouble(Sha, static_cast<double>(Frame.Confidence));
			UpdateDouble(Sha, static_cast<double>(Frame.Pinch));
			for (const FHandJointPose& Joint : Frame.Joints)
			{
				UpdateDouble(Sha, Joint.Location.X);
				UpdateDouble(Sha, Joint.Location.Y);
				UpdateDouble(Sha, Joint.Location.Z);
				UpdateDouble(Sha, Joint.Rotation.X);
				UpdateDouble(Sha, Joint.Rotation.Y);
				UpdateDouble(Sha, Joint.Rotation.Z);
				UpdateDouble(Sha, Joint.Rotation.W);
			}
		}
		return true;
	};

	if (!Consume())
	{
		return FString();
	}
	const double Duration = Player->GetDuration();
	const double Dt = 1.0 / 90.0;
	double Time = 0.0;
	while (Time + 1.0e-9 < Duration)
	{
		const double StepDt = FMath::Min(Dt, Duration - Time);
		Player->Step(StepDt);
		Time += StepDt;
		if (!Consume())
		{
			return FString();
		}
	}

	FSHAHash Hash;
	Sha.Final();
	Sha.GetHash(Hash.Hash);
	return Hash.ToString();
}

UHandClipPlayer* NewDirectPlayer(UObject* Outer)
{
	UHandClipPlayer* Player = NewObject<UHandClipPlayer>(Outer);
	Player->AddToRoot();
	return Player;
}

void DestroyPlayer(UHandClipPlayer* Player)
{
	if (!Player)
	{
		return;
	}
	Player->Stop();
	Player->RemoveFromRoot();
	Player->MarkAsGarbage();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaClipsLoadAll, "MageArena.Clips.LoadAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaClipsLoadAll::RunTest(const FString& Parameters)
{
	bool bPass = true;
	for (int32 Index = 0; Index < MageHandJointCount; ++Index)
	{
		bPass &= TestEqual(TEXT("EHandKeypoint order"), static_cast<int32>(GKeypointOrder[Index]), Index);
	}
	// EHandKeypoint is a plain enum, not a UENUM. The names the schema requires are the enumerator names.
	const TCHAR* ExpectedNames[MageHandJointCount] = {
		TEXT("Palm"), TEXT("Wrist"),
		TEXT("ThumbMetacarpal"), TEXT("ThumbProximal"), TEXT("ThumbDistal"), TEXT("ThumbTip"),
		TEXT("IndexMetacarpal"), TEXT("IndexProximal"), TEXT("IndexIntermediate"), TEXT("IndexDistal"), TEXT("IndexTip"),
		TEXT("MiddleMetacarpal"), TEXT("MiddleProximal"), TEXT("MiddleIntermediate"), TEXT("MiddleDistal"), TEXT("MiddleTip"),
		TEXT("RingMetacarpal"), TEXT("RingProximal"), TEXT("RingIntermediate"), TEXT("RingDistal"), TEXT("RingTip"),
		TEXT("LittleMetacarpal"), TEXT("LittleProximal"), TEXT("LittleIntermediate"), TEXT("LittleDistal"), TEXT("LittleTip"),
	};
	for (int32 Index = 0; Index < MageHandJointCount; ++Index)
	{
		bPass &= TestEqual(TEXT("schema keypoint name"), FString(FHandClip::KeypointName(Index)), FString(ExpectedNames[Index]));
	}

	TArray<FString> Expected;
	Expected.Reserve(UE_ARRAY_COUNT(GActions) * UE_ARRAY_COUNT(GVariantNames));
	for (const TCHAR* Action : GActions)
	{
		for (const TCHAR* Variant : GVariantNames)
		{
			Expected.Add(ExpectedFileName(Action, Variant));
		}
	}
	bPass &= TestEqual(TEXT("expected clip count"), Expected.Num(), 33);

	TArray<FString> Found;
	IFileManager::Get().FindFiles(Found, *FPaths::Combine(ClipsDirectory(), TEXT("*.jsonl")), true, false);
	Found.Sort();
	Expected.Sort();
	bPass &= TestEqual(TEXT("clip file count"), Found.Num(), Expected.Num());
	if (Found.Num() == Expected.Num())
	{
		for (int32 Index = 0; Index < Found.Num(); ++Index)
		{
			bPass &= TestEqual(TEXT("clip file name"), Found[Index], Expected[Index]);
		}
	}
	else
	{
		AddError(FString::Printf(TEXT("Clips directory %s has %d jsonl files"), *ClipsDirectory(), Found.Num()));
	}

	for (const FString& FileName : Expected)
	{
		const FString Path = FPaths::Combine(ClipsDirectory(), FileName);
		FHandClip Clip;
		FString Error;
		if (!TestTrue(*FString::Printf(TEXT("load %s"), *FileName), FHandClip::LoadFromFile(Path, Clip, Error)))
		{
			AddError(Error);
			bPass = false;
			continue;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s joints listed"), *FileName), Clip.GetHands().Num() > 0, true);
		for (const FHandClipTrack& Track : Clip.GetTracks())
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s has samples"), *FileName), Track.Frames.Num() >= 2);
			if (Track.Frames.Num() == 0)
			{
				continue;
			}
			bPass &= TestEqual(*FString::Printf(TEXT("%s starts at 0"), *FileName), Track.Frames[0].TimeSeconds, 0.0);
			for (int32 FrameIndex = 0; FrameIndex < Track.Frames.Num(); ++FrameIndex)
			{
				const FHandFrame& Frame = Track.Frames[FrameIndex];
				bPass &= TestEqual(*FString::Printf(TEXT("%s joint count"), *FileName), Frame.Joints.Num(), MageHandJointCount);
				if (FrameIndex > 0)
				{
					bPass &= TestTrue(*FString::Printf(TEXT("%s monotonic"), *FileName),
						Frame.TimeSeconds > Track.Frames[FrameIndex - 1].TimeSeconds);
				}
			}
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaClipsDeterministicReplay, "MageArena.Clips.DeterministicReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaClipsDeterministicReplay::RunTest(const FString& Parameters)
{
	bool bPass = true;
	UHandClipPlayer* Player = NewDirectPlayer(GetTransientPackage());
	for (const TCHAR* Action : GActions)
	{
		for (const TCHAR* Variant : GVariantNames)
		{
			const FString FileName = ExpectedFileName(Action, Variant);
			const FString Path = FPaths::Combine(ClipsDirectory(), FileName);
			FHandClip Clip;
			FString Error;
			if (!FHandClip::LoadFromFile(Path, Clip, Error))
			{
				AddError(FString::Printf(TEXT("%s: %s"), *FileName, *Error));
				bPass = false;
				continue;
			}
			Player->Play(Clip);
			const FString First = DigestPlayback(*this, Player, *FileName);
			Player->Play(Clip);
			const FString Second = DigestPlayback(*this, Player, *FileName);
			bPass &= TestFalse(*FString::Printf(TEXT("%s digest empty"), *FileName), First.IsEmpty());
			bPass &= TestEqual(*FString::Printf(TEXT("%s replay digest"), *FileName), First, Second);
		}
	}
	DestroyPlayer(Player);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaClipsQuickActionPath, "MageArena.Clips.QuickActionPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaClipsQuickActionPath::RunTest(const FString& Parameters)
{
	bool bPass = true;
	// A bare game instance, not Init(). Init() would boot every game-instance subsystem and rebind global net delegates.
	// PlayQuickAction is the function the key bindings call; the outer is a game instance so Within=GameInstance is satisfied.
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	UHandInputSubsystem* Hands = NewObject<UHandInputSubsystem>(Instance);
	UHandClipPlayer* Direct = NewDirectPlayer(Instance);

	for (const TCHAR* Action : GActions)
	{
		for (int32 VariantIndex = 0; VariantIndex < UE_ARRAY_COUNT(GVariantNames); ++VariantIndex)
		{
			const TCHAR* VariantName = GVariantNames[VariantIndex];
			const EClipVariant Variant = GVariantValues[VariantIndex];
			const FString Label = ExpectedFileName(Action, VariantName);

			Hands->PlayQuickAction(FName(Action), Variant);
			UHandClipPlayer* Played = Hands->GetClipPlayer();
			if (!Played || Played->GetClip().Action != Action || Played->GetClip().Variant != VariantName)
			{
				AddError(FString::Printf(TEXT("%s: PlayQuickAction did not start that clip"), *Label));
				bPass = false;
				continue;
			}
			const FString FromKeys = DigestPlayback(*this, Played, *Label);

			FHandClip Clip;
			FString Error;
			const FString Path = FHandClip::MakeFilePath(FName(Action), Variant);
			if (!FHandClip::LoadFromFile(Path, Clip, Error))
			{
				AddError(FString::Printf(TEXT("%s: %s"), *Label, *Error));
				bPass = false;
				continue;
			}
			Direct->Play(Clip);
			const FString FromFile = DigestPlayback(*this, Direct, *Label);
			bPass &= TestFalse(*FString::Printf(TEXT("%s digest empty"), *Label), FromKeys.IsEmpty());
			bPass &= TestEqual(*FString::Printf(TEXT("%s quick action matches direct replay"), *Label), FromKeys, FromFile);
		}
	}

	DestroyPlayer(Direct);
	if (Hands->GetClipPlayer())
	{
		Hands->GetClipPlayer()->Stop();
	}
	Hands->MarkAsGarbage();
	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaClipsNoEmitAfterEnd, "MageArena.Clips.NoEmitAfterEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaClipsNoEmitAfterEnd::RunTest(const FString& Parameters)
{
	UHandClipPlayer* Player = NewDirectPlayer(GetTransientPackage());
	FHandClip Clip;
	FString Error;
	const FString Path = FHandClip::MakeFilePath(TEXT("bolt"), EClipVariant::Normal);
	if (!FHandClip::LoadFromFile(Path, Clip, Error))
	{
		AddError(Error);
		DestroyPlayer(Player);
		return false;
	}
	if (Clip.GetHands().Num() == 0)
	{
		AddError(TEXT("bolt.normal has no hands"));
		DestroyPlayer(Player);
		return false;
	}

	int32 Emits = 0;
	Player->OnHandFrame().AddLambda([&Emits](const FHandFrame&)
	{
		++Emits;
	});
	Player->Play(Clip);

	int32 Guard = 0;
	while (Player->IsPlaying() && Guard < 1000)
	{
		Player->Step(1.0 / 90.0);
		++Guard;
	}

	const uint64 Sequence = Player->GetFrameSequence();
	const int32 EmitsAtEnd = Emits;
	TArray<double> LatestTimes;
	for (const EControllerHand Hand : Clip.GetHands())
	{
		FHandFrame Latest;
		if (!Player->GetLatest(Hand, Latest))
		{
			AddError(TEXT("GetLatest failed at the end of bolt.normal"));
			DestroyPlayer(Player);
			return false;
		}
		LatestTimes.Add(Latest.TimeSeconds);
	}

	Player->Step(1.0 / 90.0);
	Player->Step(0.5);
	Player->Step(1.0);

	bool bPass = true;
	bPass &= TestFalse(TEXT("playback finished"), Player->IsPlaying());
	bPass &= TestTrue(TEXT("sequence frozen after the end"), Player->GetFrameSequence() == Sequence);
	bPass &= TestEqual(TEXT("no emits after the end"), Emits, EmitsAtEnd);
	bPass &= TestTrue(TEXT("the clip itself emitted"), EmitsAtEnd > 0);
	for (int32 Index = 0; Index < Clip.GetHands().Num(); ++Index)
	{
		FHandFrame After;
		if (!Player->GetLatest(Clip.GetHands()[Index], After))
		{
			AddError(TEXT("GetLatest failed after extra steps"));
			bPass = false;
			break;
		}
		bPass &= TestEqual(TEXT("latest time unchanged"), After.TimeSeconds, LatestTimes[Index]);
	}
	DestroyPlayer(Player);
	return bPass;
}

#endif
