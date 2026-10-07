#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"

namespace
{
const double kDt = 1.0 / 72.0;

struct FPauseRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;

	bool Open(FAutomationTestBase& Test)
	{
		Instance = NewObject<UGameInstance>(GetTransientPackage());
		Instance->AddToRoot();
		Hands = NewObject<UHandInputSubsystem>(Instance);
		Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
		Sigils->LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
		Wards = NewObject<UWardDetectorSubsystem>(Instance);
		FString Error;
		if (!Wards->InitDetector(Error))
		{
			Test.AddError(Error);
			return false;
		}
		Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
		Staff = NewObject<UStaffDetectorSubsystem>(Instance);
		return true;
	}

	void Bind(FArenaSession& Session) const
	{
		Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
	}

	void Close(FArenaSession& Session)
	{
		Session.Unbind();
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
			Instance = nullptr;
		}
	}
};

int32 CountExact(const TArray<FString>& Chain, const TCHAR* Line)
{
	int32 Count = 0;
	for (const FString& Entry : Chain)
	{
		if (Entry == Line)
		{
			++Count;
		}
	}
	return Count;
}

TArray<int32> ResolveTicks(const FArenaSession& Session)
{
	TArray<int32> Ticks;
	for (const FTelegraph& Telegraph : Session.GetGames().State.Telegraphs)
	{
		Ticks.Add(Telegraph.ResolveTick);
	}
	return Ticks;
}

bool SameTicks(const TArray<int32>& Left, const TArray<int32>& Right)
{
	if (Left.Num() != Right.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Left.Num(); ++Index)
	{
		if (Left[Index] != Right[Index])
		{
			return false;
		}
	}
	return true;
}

FString SaveDir()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArena"), FGuid::NewGuid().ToString());
}

