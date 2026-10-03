#include "Session/SessionPresentation.h"

#include "Session/ArenaSession.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Greybox/GreyboxUtil.h"
#include "Hands/MageArenaPawn.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "MageArenaVR.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
const FLinearColor ConscriptColour(0.45f, 0.48f, 0.52f);
const FLinearColor SlingerColour(0.55f, 0.28f, 0.05f);
const FLinearColor WaterColour(0.009721f, 0.745404f, 0.745404f);
const FLinearColor SteelColour(0.68f, 0.68f, 0.68f);
const FLinearColor FireColour(0.760525f, 0.05448f, 0.013702f);
const FLinearColor UnblockableBody(0.006995f, 0.006995f, 0.008023f);
const FLinearColor UnblockableRim(0.745404f, 0.014444f, 0.009134f);
const FLinearColor HpColour(0.65f, 0.08f, 0.05f);
const FLinearColor ManaColour(0.08f, 0.25f, 0.75f);
const FLinearColor StaminaColour(0.15f, 0.55f, 0.18f);
const FLinearColor ClockColour(0.75f, 0.55f, 0.12f);

FLinearColor FamilyColour(const FString& Family, bool bPlayerOwned)
{
	if (bPlayerOwned || Family == TEXT("magic"))
	{
		return WaterColour;
	}
	if (Family == TEXT("unblockable"))
	{
		return UnblockableBody;
	}
	if (Family == TEXT("fire"))
	{
		return FireColour;
	}
	return SteelColour;
}
}

ASessionPresentation::ASessionPresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UStaticMeshComponent* ASessionPresentation::MakePart(const TCHAR* Shape, const FLinearColor& Colour)
{
	UStaticMesh* Mesh = Greybox::LoadShape(Shape);
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* Component = Greybox::MakeMesh(this, Mesh, Greybox::Tint(Greybox::UnlitOpaqueMaterial(), this, Colour));
	return Component;
}

ASessionPresentation::FBody& ASessionPresentation::BodyFor(int32 Id, bool bSlinger)
{
	for (FBody& Body : Bodies)
	{
		if (Body.Id == Id)
		{
			return Body;
		}
	}
	for (FBody& Body : Bodies)
	{
		if (Body.Id < 0 && Body.bSlinger == bSlinger)
		{
			Body.Id = Id;
			return Body;
		}
	}
	FBody& Created = Bodies.AddDefaulted_GetRef();
	Created.Id = Id;
	Created.bSlinger = bSlinger;
	Created.Mesh = MakePart(TEXT("Cylinder"), bSlinger ? SlingerColour : ConscriptColour);
	Created.HpBack = MakePart(TEXT("Cube"), FLinearColor(0.02f, 0.02f, 0.02f));
	Created.HpFill = MakePart(TEXT("Cube"), HpColour);
	return Created;
}

ASessionPresentation::FShot& ASessionPresentation::ShotFor(int32 Id, bool bSpear)
{
	for (FShot& Shot : Shots)
	{
		if (Shot.Id == Id)
		{
			Shot.bLive = true;
			return Shot;
		}
	}
	for (FShot& Shot : Shots)
	{
		if (!Shot.bLive && Shot.GhostUntil <= 0.0 && Shot.bSpear == bSpear && Shot.Mesh)
		{
			Shot.Id = Id;
			Shot.bLive = true;
			return Shot;
		}
	}
	FShot& Created = Shots.AddDefaulted_GetRef();
	Created.Id = Id;
	Created.bLive = true;
	Created.bSpear = bSpear;
	Created.Mesh = MakePart(bSpear ? TEXT("Cylinder") : TEXT("Sphere"), bSpear ? SteelColour : WaterColour);
	return Created;
}

ASessionPresentation::FRing& ASessionPresentation::RingFor(int32 Id)
{
	for (FRing& Ring : Rings)
	{
		if (Ring.Id == Id)
		{
			return Ring;
		}
	}
	for (FRing& Ring : Rings)
	{
		if (Ring.Id < 0)
		{
			Ring.Id = Id;
			return Ring;
		}
	}
	FRing& Created = Rings.AddDefaulted_GetRef();
	Created.Id = Id;
	Created.Mesh = MakePart(TEXT("Cylinder"), SteelColour);
	Created.Rim = MakePart(TEXT("Cylinder"), UnblockableRim);
	return Created;
}

