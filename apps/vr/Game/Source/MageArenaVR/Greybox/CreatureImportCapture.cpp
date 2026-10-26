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
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHIStats.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Session/ArenaSession.h"
#include "Session/RivalFigure.h"
#include "Session/SessionPresentation.h"
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
	if (FParse::Param(FCommandLine::Get(), TEXT("MageArenaRivalReplay")))
	{
		RivalReplay = NewObject<URivalReplayCaptureDriver>(this);
		RivalReplay->Start();
		return;
	}
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
				// The "chosen" shots show the COMMITTED material (T69): its own parameter values, read before any shot overrides
				// them, not the C++ defaults it was built from. -MageArenaEmberIntensity still wins.
				if (HoundMaterials.Num() > 0)
				{
					float Ember = 0.f;
					float Soft = 0.f;
					float Rim = 0.f;
					float Radius = 0.f;
					UMaterialInstanceDynamic* First = HoundMaterials[0];
					const bool bRead = First->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("EmberIntensity")), Ember)
					    && First->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("SoftIntensity")), Soft)
					    && First->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("RimIntensity")), Rim);
					First->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("SoftRadiusUv")), Radius);
					UE_LOG(LogMageArena, Display, TEXT("MAGEVR_CREATURE_IMPORT committed %s ember=%.3f soft=%.3f rim=%.3f softRadiusUv=%.4f read=%d (C++ defaults ember=%.3f soft=%.3f rim=%.3f softRadiusUv=%.4f)"),
					    *First->GetBaseMaterial()->GetName(), Ember, Soft, Rim, Radius, bRead ? 1 : 0, CreatureImport::DefaultEmber().EmberIntensity, CreatureImport::DefaultEmber().SoftIntensity,
					    CreatureImport::DefaultEmber().RimIntensity, CreatureImport::DefaultEmber().SoftRadiusUv);
					Evidence.Note(TEXT("committedMaterial"), FString::Printf(TEXT("ember=%.3f soft=%.3f rim=%.3f softRadiusUv=%.4f read=%d"), Ember, Soft, Rim, Radius, bRead ? 1 : 0));
					double EmberOverride = -1.0;
					FParse::Value(FCommandLine::Get(), TEXT("MageArenaEmberIntensity="), EmberOverride);
					if (bRead && Glows.IsValidIndex(4))
					{
						Glows[4] = FGlow{TEXT("chosen"), EmberOverride >= 0.0 ? EmberOverride : static_cast<double>(Ember), static_cast<double>(Soft), static_cast<double>(Rim)};
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

// T38 rival replay. Staging, stated so the frames are read for what they are:
// - The creatures: the session is idle (no bout) while the hound and the maw are stilled at their 8 m spots, as in the
//   creature cast stills, with the chosen glow (the material's defaults). Brennic is not in them: he is the duel's.
// - The duel starts through UArenaSessionSubsystem::Start, the path the game uses (as the T16 style-pick replay does):
//   bout 3, Brennic's semifinal (SetBout(2, true)), seed 1, the scripted seat. The session presentation draws him, and
//   its rival figure picks his clip from the kernel's events alone; this driver only reads which clip plays.
// - The hound and the maw stand beside him, 12 m out (DuelBehindM), looping Idle: neither is a kernel actor of this bout.
// - The seated camera of the duel (LookAtArena, pitch -4 deg) never moves to follow him; the crops are cut afterwards.
namespace
{
// Frames after a clip starts (60 per second under -usefixedtimestep -fps=60). Idle loops (2.0 s), so the first second
// is enough; Cast plays 1.53 s and Hit 0.8 s; Death plays 2.0 s and then holds, shot at 3 s and 5 s.
const TArray<int32>& OffsetsFor(ERivalClip Clip)
{
	static const TArray<int32> Idle = {6, 24, 42, 60};
	static const TArray<int32> Cast = {6, 24, 42, 60, 78};
	static const TArray<int32> Hit = {4, 16, 28, 40};
	static const TArray<int32> Death = {6, 30, 54, 78, 102, 120, 180, 300};
	switch (Clip)
	{
	case ERivalClip::Cast:
		return Cast;
	case ERivalClip::Hit:
		return Hit;
	case ERivalClip::Death:
		return Death;
	default:
		return Idle;
	}
}

// A clip cut off before its set is shot is tried again on its next start, up to this many times. Hits come 17 to 20
// ticks apart in a flurry and each one restarts the Hit, so a full Hit set waits for a hit nothing follows for 0.7 s
// (the seed-1 duel has its first at tick 1010, after 15 cut ones).
constexpr int32 MaxAttempts = 64;
// The longest duel the replay waits for (s of engine time), well past the recorded chains' longest bout.
constexpr double DuelBudgetS = 180.0;
// Duel frames before a clip set may start (see StepDuel).
constexpr int32 FigureWarmFrames = 30;
// Where the hound and the maw stand during the duel (m ahead of the centre pad; m to the seated player's right).
constexpr double DuelBehindM = 12.0;
constexpr double DuelLateralM[] = {3.0, -6.0};
}

URivalReplayCaptureDriver::URivalReplayCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void URivalReplayCaptureDriver::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	FString SeedList = TEXT("1,2");
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRivalSeeds="), SeedList, false);
	TArray<FString> SeedTexts;
	SeedList.ParseIntoArray(SeedTexts, TEXT(","));
	for (const FString& Text : SeedTexts)
	{
		Seeds.Add(static_cast<uint32>(FCString::Atoi(*Text)));
	}
	if (Seeds.Num() == 0)
	{
		Seeds.Add(1);
	}
	Sets.SetNum(RivalClips::Count);
	for (int32 Index = 0; Index < RivalClips::Count; ++Index)
	{
		Sets[Index].Offsets = OffsetsFor(static_cast<ERivalClip>(Index));
		Sets[Index].Wanted = static_cast<ERivalClip>(Index) == ERivalClip::Death ? 1 : 3;
	}
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
	Evidence.Begin(TEXT("RivalReplay"));
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_REPLAY start run=%s"), *RunId);
	if (!MageArenaCapture::CanCapture())
	{
		Fail(TEXT("no rhi"));
		return;
	}
	if (!FApp::UseFixedTimeStep() || !FMath::IsNearlyEqual(FApp::GetFixedDeltaTime(), 1.0 / 60.0, 1.0e-6))
	{
		Fail(FString::Printf(TEXT("a frame is a kernel tick only under -usefixedtimestep -fps=60 (fixed=%d dt=%.6f)"), FApp::UseFixedTimeStep() ? 1 : 0,
		    FApp::GetFixedDeltaTime()));
	}
}

void URivalReplayCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	Frames = 0;
}

void URivalReplayCaptureDriver::Fail(const FString& Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), *Reason);
	Enter(EStep::Quit);
}

UArenaSessionSubsystem* URivalReplayCaptureDriver::SessionSys() const
{
	UWorld* World = GetTickableGameObjectWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

FString URivalReplayCaptureDriver::CropOfFigure() const
{
	// The figure's bounds projected into the shot, as x y w h in its pixels: a square around him, for the crop sheets.
	const UArenaSessionSubsystem* Sys = SessionSys();
	const ASessionPresentation* Presentation = Sys ? Sys->GetPresentation() : nullptr;
	const URivalFigureComponent* Figure = Presentation ? Presentation->GetRivalFigure() : nullptr;
	UWorld* World = GetTickableGameObjectWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Figure || !Figure->IsShown() || !Controller)
	{
		return FString();
	}
	const FBox Box = Figure->GetMesh()->Bounds.GetBox();
	int32 ViewX = 0;
	int32 ViewY = 0;
	Controller->GetViewportSize(ViewX, ViewY);
	int32 ResX = 1920;
	int32 ResY = 1080;
	FParse::Value(FCommandLine::Get(), TEXT("ResX="), ResX);
	FParse::Value(FCommandLine::Get(), TEXT("ResY="), ResY);
	FBox2D Screen(ForceInit);
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Point((Corner & 1) ? Box.Max.X : Box.Min.X, (Corner & 2) ? Box.Max.Y : Box.Min.Y, (Corner & 4) ? Box.Max.Z : Box.Min.Z);
		FVector2D Pixel;
		if (!Controller->ProjectWorldLocationToScreen(Point, Pixel, true))
		{
			return FString();
		}
		if (ViewX > 0 && ViewY > 0)
		{
			Pixel.X *= static_cast<double>(ResX) / ViewX;
			Pixel.Y *= static_cast<double>(ResY) / ViewY;
		}
		Screen += Pixel;
	}
	const FVector2D Centre = Screen.GetCenter();
	const int32 Side = FMath::Clamp(FMath::RoundToInt(FMath::Max(Screen.GetSize().X, Screen.GetSize().Y) * 1.3), 64, FMath::Min(ResX, ResY));
	const int32 X = FMath::Clamp(FMath::RoundToInt(Centre.X - Side * 0.5), 0, ResX - Side);
	const int32 Y = FMath::Clamp(FMath::RoundToInt(Centre.Y - Side * 0.5), 0, ResY - Side);
	return FString::Printf(TEXT("%d %d %d %d"), X, Y, Side, Side);
}

