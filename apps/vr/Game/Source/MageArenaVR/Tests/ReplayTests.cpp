#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "Kernel/ArenaKernel.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"

namespace
{
struct FSessionRig
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

	void Close(FArenaSession& Session)
	{
		Session.Unbind();
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance = nullptr;
		}
	}
};

struct FReplayEvent {
	int32 Tick = 0;
	double SimTimeS = 0.0;
	FString Kind;
	int32 ActorId = 0;
	int32 TargetId = -1;

	FString ToJson() const {
		return FString::Printf(TEXT("{\"tick\":%d,\"simTimeS\":%.3f,\"kind\":\"%s\",\"actorId\":%d,\"targetId\":%d}"),
			Tick, SimTimeS, *Kind, ActorId, TargetId);
	}
};

bool GenerateReplayLog(FSessionRig& Rig, uint32 Seed, FString& OutLogContent, FString& OutHash)
{
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
	Session.SetScripted(true);
	Session.SetBout(2, true); // Fire duel
	if (!Session.Start(Seed)) return false;

	int32 Cursor = 0;
	int32 LastBlocks = 0;
	int32 LastPlants = 0;
	TArray<FString> LogLines;

	while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < 120.0)
	{
		Session.Advance(1.0 / 72.0, true);

		for (int32 i = Cursor; i < Session.GetGames().State.Events.Num(); ++i)
		{
			const FArenaEvent& E = Session.GetGames().State.Events[i];
			FString MappedKind = E.Kind;
			if (E.Kind == TEXT("roll")) MappedKind = TEXT("blink");
			else if (E.Kind == TEXT("down")) MappedKind = TEXT("death");

			if (MappedKind == TEXT("cast") || MappedKind == TEXT("hit") || MappedKind == TEXT("perfect") ||
				MappedKind == TEXT("blink") || MappedKind == TEXT("death"))
			{
				FReplayEvent ReplayEv;
				ReplayEv.Tick = E.Tick;
				ReplayEv.SimTimeS = E.Tick / 60.0;
				ReplayEv.Kind = MappedKind;
				ReplayEv.ActorId = E.ActorId;
				ReplayEv.TargetId = E.TargetId.Get(-1);
				LogLines.Add(ReplayEv.ToJson());
			}
		}
		Cursor = Session.GetGames().State.Events.Num();

		const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
		if (Player && Player->Metrics.Blocks > LastBlocks)
		{
			for (int i = 0; i < Player->Metrics.Blocks - LastBlocks; ++i)
			{
				FReplayEvent ReplayEv;
				ReplayEv.Tick = Session.GetGames().State.Tick;
				ReplayEv.SimTimeS = Session.GetSimSeconds();
				ReplayEv.Kind = TEXT("absorb");
				ReplayEv.ActorId = Player->Id;
				LogLines.Add(ReplayEv.ToJson());
			}
			LastBlocks = Player->Metrics.Blocks;
		}

		if (Session.GetStaffPlants() > LastPlants)
		{
			for (int i = 0; i < Session.GetStaffPlants() - LastPlants; ++i)
			{
				FReplayEvent ReplayEv;
				ReplayEv.Tick = Session.GetGames().State.Tick;
				ReplayEv.SimTimeS = Session.GetSimSeconds();
				ReplayEv.Kind = TEXT("wall");
				ReplayEv.ActorId = Session.GetGames().PlayerId;
				LogLines.Add(ReplayEv.ToJson());
			}
			LastPlants = Session.GetStaffPlants();
		}
	}

	OutHash = StateHash(Session.GetGames().State);
	OutLogContent = FString::Join(LogLines, TEXT("\n")) + TEXT("\n");
	return true;
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaReplayDeterministic, "MageArena.Replay.Deterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMageArenaReplayDeterministic::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this)) return false;

	FString Log1, Hash1;
	if (!GenerateReplayLog(Rig, 1, Log1, Hash1))
	{
		AddError(TEXT("First run failed"));
		return false;
	}

	FString Log2, Hash2;
	if (!GenerateReplayLog(Rig, 1, Log2, Hash2))
	{
		AddError(TEXT("Second run failed"));
		return false;
	}

	bool bPass = true;
	bPass &= TestEqual(TEXT("state hashes match"), Hash1, Hash2);
	bPass &= TestEqual(TEXT("event logs match"), Log1, Log2);

	if (bPass)
	{
		FString RepoDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../..")));
		FString OutputPath = FPaths::Combine(RepoDir, TEXT("runs/T16/events.jsonl"));
		FFileHelper::SaveStringToFile(Log1, *OutputPath);
	}

	FArenaSession Dummy;
	Rig.Close(Dummy);
	return bPass;
}

#endif // WITH_DEV_AUTOMATION_TESTS
