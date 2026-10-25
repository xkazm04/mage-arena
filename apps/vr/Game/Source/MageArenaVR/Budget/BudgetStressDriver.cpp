#include "Budget/BudgetStressDriver.h"

#include "Async/Future.h"
#include "Camera/CameraTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Greybox/HandPresentationComponent.h"
#include "Greybox/VisibleLod0.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandFrame.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "HeadMountedDisplayTypes.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimMath.h"
#include "MageArenaVR.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraCommon.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RHI.h"
#include "RHIStats.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Session/ArenaSession.h"
#include "Session/PresentationLook.h"
#include "Session/SessionPresentation.h"
#include "ShaderCompiler.h"

namespace
{
constexpr double RunTimeoutS = 330.0;
constexpr double WarmupCapS = 150.0;
constexpr double DumpEveryS = 10.0;
// Kernel ids of the posed population, far above anything a bout spawns. The presentation keys bodies, shots and rings in
// one set (ASessionPresentation::HideUnused), so the two ranges must not meet either.
constexpr int32 PosedEnemyIdBase = 900000;
constexpr int32 PosedShotIdBase = 910000;
constexpr double PosedEnemyMaxHp = 100.0;
constexpr int32 PosedForever = TNumericLimits<int32>::Max();

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

// The kernel point under a layout point: the session draws kernel Pos at its centre pad plus (Pos - PlayerSpawn)
// (FArenaSession::KernelToUnrealCm), so this is that mapping backwards.
FSimVec KernelAt(const FArenaSession& Bout, const FVector& PointM)
{
	const FSimVec Spawn = KernelData().PlayerSpawn;
	const FVector OriginM = Bout.KernelToUnrealCm(Spawn) / 100.0;
	return FSimVec{Spawn.X + (PointM.X - OriginM.X), Spawn.Y + (PointM.Y - OriginM.Y)};
}

UArenaSessionSubsystem* SessionsOf(UWorld* World)
{
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

AMageArenaPawn* FindPawn(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	// The first pawn, or none. A for loop that returns on its first pass is an error under clang
	// (-Wunreachable-code-loop-increment, part of UBT's -Wunreachable-code-aggressive).
	TActorIterator<AMageArenaPawn> It(World);
	return It ? *It : nullptr;
}
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
	if (!World || !Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error))
	{
		Fail(FString::Printf(TEXT("plan: %s"), Error.IsEmpty() ? TEXT("no world") : *Error));
		return false;
	}
	if (!StartSession())
	{
		return false;
	}
	UArenaSessionSubsystem* Sessions = SessionsOf(World);
	PresentationLook::LoadStyleFromCommandLine();
	if (!BuildBudgetStressPlan(Layout, Plan, Error, Sessions->GetSession().GetVrRules()))
	{
		Fail(FString::Printf(TEXT("plan: %s"), *Error));
		return false;
	}
	PlannedCensus = BudgetStressCensus(Plan);
	PoseSession(0.0);

	const FRunePad* Pad = Layout.FindPadById(TEXT("centre"));
	const FVector PadCm = Pad ? Pad->PositionM * 100.0 : FVector::ZeroVector;
	if (!SpawnFx(World, Layout, PadCm))
	{
		return false;
	}
	PopulationStartWall = FPlatformTime::Seconds();
	UpdateShots(0.0);
	KeepHandsLooping();
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_PLAN enemies=%d parts_per_enemy=3 projectiles=%d spears=%d embers=%d near=%.1fm arc=+/-%.1fdeg far=%.1fm arc=+/-%.1fdeg clip=%s.normal"),
	    Plan.Enemies.Num(), Plan.Shots.Num(), Plan.Shots.FilterByPredicate([](const FBudgetShot& Shot)
	{ return Shot.bSpear; }).Num(),
	    Plan.Shots.FilterByPredicate([](const FBudgetShot& Shot)
	{ return Shot.bEmber; })
	        .Num(),
	    Plan.NearDistanceM, Plan.NearArcDeg, Plan.FarDistanceM, Plan.FarArcDeg, BudgetStress::HandClipAction);
	return true;
}

