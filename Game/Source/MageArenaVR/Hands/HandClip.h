#pragma once

#include "CoreMinimal.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandFrame.h"

/** Samples of one hand, in increasing time. */
struct FHandClipTrack
{
	EControllerHand Hand = EControllerHand::Left;
	TArray<FHandFrame> Frames;
};

/** A hand clip loaded from Game/Clips. Positions are already in centimetres. */
struct FHandClip
{
	FString Name;
	FString Action;
	FString Variant;
	FString Source;
	FString Space;
	int64 Seed = 0;
	double Hz = 72.0;
	double Duration = 0.0;
	TArray<EControllerHand> Hands;
	TArray<FHandClipTrack> Tracks;

	static FString MakeFilePath(FName Action, EClipVariant Variant);
	static const TCHAR* VariantToString(EClipVariant Variant);
	static const TCHAR* KeypointName(int32 Index);
	static bool LoadFromFile(const FString& Path, FHandClip& OutClip, FString& OutError);

	/** Pose at TimeSeconds. Clamps to the ends. Lerp on position, pinch and confidence; slerp on rotation. */
	bool Sample(EControllerHand Hand, double TimeSeconds, FHandFrame& Out) const;

	int32 GetSampleCount() const;
	const TArray<EControllerHand>& GetHands() const { return Hands; }
	const TArray<FHandClipTrack>& GetTracks() const { return Tracks; }
	double GetDuration() const { return Duration; }
};
