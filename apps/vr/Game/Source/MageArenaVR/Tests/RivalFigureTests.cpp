#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/SimTypes.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"
#include "Session/RivalFigure.h"
#include "Session/SessionPresentation.h"

// T38 (apps/vr/tasks/T38-first-animated-opponent.md): Brennic's figure plays the clip his kernel events pick, and a tree
// without the scratch import draws his greybox body. Both run headless: no rendering and no imported asset.

namespace
{
const TCHAR* MissingRivalFolder = TEXT("/Game/T38NoSuchRivalFigure");

FArenaEvent RivalEvent(int32 Tick, const TCHAR* Kind, int32 ActorId, double Value = 0.0)
{
	FArenaEvent Event;
	Event.Tick = Tick;
	Event.Kind = Kind;
	Event.ActorId = ActorId;
	Event.Value = Value;
	return Event;
}

FString DescribeChanges(const TArray<RivalClips::FChange>& Changes)
{
	TArray<FString> Parts;
	for (const RivalClips::FChange& Change : Changes)
	{
		Parts.Add(FString::Printf(TEXT("%s@%d"), RivalClips::Name(Change.Clip), Change.Tick));
	}
	return FString::Join(Parts, TEXT(" "));
}

struct FRivalSessionRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;
	FString SaveDir;

	bool Open(FAutomationTestBase& Test, FArenaSession& Session, uint32 Seed = 1)
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
		Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
		SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("rival-figure"), FGuid::NewGuid().ToString());
		Session.SetSaveDirectory(SaveDir);
		// The Brennic semifinal with the reference seated script (seed 1 is the T16 replay and the T38 capture's first duel).
		Session.SetScripted(true);
		Session.SetBout(2, true);
		return Test.TestTrue(*FString::Printf(TEXT("the Brennic duel starts (seed %u, scripted)"), Seed), Session.Start(Seed));
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
		if (!SaveDir.IsEmpty())
		{
			IFileManager::Get().DeleteDirectory(*SaveDir, false, true);
		}
	}
};

struct FRivalWorld
{
	UWorld* World = nullptr;

