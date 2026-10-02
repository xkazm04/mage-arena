#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MageArenaPawn.generated.h"

class UCameraComponent;
class USceneComponent;

/** Stationary seated camera. No movement, so A/S/D stay free for blink clips. */
UCLASS()
class MAGEARENAVR_API AMageArenaPawn : public APawn
{
	GENERATED_BODY()

public:
	AMageArenaPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
};
