#pragma once

#include "CoreMinimal.h"
#include "Hands/HandClipPlayer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HandInputSubsystem.generated.h"

/**
 * Owns two clip players. The ward lane holds the ward hand. The action lane plays the latest
 * quick action. A casting-hand clip (sigil, bolt, blink) leaves the ward running. A clip that
 * uses the ward hand (mudra, staff, and a left-hand ward) stops the ward. Detectors bind to
 * OnHandFrame, which merges both players, so each hand is read on its own.
 */
UCLASS()
class MAGEARENAVR_API UHandInputSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Loads Game/Clips/<action>.<variant>.jsonl and plays it. This is the key path. */
	void PlayQuickAction(FName Action, EClipVariant Variant);

	/**
	 * Hold ward-raise while bHeld is true, then reverse it on release.
	 * Space and the right mouse button both call this. A second press while held does nothing.
	 * A press during the lower reverses that lower. A press after the lower has finished loads Variant.
	 */
	void SetWardHeld(bool bHeld, EClipVariant Variant);
	void SetAimYaw(double YawRadians);
	bool IsWardHeld() const { return bWardHeld; }

	void Step(double DeltaSeconds);
	/**
	 * Desktop F9 and tests. Playing clips keep their pose and emit confidence 0.
	 * With no clip playing, each Step emits an untracked frame so the session can
	 * see the loss on the hand pipeline.
	 */
	void SetTrackingDropped(bool bDropped);
	bool IsTrackingDropped() const { return bTrackingDropped; }
	/** Stops both lanes. A fresh player that is not playing stays idle. */
	void StopAll();
	bool GetLatest(EControllerHand Hand, FHandFrame& Out) const;
	uint64 GetFrameSequence() const;
	FOnHandSourceFrame& OnHandFrame();

	/** The player a test just started: the ward after SetWardHeld, the action after PlayQuickAction. */
	UHandClipPlayer* GetClipPlayer() const { return LastStarted ? LastStarted : ActionPlayer; }
	IHandSource* GetActiveSource() const { return GetClipPlayer(); }

	bool IsActionPlaying() const;
	FString GetActionName() const;

private:
	void EnsurePlayers();
	void ForwardFrame(const FHandFrame& Frame);
	bool ActionOwns(EControllerHand Hand) const;

	UPROPERTY()
	TObjectPtr<UHandClipPlayer> WardPlayer;

	UPROPERTY()
	TObjectPtr<UHandClipPlayer> ActionPlayer;

	UPROPERTY()
	TObjectPtr<UHandClipPlayer> LastStarted;

	FOnHandSourceFrame FrameDelegate;
	bool bWardHeld = false;
	bool bTrackingDropped = false;
	EClipVariant HeldVariant = EClipVariant::Normal;
};
