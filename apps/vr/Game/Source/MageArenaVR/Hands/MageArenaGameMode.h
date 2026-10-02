#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MageArenaGameMode.generated.h"

/** Greybox game mode. Play-in-editor uses AMageArenaPlayerController so the clip keys are live. */
UCLASS()
class MAGEARENAVR_API AMageArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMageArenaGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