void ASessionPresentation::HideUnused(const TSet<int32>& LiveIds, double Now, bool bPaused)
{
	for (FBody& Body : Bodies)
	{
		if (Body.Id >= 0 && !LiveIds.Contains(Body.Id))
		{
			Body.Id = -1;
			if (Body.Mesh)
			{
				Body.Mesh->SetVisibility(false);
			}
			if (Body.HpBack)
			{
				Body.HpBack->SetVisibility(false);
			}
			if (Body.HpFill)
			{
				Body.HpFill->SetVisibility(false);
			}
		}
	}
	for (FShot& Shot : Shots)
	{
		if (!Shot.bLive)
		{
			continue;
		}
		if (!LiveIds.Contains(Shot.Id))
		{
			Shot.bLive = false;
			if (!bPaused)
			{
				Shot.GhostUntil = Now + 0.28;
			}
		}
	}
	if (!bPaused)
	{
		for (FShot& Shot : Shots)
		{
			if (!Shot.bLive && Shot.Mesh && Now > Shot.GhostUntil)
			{
				Shot.Mesh->SetVisibility(false);
				Shot.GhostUntil = 0.0;
				Shot.Id = -1;
			}
		}
		for (FFlash& Flash : Flashes)
		{
			if (Flash.Mesh && Now > Flash.Until)
			{
				Flash.Mesh->SetVisibility(false);
			}
		}
	}
	for (FRing& Ring : Rings)
	{
		if (Ring.Id >= 0 && !LiveIds.Contains(Ring.Id))
		{
			Ring.Id = -1;
			if (Ring.Mesh)
			{
				Ring.Mesh->SetVisibility(false);
			}
			if (Ring.Rim)
			{
				Ring.Rim->SetVisibility(false);
			}
		}
	}
}

UCameraComponent* ASessionPresentation::FindCamera() const
{
	if (!GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AMageArenaPawn> It(GetWorld()); It; ++It)
	{
		if (UCameraComponent* Camera = It->FindComponentByClass<UCameraComponent>())
		{
			return Camera;
		}
	}
	return nullptr;
}

UTexture2D* ASessionPresentation::LoadMask(const FString& Name)
{
	if (TObjectPtr<UTexture2D>* Found = Masks.Find(Name))
	{
		return Found->Get();
	}
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../art/hud/mask"), Name + TEXT(".png"));
	TArray<uint8> Compressed;
	if (!FFileHelper::LoadFileToArray(Compressed, *Path))
	{
		UE_LOG(LogMageArena, Error, TEXT("Cuff mask missing: %s"), *Path);
		Masks.Add(Name, nullptr);
		return nullptr;
	}
	IImageWrapperModule& ImageModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = ImageModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Compressed.GetData(), Compressed.Num()))
	{
		UE_LOG(LogMageArena, Error, TEXT("Cuff mask decode failed: %s"), *Path);
		Masks.Add(Name, nullptr);
		return nullptr;
	}
	TArray<uint8> Raw;
	if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
	{
		Masks.Add(Name, nullptr);
		return nullptr;
	}
	const int32 Width = Wrapper->GetWidth();
	const int32 Height = Wrapper->GetHeight();
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		UE_LOG(LogMageArena, Error, TEXT("Cuff texture create failed: %s"), *Name);
		Masks.Add(Name, nullptr);
		return nullptr;
	}
	Texture->Rename(nullptr, this);
	Texture->SRGB = true;
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Dest = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Dest, Raw.GetData(), FMath::Min(Raw.Num(), Width * Height * 4));
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	Masks.Add(Name, Texture);
	return Texture;
}

void ASessionPresentation::ShowMask(const TCHAR* Name)
{
	if (!CuffMaterial || CuffMask == Name)
	{
		return;
	}
	UTexture2D* Texture = LoadMask(Name);
	CuffMask = Name;
	if (!Texture)
	{
		return;
	}
	TArray<FMaterialParameterInfo> Infos;
	TArray<FGuid> Ids;
	CuffMaterial->GetAllTextureParameterInfo(Infos, Ids);
	if (!bLoggedTextureParams)
	{
		bLoggedTextureParams = true;
		if (Infos.Num() == 0)
		{
			UE_LOG(LogMageArena, Warning, TEXT("Cuff material has no texture parameters"));
		}
		for (const FMaterialParameterInfo& Info : Infos)
		{
			UE_LOG(LogMageArena, Log, TEXT("Cuff texture parameter %s"), *Info.Name.ToString());
		}
	}
	for (const FMaterialParameterInfo& Info : Infos)
	{
		CuffMaterial->SetTextureParameterValueByInfo(Info, Texture);
	}
}

