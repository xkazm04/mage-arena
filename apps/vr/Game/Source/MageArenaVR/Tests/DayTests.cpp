#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// T21: the Tiro day. Literals and their sources:
// - apps/vr/data/vr/day.json: prologueMaxS 10, ritualLineS 3, ritualTimeoutS 20, introS 3, aftermathTabletS 4.
// - teach.json offerHoldS 0.40 (the ritual's both-palms hold and every stone hold).
// - pinned arena-tiers.json Tiro: wave n 1 soldiers, 2 creatures, 3 semifinal (one mage, competence 1),
//   4 final (one mage, competence 1.5).
// - combat.vr.json rivals.brennic applies to Tiro wave n 3 (semifinal) in Fire; tiroFinal { school fire, rival null }.
// - combat.json simStepHz 60, so a kernel tick is 1/60 s.

#include "Dom/JsonObject.h"
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
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Session/ArenaSession.h"

namespace
{
const double kDayTick = 1.0 / 60.0;

struct FDayRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;
	FString Dir;

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
		// Never Saved/MageArena: that is the desktop playthrough's save.
		Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("day"), FGuid::NewGuid().ToString());
		IFileManager::Get().MakeDirectory(*Dir, true);
		return true;
	}

	void Bind(FArenaSession& Session) const
	{
		Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
		Session.SetSaveDirectory(Dir);
	}

	FString SavePath() const { return FPaths::Combine(Dir, TEXT("bout.txt")); }

	FString ReadSave() const
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *SavePath());
		return Text;
	}

	void WriteSave(const TCHAR* Text) const
	{
		FFileHelper::SaveStringToFile(Text, *SavePath());
	}

	void Close(FArenaSession& Session)
	{
		Session.Unbind();
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
			Instance = nullptr;
		}
	}
};

// Steps the session until Done or LimitS of session time. Returns the seconds advanced.
double RunUntil(FArenaSession& Session, TFunctionRef<bool()> Done, double LimitS, double Dt = kDayTick)
{
	double Elapsed = 0.0;
	while (Elapsed < LimitS && !Done())
	{
		Session.Advance(Dt, true);
		Elapsed += Dt;
	}
	return Elapsed;
}

double RunWhileStage(FArenaSession& Session, const TCHAR* Stage, double LimitS, double Dt = kDayTick)
{
	return RunUntil(Session, [&Session, Stage]() { return Session.GetStage() != Stage; }, LimitS, Dt);
}

// Ends the live bout as a win: every opponent down, nothing of theirs in flight. Steps until the phase leaves active
// (a dying hound's last ember is cleared on each step, so the win lands on the next kernel tick).
void WinBout(FArenaSession& Session)
{
	for (int32 Step = 0; Step < 120 && Session.GetStage() == TEXT("active"); ++Step)
	{
		FArenaState& State = const_cast<FArenaState&>(Session.GetGames().State);
		for (FActor& Actor : State.Actors)
		{
			if (Actor.Id != Session.GetGames().PlayerId)
			{
				Actor.bDown = true;
			}
		}
		State.Projectiles.Reset();
		State.Telegraphs.Reset();
		Session.Advance(kDayTick, true);
	}
}

void LoseBout(FArenaSession& Session)
{
	FArenaState& State = const_cast<FArenaState&>(Session.GetGames().State);
	if (FActor* Player = SimFindActor(State, Session.GetGames().PlayerId))
	{
		Player->Hp = 0.0;
		Player->bDown = true;
	}
	Session.Advance(kDayTick, true);
}

const FActor* OpponentMage(const FArenaSession& Session)
{
	for (const FActor& Actor : Session.GetGames().State.Actors)
	{
		if (Actor.Id != Session.GetGames().PlayerId && !Actor.Enemy.IsSet() && !Actor.bDummy && Actor.Team != 0)
		{
			return &Actor;
		}
	}
	return nullptr;
}

int32 CountLines(const TArray<FString>& Chain, const TCHAR* Prefix)
{
	int32 Count = 0;
	for (const FString& Line : Chain)
	{
		Count += Line.StartsWith(Prefix) ? 1 : 0;
	}
	return Count;
}

// Records each stage change, so a test can compare the phase order.
struct FStageLog
{
	TArray<FString> Stages;
	void Note(const FArenaSession& Session)
	{
		const FString Stage = Session.GetStage();
		if (Stages.Num() == 0 || Stages.Last() != Stage)
		{
			Stages.Add(Stage);
		}
	}
};

void RunLogged(FArenaSession& Session, FStageLog& Log, TFunctionRef<bool()> Done, double LimitS)
{
	double Elapsed = 0.0;
	Log.Note(Session);
	while (Elapsed < LimitS && !Done())
	{
		Session.Advance(kDayTick, true);
		Elapsed += kDayTick;
		Log.Note(Session);
	}
}

// Ritual to the bout: waits for the offer line, plays the both-palms clip, then runs into the intro.
bool OfferWrists(FDayRig& Rig, FArenaSession& Session)
{
	RunUntil(Session, [&Session]() { return Session.GetPhaseClock() + 1.0e-9 >= Session.GetDayTuning().RitualLineS; }, 5.0);
	Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
	RunWhileStage(Session, TEXT("ritual"), 5.0);
	return Session.GetStage() == TEXT("intro");
}

