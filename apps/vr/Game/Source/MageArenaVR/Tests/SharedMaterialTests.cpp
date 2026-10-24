#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/HandInputSubsystem.h"
#include "MageArenaVR.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"
#include "Session/SessionPresentation.h"

// T37, D-G4 lever B: the presentation's parts of one colour share one material, so dynamic instancing can merge them, and
// a runtime recolour swaps the part to the material of its new colour instead of writing into the shared one
// (docs/research/BUDGET-DG4-2026-10.md 7.7 and 9, section 11).

namespace
{
// The most shared instances a whole day may make, from the quantised fades (T37): the fixed colours (18 opaque and 3
// curtain on the FullSeated script at T37, runs/T37/REPORT.md, rounded up to 24), 3 comfort stones x 17 fill steps, the
// crowd band's 17 swell steps, and the Wardstone body and crown at 9 glow x 9 pulse steps each: 24 + 51 + 17 + 162 = 254.
// A material made per frame (thousands within a bout) fails it; a material made per part fails the check that every
// part's material is a shared one.
constexpr int32 SharedDistinctCap = 256;

const FLinearColor PartSteel(0.68f, 0.68f, 0.68f);
const FLinearColor PartEmber(0.90f, 0.26f, 0.02f);

struct FPresentationWorld
{
	UWorld* World = nullptr;
	ASessionPresentation* Presentation = nullptr;

	bool Open(FAutomationTestBase& Test)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SharedMaterialTests"));
		if (!Test.TestNotNull(TEXT("a world is made"), World))
		{
			return false;
		}
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Presentation = World->SpawnActor<ASessionPresentation>(ASessionPresentation::StaticClass(), FTransform::Identity, Params);
		return Test.TestNotNull(TEXT("the presentation spawns"), Presentation);
	}

	void Close()
	{
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World = nullptr;
		}
	}
};

bool PartColourOf(const UStaticMeshComponent* Part, FLinearColor& Out)
{
	const UMaterialInstanceDynamic* Material = Part ? Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)) : nullptr;
	return Material && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Out);
}

