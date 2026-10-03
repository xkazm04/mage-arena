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

struct FTeachRig
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

const FActor* PlayerOf(const FArenaSession& Session)
{
	return SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
}

int32 CountExact(const TArray<FString>& Chain, const FString& Line)
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

int32 CountPrefix(const TArray<FString>& Chain, const TCHAR* Prefix)
{
	int32 Count = 0;
	for (const FString& Entry : Chain)
	{
		if (Entry.StartsWith(Prefix))
		{
			++Count;
		}
	}
	return Count;
}

bool Near(double Actual, double Expected)
{
	return FMath::IsNearlyEqual(Actual, Expected, 1.0e-6);
}

bool AdvanceUntil(FArenaSession& Session, TFunctionRef<bool()> Done, double LimitS)
{
	double Elapsed = 0.0;
	while (Elapsed < LimitS)
	{
		if (Done())
		{
			return true;
		}
		Session.Advance(kDt, true);
		Elapsed += kDt;
	}
	return Done();
}

FString PrivateSaveDir()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArena"), FGuid::NewGuid().ToString());
}

FString TeachLines(const TArray<FString>& Chain)
{
	FString Text;
	for (const FString& Line : Chain)
	{
		if (Line.StartsWith(TEXT("teach")) || Line.StartsWith(TEXT("Session defeat")) || Line.StartsWith(TEXT("cue bell")))
		{
			Text += Line + TEXT("\n");
		}
	}
	return Text;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaTeachOrder, "MageArena.Teach.ClipsInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaTeachOrder::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FTeachRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	const FString Dir = PrivateSaveDir();
	Rig.Bind(Session);
	Session.SetSaveDirectory(Dir);
	Session.SetScripted(true);
	if (!Session.BeginArc(1))
	{
		AddError(TEXT("BeginArc failed"));
		Rig.Close(Session);
		return false;
	}

	const double DistanceM = 8.0;
	const double SpeedMps = 4.0;
	const double WindupS = 1.0;
	const double RadiusM = 0.28;
	const double BlinkWindupS = 1.4;
	bool bImpact = false;
	bool bBlink = false;
	bool bLost = false;
	double WorstDamage = 0.0;
	bool bPass = true;
	bPass &= TestEqual(TEXT("cold prompt"), Session.GetPromptText(), FString(TEXT("Raise your palm to begin.")));
	bPass &= TestEqual(TEXT("cold stage"), Session.GetStage(), FString(TEXT("cold")));
	bPass &= TestTrue(TEXT("teach.json loaded"), Session.GetTuning().bLoaded);
	bPass &= TestTrue(TEXT("sim step is 60 Hz"), Near(KernelData().SimStepHz, 60.0));
	bPass &= TestTrue(TEXT("marker width from teach.json"), Near(Session.GetTuning().GlobMarkerWidthM, 0.6));
	bPass &= TestTrue(TEXT("range margin from teach.json"), Near(Session.GetTuning().GlobRangeMarginM, 4.0));
	bPass &= TestTrue(TEXT("glob settle from teach.json"), Near(Session.GetTuning().GlobSettleS, 0.8));
	bPass &= TestTrue(TEXT("perfect ring from teach.json"), Near(Session.GetTuning().PerfectRingS, 1.1));
	bPass &= TestTrue(TEXT("blink settle from teach.json"), Near(Session.GetTuning().BlinkSettleS, 0.5));

	double Elapsed = 0.0;
	while ((Session.GetStage() == TEXT("cold") || Session.GetStage() == TEXT("teach")) && Elapsed < 120.0)
	{
		if (!bImpact && Session.GetTeachStep() == TEXT("ward"))
		{
			const FTeachTuning& Tuning = Session.GetTuning();
			bPass &= TestTrue(TEXT("ward distance from teach.json"), Near(Tuning.WardDistanceM, DistanceM));
			bPass &= TestTrue(TEXT("glob speed from teach.json"), Near(Tuning.GlobSpeedMps, SpeedMps));
			bPass &= TestTrue(TEXT("ward windup from teach.json"), Near(Tuning.WardWindupS, WindupS));
			bPass &= TestTrue(TEXT("glob radius from teach.json"), Near(Tuning.GlobRadiusM, RadiusM));
			// teach.json: wardDistanceM 8, globSpeedMps 4, wardWindupS 1, globRadiusM 0.28.
			// Pinned mage radius 0.38 m. Gap = 8 - 0.38 - 0.28 = 7.34 m.
			// Windup ticks = ceil(1 * 60 - 1e-9) = 60. Move per tick = 4/60 m.
			// Moves = ceil(7.34 / (4/60) - 1e-9) = ceil(110.1) = 111.
			// Impact ticks = 60 + 111 - 1 = 170. Impact = 170/60 s.
			const double Expected = 170.0 / 60.0;
			bPass &= TestTrue(TEXT("impact is a real flight"), Session.GetScheduledImpactS() > 2.0);
			bPass &= TestTrue(*FString::Printf(TEXT("scheduled glob impact %.4f expected %.4f"), Session.GetScheduledImpactS(), Expected),
				Near(Session.GetScheduledImpactS(), Expected));
			bImpact = true;
		}
		if (!bBlink && Session.GetTeachStep() == TEXT("blink"))
		{
			// teach.json blinkWindupS 1.4. Ticks = ceil(1.4 * 60 - 1e-9) = 84. Impact = 84/60 s.
			const double Expected = 84.0 / 60.0;
			bPass &= TestTrue(TEXT("blink windup from teach.json"), Near(Session.GetTuning().BlinkWindupS, BlinkWindupS));
			bPass &= TestTrue(*FString::Printf(TEXT("scheduled lane impact %.4f expected %.4f"), Session.GetScheduledImpactS(), Expected),
				Near(Session.GetScheduledImpactS(), Expected));
			bBlink = true;
		}
		Session.Advance(kDt, true);
		Elapsed += kDt;
		if (Session.GetStage() == TEXT("lost"))
		{
			bLost = true;
		}
		if (const FActor* Player = PlayerOf(Session))
		{
			WorstDamage = FMath::Max(WorstDamage, Player->Metrics.DamageTaken);
			if (Player->bDown)
			{
				bLost = true;
			}
		}
	}

	const double Seconds = Session.GetTeachSeconds();
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_TEACH_MEASURED %.3f stage=%s"), Seconds, *Session.GetStage());
	bPass &= TestEqual(TEXT("wave 1 after the teach"), Session.GetStage(), FString(TEXT("active")));
	bPass &= TestEqual(TEXT("bout index"), Session.GetBoutIndex(), 0);
	bPass &= TestEqual(TEXT("games wave"), Session.GetGames().Wave, 0);
	bPass &= TestTrue(*FString::Printf(TEXT("teach seconds %.3f in 60-80"), Seconds), Seconds >= 60.0 && Seconds <= 80.0);
	bPass &= TestTrue(*FString::Printf(TEXT("no damage stuck (%.2f)"), WorstDamage), WorstDamage <= 0.0);
	bPass &= TestFalse(TEXT("teach never defeats"), bLost);
	bPass &= TestTrue(TEXT("checked the glob impact"), bImpact);
	bPass &= TestTrue(TEXT("checked the lane impact"), bBlink);
	bPass &= TestEqual(TEXT("no teach miss"), CountExact(Session.GetChain(), TEXT("teach miss")), 0);
	bPass &= TestEqual(TEXT("no defeat line"), CountPrefix(Session.GetChain(), TEXT("Session defeat")), 0);
	bPass &= TestTrue(TEXT("perfect bell"), CountExact(Session.GetChain(), TEXT("cue bell perfect-absorb")) >= 1);

	int32 At = -1;
	auto Take = [&](const TCHAR* Piece, bool bPrefix) -> bool
	{
		for (int32 Index = At + 1; Index < Session.GetChain().Num(); ++Index)
		{
			const FString& Line = Session.GetChain()[Index];
			if ((bPrefix && Line.StartsWith(Piece)) || (!bPrefix && Line == Piece))
			{
				At = Index;
				return true;
			}
		}
		AddError(FString::Printf(TEXT("missing %s after the previous teach line"), Piece));
		return false;
	};
	bPass &= Take(TEXT("teach cold"), false);
	bPass &= Take(TEXT("teach step ward"), false);
	bPass &= Take(TEXT("teach absorb"), false);
	bPass &= Take(TEXT("teach step perfect"), false);
	bPass &= TestTrue(TEXT("a perfect, not only a waiver"), CountExact(Session.GetChain(), TEXT("teach perfect")) >= 1);
	bPass &= Take(TEXT("teach perfect"), false);
	bPass &= Take(TEXT("teach step sigil"), false);
	bPass &= Take(TEXT("teach sigil hit"), false);
	bPass &= Take(TEXT("teach step blink"), false);
	bPass &= Take(TEXT("teach blink"), false);
	bPass &= Take(TEXT("teach complete"), false);
	bPass &= Take(TEXT("teach seconds="), true);
	if (!bPass)
	{
		AddError(TeachLines(Session.GetChain()));
	}
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaTeachMiss, "MageArena.Teach.MissRepeats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaTeachMiss::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FTeachRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	const FString Dir = PrivateSaveDir();
	Rig.Bind(Session);
	Session.SetSaveDirectory(Dir);
	Session.SetScripted(false);
	if (!Session.BeginArc(1))
	{
		AddError(TEXT("BeginArc failed"));
		Rig.Close(Session);
		return false;
	}
	Rig.Hands->PlayQuickAction(TEXT("ward-raise"), EClipVariant::Normal);
	const bool bEntered = AdvanceUntil(Session, [&Session]() { return Session.GetStage() == TEXT("teach"); }, 4.0);
	bool bPass = TestTrue(TEXT("ward raise enters the teach"), bEntered);
	const FActor* Entered = PlayerOf(Session);
	const double Hp = Entered ? Entered->Hp : -1.0;
	const double MaxHp = Entered ? Entered->MaxHp : -1.0;
	bPass &= TestTrue(TEXT("player has HP to lose"), MaxHp > 20.0 && Near(Hp, MaxHp));

	const bool bMissed = AdvanceUntil(Session, [&Session]()
	{
		return CountExact(Session.GetChain(), TEXT("teach miss")) >= 1;
	}, 8.0);
	const FActor* After = PlayerOf(Session);
	bPass &= TestTrue(TEXT("a missed glob repeats the ward"), bMissed);
	bPass &= TestEqual(TEXT("still the ward step"), Session.GetTeachStep(), FString(TEXT("ward")));
	bPass &= TestEqual(TEXT("still teaching"), Session.GetStage(), FString(TEXT("teach")));
	bPass &= TestTrue(TEXT("the miss was a real hit"), CountPrefix(Session.GetChain(), TEXT("kernel hit")) >= 1);
	bPass &= TestTrue(TEXT("HP restored"), After && Near(After->Hp, MaxHp));
	bPass &= TestTrue(TEXT("damage taken restored"), After && After->Metrics.DamageTaken <= 0.0);
	bPass &= TestTrue(TEXT("player is not down"), After && !After->bDown);
	bPass &= TestEqual(TEXT("no defeat"), CountPrefix(Session.GetChain(), TEXT("Session defeat")), 0);

	Session.SetScripted(true);
	const bool bSigil = AdvanceUntil(Session, [&Session]()
	{
		return Session.GetTeachStep() == TEXT("sigil") && Session.GetTeachClock() < 1.0;
	}, 90.0);
	bPass &= TestTrue(TEXT("reached the sigil step"), bSigil);
	Session.SetScripted(false);
	Rig.Hands->StopAll();
	const int32 MissesBefore = CountExact(Session.GetChain(), TEXT("teach miss"));
	const bool bSigilMiss = AdvanceUntil(Session, [&Session, MissesBefore]()
	{
		return CountExact(Session.GetChain(), TEXT("teach miss")) > MissesBefore;
	}, 20.0);
	const FActor* SigilAfter = PlayerOf(Session);
	bPass &= TestTrue(TEXT("an undrawn sigil repeats"), bSigilMiss);
	bPass &= TestEqual(TEXT("still the sigil step"), Session.GetTeachStep(), FString(TEXT("sigil")));
	bPass &= TestTrue(TEXT("sigil miss did not damage"), SigilAfter && Near(SigilAfter->Hp, MaxHp) && SigilAfter->Metrics.DamageTaken <= 0.0);

	Session.SetScripted(true);
	const bool bBlink = AdvanceUntil(Session, [&Session]()
	{
		return Session.GetTeachStep() == TEXT("blink") && Session.GetTeachClock() < 0.2;
	}, 40.0);
	bPass &= TestTrue(TEXT("reached the blink step"), bBlink);
	Session.SetScripted(false);
	Rig.Hands->StopAll();
	const int32 MissesAtBlink = CountExact(Session.GetChain(), TEXT("teach miss"));
	const bool bBlinkMiss = AdvanceUntil(Session, [&Session, MissesAtBlink]()
	{
		return CountExact(Session.GetChain(), TEXT("teach miss")) > MissesAtBlink;
	}, 6.0);
	const FActor* BlinkAfter = PlayerOf(Session);
	bPass &= TestTrue(TEXT("a missed lane repeats"), bBlinkMiss);
	bPass &= TestEqual(TEXT("still the blink step"), Session.GetTeachStep(), FString(TEXT("blink")));
	bPass &= TestEqual(TEXT("still teaching after the lane"), Session.GetStage(), FString(TEXT("teach")));
	bPass &= TestTrue(TEXT("lane miss did not damage"), BlinkAfter && Near(BlinkAfter->Hp, MaxHp) && BlinkAfter->Metrics.DamageTaken <= 0.0);
	bPass &= TestTrue(TEXT("lane miss did not down the player"), BlinkAfter && !BlinkAfter->bDown);
	if (!bPass)
	{
		AddError(TeachLines(Session.GetChain()));
	}
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaTeachWaiver, "MageArena.Teach.PerfectOrThree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaTeachWaiver::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FTeachRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	const FString Dir = PrivateSaveDir();
	Rig.Bind(Session);
	Session.SetSaveDirectory(Dir);
	Session.SetScripted(false);
	if (!Session.BeginArc(1))
	{
		AddError(TEXT("BeginArc failed"));
		Rig.Close(Session);
		return false;
	}
	const FTeachTuning& Tuning = Session.GetTuning();
	bool bPass = TestTrue(TEXT("teach.json loaded"), Tuning.bLoaded);
	bPass &= TestTrue(TEXT("kernel absorb window is 0.15 s"), Near(KernelData().Absorb.WindowS, 0.15));
	bPass &= TestTrue(TEXT("absorb lead is the loaded 0.55 s"), Near(Tuning.AbsorbLeadS, 0.55));
	bPass &= TestEqual(TEXT("three tries in teach.json"), Tuning.PerfectTries, 3);
	// The ward clip takes 0.20 s to rise into the pose, so this lead is not a perfect.
	bPass &= TestTrue(TEXT("loaded absorb lead is outside the kernel perfect window"),
		Tuning.AbsorbLeadS - 0.20 > KernelData().Absorb.WindowS);

	Rig.Hands->PlayQuickAction(TEXT("ward-raise"), EClipVariant::Normal);
	FString PlayedOn;
	double SeenClock = 0.0;
	double Elapsed = 0.0;
	while (Elapsed < 30.0 && CountPrefix(Session.GetChain(), TEXT("teach perfect waived")) == 0)
	{
		const double Clock = Session.GetTeachClock();
		if (Clock + 0.05 < SeenClock)
		{
			PlayedOn.Reset();
		}
		SeenClock = Clock;
		const FString Step = Session.GetTeachStep();
		if (Step != PlayedOn && (Step == TEXT("ward") || Step == TEXT("perfect")))
		{
			// Same 170/60 s flight as ClipsInOrder: gap 7.34 m, 60 windup ticks, 111 moves.
			const double Impact = 170.0 / 60.0;
			if (Session.GetTeachClock() + 1.0e-9 >= Impact - Tuning.AbsorbLeadS)
			{
				Rig.Hands->PlayQuickAction(TEXT("ward-raise"), EClipVariant::Normal);
				PlayedOn = Step;
			}
		}
		Session.Advance(kDt, true);
		Elapsed += kDt;
	}

	const FActor* Player = PlayerOf(Session);
	bPass &= TestEqual(TEXT("two early absorbs then a waiver"), CountExact(Session.GetChain(), TEXT("teach perfect retry")), 2);
	bPass &= TestEqual(TEXT("waiver on the third absorb"), CountExact(Session.GetChain(), TEXT("teach perfect waived tries=3")), 1);
	bPass &= TestEqual(TEXT("no true perfect"), CountExact(Session.GetChain(), TEXT("teach perfect")), 0);
	bPass &= TestEqual(TEXT("one opening absorb"), CountExact(Session.GetChain(), TEXT("teach absorb")), 1);
	bPass &= TestTrue(TEXT("left the perfect step"), Session.GetTeachStep() == TEXT("ward-beat") || Session.GetTeachStep() == TEXT("sigil"));
	bPass &= TestEqual(TEXT("still teaching"), Session.GetStage(), FString(TEXT("teach")));
	bPass &= TestTrue(TEXT("waiver did not damage"), Player && Player->MaxHp > 20.0 && Near(Player->Hp, Player->MaxHp) && Player->Metrics.DamageTaken <= 0.0);
	bPass &= TestTrue(TEXT("player is up"), Player && !Player->bDown);
	if (!bPass)
	{
		AddError(TeachLines(Session.GetChain()));
	}
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaTeachSkip, "MageArena.Teach.Skip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaTeachSkip::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FTeachRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	const FString Dir = PrivateSaveDir();
	Rig.Bind(Session);
	Session.SetSaveDirectory(Dir);
	bool bPass = Session.BeginArc(3);
	bPass &= TestEqual(TEXT("opens cold"), Session.GetStage(), FString(TEXT("cold")));
	Session.SkipTeach();
	bPass &= TestEqual(TEXT("skip starts the bout"), Session.GetStage(), FString(TEXT("active")));
	bPass &= TestEqual(TEXT("skip starts wave 1"), Session.GetGames().Wave, 0);
	bPass &= TestEqual(TEXT("skip does not complete the teach"), CountExact(Session.GetChain(), TEXT("teach complete")), 0);
	bPass &= TestTrue(TEXT("skip notes the bout"), CountPrefix(Session.GetChain(), TEXT("Session start")) == 1);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Rig.Close(Session);
	return bPass;
}

#endif
