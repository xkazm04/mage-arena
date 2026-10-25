#include "Greybox/ArenaLayout.h"

#include "Dom/JsonObject.h"
#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool Fail(FString& OutError, const FString& Message)
{
	OutError = Message;
	return false;
}

bool NeedObject(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Field, const TSharedPtr<FJsonObject>*& Out, FString& OutError)
{
	if (!Parent.IsValid() || !Parent->TryGetObjectField(Field, Out) || !Out || !Out->IsValid())
	{
		return Fail(OutError, FString::Printf(TEXT("missing object %s"), Field));
	}
	return true;
}

bool NeedNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, double& Out, FString& OutError)
{
	if (!Obj.IsValid() || !Obj->TryGetNumberField(Field, Out) || !FMath::IsFinite(Out))
	{
		return Fail(OutError, FString::Printf(TEXT("missing number %s"), Field));
	}
	return true;
}

bool NeedInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32& Out, FString& OutError)
{
	double Number = 0.0;
	if (!NeedNumber(Obj, Field, Number, OutError))
	{
		return false;
	}
	Out = static_cast<int32>(FMath::RoundToInt(Number));
	return true;
}

bool NeedBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool& Out, FString& OutError)
{
	if (!Obj.IsValid() || !Obj->TryGetBoolField(Field, Out))
	{
		return Fail(OutError, FString::Printf(TEXT("missing bool %s"), Field));
	}
	return true;
}

bool NeedString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, FString& Out, FString& OutError)
{
	if (!Obj.IsValid() || !Obj->TryGetStringField(Field, Out) || Out.IsEmpty())
	{
		return Fail(OutError, FString::Printf(TEXT("missing string %s"), Field));
	}
	return true;
}

bool ReadVector(const TArray<TSharedPtr<FJsonValue>>& Values, int32 Count, FVector& Out, const TCHAR* Field, FString& OutError)
{
	if (Values.Num() < Count)
	{
		return Fail(OutError, FString::Printf(TEXT("%s needs %d numbers"), Field, Count));
	}
	double Components[3] = { 0.0, 0.0, 0.0 };
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Values[Index].IsValid() || Values[Index]->Type != EJson::Number)
		{
			return Fail(OutError, FString::Printf(TEXT("%s[%d] is not a number"), Field, Index));
		}
		Components[Index] = Values[Index]->AsNumber();
		if (!FMath::IsFinite(Components[Index]))
		{
			return Fail(OutError, FString::Printf(TEXT("%s[%d] is not finite"), Field, Index));
		}
	}
	Out = FVector(Components[0], Components[1], Count > 2 ? Components[2] : 0.0);
	return true;
}

bool NeedVector(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 Count, FVector& Out, FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Obj.IsValid() || !Obj->TryGetArrayField(Field, Values) || !Values)
	{
		return Fail(OutError, FString::Printf(TEXT("missing vector %s"), Field));
	}
	return ReadVector(*Values, Count, Out, Field, OutError);
}

bool NeedColour(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, FLinearColor& Out, FString& OutError)
{
	FVector Vector = FVector::ZeroVector;
	if (!NeedVector(Obj, Field, 3, Vector, OutError))
	{
		return false;
	}
	Out = FLinearColor(static_cast<float>(Vector.X), static_cast<float>(Vector.Y), static_cast<float>(Vector.Z), Out.A);
	return true;
}

bool LoadPads(const TSharedPtr<FJsonObject>& Root, FArenaLayout& Layout, FString& OutError)
{
	const TSharedPtr<FJsonObject>* Pads = nullptr;
	if (!NeedObject(Root, TEXT("pads"), Pads, OutError))
	{
		return false;
	}
	if (!NeedNumber(*Pads, TEXT("arcRadiusM"), Layout.PadArcRadiusM, OutError)
		|| !NeedNumber(*Pads, TEXT("spacingM"), Layout.PadSpacingAuthoredM, OutError)
		|| !NeedNumber(*Pads, TEXT("discRadiusM"), Layout.PadDiscRadiusM, OutError)
		|| !NeedNumber(*Pads, TEXT("ringRadiusM"), Layout.PadRingRadiusM, OutError)
		|| !NeedNumber(*Pads, TEXT("ringWidthM"), Layout.PadRingWidthM, OutError)
		|| !NeedNumber(*Pads, TEXT("ringHeightM"), Layout.PadRingHeightM, OutError)
		|| !NeedInt(*Pads, TEXT("ringSegments"), Layout.PadRingSegments, OutError))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!(*Pads)->TryGetArrayField(TEXT("items"), Items) || !Items)
	{
		return Fail(OutError, TEXT("pads.items missing"));
	}
	for (const TSharedPtr<FJsonValue>& Value : *Items)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		FRunePad Pad;
		if (!NeedString(Item, TEXT("id"), Pad.Id, OutError)
			|| !NeedInt(Item, TEXT("index"), Pad.Index, OutError)
			|| !NeedVector(Item, TEXT("positionM"), 3, Pad.PositionM, OutError))
		{
			return false;
		}
		Layout.Pads.Add(Pad);
	}
	return true;
}

