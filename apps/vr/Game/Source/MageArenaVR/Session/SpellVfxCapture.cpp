#include "Session/SpellVfxCapture.h"

#include "Session/ArenaSession.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimMath.h"
#include "Kernel/VrRules.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Hands/MageArenaPawn.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

// Staging, stated so the stills are read for what they are:
// - The Brennic semifinal (SetBout(2, true)) is started and frozen at once. Every other team-1 actor is laid down, and
//   one posed mage of the scene's school stands 8 m ahead of the seat: the duel ring.
// - A bolt still: one magic projectile of that mage 3.5 m out, flying at the seat from 1.2 m off its line, at 14 m/s.
// - An absorb or perfect still: the bolt comes in 0.35 m right of the seat line, posed 1.1 m out for 0.6 s, then removed, and a "hit" on the seat
//   follows from its owner, with the seat's Blocks (absorb) or Perfects and a "perfect" event (perfect) counted, as
//   ResolveHit does. The still is ImpactS (0.12 s) and at least ImpactFrames frames later.
// - The unblockable: an area telegraph of the Fire mage 5 m out, half way through its windup.
// - The steel stone: a physical projectile of the Fire mage, posed as the bolt is.
// - T73, only under -MageArenaSpellVfxWall (DF-014 evidence): the Fire bolt again, in front of a fire wall of the Fire
//   mage posed as the D-G4 stress driver poses one (the overlay's distance and arc, facing the seat). The wall keeps the
//   live bout's colour, ActiveElementPalette().Fire; nothing else in the run changes.
namespace
{
constexpr int32 PosedMageIdBase = 930000;
constexpr int32 PosedShotId = 931000;
constexpr int32 PosedTelegraphId = 932000;
constexpr int32 PosedWallId = 933000;
constexpr int32 SettleFrames = 8;
// Wall seconds a pose holds before its still: longer than the presentation's 0.28 s ghost of the previous scene's shot.
constexpr double SettleS = 0.6;
constexpr int32 ImpactFrames = 3;
// Wall seconds from the ward event to the still: the splash and the ring are about a quarter grown.
constexpr double ImpactS = 0.12;

enum class EPose : uint8
{
	Bolt,
	Absorb,
	Perfect,
	Telegraph,
	Steel
};

struct FStagedShot
{
	const TCHAR* Name;
	// 0 Water, 1 Fire, 2 Air.
	int32 School;
	EPose Pose;
	// T73: a posed fire wall of the scene's mage stands behind the bolt.
	bool bWall = false;
};

const FStagedShot Scenes[] = {
    {TEXT("01-water-bolt.png"), 0, EPose::Bolt},
    {TEXT("02-water-absorb.png"), 0, EPose::Absorb},
    {TEXT("03-water-perfect.png"), 0, EPose::Perfect},
    {TEXT("04-fire-bolt.png"), 1, EPose::Bolt},
    {TEXT("05-fire-absorb.png"), 1, EPose::Absorb},
    {TEXT("06-fire-perfect.png"), 1, EPose::Perfect},
    {TEXT("07-air-bolt.png"), 2, EPose::Bolt},
    {TEXT("08-air-absorb.png"), 2, EPose::Absorb},
    {TEXT("09-air-perfect.png"), 2, EPose::Perfect},
    {TEXT("10-unblockable-telegraph.png"), 1, EPose::Telegraph},
    {TEXT("11-steel-stone.png"), 1, EPose::Steel},
    {TEXT("12-fire-bolt-wall.png"), 1, EPose::Bolt, true},
};

// The eleven stills, and the twelfth (the wall) only under -MageArenaSpellVfxWall, so a default run is the T58 set.
int32 SceneCount()
{
	static const int32 Count = FParse::Param(FCommandLine::Get(), TEXT("MageArenaSpellVfxWall")) ? UE_ARRAY_COUNT(Scenes) : UE_ARRAY_COUNT(Scenes) - 1;
	return Count;
}

AMageArenaPawn* FindPawn(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AMageArenaPawn> It(World);
	return It ? *It : nullptr;
}

FSimVec Along(const FSimVec& From, const FSimVec& Unit, double Metres, double SideM)
{
	const FSimVec Side{-Unit.Y, Unit.X};
	return SimAdd(SimAdd(From, SimScale(Unit, Metres)), SimScale(Side, SideM));
}
}

