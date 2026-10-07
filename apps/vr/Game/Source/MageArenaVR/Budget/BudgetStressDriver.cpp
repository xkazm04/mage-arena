#include "Budget/BudgetStressDriver.h"

#include "Async/Future.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ConvexVolume.h"
#include "Dom/JsonObject.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Greybox/GreyboxUtil.h"
#include "Greybox/HandPresentationComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandFrame.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "Kismet/GameplayStatics.h"
#include "MageArenaVR.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RHI.h"
#include "RHIStats.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCompiler.h"
#include "StaticMeshResources.h"

namespace
{
constexpr double RunTimeoutS = 330.0;
constexpr double WarmupCapS = 150.0;
constexpr double DumpEveryS = 10.0;
constexpr double SphereDiameterCm = 30.0;
constexpr double SpearRadiusM = 0.11;
constexpr double SpearLengthM = 1.7;
const FLinearColor HpBackColour(0.02f, 0.02f, 0.02f);
const FLinearColor HpColour(0.65f, 0.08f, 0.05f);

int32 MeshLod0Triangles(const UStaticMesh* Mesh)
{
	if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0)
	{
		return 0;
	}
	return Mesh->GetRenderData()->LODResources[0].GetNumTriangles();
}

double Percentile(TArray<double> Values, double Fraction)
{
	if (Values.Num() == 0)
	{
		return 0.0;
	}
	Values.Sort();
	const int32 Index = FMath::Clamp(FMath::CeilToInt(Fraction * Values.Num()) - 1, 0, Values.Num() - 1);
	return Values[Index];
}

double Median(TArray<double> Values)
{
	if (Values.Num() == 0)
	{
		return 0.0;
	}
	Values.Sort();
	const int32 Mid = Values.Num() / 2;
	return (Values.Num() % 2) == 0 ? 0.5 * (Values[Mid - 1] + Values[Mid]) : Values[Mid];
}

template <typename T>
TSharedRef<FJsonObject> Spread(const TArray<T>& Values)
{
	TArray<double> AsDouble;
	AsDouble.Reserve(Values.Num());
	for (const T Value : Values)
	{
		AsDouble.Add(static_cast<double>(Value));
	}
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("n"), AsDouble.Num());
	Object->SetNumberField(TEXT("min"), AsDouble.Num() ? FMath::Min(AsDouble) : 0.0);
	Object->SetNumberField(TEXT("median"), Median(AsDouble));
	Object->SetNumberField(TEXT("p95"), Percentile(AsDouble, 0.95));
	Object->SetNumberField(TEXT("max"), AsDouble.Num() ? FMath::Max(AsDouble) : 0.0);
	return Object;
}

template <typename T>
TArray<TSharedPtr<FJsonValue>> Series(const TArray<T>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	Out.Reserve(Values.Num());
	for (const T Value : Values)
	{
		Out.Add(MakeShared<FJsonValueNumber>(static_cast<double>(Value)));
	}
	return Out;
}

bool SaveJson(const TSharedRef<FJsonObject>& Root, const FString& Path)
{
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

UStaticMeshComponent* MakePart(AActor* Owner, const TCHAR* Shape, const FLinearColor& Colour, FBudgetMaterialCache* Shared)
{
	// SessionPresentation::MakePart: one component per part, each with its own tinted unlit material.
	// With a shared cache (the diagnostic lever), parts of one colour reuse one material, so dynamic instancing can merge them.
	UStaticMesh* Mesh = Greybox::LoadShape(Shape);
	if (!Mesh)
	{
		return nullptr;
	}
	UMaterialInterface* Base = Greybox::UnlitOpaqueMaterial();
	return Greybox::MakeMesh(Owner, Mesh, Shared ? BudgetSharedTint(*Shared, Base, Owner, Colour) : Greybox::Tint(Base, Owner, Colour));
}

AMageArenaPawn* FindPawn(UWorld* World)
{
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
}

UMaterialInstanceDynamic* BudgetSharedTint(FBudgetMaterialCache& Cache, UMaterialInterface* Base, UObject* Outer, const FLinearColor& Colour)
{
	if (UMaterialInstanceDynamic* const* Found = Cache.Find(Colour))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* Instance = Greybox::Tint(Base, Outer, Colour);
	if (Instance)
	{
		Cache.Add(Colour, Instance);
	}
	return Instance;
}

ABudgetStressRig::ABudgetStressRig()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

UBudgetStressDriver::UBudgetStressDriver()
	: FTickableGameObject(ETickableTickType::Never)
{
}

void UBudgetStressDriver::Start()
{
	if (bRunning)
	{
		return;
	}
	bRunning = true;
	if (!FParse::Value(FCommandLine::Get(), TEXT("MageArenaBudgetRun="), RunName) || RunName.IsEmpty())
	{
		RunName = TEXT("run");
	}
	bSharedMaterials = FParse::Param(FCommandLine::Get(), TEXT("MageArenaBudgetSharedMaterials"));
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Conditional);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_START run=%s dir=%s"), *RunName, *RunDir());
}

