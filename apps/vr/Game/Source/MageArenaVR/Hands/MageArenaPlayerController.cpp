#include "Hands/MageArenaPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Gestures/MouseSigilCapture.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/WardDetector.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "Session/ArenaSession.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "MageArenaVR.h"
#include "Misc/Parse.h"

namespace
{
struct FQuickActionKey
{
	FKey Key;
	FName Action;
	bool bPause = false;
	bool bHold = false;
};

// One table. The mapping context and PlayQuickAction calls are built from it.
const FQuickActionKey GQuickActionKeys[] = {
	{ EKeys::One, TEXT("sigil-line1"), false },
	{ EKeys::Two, TEXT("sigil-line2"), false },
	{ EKeys::Three, TEXT("sigil-line3"), false },
	{ EKeys::Four, TEXT("mudra"), false },
	{ EKeys::Q, TEXT("bolt"), false },
	{ EKeys::SpaceBar, TEXT("ward-raise"), false, true },
	{ EKeys::A, TEXT("blink-left"), false },
	{ EKeys::D, TEXT("blink-right"), false },
	{ EKeys::S, TEXT("blink-back"), false },
	{ EKeys::F, TEXT("staff-plant"), false },
	{ EKeys::R, TEXT("both-palms"), false },
	// The three stones beside the seat (comfort settings in cold, the stone pick in an intermission). Each key plays a
	// reach clip; the session reads the hand over the stone like a tracked hand.
	{ EKeys::Z, TEXT("stone-left"), false },
	{ EKeys::X, TEXT("stone-centre"), false },
	{ EKeys::C, TEXT("stone-right"), false },
	{ EKeys::Escape, NAME_None, true },
};
}

FWardHoldStep StepWardHold(const FWardHoldKeys& Prev, const FWardHoldKeys& Now)
{
	FWardHoldStep Step;
	const bool bWasHeld = Prev.bSpaceDown || Prev.bRightDown;
	const bool bHeld = Now.bSpaceDown || Now.bRightDown;
	Step.bHeld = bHeld;
	Step.bPress = bHeld && !bWasHeld;
	Step.bRelease = bWasHeld && !bHeld;
	Step.bAim = bHeld && !Step.bPress;
	Step.bAimFromCursor = Now.bRightDown;
	return Step;
}

FVector ResolveWardAim(bool bFromCursor, const FVector& CursorDirection, const FVector& ViewDirection)
{
	auto Flat = [](const FVector& Direction) -> FVector
	{
		const FVector Horizontal(Direction.X, Direction.Y, 0.0);
		if (Horizontal.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}
		return Horizontal.GetSafeNormal();
	};
	if (bFromCursor)
	{
		const FVector Cursor = Flat(CursorDirection);
		if (!Cursor.IsNearlyZero())
		{
			return Cursor;
		}
	}
	const FVector View = Flat(ViewDirection);
	if (View.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}
	return View;
}

bool IsCaptureRun(const TCHAR* CommandLine)
{
	static const TCHAR* const Flags[] = {
		TEXT("MageArenaGreyboxCapture"),
		TEXT("MageArenaWave1Capture"),
		TEXT("MageArenaTeachCapture"),
		TEXT("MageArenaSettingsCapture"),
		TEXT("MageArenaCreaturesCapture"),
		TEXT("MageArenaStylePickCapture"),
		TEXT("MageArenaPresetCapture"),
		TEXT("MageArenaDayCapture"),
		TEXT("MageArenaAirCapture"),
		TEXT("MageArenaCollarCapture"),
		TEXT("MageArenaColourAudioCapture"),
	};
	for (const TCHAR* Flag : Flags)
	{
		if (FParse::Param(CommandLine, Flag))
		{
			return true;
		}
	}
	return false;
}

AMageArenaPlayerController::AMageArenaPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AMageArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureMapping();
	InstallMapping();
	EnsureMouse();
}

void AMageArenaPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	EnsureMapping();
	InstallMapping();
	EnsureMouse();
}

void AMageArenaPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// F5/F6/F7 are L3 debug snaps. They are not quick actions and they do not play a blink clip.
	if (PlayerInput)
	{
		if (AMageArenaPawn* Mage = Cast<AMageArenaPawn>(GetPawn()))
		{
			if (WasInputKeyJustPressed(EKeys::F5))
			{
				Mage->BlinkToPad(0);
			}
			else if (WasInputKeyJustPressed(EKeys::F6))
			{
				Mage->BlinkToPad(1);
			}
			else if (WasInputKeyJustPressed(EKeys::F7))
			{
				Mage->BlinkToPad(2);
			}
			else if (WasInputKeyJustPressed(EKeys::F8))
			{
				if (UWorld* World = GetWorld())
				{
					if (UArenaSessionSubsystem* Session = World->GetSubsystem<UArenaSessionSubsystem>())
					{
						Session->GetSession().NotifyHeadsetRemoved();
						UE_LOG(LogMageArena, Log, TEXT("F8 headset removed"));
					}
				}
			}
			else if (WasInputKeyJustPressed(EKeys::F9))
			{
				if (UHandInputSubsystem* Hands = HandsOrNull())
				{
					Hands->SetTrackingDropped(!Hands->IsTrackingDropped());
					UE_LOG(LogMageArena, Log, TEXT("F9 tracking dropped=%d"), Hands->IsTrackingDropped() ? 1 : 0);
				}
			}

			const bool bDrawing = IsInputKeyDown(EKeys::LeftMouseButton) || IsInputKeyDown(EKeys::RightMouseButton);
			float DX = 0.f;
			float DY = 0.f;
			GetInputMouseDelta(DX, DY);
			// The capture sequence owns the camera. A hidden cursor delta must not drag the scripted shots.
			const bool bCapture = IsCaptureRun(FCommandLine::Get());
			if (!bDrawing && !Mage->IsBlinkBusy() && !bCapture)
			{
				FRotator Rot = GetControlRotation();
				const float Sensitivity = Mage->GetLookSensitivity();
				Rot.Yaw += DX * Sensitivity;
				Rot.Pitch = AMageArenaPawn::SeatedPitchFromMouse(
					Rot.Pitch, DY, Sensitivity, Mage->GetPitchDownDegrees(), Mage->GetPitchUpDegrees());
				SetControlRotation(Mage->ClampLookRotation(Rot));
			}
		}
	}
	if (MouseCapture)
	{
		MouseCapture->Sample(this, DeltaTime);
	}
	FWardHoldKeys Now;
	// The action latch covers a consumed Space key. The poll covers the same key once it is not consumed.
	Now.bSpaceDown = bSpaceFromAction || IsInputKeyDown(EKeys::SpaceBar);
	Now.bRightDown = IsInputKeyDown(EKeys::RightMouseButton);
	const FWardHoldStep Step = StepWardHold(WardKeys, Now);
	if (Step.bPress)
	{
		PressWard(Step.bAimFromCursor);
	}
	else if (Step.bRelease)
	{
		if (UHandInputSubsystem* Hands = HandsOrNull())
		{
			Hands->SetWardHeld(false, EClipVariant::Normal);
		}
	}
	else if (Step.bAim)
	{
		AimWard(Step.bAimFromCursor);
	}
	WardKeys = Now;
}

void AMageArenaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	EnsureMapping();
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Enhanced)
	{
		UE_LOG(LogMageArena, Error, TEXT("Player controller has no Enhanced Input component"));
		return;
	}
	for (int32 Index = 0; Index < Actions.Num(); ++Index)
	{
		const FQuickActionKey& Binding = GQuickActionKeys[Index];
		UInputAction* Action = Actions[Index];
		if (Binding.bPause)
		{
			Enhanced->BindActionValueLambda(Action, ETriggerEvent::Triggered, [this](const FInputActionValue&)
			{
				UGameInstance* Instance = GetGameInstance();
				UArenaSessionSubsystem* Session = nullptr;
				if (Instance)
				{
					if (UWorld* World = GetWorld())
					{
						Session = World->GetSubsystem<UArenaSessionSubsystem>();
					}
				}
				if (!Session)
				{
					UE_LOG(LogMageArena, Log, TEXT("Pause reserved (Esc); no clip played"));
					return;
				}
				Session->TogglePause();
			});
			continue;
		}
		if (Binding.bHold)
		{
			// The action sees Space even when a consumed key would hide it from IsInputKeyDown.
			// PlayerTick turns this latch into the hold chord. These lambdas do not press or release.
			Enhanced->BindActionValueLambda(Action, ETriggerEvent::Started, [this](const FInputActionValue&)
			{
				bSpaceFromAction = true;
			});
			Enhanced->BindActionValueLambda(Action, ETriggerEvent::Triggered, [this](const FInputActionValue&)
			{
				bSpaceFromAction = true;
			});
			Enhanced->BindActionValueLambda(Action, ETriggerEvent::Completed, [this](const FInputActionValue&)
			{
				bSpaceFromAction = false;
			});
			Enhanced->BindActionValueLambda(Action, ETriggerEvent::Canceled, [this](const FInputActionValue&)
			{
				bSpaceFromAction = false;
			});
			continue;
		}
		const FName ClipAction = Binding.Action;
		Enhanced->BindActionValueLambda(Action, ETriggerEvent::Triggered, [this, ClipAction](const FInputActionValue&)
		{
			UGameInstance* Instance = GetGameInstance();
			UHandInputSubsystem* Hands = Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
			if (!Hands)
			{
				UE_LOG(LogMageArena, Error, TEXT("PlayQuickAction %s: hand input subsystem missing"), *ClipAction.ToString());
				return;
			}
			FName Play = ClipAction;
			if (ClipAction == TEXT("staff-plant"))
			{
				if (UWorld* World = GetWorld())
				{
					if (UArenaSessionSubsystem* Session = World->GetSubsystem<UArenaSessionSubsystem>())
					{
						if (Session->GetSession().IsStaffPlanted())
						{
							Play = TEXT("staff-lift");
						}
					}
				}
			}
			Hands->PlayQuickAction(Play, VariantFromModifiers());
		});
	}
}

