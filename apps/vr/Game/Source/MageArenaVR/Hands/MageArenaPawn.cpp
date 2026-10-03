#include "Hands/MageArenaPawn.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Greybox/HandPresentationComponent.h"
#include "HAL/IConsoleManager.h"
#include "MageArenaVR.h"

AMageArenaPawn::AMageArenaPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SeatedOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("SeatedOrigin"));
	SeatedOrigin->SetupAttachment(SceneRoot);
	// Hip of the clip frame. Eyes are 8 cm forward and 62 cm up, so the hip sits 8 cm back and 58 cm up
	// when the eyes are 120 cm above the pad. Mouse look turns the camera, not this component.
	SeatedOrigin->SetRelativeLocation(FVector(-8.0, 0.0, 58.0));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("SeatedCamera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->SetRelativeLocation(FVector(0.0, 0.0, 120.0));
	Camera->SetFieldOfView(90.f);
	Camera->bUsePawnControlRotation = true;

	Hands = CreateDefaultSubobject<UHandPresentationComponent>(TEXT("Hands"));
	Hands->SetupAttachment(SeatedOrigin);
}

void AMageArenaPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	FString Error;
	FArenaLayout Loaded;
	if (!Loaded.LoadFromFile(FArenaLayout::DefaultFilePath(), Error))
	{
		UE_LOG(LogMageArena, Error, TEXT("Arena layout failed: %s"), *Error);
		bHasLayout = false;
		return;
	}
	ConfigureFromLayout(Loaded);
}

void AMageArenaPawn::ConfigureFromLayout(const FArenaLayout& InLayout)
{
	Layout = InLayout;
	bHasLayout = true;
	const double EyeCm = FArenaLayout::MetresToCentimetres(Layout.SeatedEyeHeightAbovePadM);
	const double ForwardCm = FArenaLayout::MetresToCentimetres(Layout.EyeForwardOfSeatedOriginM);
	const double AboveCm = FArenaLayout::MetresToCentimetres(Layout.EyeAboveSeatedOriginM);
	if (Camera)
	{
		Camera->SetRelativeLocation(FVector(0.0, 0.0, EyeCm));
		Camera->SetFieldOfView(static_cast<float>(Layout.CameraFovDeg));
		FPostProcessSettings& Post = Camera->PostProcessSettings;
		Post.bOverride_AutoExposureMethod = true;
		Post.AutoExposureMethod = AEM_Manual;
		Post.bOverride_AutoExposureBias = true;
		Post.AutoExposureBias = 0.f;
		Post.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Post.AutoExposureApplyPhysicalCameraExposure = 0;
		Post.bOverride_BloomIntensity = true;
		Post.BloomIntensity = 0.f;
		Post.bOverride_LocalExposureHighlightContrastScale = true;
		Post.LocalExposureHighlightContrastScale = 1.f;
		Post.bOverride_LocalExposureShadowContrastScale = true;
		Post.LocalExposureShadowContrastScale = 1.f;
	}
	if (SeatedOrigin)
	{
		SeatedOrigin->SetRelativeLocation(FVector(-ForwardCm, 0.0, EyeCm - AboveCm));
	}
}

void AMageArenaPawn::ApplyFlatPresentation()
{
	// Global cvars stay out of ConfigureFromLayout so a headless test cannot change the editor.
	if (!GetWorld())
	{
		return;
	}
	if (IConsoleVariable* PreExposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride")))
	{
		PreExposure->Set(1.f);
	}
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->EngineShowFlags.SetTonemapper(false);
	}
}

void AMageArenaPawn::BeginPlay()
{
	Super::BeginPlay();
	ApplyFlatPresentation();
	if (bHasLayout)
	{
		SeatOnPad(1);
	}
}

void AMageArenaPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ApplyLook();
}

void AMageArenaPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (BlinkPhase == EBlinkPhase::Idle)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - BlinkPhaseStart;
	if (BlinkPhase == EBlinkPhase::FadeOut)
	{
		const double Duration = FMath::Max(Layout.BlinkFadeOutS, 0.01);
		SetVignette(FMath::Clamp(Elapsed / Duration, 0.0, 1.0) * Layout.BlinkVignettePeak);
		if (Elapsed >= Duration)
		{
			SeatOnPad(PendingPad);
			BlinkPhase = EBlinkPhase::FadeIn;
			BlinkPhaseStart = Now;
			StartFade(1.f, 0.f, Layout.BlinkFadeInS, false);
		}
	}
	else if (BlinkPhase == EBlinkPhase::FadeIn)
	{
		const double Duration = FMath::Max(Layout.BlinkFadeInS, 0.01);
		SetVignette((1.0 - FMath::Clamp(Elapsed / Duration, 0.0, 1.0)) * Layout.BlinkVignettePeak);
		if (Elapsed >= Duration)
		{
			SetVignette(0.0);
			BlinkPhase = EBlinkPhase::Idle;
			bBlinkBusy = false;
		}
	}
}

void AMageArenaPawn::BlinkToPad(int32 PadIndex)
{
	UE_LOG(LogMageArena, Log, TEXT("L3 debug BlinkToPad %d"), PadIndex);
	if (!bHasLayout || !Layout.FindPad(PadIndex) || bBlinkBusy)
	{
		return;
	}
	StartBlinkFade(PadIndex);
}

