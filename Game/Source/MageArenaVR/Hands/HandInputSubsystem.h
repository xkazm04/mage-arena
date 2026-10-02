#pragma once

#include "CoreMinimal.h"
#include "Hands/HandClipPlayer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HandInputSubsystem.generated.h"

/** Owns the active hand source and plays the clip a key (or a test) asks for. */
UCLASS()
class MAGEARENAVR_API UHandInputSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Loads Game/Clips/<action>.<variant>.jsonl and plays it. This is the key path. */
	void PlayQuickAction(FName Action, EClipVariant Variant);

	void Step(double DeltaSeconds);
	bool GetLatest(EControllerHand Hand, FHandFrame& Out) const;
	uint64 GetFrameSequence() const;
	FOnHandSourceFrame& OnHandFrame();

	UHandClipPlayer* GetClipPlayer() const { return ClipPlayer; }
	IHandSource* GetActiveSource() const { return ClipPlayer; }

private:
	void EnsurePlayer();

	UPROPERTY()
	TObjectPtr<UHandClipPlayer> ClipPlayer;
};
