#include "Greybox/ArenaGreyboxActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Greybox/GreyboxCapture.h"
#include "Greybox/GreyboxUtil.h"
#include "MageArenaVR.h"
#include "Misc/Parse.h"

namespace
{
FVector ToCm(const FVector& Metres)
{
	return Metres * 100.0;
}

void AddBox(UInstancedStaticMeshComponent* Mesh, const FVector& CentreM, const FQuat& Rotation, const FVector& SizeM)
{
	Greybox::AddSized(Mesh, ToCm(CentreM), Rotation, ToCm(SizeM));
}
}

AArenaGreybox::AArenaGreybox()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AArenaGreybox::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	FString Error;
	if (!Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error))
	{
		UE_LOG(LogMageArena, Error, TEXT("Greybox layout failed: %s"), *Error);
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	DestroyStockStage();

	UStaticMesh* Cube = Greybox::LoadShape(TEXT("Cube"));
	UStaticMesh* Cylinder = Greybox::LoadShape(TEXT("Cylinder"));
	UStaticMesh* Cone = Greybox::LoadShape(TEXT("Cone"));
	UMaterialInterface* Basic = Greybox::BasicMaterial();
	if (!Cube || !Cylinder || !Basic)
	{
		UE_LOG(LogMageArena, Error, TEXT("Greybox basic shapes or material failed to load"));
		return;
	}

	BuildShell(Cube, Cylinder);
	BuildProps(Cube, Cylinder, Cone);
	BuildLights();
	BuildSky(Cube);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_GREYBOX_READY pads=%d markers=%d"), Layout.Pads.Num(), Layout.SpawnMarkers.Num());
}

void AArenaGreybox::DestroyStockStage()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TArray<AActor*> Found;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	for (TActorIterator<ASkyAtmosphere> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	// The Entry map's floor is the engine world-grid. It z-fights the sand.
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	for (AActor* Actor : Found)
	{
		if (Actor && Actor != this)
		{
			Actor->Destroy();
		}
	}
}