FString UBudgetStressDriver::RunDir() const
{
	// ProjectDir is apps/vr/Game. The git root (runs/) is three levels above that.
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../../runs/DG4"), RunName);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

void UBudgetStressDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
}

void UBudgetStressDriver::Fail(const FString& Why)
{
	if (!bFailed)
	{
		bFailed = true;
		FailReason = Why;
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_FAIL %s"), *Why);
	}
}

bool UBudgetStressDriver::SpawnPopulation()
{
	UWorld* World = GetWorld();
	FArenaLayout Layout;
	FString Error;
	if (!World || !Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error) || !BuildBudgetStressPlan(Layout, Plan, Error))
	{
		Fail(FString::Printf(TEXT("plan: %s"), Error.IsEmpty() ? TEXT("no world") : *Error));
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Rig = World->SpawnActor<ABudgetStressRig>(ABudgetStressRig::StaticClass(), FTransform::Identity, Params);
	if (!Rig)
	{
		Fail(TEXT("rig did not spawn"));
		return false;
	}

	const FRunePad* Pad = Layout.FindPadById(TEXT("centre"));
	const FVector PadCm = Pad ? Pad->PositionM * 100.0 : FVector::ZeroVector;
	FBudgetMaterialCache* Shared = bSharedMaterials ? &SharedMaterials : nullptr;
	for (int32 Index = 0; Index < Plan.Enemies.Num(); ++Index)
	{
		const FBudgetEnemy& Enemy = Plan.Enemies[Index];
		UStaticMeshComponent* Body = MakePart(Rig, Enemy.bCube ? TEXT("Cube") : TEXT("Cylinder"), Enemy.Colour, Shared);
		UStaticMeshComponent* HpBack = MakePart(Rig, TEXT("Cube"), HpBackColour, Shared);
		UStaticMeshComponent* HpFill = MakePart(Rig, TEXT("Cube"), HpColour, Shared);
		const FVector GroundCm = Enemy.GroundM * 100.0;
		const FVector Centre = GroundCm + FVector(0.0, 0.0, Enemy.BodySizeCm.Z * 0.5);
		if (Body)
		{
			FVector Facing = PadCm - GroundCm;
			Facing.Z = 0.0;
			if (!Facing.IsNearlyZero())
			{
				Body->SetWorldRotation(FRotationMatrix::MakeFromX(Facing).Rotator());
			}
			Body->SetWorldLocation(Centre);
			Greybox::SetSized(Body, Enemy.BodySizeCm);
		}
		// SessionPresentation's HP bar: 70 x 8 x 8 cm, 18 cm above the body, fill shrinking from the right.
		const double Fraction = static_cast<double>(Index % 5 + 1) / 5.0;
		const FVector Bar = Centre + FVector(0.0, 0.0, Enemy.BodySizeCm.Z * 0.5 + 18.0);
		if (HpBack)
		{
			HpBack->SetWorldLocation(Bar);
			Greybox::SetSized(HpBack, FVector(70.0, 8.0, 8.0));
		}
		if (HpFill)
		{
			HpFill->SetWorldLocation(Bar + FVector((Fraction - 1.0) * 35.0, 0.0, 0.0));
			Greybox::SetSized(HpFill, FVector(FMath::Max(4.0, 70.0 * Fraction), 8.0, 8.0));
		}
		EnemyParts.Add(Body);
		EnemyParts.Add(HpBack);
		EnemyParts.Add(HpFill);
	}
	for (const FBudgetShot& Shot : Plan.Shots)
	{
		UStaticMeshComponent* Part = MakePart(Rig, Shot.bSpear ? TEXT("Cylinder") : TEXT("Sphere"), Shot.Colour, Shared);
		if (Part)
		{
			if (Shot.bSpear)
			{
				Greybox::SetSized(Part, FVector(SpearRadiusM * 200.0, SpearRadiusM * 200.0, SpearLengthM * 100.0));
			}
			else
			{
				Greybox::SetSized(Part, FVector(SphereDiameterCm));
			}
		}
		ShotParts.Add(Part);
	}
	PopulationStartWall = FPlatformTime::Seconds();
	UpdateShots(0.0);
	KeepHandsLooping();
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_PLAN enemies=%d parts_per_enemy=3 projectiles=%d spears=%d near=%.1fm arc=+/-%.1fdeg far=%.1fm arc=+/-%.1fdeg clip=%s.normal"),
		Plan.Enemies.Num(), Plan.Shots.Num(), Plan.Shots.FilterByPredicate([](const FBudgetShot& Shot) { return Shot.bSpear; }).Num(),
		Plan.NearDistanceM, Plan.NearArcDeg, Plan.FarDistanceM, Plan.FarArcDeg, BudgetStress::HandClipAction);
	return true;
}