void AMageArenaPlayerController::EnsureMapping()
{
	if (MappingContext)
	{
		return;
	}
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("HandClipKeys"));
	for (const FQuickActionKey& Binding : GQuickActionKeys)
	{
		UInputAction* Action = NewObject<UInputAction>(this);
		Action->ValueType = EInputActionValueType::Boolean;
		// A consumed Space key never reaches IsInputKeyDown, so aim froze and releasing
		// the right button dropped a ward that Space was still holding.
		Action->bConsumeInput = !Binding.bHold;
		Action->bConsumesActionAndAxisMappings = false;
		Actions.Add(Action);
		FEnhancedActionKeyMapping& Mapped = MappingContext->MapKey(Action, Binding.Key);
		if (Binding.bHold)
		{
			UInputTriggerDown* Down = NewObject<UInputTriggerDown>(MappingContext);
			Mapped.Triggers.Add(Down);
		}
		else
		{
			UInputTriggerPressed* Pressed = NewObject<UInputTriggerPressed>(MappingContext);
			Mapped.Triggers.Add(Pressed);
		}
	}
}

void AMageArenaPlayerController::EnsureMouse()
{
	if (!MouseCapture)
	{
		MouseCapture = NewObject<UMouseSigilCapture>(this);
	}
	if (UGameInstance* Instance = GetGameInstance())
	{
		MouseCapture->SetSubsystem(Instance->GetSubsystem<USigilRecognizerSubsystem>());
	}
	// A free cursor is required to draw. Skip it when there is no viewport (headless automation).
	if (IsLocalPlayerController() && GetLocalPlayer() && GEngine && GEngine->GameViewport)
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
}

void AMageArenaPlayerController::InstallMapping()
{
	if (!MappingContext)
	{
		return;
	}
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(MappingContext, 0);
		}
	}
}

EClipVariant AMageArenaPlayerController::VariantFromModifiers() const
{
	// Shift wins when both modifiers are held. The same PlayQuickAction runs either way.
	const bool bSloppy = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	const bool bSlow = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bSloppy)
	{
		return EClipVariant::Sloppy;
	}
	if (bSlow)
	{
		return EClipVariant::Slow;
	}
	return EClipVariant::Normal;
}

void AMageArenaPlayerController::PressWard(bool bFromCursor)
{
	AimWard(bFromCursor);
	if (UHandInputSubsystem* Hands = HandsOrNull())
	{
		Hands->SetWardHeld(true, VariantFromModifiers());
	}
	else
	{
		UE_LOG(LogMageArena, Error, TEXT("Ward hold: hand input subsystem missing"));
	}
}

void AMageArenaPlayerController::AimWard(bool bFromCursor)
{
	const FVector Aim = WardAimDirection(bFromCursor);
	const double Yaw = FMath::Atan2(Aim.Y, Aim.X);
	if (UHandInputSubsystem* Hands = HandsOrNull())
	{
		Hands->SetAimYaw(Yaw);
	}
	if (UGameInstance* Instance = GetGameInstance())
	{
		if (UWardDetectorSubsystem* Wards = Instance->GetSubsystem<UWardDetectorSubsystem>())
		{
			Wards->SetAimFacing(Aim);
		}
	}
}

FVector AMageArenaPlayerController::WardAimDirection(bool bFromCursor) const
{
	FVector Cursor = FVector::ZeroVector;
	if (bFromCursor)
	{
		FVector Origin = FVector::ZeroVector;
		FVector Direction = FVector::ZeroVector;
		if (DeprojectMousePositionToWorld(Origin, Direction))
		{
			Cursor = Direction;
		}
	}
	return ResolveWardAim(bFromCursor, Cursor, GetControlRotation().Vector());
}

bool AMageArenaPlayerController::IsQuickActionConsuming(const FKey& Key) const
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(GQuickActionKeys); ++Index)
	{
		if (GQuickActionKeys[Index].Key == Key)
		{
			return Actions.IsValidIndex(Index) && Actions[Index] && Actions[Index]->bConsumeInput;
		}
	}
	return true;
}

UHandInputSubsystem* AMageArenaPlayerController::HandsOrNull() const
{
	UGameInstance* Instance = GetGameInstance();
	return Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
}