void ASessionPresentation::EnsureCuff()
{
	if (CuffRoot && CuffRoot->GetAttachParent())
	{
		return;
	}
	UCameraComponent* Camera = FindCamera();
	if (!Camera || !GetRootComponent())
	{
		return;
	}
	CuffRoot = NewObject<USceneComponent>(this, TEXT("CuffRoot"));
	CuffRoot->SetupAttachment(Camera);
	CuffRoot->RegisterComponent();
	CuffRoot->SetRelativeLocation(FVector(58.0, -26.0, -14.0));

	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"));
	if (!Base)
	{
		UE_LOG(LogMageArena, Warning, TEXT("Cuff pass-through material missing; bars still show the kernel"));
	}
	else
	{
		CuffMaterial = UMaterialInstanceDynamic::Create(Base, this);
	}
	UStaticMesh* PlaneMesh = Greybox::LoadShape(TEXT("Plane"));
	if (PlaneMesh)
	{
		CuffPlane = Greybox::MakeMesh(this, PlaneMesh, CuffMaterial ? static_cast<UMaterialInterface*>(CuffMaterial) : Greybox::UnlitOpaqueMaterial());
		if (CuffPlane)
		{
			CuffPlane->AttachToComponent(CuffRoot, FAttachmentTransformRules::KeepRelativeTransform);
			const FMatrix Axes = FRotationMatrix::MakeFromXZ(FVector(0.0, -1.0, 0.0), FVector(-1.0, 0.0, 0.0));
			CuffPlane->SetRelativeRotation(Axes.Rotator());
			CuffPlane->SetRelativeScale3D(FVector(0.24, 0.06, 1.0));
		}
	}
	auto Bar = [this](const FLinearColor& Colour, const FVector& Offset) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Mesh = MakePart(TEXT("Cube"), Colour);
		if (Mesh && CuffRoot)
		{
			Mesh->AttachToComponent(CuffRoot, FAttachmentTransformRules::KeepRelativeTransform);
			Mesh->SetRelativeLocation(Offset);
			Mesh->SetRelativeScale3D(FVector(0.16, 0.012, 0.012));
		}
		return Mesh;
	};
	HpBar = Bar(HpColour, FVector(0.0, 0.0, -4.2));
	ManaBar = Bar(ManaColour, FVector(0.0, 0.0, -5.8));
	StaminaBar = Bar(StaminaColour, FVector(0.0, 0.0, -7.4));
	ClockBar = Bar(ClockColour, FVector(0.0, 0.0, -9.0));
	ShowMask(TEXT("idle"));
}