void UBudgetStressDriver::UpdateShots(double Seconds)
{
	int64 TotalLaunches = 0;
	for (int32 Index = 0; Index < Plan.Shots.Num() && Index < ShotParts.Num(); ++Index)
	{
		const FBudgetShot& Shot = Plan.Shots[Index];
		int32 ShotLaunches = 0;
		const FVector AtCm = BudgetShotPositionM(Shot, Seconds, &ShotLaunches) * 100.0;
		TotalLaunches += ShotLaunches;
		UStaticMeshComponent* Part = ShotParts[Index];
		if (!Part)
		{
			continue;
		}
		Part->SetWorldLocation(AtCm);
		if (Shot.bSpear)
		{
			const FVector Ahead = BudgetShotPositionM(Shot, Seconds + 0.02) * 100.0;
			const FVector Direction = (Ahead - AtCm).GetSafeNormal();
			if (!Direction.IsNearlyZero())
			{
				Part->SetWorldRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
			}
		}
	}
	// Each wrap of a lane is one arrival and an immediate respawn at the source.
	Launches = TotalLaunches;
}

void UBudgetStressDriver::KeepHandsLooping()
{
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	UHandInputSubsystem* Hands = Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
	if (!Hands)
	{
		return;
	}
	if (!Hands->IsActionPlaying() || Hands->GetActionName() != BudgetStress::HandClipAction)
	{
		Hands->PlayQuickAction(BudgetStress::HandClipAction, EClipVariant::Normal);
	}
}

UBudgetStressDriver::FPopulation UBudgetStressDriver::CountPopulation() const
{
	FPopulation Population;
	for (int32 Index = 0; Index + 2 < EnemyParts.Num(); Index += 3)
	{
		bool bWhole = true;
		for (int32 Part = 0; Part < 3; ++Part)
		{
			const UStaticMeshComponent* Component = EnemyParts[Index + Part];
			bWhole &= Component && Component->IsRegistered() && Component->IsVisible() && Component->GetStaticMesh();
		}
		Population.Enemies += bWhole ? 1 : 0;
	}
	for (const UStaticMeshComponent* Component : ShotParts)
	{
		if (Component && Component->IsRegistered() && Component->IsVisible() && Component->GetStaticMesh())
		{
			++Population.Projectiles;
		}
	}

	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	const UHandInputSubsystem* Hands = Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
	int32 Tracked = 0;
	if (Hands)
	{
		for (const EControllerHand Side : { EControllerHand::Left, EControllerHand::Right })
		{
			FHandFrame Frame;
			if (Hands->GetLatest(Side, Frame) && Frame.Joints.Num() >= MageHandJointCount && Frame.Confidence > 0.f)
			{
				++Tracked;
			}
		}
	}
	// The hands only count when the presentation drew them: 26 joint spheres per hand under the hand component.
	if (AMageArenaPawn* Pawn = FindPawn(World))
	{
		TArray<UInstancedStaticMeshComponent*> Instanced;
		Pawn->GetComponents<UInstancedStaticMeshComponent>(Instanced);
		for (const UInstancedStaticMeshComponent* Component : Instanced)
		{
			if (Component && Component->IsVisible() && Cast<UHandPresentationComponent>(Component->GetAttachParent()))
			{
				Population.HandInstances += Component->GetInstanceCount();
			}
		}
	}
	Population.Hands = FMath::Min(Tracked, Population.HandInstances / MageHandJointCount);
	return Population;
}