void URivalReplayCaptureDriver::RequestShot(const FString& Name, const FString& Stage)
{
	const FString Path = MageArenaCapture::RepoPath(FString::Printf(TEXT("runs/%s/%s"), *RunId, *Name));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	const UArenaSessionSubsystem* Sys = SessionSys();
	if (Sys && Sys->GetSession().IsRunning())
	{
		Evidence.Requested(Name, Path, Sys);
	}
	else
	{
		Evidence.Requested(Name, Path, -1, -1, Stage, -1, FPlatformTime::Seconds());
	}
	const FString Crop = CropOfFigure();
	if (!Crop.IsEmpty())
	{
		Evidence.Note(FString::Printf(TEXT("crop.%s"), *Name), Crop);
	}
	Evidence.Note(FString::Printf(TEXT("counters.%s"), *Name), FString::Printf(TEXT("drawCalls=%d rhiPrimitives=%d"), GNumDrawCallsRHI[0], GNumPrimitivesDrawnRHI[0]));
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s stage=%s crop=%s drawCalls=%d path=%s"), *Name, *Stage, Crop.IsEmpty() ? TEXT("-") : *Crop, GNumDrawCallsRHI[0], *Path);
}

bool URivalReplayCaptureDriver::StartDuel()
{
	UArenaSessionSubsystem* Sys = SessionSys();
	if (!Sys)
	{
		Fail(TEXT("no session subsystem"));
		return false;
	}
	FArenaSession& Session = Sys->GetSession();
	const uint32 Seed = Seeds[SeedIndex];
	Session.SetSaveDirectory(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("rival-replay")));
	Session.SetBout(2, true);
	if (!Sys->Start(Seed, true))
	{
		Fail(FString::Printf(TEXT("the Brennic duel did not start (bout 3, seed %u, scripted)"), Seed));
		return false;
	}
	const FVrRuleset* Rules = Session.GetVrRules();
	const FVrRival* Rival = Rules ? Rules->BoundRival() : nullptr;
	Evidence.Note(FString::Printf(TEXT("duel.%d"), SeedIndex + 1),
	    FString::Printf(TEXT("bout=3 seed=%u scripted=1 rival=%s actor=%d"), Seed, Rival ? *Rival->Id : TEXT("-"), Rules ? Rules->RivalRuntime.ActorId : -1));
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_REPLAY duel seed=%u rival=%s"), Seed, Rival ? *Rival->Id : TEXT("-"));
	if (!Rival || Rival->Id != RivalClips::BrennicRivalId)
	{
		Fail(FString::Printf(TEXT("the duel's rival is %s, not Brennic"), Rival ? *Rival->Id : TEXT("none")));
		return false;
	}
	if (Timeline.IsEmpty())
	{
		Timeline = TEXT("seed\tframe\ttick\tsimS\tclip\tstarts\tphase\n");
	}
	DuelFrame = 0;
	SeenStarts = -1;
	DownFrame = -1;
	EndedFrame = -1;
	for (FClipSet& Set : Sets)
	{
		Set.Starts = -1;
	}
	return true;
}