void ASessionPresentation::SyncCuff(const FArenaSession& Session, const FActor& Player, bool bPaused)
{
	EnsureCuff();
	const double Sim = Session.GetSimSeconds();
	auto Fit = [](UStaticMeshComponent* Bar, double Fraction)
	{
		if (!Bar)
		{
			return;
		}
		const double Width = FMath::Clamp(Fraction, 0.02, 1.0) * 0.16;
		Bar->SetRelativeScale3D(FVector(Width, 0.012, 0.012));
	};
	Fit(HpBar, Player.MaxHp > 0.0 ? Player.Hp / Player.MaxHp : 0.0);
	Fit(ManaBar, Player.MaxMana > 0.0 ? Player.Mana / Player.MaxMana : 0.0);
	Fit(StaminaBar, Player.MaxStamina > 0.0 ? Player.Stamina / Player.MaxStamina : 0.0);
	const FKernelData& Data = KernelData();
	const int32 NextTier = FMath::Clamp(Player.Tier + 1, 1, 4);
	const double NextAt = Data.UnlockAtSeconds[NextTier];
	const double Clock = (Session.GetGames().State.Tick - Player.WaveStartTick + Player.ClockAdvanceTicks) * SimDt();
	Fit(ClockBar, NextAt > 0.0 ? Clock / NextAt : 1.0);

	for (int32 Index = EventCursor; Index < Session.GetGames().State.Events.Num(); ++Index)
	{
		const FArenaEvent& Event = Session.GetGames().State.Events[Index];
		if (Event.ActorId == Player.Id && Event.Kind == TEXT("unlock"))
		{
			LastUnlockSim = Sim;
		}
		if (Event.ActorId == Player.Id && Event.Kind == TEXT("perfect"))
		{
			LastPerfectSim = Sim;
		}
	}

	const TCHAR* Mask = TEXT("idle");
	if (bPaused)
	{
		Mask = TEXT("pause");
	}
	else if (Sim - LastPerfectSim < 0.45 && LastPerfectSim >= 0.0)
	{
		Mask = TEXT("perfect-advance");
	}
	else if (Sim - LastUnlockSim < 0.45 && LastUnlockSim >= 0.0)
	{
		Mask = TEXT("unlock-flash");
	}
	else if (Player.Water.Flow >= Data.FlowMax && Data.FlowMax > 0.0)
	{
		Mask = TEXT("crest-ready");
	}
	else if (Player.Water.Flow > 0)
	{
		static const TCHAR* FlowMasks[] = {
			TEXT("flow-0"), TEXT("flow-1"), TEXT("flow-2"), TEXT("flow-3"), TEXT("flow-4"), TEXT("flow-5")};
		const int32 Flow = FMath::Clamp(Player.Water.Flow, 0, 5);
		Mask = FlowMasks[Flow];
	}
	else if (Player.Tier >= 4)
	{
		Mask = TEXT("tier4");
	}
	else if (Player.Tier == 3)
	{
		Mask = TEXT("tier3");
	}
	else if (Player.Tier == 2)
	{
		Mask = TEXT("tier2");
	}
	ShowMask(Mask);
}

void ASessionPresentation::EnsureDome()
{
	if (bDomeBuilt)
	{
		return;
	}
	bDomeBuilt = true;
	DomeAnchor = NewObject<USceneComponent>(this, TEXT("DomeAnchor"));
	DomeAnchor->SetupAttachment(GetRootComponent());
	DomeAnchor->RegisterComponent();

	UStaticMesh* SphereMesh = Greybox::LoadShape(TEXT("Sphere"));
	UStaticMesh* CylinderMesh = Greybox::LoadShape(TEXT("Cylinder"));
	// The eye is inside the shell, so this alpha tints the whole view. 0.10 keeps a readable veil.
	const FLinearColor Water(WaterColour.R, WaterColour.G, WaterColour.B, 0.10f);
	const FLinearColor Rib(WaterColour.R, WaterColour.G, WaterColour.B, 0.80f);
	auto Make = [this](UStaticMesh* Shape, const FLinearColor& Colour) -> UStaticMeshComponent*
	{
		if (!Shape || !DomeAnchor)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Material = Greybox::Tint(Greybox::TranslucentMaterial(), this, Colour);
		UStaticMeshComponent* Mesh = Greybox::MakeMesh(this, Shape, Material);
		if (Mesh)
		{
			Mesh->AttachToComponent(DomeAnchor, FAttachmentTransformRules::KeepRelativeTransform);
			Mesh->SetVisibility(false);
		}
		return Mesh;
	};
	Dome = Make(SphereMesh, Water);
	if (Dome)
	{
		Dome->SetRelativeLocation(FVector::ZeroVector);
		Greybox::SetSized(Dome, FVector(440.0, 440.0, 220.0));
		// The seated camera is inside the shell. The debug material culls back faces,
		// so the inner surface has to be the front face or the dome is invisible.
		Dome->SetReverseCulling(true);
	}
	if (!CylinderMesh)
	{
		return;
	}
	// Short rods on the ellipsoid. A scaled cylinder through the centre is a slab that
	// contains the eye (pad + 120 cm; the shell centre is pad + 110 cm).
	// The shell is 440 x 440 x 220 cm. These latitudes sit on it, 8 cm out along the normal.
	// The nearest rod surface is about 1.85 m from that eye. Only the shell is closer.
	const double RadiusXY = 220.0;
	const double RadiusZ = 110.0;
	const FVector Rod(6.0, 6.0, 32.0);
	auto AddRod = [&](double LatDeg, double LonDeg, bool bAlongRing)
	{
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double Lon = FMath::DegreesToRadians(LonDeg);
		const double CosLat = FMath::Cos(Lat);
		const double SinLat = FMath::Sin(Lat);
		const double CosLon = FMath::Cos(Lon);
		const double SinLon = FMath::Sin(Lon);
		const FVector OnShell(RadiusXY * CosLat * CosLon, RadiusXY * CosLat * SinLon, RadiusZ * SinLat);
		const FVector Normal = FVector(
			OnShell.X / (RadiusXY * RadiusXY),
			OnShell.Y / (RadiusXY * RadiusXY),
			OnShell.Z / (RadiusZ * RadiusZ)).GetSafeNormal();
		const FVector Tangent = bAlongRing
			? FVector(-CosLat * SinLon, CosLat * CosLon, 0.0).GetSafeNormal()
			: FVector(-RadiusXY * SinLat * CosLon, -RadiusXY * SinLat * SinLon, RadiusZ * CosLat).GetSafeNormal();
		UStaticMeshComponent* Segment = Make(CylinderMesh, Rib);
		if (!Segment)
		{
			return;
		}
		Segment->SetRelativeLocation(OnShell + Normal * 8.0);
		Segment->SetRelativeRotation(FRotationMatrix::MakeFromZ(Tangent).Rotator());
		Greybox::SetSized(Segment, Rod);
		DomeRibs.Add(Segment);
	};
	const double MeridianLatDeg[] = {-36.0, -18.0, 0.0, 18.0};
	for (int32 LonDeg = 0; LonDeg < 360; LonDeg += 60)
	{
		for (const double LatDeg : MeridianLatDeg)
		{
			AddRod(LatDeg, static_cast<double>(LonDeg), false);
		}
	}
	for (int32 LonDeg = 10; LonDeg < 360; LonDeg += 20)
	{
		AddRod(-42.0, static_cast<double>(LonDeg), true);
	}
}