USpellVfxCaptureDriver::USpellVfxCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void USpellVfxCaptureDriver::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	bRunning = true;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_T58 start run=%s scenes=%d"), *RunId, SceneCount());
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
	Evidence.Begin(TEXT("SpellVfx"));
	if (!MageArenaCapture::CanCapture())
	{
		Fail(TEXT("no rhi"));
	}
}

void USpellVfxCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	Frames = 0;
}

void USpellVfxCaptureDriver::Fail(const FString& Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), *Reason);
	Enter(EStep::Quit);
}

UArenaSessionSubsystem* USpellVfxCaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

void USpellVfxCaptureDriver::PoseScene(int32 Index)
{
	UArenaSessionSubsystem* Session = SessionSys();
	if (!Session)
	{
		return;
	}
	FArenaSession& Bout = Session->GetSession();
	// The bout is frozen (SetFrameHold), so the kernel never steps again and the capture owns its state.
	FGames& Games = const_cast<FGames&>(Bout.GetGames());
	FArenaState& State = Games.State;
	FActor* Player = SimFindActor(State, Games.PlayerId);
	if (!Player)
	{
		Fail(TEXT("no player"));
		return;
	}
	const FStagedShot& Staged = Scenes[Index];
	const FSimVec Seat = Player->Pos;
	const FSimVec Ahead = SimUnit(Player->Facing, FSimVec{1.0, 0.0});
	if (!SimFindActor(State, PosedMageIdBase))
	{
		for (int32 School = 0; School < 3; ++School)
		{
			FActor& Mage = State.Actors.AddDefaulted_GetRef();
			Mage.Id = PosedMageIdBase + School;
			Mage.Team = 1;
			Mage.Label = School == 0 ? TEXT("Water mage") : (School == 1 ? TEXT("Fire mage") : TEXT("Air mage"));
			Mage.Fire.bSchool = School == 1;
			Mage.Air.bSchool = School == 2;
			Mage.Radius = 0.4;
			Mage.MaxHp = 100.0;
			Mage.Hp = 100.0;
		}
	}
	for (FActor& Actor : State.Actors)
	{
		if (Actor.Id == Games.PlayerId)
		{
			continue;
		}
		const bool bOwner = Actor.Id == PosedMageIdBase + Staged.School;
		Actor.bDown = !bOwner;
		if (bOwner)
		{
			Actor.Pos = Along(Seat, Ahead, 8.0, 0.0);
			Actor.PreviousPos = Actor.Pos;
			Actor.Facing = SimScale(Ahead, -1.0);
		}
	}
	State.Projectiles.Reset();
	State.Telegraphs.Reset();
	const int32 Owner = PosedMageIdBase + Staged.School;
	if (Games.VrRules.IsSet())
	{
		FVrRuleset& Rules = Games.VrRules.GetValue();
		Rules.Walls.RemoveAll([](const FVrWall& Wall)
		{ return Wall.Id == PosedWallId; });
		if (Staged.bWall)
		{
			if (!Rules.FireWall.bEnabled)
			{
				Fail(TEXT("the overlay has no fire wall to pose"));
				return;
			}
			const FSimVec MageAt = Along(Seat, Ahead, 8.0, 0.0);
			FVrWall& Wall = Rules.Walls.AddDefaulted_GetRef();
			Wall.Id = PosedWallId;
			Wall.OwnerId = Owner;
			Wall.Centre = MageAt;
			Wall.Facing = SimUnit(SimSub(Seat, MageAt));
			Wall.DistanceM = Rules.FireWall.DistanceM;
			Wall.ArcDeg = Rules.FireWall.ArcDeg;
			Wall.UntilTick = TNumericLimits<int32>::Max();
		}
	}
	else if (Staged.bWall)
	{
		Fail(TEXT("no VR overlay: no fire wall to pose"));
		return;
	}
	if (Staged.Pose == EPose::Telegraph)
	{
		FTelegraph& Telegraph = State.Telegraphs.AddDefaulted_GetRef();
		Telegraph.Id = PosedTelegraphId;
		Telegraph.ActivationId = PosedTelegraphId;
		Telegraph.OwnerId = Owner;
		Telegraph.Family = TEXT("unblockable");
		Telegraph.Kind = TEXT("area");
		Telegraph.Origin = Along(Seat, Ahead, 5.0, 0.0);
		Telegraph.Target = Telegraph.Origin;
		Telegraph.StartTick = State.Tick - 30;
		Telegraph.ResolveTick = State.Tick + 30;
	}
	else
	{
		const bool bImpact = Staged.Pose == EPose::Absorb || Staged.Pose == EPose::Perfect;
		const FSimVec From = Along(Seat, Ahead, 8.0, 0.0);
		// A bolt in flight crosses from 1.2 m off the seat's line; an impact comes in just right of it, in view.
		const FSimVec To = Along(Seat, Ahead, 0.0, bImpact ? 0.35 : 1.2);
		const FSimVec Lane = SimUnit(SimSub(To, From));
		FProjectile& Shot = State.Projectiles.AddDefaulted_GetRef();
		Shot.Id = PosedShotId + Index;
		Shot.ActivationId = Shot.Id;
		Shot.OwnerId = Owner;
		Shot.Family = Staged.Pose == EPose::Steel ? TEXT("physical") : TEXT("magic");
		Shot.bBolt = Staged.Pose != EPose::Steel;
		Shot.Radius = KernelData().ProjectileRadiusM;
		Shot.Source = From;
		Shot.OriginPos = From;
		Shot.AimedAt = To;
		// 3.5 m out in flight; 1.1 m out (at the ward) for an impact.
		const double Out = bImpact ? 1.1 : 3.5;
		const double Travelled = FMath::Max(SimDistance(From, To) - Out, 0.0);
		Shot.Pos = SimAdd(From, SimScale(Lane, Travelled));
		Shot.Velocity = SimScale(Lane, 14.0);
		Shot.PreviousPos = SimSub(Shot.Pos, SimScale(Shot.Velocity, SimDt()));
	}
	if (AMageArenaPawn* Pawn = FindPawn(GetWorld()))
	{
		if (AController* Controller = Pawn->GetController())
		{
			const FVector Delta = Bout.KernelToUnrealCm(Along(Seat, Ahead, 8.0, 0.0)) - Bout.KernelToUnrealCm(Seat);
			const float Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
			Controller->SetControlRotation(Pawn->ClampLookRotation(FRotator(-6.f, Yaw, 0.f)));
		}
	}
}

