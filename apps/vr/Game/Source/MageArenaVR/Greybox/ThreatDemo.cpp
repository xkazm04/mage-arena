#include "Greybox/ThreatDemo.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Greybox/GreyboxUtil.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "Hands/MageArenaPawn.h"
#include "MageArenaVR.h"
#include "Misc/Parse.h"

namespace
{
constexpr int32 GRimSegments = 8;

UWorld* FindGameWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && World->IsGameWorld())
		{
			return World;
		}
	}
	return nullptr;
}

void LaunchThreatsCommand()
{
	UWorld* World = FindGameWorld();
	UThreatDemoSubsystem* Demo = World ? World->GetSubsystem<UThreatDemoSubsystem>() : nullptr;
	if (!Demo)
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Demo.Threats: no game world"));
		return;
	}
	Demo->Launch();
}

FAutoConsoleCommand GThreatsCommand(
	TEXT("MageArena.Demo.Threats"),
	TEXT("Launch water, fire, steel, then an unblockable toward the active pad. Visual only."),
	FConsoleCommandDelegate::CreateStatic(&LaunchThreatsCommand));

UMaterialInterface* ThreatMaterial()
{
	if (UMaterialInterface* Unlit = Greybox::UnlitOpaqueMaterial())
	{
		return Unlit;
	}
	UE_LOG(LogMageArena, Warning, TEXT("Threat unlit material missing; falling back to the lit basic material"));
	return Greybox::BasicMaterial();
}

FLinearColor Opaque(FLinearColor Colour)
{
	Colour.A = 1.f;
	return Colour;
}

float EncodeSrgb(float Linear)
{
	if (Linear <= 0.0031308f)
	{
		return Linear * 12.92f;
	}
	return 1.055f * FMath::Pow(FMath::Max(Linear, 0.f), 1.f / 2.4f) - 0.055f;
}

FLinearColor ToSrgb(const FLinearColor& Linear)
{
	return FLinearColor(EncodeSrgb(Linear.R), EncodeSrgb(Linear.G), EncodeSrgb(Linear.B), 1.f);
}

void PlaceSized(UStaticMeshComponent* Mesh, const FVector& CentreCm, const FQuat& Rotation, const FVector& FullSizeCm)
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return;
	}
	const FVector Extent = Mesh->GetStaticMesh()->GetBounds().BoxExtent;
	const FVector Scale(
		FullSizeCm.X / FMath::Max(Extent.X * 2.0, 0.01),
		FullSizeCm.Y / FMath::Max(Extent.Y * 2.0, 0.01),
		FullSizeCm.Z / FMath::Max(Extent.Z * 2.0, 0.01));
	Mesh->SetWorldTransform(FTransform(Rotation, CentreCm, Scale));
	Mesh->SetVisibility(true);
}

void HideAll(TArray<TObjectPtr<UStaticMeshComponent>>& Meshes)
{
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh)
		{
			Mesh->SetVisibility(false);
		}
	}
}

FVector CameraOrFallback(UWorld* World)
{
	if (World)
	{
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			if (Controller->PlayerCameraManager)
			{
				return Controller->PlayerCameraManager->GetCameraLocation();
			}
		}
	}
	return FVector(-1200.0, 0.0, 180.0);
}
}

TArray<FThreatLaunch> BuildThreatLaunches(const FArenaLayout& Layout, const FRunePad& Pad)
{
	TArray<FThreatLaunch> Launches;
	const FVector Forward = Layout.FlatForwardM(Pad);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	Launches.Reserve(Layout.Threats.Num());
	for (const FThreatSpec& Spec : Layout.Threats)
	{
		const FSpawnMarker* Marker = Layout.FindMarker(Spec.MarkerId);
		if (!Marker)
		{
			continue;
		}
		FThreatLaunch Launch;
		Launch.Spec = Spec;
		const FVector LandingXY = FVector(Pad.PositionM.X, Pad.PositionM.Y, 0.0)
			+ Forward * Spec.LandingForwardM + Right * Spec.LandingRightM;
		Launch.LandingM = FVector(LandingXY.X, LandingXY.Y, Layout.SurfaceHeightM(LandingXY.X, LandingXY.Y));
		Launch.StartM = FVector(Marker->PositionM.X, Marker->PositionM.Y, Layout.ChestHeightAboveSandM);
		Launch.EndM = FVector(Launch.LandingM.X, Launch.LandingM.Y, Layout.ChestHeightAboveSandM);
		Launches.Add(Launch);
	}
	return Launches;
}

