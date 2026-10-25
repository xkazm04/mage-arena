#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"
#include "Session/CollarLedger.h"
#include "Session/SessionPresentation.h"

// T22 seam, Athena's ruling (2026-10-25): a crack seam renders only on a wrist that shows a cuff, on that cuff, never
// free-floating. The seated build has one cuff, on the casting wrist; the seam on the other wrist had no cuff under it and
// showed at death as a stray white line.

namespace
{
// The seam's place on the cuff (SessionPresentation EnsureCollarVisuals): 2 cm nearer the eye, 3.5 cm up, in cuff space.
const FVector SeamOnCuffCm(-2.0, 0.0, 3.5);
constexpr double SeamToleranceCm = 1.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCrackSeamOnCuff, "MageArena.Presentation.CrackSeam.OnCuff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCrackSeamOnCuff::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CrackSeamCuffTests"));
	if (!TestNotNull(TEXT("a world is made"), World))
	{
		return false;
	}
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// The cuff hangs on the pawn's camera: without one there is no cuff to test.
	AMageArenaPawn* Pawn = World->SpawnActor<AMageArenaPawn>(AMageArenaPawn::StaticClass(), FTransform::Identity, Params);
	bool bPass = TestNotNull(TEXT("the pawn spawns"), Pawn) && TestNotNull(TEXT("with a camera"), Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr);
	ASessionPresentation* Presentation = World->SpawnActor<ASessionPresentation>(ASessionPresentation::StaticClass(), FTransform::Identity, Params);
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	UHandInputSubsystem* Hands = NewObject<UHandInputSubsystem>(Instance);
	USigilRecognizerSubsystem* Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
	UWardDetectorSubsystem* Wards = NewObject<UWardDetectorSubsystem>(Instance);
	UBlinkDetectorSubsystem* Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
	UStaffDetectorSubsystem* Staff = NewObject<UStaffDetectorSubsystem>(Instance);
	FString Error;
	bPass = bPass && TestNotNull(TEXT("the presentation spawns"), Presentation) && TestTrue(TEXT("the ward detector starts"), Wards->InitDetector(Error));
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
		// The crack window: a fight phase and a Crack stamped now (the seam shows for CrackSeamS after it).
		const_cast<FGames&>(Session.GetGames()).Phase = TEXT("active");
		const_cast<FCollarLedger&>(Session.GetCollar()).Add(ECollarSource::Perfect, Session.GetSimSeconds());
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Presentation->Sync(Session, false);
			const USceneComponent* CuffRoot = Presentation->GetCuffRoot();
			const UStaticMeshComponent* CuffPlane = Presentation->GetCuffPlane();
			const bool bCuffShown = CuffRoot && CuffPlane && CuffPlane->IsVisible();
			bPass &= TestTrue(*FString::Printf(TEXT("frame %d: the wrist cuff is shown"), Frame), bCuffShown);
			int32 Visible = 0;
			for (int32 Index = 0; Index < Presentation->GetCrackSeamCount(); ++Index)
			{
				const UStaticMeshComponent* Seam = Presentation->GetCrackSeam(Index);
				if (!Seam || !Seam->IsVisible())
				{
					continue;
				}
				++Visible;
				if (!bCuffShown)
				{
					bPass &= TestTrue(*FString::Printf(TEXT("frame %d: seam %d is visible with no cuff shown on its wrist"), Frame, Index), false);
					continue;
				}
				const FVector Expected = CuffRoot->GetComponentTransform().TransformPosition(SeamOnCuffCm);
				const double Away = FVector::Dist(Seam->GetComponentLocation(), Expected);
				const FString OnCuff = FString::Printf(TEXT("frame %d: seam %d sits on the cuff (%.1f cm away, limit %.1f)"), Frame, Index, Away, SeamToleranceCm);
				bPass &= TestTrue(*OnCuff, Away <= SeamToleranceCm);
			}
			bPass &= TestEqual(*FString::Printf(TEXT("frame %d: exactly one seam shows, over the one cuff"), Frame), Visible, 1);
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

#endif
