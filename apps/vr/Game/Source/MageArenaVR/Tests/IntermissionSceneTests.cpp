#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// T28: the intermission scenes on the stones (docs/campaign/05-c2-slice-story-bible-and-script.md sections 4, 6 and 8).
// Literals and their sources:
// - apps/vr/data/vr/story/slice.json (C2): three scenes, after Tiro waves n 1, 2 and 3, each floorS 3 and maxS 20;
//   int-1 keeps the T20 pick (keepsExistingPick); int-2 and int-3 offer stones of their own. Variant 2A needs
//   arena.tiro.1.w2.perfects >= 2; 3A needs w3.won and w3.attempts <= 1, 3B w3.won; the last variant of each is the
//   default. The beats' floors: prologue 6, closing 6. Every line and label is read from the file, never retyped here.
// - apps/vr/data/vr/day.json: introS [8, 8, 12, 12], aftermathTabletS 16, closingCardS 34; before T28 introS 3 and
//   aftermathTabletS 4 (T21).
// - teach.json offerHoldS 0.40 (every stone hold). combat.json simStepHz 60: a kernel tick is 1/60 s.
// - Tiro waves n 3 and 4 bind a rival (Brennic, Lio): their win waits on missio (no choice spares at 5 s).

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
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Session/ArenaFlags.h"
#include "Session/ArenaSession.h"
#include "Session/StorySlice.h"

namespace
{
const double kSceneTick = 1.0 / 60.0;

struct FSceneRig
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
		Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("intermission"), FGuid::NewGuid().ToString());
		IFileManager::Get().MakeDirectory(*Dir, true);
		return true;
	}

	void Bind(FArenaSession& Session) const
	{
		Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
		Session.SetSaveDirectory(Dir);
	}

	void WriteSave(const TCHAR* Text) const
	{
		FFileHelper::SaveStringToFile(Text, *FPaths::Combine(Dir, TEXT("bout.txt")));
	}

	void RaisePalm() const
	{
		Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
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
double SceneRunUntil(FArenaSession& Session, TFunctionRef<bool()> Done, double LimitS)
{
	double Elapsed = 0.0;
	while (Elapsed < LimitS && !Done())
	{
		Session.Advance(kSceneTick, true);
		Elapsed += kSceneTick;
	}
	return Elapsed;
}

double SceneRunWhile(FArenaSession& Session, const TCHAR* Stage, double LimitS)
{
	return SceneRunUntil(Session, [&Session, Stage]()
	{ return Session.GetStage() != Stage; }, LimitS);
}

// Ends the live bout as a win: every opponent down, nothing of theirs in flight (MageArena.Session.DayFlags' WinBout).
void SceneWinBout(FArenaSession& Session)
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
		Session.Advance(kSceneTick, true);
	}
}

// A day from a save naming Bout (0-based): offer, palm, ritual, both palms, the intro, into the live bout.
bool SceneReachBout(FSceneRig& Rig, FArenaSession& Session, int32 Bout)
{
	Rig.WriteSave(*FString::Printf(TEXT("bout=%d\n"), Bout));
	Rig.Bind(Session);
	Session.SetScripted(false);
	if (!Session.BeginArc(1))
	{
		return false;
	}
	Rig.RaisePalm();
	Session.Advance(kSceneTick, true);
	SceneRunUntil(Session, [&Session]()
	{ return Session.GetPhaseClock() + 1.0e-9 >= Session.GetRitualOfferS(); }, 15.0);
	Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
	SceneRunWhile(Session, TEXT("ritual"), 5.0);
	SceneRunWhile(Session, TEXT("intro"), 15.0);
	return Session.GetStage() == TEXT("active") && Session.GetGames().Wave == Bout;
}

// Wins the live bout and, for a rival's bout, lets missio spare at its timeout. Ends in the intermission or aftermath.
void SceneWinIntoNext(FArenaSession& Session)
{
	// Bout 2's creatures arrive over time: the bout is won once nothing is left to spawn (MageArena.Preset.NoPickKeepsB).
	for (int32 Try = 0; Try < 20 && Session.GetStage() == TEXT("active"); ++Try)
	{
		SceneWinBout(Session);
	}
	if (Session.GetStage() == TEXT("missio"))
	{
		SceneRunWhile(Session, TEXT("missio"), 6.0);
	}
}

int32 SceneCountLines(const TArray<FString>& Chain, const TCHAR* Prefix)
{
	int32 Count = 0;
	for (const FString& Line : Chain)
	{
		Count += Line.StartsWith(Prefix) ? 1 : 0;
	}
	return Count;
}