FString VrDataPath(const TCHAR* Name)
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr"), Name));
}

// A copy of the overlay with combat.vr.json rewritten by Mutate, in a fresh directory. Empty on failure.
FString OverlayVariant(FAutomationTestBase& Test, const TFunctionRef<void(FJsonObject&)>& Mutate)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *VrDataPath(TEXT("combat.vr.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		Test.AddError(TEXT("combat.vr.json could not be read"));
		return FString();
	}
	Mutate(*Root);
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("tiro-final"), FGuid::NewGuid().ToString());
	IFileManager::Get().MakeDirectory(*Dir, true);
	FString Out;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Out));
	bool bWritten = FFileHelper::SaveStringToFile(Out, *FPaths::Combine(Dir, TEXT("combat.vr.json")));
	for (const TCHAR* Name : {TEXT("arena-layout.json"), TEXT("calibration-proposal.json")})
	{
		bWritten &= IFileManager::Get().Copy(*FPaths::Combine(Dir, Name), *VrDataPath(Name)) == COPY_OK;
	}
	if (!bWritten)
	{
		Test.AddError(TEXT("variant overlay could not be written"));
		return FString();
	}
	return Dir;
}

TSharedRef<FJsonObject> FinalBlock(const TCHAR* School, const TCHAR* Rival)
{
	TSharedRef<FJsonObject> Block = MakeShared<FJsonObject>();
	Block->SetStringField(TEXT("school"), School);
	if (Rival)
	{
		Block->SetStringField(TEXT("rival"), Rival);
	}
	else
	{
		Block->SetField(TEXT("rival"), MakeShared<FJsonValueNull>());
	}
	Block->SetStringField(TEXT("_why"), TEXT("test variant"));
	return Block;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDayOrderFirstLaunch, "MageArena.Session.DayOrderFirstLaunch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDayOrderFirstLaunch::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDayRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Rig.Bind(Session);
	// The script plays the cold raise, the prologue's centre stone, the teach's clips, the ritual's both palms and the
	// aftermath's centre stone. The bouts themselves are won by downing the opponents (their combat is not this test).
	Session.SetScripted(true);
	bool bPass = TestTrue(TEXT("day begins"), Session.BeginArc(1));
	bPass &= TestEqual(TEXT("no save: cold"), Session.GetStage(), FString(TEXT("cold")));
	FStageLog Log;
	RunLogged(Session, Log, [&Session]() { return Session.GetStage() == TEXT("intro"); }, 150.0);
	bPass &= TestEqual(TEXT("the finished teach saved bout=0"), Rig.ReadSave(), FString(TEXT("bout=0\n")));
	const FString Intro1 = Session.GetPromptText();
	for (int32 Bout = 0; Bout < 4; ++Bout)
	{
		RunLogged(Session, Log, [&Session]() { return Session.GetStage() == TEXT("active"); }, 10.0);
		bPass &= TestEqual(*FString::Printf(TEXT("bout %d is wave index %d"), Bout + 1, Bout), Session.GetGames().Wave, Bout);
		if (Bout == 2)
		{
			const FActor* Mage = OpponentMage(Session);
			const FVrRuleset* Rules = Session.GetVrRules();
			bPass &= TestTrue(TEXT("the semifinal mage is Fire"), Mage && Mage->Fire.bSchool);
			bPass &= TestTrue(TEXT("Brennic is bound in the semifinal"), Rules && Rules->BoundRival() && Rules->BoundRival()->Id == TEXT("brennic"));
		}
		WinBout(Session);
		Log.Note(Session);
		if (Bout < 3)
		{
			bPass &= TestEqual(*FString::Printf(TEXT("intermission after bout %d"), Bout + 1), Session.GetStage(), FString(TEXT("intermission")));
			Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
			Session.Advance(kDayTick, true);
			Log.Note(Session);
		}
	}
	RunLogged(Session, Log, [&Session]() { return Session.GetStage() == TEXT("closed"); }, 20.0);
	const TArray<FString> Expected = {
		TEXT("cold"), TEXT("prologue"), TEXT("teach"), TEXT("ritual"),
		TEXT("intro"), TEXT("active"), TEXT("intermission"),
		TEXT("intro"), TEXT("active"), TEXT("intermission"),
		TEXT("intro"), TEXT("active"), TEXT("intermission"),
		TEXT("intro"), TEXT("active"), TEXT("aftermath"), TEXT("closed")};
	bPass &= TestEqual(TEXT("first-launch phase order"), FString::Join(Log.Stages, TEXT(" > ")), FString::Join(Expected, TEXT(" > ")));
	bPass &= TestEqual(TEXT("bout 1 intro line"), Intro1, FString(TEXT("Legion soldiers. Hold the dais.")));
	bPass &= TestTrue(TEXT("the final was won"), Session.WasFinalWon());
	bPass &= TestEqual(TEXT("the day over saves a new day from Bout 1"), Rig.ReadSave(), FString(TEXT("bout=0\n")));
	bPass &= TestEqual(TEXT("closing line"), Session.GetPromptText(), FString(TEXT("Rest, Cassia. The Games resume tomorrow.")));
	bPass &= TestEqual(TEXT("the prologue was skipped on the stone"), CountLines(Session.GetChain(), TEXT("day prologue skipped")), 1);
	bPass &= TestEqual(TEXT("the ritual was offered, not timed out"), CountLines(Session.GetChain(), TEXT("day ritual offered")), 1);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDayOrderResume, "MageArena.Session.DayOrderResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDayOrderResume::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDayRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	// A save made by the T13-T20 builds: one bout line, no pick, no flags file.
	Rig.WriteSave(TEXT("bout=2\n"));
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetScripted(false);
	bool bPass = TestTrue(TEXT("day begins"), Session.BeginArc(1));
	FStageLog Log;
	Log.Note(Session);
	bPass &= TestEqual(TEXT("saved bout 3"), Session.GetBoutIndex(), 2);
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(kDayTick, true);
	Log.Note(Session);
	bPass &= TestEqual(TEXT("Septima speaks first"), Session.GetPromptText(), FString(TEXT("Septima: Kneel, Tide. The collar takes its due.")));
	bPass &= TestTrue(TEXT("both palms lock the collar"), OfferWrists(Rig, Session));
	Log.Note(Session);
	bPass &= TestEqual(TEXT("bout 3 intro line"), Session.GetPromptText(), FString(TEXT("Brennic of the Ember. The ford remembers you both.")));
	RunLogged(Session, Log, [&Session]() { return Session.GetStage() == TEXT("active"); }, 10.0);
	bPass &= TestEqual(TEXT("the kernel does not step during the intro"), Session.GetGames().State.Tick, 0);
	const TArray<FString> Expected = {TEXT("offer"), TEXT("ritual"), TEXT("intro"), TEXT("active")};
	bPass &= TestEqual(TEXT("resume phase order (no prologue, no teach)"), FString::Join(Log.Stages, TEXT(" > ")), FString::Join(Expected, TEXT(" > ")));
	bPass &= TestEqual(TEXT("semifinal wave"), Session.GetGames().Wave, 2);
	const FActor* Mage = OpponentMage(Session);
	const FVrRuleset* Rules = Session.GetVrRules();
	bPass &= TestTrue(TEXT("Fire mage"), Mage && Mage->Fire.bSchool);
	bPass &= TestTrue(TEXT("competence 1"), Mage && Mage->MageAI.IsSet() && FMath::IsNearlyEqual(Mage->MageAI->Competence, 1.0));
	bPass &= TestTrue(TEXT("Brennic bound"), Rules && Rules->BoundRival() && Rules->BoundRival()->Id == TEXT("brennic"));
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDayPrologue, "MageArena.Session.DayPrologue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDayPrologue::RunTest(const FString& Parameters)
{
	(void)Parameters;
	bool bPass = true;
	{
		FDayRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		FArenaSession Session;
		Rig.Bind(Session);
		Session.SetScripted(false);
		bPass &= TestTrue(TEXT("day begins"), Session.BeginArc(1));
		Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
		Session.Advance(kDayTick, true);
		bPass &= TestEqual(TEXT("cold raise opens the prologue"), Session.GetStage(), FString(TEXT("prologue")));
		bPass &= TestEqual(TEXT("tableau line"), Session.GetPromptText(), FString(TEXT("The ford. Four mages betrayed, four collars closed.")));
		bPass &= TestTrue(TEXT("the centre stone offers the skip"), Session.AreStoryStonesShown() && Session.StoryStoneKey(1) == TEXT("stone.skip"));
		// A raised palm is not the skip.
		Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
		const double Lasted = RunWhileStage(Session, TEXT("prologue"), 15.0);
		// day.json prologueMaxS 10: the teach opens on the tick the clock reaches 10 s (600 ticks of 1/60 s).
		bPass &= TestTrue(*FString::Printf(TEXT("the prologue times out at 10 s (%.4f)"), Lasted), FMath::Abs(Lasted - 10.0) <= kDayTick + 1.0e-6);
		bPass &= TestEqual(TEXT("then the teach"), Session.GetStage(), FString(TEXT("teach")));
		bPass &= TestEqual(TEXT("ended, not skipped"), CountLines(Session.GetChain(), TEXT("day prologue ended")), 1);
		Rig.Close(Session);
	}
	{
		FDayRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		FArenaSession Session;
		Rig.Bind(Session);
		Session.SetScripted(false);
		bPass &= TestTrue(TEXT("day begins"), Session.BeginArc(1));
		Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
		Session.Advance(kDayTick, true);
		Rig.Hands->PlayQuickAction(TEXT("stone-centre"), EClipVariant::Normal);
		double Held = 0.0;
		const double Lasted = RunUntil(Session, [&]()
		{
			Held = FMath::Max(Held, Session.GetStoryHoldFraction());
			return Session.GetStage() != TEXT("prologue");
		}, 9.0);
		bPass &= TestEqual(TEXT("the stone skips to the teach"), Session.GetStage(), FString(TEXT("teach")));
		bPass &= TestTrue(*FString::Printf(TEXT("skipped before the timeout (%.3f s)"), Lasted), Lasted < 9.0);
		bPass &= TestEqual(TEXT("skipped"), CountLines(Session.GetChain(), TEXT("day prologue skipped")), 1);
		Rig.Close(Session);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDayRitualHold, "MageArena.Session.DayRitualHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDayRitualHold::RunTest(const FString& Parameters)
{
	(void)Parameters;
	bool bPass = true;
	const double Dt = 1.0 / 72.0;
	{
		FDayRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		Rig.WriteSave(TEXT("bout=0\n"));
		FArenaSession Session;
		Rig.Bind(Session);
		Session.SetScripted(false);
		bPass &= TestTrue(TEXT("day begins"), Session.BeginArc(1));
		Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
		Session.Advance(Dt, true);
		bPass &= TestEqual(TEXT("ritual"), Session.GetStage(), FString(TEXT("ritual")));
		bPass &= TestTrue(TEXT("offerHoldS 0.40"), FMath::IsNearlyEqual(Session.GetTuning().OfferHoldS, 0.40));
		bPass &= TestTrue(TEXT("ritualLineS 3"), FMath::IsNearlyEqual(Session.GetDayTuning().RitualLineS, 3.0));
		// Both palms from the first instant. The hold may only count once the offer line stands (3 s), so the ritual
		// ends on the tick the hold reaches 0.40 s after it: at 72 Hz the line stands from tick 216 (3.000 s), and the
		// hold of ceil(0.40 * 72) = 29 ticks completes on tick 216 + 28 = 244, 244/72 = 3.389 s of ritual clock.
		Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
		double Clock = 0.0;
		const double Lasted = RunUntil(Session, [&]()
		{
			if (Session.GetStage() == TEXT("ritual"))
			{
				Clock = Session.GetPhaseClock();
			}
			return Session.GetStage() != TEXT("ritual");
		}, 6.0, Dt);
		bPass &= TestEqual(TEXT("both palms lock the collar"), CountLines(Session.GetChain(), TEXT("day ritual offered")), 1);
		bPass &= TestTrue(*FString::Printf(TEXT("not before the offer line plus the hold (ritual clock %.4f)"), Clock), Clock >= 3.0 + 0.40 - 2.0 * Dt);
		bPass &= TestTrue(*FString::Printf(TEXT("on the hold (ritual clock %.4f, expected 244/72)"), Clock), FMath::Abs(Clock - 244.0 / 72.0) <= Dt + 1.0e-6);
		bPass &= TestEqual(TEXT("into Bout 1's intro"), Session.GetStage(), FString(TEXT("intro")));
		(void)Lasted;
		Rig.Close(Session);
	}
	{
		// One palm is a ward raise, not the offer: the ritual waits for its timeout.
		FDayRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		Rig.WriteSave(TEXT("bout=0\n"));
		FArenaSession Session;
		Rig.Bind(Session);
		Session.SetScripted(false);
		bPass &= TestTrue(TEXT("day begins"), Session.BeginArc(1));
		Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
		Session.Advance(Dt, true);
		RunUntil(Session, [&Session]() { return Session.GetPhaseClock() >= 3.1; }, 5.0, Dt);
		bPass &= TestEqual(TEXT("offer line"), Session.GetPromptText(), FString(TEXT("Septima: Offer both wrists to the collar.")));
		Rig.Hands->PlayQuickAction(TEXT("ward-raise"), EClipVariant::Normal);
		RunUntil(Session, [&Session]() { return Session.GetPhaseClock() >= 6.0; }, 4.0, Dt);
		bPass &= TestEqual(TEXT("one palm does not lock the collar"), Session.GetStage(), FString(TEXT("ritual")));
		bPass &= TestFalse(TEXT("one palm sets no absorb in the ritual"), Session.GetGames().State.Actors.Num() > 0
			&& SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId)->bAbsorb);
		Rig.Close(Session);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDayRitualTimeout, "MageArena.Session.DayRitualTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDayRitualTimeout::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDayRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	Rig.WriteSave(TEXT("bout=1\n"));
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetScripted(false);
	bool bPass = TestTrue(TEXT("day begins"), Session.BeginArc(1));
	Session.SkipTeach();
	bPass &= TestEqual(TEXT("ritual"), Session.GetStage(), FString(TEXT("ritual")));
	// day.json ritualTimeoutS 20, counted from the ritual's start: 1200 ticks of 1/60 s, then Bout 2's intro.
	const double Lasted = RunWhileStage(Session, TEXT("ritual"), 25.0);
	bPass &= TestTrue(*FString::Printf(TEXT("soft timeout at 20 s (%.4f)"), Lasted), FMath::Abs(Lasted - 20.0) <= kDayTick + 1.0e-6);
	bPass &= TestEqual(TEXT("timeout continues anyway"), Session.GetStage(), FString(TEXT("intro")));
	bPass &= TestEqual(TEXT("timeout line"), CountLines(Session.GetChain(), TEXT("day ritual timeout")), 1);
	bPass &= TestEqual(TEXT("no offer"), CountLines(Session.GetChain(), TEXT("day ritual offered")), 0);
	bPass &= TestEqual(TEXT("Bout 2 is next"), Session.GetGames().Wave, 1);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDayFinalOpponent, "MageArena.Session.DayFinalOpponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDayFinalOpponent::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FVrRuleset Live;
	FString Error;
	if (!TestTrue(TEXT("overlay loads"), LoadVrRuleset(Live, Error, false)))
	{
		AddError(Error);
		return false;
	}
	bool bPass = TestEqual(TEXT("tiroFinal.school"), Live.TiroFinal.School, FString(TEXT("fire")));
	bPass &= TestTrue(TEXT("tiroFinal.rival is null"), Live.TiroFinal.Rival.IsEmpty());
	bPass &= TestEqual(TEXT("no rival binds the Fire final"), Live.FindRival(TEXT("tiro"), 4, TEXT("final"), TEXT("fire")), static_cast<int32>(INDEX_NONE));

	struct FCase
	{
		const TCHAR* Name;
		bool bLoads;
		TFunction<void(FJsonObject&)> Mutate;
	};
	auto MoveBrennicToFinal = [](FJsonObject& Root)
	{
		const TSharedPtr<FJsonObject>* Rivals = nullptr;
		const TSharedPtr<FJsonObject>* Brennic = nullptr;
		const TSharedPtr<FJsonObject>* Applies = nullptr;
		if (Root.TryGetObjectField(TEXT("rivals"), Rivals) && Rivals && (*Rivals)->TryGetObjectField(TEXT("brennic"), Brennic) && Brennic
			&& (*Brennic)->TryGetObjectField(TEXT("appliesTo"), Applies) && Applies)
		{
			(*Applies)->SetNumberField(TEXT("waveN"), 4);
			(*Applies)->SetStringField(TEXT("kind"), TEXT("final"));
		}
	};
	TArray<FCase> Cases;
	// No Air school in this kernel yet (T23 is not merged on this branch): both Air values fail the load.
	Cases.Add({TEXT("air without a rival"), false, [](FJsonObject& Root) { Root.SetObjectField(TEXT("tiroFinal"), FinalBlock(TEXT("air"), nullptr)); }});
	Cases.Add({TEXT("air with lio (T23's value)"), false, [](FJsonObject& Root) { Root.SetObjectField(TEXT("tiroFinal"), FinalBlock(TEXT("air"), TEXT("lio"))); }});
	Cases.Add({TEXT("brennic is the semifinal"), false, [](FJsonObject& Root) { Root.SetObjectField(TEXT("tiroFinal"), FinalBlock(TEXT("fire"), TEXT("brennic"))); }});
	Cases.Add({TEXT("missing block"), false, [](FJsonObject& Root) { Root.RemoveField(TEXT("tiroFinal")); }});
	Cases.Add({TEXT("unknown key"), false, [](FJsonObject& Root)
	{
		TSharedRef<FJsonObject> Block = FinalBlock(TEXT("fire"), nullptr);
		Block->SetNumberField(TEXT("competence"), 2.0);
		Root.SetObjectField(TEXT("tiroFinal"), Block);
	}});
	Cases.Add({TEXT("missing rival"), false, [](FJsonObject& Root)
	{
		TSharedRef<FJsonObject> Block = FinalBlock(TEXT("fire"), nullptr);
		Block->RemoveField(TEXT("rival"));
		Root.SetObjectField(TEXT("tiroFinal"), Block);
	}});
	Cases.Add({TEXT("empty rival"), false, [](FJsonObject& Root) { Root.SetObjectField(TEXT("tiroFinal"), FinalBlock(TEXT("fire"), TEXT(""))); }});
	Cases.Add({TEXT("school not a string"), false, [](FJsonObject& Root)
	{
		TSharedRef<FJsonObject> Block = FinalBlock(TEXT("fire"), nullptr);
		Block->SetNumberField(TEXT("school"), 1.0);
		Root.SetObjectField(TEXT("tiroFinal"), Block);
	}});
	Cases.Add({TEXT("null rival while brennic applies to the final"), false, [MoveBrennicToFinal](FJsonObject& Root) { MoveBrennicToFinal(Root); }});
	// The seam T23 uses: a named rival that applies to the final in the same school loads.
	Cases.Add({TEXT("a named rival for the final"), true, [MoveBrennicToFinal](FJsonObject& Root)
	{
		MoveBrennicToFinal(Root);
		Root.SetObjectField(TEXT("tiroFinal"), FinalBlock(TEXT("fire"), TEXT("brennic")));
	}});
	Cases.Add({TEXT("a water final"), true, [](FJsonObject& Root) { Root.SetObjectField(TEXT("tiroFinal"), FinalBlock(TEXT("water"), nullptr)); }});
	for (const FCase& Case : Cases)
	{
		const FString Dir = OverlayVariant(*this, Case.Mutate);
		if (Dir.IsEmpty())
		{
			bPass = false;
			continue;
		}
		FVrRuleset Variant;
		FString VariantError;
		SetVrDataDirForTest(Dir);
		const bool bLoaded = LoadVrRuleset(Variant, VariantError, false);
		SetVrDataDirForTest(FString());
		bPass &= TestEqual(*FString::Printf(TEXT("%s loads=%d (%s)"), Case.Name, Case.bLoads ? 1 : 0, *VariantError), bLoaded, Case.bLoads);
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
	}

	// The day's Bout 4: Tiro wave index 3 (n 4, final), competence 1.5, a Fire mage, no rival, the Corvo line.
	FDayRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	Rig.WriteSave(TEXT("bout=3\n"));
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetScripted(false);
	bPass &= TestTrue(TEXT("day begins"), Session.BeginArc(1));
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(kDayTick, true);
	bPass &= TestTrue(TEXT("ritual to the final"), OfferWrists(Rig, Session));
	bPass &= TestEqual(TEXT("final intro line"), Session.GetPromptText(), FString(TEXT("Corvo of the Ember. The final.")));
	bPass &= TestEqual(TEXT("final wave index"), Session.GetGames().Wave, 3);
	const FActor* Mage = OpponentMage(Session);
	bPass &= TestTrue(TEXT("one opponent mage"), Mage != nullptr);
	bPass &= TestTrue(TEXT("Fire mage"), Mage && Mage->Fire.bSchool);
	bPass &= TestTrue(TEXT("competence 1.5"), Mage && Mage->MageAI.IsSet() && FMath::IsNearlyEqual(Mage->MageAI->Competence, 1.5));
	bPass &= TestTrue(TEXT("no rival (no phases)"), Session.GetVrRules() && Session.GetVrRules()->BoundRival() == nullptr);
	bPass &= TestEqual(TEXT("the day's school for the final"), Session.DaySchool(3), FString(TEXT("fire")));
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDaySaveEachBout, "MageArena.Session.DaySaveEachBout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDaySaveEachBout::RunTest(const FString& Parameters)
{
	(void)Parameters;
	bool bPass = true;
	for (int32 Bout = 0; Bout < 4; ++Bout)
	{
		FDayRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		// Bout 2 carries the T20 pick line; the others are the old one-line format.
		const FString Pick = Bout == 1 ? TEXT("tideOrbIV=A\n") : TEXT("");
		const FString Saved = FString::Printf(TEXT("bout=%d\n"), Bout) + Pick;
		Rig.WriteSave(*Saved);
		FArenaSession Session;
		Rig.Bind(Session);
		Session.SetScripted(false);
		const FString Label = FString::Printf(TEXT("bout %d"), Bout + 1);
		bPass &= TestTrue(*(Label + TEXT(" day begins")), Session.BeginArc(1));
		bPass &= TestEqual(*(Label + TEXT(" offer")), Session.GetStage(), FString(TEXT("offer")));
		bPass &= TestEqual(*(Label + TEXT(" offered bout")), Session.GetBoutIndex(), Bout);
		Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
		Session.Advance(kDayTick, true);
		Session.NotifyQuit();
		bPass &= TestEqual(*(Label + TEXT(" quit in the ritual keeps the bout")), Rig.ReadSave(), Saved);
		bPass &= TestTrue(*(Label + TEXT(" ritual to the bout")), OfferWrists(Rig, Session));
		bPass &= TestEqual(*(Label + TEXT(" wave")), Session.GetGames().Wave, Bout);
		Session.NotifyQuit();
		bPass &= TestEqual(*(Label + TEXT(" quit in the intro keeps the bout")), Rig.ReadSave(), Saved);
		RunWhileStage(Session, TEXT("intro"), 5.0);
		bPass &= TestEqual(*(Label + TEXT(" fight")), Session.GetStage(), FString(TEXT("active")));
		Session.NotifyQuit();
		bPass &= TestEqual(*(Label + TEXT(" quit mid-bout keeps the bout")), Rig.ReadSave(), Saved);
		WinBout(Session);
		if (Bout < 3)
		{
			bPass &= TestEqual(*(Label + TEXT(" intermission")), Session.GetStage(), FString(TEXT("intermission")));
			Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
			Session.Advance(kDayTick, true);
			const FString Next = FString::Printf(TEXT("bout=%d\n"), Bout + 1) + Pick;
			bPass &= TestEqual(*(Label + TEXT(" the next bout is saved")), Rig.ReadSave(), Next);
			bPass &= TestEqual(*(Label + TEXT(" next intro")), Session.GetStage(), FString(TEXT("intro")));
		}
		else
		{
			bPass &= TestEqual(*(Label + TEXT(" aftermath")), Session.GetStage(), FString(TEXT("aftermath")));
			bPass &= TestEqual(*(Label + TEXT(" a new day is saved")), Rig.ReadSave(), FString(TEXT("bout=0\n")));
		}
		Rig.Close(Session);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDayFinalDefeat, "MageArena.Session.DayFinalDefeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDayFinalDefeat::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDayRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	Rig.WriteSave(TEXT("bout=3\n"));
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetScripted(false);
	bool bPass = TestTrue(TEXT("day begins"), Session.BeginArc(1));
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(kDayTick, true);
	bPass &= TestTrue(TEXT("ritual to the final"), OfferWrists(Rig, Session));
	RunWhileStage(Session, TEXT("intro"), 5.0);
	LoseBout(Session);
	bPass &= TestEqual(TEXT("defeat in the final"), Session.GetStage(), FString(TEXT("lost")));
	bPass &= TestEqual(TEXT("the final's defeat prompt"), Session.GetPromptText(), FString(TEXT("Palm: fight again. Centre stone: end the day.")));
	bPass &= TestTrue(TEXT("the centre stone ends the day"), Session.AreStoryStonesShown() && Session.StoryStoneKey(1) == TEXT("stone.end"));
	bPass &= TestEqual(TEXT("the save still offers the final"), Rig.ReadSave(), FString(TEXT("bout=3\n")));
	// The palm offers Bout 4 again, with its intro.
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(kDayTick, true);
	bPass &= TestEqual(TEXT("the palm offers Bout 4 again"), Session.GetStage(), FString(TEXT("intro")));
	bPass &= TestEqual(TEXT("the same bout"), Session.GetGames().Wave, 3);
	bPass &= TestEqual(TEXT("the intro again"), Session.GetPromptText(), FString(TEXT("Corvo of the Ember. The final.")));
	RunWhileStage(Session, TEXT("intro"), 5.0);
	LoseBout(Session);
	bPass &= TestEqual(TEXT("lost again"), Session.GetStage(), FString(TEXT("lost")));
	double Attempts = 0.0;
	bool bWon = true;
	bPass &= TestTrue(TEXT("w4 attempts"), Session.GetFlags().GetNumber(TEXT("arena.tiro.1.w4.attempts"), Attempts) && Attempts == 2.0);
	bPass &= TestTrue(TEXT("w4 not won"), Session.GetFlags().GetBool(TEXT("arena.tiro.1.w4.won"), bWon) && !bWon);
	Rig.Hands->PlayQuickAction(TEXT("stone-centre"), EClipVariant::Normal);
	RunWhileStage(Session, TEXT("lost"), 5.0);
	bPass &= TestEqual(TEXT("the stone ends the day"), Session.GetStage(), FString(TEXT("aftermath")));
	bPass &= TestFalse(TEXT("the final is recorded lost"), Session.WasFinalWon());
	bPass &= TestEqual(TEXT("tablet line first"), Session.GetPromptText(), FString(TEXT("The day is cut into stone.")));
	bPass &= TestTrue(TEXT("the tablet lists the final"), Session.GetTabletText().Contains(TEXT("4  final  lost")));
	RunUntil(Session, [&Session]() { return Session.GetPhaseClock() >= 4.0 + kDayTick; }, 6.0);
	bPass &= TestEqual(TEXT("closing line after a lost final"), Session.GetPromptText(), FString(TEXT("The sand drinks your blood. The collar holds.")));
	bPass &= TestEqual(TEXT("a new day is saved"), Rig.ReadSave(), FString(TEXT("bout=0\n")));
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDayFlags, "MageArena.Session.DayFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDayFlags::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDayRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	Rig.WriteSave(TEXT("bout=0\n"));
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetScripted(false);
	bool bPass = TestTrue(TEXT("day begins"), Session.BeginArc(1));
	Session.SkipTeach();
	RunWhileStage(Session, TEXT("ritual"), 25.0);
	RunWhileStage(Session, TEXT("intro"), 5.0);
	bPass &= TestEqual(TEXT("Bout 1 live"), Session.GetStage(), FString(TEXT("active")));
	bPass &= TestEqual(TEXT("Bout 1 starts at tick 0"), Session.GetGames().State.Tick, 0);
	// Bout 1: 120 kernel ticks of an idle seat, then the opponents go down and the win lands on tick 121.
	// time = 121 / 60 = 2.016667 s. Two perfects are added to the kernel's count (the flag is the bout's delta).
	for (int32 Tick = 0; Tick < 120; ++Tick)
	{
		Session.Advance(kDayTick, true);
	}
	bPass &= TestEqual(TEXT("120 ticks stepped"), Session.GetGames().State.Tick, 120);
	FArenaState& State = const_cast<FArenaState&>(Session.GetGames().State);
	if (FActor* Player = SimFindActor(State, Session.GetGames().PlayerId))
	{
		Player->Metrics.Perfects += 2;
	}
	WinBout(Session);
	bPass &= TestEqual(TEXT("won on tick 121"), Session.GetGames().State.Tick, 121);
	auto Number = [&Session](const TCHAR* Key)
	{
		double Value = -1.0;
		Session.GetFlags().GetNumber(Key, Value);
		return Value;
	};
	auto Bool = [&Session](const TCHAR* Key, bool& bOut)
	{
		return Session.GetFlags().GetBool(Key, bOut);
	};
	bool bWon = false;
	bPass &= TestTrue(TEXT("w1.won"), Bool(TEXT("arena.tiro.1.w1.won"), bWon) && bWon);
	bPass &= TestEqual(TEXT("w1.attempts"), Number(TEXT("arena.tiro.1.w1.attempts")), 1.0);
	bPass &= TestEqual(TEXT("w1.perfects"), Number(TEXT("arena.tiro.1.w1.perfects")), 2.0);
	bPass &= TestTrue(*FString::Printf(TEXT("w1.time %.6f is 121/60"), Number(TEXT("arena.tiro.1.w1.time"))),
		FMath::IsNearlyEqual(Number(TEXT("arena.tiro.1.w1.time")), 121.0 / 60.0, 1.0e-9));

	// Bout 2: lost after 30 ticks (the defeat lands on the next tick: 31/60), then won on the retry. Downing the hounds
	// queues their pinned ember_burst on tick 1 (a hazard, so the bout goes on); WinBout clears it and the win lands on
	// tick 2 (2/60).
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(kDayTick, true);
	RunWhileStage(Session, TEXT("intro"), 5.0);
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		Session.Advance(kDayTick, true);
	}
	LoseBout(Session);
	bPass &= TestEqual(TEXT("Bout 2 lost"), Session.GetStage(), FString(TEXT("lost")));
	bWon = true;
	bPass &= TestTrue(TEXT("w2.won false"), Bool(TEXT("arena.tiro.1.w2.won"), bWon) && !bWon);
	bPass &= TestEqual(TEXT("w2.attempts after the defeat"), Number(TEXT("arena.tiro.1.w2.attempts")), 1.0);
	bPass &= TestTrue(TEXT("w2.time of the defeat is 31/60"), FMath::IsNearlyEqual(Number(TEXT("arena.tiro.1.w2.time")), 31.0 / 60.0, 1.0e-9));
	bPass &= TestEqual(TEXT("w2.perfects of the defeat"), Number(TEXT("arena.tiro.1.w2.perfects")), 0.0);
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(kDayTick, true);
	RunWhileStage(Session, TEXT("intro"), 5.0);
	WinBout(Session);
	bPass &= TestTrue(TEXT("w2.won after the retry"), Bool(TEXT("arena.tiro.1.w2.won"), bWon) && bWon);
	bPass &= TestEqual(TEXT("w2.attempts"), Number(TEXT("arena.tiro.1.w2.attempts")), 2.0);
	bPass &= TestTrue(*FString::Printf(TEXT("w2.time of the winning attempt %.6f is 2/60"), Number(TEXT("arena.tiro.1.w2.time"))),
		FMath::IsNearlyEqual(Number(TEXT("arena.tiro.1.w2.time")), 2.0 / 60.0, 1.0e-9));
	bPass &= TestFalse(TEXT("no w3 yet"), Session.GetFlags().Has(TEXT("arena.tiro.1.w3.won")));

	// The file next to bout.txt holds the same map, and a new session reads it back.
	FArenaFlags Read;
	bPass &= TestTrue(TEXT("flags file loads"), Read.Load(Session.FlagsFilePath()));
	bPass &= TestEqual(TEXT("flags file path"), Session.FlagsFilePath(), FPaths::Combine(Rig.Dir, TEXT("arena-flags.json")));
	// T22 adds .cracks and .tithe per bout (both 0 here: an idle seat).
	bPass &= TestEqual(TEXT("twelve flags (two bouts, six fields)"), Read.Num(), 12);
	double FileTime = 0.0;
	bPass &= TestTrue(TEXT("file w1.time"), Read.GetNumber(TEXT("arena.tiro.1.w1.time"), FileTime) && FMath::IsNearlyEqual(FileTime, 121.0 / 60.0, 1.0e-9));
	Session.Unbind();
	FArenaSession Again;
	Rig.Bind(Again);
	bPass &= TestTrue(TEXT("relaunch"), Again.BeginArc(1));
	double Attempts = 0.0;
	bPass &= TestTrue(TEXT("relaunch reads the flags"), Again.GetFlags().GetNumber(TEXT("arena.tiro.1.w2.attempts"), Attempts) && Attempts == 2.0);
	Rig.Close(Again);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDayStrings, "MageArena.Session.DayStrings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDayStrings::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!TestTrue(TEXT("strings.json reads"), FFileHelper::LoadFileToString(Text, *VrDataPath(TEXT("strings.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid()))
	{
		return false;
	}
	bool bPass = true;
	int32 Story = 0;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		FString Value;
		if (Pair.Key.StartsWith(TEXT("story.")))
		{
			++Story;
			bPass &= TestTrue(*FString::Printf(TEXT("%s is a string"), *Pair.Key), Pair.Value->TryGetString(Value));
			TArray<FString> Words;
			Value.ParseIntoArrayWS(Words);
			bPass &= TestTrue(*FString::Printf(TEXT("%s has 10 words or fewer (%d)"), *Pair.Key, Words.Num()), Words.Num() > 0 && Words.Num() <= 10);
			const FString Marker = TEXT("_story.") + Pair.Key.Mid(6);
			FString MarkerText;
			bPass &= TestTrue(*FString::Printf(TEXT("%s has its C2 marker"), *Pair.Key), Root->TryGetStringField(Marker, MarkerText)
				&& MarkerText == TEXT("placeholder for C2"));
		}
		else if (Pair.Key.StartsWith(TEXT("_story.")))
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s marks a line"), *Pair.Key), Root->HasField(TEXT("story.") + Pair.Key.Mid(7)));
		}
	}
	// Every line and label the day shows.
	for (const TCHAR* Key : {TEXT("story.prologue.tableau"), TEXT("story.ritual.septima"), TEXT("story.ritual.offer"),
		TEXT("story.bout1.intro"), TEXT("story.bout2.intro"), TEXT("story.bout3.intro"), TEXT("story.bout4.intro.corvo"),
		TEXT("story.aftermath.tablet"), TEXT("story.aftermath.close"), TEXT("story.aftermath.close.lost"), TEXT("story.closed"),
		TEXT("stone.skip"), TEXT("stone.end"), TEXT("lost.final"), TEXT("tablet.title")})
	{
		bPass &= TestTrue(*FString::Printf(TEXT("%s exists"), Key), Root->HasField(Key));
	}
	bPass &= TestTrue(*FString::Printf(TEXT("story lines found (%d)"), Story), Story >= 11);
	return bPass;
}

#endif