AThreatRig::AThreatRig()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

UThreatDemoSubsystem::UThreatDemoSubsystem()
	: FTickableGameObject(ETickableTickType::Never)
{
}

bool UThreatDemoSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UThreatDemoSubsystem::Deinitialize()
{
	bRunning = false;
	bFrozen = false;
	SetTickableTickType(ETickableTickType::Never);
	Super::Deinitialize();
}

void UThreatDemoSubsystem::BeginDestroy()
{
	bRunning = false;
	bFrozen = false;
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}

void UThreatDemoSubsystem::SetFrozen(bool bInFrozen)
{
	bFrozen = bInFrozen;
}

void UThreatDemoSubsystem::EnsureRig()
{
	if (Rig || !GetWorld())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Rig = GetWorld()->SpawnActor<AThreatRig>(AThreatRig::StaticClass(), FTransform::Identity, Params);
}

void UThreatDemoSubsystem::ClearFlights()
{
	auto DestroyMesh = [](UStaticMeshComponent* Mesh)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	};
	for (FFlight& Flight : Flights)
	{
		DestroyMesh(Flight.Body);
		for (UStaticMeshComponent* Mesh : Flight.RimSegments)
		{
			DestroyMesh(Mesh);
		}
		for (UStaticMeshComponent* Mesh : Flight.RingSegments)
		{
			DestroyMesh(Mesh);
		}
	}
	Flights.Reset();
}

void UThreatDemoSubsystem::Launch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FRunePad* Pad = nullptr;
	for (TActorIterator<AMageArenaPawn> It(World); It; ++It)
	{
		if (It->HasLayout())
		{
			Layout = It->GetLayout();
			bLayout = true;
			Pad = Layout.FindPad(It->GetActivePadIndex());
			break;
		}
	}
	if (!bLayout)
	{
		FString Error;
		bLayout = Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error);
		if (!bLayout)
		{
			UE_LOG(LogMageArena, Error, TEXT("Threat demo layout failed: %s"), *Error);
			return;
		}
	}
	if (!Pad)
	{
		Pad = Layout.FindPad(1);
	}
	if (!Pad)
	{
		UE_LOG(LogMageArena, Error, TEXT("Threat demo has no pad"));
		return;
	}

	bFrozen = false;
	Plan = BuildThreatLaunches(Layout, *Pad);
	for (const FThreatLaunch& LaunchPlan : Plan)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_THREAT kind=%s colour=%s landingZ=%.3f"),
			*LaunchPlan.Spec.Kind, *LaunchPlan.Spec.ColourClass, LaunchPlan.LandingM.Z);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		return;
	}

	EnsureRig();
	if (!Rig)
	{
		return;
	}
	ClearFlights();

	UStaticMesh* Sphere = Greybox::LoadShape(TEXT("Sphere"));
	UStaticMesh* Cylinder = Greybox::LoadShape(TEXT("Cylinder"));
	UStaticMesh* Cube = Greybox::LoadShape(TEXT("Cube"));
	UMaterialInterface* Material = ThreatMaterial();
	const double Now = FPlatformTime::Seconds();

	for (int32 Index = 0; Index < Plan.Num(); ++Index)
	{
		const FThreatLaunch& LaunchPlan = Plan[Index];
		const FThreatSpec& Spec = LaunchPlan.Spec;
		FFlight Flight;
		Flight.Spec = Spec;
		Flight.StartCm = LaunchPlan.StartM * 100.0;
		Flight.EndCm = LaunchPlan.EndM * 100.0;
		Flight.LandingCm = LaunchPlan.LandingM * 100.0;
		Flight.StartTime = Now + Index * Layout.ThreatStaggerS;
		Flight.Duration = FMath::Max(Spec.FlightS, 0.05);

		UStaticMesh* BodyMesh = Spec.bElongated ? Cylinder : Sphere;
		Flight.Body = Greybox::MakeMesh(Rig, BodyMesh, Greybox::Tint(Material, Rig, Opaque(Spec.Body)));
		if (Flight.Body)
		{
			const double Diameter = Spec.RadiusM * 2.0 * 100.0;
			const double Length = (Spec.bElongated ? Spec.LengthM : Spec.RadiusM * 2.0) * 100.0;
			Greybox::SetSized(Flight.Body, FVector(Diameter, Diameter, Length));
			Flight.Body->SetVisibility(false);
		}
		if (Spec.bRimmed)
		{
			// A larger opaque sphere would hide the core. A camera-facing ring leaves the centre open.
			UMaterialInstanceDynamic* RimMaterial = Greybox::Tint(Material, Rig, Opaque(Spec.Rim));
			for (int32 Segment = 0; Segment < GRimSegments; ++Segment)
			{
				if (UStaticMeshComponent* Piece = Greybox::MakeMesh(Rig, Cube, RimMaterial))
				{
					Piece->SetVisibility(false);
					Flight.RimSegments.Add(Piece);
				}
			}
		}
		UMaterialInstanceDynamic* RingMaterial = Greybox::Tint(Material, Rig, Opaque(Spec.Telegraph));
		const int32 RingCount = FMath::Max(Layout.TelegraphSegments, 3);
		for (int32 Segment = 0; Segment < RingCount; ++Segment)
		{
			if (UStaticMeshComponent* Piece = Greybox::MakeMesh(Rig, Cube, RingMaterial))
			{
				Piece->SetVisibility(false);
				Flight.RingSegments.Add(Piece);
			}
		}
		Flights.Add(Flight);
	}

	bRunning = Flights.Num() > 0;
	if (bRunning)
	{
		SetTickableTickType(ETickableTickType::Conditional);
	}
}