int32 CountIntermissionFlags(const FArenaSession& Session)
{
	int32 Count = 0;
	for (const FString& Key : Session.GetFlags().Keys())
	{
		Count += Key.StartsWith(TEXT("arena.intermission.")) ? 1 : 0;
	}
	return Count;
}

const FStoryScene* SceneAfter(const FStorySlice& Slice, int32 WaveN)
{
	return Slice.FindSceneAfterWave(WaveN);
}

// The committed slice.json rewritten by Mutate, in a fresh directory. Empty on failure.
FString SliceVariant(FAutomationTestBase& Test, const TFunctionRef<void(FJsonObject&)>& Mutate)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *FStorySlice::DefaultFilePath()) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		Test.AddError(TEXT("slice.json could not be read"));
		return FString();
	}
	Mutate(*Root);
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("slice"), FGuid::NewGuid().ToString());
	IFileManager::Get().MakeDirectory(*Dir, true);
	FString Out;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Out));
	const FString Path = FPaths::Combine(Dir, TEXT("slice.json"));
	if (!FFileHelper::SaveStringToFile(Out, *Path))
	{
		Test.AddError(TEXT("slice variant could not be written"));
		return FString();
	}
	return Path;
}

TSharedPtr<FJsonObject> SceneObject(FJsonObject& Root, int32 Index)
{
	const TArray<TSharedPtr<FJsonValue>>* Scenes = nullptr;
	if (!Root.TryGetArrayField(TEXT("scenes"), Scenes) || !Scenes || !Scenes->IsValidIndex(Index) || !(*Scenes)[Index].IsValid())
	{
		return nullptr;
	}
	return (*Scenes)[Index]->AsObject();
}

