#include "Session/PresetCapture.h"

#include "Session/ArenaSession.h"
#include "Kernel/ArenaKernel.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "Kernel/SimMath.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

// Staging, stated so the stills are read for what they are:
// - The intermission is reached by downing Bout 1's enemies in the kernel state (as MageArena.Session.ChainAdvance
//   does). The pick itself is the real path: the stone-left reach clip is played into the hand stream and the session
//   reads the palm over the stone for offerHoldS.
// - The reflection duel is Brennic's semifinal (wave index 2, Fire mages) with an unscripted player; the player's tier
//   is raised to 2 at the start (the tier Mirror II unlocks) so a perfect on an Ember Dart can reflect without waiting
//   15 s. The scripted seated policy never scores a perfect in this duel (MageArenaDesign.Duel logs perfects=0), so
//   the driver plays the ward-raise clip teach.json perfectLeadS (0.22 s) before a Fire projectile arrives, the same
//   lead the teach uses. The ward detector, the perfect and the reflection are the real pipeline and kernel.
// - The Leviathan Orb is cast through the sigil-line1 clip after the pick set Tide Orb IV to A; the player's tier is
//   raised to 4 at the start (Tide Orb IV unlocks at 45 s) and mana is full.

namespace
{
// Turns the seated view toward a kernel point (clamped by the pawn's own seated look limits).
void FaceKernel(AMageArenaPawn* Pawn, const FArenaSession& Bout, const FSimVec& From, const FSimVec& To, float Pitch)
{
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}
	const FVector Delta = Bout.KernelToUnrealCm(To) - Bout.KernelToUnrealCm(From);
	const float Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
	Controller->SetControlRotation(Pawn->ClampLookRotation(FRotator(Pitch, Yaw, 0.f)));
}
}

static void FaceFireMage(AMageArenaPawn* Pawn, const FArenaSession& Bout, float Pitch)
{
	const FArenaState& State = Bout.GetGames().State;
	const FActor* Me = SimFindActor(State, Bout.GetGames().PlayerId);
	for (const FActor& Actor : State.Actors)
	{
		if (Me && Actor.Fire.bSchool && Actor.Team != Me->Team && !Actor.bDown)
		{
			FaceKernel(Pawn, Bout, Me->Pos, Actor.Pos, Pitch);
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_FACE %s at (%.2f, %.2f) from (%.2f, %.2f)"), *Actor.Label, Actor.Pos.X, Actor.Pos.Y, Me->Pos.X, Me->Pos.Y);
			return;
		}
	}
}

UPresetCaptureDriver::UPresetCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UPresetCaptureDriver::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
}

void UPresetCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	Frames = 0;
}

UArenaSessionSubsystem* UPresetCaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

UHandInputSubsystem* UPresetCaptureDriver::HandsSys() const
{
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	return Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
}

AMageArenaPawn* UPresetCaptureDriver::FindPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AMageArenaPawn> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

FString UPresetCaptureDriver::RepoPath(const TCHAR* Relative) const
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

bool UPresetCaptureDriver::RequestShot(const TCHAR* FileName)
{
	const FString Path = RepoPath(*FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	bShotOpen = true;
	++ShotCount;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s path=%s"), FileName, *Path);
	return true;
}

bool UPresetCaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

void UPresetCaptureDriver::Fail(const TCHAR* Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), Reason);
	Enter(EStep::Quit);
}

void UPresetCaptureDriver::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (!bRunning)
	{
		return;
	}
	if (bShotOpen)
	{
		if (!ShotFinished())
		{
			return;
		}
		bShotOpen = false;
		PendingShot.Reset();
	}
	Advance();
}