void URivalReplayCaptureDriver::StepDuel()
{
	UArenaSessionSubsystem* Sys = SessionSys();
	ASessionPresentation* Presentation = Sys ? Sys->GetPresentation() : nullptr;
	if (!Sys || !Presentation)
	{
		Fail(TEXT("no session presentation"));
		return;
	}
	FArenaSession& Session = Sys->GetSession();
	const FGames& Games = Session.GetGames();
	++DuelFrame;
	const URivalFigureComponent* Figure = Presentation->GetRivalFigure();
	if (!Figure)
	{
		// The presentation tries the import on the first frame it draws the duel.
		if (DuelFrame > 30)
		{
			Fail(TEXT("the rival figure did not load (Content/Characters/Brennic is missing or did not load)"));
		}
		return;
	}
	const ERivalClip Clip = Figure->GetClip();
	const int32 Starts = Figure->GetClipStarts();
	if (Starts != SeenStarts)
	{
		SeenStarts = Starts;
		Timeline += FString::Printf(TEXT("%u\t%d\t%d\t%.3f\t%s\t%d\t%s\n"), Seeds[SeedIndex], DuelFrame, Games.State.Tick, Session.GetSimSeconds(), RivalClips::Name(Clip), Starts,
		    *Games.Phase);
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_REPLAY clip=%s seed=%u frame=%d tick=%d sim=%.3f phase=%s"), RivalClips::Name(Clip), Seeds[SeedIndex], DuelFrame,
		    Games.State.Tick, Session.GetSimSeconds(), *Games.Phase);
		// A set in progress whose clip was cut off is tried again on that clip's next start.
		for (FClipSet& Each : Sets)
		{
			if (!Each.bDone && Each.Starts >= 0)
			{
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_REPLAY set cut after %d of %d frames"), Each.Next, Each.Offsets.Num());
				Each.Starts = -1;
			}
		}
		FClipSet& Started = Sets[static_cast<int32>(Clip)];
		// The figure is made on the duel's first frame and draws grey while its material compiles (the first run's first
		// Cast frame), so no set starts before it has stood FigureWarmFrames with the shader compiler idle.
		const bool bWarm = DuelFrame > FigureWarmFrames && (GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling());
		if (bWarm && !Started.bDone && Started.Attempt < MaxAttempts)
		{
			++Started.Attempt;
			Started.Starts = Starts;
			Started.StartFrame = DuelFrame;
			Started.Next = 0;
		}
		if (Clip == ERivalClip::Death && DownFrame < 0)
		{
			DownFrame = DuelFrame;
			// The session runs on into its intermission, as in the game: the figure holds Death where he fell, and the
			// shots and wards still in flight clear. -MageArenaRivalHold=1 holds the kernel at the down instead, which
			// freezes them in front of the seat (the first seed-2 run: the player's ward hid him in every Death frame).
			int32 Hold = 0;
			FParse::Value(FCommandLine::Get(), TEXT("MageArenaRivalHold="), Hold);
			Session.SetFrameHold(Hold != 0);
			Evidence.Note(TEXT("down"), FString::Printf(TEXT("seed=%u frame=%d tick=%d sim=%.3f phase=%s kernelHeld=%d"), Seeds[SeedIndex], DuelFrame, Games.State.Tick, Session.GetSimSeconds(), *Games.Phase, Hold != 0 ? 1 : 0));
		}
	}
	FClipSet& Set = Sets[static_cast<int32>(Clip)];
	if (!Set.bDone && Set.Starts == Starts && Set.Offsets.IsValidIndex(Set.Next) && DuelFrame - Set.StartFrame >= Set.Offsets[Set.Next] &&
	    !FScreenshotRequest::IsScreenshotRequested())
	{
		const FString Lower = FString(RivalClips::Name(Clip)).ToLower();
		const FString Name = FString::Printf(TEXT("rival/%s/seed%u-%s-a%d-%02d.png"), *Lower, Seeds[SeedIndex], *Lower, Set.Attempt, Set.Next + 1);
		RequestShot(Name, Games.Phase);
		Evidence.Note(FString::Printf(TEXT("clip.%s"), *Name), FString::Printf(TEXT("%s seed=%u attempt=%d frame=%d of the clip (wanted %d) tick=%d"), RivalClips::Name(Clip), Seeds[SeedIndex], Set.Attempt, DuelFrame - Set.StartFrame, Set.Offsets[Set.Next], Games.State.Tick));
		++Set.Next;
		if (Set.Next >= Set.Offsets.Num())
		{
			++Set.Completed;
			Set.bDone = Set.Completed >= Set.Wanted;
			Set.Starts = -1;
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_REPLAY set done clip=%s attempt=%d full=%d of %d"), RivalClips::Name(Clip), Set.Attempt, Set.Completed, Set.Wanted);
		}
	}

	// Death comes last (it ends the last duel); every other clip needs one full set by then.
	bool bAllShot = Sets[static_cast<int32>(ERivalClip::Death)].bDone;
	for (const FClipSet& Each : Sets)
	{
		bAllShot &= Each.Completed > 0;
	}
	if (bAllShot)
	{
		Enter(EStep::Finish);
		return;
	}
	// The duel ended without Brennic down, or Death is shot while another clip never played through.
	if (EndedFrame < 0 && (Sets[static_cast<int32>(ERivalClip::Death)].bDone || (DownFrame < 0 && Games.Phase != TEXT("active"))))
	{
		EndedFrame = DuelFrame;
	}
	if (EndedFrame >= 0 && DuelFrame - EndedFrame > 60)
	{
		if (SeedIndex + 1 < Seeds.Num())
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_REPLAY duel seed=%u ended phase=%s down=%d; next seed=%u"), Seeds[SeedIndex], *Games.Phase, DownFrame >= 0 ? 1 : 0,
			    Seeds[SeedIndex + 1]);
			Evidence.Note(FString::Printf(TEXT("duel.%d.end"), SeedIndex + 1), FString::Printf(TEXT("phase=%s tick=%d sim=%.3f down=%d"), *Games.Phase, Games.State.Tick, Session.GetSimSeconds(), DownFrame >= 0 ? 1 : 0));
			++SeedIndex;
			StartDuel();
			return;
		}
		TArray<FString> Missing;
		for (int32 Index = 0; Index < Sets.Num(); ++Index)
		{
			if (Sets[Index].Completed == 0)
			{
				Missing.Add(FString::Printf(TEXT("%s (%d tries)"), RivalClips::Name(static_cast<ERivalClip>(Index)), Sets[Index].Attempt));
			}
		}
		Fail(FString::Printf(TEXT("the last duel ended (phase %s, Brennic down=%d) before every clip played through: missing %s"), *Games.Phase, DownFrame >= 0 ? 1 : 0,
		    *FString::Join(Missing, TEXT(", "))));
		return;
	}
	if (DuelFrame > DuelBudgetS * 60.0)
	{
		Fail(FString::Printf(TEXT("the duel ran past %.0f s"), DuelBudgetS));
	}
}

