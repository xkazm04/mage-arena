#pragma once

#include "CoreMinimal.h"

/** One rune pad. PositionM is the top centre, in metres. */
struct FRunePad
{
	FString Id;
	int32 Index = 0;
	FVector PositionM = FVector::ZeroVector;
};

/** Ground point a threat can launch from. Distance and bearing are from the centre pad. */
struct FSpawnMarker
{
	FString Id;
	double DistanceM = 0.0;
	double BearingDeg = 0.0;
	FVector PositionM = FVector::ZeroVector;
};

/** One prop placed on the sand. PositionM is X, Y on z = 0. */
struct FArenaProp
{
	FVector PositionM = FVector::ZeroVector;
	double StandHeightM = 0.0;
	double BowlRadiusM = 0.0;
	double BowlHeightM = 0.0;
	double FlameRadiusM = 0.0;
	double PoleHeightM = 0.0;
	double PoleRadiusM = 0.0;
	FVector ClothSizeM = FVector::ZeroVector;
};

/**
 * One demo threat. ColourClass is the threat language:
 * turquoise, ember-orange (T26; was vermilion), steel, or black-core-red-rim.
 */
struct FThreatSpec
{
	FString Kind;
	FString ColourClass;
	FLinearColor Body = FLinearColor::White;
	FLinearColor Rim = FLinearColor::White;
	FLinearColor Telegraph = FLinearColor::White;
	bool bElongated = false;
	bool bRimmed = false;
	FString MarkerId;
	double FlightS = 1.0;
	double LandingForwardM = 3.0;
	double LandingRightM = 0.0;
	double RadiusM = 0.3;
	double LengthM = 0.3;
};

/** VR-owned greybox layout. Metres, Unreal axes. Loaded from data/vr/arena-layout.json. */
struct FArenaLayout
{
	double FloorRadiusXM = 16.0;
	double FloorRadiusYM = 10.0;
	double FloorThicknessM = 0.2;

	double WallHeightM = 3.0;
	double WallThicknessM = 0.45;
	int32 WallSegments = 32;
	int32 MerlonEvery = 2;
	FVector MerlonSizeM = FVector(0.4, 0.4, 0.35);

	int32 StandTiers = 3;
	double StandDepthM = 1.55;
	double StandGapM = 0.2;
	double StandFirstTopAboveWallM = 0.5;
	double StandStepRiseM = 0.85;

	FVector DaisCentreM = FVector(-11.7, 0.0, 0.3);
	FVector DaisSizeM = FVector(5.2, 8.8, 0.6);
	double DaisHeightM = 0.6;

	double PadArcRadiusM = 12.0;
	double PadSpacingAuthoredM = 3.0;
	double PadDiscRadiusM = 0.58;
	double PadRingRadiusM = 0.78;
	double PadRingWidthM = 0.16;
	double PadRingHeightM = 0.07;
	int32 PadRingSegments = 16;
	TArray<FRunePad> Pads;

	FVector SeatSizeM = FVector(0.5, 0.46, 0.42);

	double SeatedEyeHeightAbovePadM = 1.2;
	double EyeForwardOfSeatedOriginM = 0.08;
	double EyeAboveSeatedOriginM = 0.62;
	double CameraFovDeg = 90.0;
	FVector LookTargetM = FVector(0.0, 0.0, 0.9);
	FVector ArenaCentreM = FVector::ZeroVector;

	double YawLimitDeg = 100.0;
	double PitchDownDeg = 32.0;
	double PitchUpDeg = 18.0;
	double LookSensitivity = 0.08;

	double BlinkFadeOutS = 0.12;
	double BlinkFadeInS = 0.2;
	double BlinkVignettePeak = 1.15;

	TArray<FSpawnMarker> SpawnMarkers;
	double MarkerPostRadiusM = 0.1;
	double MarkerPostHeightM = 0.45;

	TArray<FArenaProp> Braziers;
	TArray<FArenaProp> Banners;

	FLinearColor Sand = FLinearColor(0.58f, 0.47f, 0.31f);
	FLinearColor Stone = FLinearColor(0.46f, 0.45f, 0.43f);
	FLinearColor Dais = FLinearColor(0.22f, 0.22f, 0.24f);
	FLinearColor PadRing = FLinearColor(0.64f, 0.56f, 0.40f);
	FLinearColor Seat = FLinearColor(0.40f, 0.39f, 0.37f);
	FLinearColor Marker = FLinearColor(0.15f, 0.15f, 0.16f);
	FLinearColor Brazier = FLinearColor(0.18f, 0.16f, 0.15f);
	FLinearColor Flame = FLinearColor(0.95f, 0.35f, 0.06f);
	FLinearColor BannerCloth = FLinearColor(0.64f, 0.59f, 0.50f);
	FLinearColor BannerPole = FLinearColor(0.30f, 0.24f, 0.16f);
	FLinearColor Hand = FLinearColor(0.75f, 0.64f, 0.54f);
	FLinearColor CastFlash = FLinearColor(0.12f, 0.82f, 0.20f);
	FLinearColor RejectFlash = FLinearColor(0.50f, 0.50f, 0.50f);
	FLinearColor WardArc = FLinearColor(0.45f, 0.75f, 0.95f, 0.40f);

	double ChestHeightAboveSandM = 1.35;
	double ThreatStaggerS = 0.2;
	double TelegraphStartRadiusM = 1.7;
	double TelegraphEndRadiusM = 0.28;
	int32 TelegraphSegments = 18;
	double TelegraphHeightM = 0.05;
	double TelegraphWidthM = 0.14;
	double ThreatRimScale = 1.18;
	/** Radial thickness of the unblockable rim tube, as a fraction of the body radius. */
	double ThreatRimTubeScale = 0.22;
	TArray<FThreatSpec> Threats;

	double JointRadiusM = 0.012;
	double BoneRadiusM = 0.0045;
	double FlashRadiusM = 0.085;
	int32 FlashSegments = 16;
	double FlashDurationS = 0.65;
	double WardArcDeg = 140.0;
	double WardArcInnerM = 0.14;
	double WardArcOuterM = 0.58;
	double WardArcHeightM = 0.2;
	int32 WardArcSegments = 14;

	double DirectionalIntensityLux = 8.0;
	double DirectionalPitchDeg = -12.0;
	double DirectionalYawDeg = 180.0;
	FLinearColor DirectionalColour = FLinearColor(1.f, 0.48f, 0.16f);
	double SkyIntensity = 1.4;
	FLinearColor SkyLightColour = FLinearColor(0.40f, 0.50f, 0.75f);
	FLinearColor SkyHorizon = FLinearColor(0.55f, 0.20f, 0.05f);
	FLinearColor SkyUpper = FLinearColor(0.05f, 0.07f, 0.16f);
	FLinearColor SkyZenith = FLinearColor(0.015f, 0.018f, 0.06f);
	bool bCastShadows = false;

	bool LoadFromFile(const FString& Path, FString& OutError);
	static FString DefaultFilePath();

	const FRunePad* FindPad(int32 Index) const;
	const FRunePad* FindPadById(const FString& Id) const;
	const FSpawnMarker* FindMarker(const FString& Id) const;

	/** Horizontal direction from the pad toward the arena centre. */
	FVector FlatForwardM(const FRunePad& Pad) const;
	FVector EyeLocationM(const FRunePad& Pad) const;
	double HorizontalPadSpacingM(int32 IndexA, int32 IndexB) const;

	/** Dais top when XY is inside the dais footprint, otherwise the sand at z = 0. */
	double SurfaceHeightM(double XM, double YM) const;

	static double MetresToCentimetres(double Metres) { return Metres * 100.0; }
};
