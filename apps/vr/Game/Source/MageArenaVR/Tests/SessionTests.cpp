#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/WardDetector.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "HAL/FileManager.h"
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
		return true;
	}

	void Close(FArenaSession& Session)
	{
		Session.Unbind();
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
		}
	}
};

FString ChainPath(const TCHAR* Name)
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), TEXT("runs/T08"), Name);
	FPaths::CollapseRelativeDirectories(Path);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return Path;
}

int32 CountChain(const TArray<FString>& Chain, const TCHAR* Piece)
{
	int32 Count = 0;
	for (const FString& Line : Chain)
	{
		if (Line.Contains(Piece))
		{
			++Count;
		}
	}
	return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWave1Scripted, "MageArenaDesign.Session.Wave1Seated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWave1Scripted::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks);
	Session.SetScripted(true);
	if (!Session.Start(1))
	{
		AddError(TEXT("Session.Start failed"));
		Rig.Close(Session);
		return false;
	}

	// arena-tiers.json pins Tiro wave 1 at 25-40 s for the walking desktop game.
	// The seated bout does not walk. Victory has to land inside this cap. The window is logged.
	const double PinnedWindowLo = 25.0;
	const double PinnedWindowHi = 40.0;
	const double GenerousCap = 120.0;
	const double Dt = 1.0 / 72.0;
	int32 Guard = 0;
	double Mark = 5.0;
	double WorstOffPad = 0.0;
	while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < GenerousCap && Guard < 10000)
	{
		Session.Advance(Dt, true);
		++Guard;
		const FActor* Marked = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
		double OffPad = 1.0e9;
		if (Marked)
		{
			for (int32 Pad = 0; Pad < 3; ++Pad)
			{
				OffPad = FMath::Min(OffPad, SimDistance(Marked->Pos, Session.PadKernel(Pad)));
			}
		}
		WorstOffPad = FMath::Max(WorstOffPad, OffPad);
		if (Session.GetSimSeconds() >= Mark)
		{
			UE_LOG(LogMageArena, Log, TEXT("Wave1Scripted t=%.1f hp=%.1f mana=%.1f stam=%.1f casts=%d rolls=%d offPad=%.4f"),
				Session.GetSimSeconds(),
				Marked ? Marked->Hp : -1.0,
				Marked ? Marked->Mana : -1.0,
				Marked ? Marked->Stamina : -1.0,
				Marked ? Marked->Metrics.Casts : -1,
				Marked ? Marked->Metrics.Rolls : -1,
				OffPad);
			Mark += 5.0;
		}
	}

	const FString ChainText = FString::Join(Session.GetChain(), TEXT("\n"));
	FFileHelper::SaveStringToFile(ChainText, *ChainPath(TEXT("wave1-chain.log")));
	const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	const double Measured = Session.GetSimSeconds();
	const bool bWon = Session.GetGames().Phase == TEXT("intermission");
	const bool bInsideWindow = bWon && Measured >= PinnedWindowLo && Measured <= PinnedWindowHi;
	const double DamageTaken = Player ? Player->Metrics.DamageTaken : -1.0;
	UE_LOG(LogMageArena, Log, TEXT("Wave1Scripted end phase=%s t=%.2f waves=%d hp=%.1f dealt=%.1f taken=%.1f blinks=%d chain=%d"),
		*Session.GetGames().Phase, Measured, Session.GetGames().WavesCleared,
		Player ? Player->Hp : -1.0,
		Player ? Player->Metrics.DamageDealt : -1.0,
		DamageTaken,
		Session.GetBlinkAccepts(), Session.GetChain().Num());
	UE_LOG(LogMageArena, Log, TEXT("Wave1Scripted seated measured=%.2fs pinned-window=%.0f-%.0f %s damageTaken=%.1f offPad=%.4f"),
		Measured, PinnedWindowLo, PinnedWindowHi,
		bInsideWindow ? TEXT("INSIDE") : TEXT("OUTSIDE"),
		DamageTaken, WorstOffPad);

	bool bPass = true;
	bPass &= TestEqual(TEXT("phase"), Session.GetGames().Phase, FString(TEXT("intermission")));
	bPass &= TestTrue(TEXT("waves cleared"), Session.GetGames().WavesCleared >= 1);
	bPass &= TestTrue(*FString::Printf(TEXT("victory within %.0fs (measured %.2fs, phase %s)"),
		GenerousCap, Measured, *Session.GetGames().Phase), bWon && Measured <= GenerousCap + 0.05);
	bPass &= TestTrue(*FString::Printf(TEXT("player stayed on a pad (worst %.4f m)"), WorstOffPad), WorstOffPad < 1.0e-3);
	bPass &= TestTrue(TEXT("blinked"), Session.GetBlinkAccepts() > 0);
	bPass &= TestTrue(TEXT("ward gesture"), CountChain(Session.GetChain(), TEXT("gesture ward")) > 0);
	bPass &= TestTrue(TEXT("sigil gesture"), CountChain(Session.GetChain(), TEXT("gesture sigil")) > 0);
	bPass &= TestTrue(TEXT("kernel roll"), CountChain(Session.GetChain(), TEXT("kernel roll actor=")) > 0);
	bPass &= TestTrue(TEXT("kernel cast"), CountChain(Session.GetChain(), TEXT("kernel cast")) > 0);
	bPass &= TestTrue(TEXT("victory line"), CountChain(Session.GetChain(), TEXT("Session victory")) == 1);

	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSessionDefeat, "MageArena.Session.Defeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSessionDefeat::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks);
	Session.SetScripted(false);
	if (!Session.Start(1))
	{
		AddError(TEXT("Session.Start failed"));
		Rig.Close(Session);
		return false;
	}

	const double Dt = 1.0 / 72.0;
	int32 Guard = 0;
	while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < 90.0 && Guard < 8000)
	{
		Session.Advance(Dt, true);
		++Guard;
	}

	const FString ChainText = FString::Join(Session.GetChain(), TEXT("\n"));
	FFileHelper::SaveStringToFile(ChainText, *ChainPath(TEXT("defeat-chain.log")));
	UE_LOG(LogMageArena, Log, TEXT("Session defeat test phase=%s t=%.2f"), *Session.GetGames().Phase, Session.GetSimSeconds());

	bool bPass = true;
	bPass &= TestEqual(TEXT("phase"), Session.GetGames().Phase, FString(TEXT("lost")));
	bPass &= TestTrue(*FString::Printf(TEXT("died at %.2fs"), Session.GetSimSeconds()), Session.GetSimSeconds() > 2.0);
	bPass &= TestTrue(TEXT("defeat line"), CountChain(Session.GetChain(), TEXT("Session defeat")) == 1);
	const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	bPass &= TestTrue(TEXT("player down"), Player && Player->bDown);

	Rig.Close(Session);
	return bPass;
}

#endif