void ASessionPresentation::SyncDome(const FArenaSession& Session, const FActor* Player)
{
	EnsureDome();
	const bool bUp = Player && Session.IsStaffPlanted();
	if (DomeAnchor)
	{
		DomeAnchor->SetVisibility(bUp, true);
	}
	if (!bUp || !Player || !DomeAnchor)
	{
		return;
	}
	FVector Centre = Session.KernelToUnrealCm(Player->Pos);
	Centre.Z += 110.0;
	DomeAnchor->SetWorldLocation(Centre);
	DomeAnchor->SetWorldRotation(FRotator::ZeroRotator);
}

void ASessionPresentation::Sync(const FArenaSession& Session, bool bPaused)
{
	if (!Session.IsRunning())
	{
		return;
	}
	const FGames& Games = Session.GetGames();
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	SyncDome(Session, Player);
	const double Now = FPlatformTime::Seconds();
	TSet<int32> LiveBodies;
	TSet<int32> LiveShots;
	TSet<int32> LiveRings;
	EnemiesOnScreen = 0;
	UCameraComponent* Camera = FindCamera();
	const FVector CamLoc = Camera ? Camera->GetComponentLocation() : FVector::ZeroVector;
	const FVector CamFwd = Camera ? Camera->GetForwardVector() : FVector::ForwardVector;

	for (const FActor& Actor : Games.State.Actors)
	{
		if (!Actor.Enemy.IsSet() || Actor.bDown)
		{
			continue;
		}
		const bool bSlinger = Actor.Enemy->Id == TEXT("slinger");
		FBody& Body = BodyFor(Actor.Id, bSlinger);
		LiveBodies.Add(Actor.Id);
		const float Height = bSlinger ? 150.f : 176.f;
		const float Width = bSlinger ? 42.f : 48.f;
		FVector Centre = Session.KernelToUnrealCm(Actor.Pos);
		Centre.Z += Height * 0.5f;
		if (Body.Mesh)
		{
			Body.Mesh->SetVisibility(true);
			Body.Mesh->SetWorldLocation(Centre);
			Greybox::SetSized(Body.Mesh, FVector(Width, Width, Height));
			const FVector To = Centre - CamLoc;
			if (Camera && FVector::DotProduct(To.GetSafeNormal(), CamFwd) > 0.45f)
			{
				++EnemiesOnScreen;
			}
		}
		const double Fraction = Actor.MaxHp > 0.0 ? FMath::Clamp(Actor.Hp / Actor.MaxHp, 0.0, 1.0) : 0.0;
		const FVector Bar = Centre + FVector(0.0, 0.0, Height * 0.5f + 18.0);
		if (Body.HpBack)
		{
			Body.HpBack->SetVisibility(true);
			Body.HpBack->SetWorldLocation(Bar);
			Greybox::SetSized(Body.HpBack, FVector(70.0, 8.0, 8.0));
		}
		if (Body.HpFill)
		{
			Body.HpFill->SetVisibility(true);
			Body.HpFill->SetWorldLocation(Bar + FVector((Fraction - 1.0) * 35.0, 0.0, 0.0));
			Greybox::SetSized(Body.HpFill, FVector(FMath::Max(4.0, 70.0 * Fraction), 8.0, 8.0));
		}
	}

	const FVrRuleset* Rules = Session.GetVrRules();
	const FVrAttackMode* SpearMode = Rules ? Rules->FindThrow(TEXT("conscript")) : nullptr;
	for (const FProjectile& Projectile : Games.State.Projectiles)
	{
		const bool bPlayerOwned = Projectile.OwnerId == Games.PlayerId;
		bool bSpear = false;
		if (Projectile.bHasAim && !bPlayerOwned)
		{
			if (const FActor* ShotOwner = SimFindActor(Games.State, Projectile.OwnerId))
			{
				bSpear = ShotOwner->Enemy.IsSet() && ShotOwner->Enemy->Id == TEXT("conscript");
			}
		}
		FShot& Shot = ShotFor(Projectile.Id, bSpear);
		LiveShots.Add(Projectile.Id);
		FVector At = Session.KernelToUnrealCm(Projectile.Pos);
		FVector Direction = FVector::UpVector;
		if (bSpear)
		{
			// Absolute metres. SurfaceHeight jumps by the dais, so the shaft would kink. Both ends sit at chest height.
			const double LengthM = SpearMode ? SpearMode->SpearLengthM : 1.7;
			const double RadiusM = SpearMode ? SpearMode->SpearRadiusM : 0.11;
			const double ApexM = SpearMode ? SpearMode->ArcApexM : 0.75;
			const double ChestM = 1.35;
			const double Span = FMath::Max(SimDistance(Projectile.OriginPos, Projectile.AimedAt), 1.0e-4);
			const double Progress = FMath::Clamp(SimDistance(Projectile.OriginPos, Projectile.Pos) / Span, 0.0, 1.0);
			At.Z = (ChestM + 4.0 * ApexM * Progress * (1.0 - Progress)) * 100.0;
			const double Slope = (4.0 * ApexM * (1.0 - 2.0 * Progress)) / Span;
			const FSimVec Flat = SimUnit(SimSub(Projectile.AimedAt, Projectile.OriginPos));
			Direction = FVector(Flat.X, Flat.Y, Slope);
			if (Shot.Mesh)
			{
				Greybox::SetSized(Shot.Mesh, FVector(RadiusM * 200.0, RadiusM * 200.0, LengthM * 100.0));
				Shot.Mesh->SetWorldRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
			}
			FRing& Ring = RingFor(Projectile.Id);
			LiveRings.Add(Projectile.Id);
			FVector Ground = Session.KernelToUnrealCm(Projectile.AimedAt);
			Ground.Z += 4.0;
			if (Ring.Mesh)
			{
				Ring.Mesh->SetVisibility(true);
				Ring.Mesh->SetWorldLocation(Ground);
				Greybox::SetSized(Ring.Mesh, FVector(160.0, 160.0, 5.0));
				if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Ring.Mesh->GetMaterial(0)))
				{
					Mid->SetVectorParameterValue(TEXT("Color"), SteelColour);
				}
			}
			if (Ring.Rim)
			{
				Ring.Rim->SetVisibility(false);
			}
		}
		else
		{
			At.Z += 120.0;
			if (Shot.Mesh)
			{
				const float Diameter = FMath::Max(18.f, static_cast<float>(Projectile.Radius * 200.0));
				Greybox::SetSized(Shot.Mesh, FVector(Diameter));
				Shot.Mesh->SetWorldRotation(FRotator::ZeroRotator);
			}
		}
		Shot.LastCm = At;
		if (Shot.Mesh)
		{
			Shot.Mesh->SetVisibility(true);
			Shot.Mesh->SetWorldLocation(At);
			if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Shot.Mesh->GetMaterial(0)))
			{
				const FLinearColor Colour = FamilyColour(Projectile.Family, bPlayerOwned);
				Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			}
		}
	}
	for (FShot& Shot : Shots)
	{
		if (!Shot.bLive && Shot.Mesh && Shot.GhostUntil > Now)
		{
			Shot.Mesh->SetVisibility(true);
			Shot.Mesh->SetWorldLocation(Shot.LastCm);
		}
	}

	for (const FTelegraph& Telegraph : Games.State.Telegraphs)
	{
		if (Telegraph.OwnerId == Games.PlayerId)
		{
			continue;
		}
		FRing& Ring = RingFor(Telegraph.Id);
		LiveRings.Add(Telegraph.Id);
		const double Span = FMath::Max(1.0, static_cast<double>(Telegraph.ResolveTick - Telegraph.StartTick));
		const double Left = FMath::Max(0.0, static_cast<double>(Telegraph.ResolveTick - Games.State.Tick));
		const double Alpha = 1.0 - Left / Span;
		const double RadiusCm = FMath::Lerp(150.0, 28.0, Alpha);
		const FSimVec Ground = Telegraph.Kind == TEXT("projectile") ? Telegraph.Target : Telegraph.Origin;
		FVector At = Session.KernelToUnrealCm(Ground);
		At.Z += 4.0;
		const bool bUnblockable = Telegraph.Family == TEXT("unblockable");
		const FLinearColor Colour = bUnblockable ? UnblockableBody : FamilyColour(Telegraph.Family, false);
		if (Ring.Mesh)
		{
			Ring.Mesh->SetVisibility(true);
			Ring.Mesh->SetWorldLocation(At);
			Greybox::SetSized(Ring.Mesh, FVector(RadiusCm * 2.0, RadiusCm * 2.0, 5.0));
			if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Ring.Mesh->GetMaterial(0)))
			{
				Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			}
		}
		if (Ring.Rim)
		{
			Ring.Rim->SetVisibility(bUnblockable);
			if (bUnblockable)
			{
				Ring.Rim->SetWorldLocation(At + FVector(0.0, 0.0, 1.0));
				Greybox::SetSized(Ring.Rim, FVector(RadiusCm * 2.2, RadiusCm * 2.2, 4.0));
			}
		}
	}

	if (Player)
	{
		for (int32 Index = EventCursor; Index < Games.State.Events.Num(); ++Index)
		{
			const FArenaEvent& Event = Games.State.Events[Index];
			if (Event.Kind != TEXT("hit"))
			{
				continue;
			}
			const FActor* Target = SimFindActor(Games.State, Event.ActorId);
			if (!Target)
			{
				continue;
			}
			FFlash* Slot = nullptr;
			for (FFlash& Flash : Flashes)
			{
				if (!Flash.Mesh || !Flash.Mesh->IsVisible())
				{
					Slot = &Flash;
					break;
				}
			}
			if (!Slot)
			{
				FFlash& Created = Flashes.AddDefaulted_GetRef();
				Created.Mesh = MakePart(TEXT("Sphere"), FLinearColor::White);
				Slot = &Created;
			}
			if (Slot && Slot->Mesh)
			{
				FVector At = Session.KernelToUnrealCm(Target->Pos);
				At.Z += 110.0;
				Slot->Mesh->SetVisibility(true);
				Slot->Mesh->SetWorldLocation(At);
				Greybox::SetSized(Slot->Mesh, FVector(46.0));
				Slot->Until = Now + 0.16;
			}
		}
		SyncCuff(Session, *Player, bPaused);
	}
	EventCursor = Games.State.Events.Num();

	TSet<int32> LiveAll = LiveBodies;
	LiveAll.Append(LiveShots);
	LiveAll.Append(LiveRings);
	HideUnused(LiveAll, Now, bPaused);
}