void UPresetCaptureDriver::Advance()
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::Quit && Step != EStep::Finish && Now - RunStartWall > 400.0)
	{
		Fail(TEXT("timeout"));
		return;
	}
	UArenaSessionSubsystem* Session = SessionSys();
	AMageArenaPawn* Pawn = FindPawn();
	FArenaSession* Bout = Session ? &Session->GetSession() : nullptr;
	++Frames;

	switch (Step)
	{
	case EStep::WaitStable:
	{
		const float Delta = FApp::GetDeltaTime();
		const bool bShadersIdle = GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling();
		if (Pawn && Pawn->HasLayout() && bShadersIdle && Delta > 0.f && Delta < 0.05f)
		{
			++StableFrames;
		}
		else
		{
			StableFrames = 0;
		}
		if ((StableFrames >= 20 && Elapsed >= 1.5) || Elapsed >= 90.0)
		{
			if (!Session || !Pawn || !Bout)
			{
				Fail(TEXT("session or pawn missing"));
				break;
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			Pawn->SetLookPitch(-20.f);
			if (GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("r.MotionBlurQuality 0"));
			}
			SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("preset-capture"));
			IFileManager::Get().Delete(*FPaths::Combine(SaveDir, TEXT("bout.txt")), false, true, true);
			Bout->SetSaveDirectory(SaveDir);
			Bout->SetBout(0, false);
			if (!Session->Start(1, false))
			{
				Fail(TEXT("Start failed"));
				break;
			}
			// Staging: Bout 1 is won by downing its enemies, so the intermission opens at once.
			FArenaState& State = const_cast<FArenaState&>(Bout->GetGames().State);
			for (FActor& Actor : State.Actors)
			{
				if (Actor.Id != Bout->GetGames().PlayerId)
				{
					Actor.bDown = true;
				}
			}
			State.Projectiles.Reset();
			State.Telegraphs.Reset();
			Enter(EStep::WaitIntermission);
		}
		break;
	}
	case EStep::WaitIntermission:
		if (Bout && Bout->IsPickOffered() && Frames >= 30)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_PICK offered prompt=\"%s\" left=%s centre=%s right=%s"), *Bout->GetPromptText(),
				*Bout->TeachString(*Bout->PickStoneKey(0)), *Bout->TeachString(*Bout->PickStoneKey(1)), *Bout->TeachString(*Bout->PickStoneKey(2)));
			RequestShot(TEXT("01-intermission-stones.png"));
			Enter(EStep::WaitIntermissionShot);
		}
		else if (Elapsed > 15.0)
		{
			Fail(TEXT("the pick was not offered"));
		}
		break;
	case EStep::WaitIntermissionShot:
		if (UHandInputSubsystem* Hands = HandsSys())
		{
			Hands->PlayQuickAction(TEXT("stone-left"), EClipVariant::Normal);
		}
		Enter(EStep::WaitHold);
		break;
	case EStep::WaitHold:
		if (Bout && Bout->IsPickOffered() && Bout->GetPickHoldStone() == 0 && Bout->GetPickHoldFraction() >= 0.45)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_PICK hold stone=%d fraction=%.2f"), Bout->GetPickHoldStone(), Bout->GetPickHoldFraction());
			RequestShot(TEXT("02-leviathan-stone-hold.png"));
			Enter(EStep::WaitHoldShot);
		}
		else if (Elapsed > 10.0 || (Bout && !Bout->IsPickOffered()))
		{
			Fail(TEXT("the left stone hold was not seen"));
		}
		break;
	case EStep::WaitHoldShot:
		Enter(EStep::WaitPicked);
		break;
	case EStep::WaitPicked:
		if (Bout && Bout->GetGames().Phase == TEXT("active") && Bout->GetGames().Wave == 1)
		{
			FString Saved;
			FFileHelper::LoadFileToString(Saved, *FPaths::Combine(SaveDir, TEXT("bout.txt")));
			Saved.ReplaceInline(TEXT("\n"), TEXT(" "));
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_PICK done pick=%s save=\"%s\""), *Bout->GetStonePick(), *Saved);
			if (Bout->GetStonePick() != TEXT("A"))
			{
				Fail(TEXT("the left stone did not pick A"));
				break;
			}
			Enter(EStep::StartDuel);
		}
		else if (Elapsed > 10.0)
		{
			Fail(TEXT("the pick did not continue to Bout 2"));
		}
		break;
	case EStep::StartDuel:
	{
		if (!Session || !Bout || !Pawn)
		{
			Fail(TEXT("session missing before the duel"));
			break;
		}
		Pawn->LookAtArena();
		Bout->SetBout(2, true);
		if (!Session->Start(static_cast<uint32>(DuelSeed), false))
		{
			Fail(TEXT("duel start failed"));
			break;
		}
		FArenaState& State = const_cast<FArenaState&>(Bout->GetGames().State);
		if (FActor* Player = SimFindActor(State, Bout->GetGames().PlayerId))
		{
			// Staging: Mirror II's tier.
			Player->Tier = 2;
		}
		EventCursor = State.Events.Num();
		ReflectedId = -1;
		// One turn toward Brennic at the start. Turning every frame smeared the still with motion blur.
		FaceFireMage(Pawn, *Bout, -6.f);
		Enter(EStep::WaitReflect);
		break;
	}
	case EStep::WaitReflect:
	{
		if (!Bout)
		{
			Fail(TEXT("session missing in the duel"));
			break;
		}
		const FArenaState& State = Bout->GetGames().State;
		if (ReflectedId < 0)
		{
			const FActor* Player = SimFindActor(State, Bout->GetGames().PlayerId);
			UHandInputSubsystem* Hands = HandsSys();
			if (Player && Hands && !Hands->IsActionPlaying() && !Player->bAbsorb)
			{
				for (const FProjectile& Projectile : State.Projectiles)
				{
					const FActor* Owner = SimFindActor(State, Projectile.OwnerId);
					const double Speed = SimLength(Projectile.Velocity);
					if (!Owner || Owner->Team == Player->Team || Projectile.Family != TEXT("magic") || Projectile.Tier > Player->Tier || Speed <= 0.0)
					{
						continue;
					}
					const FSimVec ToPlayer = SimSub(Player->Pos, Projectile.Pos);
					if (ToPlayer.X * Projectile.Velocity.X + ToPlayer.Y * Projectile.Velocity.Y <= 0.0)
					{
						continue;
					}
					const double Eta = (SimLength(ToPlayer) - Player->Radius - Projectile.Radius) / Speed;
					if (Eta <= Bout->GetTuning().PerfectLeadS && Eta >= 0.08)
					{
						UE_LOG(LogMageArena, Log, TEXT("MAGEVR_REFLECT ward-raise for projectile %d tier=%d eta=%.3f"), Projectile.Id, Projectile.Tier, Eta);
						Hands->PlayQuickAction(TEXT("ward-raise"), EClipVariant::Normal);
						break;
					}
				}
			}
			for (int32 Index = EventCursor; Index < State.Events.Num(); ++Index)
			{
				const FArenaEvent& Event = State.Events[Index];
				if (Event.Kind == TEXT("reflect"))
				{
					for (const FProjectile& Projectile : State.Projectiles)
					{
						if (Projectile.bReflected && Projectile.OwnerId == Event.ActorId)
						{
							ReflectedId = Projectile.Id;
						}
					}
					UE_LOG(LogMageArena, Log, TEXT("MAGEVR_REFLECT tick=%d reflector=%d caster=%d tier=%.0f projectile=%d"),
						Event.Tick, Event.ActorId, Event.TargetId.Get(-1), Event.Value, ReflectedId);
				}
			}
			EventCursor = State.Events.Num();
		}
		if (ReflectedId >= 0)
		{
			const FProjectile* Shot = nullptr;
			for (const FProjectile& Projectile : State.Projectiles)
			{
				if (Projectile.Id == ReflectedId)
				{
					Shot = &Projectile;
				}
			}
			if (!Shot)
			{
				UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_REFLECT projectile %d resolved before the shot; waiting for another"), ReflectedId);
				ReflectedId = -1;
				break;
			}
			const double Travelled = SimDistance(Shot->OriginPos, Shot->Pos);
			const FActor* Target = nullptr;
			for (const FArenaEvent& Event : State.Events)
			{
				if (Event.Kind == TEXT("reflect"))
				{
					Target = SimFindActor(State, Event.TargetId.Get(-1));
				}
			}
			const double Span = Target ? SimDistance(Shot->OriginPos, Target->Pos) : 6.0;
			if (Travelled >= 0.45 * Span)
			{
				Bout->SetFrameHold(true);
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_REFLECT shot travelled=%.2f of %.2f remaining=%.2f damage=%.2f caster=%s hp=%.1f"),
					Travelled, Span, Shot->RemainingM, Shot->Damage, Target ? *Target->Label : TEXT("-"), Target ? Target->Hp : -1.0);
				const FActor* Me = SimFindActor(State, Bout->GetGames().PlayerId);
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_REFLECT scene player=(%.2f, %.2f) bolt=(%.2f, %.2f) caster=(%.2f, %.2f) projectiles=%d"),
					Me ? Me->Pos.X : 0.0, Me ? Me->Pos.Y : 0.0, Shot->Pos.X, Shot->Pos.Y, Target ? Target->Pos.X : 0.0, Target ? Target->Pos.Y : 0.0,
					State.Projectiles.Num());
				// The kernel is frozen. Turn the seat toward the caster, let two frames render, then shoot.
				if (Me && Target)
				{
					FaceKernel(Pawn, *Bout, Me->Pos, Target->Pos, -4.f);
				}
				Enter(EStep::WaitReflectAim);
			}
			break;
		}
		if (Bout->GetGames().Phase != TEXT("active"))
		{
			UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_REFLECT duel seed %d ended %s without a reflection"), DuelSeed, *Bout->GetGames().Phase);
			++DuelSeed;
			if (DuelSeed > 8)
			{
				Fail(TEXT("no reflection in eight duels"));
				break;
			}
			Enter(EStep::StartDuel);
		}
		else if (Elapsed > 150.0)
		{
			Fail(TEXT("reflection timeout"));
		}
		break;
	}
	case EStep::WaitReflectAim:
		if (Frames >= 3)
		{
			RequestShot(TEXT("03-reflected-firebolt.png"));
			Enter(EStep::WaitReflectShot);
		}
		break;
	case EStep::WaitReflectShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		Enter(EStep::StartOrb);
		break;
	case EStep::StartOrb:
	{
		if (!Session || !Bout || !Pawn)
		{
			Fail(TEXT("session missing before the orb"));
			break;
		}
		Pawn->LookAtArena();
		// Brennic's semifinal again: he keeps his distance, so the orb has open sand to cross. In Bout 1 the conscripts
		// stand at the dais lip and the 3 m orb met them within 3.2 m, filling the view.
		Bout->SetBout(2, true);
		if (!Session->Start(1, false))
		{
			Fail(TEXT("orb start failed"));
			break;
		}
		FArenaState& State = const_cast<FArenaState&>(Bout->GetGames().State);
		if (FActor* Player = SimFindActor(State, Bout->GetGames().PlayerId))
		{
			// Staging: Tide Orb IV's tier, and the mana for it.
			Player->Tier = 4;
			Player->Mana = Player->MaxMana;
		}
		FaceFireMage(Pawn, *Bout, -14.f);
		if (FActor* Player = SimFindActor(State, Bout->GetGames().PlayerId))
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_ORB composition tideOrbIV=%s mirror=%s"),
				*Player->Water.Composition.Branches.TideOrb, *Player->Water.Composition.Branches.Mirror);
		}
		bSigilPlayed = false;
		bTelegraphShot = false;
		Enter(EStep::WaitOrb);
		break;
	}
	case EStep::WaitOrb:
	{
		if (!Bout)
		{
			Fail(TEXT("session missing for the orb"));
			break;
		}
		if (!bSigilPlayed && Elapsed >= 0.5)
		{
			if (UHandInputSubsystem* Hands = HandsSys())
			{
				Hands->PlayQuickAction(TEXT("sigil-line1"), EClipVariant::Normal);
			}
			bSigilPlayed = true;
		}
		const FArenaState& State = Bout->GetGames().State;
		const FActor* Me = SimFindActor(State, Bout->GetGames().PlayerId);
		if (!bTelegraphShot && Me && Me->Pending.IsSet() && Me->Pending->SpellId.Get(FString()) == TEXT("tide_orb:4:A")
			&& State.Tick - Me->Pending->StartTick >= 30)
		{
			bTelegraphShot = true;
			Bout->SetFrameHold(true);
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_ORB telegraph ticks=%d of %d"), State.Tick - Me->Pending->StartTick,
				Me->Pending->ReleaseTick - Me->Pending->StartTick);
			RequestShot(TEXT("04-leviathan-telegraph.png"));
			Enter(EStep::WaitTelegraphShot);
			break;
		}
		const FProjectile* Orb = nullptr;
		for (const FProjectile& Projectile : State.Projectiles)
		{
			if (Projectile.Family == TEXT("unblockable") && Projectile.OwnerId == Bout->GetGames().PlayerId)
			{
				Orb = &Projectile;
			}
		}
		if (Orb)
		{
			UE_LOG(LogMageArena, Verbose, TEXT("MAGEVR_ORB seen travelled=%.2f"), SimDistance(Orb->OriginPos, Orb->Pos));
			OrbSeen = FMath::Max(OrbSeen, SimDistance(Orb->OriginPos, Orb->Pos));
		}
		else if (OrbSeen > 0.0)
		{
			UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_ORB resolved after %.2f m"), OrbSeen);
			OrbSeen = 0.0;
		}
		if (Orb && SimDistance(Orb->OriginPos, Orb->Pos) >= (OrbTries == 0 ? 5.0 : 3.5))
		{
			Bout->SetFrameHold(true);
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_ORB flight speed=%.2f radius=%.2f damage=%.2f remaining=%.2f travelled=%.2f"),
				SimLength(Orb->Velocity), Orb->Radius, Orb->Damage, Orb->RemainingM, SimDistance(Orb->OriginPos, Orb->Pos));
			RequestShot(TEXT("05-leviathan-orb-path.png"));
			Enter(EStep::WaitOrbShot);
		}
		else if (Elapsed > 12.0)
		{
			++OrbTries;
			UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_ORB no orb after %.1f s (try %d)"), Elapsed, OrbTries);
			if (OrbTries >= 3)
			{
				Fail(TEXT("no Leviathan Orb"));
				break;
			}
			Enter(EStep::StartOrb);
		}
		break;
	}
	case EStep::WaitTelegraphShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		Step = EStep::WaitOrb;
		break;
	case EStep::WaitOrbShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		Enter(EStep::Finish);
		break;
	case EStep::Finish:
		if (ShotCount >= 5)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE_DONE shots=%d"), ShotCount);
		}
		else
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL shots=%d"), ShotCount);
		}
		Enter(EStep::Quit);
		break;
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
}

TStatId UPresetCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPresetCaptureDriver, STATGROUP_Tickables);
}

bool UPresetCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UPresetCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UPresetCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