void URivalReplayCaptureDriver::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (!bRunning)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Step != EStep::Quit && Step != EStep::Finish && Now - RunStartWall > 1500.0)
	{
		Fail(TEXT("timeout"));
	}
	const ECaptureShotPoll Shots = Evidence.Poll();
	if (Shots == ECaptureShotPoll::Failed && Step != EStep::Quit)
	{
		Fail(Evidence.GetFailure());
	}
	++Frames;
	UWorld* World = GetTickableGameObjectWorld();
	AMageArenaPawn* Pawn = nullptr;
	if (World)
	{
		TActorIterator<AMageArenaPawn> It(World);
		Pawn = It ? *It : nullptr;
	}
	const bool bShadersIdle = (GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling()) && FAssetCompilingManager::Get().GetNumRemainingAssets() == 0;
	const double Elapsed = Now - StepStartWall;
	static const TCHAR* const CreatureShots[] = {TEXT("creatures/01-hound-8m.png"), TEXT("creatures/02-maw-8m.png"), TEXT("creatures/03-hound-and-maw-8m.png")};
	constexpr int32 CreatureShotCount = UE_ARRAY_COUNT(CreatureShots);

	switch (Step)
	{
	case EStep::WaitStable:
	{
		const float Delta = FApp::GetDeltaTime();
		StableFrames = (Pawn && Pawn->HasLayout() && bShadersIdle && Delta > 0.f) ? StableFrames + 1 : 0;
		if ((StableFrames >= 20 && Elapsed >= 1.5) || Elapsed >= 90.0)
		{
			FString Error;
			if (!Pawn || !World || !Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error))
			{
				Fail(FString::Printf(TEXT("pawn or layout missing %s"), *Error));
				break;
			}
			const double Distance = CreatureImport::OpponentDistanceM();
			for (const CreatureImport::FCreatureSpec& Spec : CreatureImport::Specs())
			{
				if (FString(Spec.Name) == RivalClips::BrennicAsset)
				{
					continue;
				}
				ASkeletalMeshActor* Actor = CreatureImport::SpawnCreature(*World, Layout, Spec.Name, CreatureImport::StandPointCm(Layout, Distance, Spec.SceneLateralM));
				if (!Actor)
				{
					Fail(FString::Printf(TEXT("%s did not load (run its import first)"), Spec.Name));
					return;
				}
				Actor->GetSkeletalMeshComponent()->PrestreamTextures(600.f, true);
				Creatures.Add(Actor);
			}
			Evidence.Note(TEXT("opponentDistanceM"), FString::Printf(TEXT("%.2f"), Distance));
			// Brennic's texture at full resolution from his first duel frame (the first run's first Cast frame drew him
			// grey, before it streamed in); the creature stills wait for it with their own.
			BrennicTexture = LoadObject<UTexture2D>(nullptr, *CreatureImport::TextureObjectPath(RivalClips::BrennicAsset));
			if (BrennicTexture)
			{
				BrennicTexture->SetForceMipLevelsToBeResident(600.f);
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			Pawn->SetLookPitch(-4.f);
			if (GEngine)
			{
				GEngine->Exec(World, TEXT("r.MotionBlurQuality 0"));
			}
			CreatureShot = 0;
			Enter(EStep::PoseCreatures);
		}
		break;
	}
	case EStep::PoseCreatures:
		if (CreatureShot >= CreatureShotCount)
		{
			// For the duel they step back behind the fight: Brennic ranges over the near ring (he fell at about 2.6 m left
			// of the centre line at 8 m in the seed-2 duel, in front of the maw's 8 m spot), so his body would read
			// against the maw's.
			for (int32 Index = 0; Index < Creatures.Num(); ++Index)
			{
				Creatures[Index]->SetActorHiddenInGame(false);
				CreatureImport::PlaceCreature(*Creatures[Index], Layout, CreatureImport::StandPointCm(Layout, DuelBehindM, DuelLateralM[FMath::Min(Index, 1)]), 0.0);
			}
			if (StartDuel())
			{
				Enter(EStep::Duel);
			}
			break;
		}
		// 01 the hound alone, 02 the maw alone, 03 both (Creatures holds them in Specs() order: hound, maw).
		for (int32 Index = 0; Index < Creatures.Num(); ++Index)
		{
			Creatures[Index]->SetActorHiddenInGame(!(CreatureShot == 2 || CreatureShot == Index));
		}
		Enter(EStep::SettleCreatures);
		break;
	case EStep::SettleCreatures:
	{
		bool bResident = true;
		for (const CreatureImport::FCreatureSpec& Spec : CreatureImport::Specs())
		{
			UTexture2D* Texture = FindObject<UTexture2D>(nullptr, *CreatureImport::TextureObjectPath(Spec.Name));
			bResident &= !Texture || Texture->IsFullyStreamedIn();
		}
		if (Frames >= 20 && Elapsed >= 1.0 && bShadersIdle && bResident && Shots != ECaptureShotPoll::Pending)
		{
			RequestShot(CreatureShots[CreatureShot], TEXT("idle"));
			++CreatureShot;
			Enter(EStep::PoseCreatures);
		}
		else if (Elapsed > 300.0)
		{
			Fail(TEXT("shaders or textures never settled"));
		}
		break;
	}
	case EStep::Duel:
		StepDuel();
		break;
	case EStep::Finish:
	{
		if (Shots == ECaptureShotPoll::Pending)
		{
			break;
		}
		const FString Dir = MageArenaCapture::RepoPath(FString::Printf(TEXT("runs/%s"), *RunId));
		FFileHelper::SaveStringToFile(Timeline, *FPaths::Combine(Dir, TEXT("rival/clip-timeline.tsv")));
		const UArenaSessionSubsystem* Sys = SessionSys();
		const ASessionPresentation* Presentation = Sys ? Sys->GetPresentation() : nullptr;
		if (const URivalFigureComponent* Figure = Presentation ? Presentation->GetRivalFigure() : nullptr)
		{
			Evidence.Note(TEXT("lod0Triangles.Brennic"), FString::FromInt(Figure->GetLod0Triangles()));
		}
		for (const ASkeletalMeshActor* Actor : Creatures)
		{
			const USkeletalMesh* Mesh = Actor->GetSkeletalMeshComponent()->GetSkeletalMeshAsset();
			const FSkeletalMeshRenderData* Render = Mesh ? Mesh->GetResourceForRendering() : nullptr;
			Evidence.Note(FString::Printf(TEXT("lod0Triangles.%s"), Mesh ? *Mesh->GetName() : TEXT("-")),
			    FString::FromInt(Render && Render->LODRenderData.Num() > 0 ? Render->LODRenderData[0].GetTotalFaces() : 0));
		}
		FString Error;
		if (Evidence.WriteManifest(FPaths::Combine(Dir, TEXT("manifest.json")), Error))
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE_DONE shots=%d"), Evidence.GetManifest().Shots.Num());
		}
		else
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), *Error);
		}
		Enter(EStep::Quit);
		break;
	}
	case EStep::Quit:
		bRunning = false;
		SetTickableTickType(ETickableTickType::Never);
		if (World)
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

TStatId URivalReplayCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URivalReplayCaptureDriver, STATGROUP_Tickables);
}

bool URivalReplayCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* URivalReplayCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void URivalReplayCaptureDriver::BeginDestroy()
{
	bRunning = false;
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