bool UBudgetStressDriver::PopulationFull(const FPopulation& Population) const
{
	return Population.Enemies >= BudgetStress::EnemyCount
		&& Population.Projectiles >= BudgetStress::ProjectileCount
		&& Population.Hands >= BudgetStress::HandCount;
}

void UBudgetStressDriver::LogPopulation(const TCHAR* Phase, const FPopulation& Population) const
{
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_POP enemies=%d projectiles=%d hands=%d phase=%s handInstances=%d launches=%lld"),
		Population.Enemies, Population.Projectiles, Population.Hands, Phase, Population.HandInstances, Launches);
}

int32 UBudgetStressDriver::CountVisibleLod0() const
{
	// The A02 estimate (GreyboxCapture.cpp CountVisibleLod0): LOD0 triangles of every visible
	// static mesh component and instance whose bounds touch the view frustum. No occlusion.
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller || !Controller->PlayerCameraManager)
	{
		return 0;
	}
	const FMinimalViewInfo View = Controller->PlayerCameraManager->GetCameraCacheView();
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FMatrix ViewProjection;
	UGameplayStatics::GetViewProjectionMatrix(View, ViewMatrix, ProjectionMatrix, ViewProjection);
	FConvexVolume Frustum;
	GetViewFrustumBounds(Frustum, ViewProjection, false);

	int32 Total = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UStaticMeshComponent*> Components;
		It->GetComponents<UStaticMeshComponent>(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			if (!Component || !Component->IsVisible())
			{
				continue;
			}
			UStaticMesh* Mesh = Component->GetStaticMesh();
			const int32 Tris = MeshLod0Triangles(Mesh);
			if (Tris <= 0)
			{
				continue;
			}
			if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
			{
				const int32 Count = Instances->GetInstanceCount();
				for (int32 Index = 0; Index < Count; ++Index)
				{
					FTransform InstanceTransform;
					if (!Instances->GetInstanceTransform(Index, InstanceTransform, true))
					{
						continue;
					}
					const FBoxSphereBounds WorldBounds = Mesh->GetBounds().TransformBy(InstanceTransform);
					if (Frustum.IntersectBox(WorldBounds.Origin, WorldBounds.BoxExtent))
					{
						Total += Tris;
					}
				}
			}
			else if (Frustum.IntersectBox(Component->Bounds.Origin, Component->Bounds.BoxExtent))
			{
				Total += Tris;
			}
		}
	}
	return Total;
}

void UBudgetStressDriver::SampleFrame()
{
	const double Milliseconds = static_cast<double>(FApp::GetDeltaTime()) * 1000.0;
	DrawCalls.Add(GNumDrawCallsRHI[0]);
	RhiPrimitives.Add(GNumPrimitivesDrawnRHI[0]);
	Lod0Triangles.Add(CountVisibleLod0());
	FrameMs.Add(Milliseconds);
	GpuMs.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
	const FPopulation Population = CountPopulation();
	LiveShots.Add(Population.Projectiles);
	MinLiveShots = FMath::Min(MinLiveShots, Population.Projectiles);
	PeakWorkingSetBytes = FMath::Max<uint64>(PeakWorkingSetBytes, FPlatformMemory::GetStats().UsedPhysical);
}