TSharedPtr<FJsonObject> StoneObject(FJsonObject& Root, int32 SceneIndex, int32 Stone)
{
	const TSharedPtr<FJsonObject> Scene = SceneObject(Root, SceneIndex);
	const TSharedPtr<FJsonObject>* Choice = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Stones = nullptr;
	if (!Scene.IsValid() || !Scene->TryGetObjectField(TEXT("choice"), Choice) || !Choice || !(*Choice)->TryGetArrayField(TEXT("stones"), Stones) || !Stones || !Stones->IsValidIndex(Stone) || !(*Stones)[Stone].IsValid())
	{
		return nullptr;
	}
	return (*Stones)[Stone]->AsObject();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaIntermissionSliceData, "MageArena.Intermission.SliceData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaIntermissionSliceData::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FStorySlice Slice;
	FString Error;
	const bool bLoaded = Slice.Load(FStorySlice::DefaultFilePath(), Error);
	if (!TestTrue(*FString::Printf(TEXT("the committed slice.json loads (%s)"), *Error), bLoaded))
	{
		return false;
	}
	bool bPass = TestEqual(TEXT("three scenes (C2 decision D3)"), Slice.GetScenes().Num(), 3);
	for (int32 WaveN = 1; WaveN <= 3; ++WaveN)
	{
		const FStoryScene* Scene = SceneAfter(Slice, WaveN);
		const FString Label = FString::Printf(TEXT("scene after wave %d"), WaveN);
		if (!TestNotNull(*Label, Scene))
		{
			bPass = false;
			continue;
		}
		bPass &= TestEqual(*(Label + TEXT(" is intermission n")), Scene->Intermission, WaveN);
		bPass &= TestTrue(*FString::Printf(TEXT("%s: 0 <= floor %.1f <= natural %.1f <= max %.1f <= 20"), *Label, Scene->FloorS, Scene->NaturalS, Scene->MaxS),
		    Scene->FloorS >= 0.0 && Scene->FloorS <= Scene->NaturalS && Scene->NaturalS <= Scene->MaxS && Scene->MaxS <= 20.0);
		bPass &= TestEqual(*(Label + TEXT(" writes its intermission flag")), Scene->Writes, FArenaFlags::IntermissionKey(WaveN));
		bPass &= TestEqual(*(Label + TEXT(" has three stones")), Scene->Stones.Num(), 3);
		for (const FString& Stone : Scene->Stones)
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s stone label '%s' is 5 words or fewer"), *Label, *Stone), FStorySlice::CountWords(Stone) <= 5);
		}
		bPass &= TestTrue(*(Label + TEXT(" ends on a default variant (when: null)")), Scene->Variants.Num() > 0 && Scene->Variants.Last().When.Num() == 0);
	}
	const FStoryScene* First = SceneAfter(Slice, 1);
	bPass &= TestTrue(TEXT("intermission 1 wraps the T20 pick"), First && First->bKeepsExistingPick);
	bPass &= TestTrue(TEXT("prologue floor 6 s"), FMath::IsNearlyEqual(Slice.BeatFloorS(TEXT("prologue")), 6.0));
	bPass &= TestTrue(TEXT("closing floor 6 s"), FMath::IsNearlyEqual(Slice.BeatFloorS(TEXT("closing")), 6.0));
	bPass &= TestEqual(TEXT("an unknown beat has no floor"), Slice.BeatFloorS(TEXT("no-such-beat")), 0.0);

	// The fallback rule (C2 section 8): a missing, unreadable, non-version-1 or malformed file loads nothing, never half.
	struct FCase
	{
		const TCHAR* Name;
		TFunction<void(FJsonObject&)> Mutate;
	};
	TArray<FCase> Cases;
	Cases.Add({TEXT("version 2"), [](FJsonObject& Root)
	{ Root.SetNumberField(TEXT("version"), 2.0); }});
	Cases.Add({TEXT("no scenes"), [](FJsonObject& Root)
	{ Root.RemoveField(TEXT("scenes")); }});
	Cases.Add({TEXT("a scene of 25 s"), [](FJsonObject& Root)
	{
		if (const TSharedPtr<FJsonObject> Scene = SceneObject(Root, 1))
		{
			Scene->SetNumberField(TEXT("maxS"), 25.0);
		}
	}});
	Cases.Add({TEXT("floor above natural"), [](FJsonObject& Root)
	{
		if (const TSharedPtr<FJsonObject> Scene = SceneObject(Root, 2))
		{
			Scene->SetNumberField(TEXT("floorS"), 19.0);
		}
	}});
	Cases.Add({TEXT("a six-word label"), [](FJsonObject& Root)
	{
		if (const TSharedPtr<FJsonObject> Stone = StoneObject(Root, 1, 0))
		{
			Stone->SetStringField(TEXT("label"), TEXT("one two three four five six"));
		}
	}});
	Cases.Add({TEXT("a choice writing another flag"), [](FJsonObject& Root)
	{
		const TSharedPtr<FJsonObject> Scene = SceneObject(Root, 1);
		const TSharedPtr<FJsonObject>* Choice = nullptr;
		if (Scene.IsValid() && Scene->TryGetObjectField(TEXT("choice"), Choice) && Choice)
		{
			(*Choice)->SetStringField(TEXT("writes"), TEXT("arena.tiro.1.w2.won"));
		}
	}});
	for (const FCase& Case : Cases)
	{
		const FString Path = SliceVariant(*this, Case.Mutate);
		if (Path.IsEmpty())
		{
			bPass = false;
			continue;
		}
		FStorySlice Variant;
		FString VariantError;
		const bool bVariantLoaded = Variant.Load(Path, VariantError);
		bPass &= TestFalse(*FString::Printf(TEXT("%s is skipped (%s)"), Case.Name, *VariantError), bVariantLoaded);
		bPass &= TestFalse(*FString::Printf(TEXT("%s leaves nothing loaded"), Case.Name), Variant.IsLoaded() || Variant.GetScenes().Num() > 0);
		IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Path), false, true);
	}
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("slice"), FGuid::NewGuid().ToString());
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Truncated = FPaths::Combine(Dir, TEXT("slice.json"));
	FString Text;
	FFileHelper::LoadFileToString(Text, *FStorySlice::DefaultFilePath());
	FFileHelper::SaveStringToFile(Text.Left(Text.Len() / 2), *Truncated);
	FStorySlice Broken;
	bPass &= TestFalse(TEXT("truncated JSON is skipped"), Broken.Load(Truncated, Error));
	bPass &= TestFalse(TEXT("a missing file is skipped"), Broken.Load(FPaths::Combine(Dir, TEXT("absent.json")), Error));
	bPass &= TestEqual(TEXT("a skipped slice has no floor"), Broken.BeatFloorS(TEXT("prologue")), 0.0);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaIntermissionWhenSelects, "MageArena.Intermission.WhenSelectsScene",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaIntermissionWhenSelects::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FStorySlice Slice;
	FString Error;
	if (!TestTrue(TEXT("slice.json loads"), Slice.Load(FStorySlice::DefaultFilePath(), Error)))
	{
		AddError(Error);
		return false;
	}
	const FStoryScene* Second = SceneAfter(Slice, 2);
	const FStoryScene* Third = SceneAfter(Slice, 3);
	if (!TestNotNull(TEXT("scene 2"), Second) || !TestNotNull(TEXT("scene 3"), Third))
	{
		return false;
	}
	auto Pick = [](const FStoryScene& Scene, const FArenaFlags& With)
	{
		const int32 Index = Scene.SelectVariant(With);
		return Scene.Variants.IsValidIndex(Index) ? Scene.Variants[Index].Id : FString(TEXT("none"));
	};
	bool bPass = true;
	FArenaFlags Probe;
	// Scene 2: arena.tiro.1.w2.perfects >= 2 is 2A, otherwise 2B. A flag that is not set does not match (no error).
	bPass &= TestEqual(TEXT("no flags: the default 2B"), Pick(*Second, Probe), FString(TEXT("2B")));
	Probe.SetNumber(FArenaFlags::TiroKey(2, TEXT("perfects")), 1.0);
	bPass &= TestEqual(TEXT("one perfect: 2B"), Pick(*Second, Probe), FString(TEXT("2B")));
	Probe.SetNumber(FArenaFlags::TiroKey(2, TEXT("perfects")), 2.0);
	bPass &= TestEqual(TEXT("two perfects: 2A"), Pick(*Second, Probe), FString(TEXT("2A")));
	Probe.SetBool(FArenaFlags::TiroKey(2, TEXT("perfects")), true);
	bPass &= TestEqual(TEXT("a bool where a number is named does not match"), Pick(*Second, Probe), FString(TEXT("2B")));
	// Scene 3: won and one attempt is 3A, won is 3B, else 3C.
	Probe.Reset();
	bPass &= TestEqual(TEXT("not fought: 3C"), Pick(*Third, Probe), FString(TEXT("3C")));
	Probe.SetBool(FArenaFlags::TiroKey(3, TEXT("won")), false);
	Probe.SetNumber(FArenaFlags::TiroKey(3, TEXT("attempts")), 1.0);
	bPass &= TestEqual(TEXT("lost: 3C"), Pick(*Third, Probe), FString(TEXT("3C")));
	Probe.SetBool(FArenaFlags::TiroKey(3, TEXT("won")), true);
	bPass &= TestEqual(TEXT("won at the first attempt: 3A"), Pick(*Third, Probe), FString(TEXT("3A")));
	Probe.SetNumber(FArenaFlags::TiroKey(3, TEXT("attempts")), 2.0);
	bPass &= TestEqual(TEXT("won after a retry: 3B"), Pick(*Third, Probe), FString(TEXT("3B")));

	// In the day: Bout 2 won with two perfects selects 2A, and the scene shows 2A's line, then the shared line.
	FSceneRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	if (!TestTrue(TEXT("into Bout 2"), SceneReachBout(Rig, Session, 1)))
	{
		Rig.Close(Session);
		return false;
	}
	FArenaState& State = const_cast<FArenaState&>(Session.GetGames().State);
	if (FActor* Player = SimFindActor(State, Session.GetGames().PlayerId))
	{
		Player->Metrics.Perfects += 2;
	}
	SceneWinIntoNext(Session);
	bPass &= TestEqual(TEXT("the intermission after Bout 2"), Session.GetStage(), FString(TEXT("intermission")));
	Session.Advance(kSceneTick, true);
	bPass &= TestEqual(TEXT("the day selected 2A"), Session.GetSceneVariantId(), FString(TEXT("2A")));
	const FStoryVariant* Chosen = Second->Variants.Num() > 0 ? &Second->Variants[0] : nullptr;
	const bool bAuthored = Chosen && Chosen->Id == TEXT("2A") && Chosen->Lines.Num() > 0 && Second->Always.Num() > 0;
	bPass &= TestTrue(TEXT("2A and the shared line are authored"), bAuthored);
	if (bAuthored)
	{
		bPass &= TestEqual(TEXT("2A's line stands first"), Session.GetPromptText(), Chosen->Lines[0].Text);
		const double LineS = Chosen->Lines[0].DwellS;
		SceneRunUntil(Session, [&Session, LineS]()
		{ return Session.GetSceneClock() >= LineS + kSceneTick; }, LineS + 1.0);
		bPass &= TestEqual(TEXT("then the line every variant shares"), Session.GetPromptText(), Second->Always[0].Text);
	}
	bPass &= TestEqual(TEXT("the chain names the variant"), SceneCountLines(Session.GetChain(), TEXT("intermission 2 scene int-2 variant=2A")), 1);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaIntermissionChoiceWritesFlag, "MageArena.Intermission.ChoiceWritesFlag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaIntermissionChoiceWritesFlag::RunTest(const FString& Parameters)
{
	(void)Parameters;
	bool bPass = true;
	{
		// Intermission 1 is the T20 pick (C2 int-1 keepsExistingPick): the right stone picks the preset's right branch,
		// once, and the pick is recorded as the scene's choice too.
		FSceneRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		FArenaSession Session;
		if (!TestTrue(TEXT("into Bout 1"), SceneReachBout(Rig, Session, 0)))
		{
			Rig.Close(Session);
			return false;
		}
		SceneWinIntoNext(Session);
		Session.Advance(kSceneTick, true);
		bPass &= TestEqual(TEXT("intermission 1"), Session.GetStage(), FString(TEXT("intermission")));
		bPass &= TestTrue(TEXT("the T20 pick is offered"), Session.IsPickOffered());
		bPass &= TestFalse(TEXT("no second set of stones over the pick"), Session.AreSceneStonesShown());
		bPass &= TestEqual(TEXT("the pick keeps its own labels"), Session.PickStoneKey(2), FString::Printf(TEXT("pick.tideOrb.%s"), *Session.GetPlayerPreset().Pick.Right));
		Rig.Hands->PlayQuickAction(TEXT("stone-right"), EClipVariant::Normal);
		SceneRunWhile(Session, TEXT("intermission"), 3.0);
		bPass &= TestEqual(TEXT("the pick continues the day"), Session.GetStage(), FString(TEXT("intro")));
		bPass &= TestEqual(TEXT("the right branch"), Session.GetStonePick(), Session.GetPlayerPreset().Pick.Right);
		double Choice = -1.0;
		bPass &= TestTrue(TEXT("arena.intermission.1.choice is the right stone (2)"),
		    Session.GetFlags().GetNumber(FArenaFlags::IntermissionKey(1), Choice) && Choice == 2.0);
		bPass &= TestEqual(TEXT("one pick, not duplicated"), SceneCountLines(Session.GetChain(), TEXT("pick stone stone=2")), 1);
		bPass &= TestEqual(TEXT("no second scene choice"), SceneCountLines(Session.GetChain(), TEXT("intermission 1 choice")), 0);
		Rig.Close(Session);
	}
	{
		// Intermission 2 offers C2's own stones. The left one, held offerHoldS, writes 0.
		FSceneRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		FArenaSession Session;
		if (!TestTrue(TEXT("into Bout 2"), SceneReachBout(Rig, Session, 1)))
		{
			Rig.Close(Session);
			return false;
		}
		SceneWinIntoNext(Session);
		Session.Advance(kSceneTick, true);
		const FStoryScene* Scene = Session.GetActiveScene();
		if (!TestNotNull(TEXT("intermission 2 has its scene"), Scene))
		{
			Rig.Close(Session);
			return false;
		}
		bPass &= TestFalse(TEXT("no T20 pick after Bout 2"), Session.IsPickOffered());
		bPass &= TestTrue(TEXT("the scene's stones are shown"), Session.AreSceneStonesShown());
		for (int32 Index = 0; Index < 3; ++Index)
		{
			bPass &= TestEqual(*FString::Printf(TEXT("stone %d carries C2's label"), Index), Session.SceneStoneKey(Index), Scene->Stones[Index]);
			bPass &= TestTrue(*FString::Printf(TEXT("stone %d shows 5 words or fewer"), Index),
			    FStorySlice::CountWords(Session.TeachString(*Session.SceneStoneKey(Index))) <= 5);
		}
		bPass &= TestEqual(TEXT("the centre stone reads Continue"), Session.TeachString(*Session.SceneStoneKey(1)), Session.TeachString(TEXT("pick.continue")));
		Rig.Hands->PlayQuickAction(TEXT("stone-left"), EClipVariant::Normal);
		double Glow = 0.0;
		const double Took = SceneRunUntil(Session, [&]()
		{
			Glow = FMath::Max(Glow, Session.GetSceneHoldFraction());
			return Session.GetStage() != TEXT("intermission");
		}, 3.0);
		bPass &= TestEqual(TEXT("the choice continues the day"), Session.GetStage(), FString(TEXT("intro")));
		bPass &= TestTrue(*FString::Printf(TEXT("chosen within the scene (%.2f s)"), Took), Took < 3.0);
		bPass &= TestTrue(*FString::Printf(TEXT("the stone glowed through the hold (%.2f)"), Glow), Glow >= 0.9);
		double Choice = -1.0;
		bPass &= TestTrue(TEXT("arena.intermission.2.choice is the left stone (0)"),
		    Session.GetFlags().GetNumber(FArenaFlags::IntermissionKey(2), Choice) && Choice == 0.0);
		bPass &= TestEqual(TEXT("one choice line"), SceneCountLines(Session.GetChain(), TEXT("intermission 2 choice stone=0")), 1);
		FArenaFlags Saved;
		double SavedChoice = -1.0;
		bPass &= TestTrue(TEXT("the flag is in the flags file"),
		    Saved.Load(Session.FlagsFilePath()) && Saved.GetNumber(FArenaFlags::IntermissionKey(2), SavedChoice) && SavedChoice == 0.0);
		bPass &= TestTrue(TEXT("the choice changes no stone pick"), Session.GetStonePick().IsEmpty());
		Rig.Close(Session);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaIntermissionTimeoutWritesNone, "MageArena.Intermission.TimeoutWritesNone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaIntermissionTimeoutWritesNone::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSceneRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	// Bout 3 (Brennic): won, spared by missio's timeout, then intermission 3 with nobody at the stones.
	if (!TestTrue(TEXT("into Bout 3"), SceneReachBout(Rig, Session, 2)))
	{
		Rig.Close(Session);
		return false;
	}
	SceneWinIntoNext(Session);
	bool bPass = TestEqual(TEXT("intermission 3"), Session.GetStage(), FString(TEXT("intermission")));
	const int32 FlagsBefore = Session.GetFlags().Num();
	const int32 CracksBefore = Session.GetDayCracks();
	const int32 TitheBefore = Session.GetDayTithe();
	const double Lasted = SceneRunWhile(Session, TEXT("intermission"), 25.0);
	// slice.json int-3 maxS 20: the day goes on on the tick the scene clock reaches 20 s (1200 ticks of 1/60 s).
	bPass &= TestTrue(*FString::Printf(TEXT("the scene continues by itself at 20 s (%.4f)"), Lasted), FMath::Abs(Lasted - 20.0) <= kSceneTick + 1.0e-6);
	bPass &= TestEqual(TEXT("into the final's intro"), Session.GetStage(), FString(TEXT("intro")));
	bPass &= TestEqual(TEXT("the final is next"), Session.GetGames().Wave, 3);
	bPass &= TestFalse(TEXT("no choice flag on a timeout"), Session.GetFlags().Has(FArenaFlags::IntermissionKey(3)));
	bPass &= TestEqual(TEXT("no flag written at all"), Session.GetFlags().Num(), FlagsBefore);
	bPass &= TestEqual(TEXT("no Cracks charged"), Session.GetDayCracks(), CracksBefore);
	bPass &= TestEqual(TEXT("no Tithe charged"), Session.GetDayTithe(), TitheBefore);
	bPass &= TestEqual(TEXT("one timeout line"), SceneCountLines(Session.GetChain(), TEXT("intermission 3 timeout")), 1);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaIntermissionNeverPast20, "MageArena.Intermission.NeverPast20",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaIntermissionNeverPast20::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSceneRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	// The whole day with nobody at the stones: every intermission ends itself within 20 s, and so does the closing card.
	if (!TestTrue(TEXT("into Bout 1"), SceneReachBout(Rig, Session, 0)))
	{
		Rig.Close(Session);
		return false;
	}
	bool bPass = true;
	for (int32 Bout = 0; Bout < 4; ++Bout)
	{
		bPass &= TestEqual(*FString::Printf(TEXT("bout %d is live"), Bout + 1), Session.GetStage(), FString(TEXT("active")));
		SceneWinIntoNext(Session);
		if (Bout == 3)
		{
			break;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("intermission after bout %d"), Bout + 1), Session.GetStage(), FString(TEXT("intermission")));
		const double Lasted = SceneRunWhile(Session, TEXT("intermission"), 30.0);
		bPass &= TestTrue(*FString::Printf(TEXT("intermission %d held the day %.4f s, never past 20 s"), Bout + 1, Lasted), Lasted <= 20.0 + kSceneTick + 1.0e-6);
		bPass &= TestEqual(*FString::Printf(TEXT("intermission %d went on to the intro"), Bout + 1), Session.GetStage(), FString(TEXT("intro")));
		SceneRunWhile(Session, TEXT("intro"), 15.0);
	}
	bPass &= TestEqual(TEXT("no choice was written"), CountIntermissionFlags(Session), 0);
	bPass &= TestEqual(TEXT("the aftermath"), Session.GetStage(), FString(TEXT("aftermath")));
	// day.json aftermathTabletS 16 + closingCardS 34: the day closes itself at 50 s of aftermath.
	const double Closing = SceneRunWhile(Session, TEXT("aftermath"), 60.0);
	const double Expected = Session.GetDayTuning().AftermathTabletS + Session.GetDayTuning().ClosingCardS;
	bPass &= TestTrue(*FString::Printf(TEXT("the closing card ends itself at %.1f s (%.4f)"), Expected, Closing), FMath::Abs(Closing - Expected) <= kSceneTick + 1.0e-6);
	bPass &= TestEqual(TEXT("closed"), Session.GetStage(), FString(TEXT("closed")));
	bPass &= TestEqual(TEXT("one closing timeout line"), SceneCountLines(Session.GetChain(), TEXT("day closing timeout")), 1);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaIntermissionFloorGatesPalm, "MageArena.Intermission.FloorGatesPalm",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaIntermissionFloorGatesPalm::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSceneRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	if (!TestTrue(TEXT("into Bout 2"), SceneReachBout(Rig, Session, 1)))
	{
		Rig.Close(Session);
		return false;
	}
	SceneWinIntoNext(Session);
	const FStoryScene* Scene = Session.GetActiveScene();
	if (!TestNotNull(TEXT("intermission 2 has its scene"), Scene))
	{
		Rig.Close(Session);
		return false;
	}
	const double Floor = Scene->FloorS;
	bool bPass = TestTrue(TEXT("floorS 3"), FMath::IsNearlyEqual(Floor, 3.0));
	// A palm at 1 s (a ward still up from the bout) does nothing.
	SceneRunUntil(Session, [&Session]()
	{ return Session.GetSceneClock() >= 1.0; }, 2.0);
	Rig.RaisePalm();
	Session.Advance(kSceneTick, true);
	bPass &= TestEqual(TEXT("below the floor the palm does not continue"), Session.GetStage(), FString(TEXT("intermission")));
	bPass &= TestEqual(TEXT("the ignored palm is noted"), SceneCountLines(Session.GetChain(), TEXT("intermission 2 palm below the floor")), 1);
	SceneRunUntil(Session, [&Session, Floor]()
	{ return Session.GetSceneClock() + 1.0e-9 >= Floor; }, Floor);
	bPass &= TestEqual(TEXT("the ignored palm is not kept for later"), Session.GetStage(), FString(TEXT("intermission")));
	Rig.RaisePalm();
	Session.Advance(kSceneTick, true);
	bPass &= TestEqual(TEXT("from the floor the palm continues"), Session.GetStage(), FString(TEXT("intro")));
	bPass &= TestFalse(TEXT("a palm writes no choice"), Session.GetFlags().Has(FArenaFlags::IntermissionKey(2)));
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaIntermissionPickUnreachableInBout, "MageArena.Intermission.PickUnreachableInBout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaIntermissionPickUnreachableInBout::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSceneRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	// Bout 1: an idle seat loses it only at 5.48 s (Session/DayCapture.cpp), so 2 s of the bout stay live.
	Rig.WriteSave(TEXT("bout=0\n"));
	Rig.Bind(Session);
	Session.SetScripted(false);
	bool bPass = TestTrue(TEXT("day begins"), Session.BeginArc(1));
	Rig.RaisePalm();
	Session.Advance(kSceneTick, true);
	SceneRunUntil(Session, [&Session]()
	{ return Session.GetPhaseClock() + 1.0e-9 >= Session.GetRitualOfferS(); }, 15.0);
	Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
	SceneRunWhile(Session, TEXT("ritual"), 5.0);
	bPass &= TestEqual(TEXT("the intro"), Session.GetStage(), FString(TEXT("intro")));
	// The left stone held through the intro and into the live bout: no scene, no stones, no choice.
	bool bReachable = false;
	bool bReplayed = false;
	double ActiveS = 0.0;
	Rig.Hands->PlayQuickAction(TEXT("stone-left"), EClipVariant::Normal);
	for (int32 Step = 0; Step < 60 * 20 && ActiveS < 2.0; ++Step)
	{
		if (Session.GetStage() == TEXT("active") && !bReplayed)
		{
			Rig.Hands->PlayQuickAction(TEXT("stone-left"), EClipVariant::Normal);
			bReplayed = true;
		}
		Session.Advance(kSceneTick, true);
		ActiveS += Session.GetStage() == TEXT("active") ? kSceneTick : 0.0;
		bReachable |= Session.GetActiveScene() != nullptr || Session.AreSceneStonesShown() || Session.IsPickOffered();
	}
	bPass &= TestTrue(*FString::Printf(TEXT("the bout ran (%.2f s active)"), ActiveS), ActiveS >= 2.0);
	bPass &= TestFalse(TEXT("no scene or pick is reachable in the intro or the bout"), bReachable);
	bPass &= TestEqual(TEXT("still the live bout"), Session.GetStage(), FString(TEXT("active")));
	bPass &= TestEqual(TEXT("no choice flag"), CountIntermissionFlags(Session), 0);
	bPass &= TestEqual(TEXT("no scene line in the chain"), SceneCountLines(Session.GetChain(), TEXT("intermission ")), 0);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaIntermissionWithoutSlice, "MageArena.Intermission.WithoutSlice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaIntermissionWithoutSlice::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSceneRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	// A missing slice is skipped with a warning; the day plays as before T28: no floor, no scene, the palm at once.
	Session.SetSlicePath(FPaths::Combine(Rig.Dir, TEXT("no-slice.json")));
	if (!TestTrue(TEXT("the day still begins and reaches Bout 1"), SceneReachBout(Rig, Session, 0)))
	{
		Rig.Close(Session);
		return false;
	}
	bool bPass = TestFalse(TEXT("no slice loaded"), Session.GetSlice().IsLoaded());
	bPass &= TestEqual(TEXT("no prologue floor"), Session.BeatFloorS(TEXT("prologue")), 0.0);
	SceneWinIntoNext(Session);
	bPass &= TestEqual(TEXT("intermission 1"), Session.GetStage(), FString(TEXT("intermission")));
	bPass &= TestNull(TEXT("no scene"), Session.GetActiveScene());
	bPass &= TestTrue(TEXT("the T20 pick still stands"), Session.IsPickOffered());
	Rig.RaisePalm();
	Session.Advance(kSceneTick, true);
	bPass &= TestEqual(TEXT("the palm continues at once, as before"), Session.GetStage(), FString(TEXT("intro")));
	bPass &= TestEqual(TEXT("no choice flag"), CountIntermissionFlags(Session), 0);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaIntermissionDayLengthBudget, "MageArena.Intermission.DayLengthBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaIntermissionDayLengthBudget::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// MageArenaDesign.Session.FullSeated plays the rushed path: every skip at its floor, the palm at each scene's floor.
	// Its expected move from T28's data, against T21's (introS 3 per bout, aftermathTabletS 4, no floors, no scenes).
	FSceneRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Rig.WriteSave(TEXT("bout=0\n"));
	Rig.Bind(Session);
	if (!TestTrue(TEXT("day begins"), Session.BeginArc(1)))
	{
		Rig.Close(Session);
		return false;
	}
	const FDayTuning& Day = Session.GetDayTuning();
	const FStorySlice& Slice = Session.GetSlice();
	double IntroS = 0.0;
	for (int32 Wave = 0; Wave < 4; ++Wave)
	{
		IntroS += Day.IntroSeconds(Wave);
	}
	double FloorsS = 0.0;
	double AllowedS = 0.0;
	for (const FStoryScene& Scene : Slice.GetScenes())
	{
		FloorsS += Scene.FloorS;
		AllowedS += Scene.MaxS;
	}
	const double PrologueS = Slice.BeatFloorS(TEXT("prologue"));
	const double ClosingS = Slice.BeatFloorS(TEXT("closing"));
	const double IntroDelta = IntroS - 4.0 * 3.0;
	const double TabletDelta = Day.AftermathTabletS - 4.0;
	const double FullDayDelta = PrologueS + IntroDelta + FloorsS + TabletDelta + ClosingS;
	// Today's FullSeated stops at bout 3 (DF-011): bouts 1-3's intros, intermissions 1-2, no aftermath.
	const double ThreeBoutDelta = PrologueS + (Day.IntroSeconds(0) + Day.IntroSeconds(1) + Day.IntroSeconds(2) - 3.0 * 3.0) + (SceneAfter(Slice, 1) ? SceneAfter(Slice, 1)->FloorS : 0.0) + (SceneAfter(Slice, 2) ? SceneAfter(Slice, 2)->FloorS : 0.0);
	UE_LOG(LogMageArena, Log, TEXT("MageArena.Intermission.DayLengthBudget FullSeated expected +%.0f s (day closes) / +%.0f s (stops at bout 3): prologue +%.0f, intros +%.0f, intermissions +%.0f of %.0f allowed, tablet +%.0f, closing +%.0f"),
	    FullDayDelta, ThreeBoutDelta, PrologueS, IntroDelta, FloorsS, AllowedS, TabletDelta, ClosingS);
	bool bPass = TestEqual(TEXT("introS [8, 8, 12, 12] is +28 s over T21's 3 s per bout"), IntroDelta, 28.0);
	bPass &= TestEqual(TEXT("aftermathTabletS 16 is +12 s"), TabletDelta, 12.0);
	bPass &= TestEqual(TEXT("the three scenes allow 60 s"), AllowedS, 60.0);
	bPass &= TestEqual(TEXT("the rushed intermissions cost their floors, 9 s"), FloorsS, 9.0);
	bPass &= TestTrue(TEXT("the intermissions move FullSeated by no more than the scene time allowed"), FloorsS <= AllowedS);
	bPass &= TestEqual(TEXT("a FullSeated day that closes moves by +61 s"), FullDayDelta, 61.0);
	bPass &= TestEqual(TEXT("today's FullSeated (stops at bout 3) moves by +31 s"), ThreeBoutDelta, 31.0);
	Rig.Close(Session);
	return bPass;
}

#endif
