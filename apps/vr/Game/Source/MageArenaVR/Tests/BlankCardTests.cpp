#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/HandInputSubsystem.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"
#include "Session/SessionPresentation.h"

// T64: the style-pick death frame showed a black plate across the upper view and two white bars. They were text cards
// drawn the frame they were first shown: a widget component draws only while visible, so that frame has no text yet.
// No card may be drawn before its text is.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresentationNoBlankCard, "MageArena.Presentation.Cards.NoBlankCard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresentationNoBlankCard::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BlankCardTests"));
	if (!TestNotNull(TEXT("a world is made"), World))
	{
		return false;
	}
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASessionPresentation* Presentation = World->SpawnActor<ASessionPresentation>(ASessionPresentation::StaticClass(), FTransform::Identity, Params);
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	UHandInputSubsystem* Hands = NewObject<UHandInputSubsystem>(Instance);
	USigilRecognizerSubsystem* Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
	UWardDetectorSubsystem* Wards = NewObject<UWardDetectorSubsystem>(Instance);
	UBlinkDetectorSubsystem* Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
	UStaffDetectorSubsystem* Staff = NewObject<UStaffDetectorSubsystem>(Instance);
	FString Error;
	bool bPass = TestNotNull(TEXT("the presentation spawns"), Presentation) && TestTrue(TEXT("the ward detector starts"), Wards->InitDetector(Error));
	FArenaSession Session;
	const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("session-save"), FGuid::NewGuid().ToString());
	if (bPass)
	{
		Sigils->LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
		Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
		Session.SetScripted(false);
		Session.SetSaveDirectory(SaveDir);
		bPass = TestTrue(TEXT("BeginArc"), Session.BeginArc(1));
	}
	if (bPass)
	{
		Session.Advance(1.0 / 72.0, true);
		const FString Prompt = Session.GetPromptText();
		bPass &= TestFalse(TEXT("the day opens on a prompt"), Prompt.IsEmpty());
		// The frame the prompt first shows, and the ones after it: no card is drawn before its text.
		for (int32 Frame = 0; Frame < 4; ++Frame)
		{
			Presentation->Sync(Session, false);
			bPass &= TestEqual(*FString::Printf(TEXT("frame %d: no text card is drawn blank"), Frame), Presentation->CountBlankCards(), 0);
		}
	}
	Session.Unbind();
	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();
	IFileManager::Get().DeleteDirectory(*SaveDir, false, true);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresentationCardReveal, "MageArena.Presentation.Cards.RevealAfterPrime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresentationCardReveal::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FCardReveal Card;
	bool bPass = TestFalse(TEXT("a card nobody wants is not shown"), Card.Step(false));
	for (int32 Sync = 0; Sync < FCardReveal::PrimeSyncs; ++Sync)
	{
		bPass &= TestFalse(*FString::Printf(TEXT("sync %d of a new card is still priming"), Sync), Card.Step(true));
	}
	bPass &= TestTrue(TEXT("the card shows once it has primed"), Card.Step(true));
	bPass &= TestTrue(TEXT("and stays"), Card.Step(true));
	bPass &= TestFalse(TEXT("a card taken away is gone"), Card.Step(false));
	bPass &= TestFalse(TEXT("and primes again when it returns"), Card.Step(true));
	return bPass;
}

#endif
