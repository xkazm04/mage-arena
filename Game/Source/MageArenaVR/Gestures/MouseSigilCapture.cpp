#include "Gestures/MouseSigilCapture.h"

#include "GameFramework/PlayerController.h"
#include "Gestures/SigilRecognizerSubsystem.h"

void UMouseSigilCapture::SetSubsystem(USigilRecognizerSubsystem* InSigils)
{
	Sigils = InSigils;
}

bool UMouseSigilCapture::ProjectCursor(APlayerController* Player, FVector& OutTipCm) const
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!Player->GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	if (!Player->DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Direction))
	{
		return false;
	}
	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	Player->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector Normal = CameraRotation.Vector();
	const double Denominator = FVector::DotProduct(Direction, Normal);
	if (FMath::Abs(Denominator) < 1.0e-6)
	{
		return false;
	}
	const FVector PlanePoint = CameraLocation + Normal * PlaneDistanceCm;
	const double T = FVector::DotProduct(PlanePoint - Origin, Normal) / Denominator;
	if (T <= 0.0)
	{
		return false;
	}
	OutTipCm = Origin + Direction * T;
	return true;
}

void UMouseSigilCapture::Sample(APlayerController* Player, float DeltaSeconds)
{
	USigilRecognizerSubsystem* Subsystem = Sigils.Get();
	if (!Player || !Subsystem)
	{
		return;
	}
	TimeSeconds += FMath::Max(0.0f, DeltaSeconds);
	const bool bDown = Player->IsInputKeyDown(EKeys::LeftMouseButton);
	if (bDown)
	{
		FVector Tip = FVector::ZeroVector;
		if (ProjectCursor(Player, Tip))
		{
			LastTip = Tip;
			bHasPoint = true;
			Subsystem->IngestMouse(Tip, 1.0f, TimeSeconds);
		}
	}
	else if (bWasDown && bHasPoint)
	{
		Subsystem->IngestMouse(LastTip, 0.0f, TimeSeconds);
		bHasPoint = false;
	}
	bWasDown = bDown;
}