void UBudgetStressDriver::BeginCsv()
{
#if CSV_PROFILER
	if (FCsvProfiler* Csv = FCsvProfiler::Get())
	{
		// Per-frame mesh draw command stats need -csvCategories=MeshDrawCommandStats on the command line. capture-budget.ps1
		// leaves them off by default: with them on every frame, the first trial run crashed the render thread at 58 s.
		if (FCsvProfiler::IsCapturing())
		{
			Csv->EndCapture();
		}
		const FString Name = FString::Printf(TEXT("dg4-%s.csv"), *RunName);
		CsvPath = FPaths::Combine(RunDir(), Name);
		Csv->BeginCapture(-1, RunDir(), Name);
		bCsvStarted = true;
	}
#endif
}

void UBudgetStressDriver::EndCsv()
{
#if CSV_PROFILER
	if (bCsvStarted)
	{
		if (FCsvProfiler* Csv = FCsvProfiler::Get())
		{
			CsvFuture = Csv->EndCapture();
		}
	}
#endif
}

void UBudgetStressDriver::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::WaitCsv && Step != EStep::Quit && Now - RunStartWall > RunTimeoutS)
	{
		Fail(TEXT("timeout"));
		EndCsv();
		Enter(EStep::WaitCsv);
	}
	if (Rig && (Step == EStep::Warmup || Step == EStep::Sample))
	{
		UpdateShots(Now - PopulationStartWall);
		KeepHandsLooping();
	}

	switch (Step)
	{
	case EStep::WaitStable:
	{
		AMageArenaPawn* Pawn = FindPawn(GetWorld());
		const float Delta = FApp::GetDeltaTime();
		const bool bShadersIdle = GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling();
		StableFrames = (Pawn && Pawn->HasLayout() && bShadersIdle && Delta > 0.f && Delta < 0.05f) ? StableFrames + 1 : 0;
		if ((StableFrames >= 30 && Elapsed >= 2.5) || Elapsed >= 90.0)
		{
			if (!Pawn || !Pawn->HasLayout())
			{
				Fail(TEXT("pawn missing"));
				Enter(EStep::WaitCsv);
				break;
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			if (!SpawnPopulation())
			{
				Enter(EStep::WaitCsv);
				break;
			}
			Enter(EStep::Warmup);
		}
		break;
	}
	case EStep::Warmup:
	{
		// Warm-up: at least WarmupS after the population appears, and 60 frames in a row with no
		// shader compiling, so the new shapes' shaders are not in the sample window.
		const bool bShadersIdle = GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling();
		ShaderIdleFrames = bShadersIdle ? ShaderIdleFrames + 1 : 0;
		if ((Elapsed >= WarmupS && ShaderIdleFrames >= 60) || Elapsed >= WarmupCapS)
		{
			WarmupWallS = Elapsed;
			if (Elapsed >= WarmupCapS)
			{
				UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_BUDGET warm-up hit its %.0f s cap with shaders still compiling"), WarmupCapS);
			}
			SteadyPopulation = CountPopulation();
			LogPopulation(TEXT("steady"), SteadyPopulation);
			if (bSharedMaterials)
			{
				LogSharedMaterials();
			}
			if (!PopulationFull(SteadyPopulation))
			{
				Fail(TEXT("population short at steady state"));
				Enter(EStep::WaitCsv);
				break;
			}
			BeginCsv();
			NextDumpWall = Now + DumpEveryS * 0.5;
			Enter(EStep::Sample);
		}
		break;
	}
	case EStep::Sample:
		SampleFrame();
		if (Now >= NextDumpWall)
		{
			// One frame of per-pass, per-draw GPU-culled counts into Saved/Profiling. capture-budget.ps1 collects them.
			GEngine->Exec(GetWorld(), *FString::Printf(TEXT("r.MeshDrawCommands.DumpStats DG4%s"), *RunName));
			++DumpsRequested;
			NextDumpWall = Now + DumpEveryS;
		}
		if (Elapsed >= SampleS)
		{
			EndPopulation = CountPopulation();
			LogPopulation(TEXT("end"), EndPopulation);
			if (!PopulationFull(EndPopulation))
			{
				Fail(TEXT("population short at the end"));
			}
			if (MinLiveShots < BudgetStress::ProjectileCount)
			{
				Fail(FString::Printf(TEXT("live projectiles fell to %d during the window"), MinLiveShots));
			}
			EndCsv();
			Enter(EStep::WaitCsv);
		}
		break;
	case EStep::WaitCsv:
	{
		const bool bReady = !CsvFuture.IsValid() || CsvFuture.IsReady();
		if (bReady || Elapsed >= 30.0)
		{
			bCsvWritten = CsvFuture.IsValid() && CsvFuture.IsReady() && !CsvFuture.Get().IsEmpty();
			if (bCsvWritten)
			{
				CsvPath = CsvFuture.Get();
			}
			WriteResults();
			Enter(EStep::Quit);
		}
		break;
	}
	case EStep::Quit:
		bRunning = false;
		SetTickableTickType(ETickableTickType::Never);
		if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			Controller->ConsoleCommand(TEXT("Quit"));
		}
		break;
	default:
		break;
	}
}