	bool Open(FAutomationTestBase& Test)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RivalFigureTests"));
		if (!Test.TestNotNull(TEXT("a world is made"), World))
		{
			return false;
		}
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		return true;
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

int32 CountSkeletalMeshes(const AActor& Actor)
{
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(&Actor);
	return Meshes.Num();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaRivalFigureClipFollowsEvents, "MageArena.Presentation.RivalFigure.ClipFollowsEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaRivalFigureClipFollowsEvents::RunTest(const FString& Parameters)
{
	// A scripted log: the rival is actor 2, the player actor 1. Cast plays 1.5 s (90 ticks), Hit 0.8 s (48 ticks).
	constexpr int32 Rival = 2;
	constexpr int32 Player = 1;
	RivalClips::FLengths Lengths;
	Lengths.CastS = 1.5;
	Lengths.HitS = 0.8;
	Lengths.StepHz = 60.0;
	const TArray<FArenaEvent> Events = {
	    RivalEvent(10, TEXT("cast"), Player), // the player's cast: nothing
	    RivalEvent(60, TEXT("cast"), Rival), // Cast
	    RivalEvent(70, TEXT("hit"), Rival, 6.0), // a hit that removed 6 HP cuts the Cast: Hit, then Idle at 118
	    RivalEvent(200, TEXT("hit"), Rival, 0.0), // a hit that removed nothing: no Hit
	    RivalEvent(210, TEXT("hit"), Player, 9.0), // the player is hit: nothing
	    RivalEvent(220, TEXT("roll"), Rival), // any other kind: nothing
	    RivalEvent(300, TEXT("cast"), Rival), // Cast
	    RivalEvent(330, TEXT("cast"), Rival), // a cast during the Cast lets it play on: Idle at 390
	    RivalEvent(400, TEXT("hit"), Rival, 3.0), // Hit
	    RivalEvent(420, TEXT("hit"), Rival, 4.0), // a second hit plays the Hit again from its start, then Idle at 468
	    RivalEvent(500, TEXT("cast"), Rival), // Cast
	    RivalEvent(510, TEXT("hit"), Rival, 12.0), // the killing blow: Hit
	    RivalEvent(510, TEXT("down"), Rival), // 0 HP, the duel ends: Death on the same tick
	    RivalEvent(600, TEXT("cast"), Rival), // nothing replaces Death
	    RivalEvent(700, TEXT("hit"), Rival, 5.0),
	};
	const FString Expected = TEXT("Cast@60 Hit@70 Idle@118 Cast@300 Idle@390 Hit@400 Hit@420 Idle@468 Cast@500 Hit@510 Death@510");

	// All at once.
	RivalClips::FState Whole;
	TArray<RivalClips::FChange> WholeChanges;
	RivalClips::Step(Whole, Events, Rival, 2000, Lengths, &WholeChanges);
	bool bPass = TestEqual(TEXT("the clip sequence of the scripted log"), DescribeChanges(WholeChanges), Expected);
	bPass &= TestTrue(TEXT("Death holds to the end"), Whole.Clip == ERivalClip::Death && Whole.StartTick == 510);

	// One kernel tick at a time, as the presentation sees it: the same sequence, and the clip of each moment.
	RivalClips::FState Stepped;
	TArray<RivalClips::FChange> SteppedChanges;
	int32 Cursor = 0;
	TMap<int32, ERivalClip> At;
	for (int32 Tick = 0; Tick <= 2000; ++Tick)
	{
		int32 End = Cursor;
		while (End < Events.Num() && Events[End].Tick <= Tick)
		{
			++End;
		}
		RivalClips::Step(Stepped, TConstArrayView<FArenaEvent>(Events).Slice(Cursor, End - Cursor), Rival, Tick, Lengths, &SteppedChanges);
		Cursor = End;
		At.Add(Tick, Stepped.Clip);
	}
	bPass &= TestEqual(TEXT("tick by tick gives the same sequence"), DescribeChanges(SteppedChanges), Expected);
	const TPair<int32, ERivalClip> Moments[] = {
	    {0, ERivalClip::Idle},
	    {59, ERivalClip::Idle},
	    {60, ERivalClip::Cast},
	    {69, ERivalClip::Cast},
	    {70, ERivalClip::Hit},
	    {117, ERivalClip::Hit},
	    {118, ERivalClip::Idle},
	    {205, ERivalClip::Idle},
	    {215, ERivalClip::Idle},
	    {389, ERivalClip::Cast},
	    {390, ERivalClip::Idle},
	    {419, ERivalClip::Hit},
	    {467, ERivalClip::Hit},
	    {468, ERivalClip::Idle},
	    {509, ERivalClip::Cast},
	    {510, ERivalClip::Death},
	    {2000, ERivalClip::Death},
	};
	for (const TPair<int32, ERivalClip>& Moment : Moments)
	{
		bPass &= TestEqual(*FString::Printf(TEXT("the clip at tick %d"), Moment.Key), FString(RivalClips::Name(At[Moment.Key])), FString(RivalClips::Name(Moment.Value)));
	}

	// The real logs: the Brennic duel with the reference seated script, stepped headless to its end. Seed 1 is the T16
	// replay (Brennic wins it, DF-011); seed 2 is the capture's second duel, where he falls. Whoever wins, Death plays
	// exactly when the kernel has him down, and last.
	for (const uint32 Seed : {1u, 2u})
	{
		FArenaSession Session;
		FRivalSessionRig Rig;
		if (!Rig.Open(*this, Session, Seed))
		{
			Rig.Close(Session);
			return false;
		}
		const FVrRuleset* Rules = Session.GetVrRules();
		const FVrRival* Bound = Rules ? Rules->BoundRival() : nullptr;
		bPass &= TestTrue(TEXT("the semifinal binds Brennic"), Bound && Bound->Id == RivalClips::BrennicRivalId);
		const int32 RivalId = Rules ? Rules->RivalRuntime.ActorId : INDEX_NONE;
		int32 Steps = 0;
		while (Session.GetGames().Phase == TEXT("active") && Steps < 60 * 150)
		{
			Session.Advance(1.0 / 60.0, true);
			++Steps;
		}
		const FGames& Games = Session.GetGames();
		const FActor* RivalActor = SimFindActor(Games.State, RivalId);
		RivalClips::FState Duel;
		TArray<RivalClips::FChange> DuelChanges;
		RivalClips::Step(Duel, Games.State.Events, RivalId, Games.State.Tick, RivalClips::FLengths(), &DuelChanges);
		TSet<ERivalClip> Seen;
		Seen.Add(ERivalClip::Idle);
		for (const RivalClips::FChange& Change : DuelChanges)
		{
			Seen.Add(Change.Clip);
		}
		const bool bDown = RivalActor && RivalActor->bDown;
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_DUEL seed=%u rival=%d phase=%s tick=%d down=%d clips=%d changes=%d: %s"), Seed, RivalId, *Games.Phase, Games.State.Tick,
		    bDown ? 1 : 0, Seen.Num(), DuelChanges.Num(), *DescribeChanges(DuelChanges));
		bPass &= TestTrue(*FString::Printf(TEXT("seed %u: Brennic casts"), Seed), Seen.Contains(ERivalClip::Cast));
		bPass &= TestTrue(*FString::Printf(TEXT("seed %u: Death plays exactly when Brennic is down"), Seed), Seen.Contains(ERivalClip::Death) == bDown);
		if (bDown)
		{
			bPass &= TestTrue(*FString::Printf(TEXT("seed %u: Death is the last clip"), Seed), DuelChanges.Num() > 0 && DuelChanges.Last().Clip == ERivalClip::Death);
		}
		Rig.Close(Session);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaRivalFigureFallsBackToGreybox, "MageArena.Presentation.RivalFigure.FallsBackToGreybox",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaRivalFigureFallsBackToGreybox::RunTest(const FString& Parameters)
{
	FRivalWorld World;
	if (!World.Open(*this))
	{
		World.Close();
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// The component alone: a folder without the import loads nothing and makes no mesh.
	AActor* Holder = World.World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	URivalFigureComponent* Figure = Holder ? NewObject<URivalFigureComponent>(Holder) : nullptr;
	bool bPass = TestNotNull(TEXT("a figure component is made"), Figure);
	if (Figure)
	{
		Figure->RegisterComponent();
		bPass &= TestFalse(TEXT("a missing import does not load"), Figure->Load(MissingRivalFolder, RivalClips::BrennicAsset));
		bPass &= TestFalse(TEXT("nothing is loaded"), Figure->IsLoaded());
		bPass &= TestEqual(TEXT("no skeletal mesh is made"), CountSkeletalMeshes(*Holder), 0);
		bPass &= TestEqual(TEXT("its LOD0 counts no triangle"), Figure->GetLod0Triangles(), 0);
		// A sync without a mesh is a no-op.
		Figure->Sync({}, 2, 0, FVector::ZeroVector, FVector::ForwardVector, false);
		bPass &= TestFalse(TEXT("nothing is shown"), Figure->IsShown());
	}

	// The presentation in the Brennic duel: without the import his greybox body stands.
	ASessionPresentation* Presentation = World.World->SpawnActor<ASessionPresentation>(ASessionPresentation::StaticClass(), FTransform::Identity, Params);
	bPass &= TestNotNull(TEXT("the presentation spawns"), Presentation);
	FArenaSession Session;
	FRivalSessionRig Rig;
	if (!Presentation || !Rig.Open(*this, Session))
	{
		Rig.Close(Session);
		World.Close();
		return false;
	}
	Presentation->SetRivalFigureSource(MissingRivalFolder, RivalClips::BrennicAsset);
	const FVrRuleset* Rules = Session.GetVrRules();
	const int32 RivalId = Rules ? Rules->RivalRuntime.ActorId : INDEX_NONE;
	bPass &= TestTrue(TEXT("Brennic is the bound rival"), Rules && Rules->BoundRival() && Rules->BoundRival()->Id == RivalClips::BrennicRivalId);
	int32 BodyFrames = 0;
	int32 UpFrames = 0;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Session.Advance(1.0 / 60.0, true);
		Presentation->Sync(Session, false);
		const FActor* Rival = SimFindActor(Session.GetGames().State, RivalId);
		if (Rival && !Rival->bDown)
		{
			++UpFrames;
			BodyFrames += Presentation->IsBodyDrawn(RivalId) ? 1 : 0;
		}
	}
	bPass &= TestTrue(TEXT("the duel ran with Brennic up"), UpFrames > 60);
	bPass &= TestEqual(TEXT("his greybox body is drawn on every frame he stands"), BodyFrames, UpFrames);
	bPass &= TestNull(TEXT("no figure is kept"), Presentation->GetRivalFigure());
	bPass &= TestFalse(TEXT("no figure is shown"), Presentation->IsRivalFigureShown());
	bPass &= TestEqual(TEXT("the presentation holds no skeletal mesh"), CountSkeletalMeshes(*Presentation), 0);
	Rig.Close(Session);
	World.Close();
	return bPass;
}

#endif
