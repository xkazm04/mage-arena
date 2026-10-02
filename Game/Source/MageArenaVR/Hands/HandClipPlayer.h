#pragma once

#include "CoreMinimal.h"
#include "Hands/HandClip.h"
#include "Hands/IHandSource.h"
#include "Tickable.h"
#include "HandClipPlayer.generated.h"

/**
 * Plays a loaded clip as an IHandSource. Game time samples by t.
 * Tests call Step with a fixed dt; that path does not use the world tick.
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
	void Emit();

	FHandClip Clip;
	double TimeSeconds = 0.0;
	double Duration = 0.0;
	uint64 Sequence = 0;
	bool bHasClip = false;
	bool bPlaying = false;
	bool bHasLeft = false;
	bool bHasRight = false;
	FHandFrame LatestLeft;
	FHandFrame LatestRight;
	FOnHandSourceFrame FrameDelegate;
};