bool LoadMarkers(const TSharedPtr<FJsonObject>& Root, FArenaLayout& Layout, FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Root->TryGetArrayField(TEXT("spawnMarkers"), Items) || !Items)
	{
		return Fail(OutError, TEXT("spawnMarkers missing"));
	}
	for (const TSharedPtr<FJsonValue>& Value : *Items)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		FSpawnMarker Marker;
		if (!NeedString(Item, TEXT("id"), Marker.Id, OutError)
			|| !NeedNumber(Item, TEXT("distanceM"), Marker.DistanceM, OutError)
			|| !NeedNumber(Item, TEXT("bearingDeg"), Marker.BearingDeg, OutError)
			|| !NeedVector(Item, TEXT("positionM"), 3, Marker.PositionM, OutError))
		{
			return false;
		}
		Layout.SpawnMarkers.Add(Marker);
	}
	const TSharedPtr<FJsonObject>* Post = nullptr;
	if (!NeedObject(Root, TEXT("markerPost"), Post, OutError))
	{
		return false;
	}
	return NeedNumber(*Post, TEXT("radiusM"), Layout.MarkerPostRadiusM, OutError)
		&& NeedNumber(*Post, TEXT("heightM"), Layout.MarkerPostHeightM, OutError);
}

bool LoadProps(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field, bool bBrazier, TArray<FArenaProp>& Out, FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Root->TryGetArrayField(Field, Items) || !Items)
	{
		return Fail(OutError, FString::Printf(TEXT("%s missing"), Field));
	}
	for (const TSharedPtr<FJsonValue>& Value : *Items)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		FArenaProp Prop;
		FVector Position = FVector::ZeroVector;
		if (!NeedVector(Item, TEXT("positionM"), 2, Position, OutError))
		{
			return false;
		}
		Prop.PositionM = Position;
		if (bBrazier)
		{
			if (!NeedNumber(Item, TEXT("standHeightM"), Prop.StandHeightM, OutError)
				|| !NeedNumber(Item, TEXT("bowlRadiusM"), Prop.BowlRadiusM, OutError)
				|| !NeedNumber(Item, TEXT("bowlHeightM"), Prop.BowlHeightM, OutError)
				|| !NeedNumber(Item, TEXT("flameRadiusM"), Prop.FlameRadiusM, OutError))
			{
				return false;
			}
		}
		else if (!NeedNumber(Item, TEXT("poleHeightM"), Prop.PoleHeightM, OutError)
			|| !NeedNumber(Item, TEXT("poleRadiusM"), Prop.PoleRadiusM, OutError)
			|| !NeedVector(Item, TEXT("clothSizeM"), 3, Prop.ClothSizeM, OutError))
		{
			return false;
		}
		Out.Add(Prop);
	}
	return true;
}