bool FreezeHolds(FAutomationTestBase& Test, FArenaSession& Session, const TCHAR* Label)
{
	const FString Hash = Session.GetStateHash();
	const int32 Tick = Session.GetGames().State.Tick;
	const TArray<int32> Resolves = ResolveTicks(Session);
	bool bPass = Test.TestTrue(*FString::Printf(TEXT("%s paused"), Label), Session.IsPaused());
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Session.Advance(SimDt(), true);
	}
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s hash"), Label), Session.GetStateHash(), Hash);
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s tick"), Label), Session.GetGames().State.Tick, Tick);
	bPass &= Test.TestTrue(*FString::Printf(TEXT("%s telegraph resolve ticks"), Label), SameTicks(Resolves, ResolveTicks(Session)));
	bPass &= Test.TestTrue(*FString::Printf(TEXT("%s reason"), Label), CountExact(Session.GetChain(), *FString::Printf(TEXT("Session pause reason=%s"), Label)) == 1);
	return bPass;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPauseFreeze, "MageArena.Pause.FreezesKernel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPauseFreeze::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPauseRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	auto Live = [&Rig](FArenaSession& Session) -> bool
	{
		Rig.Bind(Session);
		Session.SetScripted(true);
		if (!Session.Start(1))
		{
			return false;
		}
		for (int32 Frame = 0; Frame < 1200 && Session.GetGames().State.Telegraphs.Num() == 0; ++Frame)
		{
			Session.Advance(SimDt(), true);
		}
		return Session.GetGames().State.Telegraphs.Num() > 0 && Session.GetStage() == TEXT("active");
	};

	FArenaSession Focus;
	bool bPass = TestTrue(TEXT("focus bout has a telegraph"), Live(Focus));
	const int32 FocusTick = Focus.GetGames().State.Tick;
	Focus.NotifyFocusLost();
	bPass &= FreezeHolds(*this, Focus, TEXT("focus"));
	bPass &= TestEqual(TEXT("focus did not step on the pause frame"), Focus.GetGames().State.Tick, FocusTick);
	Focus.Unbind();

	FArenaSession Headset;
	bPass &= TestTrue(TEXT("headset bout has a telegraph"), Live(Headset));
	Headset.NotifyHeadsetRemoved();
	bPass &= FreezeHolds(*this, Headset, TEXT("headset"));
	Headset.NotifyFocusLost();
	bPass &= TestEqual(TEXT("a second trigger does not re-enter"), CountExact(Headset.GetChain(), TEXT("Session pause reason=focus")), 0);
	Headset.TogglePause();
	bPass &= TestFalse(TEXT("Esc resumes immediately"), Headset.IsPaused());
	bPass &= TestFalse(TEXT("Esc clears the count"), Headset.IsResumeCounting());
	const int32 EscTick = Headset.GetGames().State.Tick;
	const FString EscHash = Headset.GetStateHash();
	Headset.Advance(SimDt(), true);
	bPass &= TestEqual(TEXT("Esc resume steps one tick"), Headset.GetGames().State.Tick, EscTick + 1);
	bPass &= TestFalse(TEXT("that tick changes the hash"), Headset.GetStateHash() == EscHash);
	Headset.Unbind();

	Rig.Close(Focus);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPauseTracking, "MageArena.Pause.TrackingLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPauseTracking::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPauseRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	const double LossS = 1.5;
	const FString ColdDir = SaveDir();
	FArenaSession Cold;
	Rig.Bind(Cold);
	Cold.SetSaveDirectory(ColdDir);
	Cold.SetScripted(false);
	bool bPass = TestTrue(TEXT("cold arc"), Cold.BeginArc(1));
	bPass &= TestTrue(TEXT("tracking loss is 1.5 s"), FMath::IsNearlyEqual(Cold.GetTuning().TrackingLossS, LossS, 1.0e-9));
	Rig.Hands->SetTrackingDropped(true);
	double ColdElapsed = 0.0;
	while (ColdElapsed + 1.0e-9 < LossS + 1.0)
	{
		Cold.Advance(SimDt(), true);
		ColdElapsed += SimDt();
		if (Cold.IsPaused())
		{
			break;
		}
	}
	bPass &= TestFalse(TEXT("tracking loss does not pause the cold start"), Cold.IsPaused());
	bPass &= TestEqual(TEXT("still cold"), Cold.GetStage(), FString(TEXT("cold")));
	Cold.Unbind();

	const FString TeachDir = SaveDir();
	FArenaSession Teach;
	Rig.Bind(Teach);
	Teach.SetSaveDirectory(TeachDir);
	Teach.SetScripted(false);
	bPass &= TestTrue(TEXT("teach arc"), Teach.BeginArc(1));
	Rig.Hands->SetTrackingDropped(false);
	Rig.Hands->PlayQuickAction(TEXT("ward-raise"), EClipVariant::Normal);
	double Enter = 0.0;
	// T21: the first-launch prologue (10 s, day.json prologueMaxS) comes between cold and the teach.
	while (Teach.GetStage() != TEXT("teach") && Enter < 16.0)
	{
		Teach.Advance(kDt, true);
		Enter += kDt;
	}
	bPass &= TestEqual(TEXT("entered the teach"), Teach.GetStage(), FString(TEXT("teach")));
	double Quiet = 0.0;
	while (Quiet < 0.75)
	{
		Teach.Advance(kDt, true);
		Quiet += kDt;
	}
	bPass &= TestEqual(TEXT("glob is not spawned yet"), Teach.GetGames().State.Projectiles.Num(), 0);
	Rig.Hands->SetTrackingDropped(true);
	double Dropped = 0.0;
	while (Dropped + 1.0e-9 < LossS + 0.5)
	{
		Teach.Advance(SimDt(), true);
		Dropped += SimDt();
	}
	bPass &= TestFalse(TEXT("tracking loss does not pause the teach"), Teach.IsPaused());
	bPass &= TestEqual(TEXT("teach continues"), Teach.GetStage(), FString(TEXT("teach")));
	Teach.Unbind();

	FArenaSession Combat;
	Rig.Bind(Combat);
	Combat.SetScripted(true);
	bPass &= TestTrue(TEXT("combat start"), Combat.Start(1));
	bPass &= TestTrue(TEXT("combat uses the 1.5 s default"), FMath::IsNearlyEqual(Combat.GetTuning().TrackingLossS, LossS, 1.0e-9));
	bPass &= TestTrue(TEXT("teach.json loaded for combat"), Combat.GetTuning().bLoaded);
	// The drop flag is not what the session reads. It tells the clip players to emit a
	// confidence-0 frame for each hand on every Step, including while idle. 84 sim frames
	// are 84/60 = 1.4 s and must not pause. 90 frames are 90/60 = 1.5 s and must.
	Rig.Hands->SetTrackingDropped(true);
	Combat.Advance(SimDt(), true);
	FHandFrame LeftFrame;
	FHandFrame RightFrame;
	bPass &= TestTrue(TEXT("clip player fed an untracked left frame"),
		Rig.Hands->GetLatest(EControllerHand::Left, LeftFrame) && LeftFrame.Confidence == 0.0f);
	bPass &= TestTrue(TEXT("clip player fed an untracked right frame"),
		Rig.Hands->GetLatest(EControllerHand::Right, RightFrame) && RightFrame.Confidence == 0.0f);
	for (int32 Frame = 1; Frame < 84; ++Frame)
	{
		Combat.Advance(SimDt(), true);
	}
	bPass &= TestFalse(TEXT("1.4 s of untracked frames does not pause"), Combat.IsPaused());
	for (int32 Frame = 84; Frame < 89; ++Frame)
	{
		Combat.Advance(SimDt(), true);
	}
	// 89/60 = 1.4833 s is still under the 1.5 s threshold: the pause must land on frame 90, not earlier.
	bPass &= TestFalse(TEXT("89 untracked frames (1.483 s) does not pause"), Combat.IsPaused());
	Combat.Advance(SimDt(), true);
	bPass &= TestTrue(TEXT("1.5 s of untracked frames pauses"), Combat.IsPaused());
	bPass &= TestEqual(TEXT("tracking reason"), CountExact(Combat.GetChain(), TEXT("Session pause reason=tracking")), 1);
	const FString Hash = Combat.GetStateHash();
	const int32 Tick = Combat.GetGames().State.Tick;
	Combat.Advance(SimDt(), true);
	bPass &= TestEqual(TEXT("paused combat does not step"), Combat.GetStateHash(), Hash);
	bPass &= TestEqual(TEXT("paused combat tick"), Combat.GetGames().State.Tick, Tick);

	Rig.Hands->SetTrackingDropped(false);
	Combat.Advance(LossS + 0.5, true);
	bPass &= TestEqual(TEXT("clearing the drop does not pause a second time"), CountExact(Combat.GetChain(), TEXT("Session pause reason=tracking")), 1);
	bPass &= TestTrue(TEXT("the combat pause stays until both palms"), Combat.IsPaused());
	IFileManager::Get().DeleteDirectory(*ColdDir, false, true);
	IFileManager::Get().DeleteDirectory(*TeachDir, false, true);
	Rig.Close(Combat);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPauseResume, "MageArena.Pause.ResumeCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPauseResume::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPauseRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetScripted(true);
	if (!Session.Start(1))
	{
		AddError(TEXT("Start failed"));
		Rig.Close(Session);
		return false;
	}
	for (int32 Frame = 0; Frame < 1200 && Session.GetGames().State.Telegraphs.Num() == 0; ++Frame)
	{
		Session.Advance(SimDt(), true);
	}
	bool bPass = TestTrue(TEXT("a telegraph is in flight"), Session.GetGames().State.Telegraphs.Num() > 0);
	const FString Hash = Session.GetStateHash();
	const int32 Tick = Session.GetGames().State.Tick;
	const TArray<int32> Resolves = ResolveTicks(Session);
	Session.NotifyHeadsetRemoved();
	for (int32 Frame = 0; Frame < 40; ++Frame)
	{
		Session.Advance(kDt, true);
	}
	bPass &= TestTrue(TEXT("still paused without both palms"), Session.IsPaused());
	bPass &= TestFalse(TEXT("the count has not started"), Session.IsResumeCounting());
	bPass &= TestEqual(TEXT("hash while waiting"), Session.GetStateHash(), Hash);

	Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
	int32 Arm = 0;
	while (!Session.IsResumeCounting() && Arm < 400)
	{
		Session.Advance(kDt, true);
		++Arm;
	}
	bPass &= TestTrue(TEXT("both palms start the count"), Session.IsResumeCounting());
	bPass &= TestEqual(TEXT("count opens on 3"), Session.GetResumeCount(), 3);
	bPass &= TestEqual(TEXT("count line"), CountExact(Session.GetChain(), TEXT("Session resume count")), 1);
	bPass &= TestEqual(TEXT("opening frame did not step"), Session.GetGames().State.Tick, Tick);

	const double CountS = 3.0;
	bPass &= TestTrue(TEXT("count is 3 s"), FMath::IsNearlyEqual(Session.GetTuning().CountStepS * Session.GetTuning().CountSteps, CountS, 1.0e-9));
	bool bSaw2 = false;
	bool bSaw1 = false;
	int32 Frames = 0;
	while (Session.IsPaused() && Frames < 500)
	{
		const int32 Shown = Session.GetResumeCount();
		bSaw2 = bSaw2 || Shown == 2;
		bSaw1 = bSaw1 || Shown == 1;
		Session.Advance(kDt, true);
		++Frames;
		if (Session.IsPaused())
		{
			bPass &= TestEqual(TEXT("kernel stays put during the count"), Session.GetGames().State.Tick, Tick);
		}
	}
	const double Elapsed = static_cast<double>(Frames) * kDt;
	bPass &= TestFalse(TEXT("count ends the pause"), Session.IsPaused());
	bPass &= TestTrue(TEXT("showed 2"), bSaw2);
	bPass &= TestTrue(TEXT("showed 1"), bSaw1);
	bPass &= TestTrue(*FString::Printf(TEXT("count lasted %.4f s (%d frames)"), Elapsed, Frames),
		Elapsed + 1.0e-6 >= CountS && (Frames - 1) * kDt + 1.0e-9 < CountS);
	bPass &= TestEqual(TEXT("the completing frame does not step"), Session.GetGames().State.Tick, Tick);
	bPass &= TestEqual(TEXT("hash before the resume step"), Session.GetStateHash(), Hash);
	bPass &= TestTrue(TEXT("telegraphs did not move during the count"), SameTicks(Resolves, ResolveTicks(Session)));

	Session.Advance(SimDt(), true);
	bPass &= TestEqual(TEXT("the first stepped tick is exactly one"), Session.GetGames().State.Tick, Tick + 1);
	bool bFutureHeld = true;
	const TArray<int32> AfterStep = ResolveTicks(Session);
	for (int32 Resolve : Resolves)
	{
		if (Resolve > Tick + 1 && !AfterStep.Contains(Resolve))
		{
			bFutureHeld = false;
		}
	}
	bPass &= TestTrue(TEXT("resolve ticks are the same absolute ticks"), bFutureHeld);
	bPass &= TestFalse(TEXT("one tick changes the hash"), Session.GetStateHash() == Hash);

	Session.NotifyHeadsetRemoved();
	const FString TeachHash = Session.GetStateHash();
	Rig.Hands->PlayQuickAction(TEXT("ward-raise"), EClipVariant::Normal);
	for (int32 Frame = 0; Frame < 90; ++Frame)
	{
		Session.Advance(kDt, true);
	}
	bPass &= TestTrue(TEXT("one palm does not start the count"), Session.IsPaused() && !Session.IsResumeCounting());
	bPass &= TestEqual(TEXT("one palm does not step"), Session.GetStateHash(), TeachHash);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPauseSave, "MageArena.Pause.SaveContinue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPauseSave::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPauseRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	const FString Dir = SaveDir();
	const FString Path = FPaths::Combine(Dir, TEXT("bout.txt"));
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetSaveDirectory(Dir);
	Session.SetBout(2, false);
	bool bPass = TestTrue(TEXT("start wave 3"), Session.Start(9));
	bPass &= TestEqual(TEXT("live wave index"), Session.GetGames().Wave, 2);
	Session.NotifyQuit();
	FString Saved;
	bPass &= TestTrue(TEXT("save file exists"), FFileHelper::LoadFileToString(Saved, *Path));
	bPass &= TestTrue(TEXT("save stores bout=2"), Saved.Contains(TEXT("bout=2")));
	Session.Unbind();

	FArenaSession Teach;
	Rig.Bind(Teach);
	Teach.SetSaveDirectory(Dir);
	Teach.SetScripted(false);
	bPass &= TestTrue(TEXT("teach does not save"), Teach.BeginArc(9));
	bPass &= TestEqual(TEXT("offer"), Teach.GetStage(), FString(TEXT("offer")));
	Teach.NotifyQuit();
	FString Still;
	bPass &= TestTrue(TEXT("teach quit leaves the file"), FFileHelper::LoadFileToString(Still, *Path));
	bPass &= TestTrue(TEXT("file still says bout=2"), Still.Contains(TEXT("bout=2")));
	bPass &= TestEqual(TEXT("offer bout"), Teach.GetBoutIndex(), 2);
	bPass &= TestEqual(TEXT("offer prompt"), Teach.GetPromptText(), FString(TEXT("Raise your palm to continue. Both palms to start over.")));
	Teach.Unbind();

	FArenaSession Resume;
	Rig.Bind(Resume);
	Resume.SetSaveDirectory(Dir);
	Resume.SetScripted(false);
	bPass &= TestTrue(TEXT("offer again"), Resume.BeginArc(9));
	Rig.Hands->PlayQuickAction(TEXT("ward-raise"), EClipVariant::Normal);
	double Elapsed = 0.0;
	while (Resume.GetStage() == TEXT("offer") && Elapsed < 4.0)
	{
		Resume.Advance(kDt, true);
		Elapsed += kDt;
	}
	// T21: the palm continues into the collar ritual (every launch), then the saved bout. The save stays until the day
	// moves on: it also records that the teach is done.
	bPass &= TestEqual(TEXT("palm continues to the ritual"), Resume.GetStage(), FString(TEXT("ritual")));
	bPass &= TestEqual(TEXT("continued bout"), Resume.GetBoutIndex(), 2);
	FString Kept;
	bPass &= TestTrue(TEXT("continue keeps the save"), FFileHelper::LoadFileToString(Kept, *Path));
	bPass &= TestTrue(TEXT("kept save still says bout=2"), Kept.Contains(TEXT("bout=2")));
	Resume.Unbind();

	FFileHelper::SaveStringToFile(TEXT("bout=1\n"), *Path);
	FArenaSession Restart;
	Rig.Bind(Restart);
	Restart.SetSaveDirectory(Dir);
	Restart.SetScripted(false);
	bPass &= TestTrue(TEXT("offer for restart"), Restart.BeginArc(4));
	bPass &= TestEqual(TEXT("saved bout is 1"), Restart.GetBoutIndex(), 1);
	Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
	Elapsed = 0.0;
	while (Restart.GetStage() == TEXT("offer") && Elapsed < 4.0)
	{
		Restart.Advance(kDt, true);
		Elapsed += kDt;
	}
	bPass &= TestEqual(TEXT("both palms restart the teach"), Restart.GetStage(), FString(TEXT("teach")));
	bPass &= TestEqual(TEXT("restart bout is wave 1"), Restart.GetBoutIndex(), 0);
	bPass &= TestFalse(TEXT("restart deletes the save"), IFileManager::Get().FileExists(*Path));
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Rig.Close(Restart);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPauseTeachResume, "MageArena.Pause.TeachResumes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPauseTeachResume::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPauseRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	const FString Dir = SaveDir();
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetSaveDirectory(Dir);
	Session.SetScripted(false);
	bool bPass = TestTrue(TEXT("arc"), Session.BeginArc(1));
	Rig.Hands->PlayQuickAction(TEXT("ward-raise"), EClipVariant::Normal);
	double Elapsed = 0.0;
	// T21: the first-launch prologue (10 s) comes between cold and the teach.
	while (Session.GetStage() != TEXT("teach") && Elapsed < 16.0)
	{
		Session.Advance(kDt, true);
		Elapsed += kDt;
	}
	bPass &= TestEqual(TEXT("ward step"), Session.GetTeachStep(), FString(TEXT("ward")));
	const double Clock = Session.GetTeachClock();
	const FString Hash = Session.GetStateHash();
	Session.NotifyHeadsetRemoved();
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Session.Advance(kDt, true);
	}
	bPass &= TestTrue(TEXT("teach pauses"), Session.IsPaused());
	bPass &= TestEqual(TEXT("teach clock frozen"), Session.GetTeachClock(), Clock);
	bPass &= TestEqual(TEXT("teach hash frozen"), Session.GetStateHash(), Hash);
	bPass &= TestEqual(TEXT("same teach step"), Session.GetTeachStep(), FString(TEXT("ward")));

	Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
	int32 Frames = 0;
	while (Session.IsPaused() && Frames < 600)
	{
		Session.Advance(kDt, true);
		++Frames;
	}
	bPass &= TestFalse(TEXT("teach resumes"), Session.IsPaused());
	bPass &= TestEqual(TEXT("resumed the ward step"), Session.GetTeachStep(), FString(TEXT("ward")));
	bPass &= TestEqual(TEXT("stage is still the teach"), Session.GetStage(), FString(TEXT("teach")));
	Session.Advance(kDt, true);
	bPass &= TestTrue(TEXT("the teach clock moves again"), Session.GetTeachClock() > Clock);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPauseSaveReject, "MageArena.Pause.SaveRejects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPauseSaveReject::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FPauseRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}

	auto Cold = [&](const FString& Body, bool bWrite, const TCHAR* Label) -> bool
	{
		const FString Dir = SaveDir();
		IFileManager::Get().MakeDirectory(*Dir, true);
		if (bWrite && !FFileHelper::SaveStringToFile(Body, *FPaths::Combine(Dir, TEXT("bout.txt"))))
		{
			AddError(FString::Printf(TEXT("%s: could not write the save"), Label));
			IFileManager::Get().DeleteDirectory(*Dir, false, true);
			return false;
		}
		FArenaSession Session;
		Rig.Bind(Session);
		Session.SetSaveDirectory(Dir);
		bool bOk = TestTrue(*FString::Printf(TEXT("%s begins"), Label), Session.BeginArc(1));
		bOk &= TestEqual(*FString::Printf(TEXT("%s falls back to the teach start"), Label), Session.GetStage(), FString(TEXT("cold")));
		bOk &= TestEqual(*FString::Printf(TEXT("%s bout is wave 1"), Label), Session.GetBoutIndex(), 0);
		Session.Unbind();
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
		return bOk;
	};

	auto Offer = [&](const FString& Body, int32 Bout, const TCHAR* Label) -> bool
	{
		const FString Dir = SaveDir();
		IFileManager::Get().MakeDirectory(*Dir, true);
		if (!FFileHelper::SaveStringToFile(Body, *FPaths::Combine(Dir, TEXT("bout.txt"))))
		{
			AddError(FString::Printf(TEXT("%s: could not write the save"), Label));
			IFileManager::Get().DeleteDirectory(*Dir, false, true);
			return false;
		}
		FArenaSession Session;
		Rig.Bind(Session);
		Session.SetSaveDirectory(Dir);
		bool bOk = TestTrue(*FString::Printf(TEXT("%s begins"), Label), Session.BeginArc(1));
		bOk &= TestEqual(*FString::Printf(TEXT("%s offers continue"), Label), Session.GetStage(), FString(TEXT("offer")));
		bOk &= TestEqual(*FString::Printf(TEXT("%s bout"), Label), Session.GetBoutIndex(), Bout);
		Session.Unbind();
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
		return bOk;
	};

	bool bPass = Cold(FString(), false, TEXT("missing file"));
	bPass &= Cold(TEXT("hello"), true, TEXT("corrupt"));
	bPass &= Cold(TEXT("bout="), true, TEXT("empty number"));
	bPass &= Cold(TEXT("bout=1.5"), true, TEXT("fraction"));
	bPass &= Cold(TEXT("bout=02"), true, TEXT("leading zero"));
	bPass &= Cold(TEXT("bout=999abc"), true, TEXT("trailing junk"));
	bPass &= Cold(TEXT("bout=-"), true, TEXT("lone minus"));
	bPass &= Cold(TEXT("bout=-1"), true, TEXT("negative"));
	bPass &= Cold(TEXT("bout=4"), true, TEXT("past the last Tiro wave"));
	bPass &= Cold(TEXT("bout=999"), true, TEXT("bout 999"));
	bPass &= Offer(TEXT("bout=0\n"), 0, TEXT("first wave"));
	bPass &= Offer(TEXT("bout=3\n"), 3, TEXT("last Tiro wave"));

	FArenaSession Guard;
	Rig.Bind(Guard);
	// Refusing to spawn is logged at Error. Those lines are the expected refusal.
	AddExpectedError(TEXT("Session bout 999 is outside Tiro waves"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Session bout -1 is outside Tiro waves"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Session bout 4 is outside Tiro waves"), EAutomationExpectedErrorFlags::Contains, 1);
	Guard.SetBout(999, false);
	bPass &= TestFalse(TEXT("bout 999 does not spawn"), Guard.Start(1));
	Guard.SetBout(-1, false);
	bPass &= TestFalse(TEXT("a negative bout does not spawn"), Guard.Start(1));
	Guard.SetBout(4, false);
	bPass &= TestFalse(TEXT("bout 4 does not spawn"), Guard.Start(1));
	Guard.SetBout(3, false);
	bPass &= TestTrue(TEXT("the last Tiro wave still starts"), Guard.Start(1));
	bPass &= TestEqual(TEXT("started wave index 3"), Guard.GetGames().Wave, 3);
	Rig.Close(Guard);
	return bPass;
}

#endif
