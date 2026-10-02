#pragma once

#include "CoreMinimal.h"
#include "MouseSigilCapture.generated.h"

class APlayerController;
class USigilRecognizerSubsystem;

/** While the left button is held, projects the cursor onto a plane 40 cm in front of the camera. */
UCLASS()
class MAGEARENAVR_API UMouseSigilCapture : public UObject
{
	GENERATED_BODY()

public:
	void SetSubsystem(USigilRecognizerSubsystem* InSigils);
	void Sample(APlayerController* Player, float DeltaSeconds);

private:
	static constexpr double PlaneDistanceCm = 40.0;

	bool ProjectCursor(APlayerController* Player, FVector& OutTipCm) const;

	TWeakObjectPtr<USigilRecognizerSubsystem> Sigils;
	bool bWasDown = false;
	bool bHasPoint = false;
	FVector LastTip = FVector::ZeroVector;
	double TimeSeconds = 0.0;
};