bool UThreatDemoSubsystem::IsShowcaseMoment() const
{
	if (!bRunning || Flights.Num() < 4)
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	for (const FFlight& Flight : Flights)
	{
		const double Progress = (Now - Flight.StartTime) / Flight.Duration;
		if (Progress <= 0.18 || Progress >= 0.90)
		{
			return false;
		}
	}
	return true;
}

TArray<FThreatColourProbe> UThreatDemoSubsystem::MakeColourProbes(const FVector& CameraWorldCm) const
{
	TArray<FThreatColourProbe> Probes;
	for (const FFlight& Flight : Flights)
	{
		if (!Flight.Body)
		{
			continue;
		}
		const FVector BodyCm = Flight.Body->GetComponentLocation();
		FThreatColourProbe BodyProbe;
		BodyProbe.Kind = Flight.Spec.Kind;
		BodyProbe.Role = TEXT("body");
		BodyProbe.WorldCm = BodyCm;
		BodyProbe.TargetSrgb = ToSrgb(Opaque(Flight.Spec.Body));
		Probes.Add(BodyProbe);

		if (!Flight.Spec.bRimmed || Flight.RimSegments.Num() == 0)
		{
			continue;
		}
		const FVector ToCamera = (CameraWorldCm - BodyCm).GetSafeNormal();
		FVector Right = FVector::CrossProduct(FVector::UpVector, ToCamera).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			Right = FVector::RightVector;
		}
		const double RimRadiusCm = Flight.Spec.RadiusM * Layout.ThreatRimScale * 100.0;
		FThreatColourProbe RimProbe;
		RimProbe.Kind = Flight.Spec.Kind;
		RimProbe.Role = TEXT("rim");
		RimProbe.WorldCm = BodyCm + Right * RimRadiusCm;
		RimProbe.TargetSrgb = ToSrgb(Opaque(Flight.Spec.Rim));
		Probes.Add(RimProbe);
	}
	return Probes;
}

