#include "Session/ColourAudioCapture.h"

#include "Session/ArenaAudio.h"
#include "Session/ArenaSession.h"
#include "Session/SessionPresentation.h"
#include "Kernel/Air.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/SimMath.h"
#include "Kernel/VrRules.h"

#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/MageArenaPawn.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "Sound/SoundSubmixSend.h"
#include "UnrealClient.h"

// Staging, stated so the stills and the recording are read for what they are:
// - Every bout is the session's own bout on the live kernel, presentation and audio. The colours are whatever
//   ElementLook draws; nothing is recoloured for the capture.
// - Hound ember: the creatures bout (Bout 2, scripted seat). The still is the first frame with a hound's ember in
//   flight 2.5 to 6 m from the seat (after 30 s, any ember beyond 2.5 m), the camera turned to it, the kernel frozen
//   for the shot (frame hold).
// - Fire and Water bolts: the Brennic semifinal (SetBout(2, true), scripted seat). The still is the first frame with a
//   Brennic magic projectile (beyond 3 m) and a player projectile (beyond 4 m) in flight and nothing within 2 m of the
//   seat; after 40 s without one,
//   the first frame with a Brennic bolt alone (logged).
// - Squall: Lio in the air final (SetBout(3, true)); the Squall is started with StartGrantedAirCast, the real cast path
//   minus the tier gate, cooldown and mana, as T23's capture does, and the still waits for three orbs in flight.
// - Audio: the Brennic semifinal from Start, scripted seat, 30 s of game time recorded from the master submix with
//   UAudioMixerBlueprintLibrary on the real-time mixer (the default output device also plays it), frame rate capped at
//   60. Nothing is frozen or granted during the recording.

namespace
{
// 30 s of play plus the start-up latency of the submix recorder (about 0.3 to 0.5 s measured), so the WAV is at least 30 s.
constexpr double RecordSeconds = 30.8;

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

const FActor* FindSchoolMage(const FArenaSession& Bout, bool bFire)
{
	for (const FActor& Actor : Bout.GetGames().State.Actors)
	{
		if (Actor.Team != 0 && !Actor.bDown && !Actor.Enemy.IsSet() && (bFire ? Actor.Fire.bSchool : Actor.Air.bSchool))
		{
			return &Actor;
		}
	}
	return nullptr;
}
}

UColourAudioCaptureDriver::UColourAudioCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UColourAudioCaptureDriver::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	FString Mode;
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaT26Mode="), Mode);
	bAudio = Mode == TEXT("audio");
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_T26 start mode=%s run=%s"), bAudio ? TEXT("audio") : TEXT("stills"), *RunId);
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
}

void UColourAudioCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	Frames = 0;
}

UArenaSessionSubsystem* UColourAudioCaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

AMageArenaPawn* UColourAudioCaptureDriver::FindPawn() const
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

FString UColourAudioCaptureDriver::RepoPath(const FString& Relative) const
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

void UColourAudioCaptureDriver::RequestShot(const TCHAR* FileName)
{
	const FString Path = RepoPath(FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	bShotOpen = true;
	++ShotCount;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s path=%s"), FileName, *Path);
}

bool UColourAudioCaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

void UColourAudioCaptureDriver::Fail(const FString& Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), *Reason);
	Enter(EStep::Quit);
}

void UColourAudioCaptureDriver::Tick(float DeltaTime)
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
		if (UArenaSessionSubsystem* Session = SessionSys())
		{
			Session->GetSession().SetFrameHold(false);
		}
		Enter(AfterShot);
	}
	Advance();
}