void AArenaGreybox::BuildShell(UStaticMesh* Cube, UStaticMesh* Cylinder)
{
	UMaterialInterface* Basic = Greybox::BasicMaterial();
	UInstancedStaticMeshComponent* Sand = Greybox::MakeInstances(this, Cylinder, Greybox::Tint(Basic, this, Layout.Sand));
	UInstancedStaticMeshComponent* Stone = Greybox::MakeInstances(this, Cube, Greybox::Tint(Basic, this, Layout.Stone));
	UInstancedStaticMeshComponent* DaisMesh = Greybox::MakeInstances(this, Cube, Greybox::Tint(Basic, this, Layout.Dais));
	UInstancedStaticMeshComponent* PadDisc = Greybox::MakeInstances(this, Cylinder, Greybox::Tint(Basic, this, Layout.PadRing));
	UInstancedStaticMeshComponent* PadRing = Greybox::MakeInstances(this, Cube, Greybox::Tint(Basic, this, Layout.PadRing));
	UInstancedStaticMeshComponent* Seats = Greybox::MakeInstances(this, Cube, Greybox::Tint(Basic, this, Layout.Seat));

	// One centimetre above z=0 so a leftover map plane cannot win a coplanar fight.
	const FVector FloorCentre(0.0, 0.0, -Layout.FloorThicknessM * 0.5 + 0.01);
	const FVector FloorSize(Layout.FloorRadiusXM * 2.0, Layout.FloorRadiusYM * 2.0, Layout.FloorThicknessM);
	AddBox(Sand, FloorCentre, FQuat::Identity, FloorSize);
	AddBox(DaisMesh, Layout.DaisCentreM, FQuat::Identity, Layout.DaisSizeM);

	const double A = Layout.FloorRadiusXM;
	const double B = Layout.FloorRadiusYM;
	const int32 Segments = FMath::Max(Layout.WallSegments, 3);
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const double T0 = 2.0 * PI * Index / Segments;
		const double T1 = 2.0 * PI * (Index + 1) / Segments;
		const double MidAngle = (T0 + T1) * 0.5;
		const FVector P0(A * FMath::Cos(T0), B * FMath::Sin(T0), 0.0);
		const FVector P1(A * FMath::Cos(T1), B * FMath::Sin(T1), 0.0);
		const FVector Chord = P1 - P0;
		const double ChordLen = Chord.Size();
		const FVector Mid = (P0 + P1) * 0.5;
		const FVector Normal = FVector(B * FMath::Cos(MidAngle), A * FMath::Sin(MidAngle), 0.0).GetSafeNormal();
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(Chord.GetSafeNormal(), FVector::UpVector).ToQuat();

		const FVector WallCentre = Mid + Normal * (Layout.WallThicknessM * 0.5) + FVector(0.0, 0.0, Layout.WallHeightM * 0.5);
		AddBox(Stone, WallCentre, Rotation, FVector(ChordLen * 1.04, Layout.WallThicknessM, Layout.WallHeightM));

		const double MidRadius = FMath::Max(Mid.Size2D(), 0.01);
		for (int32 Tier = 0; Tier < Layout.StandTiers; ++Tier)
		{
			const double Offset = Layout.WallThicknessM + Layout.StandGapM + (static_cast<double>(Tier) + 0.5) * Layout.StandDepthM;
			const FVector StandMid = Mid + Normal * Offset;
			const double Scale = StandMid.Size2D() / MidRadius;
			const double Top = Layout.WallHeightM + Layout.StandFirstTopAboveWallM + Tier * Layout.StandStepRiseM;
			const FVector StandCentre(StandMid.X, StandMid.Y, Top * 0.5);
			AddBox(Stone, StandCentre, Rotation, FVector(ChordLen * 1.04 * Scale, Layout.StandDepthM, Top));
		}
	}

	const double DiscHeight = 0.04;
	for (const FRunePad& Pad : Layout.Pads)
	{
		const FVector Forward = Layout.FlatForwardM(Pad);
		const FQuat PadRotation = FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).ToQuat();
		const FVector DiscCentre(Pad.PositionM.X, Pad.PositionM.Y, Pad.PositionM.Z + DiscHeight * 0.5);
		AddBox(PadDisc, DiscCentre, FQuat::Identity, FVector(Layout.PadDiscRadiusM * 2.0, Layout.PadDiscRadiusM * 2.0, DiscHeight));

		const int32 RingSegments = FMath::Max(Layout.PadRingSegments, 3);
		for (int32 Segment = 0; Segment < RingSegments; ++Segment)
		{
			const double Angle = 2.0 * PI * Segment / RingSegments;
			const FVector Offset(FMath::Cos(Angle) * Layout.PadRingRadiusM, FMath::Sin(Angle) * Layout.PadRingRadiusM, 0.0);
			const FVector Tangent(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0);
			const FQuat RingRotation = FRotationMatrix::MakeFromXZ(Tangent.GetSafeNormal(), FVector::UpVector).ToQuat();
			const double Arc = (2.0 * PI * Layout.PadRingRadiusM / RingSegments) * 1.08;
			const FVector RingCentre = FVector(Pad.PositionM.X, Pad.PositionM.Y, Pad.PositionM.Z) + Offset
				+ FVector(0.0, 0.0, Layout.PadRingHeightM * 0.5);
			AddBox(PadRing, RingCentre, RingRotation, FVector(Arc, Layout.PadRingWidthM, Layout.PadRingHeightM));
		}

		const FVector SeatCentre(Pad.PositionM.X, Pad.PositionM.Y, Pad.PositionM.Z + Layout.SeatSizeM.Z * 0.5);
		AddBox(Seats, SeatCentre, PadRotation, Layout.SeatSizeM);
	}
}