void AMageArenaPawn::PresentBlink(int32 PadIndex)
{
	UE_LOG(LogMageArena, Log, TEXT("Gesture blink to pad %d"), PadIndex);
	if (!bHasLayout || !Layout.FindPad(PadIndex))
	{
		return;
	}
	// Fade-out has not seated yet, so the pending pad is the one it will seat.
	// Fade-in has already seated. A new blink fades out again instead of snapping.
	if (bBlinkBusy && BlinkPhase == EBlinkPhase::FadeOut)
	{
		PendingPad = PadIndex;
		return;
	}
	StartBlinkFade(PadIndex);
}

void AMageArenaPawn::StartBlinkFade(int32 PadIndex)
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	APlayerCameraManager* Manager = Player ? Player->PlayerCameraManager : nullptr;
	if (!Manager || Layout.BlinkFadeOutS <= 0.0)
	{
		SeatOnPad(PadIndex);
		return;
	}
	PendingPad = PadIndex;
	bBlinkBusy = true;
	BlinkPhase = EBlinkPhase::FadeOut;
	BlinkPhaseStart = FPlatformTime::Seconds();
	StartFade(0.f, 1.f, Layout.BlinkFadeOutS, true);
}

void AMageArenaPawn::SeatOnPad(int32 PadIndex)
{
	const FRunePad* Pad = Layout.FindPad(PadIndex);
	if (!Pad)
	{
		return;
	}
	ActivePad = PadIndex;
	const FVector Forward = Layout.FlatForwardM(*Pad);
	const FVector Location(
		FArenaLayout::MetresToCentimetres(Pad->PositionM.X),
		FArenaLayout::MetresToCentimetres(Pad->PositionM.Y),
		FArenaLayout::MetresToCentimetres(Pad->PositionM.Z));
	SetActorLocation(Location);
	SetActorRotation(Forward.Rotation());
	if (USceneComponent* Root = GetRootComponent())
	{
		// A NewObject pawn is not registered, so MoveComponent can leave the world transform stale.
		Root->SetRelativeLocationAndRotation(Location, Forward.Rotation());
		Root->UpdateComponentToWorld();
	}
	if (Camera)
	{
		Camera->UpdateComponentToWorld();
	}
	FacingYaw = GetActorRotation().Yaw;
	ApplyLook();
}

void AMageArenaPawn::ApplyLook()
{
	const FRunePad* Pad = Layout.FindPad(ActivePad);
	if (!Pad)
	{
		return;
	}
	const FVector Eye = Layout.EyeLocationM(*Pad);
	const FRotator Look = ClampLookRotation((Layout.LookTargetM - Eye).Rotation());
	if (AController* OwnerController = GetController())
	{
		OwnerController->SetControlRotation(Look);
	}
}

float AMageArenaPawn::SeatedPitchFromMouse(float PitchDegrees, float MouseDY, float Sensitivity, float PitchDown, float PitchUp)
{
	const float Next = FRotator::NormalizeAxis(PitchDegrees + MouseDY * Sensitivity);
	return FMath::Clamp(Next, -PitchDown, PitchUp);
}

FRotator AMageArenaPawn::ClampLookRotation(const FRotator& Rotation) const
{
	FRotator Out = Rotation;
	float DeltaYaw = FRotator::NormalizeAxis(Out.Yaw - FacingYaw);
	DeltaYaw = FMath::Clamp(DeltaYaw, -GetYawLimitDegrees(), GetYawLimitDegrees());
	Out.Yaw = FacingYaw + DeltaYaw;
	Out.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Out.Pitch), -GetPitchDownDegrees(), GetPitchUpDegrees());
	Out.Roll = 0.f;
	return Out;
}

void AMageArenaPawn::LookAtArena()
{
	ApplyLook();
}

void AMageArenaPawn::SetLookPitch(float PitchDegrees)
{
	if (AController* OwnerController = GetController())
	{
		FRotator Rot = OwnerController->GetControlRotation();
		Rot.Pitch = PitchDegrees;
		OwnerController->SetControlRotation(ClampLookRotation(Rot));
	}
}

void AMageArenaPawn::SetVignette(double Intensity)
{
	if (!Camera)
	{
		return;
	}
	Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
	Camera->PostProcessSettings.VignetteIntensity = static_cast<float>(Intensity);
}

void AMageArenaPawn::StartFade(float FromAlpha, float ToAlpha, double Duration, bool bHold)
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (!Player || !Player->PlayerCameraManager)
	{
		return;
	}
	Player->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, static_cast<float>(Duration), FLinearColor::Black, false, bHold);
}

double AMageArenaPawn::GetCameraHeightAbovePadCm() const
{
	if (!Camera)
	{
		return 0.0;
	}
	const FRunePad* Pad = bHasLayout ? Layout.FindPad(ActivePad) : nullptr;
	const double PadTopCm = Pad ? FArenaLayout::MetresToCentimetres(Pad->PositionM.Z) : 0.0;
	return Camera->GetComponentLocation().Z - PadTopCm;
}