bool UBudgetStressDriver::StartSession()
{
	// T46: the measured frame is the session's own. Its subsystem spawned the one ASessionPresentation of this world (and
	// left it idle under the capture flag); start that session's bout and freeze it, and the subsystem keeps syncing the
	// presentation from the posed state every frame, as in a bout.
	UArenaSessionSubsystem* Sessions = SessionsOf(GetWorld());
	Presentation = Sessions ? Sessions->GetPresentation() : nullptr;
	if (!Sessions || !Presentation)
	{
		Fail(TEXT("no live session presentation: UArenaSessionSubsystem spawned none"));
		return false;
	}
	FArenaSession& Bout = Sessions->GetSession();
	Bout.SetSaveDirectory(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("budget-capture")));
	Bout.SetBout(BudgetStress::SessionBout, true);
	if (!Sessions->Start(1, true))
	{
		Fail(TEXT("the session bout did not start"));
		return false;
	}
	Bout.SetFrameHold(true);
	const FVrRuleset* Rules = Bout.GetVrRules();
	const FVrRival* Rival = Rules ? Rules->BoundRival() : nullptr;
	SessionNote = FString::Printf(TEXT("bout=%d phase=%s rival=%s actors=%d"), BudgetStress::SessionBout + 1, *Bout.GetGames().Phase,
	    Rival ? *Rival->Id : TEXT("-"), Bout.GetGames().State.Actors.Num());
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_SESSION %s"), *SessionNote);
	return true;
}

void UBudgetStressDriver::PoseSession(double Seconds)
{
	UArenaSessionSubsystem* Sessions = SessionsOf(GetWorld());
	if (!Sessions)
	{
		return;
	}
	FArenaSession& Bout = Sessions->GetSession();
	// The bout is frozen (SetFrameHold), so the kernel never steps again and the capture owns its state, as the other
	// capture drivers do when they pose a still.
	FGames& Games = const_cast<FGames&>(Bout.GetGames());
	FArenaState& State = Games.State;
	const FActor* Player = SimFindActor(State, Games.PlayerId);
	const FSimVec Seat = Player ? Player->Pos : KernelData().PlayerSpawn;
	if (EnemyActorIds.Num() == 0)
	{
		for (int32 Index = 0; Index < Plan.Enemies.Num(); ++Index)
		{
			const FBudgetEnemy& Planned = Plan.Enemies[Index];
			FActor& Enemy = State.Actors.AddDefaulted_GetRef();
			Enemy.Id = PosedEnemyIdBase + Index;
			Enemy.Team = 1;
			Enemy.Label = Planned.EnemyId;
			Enemy.Pos = KernelAt(Bout, Planned.GroundM);
			Enemy.PreviousPos = Enemy.Pos;
			Enemy.Facing = SimUnit(SimSub(Seat, Enemy.Pos));
			Enemy.Radius = 0.4;
			Enemy.MaxHp = PosedEnemyMaxHp;
			Enemy.Hp = PosedEnemyMaxHp * Planned.HpFraction;
			FEnemyBrain Brain;
			Brain.Id = Planned.EnemyId;
			Enemy.Enemy = Brain;
			EnemyActorIds.Add(Enemy.Id);
		}
		for (int32 Index = 0; Index < Plan.Shots.Num(); ++Index)
		{
			const FBudgetShot& Planned = Plan.Shots[Index];
			FProjectile& Shot = State.Projectiles.AddDefaulted_GetRef();
			Shot.Id = PosedShotIdBase + Index;
			Shot.ActivationId = Shot.Id;
			Shot.OwnerId = EnemyActorIds.IsValidIndex(Planned.EnemyIndex) ? EnemyActorIds[Planned.EnemyIndex] : 0;
			Shot.Family = Planned.Family;
			Shot.Radius = KernelData().ProjectileRadiusM;
			Shot.OriginPos = KernelAt(Bout, Planned.StartM);
			Shot.AimedAt = KernelAt(Bout, Planned.EndM);
			// A spear and an ember fly from a telegraph with a target; the presentation draws their ground ring from it.
			Shot.bHasAim = Planned.bSpear || Planned.bEmber;
			ShotIds.Add(Shot.Id);
		}
	}
	for (int32 Index = 0; Index < Plan.Shots.Num(); ++Index)
	{
		const int32 Id = PosedShotIdBase + Index;
		FProjectile* Shot = State.Projectiles.FindByPredicate([Id](const FProjectile& Each)
		{ return Each.Id == Id; });
		if (!Shot)
		{
			continue;
		}
		const FBudgetShot& Planned = Plan.Shots[Index];
		const FSimVec At = KernelAt(Bout, BudgetShotPositionM(Planned, Seconds));
		const FSimVec Lane = SimSub(Shot->AimedAt, Shot->OriginPos);
		Shot->Velocity = SimScale(SimUnit(Lane), SimLength(Lane) / FMath::Max(Planned.FlightS, 1.0e-3));
		Shot->PreviousPos = Shot->Pos;
		Shot->Pos = At;
		Shot->RemainingM = SimDistance(At, Shot->AimedAt);
	}
	// The heaviest defences a bout shows at once: the player's staff dome and the rival's fire wall, both translucent.
	if (Games.VrRules.IsSet())
	{
		FVrRuleset& Rules = Games.VrRules.GetValue();
		bDomePosed = Rules.Staff.bEnabled;
		if (bDomePosed)
		{
			Rules.StaffRuntime.bPlanted = true;
			Rules.StaffRuntime.ActorId = Games.PlayerId;
			Rules.StaffRuntime.UntilTick = PosedForever;
		}
		bWallPosed = Rules.FireWall.bEnabled;
		if (bWallPosed && Rules.Walls.Num() == 0)
		{
			const FActor* Rival = SimFindActor(State, Rules.RivalRuntime.ActorId);
			FVrWall& Wall = Rules.Walls.AddDefaulted_GetRef();
			Wall.Id = 1;
			Wall.OwnerId = Rival ? Rival->Id : -1;
			Wall.Centre = Rival ? Rival->Pos : KernelAt(Bout, FVector::ZeroVector);
			Wall.Facing = SimUnit(SimSub(Seat, Wall.Centre));
			Wall.DistanceM = Rules.FireWall.DistanceM;
			Wall.ArcDeg = Rules.FireWall.ArcDeg;
			Wall.UntilTick = PosedForever;
		}
	}
}