void AArenaGreybox::BuildProps(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Cone)
{
	UMaterialInterface* Basic = Greybox::BasicMaterial();
	UInstancedStaticMeshComponent* Brazier = Greybox::MakeInstances(this, Cylinder, Greybox::Tint(Basic, this, Layout.Brazier));
	FLinearColor Flame = Layout.Flame;
	Flame.A = 1.f;
	// Same lit basic material as the shell. The unlit debug material cannot instance in -game.
	UInstancedStaticMeshComponent* Flames = Cone ? Greybox::MakeInstances(this, Cone, Greybox::Tint(Basic, this, Flame)) : nullptr;
	UInstancedStaticMeshComponent* Poles = Greybox::MakeInstances(this, Cylinder, Greybox::Tint(Basic, this, Layout.BannerPole));
	UInstancedStaticMeshComponent* Bars = Greybox::MakeInstances(this, Cube, Greybox::Tint(Basic, this, Layout.BannerPole));
	UInstancedStaticMeshComponent* Cloth = Greybox::MakeInstances(this, Cube, Greybox::Tint(Basic, this, Layout.BannerCloth));
	UInstancedStaticMeshComponent* Markers = Greybox::MakeInstances(this, Cylinder, Greybox::Tint(Basic, this, Layout.Marker));

	for (const FArenaProp& Prop : Layout.Braziers)
	{
		const double StandRadius = Prop.BowlRadiusM * 0.35;
		const FVector StandCentre(Prop.PositionM.X, Prop.PositionM.Y, Prop.StandHeightM * 0.5);
		AddBox(Brazier, StandCentre, FQuat::Identity, FVector(StandRadius * 2.0, StandRadius * 2.0, Prop.StandHeightM));
		const FVector BowlCentre(Prop.PositionM.X, Prop.PositionM.Y, Prop.StandHeightM + Prop.BowlHeightM * 0.5);
		AddBox(Brazier, BowlCentre, FQuat::Identity, FVector(Prop.BowlRadiusM * 2.0, Prop.BowlRadiusM * 2.0, Prop.BowlHeightM));
		const double FlameHeight = Prop.FlameRadiusM * 2.5;
		const FVector FlameCentre(Prop.PositionM.X, Prop.PositionM.Y, Prop.StandHeightM + Prop.BowlHeightM + FlameHeight * 0.5);
		AddBox(Flames, FlameCentre, FQuat::Identity, FVector(Prop.FlameRadiusM * 2.0, Prop.FlameRadiusM * 2.0, FlameHeight));
	}

	for (const FArenaProp& Prop : Layout.Banners)
	{
		const FVector PoleCentre(Prop.PositionM.X, Prop.PositionM.Y, Prop.PoleHeightM * 0.5);
		AddBox(Poles, PoleCentre, FQuat::Identity, FVector(Prop.PoleRadiusM * 2.0, Prop.PoleRadiusM * 2.0, Prop.PoleHeightM));
		FVector ToCentre = Layout.ArenaCentreM - Prop.PositionM;
		ToCentre.Z = 0.0;
		ToCentre = ToCentre.GetSafeNormal();
		if (ToCentre.IsNearlyZero())
		{
			ToCentre = FVector::ForwardVector;
		}
		const FQuat ClothRotation = FRotationMatrix::MakeFromXZ(ToCentre, FVector::UpVector).ToQuat();
		const double ClothZ = Prop.PoleHeightM - Prop.ClothSizeM.Z * 0.5 - 0.08;
		const FVector ClothCentre(Prop.PositionM.X, Prop.PositionM.Y, ClothZ);
		AddBox(Cloth, ClothCentre + ToCentre * (Prop.ClothSizeM.X * 0.5), ClothRotation, Prop.ClothSizeM);
		const FVector BarSize(0.08, Prop.ClothSizeM.Y, 0.06);
		const FVector BarCentre(Prop.PositionM.X, Prop.PositionM.Y, Prop.PoleHeightM - 0.05);
		AddBox(Bars, BarCentre, ClothRotation, BarSize);
	}

	for (const FSpawnMarker& Marker : Layout.SpawnMarkers)
	{
		const FVector Centre(Marker.PositionM.X, Marker.PositionM.Y, Layout.MarkerPostHeightM * 0.5);
		AddBox(Markers, Centre, FQuat::Identity, FVector(Layout.MarkerPostRadiusM * 2.0, Layout.MarkerPostRadiusM * 2.0, Layout.MarkerPostHeightM));
	}
}

