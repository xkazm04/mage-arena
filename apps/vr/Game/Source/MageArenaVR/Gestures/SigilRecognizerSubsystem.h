#pragma once

#include "CoreMinimal.h"
#include "Gestures/QPointCloudRecognizer.h"
#include "Gestures/SigilStrokeBuilder.h"
#include "Hands/HandFrame.h"
#include "Hands/SystemGestureGate.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SigilRecognizerSubsystem.generated.h"

class UHandInputSubsystem;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSigilCast, FName /*Line*/, float /*Score*/, double /*LatencyMs*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSigilRejected, double /*Distance*/);

/** Subscribes to the active hand source, runs the stroke builder and $Q, and broadcasts one cast event. */
UCLASS()
class MAGEARENAVR_API USigilRecognizerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void SetDrawStyle(ESigilDrawStyle Style);
	ESigilDrawStyle GetDrawStyle() const { return DrawStyle; }

	/** Loads every template clip in the directory. Missing or unrecognizable files are logged and skipped. */
	void LoadTemplatesFromDirectory(const FString& Directory);
	void BindToHands(UHandInputSubsystem* Hands);
	void ResetStrokes();

	/** One mouse sample in centimetres. Pinch is 1 while the button is held and 0 on release. */
	void IngestMouse(const FVector& TipCm, float Pinch, double TimeSeconds);

	FOnSigilCast OnSigilCast;
	FOnSigilRejected OnSigilRejected;

	const FQPointCloudRecognizer& GetRecognizer() const { return Recognizer; }
	FQPointCloudRecognizer& GetRecognizer() { return Recognizer; }

private:
	void HandleHandFrame(const FHandFrame& Frame);
	void Dispatch(ESigilStrokeEvent Event, const TArray<FQPoint>& Points);

	FQPointCloudRecognizer Recognizer;
	FSigilStrokeBuilder HandBuilder;
	FSigilStrokeBuilder MouseBuilder;
	FSystemGestureGate Gate;
	FDelegateHandle HandHandle;
	TWeakObjectPtr<UHandInputSubsystem> BoundHands;
	ESigilDrawStyle DrawStyle = ESigilDrawStyle::StyleA;
	bool bHasHandTime = false;
	double LastHandTime = 0.0;
};