void UBudgetStressDriver::LogSharedMaterials()
{
	// Read back from the parts themselves, not only the cache: the materials the renderer will actually see.
	TSet<const UMaterialInterface*> Materials;
	SharedMaterialParts = 0;
	for (const TArray<TObjectPtr<UStaticMeshComponent>>* Parts : { &EnemyParts, &ShotParts })
	{
		for (const UStaticMeshComponent* Component : *Parts)
		{
			if (Component)
			{
				Materials.Add(Component->GetMaterial(0));
				++SharedMaterialParts;
			}
		}
	}
	SharedMaterialDistinct = Materials.Num();
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_MATERIALS mids=%d colours=%d parts=%d"), SharedMaterialDistinct, SharedMaterials.Num(), SharedMaterialParts);
}

void UBudgetStressDriver::WriteResults()
{
	const FString Dir = RunDir();
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
	PeakWorkingSetBytes = FMath::Max<uint64>(PeakWorkingSetBytes, Memory.PeakUsedPhysical);

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("label"), TEXT("desktop proxy, synthetic clip-driven hands, not Quest truth; cannot prove 72 Hz"));
	Root->SetStringField(TEXT("run"), RunName);
	Root->SetBoolField(TEXT("ok"), !bFailed);
	Root->SetStringField(TEXT("failReason"), FailReason);
	Root->SetStringField(TEXT("commandLine"), FCommandLine::Get());
	Root->SetStringField(TEXT("rhi"), GDynamicRHI ? GDynamicRHI->GetName() : TEXT("none"));
	Root->SetStringField(TEXT("gpu"), GRHIAdapterName);
	Root->SetStringField(TEXT("cpu"), FPlatformMisc::GetCPUBrand().TrimStartAndEnd());
	Root->SetNumberField(TEXT("warmupS"), WarmupWallS);
	Root->SetNumberField(TEXT("sampleWindowS"), SampleS);

	TSharedRef<FJsonObject> Scene = MakeShared<FJsonObject>();
	Scene->SetNumberField(TEXT("enemies"), Plan.Enemies.Num());
	Scene->SetNumberField(TEXT("partsPerEnemy"), 3);
	Scene->SetNumberField(TEXT("projectiles"), Plan.Shots.Num());
	Scene->SetNumberField(TEXT("spears"), Plan.Shots.FilterByPredicate([](const FBudgetShot& Shot) { return Shot.bSpear; }).Num());
	Scene->SetNumberField(TEXT("partsPerProjectile"), 1);
	Scene->SetNumberField(TEXT("hands"), Plan.Hands);
	Scene->SetStringField(TEXT("handClip"), FString::Printf(TEXT("Game/Clips/%s.normal.jsonl, looped"), BudgetStress::HandClipAction));
	Scene->SetNumberField(TEXT("nearDistanceM"), Plan.NearDistanceM);
	Scene->SetNumberField(TEXT("nearArcDeg"), Plan.NearArcDeg);
	Scene->SetNumberField(TEXT("farDistanceM"), Plan.FarDistanceM);
	Scene->SetNumberField(TEXT("farArcDeg"), Plan.FarArcDeg);
	Scene->SetNumberField(TEXT("niagaraSystems"), 0);
	Root->SetObjectField(TEXT("scene"), Scene);
	if (bSharedMaterials)
	{
		TSharedRef<FJsonObject> Shared = MakeShared<FJsonObject>();
		Shared->SetNumberField(TEXT("mids"), SharedMaterialDistinct);
		Shared->SetNumberField(TEXT("colours"), SharedMaterials.Num());
		Shared->SetNumberField(TEXT("parts"), SharedMaterialParts);
		Root->SetObjectField(TEXT("sharedMaterials"), Shared);
	}

	auto PopulationJson = [](const FPopulation& Population)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("enemies"), Population.Enemies);
		Object->SetNumberField(TEXT("projectiles"), Population.Projectiles);
		Object->SetNumberField(TEXT("hands"), Population.Hands);
		Object->SetNumberField(TEXT("handInstances"), Population.HandInstances);
		return Object;
	};
	Root->SetObjectField(TEXT("populationSteady"), PopulationJson(SteadyPopulation));
	Root->SetObjectField(TEXT("populationEnd"), PopulationJson(EndPopulation));
	Root->SetNumberField(TEXT("minLiveProjectiles"), MinLiveShots == TNumericLimits<int32>::Max() ? 0 : MinLiveShots);
	Root->SetNumberField(TEXT("projectileLaunches"), static_cast<double>(Launches));

	Root->SetNumberField(TEXT("frames"), DrawCalls.Num());
	Root->SetObjectField(TEXT("drawCalls"), Spread(DrawCalls));
	Root->SetObjectField(TEXT("rhiPrimitivesDrawn"), Spread(RhiPrimitives));
	Root->SetObjectField(TEXT("lod0InFrustumTriangles"), Spread(Lod0Triangles));
	Root->SetObjectField(TEXT("frameMs"), Spread(FrameMs));
	Root->SetObjectField(TEXT("gpuMs"), Spread(GpuMs));
	Root->SetNumberField(TEXT("peakWorkingSetMB"), static_cast<double>(PeakWorkingSetBytes) / (1024.0 * 1024.0));
	Root->SetNumberField(TEXT("liveParticles"), 0);
	Root->SetNumberField(TEXT("gpuEmitters"), 0);
	Root->SetStringField(TEXT("particlesNote"), TEXT("not exercised: no effect exists yet"));
	Root->SetNumberField(TEXT("meshDrawCommandDumpsRequested"), DumpsRequested);
	Root->SetStringField(TEXT("csv"), bCsvWritten ? CsvPath : FString());

	TSharedRef<FJsonObject> Samples = MakeShared<FJsonObject>();
	Samples->SetArrayField(TEXT("drawCalls"), Series(DrawCalls));
	Samples->SetArrayField(TEXT("rhiPrimitivesDrawn"), Series(RhiPrimitives));
	Samples->SetArrayField(TEXT("lod0InFrustumTriangles"), Series(Lod0Triangles));
	Samples->SetArrayField(TEXT("frameMs"), Series(FrameMs));
	Samples->SetArrayField(TEXT("gpuMs"), Series(GpuMs));
	Samples->SetArrayField(TEXT("liveProjectiles"), Series(LiveShots));

	const bool bSummary = SaveJson(Root, FPaths::Combine(Dir, TEXT("driver.json")));
	const bool bSamples = SaveJson(Samples, FPaths::Combine(Dir, TEXT("samples.json")));
	if (!bSummary || !bSamples)
	{
		Fail(TEXT("could not write driver.json or samples.json"));
	}
	if (!bFailed)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_DONE run=%s frames=%d csv=%s"), *RunName, DrawCalls.Num(), bCsvWritten ? *CsvPath : TEXT("none"));
	}
}

TStatId UBudgetStressDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBudgetStressDriver, STATGROUP_Tickables);
}

bool UBudgetStressDriver::IsTickable() const
{
	return bRunning && GetWorld() != nullptr;
}

UWorld* UBudgetStressDriver::GetTickableGameObjectWorld() const
{
	return GetWorld();
}