bool LoadThreats(const TSharedPtr<FJsonObject>& Root, FArenaLayout& Layout, FString& OutError)
{
	const TSharedPtr<FJsonObject>* Threats = nullptr;
	if (!NeedObject(Root, TEXT("threats"), Threats, OutError))
	{
		return false;
	}
	if (!NeedNumber(*Threats, TEXT("chestHeightAboveSandM"), Layout.ChestHeightAboveSandM, OutError)
		|| !NeedNumber(*Threats, TEXT("staggerS"), Layout.ThreatStaggerS, OutError)
		|| !NeedNumber(*Threats, TEXT("telegraphStartRadiusM"), Layout.TelegraphStartRadiusM, OutError)
		|| !NeedNumber(*Threats, TEXT("telegraphEndRadiusM"), Layout.TelegraphEndRadiusM, OutError)
		|| !NeedInt(*Threats, TEXT("telegraphSegments"), Layout.TelegraphSegments, OutError)
		|| !NeedNumber(*Threats, TEXT("telegraphHeightM"), Layout.TelegraphHeightM, OutError)
		|| !NeedNumber(*Threats, TEXT("telegraphWidthM"), Layout.TelegraphWidthM, OutError)
		|| !NeedNumber(*Threats, TEXT("rimScale"), Layout.ThreatRimScale, OutError)
		|| !NeedNumber(*Threats, TEXT("rimTubeScale"), Layout.ThreatRimTubeScale, OutError))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Order = nullptr;
	if (!(*Threats)->TryGetArrayField(TEXT("order"), Order) || !Order)
	{
		return Fail(OutError, TEXT("threats.order missing"));
	}
	for (const TSharedPtr<FJsonValue>& Value : *Order)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		FThreatSpec Spec;
		if (!NeedString(Item, TEXT("kind"), Spec.Kind, OutError)
			|| !NeedString(Item, TEXT("colourClass"), Spec.ColourClass, OutError)
			|| !NeedColour(Item, TEXT("body"), Spec.Body, OutError)
			|| !NeedColour(Item, TEXT("rim"), Spec.Rim, OutError)
			|| !NeedColour(Item, TEXT("telegraph"), Spec.Telegraph, OutError)
			|| !NeedBool(Item, TEXT("elongated"), Spec.bElongated, OutError)
			|| !NeedBool(Item, TEXT("rimmed"), Spec.bRimmed, OutError)
			|| !NeedString(Item, TEXT("marker"), Spec.MarkerId, OutError)
			|| !NeedNumber(Item, TEXT("flightS"), Spec.FlightS, OutError)
			|| !NeedNumber(Item, TEXT("landingForwardM"), Spec.LandingForwardM, OutError)
			|| !NeedNumber(Item, TEXT("landingRightM"), Spec.LandingRightM, OutError)
			|| !NeedNumber(Item, TEXT("radiusM"), Spec.RadiusM, OutError)
			|| !NeedNumber(Item, TEXT("lengthM"), Spec.LengthM, OutError))
		{
			return false;
		}
		Layout.Threats.Add(Spec);
	}
	return true;
}
}

FString FArenaLayout::DefaultFilePath()
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/arena-layout.json"));
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

