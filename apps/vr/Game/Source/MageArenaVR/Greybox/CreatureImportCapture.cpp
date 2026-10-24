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
// - The session stays idle (no bout), as in the greybox capture: the arena, the T56 dusk light, the imported meshes.
// - The camera stays on the seated view (centre pad, looking down the arena); an asset turns in place for its angles.
//   Every asset stands at its scene spot at opponent distance (CreatureImport::OpponentDistanceM, 8 m;
//   FCreatureSpec::SceneLateralM), never on the centre line, where the near-centre spawn marker's post stands.
// - Each mesh loops its imported Idle clip, so a pose is a moment of that clip. The hound's clip is held still from its
//   first shot until the cast still, so each glow shot and its mask shot show the same pose.
// - The glow candidates change only the hound's material parameters (a dynamic instance per slot): no tonemapper, no
//   exposure, no post-process change, so the rest of the T56 frame is the same in every shot.
// - 00 is the empty arena (every creature hidden): with a mask shot it parts the hound's pixels from the sand.

UCreatureImportCaptureDriver::UCreatureImportCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UCreatureImportCaptureDriver::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	// The candidates. Each value can be overridden on the command line, so a candidate is tuned without a rebuild.
	const CreatureImport::FEmberParams& Ember = CreatureImport::DefaultEmber();
	constexpr double BaseIntensity = 1.6; // bc630e0, CREATURE-UE-IMPORT section 4
	FGlow Intensity{TEXT("intensity"), 3.0, 0.0, 0.0};
	FGlow Rim{TEXT("rim"), BaseIntensity, 0.0, 0.8};
	FGlow Soft{TEXT("soft"), BaseIntensity, 1.0, 0.0};
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaGlowIntensity="), Intensity.Intensity);
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaGlowRim="), Rim.Rim);
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaGlowSoft="), Soft.Soft);
	FGlow Chosen{TEXT("chosen"), Ember.EmberIntensity, Ember.SoftIntensity, Ember.RimIntensity};
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaEmberIntensity="), Chosen.Intensity);
	Glows = {FGlow{TEXT("base"), BaseIntensity, 0.0, 0.0}, Intensity, Rim, Soft, Chosen};
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

ASkeletalMeshActor* UCreatureImportCaptureDriver::FindActor(const FString& Name) const
{
	const TConstArrayView<CreatureImport::FCreatureSpec> Specs = CreatureImport::Specs();
	for (int32 Index = 0; Index < Specs.Num() && Index < Actors.Num(); ++Index)
	{
		if (Name == Specs[Index].Name)
		{
			return Actors[Index].Get();
		}
	}
	return nullptr;
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

void UCreatureImportCaptureDriver::ApplyGlow(const FGlow& Glow)
{
	for (UMaterialInstanceDynamic* Dynamic : HoundMaterials)
	{
		Dynamic->SetScalarParameterValue(TEXT("EmberIntensity"), static_cast<float>(Glow.Intensity));
		Dynamic->SetScalarParameterValue(TEXT("SoftIntensity"), static_cast<float>(Glow.Soft));
		Dynamic->SetScalarParameterValue(TEXT("RimIntensity"), static_cast<float>(Glow.Rim));
	}
}

void UCreatureImportCaptureDriver::PoseShot(const FShotPlan& Plan)
{
	const FString Subject = Plan.Subject;
	const bool bCast = Subject == TEXT("cast");
	const TConstArrayView<CreatureImport::FCreatureSpec> Specs = CreatureImport::Specs();
	const double Distance = CreatureImport::OpponentDistanceM();
	for (int32 Index = 0; Index < Actors.Num(); ++Index)
	{
		ASkeletalMeshActor* Actor = Actors[Index].Get();
		const bool bShown = bCast || Subject == Specs[Index].Name;
		Actor->SetActorHiddenInGame(!bShown);
		if (bShown)
		{
			CreatureImport::PlaceCreature(*Actor, Layout, CreatureImport::StandPointCm(Layout, Distance, Specs[Index].SceneLateralM), Plan.YawOffsetDeg);
		}
	}
	const FGlow& Glow = Glows[Plan.Glow];
	ApplyGlow(Glow);
	if (ASkeletalMeshActor* Hound = FindActor(TEXT("CinderHound")))
	{
		USkeletalMeshComponent* Component = Hound->GetSkeletalMeshComponent();
		// The mask shot drops the shadow; every other shot keeps the spawn's shadow policy (CreatureImport::FCreatureBudget).
		const FVector HoundPoint = CreatureImport::StandPointCm(Layout, Distance, CreatureImport::FindSpec(TEXT("CinderHound"))->SceneLateralM);
		Component->SetCastShadow(!Plan.bMask && CreatureImport::CastsShadowAt(CreatureImport::BudgetFromCommandLine(), CreatureImport::DistanceFromViewM(Layout, HoundPoint)));
		// Held from the hound's first shot (a mask shot) through its glow shots; the cast still lets it breathe again.
		if (Subject == TEXT("CinderHound"))
		{
			Component->GlobalAnimRateScale = 0.f;
		}
		else if (bCast)
		{
			Component->GlobalAnimRateScale = 1.f;
		}
	}
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CREATURE_IMPORT pose %s subject=%s yawOffset=%.0f distance=%.1f m glow=%s intensity=%.2f soft=%.2f rim=%.2f mask=%d"), Plan.File,
	    Plan.Subject, Plan.YawOffsetDeg, Distance, Glow.Label, Glow.Intensity, Glow.Soft, Glow.Rim, Plan.bMask ? 1 : 0);
	Evidence.Note(FString::Printf(TEXT("glow.%s"), Plan.File), FString::Printf(TEXT("%s intensity=%.2f soft=%.2f rim=%.2f mask=%d"), Glow.Label, Glow.Intensity, Glow.Soft, Glow.Rim, Plan.bMask ? 1 : 0));
}