void UBudgetStressDriver::CheckParity(const TCHAR* Phase)
{
	DrawnCensus = FSceneCensus();
	if (Presentation)
	{
		AddDrawnParts(DrawnCensus, *Presentation);
	}
	Parity = CompareSceneCensus(PlannedCensus, DrawnCensus);
	bParityChecked = true;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_PARITY phase=%s ok=%d planned=%d parts/%d tuples drawn=%d parts/%d tuples extras=%d tuples plannedHash=%s drawnHash=%s"),
	    Phase, Parity.bOk ? 1 : 0, PlannedCensus.Parts(), PlannedCensus.Rows.Num(), DrawnCensus.Parts(), DrawnCensus.Rows.Num(), Parity.Extras.Num(),
	    *PlannedCensus.Hash(), *DrawnCensus.Hash());
	for (const FCensusRow& Row : DrawnCensus.Rows)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_CENSUS phase=%s drawn=%d planned=%d %s"), Phase, Row.Count, PlannedCensus.CountOf(Row.Key), *Row.Key.ToString());
	}
	for (const FString& Line : Parity.Divergences)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET_PARITY_DIVERGENCE phase=%s %s"), Phase, *Line);
	}
	if (!Parity.bOk)
	{
		const FString More = Parity.Divergences.Num() > 1 ? FString::Printf(TEXT(" (+%d more)"), Parity.Divergences.Num() - 1) : FString();
		Fail(FString::Printf(TEXT("scene parity %s%s"), *Parity.Divergences[0], *More));
	}
}

