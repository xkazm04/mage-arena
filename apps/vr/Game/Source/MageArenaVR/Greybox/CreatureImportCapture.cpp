#include "Greybox/CreatureImportCapture.h"

#include "Animation/SkeletalMeshActor.h"
#include "AssetCompilingManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Greybox/CreatureImport.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/MageArenaPawn.h"
#include "MageArenaVR.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIStats.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

// Staging, stated so the stills are read for what they are:
// - The session stays idle (no bout), as in the greybox capture: the arena, the T56 dusk light, the two imported meshes.
// - The camera stays on the seated view (centre pad, looking down the arena); the asset turns in place for the three
//   angles. Each asset stands alone at opponent distance (CreatureImport::OpponentDistanceM, 8 m) for its turn, at its
//   spot of the side-by-side still (1.4 m left for Brennic, right for the hound): the centre line holds the
//   near-centre spawn marker's post, which stood through the meshes in the first stills.
// - Each mesh loops its imported Idle clip, so a pose is a moment of that clip, not the bind pose.
// - 07 brings the hound to 5 m to show the ember glow up close (at 3.5 m the dais edge hid its front half); the pawn
//   re-applies its FOV every tick, so a zoom is not available and the glow at duel distance is read in 04.

namespace
{
constexpr double NearDistanceM = 5.0;
}

UCreatureImportCaptureDriver::UCreatureImportCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UCreatureImportCaptureDriver::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
	Evidence.Begin(TEXT("CreatureImport"));
	if (!MageArenaCapture::CanCapture())
	{
		Fail(TEXT("no rhi"));
	}
}

void UCreatureImportCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	Frames = 0;
}

AMageArenaPawn* UCreatureImportCaptureDriver::FindPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AMageArenaPawn> It(World);
	return It ? *It : nullptr;
}

void UCreatureImportCaptureDriver::Fail(const FString& Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), *Reason);
	Enter(EStep::Quit);
}

void UCreatureImportCaptureDriver::RequestShot(const TCHAR* FileName)
{
	const FString Path = MageArenaCapture::RepoPath(FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	bShotOpen = true;
	Evidence.Requested(FileName, Path, -1, -1, TEXT("idle"), -1, FPlatformTime::Seconds());
	++ShotCount;
	// The previous frame's RHI counters: the scene as posed for this shot (Settle waits at least 20 frames).
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s drawCalls=%d rhiPrimitives=%d path=%s"), FileName, GNumDrawCallsRHI[0], GNumPrimitivesDrawnRHI[0], *Path);
	Evidence.Note(FString::Printf(TEXT("counters.%s"), FileName), FString::Printf(TEXT("drawCalls=%d rhiPrimitives=%d"), GNumDrawCallsRHI[0], GNumPrimitivesDrawnRHI[0]));
}

void UCreatureImportCaptureDriver::PoseShot(const FShotPlan& Plan)
{
	const FString Subject = Plan.Subject;
	const bool bBoth = Subject == TEXT("both");
	if (bBoth)
	{
		const double Distance = CreatureImport::OpponentDistanceM();
		CreatureImport::PlaceCreature(*Brennic, Layout, CreatureImport::StandPointCm(Layout, Distance, -CreatureImport::SceneHalfSpacingM), Plan.YawOffsetDeg);
		CreatureImport::PlaceCreature(*Hound, Layout, CreatureImport::StandPointCm(Layout, Distance, CreatureImport::SceneHalfSpacingM), Plan.YawOffsetDeg);
	}
	else
	{
		ASkeletalMeshActor* Actor = Subject == TEXT("Brennic") ? Brennic.Get() : Hound.Get();
		const double Distance = Plan.DistanceM > 0.0 ? Plan.DistanceM : CreatureImport::OpponentDistanceM();
		const double Lateral = Actor == Brennic.Get() ? -CreatureImport::SceneHalfSpacingM : CreatureImport::SceneHalfSpacingM;
		CreatureImport::PlaceCreature(*Actor, Layout, CreatureImport::StandPointCm(Layout, Distance, Lateral), Plan.YawOffsetDeg);
	}
	Brennic->SetActorHiddenInGame(!bBoth && Subject != TEXT("Brennic"));
	Hound->SetActorHiddenInGame(!bBoth && Subject != TEXT("CinderHound"));
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CREATURE_IMPORT pose %s subject=%s yawOffset=%.0f distance=%.1f m"), Plan.File, Plan.Subject, Plan.YawOffsetDeg,
	    Plan.DistanceM > 0.0 ? Plan.DistanceM : CreatureImport::OpponentDistanceM());
}

void UCreatureImportCaptureDriver::NoteMemory()
{
	// What the two assets add to memory, as the engine sizes them (the D-G4 run reports the process working set). Read
	// at the end, once the textures have compiled and streamed: before that a texture is the 32x32 placeholder.
	for (const CreatureImport::FCreatureSpec& Spec : CreatureImport::Specs())
	{
		const UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *CreatureImport::TextureObjectPath(Spec.Name));
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *CreatureImport::MeshObjectPath(Spec.Name));
		const double TextureMB = Texture ? Texture->CalcTextureMemorySizeEnum(TMC_AllMips) / (1024.0 * 1024.0) : -1.0;
		const double MeshMB = Mesh ? Mesh->GetResourceSizeBytes(EResourceSizeMode::Exclusive) / (1024.0 * 1024.0) : -1.0;
		const FString Line = FString::Printf(TEXT("texture %dx%d format=%s all-mips %.2f MB; mesh exclusive %.2f MB"), Texture ? Texture->GetSizeX() : 0,
		    Texture ? Texture->GetSizeY() : 0, Texture ? GetPixelFormatString(Texture->GetPixelFormat()) : TEXT("-"), TextureMB, MeshMB);
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CREATURE_IMPORT memory %s %s"), Spec.Name, *Line);
		Evidence.Note(FString::Printf(TEXT("memory.%s"), Spec.Name), Line);
	}
}