void UColourAudioCaptureDriver::LogNewCues()
{
	UArenaSessionSubsystem* Session = SessionSys();
	ASessionPresentation* Presentation = Session ? Session->GetPresentation() : nullptr;
	UArenaAudioComponent* Audio = Presentation ? Presentation->GetAudio() : nullptr;
	if (!Audio)
	{
		return;
	}
	const TArray<FAudioPlayRequest>& History = Audio->GetHistory();
	const double Wall = RecordedS;
	const TCHAR* Kinds[] = {TEXT("oneshot"), TEXT("loop-start"), TEXT("loop-stop"), TEXT("swell")};
	for (; CuesLogged < History.Num(); ++CuesLogged)
	{
		const FAudioPlayRequest& Request = History[CuesLogged];
		CueLog += FString::Printf(TEXT("%.3f\t%.3f\t%s\t%s\t%s\t%d\t%.0f\t%.0f\t%.0f\t%.1f\n"), Wall, Request.AtS, Kinds[static_cast<int32>(Request.Kind)],
			*Request.Cue.ToString(), *Request.Event, Request.SourceId, Request.LocationCm.X, Request.LocationCm.Y, Request.LocationCm.Z, Request.GainDb);
	}
}

void UColourAudioCaptureDriver::Advance()
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::Quit && Step != EStep::Finish && Now - RunStartWall > 480.0)
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
			if (GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("r.MotionBlurQuality 0"));
				if (bAudio)
				{
					// Uncapped, the greybox runs near 1000 fps and starves the audio render thread: the first recording
					// came out 20.8 s long for 30 s of play. 60 fps leaves the mixer its time.
					GEngine->Exec(GetWorld(), TEXT("t.MaxFPS 60"));
					// The capture window is hidden (-RenderOffScreen), so the app is never focused, and an unfocused app
					// plays at [Audio] UnfocusedVolumeMultiplier (0 by default): the master submix would record silence.
					FApp::SetUnfocusedVolumeMultiplier(1.f);
				}
			}
			Bout->SetSaveDirectory(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("t26-capture")));
			Enter(bAudio ? EStep::StartRecording : EStep::StartCreatures);
		}
		break;
	}
	case EStep::StartCreatures:
		Bout->SetBout(1, false);
		if (!Session->Start(1, true))
		{
			Fail(TEXT("creatures Start failed"));
			break;
		}
		Enter(EStep::WaitEmber);
		break;
	case EStep::WaitEmber:
	{
		const FGames& Games = Bout->GetGames();
		const FActor* Me = SimFindActor(Games.State, Games.PlayerId);
		const FProjectile* Ember = nullptr;
		for (const FProjectile& Shot : Games.State.Projectiles)
		{
			const FActor* Owner = SimFindActor(Games.State, Shot.OwnerId);
			const double Distance = Me ? SimDistance(Shot.Pos, Me->Pos) : 0.0;
			if (Me && Owner && Owner->Enemy.IsSet() && Owner->Enemy->Id == TEXT("cinder_hound") && Shot.Family == TEXT("magic")
				&& Distance > 2.5 && (Distance < 6.0 || Elapsed > 30.0))
			{
				Ember = &Shot;
				break;
			}
		}
		// Nothing of anyone's within 2 m of the seat: a ball at the lens fills the frame (the first take had one).
		for (const FProjectile& Shot : Games.State.Projectiles)
		{
			if (Me && SimDistance(Shot.Pos, Me->Pos) < 2.0)
			{
				Ember = nullptr;
			}
		}
		if (Ember && Me)
		{
			Bout->SetFrameHold(true);
			FaceKernel(Pawn, *Bout, Me->Pos, Ember->Pos, -10.f);
			if (Frames >= 4)
			{
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_T26 ember owner=%d at (%.2f, %.2f) dist=%.2f tick=%d"), Ember->OwnerId, Ember->Pos.X, Ember->Pos.Y,
					SimDistance(Ember->Pos, Me->Pos), Games.State.Tick);
				AfterShot = EStep::StartDuel;
				RequestShot(TEXT("01-hound-ember.png"));
			}
			break;
		}
		Frames = 0;
		if (Elapsed > 60.0)
		{
			Fail(TEXT("no hound ember in flight in 60 s"));
		}
		break;
	}
	case EStep::StartDuel:
		Bout->SetBout(2, true);
		Bout->SetAirFinal(true);
		if (!Session->Start(1, true))
		{
			Fail(TEXT("duel Start failed"));
			break;
		}
		Enter(EStep::WaitBolts);
		break;
	case EStep::WaitBolts:
	{
		const FGames& Games = Bout->GetGames();
		const FActor* Me = SimFindActor(Games.State, Games.PlayerId);
		const FActor* Brennic = FindSchoolMage(*Bout, true);
		if (!Me || !Brennic)
		{
			if (Elapsed > 5.0)
			{
				Fail(TEXT("no Fire mage in the semifinal"));
			}
			break;
		}
		const FProjectile* FireBolt = nullptr;
		const FProjectile* WaterBolt = nullptr;
		for (const FProjectile& Shot : Games.State.Projectiles)
		{
			if (Shot.OwnerId == Brennic->Id && Shot.Family == TEXT("magic") && SimDistance(Shot.Pos, Me->Pos) > 3.0)
			{
				FireBolt = &Shot;
			}
			if (Shot.OwnerId == Games.PlayerId && SimDistance(Shot.Pos, Me->Pos) > 4.0)
			{
				WaterBolt = &Shot;
			}
		}
		bool bLensClear = true;
		for (const FProjectile& Shot : Games.State.Projectiles)
		{
			bLensClear &= SimDistance(Shot.Pos, Me->Pos) >= 2.0;
		}
		if (!bLensClear)
		{
			FireBolt = nullptr;
		}
		const bool bBoth = FireBolt && WaterBolt;
		if (bBoth || (FireBolt && Elapsed > 40.0))
		{
			Bout->SetFrameHold(true);
			FaceKernel(Pawn, *Bout, Me->Pos, Brennic->Pos, -6.f);
			if (Frames >= 4)
			{
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_T26 bolts fire=(%.2f, %.2f) water=%s brennic=(%.2f, %.2f) both=%d tick=%d"), FireBolt->Pos.X,
					FireBolt->Pos.Y, WaterBolt ? *FString::Printf(TEXT("(%.2f, %.2f)"), WaterBolt->Pos.X, WaterBolt->Pos.Y) : TEXT("none"), Brennic->Pos.X,
					Brennic->Pos.Y, bBoth ? 1 : 0, Games.State.Tick);
				AfterShot = EStep::StartAir;
				RequestShot(TEXT("02-fire-and-water-bolts.png"));
			}
			break;
		}
		Frames = 0;
		if (Games.Phase != TEXT("active") && Elapsed > 5.0)
		{
			Fail(TEXT("the duel ended before the bolts still"));
		}
		else if (Elapsed > 90.0)
		{
			Fail(TEXT("no fire bolt in 90 s"));
		}
		break;
	}
	case EStep::StartAir:
		Bout->SetBout(3, true);
		Bout->SetAirFinal(true);
		if (!Session->Start(1, true))
		{
			Fail(TEXT("air final Start failed"));
			break;
		}
		Enter(EStep::WaitLio);
		break;
	case EStep::WaitLio:
	{
		if (Elapsed < 1.2)
		{
			break;
		}
		Bout->SetScripted(false);
		FArenaState& State = const_cast<FArenaState&>(Bout->GetGames().State);
		FActor* Lio = const_cast<FActor*>(FindSchoolMage(*Bout, false));
		FActor* Me = SimFindActor(State, Bout->GetGames().PlayerId);
		if (!Lio || !Me)
		{
			Fail(TEXT("Lio or the player is missing"));
			break;
		}
		if (Lio->Pending.IsSet() || AirChannelBusy(*Lio) || State.Tick < Lio->RollUntil)
		{
			if (Elapsed > 12.0)
			{
				Fail(TEXT("Lio never came free"));
			}
			break;
		}
		Lio->Hp = Lio->MaxHp;
		Me->Hp = Me->MaxHp;
		if (!StartGrantedAirCast(State, *Lio, TEXT("air_squall"), Me->Pos, Bout->GetVrRules()))
		{
			Fail(TEXT("the Squall grant did not start"));
			break;
		}
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_T26 squall granted tick=%d"), State.Tick);
		Enter(EStep::WaitSquall);
		break;
	}
	case EStep::WaitSquall:
	{
		const FGames& Games = Bout->GetGames();
		const FActor* Lio = FindSchoolMage(*Bout, false);
		const FActor* Me = SimFindActor(Games.State, Games.PlayerId);
		int32 Orbs = 0;
		for (const FProjectile& Shot : Games.State.Projectiles)
		{
			Orbs += (Lio && Shot.OwnerId == Lio->Id && Shot.bAir) ? 1 : 0;
		}
		if (Lio && Me && Orbs >= 3)
		{
			Bout->SetFrameHold(true);
			FaceKernel(Pawn, *Bout, Me->Pos, Lio->Pos, -8.f);
			if (Frames >= 4)
			{
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_T26 squall orbs=%d tick=%d"), Orbs, Games.State.Tick);
				AfterShot = EStep::Finish;
				RequestShot(TEXT("03-air-squall.png"));
			}
			break;
		}
		Frames = 0;
		if (Elapsed > 10.0)
		{
			Fail(FString::Printf(TEXT("the Squall never had three orbs in flight (%d)"), Orbs));
		}
		break;
	}
	case EStep::StartRecording:
	{
		Bout->SetBout(2, true);
		Bout->SetAirFinal(true);
		if (!Session->Start(1, true))
		{
			Fail(TEXT("duel Start failed"));
			break;
		}
		UAudioMixerBlueprintLibrary::StartRecordingOutput(GetWorld(), static_cast<float>(RecordSeconds));
		RecordStartWall = FPlatformTime::Seconds();
		RecordedS = 0.0;
		CueLog = TEXT("audio_s\tsim_s\tkind\tcue\tevent\tsource\tx_cm\ty_cm\tz_cm\tgain_db\n");
		const FActor* Brennic = FindSchoolMage(*Bout, true);
		FAudioDevice* Device = GetWorld() ? GetWorld()->GetAudioDeviceRaw() : nullptr;
		RecordStartClock = Device ? Device->GetAudioClock() : -1.0;
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_T26 recording start brennic=%d audioclock=%.3f"), Brennic ? Brennic->Id : -1, RecordStartClock);
		Enter(EStep::Recording);
		break;
	}
	case EStep::Recording:
		RecordedS += FApp::GetDeltaTime();
		LogNewCues();
		if (RecordedS >= RecordSeconds)
		{
			const FString Dir = RepoPath(FString::Printf(TEXT("runs/%s/audio"), *RunId));
			IFileManager::Get().MakeDirectory(*Dir, true);
			UAudioMixerBlueprintLibrary::StopRecordingOutput(GetWorld(), EAudioRecordingExportType::WavFile, TEXT("brennic-opening"), Dir);
			FFileHelper::SaveStringToFile(CueLog, *FPaths::Combine(Dir, TEXT("brennic-opening-cues.tsv")));
			FAudioDevice* Device = GetWorld() ? GetWorld()->GetAudioDeviceRaw() : nullptr;
			const double Clock = Device ? Device->GetAudioClock() : -1.0;
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_T26 recording stop audio=%.3f wall=%.3f sim=%.3f mixer=%.3f cues=%d phase=%s"), RecordedS,
				Now - RecordStartWall, Bout->GetSimSeconds(), Clock - RecordStartClock, CuesLogged, *Bout->GetGames().Phase);
			Enter(EStep::WaitWav);
		}
		break;
	case EStep::WaitWav:
	{
		const FString Wav = FPaths::Combine(RepoPath(FString::Printf(TEXT("runs/%s/audio"), *RunId)), TEXT("brennic-opening.wav"));
		const int64 Size = IFileManager::Get().FileSize(*Wav);
		WavStableFrames = (Size > 0 && Size == LastWavSize) ? WavStableFrames + 1 : 0;
		LastWavSize = Size;
		if (WavStableFrames >= 30)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_T26 wav %s bytes=%lld"), *Wav, Size);
			Enter(EStep::Finish);
		}
		else if (Elapsed > 30.0)
		{
			Fail(FString::Printf(TEXT("the recording was not written (%s size %lld)"), *Wav, Size));
		}
		break;
	}
	case EStep::Finish:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		if (bAudio || ShotCount >= 3)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE_DONE mode=%s shots=%d"), bAudio ? TEXT("audio") : TEXT("stills"), ShotCount);
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

TStatId UColourAudioCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UColourAudioCaptureDriver, STATGROUP_Tickables);
}

bool UColourAudioCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UColourAudioCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UColourAudioCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
