#include "Hands/MageArenaGameMode.h"

#include "Hands/MageArenaPawn.h"
#include "Hands/MageArenaPlayerController.h"

AMageArenaGameMode::AMageArenaGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerControllerClass = AMageArenaPlayerController::StaticClass();
	DefaultPawnClass = AMageArenaPawn::StaticClass();
}
