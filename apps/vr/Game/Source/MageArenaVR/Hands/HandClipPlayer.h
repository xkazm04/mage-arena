#pragma once

#include "CoreMinimal.h"
#include "Hands/HandClip.h"
#include "Hands/IHandSource.h"
#include "Tickable.h"
#include "HandClipPlayer.generated.h"

/**
 * Plays a loaded clip as an IHandSource. Game time samples by t.
 * Tests call Step with a fixed dt; that path does not use the world tick.
 * After the clip ends, Step emits nothing. A hold is the exception: the pose
 * stays latched and each Step stamps a new time until EndHold reverses it.
 */
UCLASS()
class MAGEARENAVR_API UHandClipPlayer : public UObject, public FTickableGameObject, public IHandSource
{
	GENERATED_BODY()

public:
	UHandClipPlayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void Play(const FHandClip& InClip);
	void Stop();
	void Step(double DeltaSeconds);

	/** Latch the pose at HoldPoseTime and keep streaming it while the caller holds. */
	void BeginHold(double InHoldPoseTime);
	/** Sample the clip backward from the current pose down to the start. */
	void EndHold();
	/** True only while the hold clip is still lowering. A finished lower reloads on the next press. */
	bool CanResumeHold() const;
	/** Continue that hold without restarting the emit clock. */
	void ResumeHold();
	/** Yaw the emitted hand around seated +Z. Zero keeps the clip's authored facing. */
	void SetAimYaw(double YawRadians);
	/**
	 * Clip-level tracking loss. A playing clip keeps its pose and reports confidence 0.
	 * While nothing is playing, Step emits an untracked frame (confidence 0, no joints)
	 * so a drop is visible on the hand pipeline without a separate session flag.
	 */
	void SetTrackingDropped(bool bDropped);

	/** First sample of the settled palm pose. Ward-raise's hold lives there. */
	static double FindPlateauStart(const FHandClip& Clip, EControllerHand Hand);

	bool IsPlaying() const { return bPlaying; }
	double GetTime() const { return TimeSeconds; }
	double GetDuration() const { return Duration; }
	const FHandClip& GetClip() const { return Clip; }

	virtual bool GetLatest(EControllerHand Hand, FHandFrame& Out) const override;
	virtual uint64 GetFrameSequence() const override { return Sequence; }
	virtual FOnHandSourceFrame& OnHandFrame() override { return FrameDelegate; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

	virtual void BeginDestroy() override;

private:
	void EmitSample(double SampleAt, double StampTime);
	void EmitUntracked(double StampTime);
	void ApplyAimYaw(FHandFrame& Frame) const;
	void RefreshTick();

	FHandClip Clip;
	double TimeSeconds = 0.0;
	double SampleTime = 0.0;
	double Duration = 0.0;
	double HoldPoseTime = 0.0;
	double AimYawRadians = 0.0;
	uint64 Sequence = 0;
	bool bHasClip = false;
	bool bPlaying = false;
	bool bHoldArmed = false;
	bool bHoldEmitting = false;
	bool bLowering = false;
	bool bHoldClip = false;
	bool bHasLeft = false;
	bool bHasRight = false;
	bool bTrackingDropped = false;
	FHandFrame LatestLeft;
	FHandFrame LatestRight;
	FOnHandSourceFrame FrameDelegate;
};