bool UBudgetStressDriver::SpawnFx(UWorld* World, const FArenaLayout& Layout, const FVector& PadCm)
{
	const FRunePad* Pad = Layout.FindPadById(TEXT("centre"));
	const FVector Forward = Pad ? Layout.FlatForwardM(*Pad) : FVector::ForwardVector;
	// In front of the dais at palm height; the motes hang over the arena centre.
	const FVector PalmCm = PadCm + Forward * 60.0 + FVector(0.0, 0.0, 110.0);
	const FVector MotesCm = PadCm + Forward * 250.0 + FVector(0.0, 0.0, 130.0);
	FVector TelegraphCm = PadCm + Forward * 600.0;
	double FarthestM = -1.0;
	for (const FBudgetEnemy& Enemy : Plan.Enemies)
	{
		if (Enemy.DistanceM > FarthestM)
		{
			FarthestM = Enemy.DistanceM;
			TelegraphCm = Enemy.GroundM * 100.0 + FVector(0.0, 0.0, 100.0);
		}
	}
	bAllowGpuParticles = FNiagaraUtilities::AllowGPUParticles();

	for (const FBudgetFxRow& Row : BudgetStress::FxRows())
	{
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *FString::Printf(TEXT("%s.%s"), *Row.AssetPath, *Row.Name));
		if (!System)
		{
			Fail(FString::Printf(TEXT("niagara system %s did not load"), *Row.AssetPath));
			return false;
		}
		UNiagaraComponent* Component = nullptr;
		{
			FVector At = PalmCm;
			if (Row.Name == TEXT("NS_Budget_Telegraph"))
			{
				At = TelegraphCm;
			}
			else if (Row.Name == TEXT("NS_Budget_Motes"))
			{
				At = MotesCm;
			}
			else if (Row.Name == TEXT("NS_Budget_BoltTrail") && Plan.Shots.Num() > 0)
			{
				// It rides the first shot (UpdateFx moves it along that lane).
				At = BudgetShotPositionM(Plan.Shots[0], 0.0) * 100.0;
			}
			Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, At, FRotator::ZeroRotator, FVector::OneVector, false);
		}
		if (!Component)
		{
			Fail(FString::Printf(TEXT("niagara system %s did not spawn"), *Row.AssetPath));
			return false;
		}
		FxComponents.Add(Component);
	}
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_FX spawned=%d allowGpuParticles=%d"), FxComponents.Num(), bAllowGpuParticles ? 1 : 0);
	return true;
}

void UBudgetStressDriver::UpdateFx(double Seconds)
{
	// The bolt trail (row 0) rides the first shot's lane.
	constexpr int32 TrailIndex = 0;
	if (FxComponents.IsValidIndex(TrailIndex) && FxComponents[TrailIndex] && Plan.Shots.Num() > 0 && BudgetStress::FxRows()[TrailIndex].Name == TEXT("NS_Budget_BoltTrail"))
	{
		FxComponents[TrailIndex]->SetWorldLocation(BudgetShotPositionM(Plan.Shots[0], Seconds) * 100.0);
	}
	// The sigil trail (row 1) follows the right index fingertip; joints are local to the hand presentation component.
	constexpr int32 SigilIndex = 1;
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	const UHandInputSubsystem* Hands = Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
	AMageArenaPawn* Pawn = FindPawn(World);
	UHandPresentationComponent* HandView = Pawn ? Pawn->FindComponentByClass<UHandPresentationComponent>() : nullptr;
	FHandFrame Frame;
	if (!FxComponents.IsValidIndex(SigilIndex) || !FxComponents[SigilIndex] || !Hands || !HandView || !Hands->GetLatest(EControllerHand::Right, Frame) || !Frame.Joints.IsValidIndex(static_cast<int32>(EHandKeypoint::IndexTip)))
	{
		return;
	}
	const FVector Local = Frame.Joints[static_cast<int32>(EHandKeypoint::IndexTip)].Location;
	FxComponents[SigilIndex]->SetWorldLocation(HandView->GetComponentTransform().TransformPosition(Local));
}

int32 UBudgetStressDriver::CountLiveFx() const
{
	int32 Live = 0;
	for (const UNiagaraComponent* Component : FxComponents)
	{
		if (Component && !Component->IsComplete() && Component->GetSystemInstanceController().IsValid())
		{
			++Live;
		}
	}
	return Live;
}

