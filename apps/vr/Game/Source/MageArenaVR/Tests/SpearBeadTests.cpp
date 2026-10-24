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
#include "Kernel/KernelData.h"
#include "Kernel/SimMath.h"
#include "Kernel/SimTypes.h"
#include "Kernel/VrRules.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"
#include "Session/SessionPresentation.h"

// The bead fix (docs/research/BUDGET-DG4-2026-10.md section 13): a conscript spear's ring draws no perfect beads. Before
// it, RingFor made the 20 beads visible and the spear branch of Sync never placed or hid them, so every spear in flight
// drew 20 white 1 m cubes at world origin (T46 found 500 of the stress frame's 885 drawn parts were these).

namespace
{
constexpr int32 BeadSpearId = 970001;
constexpr int32 BeadEmberId = 970002;
constexpr int32 BeadConscriptActorId = 970101;
constexpr int32 BeadHoundActorId = 970102;

struct FBeadRig
{
	UWorld* World = nullptr;
	ASessionPresentation* Presentation = nullptr;
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;
	FArenaSession Session;
	FString SaveDir;
	bool bBound = false;

	bool Open(FAutomationTestBase& Test)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SpearBeadTests"));
		if (!Test.TestNotNull(TEXT("a world is made"), World))
		{
			return false;
		}
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Presentation = World->SpawnActor<ASessionPresentation>(ASessionPresentation::StaticClass(), FTransform::Identity, Params);
		if (!Test.TestNotNull(TEXT("the presentation spawns"), Presentation))
		{
			return false;
		}
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
		bBound = true;
		Session.SetScripted(true);
		SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("session-save"), FGuid::NewGuid().ToString());
		Session.SetSaveDirectory(SaveDir);
		if (!Test.TestTrue(TEXT("BeginArc"), Session.BeginArc(1)))
		{
			return false;
		}
		Session.SkipTeach();
		Session.Advance(1.0 / 72.0, true);
		return Test.TestNotNull(TEXT("the session has a player"), SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId)) && Test.TestTrue(TEXT("the VR overlay is active"), Session.GetVrRules() && Session.GetVrRules()->bActive);
	}

	void Close()
	{
		if (bBound)
		{
			Session.Unbind();
		}
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
		}
		if (!SaveDir.IsEmpty())
		{
			IFileManager::Get().DeleteDirectory(*SaveDir, false, true);
		}
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World = nullptr;
		}
	}

	// The stress driver's way in (BudgetStressDriver.cpp): the posed parts go straight into the kernel state, then Sync.
	FGames& Games() { return const_cast<FGames&>(Session.GetGames()); }

	const FActor& Player() { return *SimFindActor(Games().State, Games().PlayerId); }

	void AddEnemy(int32 Id, const TCHAR* EnemyId, const FSimVec& At)
	{
		FActor& Enemy = Games().State.Actors.AddDefaulted_GetRef();
		Enemy.Id = Id;
		Enemy.Team = 1;
		Enemy.Label = EnemyId;
		Enemy.Pos = At;
		Enemy.PreviousPos = At;
		Enemy.Radius = 0.4;
		Enemy.MaxHp = 100.0;
		Enemy.Hp = 100.0;
		FEnemyBrain Brain;
		Brain.Id = EnemyId;
		Enemy.Enemy = Brain;
	}

	// A threat flying straight at the player, Seconds from contact (SessionPresentation.cpp SecondsToContact).
	void AddThreat(int32 Id, int32 OwnerId, const TCHAR* Family, double Seconds)
	{
		const FActor& Seat = Player();
		const double Speed = 8.0;
		const double Radius = KernelData().ProjectileRadiusM;
		const FSimVec Toward{1.0, 0.0};
		FProjectile& Shot = Games().State.Projectiles.AddDefaulted_GetRef();
		Shot.Id = Id;
		Shot.ActivationId = Id;
		Shot.OwnerId = OwnerId;
		Shot.Family = Family;
		Shot.Radius = Radius;
		Shot.bHasAim = true;
		Shot.Pos = SimSub(Seat.Pos, SimScale(Toward, Seat.Radius + Radius + Speed * Seconds));
		Shot.PreviousPos = Shot.Pos;
		Shot.OriginPos = SimSub(Shot.Pos, SimScale(Toward, 4.0));
		Shot.AimedAt = Seat.Pos;
		Shot.Velocity = SimScale(Toward, Speed);
		Shot.RemainingM = SimDistance(Shot.Pos, Shot.AimedAt);
	}

	FProjectile* Find(int32 Id)
	{
		return Games().State.Projectiles.FindByPredicate([Id](const FProjectile& Each)
		{ return Each.Id == Id; });
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresentationSpearNoBeads, "MageArena.Presentation.Beads.SpearRingHasNone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresentationSpearNoBeads::RunTest(const FString& Parameters)
{
	FBeadRig Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	const FSimVec Seat = Rig.Player().Pos;
	Rig.AddEnemy(BeadConscriptActorId, TEXT("conscript"), SimAdd(Seat, FSimVec{-8.0, 0.0}));
	// Inside the perfect window, where a magic threat would show its beads: a spear still shows none.
	Rig.AddThreat(BeadSpearId, BeadConscriptActorId, TEXT("physical"), 0.5 * KernelData().Absorb.WindowS);
	bool bPass = true;
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		Rig.Presentation->Sync(Rig.Session, false);
		bPass &= TestTrue(TEXT("the spear is drawn"), Rig.Presentation->IsShotDrawn(BeadSpearId));
		const int32 Beads = Rig.Presentation->CountRingBeadsDrawn(BeadSpearId);
		bPass &= TestEqual(TEXT("the spear holds a ground ring"), Beads >= 0, true);
		bPass &= TestEqual(*FString::Printf(TEXT("frame %d: no bead of the spear ring is visible"), Frame), Beads, 0);
	}
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaPresentationReusedRingNoBeads, "MageArena.Presentation.Beads.RingReusedFromMagicHasNone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaPresentationReusedRingNoBeads::RunTest(const FString& Parameters)
{
	FBeadRig Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	const FSimVec Seat = Rig.Player().Pos;
	Rig.AddEnemy(BeadHoundActorId, TEXT("cinder_hound"), SimAdd(Seat, FSimVec{-6.0, 0.0}));
	Rig.AddEnemy(BeadConscriptActorId, TEXT("conscript"), SimAdd(Seat, FSimVec{-8.0, 1.0}));
	bool bPass = TestNotNull(TEXT("the hound spits (VR overlay)"), Rig.Session.GetVrRules()->FindSpit(TEXT("cinder_hound")));

	// A hound ember inside the perfect window: its ring shows the beads (the magic cue this fix must leave alone).
	Rig.AddThreat(BeadEmberId, BeadHoundActorId, TEXT("magic"), 0.5 * KernelData().Absorb.WindowS);
	Rig.Presentation->Sync(Rig.Session, false);
	const int32 EmberBeads = Rig.Presentation->CountRingBeadsDrawn(BeadEmberId);
	bPass &= TestTrue(*FString::Printf(TEXT("the ember in the perfect window shows its beads (%d)"), EmberBeads), EmberBeads > 0);

	// The same ring carried into a spear: the projectile keeps its id, so RingFor hands the spear the ember's ring with its
	// beads still lit. The spear branch must hide them.
	if (FProjectile* Shot = Rig.Find(BeadEmberId))
	{
		Shot->OwnerId = BeadConscriptActorId;
		Shot->Family = TEXT("physical");
	}
	Rig.Presentation->Sync(Rig.Session, false);
	bPass &= TestEqual(TEXT("the ring carried from the ember into a spear shows no bead"), Rig.Presentation->CountRingBeadsDrawn(BeadEmberId), 0);

	// Free-slot reuse: the threat ends, its ring is freed, and a new spear takes the free ring.
	Rig.Games().State.Projectiles.RemoveAll([](const FProjectile& Each)
	{ return Each.Id == BeadEmberId; });
	Rig.AddThreat(BeadEmberId + 10, BeadHoundActorId, TEXT("magic"), 0.5 * KernelData().Absorb.WindowS);
	Rig.Presentation->Sync(Rig.Session, false);
	bPass &= TestTrue(TEXT("a second ember shows its beads"), Rig.Presentation->CountRingBeadsDrawn(BeadEmberId + 10) > 0);
	Rig.Games().State.Projectiles.RemoveAll([](const FProjectile& Each)
	{ return Each.Id == BeadEmberId + 10; });
	Rig.Presentation->Sync(Rig.Session, false);
	Rig.AddThreat(BeadSpearId, BeadConscriptActorId, TEXT("physical"), 0.5 * KernelData().Absorb.WindowS);
	Rig.Presentation->Sync(Rig.Session, false);
	bPass &= TestEqual(TEXT("a spear on a ring freed by an ember shows no bead"), Rig.Presentation->CountRingBeadsDrawn(BeadSpearId), 0);
	Rig.Close();
	return bPass;
}

#endif
