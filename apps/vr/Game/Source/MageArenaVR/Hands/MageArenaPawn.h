#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Greybox/ArenaLayout.h"
#include "MageArenaPawn.generated.h"

class UCameraComponent;
class UHandPresentationComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class USceneComponent;

/**
 * Stationary seated camera. No locomotion, so A/S/D stay free for blink clips.
 * F5/F6/F7 call BlinkToPad. That is an L3 debug snap, not a gesture.
 */
UCLASS()
class MAGEARENAVR_API AMageArenaPawn : public APawn
{
	GENERATED_BODY()

public:
	AMageArenaPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;

	/** L3 debug. 0 left, 1 centre, 2 right. Snaps during a fade. Ignores a second call while busy. */
	void BlinkToPad(int32 PadIndex);

	/**
	 * Gesture blink. Same fade as the debug snap. Logs separately from BlinkToPad.
	 * A second call during fade-out retargets the pad that fade will seat.
	 * A call during fade-in starts a new fade. It does not snap the pawn into place.
	 */
	void PresentBlink(int32 PadIndex);

	void ConfigureFromLayout(const FArenaLayout& InLayout);
	void LookAtArena();
	/** Pitch in degrees, positive up, clamped to the layout. Yaw stays inside the facing limit. */
	void SetLookPitch(float PitchDegrees);

	/**
	 * Positive MouseDY looks up. Windows raw Y is positive when the mouse moves down, and
	 * GetInputMouseDelta flips that, so DY is already positive when the mouse moves up.
	 */
	static float SeatedPitchFromMouse(float PitchDegrees, float MouseDY, float Sensitivity, float PitchDown, float PitchUp);

	/** Yaw stays within +/- the layout limit of the pad facing. Pitch is clamped. Roll is cleared. */
	FRotator ClampLookRotation(const FRotator& Rotation) const;

	/** Manual exposure at 1, bloom off, tonemapper off, so unlit linear colours survive into the screenshot. */
	void ApplyFlatPresentation();

	bool HasLayout() const { return bHasLayout; }
	const FArenaLayout& GetLayout() const { return Layout; }
	USceneComponent* GetSeatedOrigin() const { return SeatedOrigin; }
	int32 GetActivePadIndex() const { return ActivePad; }
	bool IsBlinkBusy() const { return bBlinkBusy; }
	/** 0 outside a fade-out, else how far the fade-out has run, 0 to 1. For the capture. */
	double GetBlinkFadeOutFraction() const;
	/** What the blink vignette shows for an intensity. Opacity is 0 to 1. The inner half-angle is of the clear opening. */
	struct FVignetteAperture
	{
		double Opacity = 0.0;
		double InnerHalfAngleDeg = 0.0;
	};

	/** Pure. Zero is open and clear. Intensity at the peak is fully closed. Anything above the peak clamps. */
	static FVignetteAperture VignetteAperture(double Intensity, double Peak);

	/**
	 * Drives the four camera-attached vignette quads. Zero hides them. Public so a test can drive it
	 * without a blink.
	 */
	void SetVignette(double Intensity);
	bool IsVignetteVisible() const;
	/** The opacity the vignette material holds now, or -1 when it has no material yet. */
	double GetVignetteOpacity() const;
	/** The intensity last given to SetVignette. */
	double GetVignetteIntensity() const { return VignetteValue; }

	double GetCameraHeightAbovePadCm() const;
	float GetFacingYawDegrees() const { return FacingYaw; }
	float GetYawLimitDegrees() const { return static_cast<float>(Layout.YawLimitDeg); }
	float GetPitchDownDegrees() const { return static_cast<float>(Layout.PitchDownDeg); }
	float GetPitchUpDegrees() const { return static_cast<float>(Layout.PitchUpDeg); }
	float GetLookSensitivity() const { return static_cast<float>(Layout.LookSensitivity); }

private:
	enum class EBlinkPhase : uint8
	{
		Idle,
		FadeOut,
		FadeIn
	};

	void StartBlinkFade(int32 PadIndex);
	void SeatOnPad(int32 PadIndex);
	void ApplyLook();
	void StartFade(float FromAlpha, float ToAlpha, double Duration, bool bHold);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<USceneComponent> SeatedOrigin;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UHandPresentationComponent> Hands;

	void EnsureVignetteAssets();
	void UpdateVignetteQuads(const FVignetteAperture& Aperture);

	static constexpr int32 VignetteQuadCount = 4;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> VignetteQuads;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> VignetteMaterial;

	FArenaLayout Layout;
	bool bHasLayout = false;
	int32 ActivePad = 1;
	int32 PendingPad = 1;
	float FacingYaw = 0.f;
	bool bBlinkBusy = false;
	EBlinkPhase BlinkPhase = EBlinkPhase::Idle;
	double BlinkPhaseStart = 0.0;
	double VignetteValue = 0.0;
};