FString FArenaLayout::StyleFilePathFromCommandLine()
{
	FString StyleId;
	if (!FParse::Value(FCommandLine::Get(), TEXT("MageArenaStyle="), StyleId) || StyleId.IsEmpty() || StyleId == TEXT("greybox"))
	{
		return FString();
	}
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/styles/style-") + StyleId + TEXT(".json"));
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

int32 FArenaLayout::ApplyStyle(const FJsonObject& Style)
{
	auto Read = [](const FJsonObject& Object, const TCHAR* Field, FLinearColor& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.TryGetArrayField(Field, Values) || !Values || Values->Num() < 3)
		{
			return 0;
		}
		Out = FLinearColor(static_cast<float>((*Values)[0]->AsNumber()), static_cast<float>((*Values)[1]->AsNumber()),
		    static_cast<float>((*Values)[2]->AsNumber()), Out.A);
		return 1;
	};
	int32 Applied = 0;
	const TSharedPtr<FJsonObject>* Arena = nullptr;
	if (Style.TryGetObjectField(TEXT("arena"), Arena) && Arena && Arena->IsValid())
	{
		const TPair<const TCHAR*, FLinearColor*> Fields[] = {
		    {TEXT("sand"), &Sand},
		    {TEXT("stone"), &Stone},
		    {TEXT("wall"), &WallColour},
		    {TEXT("dais"), &Dais},
		    {TEXT("padRing"), &PadRing},
		    {TEXT("seat"), &Seat},
		    {TEXT("marker"), &Marker},
		    {TEXT("brazier"), &Brazier},
		    {TEXT("bannerCloth"), &BannerCloth},
		    {TEXT("bannerPole"), &BannerPole},
		    {TEXT("hand"), &Hand},
		    {TEXT("castFlash"), &CastFlash},
		    {TEXT("rejectFlash"), &RejectFlash},
		    {TEXT("wardArc"), &WardArc},
		};
		for (const TPair<const TCHAR*, FLinearColor*>& Field : Fields)
		{
			Applied += Read(**Arena, Field.Key, *Field.Value);
		}
	}
	// T60: no brazier flame colour comes from a style. `projectiles.fire` is the fire spell's absorb colour, so a flame in
	// it read as an incoming fire bolt from the corner of the eye; `arena.flame` is no better (style 25 samples it as the
	// leave red, which over its near-black brazier is the unblockable's pair). The flame is an environment colour of its
	// own, derived in Greybox/BrazierFire.h, out of every threat class by chroma, its luminance between the rim's and the
	// orange glows' (T61).
	const TSharedPtr<FJsonObject>* Sky = nullptr;
	if (Style.TryGetObjectField(TEXT("sky"), Sky) && Sky && Sky->IsValid())
	{
		const int32 SkyApplied = Read(**Sky, TEXT("horizon"), SkyHorizon) + Read(**Sky, TEXT("zenith"), SkyZenith);
		if (SkyApplied == 2)
		{
			// The style files sample two sky tones; the dome's middle band sits between them.
			SkyUpper = FLinearColor((SkyHorizon.R + SkyZenith.R) * 0.5f, (SkyHorizon.G + SkyZenith.G) * 0.5f,
			    (SkyHorizon.B + SkyZenith.B) * 0.5f, SkyUpper.A);
		}
		Applied += SkyApplied;
	}
	if (Applied > 0)
	{
		// The dusk sun is orange and the sky light blue; a style keeps their energy (luminance) and drops their hue. White
		// at full strength drew a style's sand about three times its sampled colour (T16, style 25, linear).
		// T56: the dusk haze is warm too, and it veils the far stands a style paints.
		for (FLinearColor* Light : {&DirectionalColour, &SkyLightColour, &FogInscatter, &FogSunInscatter})
		{
			const float Luminance = Light->GetLuminance();
			*Light = FLinearColor(Luminance, Luminance, Luminance, Light->A);
		}
	}
	StyleColours = Applied;
	return Applied;
}

void FArenaLayout::ApplyDuskLight()
{
	// Linear colours. The pawn's camera has manual exposure at bias 0 and no tonemapper, so the unlit dome shows its
	// colours as they are and a lit surface shows its albedo times (sun lux x cos + sky fill).
	// A sun 20 degrees up throws a shadow 2.7 times an object's height; it stays high enough that the far stands'
	// shadow ends short of the far spawn markers. Yaw 165 sets it ahead and a little left of the seated view, behind
	// the opponent as in key art S1, so shadows fall toward the player.
	DirectionalPitchDeg = -20.0;
	DirectionalYawDeg = 165.0;
	DirectionalIntensityLux = 2.4;
	DirectionalColour = FLinearColor(1.0f, 0.50f, 0.16f);
	SunSourceAngleDeg = 2.0;
	bCastShadows = true;
	// The far stands are about 33 m from the seated eye; two cascades cover them at the Quest cascade cap.
	ShadowDistanceM = 45.0;
	ShadowCascades = 2;
	// The shadow side is lit by the sky alone: a rose-lilac fill at about three quarters of the sun on the sand, so
	// it stays readable and warm, as the key art's shadow side is.
	SkyIntensity = 0.8;
	SkyLightColour = FLinearColor(0.62f, 0.40f, 0.42f);
	// Gold at the horizon, rose, then lilac at the zenith. Every band stays out of the ember-orange threat class.
	SkyBands = 5;
	SkyHorizon = FLinearColor(0.98f, 0.52f, 0.20f);
	SkyUpper = FLinearColor(0.66f, 0.34f, 0.32f);
	SkyZenith = FLinearColor(0.42f, 0.27f, 0.40f);
	// Fog starts past the farthest spawn marker (15 m from the centre pad), so no threat colour is veiled, and stops
	// short of the dome (58 m and more from the seated eye), so the bands keep their colours.
	bHeightFog = true;
	FogInscatter = FLinearColor(0.90f, 0.52f, 0.30f);
	FogSunInscatter = FLinearColor(1.0f, 0.55f, 0.22f);
	FogDensity = 0.035;
	FogHeightFalloff = 0.2;
	FogStartM = 20.0;
	FogCutoffM = 50.0;
}