void UCreatureImportCaptureDriver::NoteMemory()
{
	// What the assets add to memory, as the engine sizes them (the D-G4 run reports the process working set). Read at
	// the end, once the textures have compiled and streamed: before that a texture is the 32x32 placeholder.
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
	// Glow indices: 0 base, 1 intensity, 2 rim, 3 soft, 4 chosen (the material's defaults).
	static const FShotPlan Plans[] = {
	    {TEXT("00-plate.png"), TEXT("none"), 0.0, 4, false},
	    {TEXT("01-maw-front.png"), TEXT("MireMaw"), 0.0, 4, false},
	    {TEXT("02-maw-three-quarter.png"), TEXT("MireMaw"), 45.0, 4, false},
	    {TEXT("03-maw-back.png"), TEXT("MireMaw"), 180.0, 4, false},
	    {TEXT("04-hound-mask-front.png"), TEXT("CinderHound"), 0.0, 0, true},
	    {TEXT("05-hound-glow-base-front.png"), TEXT("CinderHound"), 0.0, 0, false},
	    {TEXT("06-hound-glow-intensity-front.png"), TEXT("CinderHound"), 0.0, 1, false},
	    {TEXT("07-hound-glow-rim-front.png"), TEXT("CinderHound"), 0.0, 2, false},
	    {TEXT("08-hound-glow-soft-front.png"), TEXT("CinderHound"), 0.0, 3, false},
	    {TEXT("09-hound-glow-chosen-front.png"), TEXT("CinderHound"), 0.0, 4, false},
	    {TEXT("10-hound-mask-three-quarter.png"), TEXT("CinderHound"), 45.0, 0, true},
	    {TEXT("11-hound-glow-base-three-quarter.png"), TEXT("CinderHound"), 45.0, 0, false},
	    {TEXT("12-hound-glow-intensity-three-quarter.png"), TEXT("CinderHound"), 45.0, 1, false},
	    {TEXT("13-hound-glow-rim-three-quarter.png"), TEXT("CinderHound"), 45.0, 2, false},
	    {TEXT("14-hound-glow-soft-three-quarter.png"), TEXT("CinderHound"), 45.0, 3, false},
	    {TEXT("15-hound-glow-chosen-three-quarter.png"), TEXT("CinderHound"), 45.0, 4, false},
	    {TEXT("16-cast-duel.png"), TEXT("cast"), 0.0, 4, false},
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
			if (Scene.Num() != CreatureImport::Specs().Num())
			{
				Fail(TEXT("the imported meshes did not load"));
				break;
			}
			Actors.Reset();
			for (ASkeletalMeshActor* Actor : Scene)
			{
				Actors.Add(Actor);
				// Full-resolution textures in every still, not a streaming mip.
				Actor->GetSkeletalMeshComponent()->PrestreamTextures(600.f, true);
			}
			if (ASkeletalMeshActor* Hound = FindActor(TEXT("CinderHound")))
			{
				USkeletalMeshComponent* Component = Hound->GetSkeletalMeshComponent();
				for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
				{
					if (UMaterialInstanceDynamic* Dynamic = Component->CreateDynamicMaterialInstance(Slot))
					{
						HoundMaterials.Add(Dynamic);
					}
				}
			}
			for (const FGlow& Glow : Glows)
			{
				Evidence.Note(FString::Printf(TEXT("glowCandidate.%s"), Glow.Label), FString::Printf(TEXT("intensity=%.2f soft=%.2f rim=%.2f"), Glow.Intensity, Glow.Soft, Glow.Rim));
			}
			Evidence.Note(TEXT("opponentDistanceM"), FString::Printf(TEXT("%.2f"), CreatureImport::OpponentDistanceM()));
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