bool UCreatureImportCaptureDriver::TexturesResident() const
{
	for (const CreatureImport::FCreatureSpec& Spec : CreatureImport::Specs())
	{
		UTexture2D* Texture = FindObject<UTexture2D>(nullptr, *CreatureImport::TextureObjectPath(Spec.Name));
		if (Texture && !Texture->IsFullyStreamedIn())
		{
			return false;
		}
	}
	return true;
}

void UCreatureImportCaptureDriver::Tick(float DeltaTime)
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
		}
	}
	Advance();
}

void UCreatureImportCaptureDriver::Advance()
{
	static const FShotPlan Plans[] = {
	    {TEXT("01-brennic-front.png"), TEXT("Brennic"), 0.0, 0.0},
	    {TEXT("02-brennic-three-quarter.png"), TEXT("Brennic"), 45.0, 0.0},
	    {TEXT("03-brennic-back.png"), TEXT("Brennic"), 180.0, 0.0},
	    {TEXT("04-hound-front.png"), TEXT("CinderHound"), 0.0, 0.0},
	    {TEXT("05-hound-three-quarter.png"), TEXT("CinderHound"), 45.0, 0.0},
	    {TEXT("06-hound-back.png"), TEXT("CinderHound"), 180.0, 0.0},
	    {TEXT("07-hound-glow-near.png"), TEXT("CinderHound"), 30.0, NearDistanceM},
	    {TEXT("08-both-duel.png"), TEXT("both"), 0.0, 0.0},
	};
	constexpr int32 PlanCount = UE_ARRAY_COUNT(Plans);

	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::Quit && Step != EStep::Finish && Now - RunStartWall > 600.0)
	{
		Fail(TEXT("timeout"));
		return;
	}
	++Frames;
	AMageArenaPawn* Pawn = FindPawn();
	// Shaders, textures and meshes still compiling (the editor binary compiles derived data on load in -game).
	const bool bShadersIdle = (GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling()) && FAssetCompilingManager::Get().GetNumRemainingAssets() == 0;

	switch (Step)
	{
	case EStep::WaitStable:
	{
		const float Delta = FApp::GetDeltaTime();
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
			FString Error;
			if (!Pawn || !Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error))
			{
				Fail(FString::Printf(TEXT("pawn or layout missing %s"), *Error));
				break;
			}
			const TArray<ASkeletalMeshActor*> Scene = CreatureImport::SpawnScene(*GetWorld(), Layout);
			if (Scene.Num() != 2)
			{
				Fail(TEXT("the imported meshes did not load"));
				break;
			}
			Brennic = Scene[0];
			Hound = Scene[1];
			double Intensity = CreatureImport::DefaultEmber().EmberIntensity;
			if (FParse::Value(FCommandLine::Get(), TEXT("MageArenaEmberIntensity="), Intensity))
			{
				USkeletalMeshComponent* Component = Hound->GetSkeletalMeshComponent();
				if (UMaterialInstanceDynamic* Dynamic = Component->CreateDynamicMaterialInstance(0))
				{
					Dynamic->SetScalarParameterValue(TEXT("EmberIntensity"), static_cast<float>(Intensity));
				}
			}
			Evidence.Note(TEXT("emberIntensity"), FString::Printf(TEXT("%.2f"), Intensity));
			Evidence.Note(TEXT("opponentDistanceM"), FString::Printf(TEXT("%.2f"), CreatureImport::OpponentDistanceM()));
			for (ASkeletalMeshActor* Actor : Scene)
			{
				// Full-resolution textures in every still, not a streaming mip.
				Actor->GetSkeletalMeshComponent()->PrestreamTextures(600.f, true);
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			Pawn->SetLookPitch(-4.f);
			if (GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("r.MotionBlurQuality 0"));
			}
			ShotIndex = 0;
			Enter(EStep::Pose);
		}
		break;
	}
	case EStep::Pose:
		if (ShotIndex >= PlanCount)
		{
			Enter(EStep::Finish);
			break;
		}
		PoseShot(Plans[ShotIndex]);
		Enter(EStep::Settle);
		break;
	case EStep::Settle:
		// The new pose's shaders (the first frame of each mesh compiles its material) and shadows settle first.
		if (Frames >= 20 && Elapsed >= 1.0 && bShadersIdle && TexturesResident())
		{
			RequestShot(Plans[ShotIndex].File);
			++ShotIndex;
			Enter(EStep::Pose);
		}
		else if (Elapsed > 300.0)
		{
			Fail(TEXT("shaders or textures never settled"));
		}
		break;
	case EStep::Finish:
		if (ShotCount >= PlanCount)
		{
			NoteMemory();
			FString ManifestError;
			if (Evidence.WriteManifest(MageArenaCapture::RepoPath(FString::Printf(TEXT("runs/%s/shots/manifest.json"), *RunId)), ManifestError))
			{
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE_DONE shots=%d"), ShotCount);
			}
			else
			{
				UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), *ManifestError);
			}
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

TStatId UCreatureImportCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCreatureImportCaptureDriver, STATGROUP_Tickables);
}

bool UCreatureImportCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UCreatureImportCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UCreatureImportCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
