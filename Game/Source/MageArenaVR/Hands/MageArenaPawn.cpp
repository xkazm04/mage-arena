#include "Hands/MageArenaPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

AMageArenaPawn::AMageArenaPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("SeatedCamera"));
	Camera->SetupAttachment(SceneRoot);
	// Seated origin is the hip point. Eyes sit about 62 cm up and 8 cm forward, looking along +X.
	Camera->SetRelativeLocation(FVector(8.0, 0.0, 62.0));
}