void UBudgetStressDriver::SampleFx()
{
	// NIAGARA-PROXY 2.2: component -> system instance -> emitters. The instance accessor is "unsafe" because a concurrent
	// tick may still be running; the driver ticks after the world, so wait for it to finish first.
	TArray<FBudgetEmitterSample> Samples;
	int32 Live = 0;
	int32 CrossCheck = 0;
	for (UNiagaraComponent* Component : FxComponents)
	{
		if (!Component || Component->IsComplete())
		{
			continue;
		}
		FNiagaraSystemInstanceControllerPtr Controller = Component->GetSystemInstanceController();
		FNiagaraSystemInstance* SystemInstance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
		if (!SystemInstance)
		{
			continue;
		}
		SystemInstance->WaitForConcurrentTickAndFinalize();
		++Live;
		CrossCheck += static_cast<int32>(SystemInstance->ActiveGPUEmitterCount);
		for (const FNiagaraEmitterInstanceRef& Emitter : SystemInstance->GetEmitters())
		{
			FBudgetEmitterSample Sample;
			Sample.bEnabled = !Emitter->IsDisabled();
			Sample.SimTarget = Emitter->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim ? EBudgetSimTarget::Gpu : EBudgetSimTarget::Cpu;
			switch (Emitter->GetExecutionState())
			{
			case ENiagaraExecutionState::Active:
				Sample.ExecutionState = EBudgetExecutionState::Active;
				break;
			case ENiagaraExecutionState::Inactive:
				Sample.ExecutionState = EBudgetExecutionState::Inactive;
				break;
			case ENiagaraExecutionState::InactiveClear:
				Sample.ExecutionState = EBudgetExecutionState::InactiveClear;
				break;
			case ENiagaraExecutionState::Complete:
				Sample.ExecutionState = EBudgetExecutionState::Complete;
				break;
			default:
				Sample.ExecutionState = EBudgetExecutionState::Disabled;
				break;
			}
			Sample.ParticleCount = Emitter->GetNumParticles();
			Samples.Add(Sample);
		}
	}
	const FBudgetParticleCount Count = CountEmitters(Samples);
	LiveParticles.Add(Count.Total());
	LiveParticlesCpu.Add(Count.Cpu);
	LiveParticlesGpu.Add(Count.Gpu);
	GlowPuffs.Add(Presentation ? Presentation->GetGlowPuffsDrawn() : 0);
	GlowLayers.Add(Presentation ? Presentation->CountGlowLayersDrawn() : 0);
	GpuEmitterCounts.Add(Count.ActiveGpuEmitters);
	GpuCrossCheckCounts.Add(CrossCheck);
	NiagaraLive.Add(Live);
	ZeroParticleFrames += Count.Total() == 0 ? 1 : 0;
}

void UBudgetStressDriver::JudgeParticles()
{
	const int32 MinLive = NiagaraLive.Num() ? FMath::Min(NiagaraLive) : 0;
	const int32 MaxParticles = LiveParticles.Num() ? FMath::Max(LiveParticles) : 0;
	const int32 MaxGpu = GpuEmitterCounts.Num() ? FMath::Max(GpuEmitterCounts) : 0;
	ParticleVerdict = EvaluateParticles(MinLive, MaxParticles, ZeroParticleFrames, MaxGpu, ParticlesNote);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_PARTICLES live=%d gpuEmitters=%d zeroFrames=%d minSystems=%d verdict=%s"), MaxParticles, MaxGpu,
		ZeroParticleFrames, MinLive, *ParticlesNote);
	if (ParticleVerdict == EBudgetParticleVerdict::NotExercised)
	{
		Fail(FString::Printf(TEXT("particles not exercised (%s)"), *ParticlesNote));
	}
}

void UBudgetStressDriver::UpdateShots(double Seconds)
{
	// The shots move in PoseSession; this counts the launches. Each wrap of a lane is one arrival and an immediate
	// respawn at the source.
	int64 TotalLaunches = 0;
	for (const FBudgetShot& Shot : Plan.Shots)
	{
		int32 ShotLaunches = 0;
		BudgetShotPositionM(Shot, Seconds, &ShotLaunches);
		TotalLaunches += ShotLaunches;
	}
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
	// What the presentation drew of the posed population: an enemy counts with its body and both HP bar parts visible.
	for (const int32 Id : EnemyActorIds)
	{
		Population.Enemies += Presentation && Presentation->IsBodyDrawn(Id) ? 1 : 0;
	}
	for (const int32 Id : ShotIds)
	{
		Population.Projectiles += Presentation && Presentation->IsShotDrawn(Id) ? 1 : 0;
	}

	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	const UHandInputSubsystem* Hands = Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
	Population.Niagara = CountLiveFx();
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
		&& Population.Hands >= BudgetStress::HandCount
		&& Population.Niagara >= BudgetStress::FxRowCount;
}

