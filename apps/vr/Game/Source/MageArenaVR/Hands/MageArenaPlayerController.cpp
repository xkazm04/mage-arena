#include "Hands/MageArenaPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Gestures/MouseSigilCapture.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Hands/HandInputSubsystem.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "MageArenaVR.h"

namespace
{
struct FQuickActionKey
{
	FKey Key;
	FName Action;
	bool bPause = false;
};

// One table. The mapping context and PlayQuickAction calls are built from it.
const FQuickActionKey GQuickActionKeys[] = {
	{ EKeys::One, TEXT("sigil-line1"), false },
	{ EKeys::Two, TEXT("sigil-line2"), false },
	{ EKeys::Three, TEXT("sigil-line3"), false },
	{ EKeys::Four, TEXT("mudra"), false },
	{ EKeys::Q, TEXT("bolt"), false },
	{ EKeys::SpaceBar, TEXT("ward-raise"), false },
	{ EKeys::A, TEXT("blink-left"), false },
	{ EKeys::D, TEXT("blink-right"), false },
	{ EKeys::S, TEXT("blink-back"), false },
	{ EKeys::Escape, NAME_None, true },
};
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
	if (MouseCapture)
	{
		MouseCapture->Sample(this, DeltaTime);
	}
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
				UE_LOG(LogMageArena, Log, TEXT("Pause reserved (Esc); no clip played"));
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
			Hands->PlayQuickAction(ClipAction, VariantFromModifiers());
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
		Action->bConsumeInput = true;
		Actions.Add(Action);
		FEnhancedActionKeyMapping& Mapped = MappingContext->MapKey(Action, Binding.Key);
		UInputTriggerPressed* Pressed = NewObject<UInputTriggerPressed>(MappingContext);
		Mapped.Triggers.Add(Pressed);
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