void USpellVfxCaptureDriver::ImpactScene(int32 Index)
{
	UArenaSessionSubsystem* Session = SessionSys();
	if (!Session)
	{
		return;
	}
	FGames& Games = const_cast<FGames&>(Session->GetSession().GetGames());
	FArenaState& State = Games.State;
	FActor* Player = SimFindActor(State, Games.PlayerId);
	if (!Player)
	{
		Fail(TEXT("no player"));
		return;
	}
	const FStagedShot& Staged = Scenes[Index];
	const int32 Owner = PosedMageIdBase + Staged.School;
	State.Projectiles.Reset();
	// ResolveHit's order: a perfect counts and emits "perfect" first, a plain absorb counts a block, then the "hit".
	if (Staged.Pose == EPose::Perfect)
	{
		Player->Metrics.Perfects++;
		Emit(State, TEXT("perfect"), *Player, 0.0, Owner);
	}
	else
	{
		Player->Metrics.Blocks++;
	}
	Emit(State, TEXT("hit"), *Player, 0.0, Owner);
}

void USpellVfxCaptureDriver::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (!bRunning)
	{
		return;
	}
	if (bShotOpen)
	{
		const ECaptureShotPoll Shot = Evidence.Poll();
		if (Shot == ECaptureShotPoll::Pending)
		{
			return;
		}
		bShotOpen = false;
		if (Shot == ECaptureShotPoll::Failed)
		{
			Fail(Evidence.GetFailure());
			return;
		}
		++Scene;
		Enter(Scene < SceneCount() ? EStep::Pose : EStep::Finish);
	}
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	UArenaSessionSubsystem* Session = SessionSys();
	FArenaSession* Bout = Session ? &Session->GetSession() : nullptr;
	++Frames;
	switch (Step)
	{
	case EStep::WaitStable:
	{
		AMageArenaPawn* Pawn = FindPawn(GetWorld());
		const bool bShadersIdle = GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling();
		if ((Pawn && Pawn->HasLayout() && bShadersIdle && Elapsed >= 2.0) || Elapsed >= 90.0)
		{
			if (!Session || !Pawn || !Bout)
			{
				Fail(TEXT("session or pawn missing"));
				break;
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			if (GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("r.MotionBlurQuality 0"));
			}
			Bout->SetSaveDirectory(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("t58-capture")));
			Enter(EStep::StartBout);
		}
		break;
	}
	case EStep::StartBout:
		Bout->SetBout(2, true);
		if (!Session->Start(1, true))
		{
			Fail(TEXT("the semifinal did not start"));
			break;
		}
		Bout->SetFrameHold(true);
		Scene = 0;
		Enter(EStep::Pose);
		break;
	case EStep::Pose:
		if (Frames == 1)
		{
			PoseScene(Scene);
		}
		if (Frames >= SettleFrames && Elapsed >= SettleS)
		{
			if (GShaderCompilingManager)
			{
				GShaderCompilingManager->FinishAllCompilation();
			}
			const EPose Pose = Scenes[Scene].Pose;
			Enter(Pose == EPose::Absorb || Pose == EPose::Perfect ? EStep::Impact : EStep::Settle);
		}
		break;
	case EStep::Impact:
		if (Frames == 1)
		{
			ImpactScene(Scene);
		}
		if (Frames > ImpactFrames && Elapsed >= ImpactS)
		{
			Enter(EStep::Settle);
		}
		break;
	case EStep::Settle:
	{
		const FString Path = MageArenaCapture::RepoPath(FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, Scenes[Scene].Name));
		Evidence.Requested(Scenes[Scene].Name, Path, Session);
		FScreenshotRequest::RequestScreenshot(Path, false, false, false);
		bShotOpen = true;
		++ShotCount;
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s path=%s"), Scenes[Scene].Name, *Path);
		break;
	}
	case EStep::Finish:
	{
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		FString ManifestError;
		if (ShotCount == SceneCount() && Evidence.WriteManifest(MageArenaCapture::RepoPath(FString::Printf(TEXT("runs/%s/shots/manifest.json"), *RunId)), ManifestError))
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE_DONE shots=%d"), ShotCount);
		}
		else
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL shots=%d %s"), ShotCount, *ManifestError);
		}
		Enter(EStep::Quit);
		break;
	}
	case EStep::Quit:
		bRunning = false;
		SetTickableTickType(ETickableTickType::Never);
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* Controller = World->GetFirstPlayerController())
			{
				Controller->ConsoleCommand(TEXT("Quit"));
			}
		}
		break;
	default:
		break;
	}
	if (Step != EStep::Quit && Elapsed > 120.0)
	{
		Fail(FString::Printf(TEXT("step %d of scene %d stalled"), static_cast<int32>(Step), Scene));
	}
}

TStatId USpellVfxCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USpellVfxCaptureDriver, STATGROUP_Tickables);
}

bool USpellVfxCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* USpellVfxCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void USpellVfxCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