void UBudgetStressDriver::LogPopulation(const TCHAR* Phase, const FPopulation& Population) const
{
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_POP enemies=%d projectiles=%d hands=%d niagara=%d phase=%s handInstances=%d launches=%lld"),
		Population.Enemies, Population.Projectiles, Population.Hands, Population.Niagara, Phase, Population.HandInstances, Launches);
}

int32 UBudgetStressDriver::VisibleLod0(UWorld* World, const FMinimalViewInfo& View)
{
	return Greybox::CountVisibleLod0(World, View);
}

void UBudgetStressDriver::SampleFrame()
{
	const double Milliseconds = static_cast<double>(FApp::GetDeltaTime()) * 1000.0;
	DrawCalls.Add(GNumDrawCallsRHI[0]);
	RhiPrimitives.Add(GNumPrimitivesDrawnRHI[0]);
	FMinimalViewInfo View;
	const Greybox::FVisibleLod0 Lod0 = Greybox::FirstPlayerView(GetWorld(), View) ? Greybox::CountVisibleLod0Split(GetWorld(), View) : Greybox::FVisibleLod0();
	Lod0Triangles.Add(Lod0.Total());
	Lod0SkeletalTriangles.Add(Lod0.SkeletalTriangles);
	FrameMs.Add(Milliseconds);
	GpuMs.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
	const FPopulation Population = CountPopulation();
	LiveShots.Add(Population.Projectiles);
	MinLiveShots = FMath::Min(MinLiveShots, Population.Projectiles);
	SampleFx();
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
	if (Presentation && (Step == EStep::Warmup || Step == EStep::Sample))
	{
		PoseSession(Now - PopulationStartWall);
		UpdateShots(Now - PopulationStartWall);
		UpdateFx(Now - PopulationStartWall);
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
			LogSharedMaterials();
			CheckParity(TEXT("steady"));
			if (!PopulationFull(SteadyPopulation))
			{
				Fail(TEXT("population short at steady state"));
			}
			if (bFailed)
			{
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
			CheckParity(TEXT("end"));
			if (!PopulationFull(EndPopulation))
			{
				Fail(TEXT("population short at the end"));
			}
			if (MinLiveShots < BudgetStress::ProjectileCount)
			{
				Fail(FString::Printf(TEXT("live projectiles fell to %d during the window"), MinLiveShots));
			}
			JudgeParticles();
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
	// Read back from the presentation's parts, not only its caches: the materials the renderer will actually see.
	TSet<const UMaterialInterface*> Materials;
	SharedMaterialParts = 0;
	if (Presentation)
	{
		TInlineComponentArray<UStaticMeshComponent*> Parts(Presentation.Get());
		for (const UStaticMeshComponent* Component : Parts)
		{
			if (Component)
			{
				Materials.Add(Component->GetMaterial(0));
				++SharedMaterialParts;
			}
		}
		SharedMaterialTints = Presentation->GetSharedTintCount();
	}
	SharedMaterialDistinct = Materials.Num();
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET_MATERIALS mids=%d colours=%d parts=%d"), SharedMaterialDistinct, SharedMaterialTints, SharedMaterialParts);
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
	Scene->SetNumberField(TEXT("embers"), Plan.Shots.FilterByPredicate([](const FBudgetShot& Shot)
	{ return Shot.bEmber; }).Num());
	Scene->SetStringField(TEXT("drawnBy"), TEXT("the session's ASessionPresentation (T46), posed into a frozen bout"));
	Scene->SetStringField(TEXT("session"), SessionNote);
	Scene->SetBoolField(TEXT("domePosed"), bDomePosed);
	Scene->SetBoolField(TEXT("fireWallPosed"), bWallPosed);
	Scene->SetNumberField(TEXT("partsPerProjectile"), 1);
	Scene->SetNumberField(TEXT("hands"), Plan.Hands);
	Scene->SetStringField(TEXT("handClip"), FString::Printf(TEXT("Game/Clips/%s.normal.jsonl, looped"), BudgetStress::HandClipAction));
	Scene->SetNumberField(TEXT("nearDistanceM"), Plan.NearDistanceM);
	Scene->SetNumberField(TEXT("nearArcDeg"), Plan.NearArcDeg);
	Scene->SetNumberField(TEXT("farDistanceM"), Plan.FarDistanceM);
	Scene->SetNumberField(TEXT("farArcDeg"), Plan.FarArcDeg);
	Scene->SetNumberField(TEXT("niagaraSystems"), BudgetStress::FxRows().Num());
	Root->SetObjectField(TEXT("scene"), Scene);
	TSharedRef<FJsonObject> Shared = MakeShared<FJsonObject>();
	Shared->SetNumberField(TEXT("mids"), SharedMaterialDistinct);
	Shared->SetNumberField(TEXT("colours"), SharedMaterialTints);
	Shared->SetNumberField(TEXT("parts"), SharedMaterialParts);
	Shared->SetStringField(TEXT("scope"), TEXT("every mesh component of the session presentation"));
	Root->SetObjectField(TEXT("sharedMaterials"), Shared);
	TSharedRef<FJsonObject> ParityJson = MakeShared<FJsonObject>();
	ParityJson->SetBoolField(TEXT("checked"), bParityChecked);
	ParityJson->SetBoolField(TEXT("ok"), bParityChecked && Parity.bOk);
	ParityJson->SetStringField(TEXT("rule"),
	    TEXT("every planned (mesh, colour, material, pass) row drawn at least its planned count, tolerance 0 parts; every planned or required pass drawn; drawn rows beyond the plan are the session's own parts (runs/T46/parity-census.md section 4)"));
	TArray<TSharedPtr<FJsonValue>> Divergences;
	for (const FString& Line : Parity.Divergences)
	{
		Divergences.Add(MakeShared<FJsonValueString>(Line));
	}
	ParityJson->SetArrayField(TEXT("divergences"), Divergences);
	ParityJson->SetObjectField(TEXT("planned"), PlannedCensus.ToJson());
	ParityJson->SetObjectField(TEXT("drawn"), DrawnCensus.ToJson());
	Root->SetObjectField(TEXT("parity"), ParityJson);

	auto PopulationJson = [](const FPopulation& Population)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("enemies"), Population.Enemies);
		Object->SetNumberField(TEXT("projectiles"), Population.Projectiles);
		Object->SetNumberField(TEXT("hands"), Population.Hands);
		Object->SetNumberField(TEXT("handInstances"), Population.HandInstances);
		Object->SetNumberField(TEXT("niagara"), Population.Niagara);
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
	// The skinned share of lod0InFrustumTriangles (the creatures). A row from before it counted static meshes only, so it
	// compares with lod0InFrustumTriangles minus this field.
	Root->SetObjectField(TEXT("lod0InFrustumSkeletalTriangles"), Spread(Lod0SkeletalTriangles));
	Root->SetObjectField(TEXT("frameMs"), Spread(FrameMs));
	Root->SetObjectField(TEXT("gpuMs"), Spread(GpuMs));
	Root->SetNumberField(TEXT("peakWorkingSetMB"), static_cast<double>(PeakWorkingSetBytes) / (1024.0 * 1024.0));
	// The window maximum is the figure the particle row is judged on (NIAGARA-PROXY 2.5).
	Root->SetNumberField(TEXT("liveParticles"), LiveParticles.Num() ? FMath::Max(LiveParticles) : 0);
	Root->SetNumberField(TEXT("gpuEmitters"), GpuEmitterCounts.Num() ? FMath::Max(GpuEmitterCounts) : 0);
	Root->SetObjectField(TEXT("liveParticlesSpread"), Spread(LiveParticles));
	Root->SetObjectField(TEXT("liveParticlesCpu"), Spread(LiveParticlesCpu));
	Root->SetObjectField(TEXT("liveParticlesGpu"), Spread(LiveParticlesGpu));
	Root->SetObjectField(TEXT("glowPuffs"), Spread(GlowPuffs));
	Root->SetObjectField(TEXT("glowLayers"), Spread(GlowLayers));
	Root->SetNumberField(TEXT("gpuEmittersCrossCheck"), GpuCrossCheckCounts.Num() ? FMath::Max(GpuCrossCheckCounts) : 0);
	TSharedRef<FJsonObject> SystemsLive = MakeShared<FJsonObject>();
	SystemsLive->SetNumberField(TEXT("min"), NiagaraLive.Num() ? FMath::Min(NiagaraLive) : 0);
	SystemsLive->SetNumberField(TEXT("max"), NiagaraLive.Num() ? FMath::Max(NiagaraLive) : 0);
	Root->SetObjectField(TEXT("niagaraSystemsLive"), SystemsLive);
	Root->SetNumberField(TEXT("zeroParticleFrames"), ZeroParticleFrames);
	Root->SetBoolField(TEXT("particlesExercised"), ParticleVerdict != EBudgetParticleVerdict::NotExercised);
	Root->SetStringField(TEXT("particlesVerdict"), ParticleVerdict == EBudgetParticleVerdict::Pass ? TEXT("pass")
		: ParticleVerdict == EBudgetParticleVerdict::Fail ? TEXT("fail") : TEXT("not exercised"));
	Root->SetStringField(TEXT("particlesNote"), ParticlesNote);
	Root->SetStringField(TEXT("gpuCountNote"), TEXT("GPU counts are a delayed upper bound (instance-count readback)"));
	TSharedRef<FJsonObject> NiagaraJson = MakeShared<FJsonObject>();
	NiagaraJson->SetBoolField(TEXT("allowGpuParticles"), bAllowGpuParticles);
	TArray<TSharedPtr<FJsonValue>> SystemsJson;
	for (int32 Index = 0; Index < BudgetStress::FxRows().Num(); ++Index)
	{
		const FBudgetFxRow& Row = BudgetStress::FxRows()[Index];
		TSharedRef<FJsonObject> One = MakeShared<FJsonObject>();
		One->SetStringField(TEXT("asset"), Row.AssetPath);
		One->SetNumberField(TEXT("authoredPeak"), Row.PeakLive);
		TArray<TSharedPtr<FJsonValue>> Targets;
		UNiagaraComponent* Component = FxComponents.IsValidIndex(Index) ? FxComponents[Index].Get() : nullptr;
		FNiagaraSystemInstanceControllerPtr Controller = Component ? Component->GetSystemInstanceController() : nullptr;
		FNiagaraSystemInstance* SystemInstance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
		if (SystemInstance)
		{
			for (const FNiagaraEmitterInstanceRef& Emitter : SystemInstance->GetEmitters())
			{
				Targets.Add(MakeShared<FJsonValueString>(Emitter->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim ? TEXT("gpu") : TEXT("cpu")));
			}
		}
		One->SetNumberField(TEXT("emitters"), Targets.Num());
		One->SetArrayField(TEXT("simTargets"), Targets);
		SystemsJson.Add(MakeShared<FJsonValueObject>(One));
	}
	NiagaraJson->SetArrayField(TEXT("systems"), SystemsJson);
	Root->SetObjectField(TEXT("niagara"), NiagaraJson);
	Root->SetNumberField(TEXT("meshDrawCommandDumpsRequested"), DumpsRequested);
	Root->SetStringField(TEXT("csv"), bCsvWritten ? CsvPath : FString());

	TSharedRef<FJsonObject> Samples = MakeShared<FJsonObject>();
	Samples->SetArrayField(TEXT("drawCalls"), Series(DrawCalls));
	Samples->SetArrayField(TEXT("rhiPrimitivesDrawn"), Series(RhiPrimitives));
	Samples->SetArrayField(TEXT("lod0InFrustumTriangles"), Series(Lod0Triangles));
	Samples->SetArrayField(TEXT("lod0InFrustumSkeletalTriangles"), Series(Lod0SkeletalTriangles));
	Samples->SetArrayField(TEXT("frameMs"), Series(FrameMs));
	Samples->SetArrayField(TEXT("gpuMs"), Series(GpuMs));
	Samples->SetArrayField(TEXT("liveProjectiles"), Series(LiveShots));
	Samples->SetArrayField(TEXT("liveParticles"), Series(LiveParticles));
	Samples->SetArrayField(TEXT("liveParticlesGpu"), Series(LiveParticlesGpu));
	Samples->SetArrayField(TEXT("gpuEmitters"), Series(GpuEmitterCounts));

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
