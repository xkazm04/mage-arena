#include "Session/Wave1Capture.h"

#include "Session/ArenaSession.h"
#include "Session/SessionPresentation.h"

#include "Camera/CameraComponent.h"
#include "ConvexVolume.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/MageArenaPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kernel/SimMath.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIStats.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

#include <cmath>

namespace
{
bool FireWallBoltClose(const FArenaSession& Bout)
{
	const FVrRuleset* Rules = Bout.GetVrRules();
	if (!Rules || Rules->Walls.Num() == 0)
	{
		return false;
	}
	const int32 PlayerId = Bout.GetGames().PlayerId;
	for (const FProjectile& Shot : Bout.GetGames().State.Projectiles)
	{
		if (Shot.OwnerId != PlayerId || Shot.Family == TEXT("unblockable"))
		{
			continue;
		}
		for (const FVrWall& Wall : Rules->Walls)
		{
			const FSimVec Toward = SimSub(Shot.Pos, Wall.Centre);
			const double Gap = std::abs(SimDistance(Shot.Pos, Wall.Centre) - Wall.DistanceM);
			if (Gap < 1.2 && SimInArc(Wall.Facing, Toward, Wall.ArcDeg + 24.0))
			{
				return true;
			}
		}
	}
	return false;
}

double MedianOf(TArray<double> Values)
{
	if (Values.Num() == 0)
	{
		return 0.0;
	}
	Values.Sort();
	const int32 Mid = Values.Num() / 2;
	if ((Values.Num() % 2) == 0)
	{
		return 0.5 * (Values[Mid - 1] + Values[Mid]);
	}
	return Values[Mid];
}

TSharedRef<FJsonObject> SpanInt(const TArray<int32>& Values)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	TArray<double> AsDouble;
	int32 Min = 0;
	int32 Max = 0;
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		AsDouble.Add(static_cast<double>(Values[Index]));
		Min = Index == 0 ? Values[Index] : FMath::Min(Min, Values[Index]);
		Max = Index == 0 ? Values[Index] : FMath::Max(Max, Values[Index]);
	}
	Object->SetNumberField(TEXT("min"), Min);
	Object->SetNumberField(TEXT("median"), MedianOf(AsDouble));
	Object->SetNumberField(TEXT("max"), Max);
	return Object;
}

TSharedRef<FJsonObject> SpanTime(const TArray<double>& Values)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	TArray<double> Copy = Values;
	Copy.Sort();
	const double Min = Copy.Num() ? Copy[0] : 0.0;
	const double Max = Copy.Num() ? Copy.Last() : 0.0;
	const int32 P95 = Copy.Num() ? FMath::Clamp(FMath::CeilToInt(0.95 * Copy.Num()) - 1, 0, Copy.Num() - 1) : 0;
	Object->SetNumberField(TEXT("min"), Min);
	Object->SetNumberField(TEXT("median"), MedianOf(Copy));
	Object->SetNumberField(TEXT("p95"), Copy.Num() ? Copy[P95] : 0.0);
	Object->SetNumberField(TEXT("max"), Max);
	return Object;
}

bool ConscriptAtLip(const FArenaSession& Bout)
{
	const FVrRuleset* Rules = Bout.GetVrRules();
	if (!Rules)
	{
		return false;
	}
	for (const FActor& Actor : Bout.GetGames().State.Actors)
	{
		if (Actor.bDown || !Actor.Enemy.IsSet() || Actor.Enemy->Id != TEXT("conscript"))
		{
			continue;
		}
		const double Gap = Rules->OutsideGap(Actor.Pos);
		if (Gap >= -0.05 && Gap < 0.45)
		{
			return true;
		}
	}
	return false;
}