FLinearColor FArenaLayout::SkyBandColour(int32 Band) const
{
	const int32 Bands = FMath::Max(SkyBands, 3);
	const float T = static_cast<float>(FMath::Clamp(Band, 0, Bands - 1)) / static_cast<float>(Bands - 1);
	return T <= 0.5f ? FMath::Lerp(SkyHorizon, SkyUpper, T * 2.f) : FMath::Lerp(SkyUpper, SkyZenith, T * 2.f - 1.f);
}

bool FArenaLayout::LoadFromFile(const FString& Path, FString& OutError)
{
	*this = FArenaLayout();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return Fail(OutError, FString::Printf(TEXT("could not read %s"), *Path));
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Fail(OutError, FString::Printf(TEXT("could not parse %s"), *Path));
	}

	FString Units;
	if (!NeedString(Root, TEXT("units"), Units, OutError) || Units != TEXT("metres"))
	{
		return Fail(OutError, TEXT("units must be metres"));
	}
	if (!NeedVector(Root, TEXT("arenaCentreM"), 3, ArenaCentreM, OutError)
		|| !NeedNumber(Root, TEXT("seatedEyeHeightAbovePadM"), SeatedEyeHeightAbovePadM, OutError)
		|| !NeedNumber(Root, TEXT("eyeForwardOfSeatedOriginM"), EyeForwardOfSeatedOriginM, OutError)
		|| !NeedNumber(Root, TEXT("eyeAboveSeatedOriginM"), EyeAboveSeatedOriginM, OutError)
		|| !NeedNumber(Root, TEXT("cameraFovDeg"), CameraFovDeg, OutError)
		|| !NeedVector(Root, TEXT("lookTargetM"), 3, LookTargetM, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Floor = nullptr;
	const TSharedPtr<FJsonObject>* Wall = nullptr;
	const TSharedPtr<FJsonObject>* Stands = nullptr;
	const TSharedPtr<FJsonObject>* DaisObject = nullptr;
	const TSharedPtr<FJsonObject>* SeatObject = nullptr;
	const TSharedPtr<FJsonObject>* Look = nullptr;
	const TSharedPtr<FJsonObject>* Blink = nullptr;
	const TSharedPtr<FJsonObject>* Colours = nullptr;
	const TSharedPtr<FJsonObject>* Hands = nullptr;
	const TSharedPtr<FJsonObject>* Lighting = nullptr;
	if (!NeedObject(Root, TEXT("floor"), Floor, OutError)
		|| !NeedNumber(*Floor, TEXT("radiusXM"), FloorRadiusXM, OutError)
		|| !NeedNumber(*Floor, TEXT("radiusYM"), FloorRadiusYM, OutError)
		|| !NeedNumber(*Floor, TEXT("thicknessM"), FloorThicknessM, OutError)
		|| !NeedObject(Root, TEXT("wall"), Wall, OutError)
		|| !NeedNumber(*Wall, TEXT("heightM"), WallHeightM, OutError)
		|| !NeedNumber(*Wall, TEXT("thicknessM"), WallThicknessM, OutError)
		|| !NeedInt(*Wall, TEXT("segments"), WallSegments, OutError)
		|| !NeedInt(*Wall, TEXT("merlonEvery"), MerlonEvery, OutError)
		|| !NeedVector(*Wall, TEXT("merlonSizeM"), 3, MerlonSizeM, OutError)
		|| !NeedObject(Root, TEXT("stands"), Stands, OutError)
		|| !NeedInt(*Stands, TEXT("tiers"), StandTiers, OutError)
		|| !NeedNumber(*Stands, TEXT("depthM"), StandDepthM, OutError)
		|| !NeedNumber(*Stands, TEXT("gapOutsideWallM"), StandGapM, OutError)
		|| !NeedNumber(*Stands, TEXT("firstTopAboveWallM"), StandFirstTopAboveWallM, OutError)
		|| !NeedNumber(*Stands, TEXT("stepRiseM"), StandStepRiseM, OutError)
		|| !NeedObject(Root, TEXT("dais"), DaisObject, OutError)
		|| !NeedVector(*DaisObject, TEXT("centreM"), 3, DaisCentreM, OutError)
		|| !NeedVector(*DaisObject, TEXT("sizeM"), 3, DaisSizeM, OutError)
		|| !NeedNumber(*DaisObject, TEXT("heightM"), DaisHeightM, OutError)
		|| !NeedObject(Root, TEXT("seat"), SeatObject, OutError)
		|| !NeedVector(*SeatObject, TEXT("sizeM"), 3, SeatSizeM, OutError))
	{
		return false;
	}
	if (!LoadPads(Root, *this, OutError) || !LoadMarkers(Root, *this, OutError))
	{
		return false;
	}
	if (!LoadProps(Root, TEXT("braziers"), true, Braziers, OutError)
		|| !LoadProps(Root, TEXT("banners"), false, Banners, OutError))
	{
		return false;
	}

	if (!NeedObject(Root, TEXT("mouseLook"), Look, OutError)
		|| !NeedNumber(*Look, TEXT("yawLimitDeg"), YawLimitDeg, OutError)
		|| !NeedNumber(*Look, TEXT("pitchDownDeg"), PitchDownDeg, OutError)
		|| !NeedNumber(*Look, TEXT("pitchUpDeg"), PitchUpDeg, OutError)
		|| !NeedNumber(*Look, TEXT("sensitivityDegPerPixel"), LookSensitivity, OutError)
		|| !NeedObject(Root, TEXT("blink"), Blink, OutError)
		|| !NeedNumber(*Blink, TEXT("fadeOutS"), BlinkFadeOutS, OutError)
		|| !NeedNumber(*Blink, TEXT("fadeInS"), BlinkFadeInS, OutError)
		|| !NeedNumber(*Blink, TEXT("vignettePeak"), BlinkVignettePeak, OutError))
	{
		return false;
	}

	double WardAlpha = 0.4;
	if (!NeedObject(Root, TEXT("colours"), Colours, OutError)
		|| !NeedColour(*Colours, TEXT("sand"), Sand, OutError)
		|| !NeedColour(*Colours, TEXT("stone"), Stone, OutError)
		|| !NeedColour(*Colours, TEXT("dais"), Dais, OutError)
		|| !NeedColour(*Colours, TEXT("padRing"), PadRing, OutError)
		|| !NeedColour(*Colours, TEXT("seat"), Seat, OutError)
		|| !NeedColour(*Colours, TEXT("marker"), Marker, OutError)
		|| !NeedColour(*Colours, TEXT("brazier"), Brazier, OutError)
		|| !NeedColour(*Colours, TEXT("bannerCloth"), BannerCloth, OutError)
		|| !NeedColour(*Colours, TEXT("bannerPole"), BannerPole, OutError)
		|| !NeedColour(*Colours, TEXT("hand"), Hand, OutError)
		|| !NeedColour(*Colours, TEXT("castFlash"), CastFlash, OutError)
		|| !NeedColour(*Colours, TEXT("rejectFlash"), RejectFlash, OutError)
		|| !NeedColour(*Colours, TEXT("wardArc"), WardArc, OutError)
		|| !NeedNumber(*Colours, TEXT("wardArcAlpha"), WardAlpha, OutError))
	{
		return false;
	}
	WardArc.A = static_cast<float>(WardAlpha);

	if (!LoadThreats(Root, *this, OutError))
	{
		return false;
	}
	if (!NeedObject(Root, TEXT("hands"), Hands, OutError)
		|| !NeedNumber(*Hands, TEXT("jointRadiusM"), JointRadiusM, OutError)
		|| !NeedNumber(*Hands, TEXT("boneRadiusM"), BoneRadiusM, OutError)
		|| !NeedNumber(*Hands, TEXT("flashRadiusM"), FlashRadiusM, OutError)
		|| !NeedInt(*Hands, TEXT("flashSegments"), FlashSegments, OutError)
		|| !NeedNumber(*Hands, TEXT("flashDurationS"), FlashDurationS, OutError)
		|| !NeedNumber(*Hands, TEXT("wardArcDeg"), WardArcDeg, OutError)
		|| !NeedNumber(*Hands, TEXT("wardArcInnerM"), WardArcInnerM, OutError)
		|| !NeedNumber(*Hands, TEXT("wardArcOuterM"), WardArcOuterM, OutError)
		|| !NeedNumber(*Hands, TEXT("wardArcHeightM"), WardArcHeightM, OutError)
		|| !NeedInt(*Hands, TEXT("wardArcSegments"), WardArcSegments, OutError)
		|| !NeedObject(Root, TEXT("lighting"), Lighting, OutError)
		|| !NeedNumber(*Lighting, TEXT("directionalIntensityLux"), DirectionalIntensityLux, OutError)
		|| !NeedNumber(*Lighting, TEXT("directionalPitchDeg"), DirectionalPitchDeg, OutError)
		|| !NeedNumber(*Lighting, TEXT("directionalYawDeg"), DirectionalYawDeg, OutError)
		|| !NeedColour(*Lighting, TEXT("directionalColour"), DirectionalColour, OutError)
		|| !NeedNumber(*Lighting, TEXT("skyIntensity"), SkyIntensity, OutError)
		|| !NeedColour(*Lighting, TEXT("skyLightColour"), SkyLightColour, OutError)
		|| !NeedColour(*Lighting, TEXT("skyHorizon"), SkyHorizon, OutError)
		|| !NeedColour(*Lighting, TEXT("skyUpper"), SkyUpper, OutError)
		|| !NeedColour(*Lighting, TEXT("skyZenith"), SkyZenith, OutError)
		|| !NeedBool(*Lighting, TEXT("castShadows"), bCastShadows, OutError))
	{
		return false;
	}
	WallColour = Stone;
	ApplyDuskLight();

	const FString StylePath = StyleFilePathFromCommandLine();
	if (!StylePath.IsEmpty())
	{
		FString StyleJson;
		TSharedPtr<FJsonObject> StyleRoot;
		if (FFileHelper::LoadFileToString(StyleJson, *StylePath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(StyleJson), StyleRoot) && StyleRoot.IsValid())
		{
			ApplyStyle(*StyleRoot);
		}
		UE_LOG(LogMageArena, Log, TEXT("arena style: %d colours applied from %s"), StyleColours, *StylePath);
	}
	return true;
}

const FRunePad* FArenaLayout::FindPad(int32 Index) const
{
	for (const FRunePad& Pad : Pads)
	{
		if (Pad.Index == Index)
		{
			return &Pad;
		}
	}
	return nullptr;
}

const FRunePad* FArenaLayout::FindPadById(const FString& Id) const
{
	for (const FRunePad& Pad : Pads)
	{
		if (Pad.Id == Id)
		{
			return &Pad;
		}
	}
	return nullptr;
}

const FSpawnMarker* FArenaLayout::FindMarker(const FString& Id) const
{
	for (const FSpawnMarker& Candidate : SpawnMarkers)
	{
		if (Candidate.Id == Id)
		{
			return &Candidate;
		}
	}
	return nullptr;
}

FVector FArenaLayout::FlatForwardM(const FRunePad& Pad) const
{
	FVector Toward = ArenaCentreM - Pad.PositionM;
	Toward.Z = 0.0;
	if (Toward.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}
	return Toward.GetSafeNormal();
}

FVector FArenaLayout::EyeLocationM(const FRunePad& Pad) const
{
	return FVector(Pad.PositionM.X, Pad.PositionM.Y, Pad.PositionM.Z + SeatedEyeHeightAbovePadM);
}

double FArenaLayout::SurfaceHeightM(double XM, double YM) const
{
	const double HalfX = FMath::Abs(DaisSizeM.X) * 0.5;
	const double HalfY = FMath::Abs(DaisSizeM.Y) * 0.5;
	const double TopZ = DaisCentreM.Z + FMath::Abs(DaisSizeM.Z) * 0.5;
	if (FMath::Abs(XM - DaisCentreM.X) <= HalfX && FMath::Abs(YM - DaisCentreM.Y) <= HalfY)
	{
		return TopZ;
	}
	return 0.0;
}

double FArenaLayout::HorizontalPadSpacingM(int32 IndexA, int32 IndexB) const
{
	const FRunePad* A = FindPad(IndexA);
	const FRunePad* B = FindPad(IndexB);
	if (!A || !B)
	{
		return -1.0;
	}
	const double DX = A->PositionM.X - B->PositionM.X;
	const double DY = A->PositionM.Y - B->PositionM.Y;
	return FMath::Sqrt(DX * DX + DY * DY);
}
