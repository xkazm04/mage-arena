#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Hands/ClipVariant.h"
#include "MageArenaPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/** Installs the desktop quick-action keys. Each key calls UHandInputSubsystem::PlayQuickAction. */
UCLASS()
class MAGEARENAVR_API AMageArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMageArenaPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void ReceivedPlayer() override;
	virtual void SetupInputComponent() override;

private:
	void EnsureMapping();
	void InstallMapping();
	EClipVariant VariantFromModifiers() const;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> Actions;
};