bool ConscriptSpearInFlight(const FArenaSession& Bout)
{
	const FGames& Games = Bout.GetGames();
	for (const FProjectile& Projectile : Games.State.Projectiles)
	{
		if (!Projectile.bHasAim || Projectile.OwnerId == Games.PlayerId)
		{
			continue;
		}
		const FActor* Owner = SimFindActor(Games.State, Projectile.OwnerId);
		if (Owner && Owner->Enemy.IsSet() && Owner->Enemy->Id == TEXT("conscript"))
		{
			return true;
		}
	}
	return false;
}
}

UWave1CaptureDriver::UWave1CaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UWave1CaptureDriver::Start()
{
	RunId = TEXT("T08");
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
}

void UWave1CaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
}

UArenaSessionSubsystem* UWave1CaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

AMageArenaPawn* UWave1CaptureDriver::FindPawn() const
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

FString UWave1CaptureDriver::RepoPath(const TCHAR* Relative) const
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

bool UWave1CaptureDriver::RequestShot(const TCHAR* FileName, bool bSequence)
{
	const FString Path = RepoPath(*FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	bShotOpen = true;
	if (!bSequence)
	{
		ShotNames.Add(FileName);
	}
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s path=%s"), FileName, *Path);
	return true;
}

bool UWave1CaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

void UWave1CaptureDriver::SampleBudget()
{
	const double Milliseconds = static_cast<double>(FApp::GetDeltaTime()) * 1000.0;
	const int32 Draws = GNumDrawCallsRHI[0];
	if (Draws <= 0 || Milliseconds <= 0.0 || Milliseconds > 500.0)
	{
		return;
	}
	DrawCalls.Add(Draws);
	MeasuredTriangles.Add(GNumPrimitivesDrawnRHI[0]);
	FrameMs.Add(Milliseconds);
	int32 Enemies = 0;
	if (UArenaSessionSubsystem* Session = SessionSys())
	{
		if (ASessionPresentation* Presentation = Session->GetPresentation())
		{
			Enemies = Presentation->GetEnemiesOnScreen();
		}
	}
	EnemiesOnScreen.Add(Enemies);
}

void UWave1CaptureDriver::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	if (Step != EStep::WaitStable && Step != EStep::WriteBudget && Step != EStep::Quit)
	{
		SampleBudget();
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
	const double Now = FPlatformTime::Seconds();
	const bool bWaiting =
		Step == EStep::WaitSoldiers || Step == EStep::WaitWard || Step == EStep::WaitSigil
		|| Step == EStep::WaitBlink || Step == EStep::WaitVictory;
	if (bWaiting && SequenceIndex < 24 && Now - LastSequenceWall > 1.6)
	{
		LastSequenceWall = Now;
		++SequenceIndex;
		RequestShot(*FString::Printf(TEXT("seq/frame_%04d.png"), SequenceIndex), true);
		return;
	}
	Advance();
}

void UWave1CaptureDriver::Advance()
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::Quit && Step != EStep::WriteBudget && Now - RunStartWall > 300.0)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL timeout"));
		Enter(EStep::WriteBudget);
		return;
	}
	UArenaSessionSubsystem* Session = SessionSys();
	AMageArenaPawn* Pawn = FindPawn();
	const FArenaSession* Bout = Session ? &Session->GetSession() : nullptr;

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
			if (!Session || !Pawn)
			{
				UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL session or pawn missing"));
				Enter(EStep::WriteBudget);
				break;
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			if (RunId == TEXT("T12"))
			{
				Session->GetSession().SetBout(2, true);
			}
			Session->Start(1, true);
			LastSequenceWall = Now;
			Enter(EStep::WaitSoldiers);
		}
		break;
	}
	case EStep::WaitSoldiers:
		if (RunId == TEXT("T12"))
		{
			if (Bout && FireWallBoltClose(*Bout))
			{
				if (Session)
				{
					Session->GetSession().SetPaused(true);
				}
				RequestShot(TEXT("01-firewall-bolt.png"), false);
				Enter(EStep::WaitSoldiersShot);
			}
			else if (Elapsed > 75.0)
			{
				UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL firewall bolt was not in frame"));
				Enter(EStep::WriteBudget);
			}
			break;
		}
		if (RunId == TEXT("T11"))
		{
			// The sigil clip is 2.1 s. The palm is in frame by 1 s, and the line is still being drawn.
			const bool bPose = Bout && Bout->IsSplitPose() && Bout->GetSimSeconds() >= 1.0;
			if (bPose || Elapsed > 18.0)
			{
				if (Session)
				{
					Session->GetSession().SetPaused(true);
				}
				RequestShot(TEXT("01-split-hands.png"), false);
				Enter(EStep::WaitSoldiersShot);
			}
			break;
		}
		if (RunId == TEXT("T10"))
		{
			if ((Bout && ConscriptAtLip(*Bout)) || Elapsed > 25.0)
			{
				if (Session)
				{
					Session->GetSession().SetPaused(true);
				}
				RequestShot(TEXT("01-dais-edge.png"), false);
				Enter(EStep::WaitSoldiersShot);
			}
			break;
		}
		if (Bout && Bout->GetGames().State.Actors.Num() >= 5 && Elapsed > 0.4)
		{
			Session->GetSession().SetPaused(true);
			RequestShot(TEXT("01-first-soldiers.png"), false);
			Enter(EStep::WaitSoldiersShot);
		}
		else if (Elapsed > 12.0)
		{
			RequestShot(TEXT("01-first-soldiers.png"), false);
			Enter(EStep::WaitSoldiersShot);
		}
		break;
	case EStep::WaitSoldiersShot:
		if (RunId == TEXT("T12"))
		{
			Enter(EStep::WriteBudget);
			break;
		}
		if (Session)
		{
			Session->GetSession().SetPaused(false);
		}
		Enter(EStep::WaitWard);
		break;
	case EStep::WaitWard:
		if (RunId == TEXT("T11"))
		{
			if ((Bout && Bout->IsStaffPlanted()) || Elapsed > 22.0)
			{
				if (Session)
				{
					Session->GetSession().SetPaused(true);
				}
				RequestShot(TEXT("02-planted-dome.png"), false);
				Enter(EStep::WaitWardShot);
			}
			break;
		}
		if (RunId == TEXT("T10"))
		{
			if ((Bout && ConscriptSpearInFlight(*Bout)) || Elapsed > 12.0)
			{
				if (Session)
				{
					Session->GetSession().SetPaused(true);
				}
				RequestShot(TEXT("02-spear-flight.png"), false);
				Enter(EStep::WaitWardShot);
			}
			break;
		}
		if ((Bout && Bout->WasWardOnStone()) || Elapsed > 8.0)
		{
			if (Session)
			{
				Session->GetSession().SetPaused(true);
			}
			RequestShot(TEXT("02-ward-meets-stone.png"), false);
			Enter(EStep::WaitWardShot);
		}
		break;
	case EStep::WaitWardShot:
		if (Session)
		{
			Session->GetSession().SetPaused(false);
		}
		Enter(EStep::WaitSigil);
		break;
	case EStep::WaitSigil:
		if (RunId == TEXT("T10"))
		{
			Enter(EStep::WaitBlink);
			break;
		}
		if (RunId == TEXT("T11"))
		{
			Enter(EStep::WaitVictory);
			break;
		}
		if ((Bout && Bout->WasSigilHit()) || Elapsed > 14.0)
		{
			if (Session)
			{
				Session->GetSession().SetPaused(true);
			}
			RequestShot(TEXT("03-sigil-hit.png"), false);
			Enter(EStep::WaitSigilShot);
		}
		break;
	case EStep::WaitSigilShot:
		if (Session)
		{
			Session->GetSession().SetPaused(false);
		}
		Enter(EStep::WaitBlink);
		break;
	case EStep::WaitBlink:
		if (Bout && Bout->GetBlinkAccepts() > 0 && Pawn && !Pawn->IsBlinkBusy())
		{
			if (Session)
			{
				Session->GetSession().SetPaused(true);
			}
			RequestShot(RunId == TEXT("T10") ? TEXT("03-blink.png") : TEXT("04-blink.png"), false);
			Enter(EStep::WaitBlinkShot);
		}
		else if (Elapsed > 20.0)
		{
			RequestShot(RunId == TEXT("T10") ? TEXT("03-blink.png") : TEXT("04-blink.png"), false);
			Enter(EStep::WaitBlinkShot);
		}
		break;
	case EStep::WaitBlinkShot:
		if (Session)
		{
			Session->GetSession().SetPaused(false);
		}
		Enter(EStep::WaitVictory);
		break;
	case EStep::WaitVictory:
		// The seated bout does not walk, so it may outlast the 25-40 s window. Wait for the
		// phase change, or the same 120 s cap the scripted test uses.
		if (!Bout || Bout->GetGames().Phase != TEXT("active") || Bout->GetSimSeconds() > 120.0 || Elapsed > 160.0)
		{
			if (Session)
			{
				Session->GetSession().SetPaused(true);
			}
			RequestShot(RunId == TEXT("T11") ? TEXT("03-end.png") : RunId == TEXT("T10") ? TEXT("04-end.png") : TEXT("05-victory.png"), false);
			Enter(EStep::WaitVictoryShot);
		}
		break;
	case EStep::WaitVictoryShot:
		Enter(EStep::WriteBudget);
		break;
	case EStep::WriteBudget:
	{
		WriteBudgetFile();
		const int32 Needed = RunId == TEXT("T12") ? 1 : RunId == TEXT("T11") ? 3 : RunId == TEXT("T10") ? 4 : 5;
		if (ShotNames.Num() >= Needed)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE_DONE shots=%d samples=%d"), ShotNames.Num(), DrawCalls.Num());
		}
		else
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL shots=%d"), ShotNames.Num());
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
}