void AArenaGreybox::BuildLights()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform::Identity, Params);
	if (Sun)
	{
		Sun->SetActorRotation(FRotator(Layout.DirectionalPitchDeg, Layout.DirectionalYawDeg, 0.0));
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(static_cast<float>(Layout.DirectionalIntensityLux));
			Light->SetLightColor(Layout.DirectionalColour);
			Light->SetCastShadows(Layout.bCastShadows);
			Light->SetAtmosphereSunLight(false);
		}
	}

	ASkyLight* Sky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity, Params);
	if (Sky)
	{
		if (USkyLightComponent* Light = Sky->GetLightComponent())
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
			Light->SetIntensity(static_cast<float>(Layout.SkyIntensity));
			Light->SetLightColor(Layout.SkyLightColour);
			Light->SetCastShadows(false);
			Light->bLowerHemisphereIsBlack = false;
			if (UTextureCube* Cube = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/EngineResources/GrayLightTextureCube.GrayLightTextureCube")))
			{
				Light->SetCubemap(Cube);
			}
			else
			{
				UE_LOG(LogMageArena, Warning, TEXT("Greybox sky cubemap missing"));
			}
			Light->RecaptureSky();
		}
	}
}

void AArenaGreybox::BuildSky(UStaticMesh* Cube)
{
	if (!Cube)
	{
		return;
	}
	UMaterialInterface* Unlit = Greybox::UnlitOpaqueMaterial();
	if (!Unlit)
	{
		UE_LOG(LogMageArena, Warning, TEXT("Dusk dome unlit material missing"));
		return;
	}
	// The engine sky atmosphere stays black in this offscreen capture. An inward-facing
	// unlit dome is the background. LevelColorationUnlitMaterial is not two-sided, so
	// each panel's +X points at the arena.
	UMaterialInterface* Horizon = Greybox::Tint(Unlit, this, Layout.SkyHorizon);
	UMaterialInterface* Upper = Greybox::Tint(Unlit, this, Layout.SkyUpper);
	UMaterialInterface* Zenith = Greybox::Tint(Unlit, this, Layout.SkyZenith);

	const double Radius = 70.0;
	const int32 Azimuth = 4;
	const double Thickness = 2.0;
	const double LowerCentreZ = 18.0;
	const double LowerHeight = 40.0;
	const double UpperCentreZ = 52.0;
	const double UpperHeight = 36.0;

	auto AddPanel = [this, Cube](UMaterialInterface* Material, const FVector& CentreM, const FQuat& Rotation, const FVector& SizeM)
	{
		UStaticMeshComponent* Panel = Greybox::MakeMesh(this, Cube, Material);
		if (!Panel)
		{
			return;
		}
		Greybox::SetSized(Panel, SizeM * 100.0);
		Panel->SetWorldLocation(CentreM * 100.0);
		Panel->SetWorldRotation(Rotation);
	};

	// Index/Azimuth puts one panel on +X, where the seated player looks. The old
	// half-step angles left the clear colour in a vertical gap straight ahead.
	// A flat panel on the circle must span a full diameter or the corners stay open.
	for (int32 Index = 0; Index < Azimuth; ++Index)
	{
		const double Angle = 2.0 * PI * static_cast<double>(Index) / static_cast<double>(Azimuth);
		const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
		const FQuat Rotation = FRotationMatrix::MakeFromX(-Radial).ToQuat();
		const double Chord = 2.0 * Radius * 1.2;
		const FVector Place = Radial * Radius;
		AddPanel(Horizon, Place + FVector(0.0, 0.0, LowerCentreZ), Rotation, FVector(Thickness, Chord, LowerHeight));
		AddPanel(Upper, Place + FVector(0.0, 0.0, UpperCentreZ), Rotation, FVector(Thickness, Chord, UpperHeight));
	}

	const FQuat CapRotation = FRotationMatrix::MakeFromX(FVector(0.0, 0.0, -1.0)).ToQuat();
	AddPanel(Zenith, FVector(0.0, 0.0, 68.0), CapRotation, FVector(Thickness, 160.0, 160.0));
}

bool UArenaGreyboxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UArenaGreyboxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AArenaGreybox* Arena = InWorld.SpawnActor<AArenaGreybox>(AArenaGreybox::StaticClass(), FTransform::Identity, Params))
	{
		Arena->Build();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("MageArenaGreyboxCapture")))
	{
		Capture = NewObject<UGreyboxCaptureDriver>(this);
		Capture->Start();
	}
}
