#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Hands/ClipVariant.h"
#include "InputCoreTypes.h"
#include "MageArenaPlayerController.generated.h"

class UHandInputSubsystem;
class UInputAction;
class UInputMappingContext;
class UMouseSigilCapture;

/** Keys that hold the ward. Space aims with the view. The right button aims at the cursor. */
struct FWardHoldKeys
{
	bool bSpaceDown = false;
	bool bRightDown = false;
};

/** One tick of the hold chord. Releasing one button leaves the ward up while the other is down. */
struct FWardHoldStep
{
	bool bHeld = false;
	bool bPress = false;
	bool bRelease = false;
	bool bAim = false;
	bool bAimFromCursor = false;
};

/** Pure hold chord. Both buttons hold. A press only happens on the rising edge of the pair. */
FWardHoldStep StepWardHold(const FWardHoldKeys& Prev, const FWardHoldKeys& Now);

/**
 * Ground-plane aim. The cursor wins while the right button aims.
 * A vertical or zero cursor falls back to the view. A vertical view falls back to +X.
 */
FVector ResolveWardAim(bool bFromCursor, const FVector& CursorDirection, const FVector& ViewDirection);

/** True when the command line starts one of the scripted capture drivers. A capture owns the camera, so mouse look stands down. */
bool IsCaptureRun(const TCHAR* CommandLine);

/** Installs the desktop quick-action keys. Taps call PlayQuickAction. Space and the right mouse button hold the ward. */
UCLASS()
class MAGEARENAVR_API AMageArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMageArenaPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void ReceivedPlayer() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Builds the key mapping. Safe without a world. Hold keys are not consumed. */
	void EnsureMapping();
	bool IsQuickActionConsuming(const FKey& Key) const;

private:
	void InstallMapping();
	void EnsureMouse();
	EClipVariant VariantFromModifiers() const;
	void PressWard(bool bFromCursor);
	void AimWard(bool bFromCursor);
	FVector WardAimDirection(bool bFromCursor) const;
	UHandInputSubsystem* HandsOrNull() const;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> Actions;

	UPROPERTY()
	TObjectPtr<UMouseSigilCapture> MouseCapture;

	FWardHoldKeys WardKeys;
	/** Set by the Space action itself. A consumed key can hide from IsInputKeyDown. */
	bool bSpaceFromAction = false;
};
