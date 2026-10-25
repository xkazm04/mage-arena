#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// T27: the C2 voice lines, wired with no WAV in the tree (audio deferred, docs/DECISIONS.md 2026-10-24, "Music and audio
// deferred"). Literals and their sources:
// - apps/vr/art/audio/voice/manifest.json: the cue sheet of docs/campaign/05-c2-slice-story-bible-and-script.md
//   section 9, 27 rows, every line 10 words or fewer. Read, never retyped here.
// - apps/vr/data/vr/story/slice.json (C2): the lines that carry the cues, the taunts at 0.66 and 0.33 of the rival's HP.
// - apps/vr/data/vr/day.json: ritualLineS [12, 14, 12], ritualTimeoutS 45; slice.json beats ritual.floorS 10.
// - combat.vr.json rivals brennic and lio: two HP-gated phases each.
// - teach.json offerHoldS 0.40.

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Session/ArenaAudio.h"
#include "Session/ArenaSession.h"
#include "Session/StorySlice.h"
#include "Session/VoiceLines.h"

namespace
{
const double kVoiceTick = 1.0 / 60.0;

struct FVoiceRig
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
		Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("voice"), FGuid::NewGuid().ToString());
		IFileManager::Get().MakeDirectory(*Dir, true);
		return true;
	}

	void Bind(FArenaSession& Session) const
	{
		Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
		Session.SetSaveDirectory(Dir);
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

bool LoadManifest(FAutomationTestBase& Test, FVoiceManifest& Manifest)
{
	FString Error;
	return Test.TestTrue(*FString::Printf(TEXT("the voice manifest loads (%s)"), *Error), Manifest.LoadFromFile(FVoiceManifest::DefaultPath(), Error));
}

bool LoadSlice(FAutomationTestBase& Test, FStorySlice& Slice)
{
	FString Error;
	return Test.TestTrue(*FString::Printf(TEXT("slice.json loads (%s)"), *Error), Slice.Load(FStorySlice::DefaultFilePath(), Error));
}

// From a save naming Bout 1: the offer, a palm, into the ritual.
bool ReachRitual(FVoiceRig& Rig, FArenaSession& Session)
{
	FFileHelper::SaveStringToFile(TEXT("bout=0\n"), *FPaths::Combine(Rig.Dir, TEXT("bout.txt")));
	Rig.Bind(Session);
	Session.SetScripted(false);
	if (!Session.BeginArc(1))
	{
		return false;
	}
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(kVoiceTick, true);
	return Session.GetStage() == TEXT("ritual");
}

// Steps the session and the audio component together, as the presentation does each frame.
double RunVoice(FArenaSession& Session, UArenaAudioComponent& Audio, TFunctionRef<bool()> Done, double LimitS)
{
	double Elapsed = 0.0;
	while (Elapsed < LimitS && !Done())
	{
		Session.Advance(kVoiceTick, true);
		Audio.Sync(Session, false, FVector(-1200.0, 0.0, 180.0), FRotator::ZeroRotator);
		Elapsed += kVoiceTick;
	}
	return Elapsed;
}

FString CueList(const TArray<FVoiceCue>& Cues)
{
	TArray<FString> Ids;
	for (const FVoiceCue& Cue : Cues)
	{
		Ids.Add(FString::Printf(TEXT("%s@%s+%.2f"), *Cue.Cue, *Cue.Phase, Cue.PhaseS));
	}
	return FString::Join(Ids, TEXT(" "));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaVoiceTenWords, "MageArena.Voice.TenWordsOrFewer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaVoiceTenWords::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FVoiceManifest Manifest;
	if (!LoadManifest(*this, Manifest))
	{
		return false;
	}
	// C2 section 9: 27 cues, every one 10 words or fewer.
	bool bPass = TestEqual(TEXT("the cue sheet has 27 rows"), Manifest.Rows.Num(), 27);
	for (const FVoiceManifestRow& Row : Manifest.Rows)
	{
		const int32 Words = FStorySlice::CountWords(Row.Text);
		bPass &= TestTrue(*FString::Printf(TEXT("%s has 10 words or fewer (%d: %s)"), *Row.Id, Words, *Row.Text), Words > 0 && Words <= 10);
		bPass &= TestEqual(*FString::Printf(TEXT("%s words matches C2's count"), *Row.Id), Words, Row.Words);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaVoiceTriggerPerRow, "MageArena.Voice.TriggerPerRow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaVoiceTriggerPerRow::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FVoiceManifest Manifest;
	FStorySlice Slice;
	if (!LoadManifest(*this, Manifest) || !LoadSlice(*this, Slice))
	{
		return false;
	}
	bool bPass = true;
	const TMap<FString, TArray<FStoryVoiceTrigger>> Triggers = Slice.VoiceTriggers();
	// Every cue-sheet row maps to exactly one trigger, and speaks the slice's text in the slice's voice.
	for (const FVoiceManifestRow& Row : Manifest.Rows)
	{
		const TArray<FStoryVoiceTrigger>* Found = Triggers.Find(Row.Id);
		TArray<FString> Events;
		if (Found)
		{
			for (const FStoryVoiceTrigger& Trigger : *Found)
			{
				Events.AddUnique(Trigger.Event);
			}
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s maps to one trigger (%s)"), *Row.Id, *FString::Join(Events, TEXT(", "))), Events.Num(), 1);
		if (Found)
		{
			for (const FStoryVoiceTrigger& Trigger : *Found)
			{
				bPass &= TestEqual(*FString::Printf(TEXT("%s text at %s"), *Row.Id, *Trigger.Event), Trigger.Text, Row.Text);
				bPass &= TestEqual(*FString::Printf(TEXT("%s speaker at %s"), *Row.Id, *Trigger.Event), Trigger.Speaker, Row.Speaker);
			}
		}
	}
	// And no slice line carries a cue the sheet does not have.
	for (const TPair<FString, TArray<FStoryVoiceTrigger>>& Pair : Triggers)
	{
		bPass &= TestNotNull(*FString::Printf(TEXT("slice cue %s is on the cue sheet"), *Pair.Key), Manifest.Find(Pair.Key));
	}
	// The taunts break at the rivals' HP-gated phases (combat.vr.json), in order: the trigger the session raises on a
	// kernel phase event is the line whose atHpFraction is that phase's threshold.
	FVrRuleset Rules;
	FString RulesError;
	if (!TestTrue(*FString::Printf(TEXT("combat.vr.json loads (%s)"), *RulesError), LoadVrRuleset(Rules, RulesError)))
	{
		return false;
	}
	for (const TCHAR* RivalId : {TEXT("brennic"), TEXT("lio")})
	{
		const FVrRival* Rival = Rules.Rivals.FindByPredicate([RivalId](const FVrRival& Each)
		{ return Each.Id == RivalId; });
		if (!TestNotNull(*FString::Printf(TEXT("rival %s"), RivalId), Rival))
		{
			bPass = false;
			continue;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s breaks twice"), RivalId), Rival->Phases.Num(), 2);
		for (int32 Index = 0; Index < Rival->Phases.Num(); ++Index)
		{
			const FStoryTaunt* Taunt = Slice.FindTaunt(RivalId, Rival->Phases[Index].AtHpFraction);
			const FString Expected = FString::Printf(TEXT("%s-0%d"), RivalId == FString(TEXT("brennic")) ? TEXT("BRE") : TEXT("LIO"), Index + 1);
			bPass &= TestTrue(*FString::Printf(TEXT("%s phase %d at %.2f says %s"), RivalId, Index + 1, Rival->Phases[Index].AtHpFraction, *Expected),
			    Taunt && Taunt->Line.Cue == Expected);
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaVoiceMissingAssetNoStall, "MageArena.Voice.MissingAssetNoStall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaVoiceMissingAssetNoStall::RunTest(const FString& Parameters)
{
	(void)Parameters;
	bool bPass = true;
	{
		// No palm: the ritual's three lines raise their cues at 0, 12 and 26 s (day.json ritualLineS [12, 14, 12]), no
		// asset exists for any of them, and the ritual still ends at its 45 s timeout into Bout 1's intro and VEN-01.
		FVoiceRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		FArenaSession Session;
		UArenaAudioComponent* Audio = NewObject<UArenaAudioComponent>();
		Audio->AddToRoot();
		Audio->SetRecordOnly(true);
		bPass &= TestTrue(TEXT("into the ritual"), ReachRitual(Rig, Session));
		const double Lasted = RunVoice(Session, *Audio, [&Session]()
		{ return Session.GetStage() != TEXT("ritual"); }, 50.0);
		RunVoice(Session, *Audio, [&Session]()
		{ return false; }, 2.0 * kVoiceTick);
		const TArray<FVoiceCue>& Cues = Session.GetVoiceCues();
		const FString Seen = CueList(Cues);
		bPass &= TestTrue(*FString::Printf(TEXT("the ritual times out at 45 s, not stalled (%.3f)"), Lasted), FMath::Abs(Lasted - 45.0) <= 2.0 * kVoiceTick);
		bPass &= TestEqual(TEXT("into Bout 1's intro"), Session.GetStage(), FString(TEXT("intro")));
		bPass &= TestEqual(*FString::Printf(TEXT("four cues: the ritual's three, then the intro (%s)"), *Seen), Cues.Num(), 4);
		const TCHAR* Expected[] = {TEXT("RIT-01"), TEXT("RIT-02"), TEXT("RIT-03"), TEXT("VEN-01")};
		const double At[] = {0.0, 12.0, 26.0, 0.0};
		for (int32 Index = 0; Index < 4 && Index < Cues.Num(); ++Index)
		{
			bPass &= TestEqual(*FString::Printf(TEXT("cue %d"), Index + 1), Cues[Index].Cue, FString(Expected[Index]));
			bPass &= TestTrue(*FString::Printf(TEXT("%s on its line (%.3f s)"), Expected[Index], Cues[Index].PhaseS),
			    FMath::Abs(Cues[Index].PhaseS - At[Index]) <= 2.0 * kVoiceTick);
		}
		bPass &= TestTrue(TEXT("Septima speaks the ritual"), Cues.Num() >= 3 && Cues[0].Speaker == TEXT("septima") && Cues[2].Speaker == TEXT("septima"));
		bPass &= TestEqual(TEXT("no asset, every cue skipped"), Audio->GetVoiceSkipped(), Cues.Num());
		bPass &= TestEqual(TEXT("nothing played"), Audio->GetRecorded().FilterByPredicate([](const FAudioPlayRequest& Request)
		{ return Request.Event.StartsWith(TEXT("voice.")); })
		                                               .Num(),
		    0);
		Audio->RemoveFromRoot();
		Rig.Close(Session);
	}
	{
		// Both palms from the ritual's floor (10 s): the offer locks the collar on the hold, no voice is waited for.
		FVoiceRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		FArenaSession Session;
		UArenaAudioComponent* Audio = NewObject<UArenaAudioComponent>();
		Audio->AddToRoot();
		Audio->SetRecordOnly(true);
		bPass &= TestTrue(TEXT("into the ritual"), ReachRitual(Rig, Session));
		bPass &= TestTrue(TEXT("the offer counts from the ritual floor, 10 s"), FMath::IsNearlyEqual(Session.GetRitualOfferS(), 10.0));
		RunVoice(Session, *Audio, [&Session]()
		{ return Session.GetPhaseClock() + 1.0e-9 >= Session.GetRitualOfferS(); }, 12.0);
		Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
		double Clock = 0.0;
		RunVoice(Session, *Audio, [&Session, &Clock]()
		{
			Clock = Session.GetStage() == TEXT("ritual") ? Session.GetPhaseClock() : Clock;
			return Session.GetStage() != TEXT("ritual");
		}, 5.0);
		bPass &= TestEqual(TEXT("both palms lock the collar"), Session.GetStage(), FString(TEXT("intro")));
		bPass &= TestTrue(*FString::Printf(TEXT("on the hold after the floor (ritual clock %.3f)"), Clock), Clock >= 10.0 + 0.40 - 2.0 * kVoiceTick && Clock < 12.0);
		bPass &= TestEqual(*FString::Printf(TEXT("RIT-01, then the intro (%s)"), *CueList(Session.GetVoiceCues())), Session.GetVoiceCues().Num(), 2);
		bPass &= TestEqual(TEXT("both skipped"), Audio->GetVoiceSkipped(), Session.GetVoiceCues().Num());
		Audio->RemoveFromRoot();
		Rig.Close(Session);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaVoiceAssetsImported, "MageArena.Voice.AssetsImported",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaVoiceAssetsImported::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FVoiceManifest Manifest;
	if (!LoadManifest(*this, Manifest))
	{
		return false;
	}
	// Every manifest id whose WAV is committed must have an imported asset. With no WAV in the tree (audio deferred,
	// docs/DECISIONS.md 2026-10-24) the check is reported unproven and does not fail.
	bool bPass = true;
	int32 Wavs = 0;
	int32 Assets = 0;
	for (const FVoiceManifestRow& Row : Manifest.Rows)
	{
		const bool bWav = IFileManager::Get().FileExists(*FPaths::Combine(FVoiceManifest::VoiceDir(), Row.File));
		const bool bAsset = FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(VoiceLines::AssetPath(Row.Id)));
		Wavs += bWav ? 1 : 0;
		Assets += bAsset ? 1 : 0;
		if (bWav)
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s has an imported asset (%s)"), *Row.Id, *VoiceLines::AssetPath(Row.Id)), bAsset);
		}
	}
	if (Wavs == 0)
	{
		AddInfo(FString::Printf(TEXT("MageArena.Voice.AssetsImported unproven: no WAVs (0 of %d rows; %d assets)"), Manifest.Rows.Num(), Assets));
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_VOICE assets unproven: no WAVs (0 of %d rows, %d assets imported)"), Manifest.Rows.Num(), Assets);
	}
	return bPass;
}

#endif