void UThreatDemoSubsystem::Tick(float DeltaTime)
{
	if (!bRunning || bFrozen)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	bool bAny = false;
	for (FFlight& Flight : Flights)
	{
		UpdateFlight(Flight, Now);
		if (Now < Flight.StartTime + Flight.Duration)
		{
			bAny = true;
		}
	}
	if (!bAny)
	{
		bRunning = false;
		SetTickableTickType(ETickableTickType::Never);
	}
}

void UThreatDemoSubsystem::UpdateFlight(FFlight& Flight, double Now)
{
	const double Progress = (Now - Flight.StartTime) / Flight.Duration;
	const bool bAirborne = Progress >= 0.0 && Progress <= 1.0;
	if (Flight.Body)
	{
		Flight.Body->SetVisibility(bAirborne);
	}
	if (!bAirborne)
	{
		HideAll(Flight.RimSegments);
		HideAll(Flight.RingSegments);
		return;
	}
	const FVector Position = FMath::Lerp(Flight.StartCm, Flight.EndCm, Progress);
	if (Flight.Body)
	{
		Flight.Body->SetWorldLocation(Position);
		if (Flight.Spec.bElongated)
		{
			const FVector Direction = (Flight.EndCm - Flight.StartCm).GetSafeNormal();
			Flight.Body->SetWorldRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
		}
	}
	if (Flight.RimSegments.Num() > 0)
	{
		const FVector CameraCm = CameraOrFallback(GetWorld());
		const FVector ToCamera = (CameraCm - Position).GetSafeNormal();
		FVector Right = FVector::CrossProduct(FVector::UpVector, ToCamera).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			Right = FVector::RightVector;
		}
		const FVector Up = FVector::CrossProduct(ToCamera, Right).GetSafeNormal();
		const double RimRadiusCm = Flight.Spec.RadiusM * Layout.ThreatRimScale * 100.0;
		const double TubeCm = Flight.Spec.RadiusM * Layout.ThreatRimTubeScale * 100.0;
		const int32 Count = Flight.RimSegments.Num();
		const double Arc = (2.0 * PI * RimRadiusCm / FMath::Max(Count, 1)) * 1.08;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Angle = 2.0 * PI * Index / Count;
			const FVector Radial = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
			const FVector Tangent = -Right * FMath::Sin(Angle) + Up * FMath::Cos(Angle);
			const FQuat Rotation = FRotationMatrix::MakeFromXY(Tangent.GetSafeNormal(), Radial.GetSafeNormal()).ToQuat();
			PlaceSized(Flight.RimSegments[Index], Position + Radial * RimRadiusCm, Rotation, FVector(Arc, TubeCm, TubeCm));
		}
	}
	const double Radius = FMath::Lerp(Layout.TelegraphStartRadiusM, Layout.TelegraphEndRadiusM, FMath::Clamp(Progress, 0.0, 1.0));
	FillRing(Flight, Radius);
}

void UThreatDemoSubsystem::FillRing(FFlight& Flight, double RadiusM)
{
	const int32 Count = Flight.RingSegments.Num();
	if (Count == 0)
	{
		return;
	}
	const double RadiusCm = RadiusM * 100.0;
	const double WidthCm = Layout.TelegraphWidthM * 100.0;
	const double HeightCm = Layout.TelegraphHeightM * 100.0;
	const double Arc = (2.0 * PI * RadiusCm / Count) * 1.15;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Angle = 2.0 * PI * Index / Count;
		const FVector Offset(FMath::Cos(Angle) * RadiusCm, FMath::Sin(Angle) * RadiusCm, HeightCm * 0.5);
		const FVector Tangent(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0);
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(Tangent.GetSafeNormal(), FVector::UpVector).ToQuat();
		PlaceSized(Flight.RingSegments[Index], Flight.LandingCm + Offset, Rotation, FVector(Arc, WidthCm, HeightCm));
	}
}

TStatId UThreatDemoSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UThreatDemoSubsystem, STATGROUP_Tickables);
}

bool UThreatDemoSubsystem::IsTickable() const
{
	return bRunning && GetWorld() != nullptr;
}

UWorld* UThreatDemoSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}