void UWave1CaptureDriver::WriteBudgetFile()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("label"), TEXT("desktop proxy - not Quest truth"));
	Root->SetStringField(TEXT("note"),
		RunId == TEXT("T12")
			? TEXT("Seated Fire duel. The orange-red curtain is the fire mage's wall. The bolt is still in the air, short of the arc that will destroy it. The curtain is not the black-and-red unblockable rim.")
			: RunId == TEXT("T11")
			? TEXT("Wave 1 with the ward held while a sigil is drawn, and a planted staff dome. Threats are physical, so the dome is a reduction, not a perfect absorb.")
			: RunId == TEXT("T10")
			? TEXT("Wave 1 conscripts throw spears from the dais lip. The threats are physical, so a kernel perfect absorb is not possible. 02-spear-flight is a spear in the air, not a perfect.")
			: TEXT("Wave 1 soldiers are physical. A kernel perfect absorb is not possible against them. 02-ward-meets-stone is a ward meeting a sling stone, not a perfect."));
	Root->SetNumberField(TEXT("sampleCount"), DrawCalls.Num());
	Root->SetObjectField(TEXT("drawCalls"), SpanInt(DrawCalls));
	Root->SetObjectField(TEXT("rhiPrimitivesDrawn"), SpanInt(MeasuredTriangles));
	Root->SetObjectField(TEXT("frameTimeMs"), SpanTime(FrameMs));
	Root->SetObjectField(TEXT("enemiesOnScreen"), SpanInt(EnemiesOnScreen));
	TArray<TSharedPtr<FJsonValue>> Shots;
	for (const FString& Name : ShotNames)
	{
		Shots.Add(MakeShared<FJsonValueString>(Name));
	}
	Root->SetArrayField(TEXT("shots"), Shots);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	const FString Path = RepoPath(*FString::Printf(TEXT("runs/%s/budget.json"), *RunId));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Text, *Path))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET write failed %s"), *Path);
		return;
	}
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET path=%s samples=%d"), *Path, DrawCalls.Num());
}

TStatId UWave1CaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWave1CaptureDriver, STATGROUP_Tickables);
}

bool UWave1CaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UWave1CaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UWave1CaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
