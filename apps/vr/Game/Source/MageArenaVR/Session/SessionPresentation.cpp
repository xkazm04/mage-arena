#include "Session/SessionPresentation.h"

#include "Session/ArenaSession.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Greybox/GreyboxUtil.h"
#include "Hands/MageArenaPawn.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimMath.h"
#include "Kernel/SimTypes.h"
#include "Kernel/VrRules.h"
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
const FLinearColor TrainingDummyColour(0.86f, 0.78f, 0.62f);
const FLinearColor SignPlateColour(0.015f, 0.016f, 0.02f);
const FLinearColor GlyphPlateColour(0.01f, 0.012f, 0.014f);

FVector PadForward(const FArenaSession& Session)
{
	FVector Forward(1.0, 0.0, 0.0);
	if (Session.HasLayout())
	{
		if (const FRunePad* Pad = Session.GetLayout().FindPad(Session.GetActivePad()))
		{
			Forward = Session.GetLayout().FlatForwardM(*Pad);
		}
	}
	if (Forward.IsNearlyZero())
	{
		return FVector(1.0, 0.0, 0.0);
	}
	return Forward.GetSafeNormal();
}

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

UTexture2D* ASessionPresentation::LoadPng(const FString& Name, const FString& Path)
{
	if (TObjectPtr<UTexture2D>* Found = Masks.Find(Name))
	{
		return Found->Get();
	}
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

UTexture2D* ASessionPresentation::LoadMask(const FString& Name)
{
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../art/hud/mask"), Name + TEXT(".png"));
	return LoadPng(Name, Path);
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
	CuffCue = NewObject<UTextRenderComponent>(this, TEXT("CuffCue"));
	if (CuffCue && CuffRoot)
	{
		CuffCue->SetupAttachment(CuffRoot);
		CuffCue->RegisterComponent();
		if (GEngine && GEngine->GetMediumFont())
		{
			CuffCue->SetFont(GEngine->GetMediumFont());
		}
		CuffCue->SetWorldSize(3.5f);
		CuffCue->SetTextRenderColor(FColor(236, 232, 220));
		CuffCue->SetHorizontalAlignment(EHTA_Center);
		CuffCue->SetRelativeLocation(FVector(-1.0, 0.0, 4.0));
		CuffCue->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
		CuffCue->SetVisibility(false);
	}
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
	if (CuffCue)
	{
		const FString Cue = Session.GetWristCue();
		CuffCue->SetText(FText::FromString(Cue));
		CuffCue->SetVisibility(!Cue.IsEmpty());
	}
}

void ASessionPresentation::EnsureTeachVisuals()
{
	if (bTeachVisuals || !GetRootComponent())
	{
		return;
	}
	bTeachVisuals = true;
	DaisWidget = NewObject<UWidgetComponent>(this, TEXT("DaisPrompt"));
	if (DaisWidget)
	{
		DaisWidget->SetupAttachment(GetRootComponent());
		DaisWidget->SetWidgetSpace(EWidgetSpace::World);
		DaisWidget->SetDrawSize(FVector2D(1100.f, 180.f));
		DaisWidget->SetPivot(FVector2D(0.5f, 0.5f));
		DaisWidget->SetTwoSided(true);
		DaisWidget->SetBlendMode(EWidgetBlendMode::Transparent);
		DaisWidget->SetBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		DaisWidget->SetTintColorAndOpacity(FLinearColor::White);
		DaisWidget->SetTickMode(ETickMode::Enabled);
		DaisWidget->SetTickWhenOffscreen(true);
		DaisWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DaisWidget->SetCastShadow(false);
		DaisWidget->SetVisibility(false);
		DaisWidget->RegisterComponent();
		AddInstanceComponent(DaisWidget);
		DaisWidget->SetSlateWidget(
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 52))
			.ColorAndOpacity(FLinearColor(1.f, 0.97f, 0.90f, 1.f))
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
			.WrapTextAt(1040.f));
	}
	DaisPlate = MakePart(TEXT("Cube"), SignPlateColour);
	if (DaisPlate)
	{
		DaisPlate->SetVisibility(false);
	}
	GlyphAnchor = NewObject<USceneComponent>(this, TEXT("GlyphAnchor"));
	if (GlyphAnchor)
	{
		GlyphAnchor->SetupAttachment(GetRootComponent());
		GlyphAnchor->RegisterComponent();
	}
	UMaterialInterface* Pass = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"));
	UStaticMesh* PlaneMesh = Greybox::LoadShape(TEXT("Plane"));
	if (Pass)
	{
		GlyphMaterial = UMaterialInstanceDynamic::Create(Pass, this);
	}
	if (PlaneMesh && GlyphAnchor)
	{
		UMaterialInterface* GlyphMat = GlyphMaterial
			? static_cast<UMaterialInterface*>(GlyphMaterial)
			: Greybox::Tint(Greybox::UnlitOpaqueMaterial(), this, FLinearColor::White);
		GlyphPlane = Greybox::MakeMesh(this, PlaneMesh, GlyphMat);
		if (GlyphPlane)
		{
			GlyphPlane->AttachToComponent(GlyphAnchor, FAttachmentTransformRules::KeepRelativeTransform);
			// Engine plane faces local +Z. Point that at the seated player (anchor -X).
			GlyphPlane->SetRelativeRotation(FRotationMatrix::MakeFromZ(FVector(-1.0, 0.0, 0.0)).Rotator());
			GlyphPlane->SetRelativeScale3D(FVector(0.82, 0.82, 1.0));
			GlyphPlane->SetVisibility(false);
		}
	}
	GlyphBack = MakePart(TEXT("Cube"), GlyphPlateColour);
	if (GlyphBack && GlyphAnchor)
	{
		GlyphBack->AttachToComponent(GlyphAnchor, FAttachmentTransformRules::KeepRelativeTransform);
		GlyphBack->SetRelativeLocation(FVector(3.0, 0.0, 0.0));
		Greybox::SetSized(GlyphBack, FVector(3.0, 92.0, 92.0));
		GlyphBack->SetVisibility(false);
	}
	// Greybox of A03 sigil-line1 (circle, then the bar) if the mask plane does not draw.
	for (int32 Index = 0; Index < 18; ++Index)
	{
		UStaticMeshComponent* Bead = MakePart(TEXT("Cube"), FLinearColor::White);
		if (!Bead || !GlyphAnchor)
		{
			continue;
		}
		const double Angle = 2.0 * PI * static_cast<double>(Index) / 18.0;
		Bead->AttachToComponent(GlyphAnchor, FAttachmentTransformRules::KeepRelativeTransform);
		Bead->SetRelativeLocation(FVector(1.5, FMath::Cos(Angle) * 34.0, FMath::Sin(Angle) * 34.0));
		Greybox::SetSized(Bead, FVector(5.0, 8.0, 8.0));
		Bead->SetVisibility(false);
		GlyphStrokes.Add(Bead);
	}
	if (UStaticMeshComponent* Bar = MakePart(TEXT("Cube"), FLinearColor::White))
	{
		if (GlyphAnchor)
		{
			Bar->AttachToComponent(GlyphAnchor, FAttachmentTransformRules::KeepRelativeTransform);
			Bar->SetRelativeLocation(FVector(1.5, 0.0, 4.0));
			Greybox::SetSized(Bar, FVector(5.0, 7.0, 40.0));
			Bar->SetVisibility(false);
			GlyphStrokes.Add(Bar);
		}
	}
	for (int32 Index = 0; Index < 20; ++Index)
	{
		if (UStaticMeshComponent* Bead = MakePart(TEXT("Cube"), FLinearColor::White))
		{
			Bead->SetVisibility(false);
			PerfectRingParts.Add(Bead);
		}
	}
	FString GlyphPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("../art/glyphs/mask/sigil-line1-512.png"));
	FPaths::CollapseRelativeDirectories(GlyphPath);
	if (UTexture2D* Glyph = LoadPng(TEXT("glyph-sigil-line1"), GlyphPath))
	{
		if (GlyphMaterial)
		{
			TArray<FMaterialParameterInfo> Infos;
			TArray<FGuid> Ids;
			GlyphMaterial->GetAllTextureParameterInfo(Infos, Ids);
			for (const FMaterialParameterInfo& Info : Infos)
			{
				GlyphMaterial->SetTextureParameterValueByInfo(Info, Glyph);
			}
		}
	}
}