struct FMaterialSessionRig
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
			Instance->MarkAsGarbage();
		}
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSharedMaterialsSameColour, "MageArena.Presentation.SharedMaterials.SameColourSameMaterial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSharedMaterialsSameColour::RunTest(const FString& Parameters)
{
	FPresentationWorld Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	UStaticMeshComponent* First = Rig.Presentation->MakePart(TEXT("Cube"), PartSteel);
	UStaticMeshComponent* Second = Rig.Presentation->MakePart(TEXT("Cube"), PartSteel);
	UStaticMeshComponent* Sphere = Rig.Presentation->MakePart(TEXT("Sphere"), PartSteel);
	UStaticMeshComponent* Other = Rig.Presentation->MakePart(TEXT("Cube"), PartEmber);
	bool bPass = TestTrue(TEXT("four parts are made"), First && Second && Sphere && Other);
	if (bPass)
	{
		bPass &= TestNotNull(TEXT("a part has a material"), First->GetMaterial(0));
		bPass &= TestTrue(TEXT("two parts of one colour hold the same material"), First->GetMaterial(0) == Second->GetMaterial(0));
		bPass &= TestTrue(TEXT("the colour decides the material, not the shape"), First->GetMaterial(0) == Sphere->GetMaterial(0));
		bPass &= TestTrue(TEXT("two colours hold two materials"), First->GetMaterial(0) != Other->GetMaterial(0));
		bPass &= TestEqual(TEXT("two colours in the cache"), Rig.Presentation->GetSharedTintCount(), 2);
	}
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSharedMaterialsRecolour, "MageArena.Presentation.SharedMaterials.RecolourDoesNotLeak",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSharedMaterialsRecolour::RunTest(const FString& Parameters)
{
	FPresentationWorld Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	UStaticMeshComponent* A = Rig.Presentation->MakePart(TEXT("Cube"), PartSteel);
	UStaticMeshComponent* B = Rig.Presentation->MakePart(TEXT("Cube"), PartSteel);
	bool bPass = TestTrue(TEXT("two parts are made"), A && B);
	if (bPass)
	{
		Rig.Presentation->Recolour(A, PartEmber);
		FLinearColor ColourA;
		FLinearColor ColourB;
		bPass &= TestTrue(TEXT("both parts carry a colour parameter"), PartColourOf(A, ColourA) && PartColourOf(B, ColourB));
		bPass &= TestTrue(*FString::Printf(TEXT("part A shows its new colour (got %s)"), *ColourA.ToString()), ColourA.Equals(PartEmber));
		bPass &= TestTrue(*FString::Printf(TEXT("part B keeps its colour after A is recoloured (got %s)"), *ColourB.ToString()), ColourB.Equals(PartSteel));
		bPass &= TestTrue(TEXT("the recoloured part left the shared material"), A->GetMaterial(0) != B->GetMaterial(0));
		// Back to the start colour: the part rejoins the shared material instead of keeping one of its own.
		Rig.Presentation->Recolour(A, PartSteel);
		bPass &= TestTrue(TEXT("recoloured back, A shares B's material again"), A->GetMaterial(0) == B->GetMaterial(0));
	}
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSharedMaterialsBounded, "MageArena.Presentation.SharedMaterials.Bounded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSharedMaterialsBounded::RunTest(const FString& Parameters)
{
	// The FullSeated script (MageArenaDesign.Session.FullSeated) played with the presentation synced on every step: the
	// ritual, the bouts it reaches with their intros and intermissions, and the aftermath. Like FullSeated, a bout the
	// script loses before the final ends the run (DF-011 tracks bout 3); the bound is about colours, not the outcome.
	const double Dt = 1.0 / 72.0;
	FPresentationWorld World;
	FMaterialSessionRig Rig;
	if (!World.Open(*this) || !Rig.Open(*this))
	{
		World.Close();
		return false;
	}
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
	Session.SetScripted(true);
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("session-save"), FGuid::NewGuid().ToString());
	Session.SetSaveDirectory(Dir);
	if (!TestTrue(TEXT("BeginArc"), Session.BeginArc(1)))
	{
		Rig.Close(Session);
		World.Close();
		return false;
	}
	Session.SkipTeach();
	int32 Guard = 0;
	int32 Peak = 0;
	FString LastStage;
	while (Session.GetStage() != TEXT("closed") && Guard < 72 * 1800)
	{
		const FString Stage = Session.GetStage();
		if (Stage != LastStage && Stage == TEXT("intermission"))
		{
			Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
		}
		LastStage = Stage;
		if (Stage == TEXT("lost") && Session.GetGames().Wave < 3)
		{
			break;
		}
		Session.Advance(Dt, true);
		World.Presentation->Sync(Session, false);
		Peak = FMath::Max(Peak, World.Presentation->CountDistinctMaterials());
		++Guard;
	}
	const int32 Distinct = World.Presentation->CountDistinctMaterials();
	const int32 Shared = World.Presentation->GetSharedTintCount();
	TInlineComponentArray<UStaticMeshComponent*> Parts(World.Presentation);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_PRESENTATION_MATERIALS distinct=%d peak=%d shared=%d parts=%d cap=%d stage=%s bout=%d steps=%d"),
	    Distinct, Peak, Shared, Parts.Num(), SharedDistinctCap, *Session.GetStage(), Session.GetGames().Wave + 1, Guard);
	bool bPass = TestTrue(TEXT("the script played past the ritual"), Guard > 72 * 60);
	bPass &= TestTrue(*FString::Printf(TEXT("shared instances %d under the cap %d"), Shared, SharedDistinctCap), Shared <= SharedDistinctCap);
	// The cuff and the glyph plane keep their own pass-through instances (texture parameters); every other part is shared.
	bPass &= TestTrue(*FString::Printf(TEXT("every part shows a shared material: peak %d distinct, %d shared + cuff + glyph"), Peak, Shared),
	    Peak <= Shared + 2);
	bPass &= TestTrue(*FString::Printf(TEXT("fewer materials (%d) than parts (%d)"), Distinct, Parts.Num()), Distinct < Parts.Num());
	Rig.Close(Session);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	World.Close();
	return bPass;
}

#endif