void ASessionPresentation::SyncTeachVisuals(const FArenaSession& Session, const FActor* Player)
{
	EnsureTeachVisuals();
	const FVector Forward = PadForward(Session);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FVector Pad = Session.KernelToUnrealCm(Session.PadKernel(Session.GetActivePad()));
	const FString Prompt = Session.GetPromptText();
	// Centred on the seated forward axis, 220 cm ahead and 162 cm above the pad.
	// From the eye that is about 15° above the view centre. The 196 cm plate is
	// atan(98/220) ≈ 24° either side, inside ±30°. The cuff is camera-locked at
	// the lower left, and sigils are drawn in front of the hands, so the plaque
	// sits above both and above the dummy.
	const FVector Sign = Pad + Forward * 220.0 + FVector(0.0, 0.0, 162.0);
	const FRotator Facing = Forward.Rotation();
	if (DaisWidget)
	{
		const bool bShow = !Prompt.IsEmpty();
		DaisWidget->SetVisibility(bShow);
		DaisWidget->SetWorldLocation(Sign - Forward * 8.0);
		DaisWidget->SetWorldRotation((-Forward).Rotation());
		DaisWidget->SetWorldScale3D(FVector(0.16f));
		if (bShow)
		{
			if (TSharedPtr<STextBlock> Label = StaticCastSharedPtr<STextBlock>(DaisWidget->GetSlateWidget()))
			{
				Label->SetText(FText::FromString(Prompt));
			}
			DaisWidget->RequestRenderUpdate();
		}
	}
	if (DaisPlate)
	{
		DaisPlate->SetVisibility(!Prompt.IsEmpty());
		DaisPlate->SetWorldLocation(Sign + Forward * 2.0);
		DaisPlate->SetWorldRotation(Facing);
		Greybox::SetSized(DaisPlate, FVector(4.0, 196.0, 36.0));
	}
	const bool bGlyph = Session.GetGlyphId() == TEXT("sigil-line1");
	if (GlyphAnchor)
	{
		GlyphAnchor->SetVisibility(bGlyph, true);
		if (bGlyph)
		{
			const FVector At = Pad + Forward * 230.0 + Right * 78.0 + FVector(0.0, 0.0, 88.0);
			GlyphAnchor->SetWorldLocation(At);
			GlyphAnchor->SetWorldRotation(Facing);
		}
	}
	if (GlyphPlane)
	{
		GlyphPlane->SetVisibility(bGlyph);
	}
	if (GlyphBack)
	{
		GlyphBack->SetVisibility(bGlyph);
	}
	for (UStaticMeshComponent* Stroke : GlyphStrokes)
	{
		if (Stroke)
		{
			// The mask plane already draws the circle and the bar. The beads would double it.
			Stroke->SetVisibility(false);
		}
	}
	const bool bRing = Session.IsPerfectRingVisible() && Player;
	FVector RingCentre = Pad + Forward * 90.0 + FVector(0.0, 0.0, 120.0);
	if (bThreatCm)
	{
		RingCentre = ThreatCm;
	}
	const int32 RingCount = PerfectRingParts.Num();
	for (int32 Index = 0; Index < RingCount; ++Index)
	{
		UStaticMeshComponent* Bead = PerfectRingParts[Index];
		if (!Bead)
		{
			continue;
		}
		Bead->SetVisibility(bRing);
		if (!bRing)
		{
			continue;
		}
		const double Angle = 2.0 * PI * static_cast<double>(Index) / static_cast<double>(RingCount);
		const FVector Offset = (Right * FMath::Cos(Angle) + FVector::UpVector * FMath::Sin(Angle)) * 72.0;
		const FVector Radial = Offset.GetSafeNormal();
		const FVector Tangent = FVector::CrossProduct(Radial, Forward).GetSafeNormal();
		Bead->SetWorldLocation(RingCentre + Offset);
		Bead->SetWorldRotation(FRotationMatrix::MakeFromXZ(Radial, Tangent).Rotator());
		Greybox::SetSized(Bead, FVector(8.0, 10.0, 24.0));
	}
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

void ASessionPresentation::SyncWalls(const FArenaSession& Session)
{
	const FVrRuleset* Rules = Session.GetVrRules();
	int32 Used = 0;
	if (Rules && Rules->FireWall.bEnabled)
	{
		UStaticMesh* CylinderMesh = Greybox::LoadShape(TEXT("Cylinder"));
		const FLinearColor Curtain(FireColour.R, FireColour.G, FireColour.B, 0.88f);
		for (const FVrWall& Wall : Rules->Walls)
		{
			const int32 Segments = 8;
			const double Arc = Wall.ArcDeg * SimPi / 180.0;
			for (int32 Index = 0; Index < Segments; ++Index)
			{
				const double Angle = -Arc * 0.5 + Arc * (static_cast<double>(Index) + 0.5) / static_cast<double>(Segments);
				const FSimVec Direction = SimRotate(Wall.Facing, Angle);
				const FSimVec Point = SimAdd(Wall.Centre, SimScale(Direction, Wall.DistanceM));
				if (!WallSlats.IsValidIndex(Used))
				{
					UMaterialInstanceDynamic* Material = Greybox::Tint(Greybox::TranslucentMaterial(), this, Curtain);
					UStaticMeshComponent* Mesh = Greybox::MakeMesh(this, CylinderMesh, Material);
					if (Mesh)
					{
						Mesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
					}
					WallSlats.Add(Mesh);
				}
				UStaticMeshComponent* Slat = WallSlats[Used];
				if (Slat)
				{
					FVector World = Session.KernelToUnrealCm(Point);
					World.Z += 60.0;
					Slat->SetWorldLocation(World);
					Slat->SetWorldRotation(FRotator::ZeroRotator);
					Greybox::SetSized(Slat, FVector(16.0, 36.0, 120.0));
					Slat->SetVisibility(true);
				}
				++Used;
			}
		}
	}
	for (int32 Index = Used; Index < WallSlats.Num(); ++Index)
	{
		if (WallSlats[Index])
		{
			WallSlats[Index]->SetVisibility(false);
		}
	}
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
	SyncWalls(Session);
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
		if (Actor.Id == Games.PlayerId || Actor.bDown)
		{
			continue;
		}
		if (!Actor.Enemy.IsSet() && !Actor.bDummy)
		{
			continue;
		}
		const bool bSlinger = Actor.Enemy.IsSet() && Actor.Enemy->Id == TEXT("slinger");
		FBody& Body = BodyFor(Actor.Id, bSlinger);
		LiveBodies.Add(Actor.Id);
		const float Height = bSlinger ? 150.f : 176.f;
		const float Width = bSlinger ? 42.f : 48.f;
		FVector Centre = Session.KernelToUnrealCm(Actor.Pos);
		Centre.Z += Height * 0.5f;
		const FLinearColor BodyColour = Actor.bDummy ? TrainingDummyColour : (bSlinger ? SlingerColour : ConscriptColour);
		if (Body.Mesh)
		{
			Body.Mesh->SetVisibility(true);
			Body.Mesh->SetWorldLocation(Centre);
			Greybox::SetSized(Body.Mesh, FVector(Width, Width, Height));
			if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Body.Mesh->GetMaterial(0)))
			{
				Mid->SetVectorParameterValue(TEXT("Color"), BodyColour);
			}
			const FVector To = Centre - CamLoc;
			if (!Actor.bDummy && Camera && FVector::DotProduct(To.GetSafeNormal(), CamFwd) > 0.45f)
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
		Shot.bOwned = bPlayerOwned;
		if (!bPlayerOwned)
		{
			bThreatCm = true;
			ThreatCm = At;
		}
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
			if (!Shot.bOwned)
			{
				bThreatCm = true;
				ThreatCm = Shot.LastCm;
			}
		}
	}

	UStaticMesh* CubeMesh = Greybox::LoadShape(TEXT("Cube"));
	UStaticMesh* CylinderMesh = Greybox::LoadShape(TEXT("Cylinder"));
	auto UseMesh = [](UStaticMeshComponent* Mesh, UStaticMesh* Shape, const FLinearColor& Colour)
	{
		if (!Mesh || !Shape)
		{
			return;
		}
		if (Mesh->GetStaticMesh() != Shape)
		{
			Mesh->SetStaticMesh(Shape);
			Mesh->SetMaterial(0, Greybox::Tint(Greybox::UnlitOpaqueMaterial(), Mesh, Colour));
		}
	};
	for (const FTelegraph& Telegraph : Games.State.Telegraphs)
	{
		if (Telegraph.OwnerId == Games.PlayerId)
		{
			continue;
		}
		FRing& Ring = RingFor(Telegraph.Id);
		LiveRings.Add(Telegraph.Id);
		// A ring under the seat sits below the seated view. A lane is a strip the eye can see.
		if (Telegraph.Kind == TEXT("lane"))
		{
			FVector From = Session.KernelToUnrealCm(Telegraph.Origin);
			FVector To = Session.KernelToUnrealCm(Telegraph.Target);
			From.Z += 8.0;
			To.Z += 8.0;
			const FVector Delta = To - From;
			const double Length = FMath::Max(Delta.Size(), 50.0);
			const FVector Dir = Delta.GetSafeNormal();
			const FRotator Rot = Dir.Rotation();
			const FVector Mid = From + Dir * (Length * 0.5);
			const double Width = FMath::Max(FMath::Clamp(Telegraph.WidthM, 0.5, 3.0), 1.8) * 100.0;
			UseMesh(Ring.Mesh, CubeMesh, UnblockableBody);
			UseMesh(Ring.Rim, CubeMesh, UnblockableRim);
			if (Ring.Mesh)
			{
				Ring.Mesh->SetVisibility(true);
				Ring.Mesh->SetWorldLocation(Mid);
				Ring.Mesh->SetWorldRotation(Rot);
				Greybox::SetSized(Ring.Mesh, FVector(Length, Width, 8.0));
				if (UMaterialInstanceDynamic* MidMat = Cast<UMaterialInstanceDynamic>(Ring.Mesh->GetMaterial(0)))
				{
					MidMat->SetVectorParameterValue(TEXT("Color"), UnblockableBody);
				}
			}
			if (Ring.Rim)
			{
				Ring.Rim->SetVisibility(true);
				Ring.Rim->SetWorldLocation(Mid - FVector(0.0, 0.0, 3.0));
				Ring.Rim->SetWorldRotation(Rot);
				Greybox::SetSized(Ring.Rim, FVector(Length + 24.0, Width + 36.0, 5.0));
				if (UMaterialInstanceDynamic* RimMat = Cast<UMaterialInstanceDynamic>(Ring.Rim->GetMaterial(0)))
				{
					RimMat->SetVectorParameterValue(TEXT("Color"), UnblockableRim);
				}
			}
			continue;
		}
		UseMesh(Ring.Mesh, CylinderMesh, SteelColour);
		UseMesh(Ring.Rim, CylinderMesh, UnblockableRim);
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
			Ring.Mesh->SetWorldRotation(FRotator::ZeroRotator);
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
				Ring.Rim->SetWorldRotation(FRotator::ZeroRotator);
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
				const bool bTeachFlash = Session.GetStage() == TEXT("teach") || Target->bDummy;
				Slot->Until = Now + (bTeachFlash ? 1.2 : 0.16);
			}
		}
		SyncCuff(Session, *Player, bPaused);
	}
	SyncTeachVisuals(Session, Player);
	EventCursor = Games.State.Events.Num();

	TSet<int32> LiveAll = LiveBodies;
	LiveAll.Append(LiveShots);
	LiveAll.Append(LiveRings);
	HideUnused(LiveAll, Now, bPaused);
}
