#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Budget/BudgetFx.h"
#include "Dom/JsonObject.h"
#include "Greybox/ArenaLayout.h"
#include "Greybox/ThreatDemo.h"
#include "Hands/MageArenaPawn.h"
#include "Session/ElementColour.h"
#include "Session/PresentationLook.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool IsTurquoise(const FLinearColor& Colour)
{
	return Colour.R < 0.35f && Colour.G > 0.45f && Colour.B > 0.45f;
}

// T26: Fire is ember orange (0.90, 0.26, 0.02), kept away from the unblockable rim red (G 0.014).
bool IsEmberOrange(const FLinearColor& Colour)
{
	return Colour.R > 0.7f && Colour.G > 0.15f && Colour.G < 0.45f && Colour.B < 0.25f;
}

bool IsSteel(const FLinearColor& Colour)
{
	const float Least = FMath::Min3(Colour.R, Colour.G, Colour.B);
	const float Most = FMath::Max3(Colour.R, Colour.G, Colour.B);
	return Colour.R > 0.65f && Colour.G > 0.65f && Colour.B > 0.65f && (Most - Least) < 0.12f;
}

bool IsBlackCore(const FLinearColor& Colour)
{
	return Colour.R < 0.08f && Colour.G < 0.08f && Colour.B < 0.08f;
}

bool IsRedRim(const FLinearColor& Colour)
{
	return Colour.R > 0.65f && Colour.G < 0.2f && Colour.B < 0.2f;
}

double BearingDeg(const FVector& Forward, const FVector& Offset)
{
	const double Dot = Forward.X * Offset.X + Forward.Y * Offset.Y;
	const double Cross = Forward.X * Offset.Y - Forward.Y * Offset.X;
	return FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaGreyboxLayout, "MageArena.Greybox.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaGreyboxLayout::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	FString Error;
	if (!TestTrue(TEXT("layout file loads"), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error)))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("three pads"), Layout.Pads.Num(), 3);
	const FRunePad* Left = Layout.FindPadById(TEXT("left"));
	const FRunePad* Centre = Layout.FindPadById(TEXT("centre"));
	const FRunePad* Right = Layout.FindPadById(TEXT("right"));
	if (!TestNotNull(TEXT("left pad"), Left) || !TestNotNull(TEXT("centre pad"), Centre) || !TestNotNull(TEXT("right pad"), Right))
	{
		return false;
	}
	TestEqual(TEXT("left index"), Left->Index, 0);
	TestEqual(TEXT("centre index"), Centre->Index, 1);
	TestEqual(TEXT("right index"), Right->Index, 2);

	const double SpacingLeft = Layout.HorizontalPadSpacingM(0, 1);
	const double SpacingRight = Layout.HorizontalPadSpacingM(1, 2);
	TestTrue(TEXT("left-centre spacing is 2.5 to 3.5 m"), SpacingLeft >= 2.5 && SpacingLeft <= 3.5);
	TestTrue(TEXT("centre-right spacing is 2.5 to 3.5 m"), SpacingRight >= 2.5 && SpacingRight <= 3.5);
	TestTrue(TEXT("seated eye height is 1.2 m"), FMath::IsNearlyEqual(Layout.SeatedEyeHeightAbovePadM, 1.2, 1.0e-6));
	TestTrue(TEXT("dais top is 0.6 m"), FMath::IsNearlyEqual(Layout.SurfaceHeightM(Layout.DaisCentreM.X, Layout.DaisCentreM.Y), 0.6, 1.0e-4));
	TestTrue(TEXT("arena centre is sand"), FMath::IsNearlyEqual(Layout.SurfaceHeightM(0.0, 0.0), 0.0, 1.0e-6));
	// Two fields state the dais top: dais.heightM feeds the VR throw rules, centreM + sizeM feed SurfaceHeightM and the
	// threat landings. Nothing else ties them, so a one-sided edit to the JSON would split the two consumers.
	TestTrue(TEXT("dais.heightM is the top that SurfaceHeightM reports"),
		FMath::IsNearlyEqual(Layout.DaisHeightM, Layout.SurfaceHeightM(Layout.DaisCentreM.X, Layout.DaisCentreM.Y), 1.0e-6));

	AMageArenaPawn* Pawn = NewObject<AMageArenaPawn>(GetTransientPackage());
	if (!TestNotNull(TEXT("pawn constructs"), Pawn))
	{
		return false;
	}
	Pawn->AddToRoot();
	Pawn->ConfigureFromLayout(Layout);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Pawn->BlinkToPad(Index);
		TestEqual(*FString::Printf(TEXT("blink selects pad %d"), Index), Pawn->GetActivePadIndex(), Index);
		TestTrue(*FString::Printf(TEXT("pad %d camera is 120 cm above the pad top"), Index),
			FMath::IsNearlyEqual(Pawn->GetCameraHeightAbovePadCm(), 120.0, 0.5));
	}

	Pawn->BlinkToPad(1);
	const float Facing = Pawn->GetFacingYawDegrees();
	FRotator Wild(0.f, Facing + 170.f, 40.f);
	const FRotator Clamped = Pawn->ClampLookRotation(Wild);
	const float DeltaYaw = FRotator::NormalizeAxis(Clamped.Yaw - Facing);
	TestTrue(TEXT("yaw 170 degrees off facing clamps to the limit"), FMath::IsNearlyEqual(FMath::Abs(DeltaYaw), Pawn->GetYawLimitDegrees(), 0.05f));
	TestTrue(TEXT("clamped look has no roll"), FMath::IsNearlyEqual(Clamped.Roll, 0.f, 0.01f));
	const float Raised = AMageArenaPawn::SeatedPitchFromMouse(0.f, 25.f, Pawn->GetLookSensitivity(), Pawn->GetPitchDownDegrees(), Pawn->GetPitchUpDegrees());
	const float Lowered = AMageArenaPawn::SeatedPitchFromMouse(0.f, -25.f, Pawn->GetLookSensitivity(), Pawn->GetPitchDownDegrees(), Pawn->GetPitchUpDegrees());
	TestTrue(TEXT("positive mouse DY raises pitch"), Raised > 0.f);
	TestTrue(TEXT("negative mouse DY lowers pitch"), Lowered < 0.f);
	const float High = AMageArenaPawn::SeatedPitchFromMouse(0.f, 40.f, 1.f, Pawn->GetPitchDownDegrees(), Pawn->GetPitchUpDegrees());
	TestTrue(TEXT("pitch clamps at the up limit"), FMath::IsNearlyEqual(High, Pawn->GetPitchUpDegrees(), 0.01f));

	Pawn->RemoveFromRoot();
	Pawn->MarkAsGarbage();

	const FVector Forward = Layout.FlatForwardM(*Centre);
	bool bHas8 = false;
	bool bHas15 = false;
	TestTrue(TEXT("spawn markers exist"), Layout.SpawnMarkers.Num() > 0);
	for (const FSpawnMarker& Marker : Layout.SpawnMarkers)
	{
		const FVector Offset(Marker.PositionM.X - Centre->PositionM.X, Marker.PositionM.Y - Centre->PositionM.Y, 0.0);
		const double Distance = Offset.Size();
		const bool bNear = FMath::IsNearlyEqual(Distance, 8.0, 0.05);
		const bool bFar = FMath::IsNearlyEqual(Distance, 15.0, 0.05);
		TestTrue(*FString::Printf(TEXT("%s is at 8 m or 15 m (%.3f)"), *Marker.Id, Distance), bNear || bFar);
		bHas8 = bHas8 || bNear;
		bHas15 = bHas15 || bFar;
		const double Bearing = BearingDeg(Forward, Offset);
		TestTrue(*FString::Printf(TEXT("%s bearing %.2f is inside +/-70"), *Marker.Id, Bearing), FMath::Abs(Bearing) <= 70.0);
		TestTrue(*FString::Printf(TEXT("%s documented distance matches the position"), *Marker.Id),
			FMath::IsNearlyEqual(Marker.DistanceM, Distance, 0.05));
		TestTrue(*FString::Printf(TEXT("%s documented bearing matches the position"), *Marker.Id),
			FMath::IsNearlyEqual(Marker.BearingDeg, Bearing, 0.5));
	}
	TestTrue(TEXT("a marker sits at 8 m"), bHas8);
	TestTrue(TEXT("a marker sits at 15 m"), bHas15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaGreyboxThreatColours, "MageArena.Greybox.ThreatColours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaGreyboxThreatColours::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	FString Error;
	if (!TestTrue(TEXT("layout file loads"), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("four threats"), Layout.Threats.Num(), 4))
	{
		return false;
	}

	const TCHAR* Kinds[] = { TEXT("water"), TEXT("fire"), TEXT("steel"), TEXT("unblockable") };
	const TCHAR* Classes[] = { TEXT("turquoise"), TEXT("ember-orange"), TEXT("steel"), TEXT("black-core-red-rim") };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestEqual(*FString::Printf(TEXT("threat %d kind"), Index), Layout.Threats[Index].Kind, FString(Kinds[Index]));
		TestEqual(*FString::Printf(TEXT("threat %d class"), Index), Layout.Threats[Index].ColourClass, FString(Classes[Index]));
	}

	const FThreatSpec& Water = Layout.Threats[0];
	const FThreatSpec& Fire = Layout.Threats[1];
	const FThreatSpec& Steel = Layout.Threats[2];
	const FThreatSpec& Unblockable = Layout.Threats[3];

	TestTrue(TEXT("water body is turquoise"), IsTurquoise(Water.Body));
	TestTrue(TEXT("water telegraph is turquoise"), IsTurquoise(Water.Telegraph));
	TestFalse(TEXT("water is not a spear"), Water.bElongated);
	TestFalse(TEXT("water has no rim"), Water.bRimmed);

	TestTrue(TEXT("fire body is ember orange"), IsEmberOrange(Fire.Body));
	TestTrue(TEXT("fire telegraph is ember orange"), IsEmberOrange(Fire.Telegraph));
	TestFalse(TEXT("fire is not a spear"), Fire.bElongated);

	TestTrue(TEXT("steel body is white/grey"), IsSteel(Steel.Body));
	TestTrue(TEXT("steel telegraph is white/grey"), IsSteel(Steel.Telegraph));
	TestTrue(TEXT("steel is elongated"), Steel.bElongated);
	TestFalse(TEXT("steel has no rim"), Steel.bRimmed);

	TestTrue(TEXT("unblockable core is black"), IsBlackCore(Unblockable.Body));
	TestTrue(TEXT("unblockable rim is red"), IsRedRim(Unblockable.Rim));
	TestTrue(TEXT("unblockable telegraph is red"), IsRedRim(Unblockable.Telegraph));
	TestTrue(TEXT("unblockable is rimmed"), Unblockable.bRimmed);
	TestFalse(TEXT("unblockable is not a spear"), Unblockable.bElongated);

	const FRunePad* CentrePad = Layout.FindPad(1);
	if (!TestNotNull(TEXT("centre pad for the planner"), CentrePad))
	{
		return false;
	}
	const TArray<FThreatLaunch> Plan = BuildThreatLaunches(Layout, *CentrePad);
	if (!TestEqual(TEXT("planner launches four threats"), Plan.Num(), 4))
	{
		return false;
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestEqual(*FString::Printf(TEXT("plan %d kind"), Index), Plan[Index].Spec.Kind, FString(Kinds[Index]));
		TestEqual(*FString::Printf(TEXT("plan %d class"), Index), Plan[Index].Spec.ColourClass, FString(Classes[Index]));
		const double Surface = Layout.SurfaceHeightM(Plan[Index].LandingM.X, Plan[Index].LandingM.Y);
		TestTrue(*FString::Printf(TEXT("plan %d landing sits on the surface"), Index),
			FMath::IsNearlyEqual(Plan[Index].LandingM.Z, Surface, 1.0e-4));
		TestTrue(*FString::Printf(TEXT("plan %d ends at chest height"), Index),
			FMath::IsNearlyEqual(Plan[Index].EndM.Z, Layout.ChestHeightAboveSandM, 1.0e-4));
	}
	TestTrue(TEXT("water lands on the sand"), FMath::IsNearlyEqual(Plan[0].LandingM.Z, 0.0, 1.0e-4));
	TestTrue(TEXT("fire lands on the dais"), FMath::IsNearlyEqual(Plan[1].LandingM.Z, 0.6, 1.0e-3));
	TestTrue(TEXT("steel lands on the dais"), FMath::IsNearlyEqual(Plan[2].LandingM.Z, 0.6, 1.0e-3));
	TestTrue(TEXT("unblockable lands on the sand"), FMath::IsNearlyEqual(Plan[3].LandingM.Z, 0.0, 1.0e-4));

	const double RimRadius = Plan[3].Spec.RadiusM * Layout.ThreatRimScale;
	const double Tube = Plan[3].Spec.RadiusM * Layout.ThreatRimTubeScale;
	TestTrue(TEXT("rim radius is outside the core"), RimRadius > Plan[3].Spec.RadiusM);
	TestTrue(TEXT("rim tube leaves the core visible"), RimRadius - Tube * 0.5 >= Plan[3].Spec.RadiusM - 1.0e-4);
	return true;
}

// T49 (owner ruling 2026-10-21). A near-black body with a red rim means "leave" and belongs to the unblockable alone:
// exactly one layout threat wears it, and the HP bar the game builds (PresentationLook) wears neither half.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaGreyboxThreatPairReserved, "MageArena.Greybox.ThreatPairReserved",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaGreyboxThreatPairReserved::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	FString Error;
	if (!TestTrue(TEXT("layout file loads"), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error)))
	{
		AddError(Error);
		return false;
	}
	int32 Pairs = 0;
	for (const FThreatSpec& Threat : Layout.Threats)
	{
		if (IsBlackCore(Threat.Body) && IsRedRim(Threat.Rim))
		{
			++Pairs;
			TestEqual(TEXT("the black core with a red rim is the unblockable"), Threat.Kind, FString(TEXT("unblockable")));
		}
	}
	TestEqual(TEXT("exactly one layout threat is a black core with a red rim"), Pairs, 1);

	const FLinearColor Back = PresentationLook::HpBackColour();
	const FLinearColor Fill = PresentationLook::HpFillColour();
	for (const FLinearColor& Part : {Back, Fill})
	{
		TestFalse(*FString::Printf(TEXT("HP colour %s is not a black core"), *Part.ToString()), IsBlackCore(Part));
		TestFalse(*FString::Printf(TEXT("HP colour %s is not a red rim"), *Part.ToString()), IsRedRim(Part));
	}
	// Not just under the IsBlackCore line: the back's brightest channel sits well above it.
	TestTrue(TEXT("HP back is not near-black"), FMath::Max3(Back.R, Back.G, Back.B) >= 0.1f);
	// The bar stays a bar: the fill stands off the back by luminance (WCAG ratio on linear values).
	const float Ratio = (Fill.GetLuminance() + 0.05f) / (Back.GetLuminance() + 0.05f);
	TestTrue(*FString::Printf(TEXT("HP fill stands off the back (ratio %.2f)"), Ratio), Ratio >= 2.5f);
	// Neither part borrows a cue colour: every element, steel and the unblockable pair are far away in linear RGB.
	const FElementPalette Palette = FElementPalette::Greybox();
	const FLinearColor Cues[] = {Palette.Water, Palette.Fire, Palette.Air, Palette.Earth, Palette.Steel, Palette.UnblockableBody, Palette.UnblockableRim};
	for (const FLinearColor& Cue : Cues)
	{
		TestTrue(*FString::Printf(TEXT("HP fill is not the cue colour %s"), *Cue.ToString()), FLinearColor::Dist(Fill, Cue) > 0.3f);
	}
	TestTrue(TEXT("HP back is not the unblockable core"), FLinearColor::Dist(Back, Palette.UnblockableBody) > 0.1f);
	return true;
}

// T49. FElementPalette is the one definition of the unblockable red. The layout JSON keeps its own copy (the data is not
// edited by hand), so this holds it to the palette; the D-G4 telegraph bake reads the palette itself.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaGreyboxUnblockableRed, "MageArena.Greybox.UnblockableRed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaGreyboxUnblockableRed::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	FString Error;
	if (!TestTrue(TEXT("layout file loads"), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error)))
	{
		AddError(Error);
		return false;
	}
	const FElementPalette Palette = FElementPalette::Greybox();
	const FThreatSpec* Unblockable = Layout.Threats.FindByPredicate([](const FThreatSpec& Threat)
	{ return Threat.Kind == TEXT("unblockable"); });
	if (!TestNotNull(TEXT("layout has an unblockable threat"), Unblockable))
	{
		return false;
	}
	TestTrue(*FString::Printf(TEXT("layout unblockable rim %s is the palette's"), *Unblockable->Rim.ToString()), Unblockable->Rim.Equals(Palette.UnblockableRim, 1.0e-4f));
	TestTrue(*FString::Printf(TEXT("layout unblockable telegraph %s is the palette's"), *Unblockable->Telegraph.ToString()),
	    Unblockable->Telegraph.Equals(Palette.UnblockableRim, 1.0e-4f));
	TestTrue(*FString::Printf(TEXT("layout unblockable body %s is the palette's"), *Unblockable->Body.ToString()), Unblockable->Body.Equals(Palette.UnblockableBody, 1.0e-4f));

	const FBudgetFxRow* Telegraph = BudgetStress::FxRows().FindByPredicate([](const FBudgetFxRow& Row)
	{ return Row.Name == TEXT("NS_Budget_Telegraph"); });
	if (!TestNotNull(TEXT("the D-G4 table has the telegraph row"), Telegraph))
	{
		return false;
	}
	TestTrue(*FString::Printf(TEXT("NS_Budget_Telegraph colour %s is the palette's"), *Telegraph->Colour.ToString()), Telegraph->Colour.Equals(Palette.UnblockableRim, 1.0e-6f));
	return true;
}

// T16 (Retry 1 item 3): a style recolours every greybox surface - sand, stands, wall, dais, pads, seats, markers, braziers,
// banners, hands and the sky dome - under neutral light. Without -MageArenaStyle the layout is the greybox, and no surface
// a style paints is the unblockable's red rim, so no near-black prop beside it can form the leave pair.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaGreyboxStyleSkin, "MageArena.Greybox.StyleSkin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaGreyboxStyleSkin::RunTest(const FString& Parameters)
{
	FArenaLayout Base;
	FString Error;
	if (!TestTrue(TEXT("layout file loads"), Base.LoadFromFile(FArenaLayout::DefaultFilePath(), Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("no -MageArenaStyle applies no style colour"), Base.StyleColours, 0);
	TestTrue(TEXT("the greybox wall is the stone"), Base.WallColour == Base.Stone);
	{
		FArenaLayout Untouched = Base;
		TestEqual(TEXT("an empty style applies nothing"), Untouched.ApplyStyle(FJsonObject()), 0);
		TestTrue(TEXT("an empty style keeps the dusk sun"), Untouched.DirectionalColour == Base.DirectionalColour);
	}

	for (const TCHAR* Id : {TEXT("13"), TEXT("25"), TEXT("30")})
	{
		FString Path = FPaths::Combine(FPaths::ProjectDir(), FString::Printf(TEXT("../data/vr/styles/style-%s.json"), Id));
		FPaths::CollapseRelativeDirectories(Path);
		FString Json;
		TSharedPtr<FJsonObject> Style;
		if (!TestTrue(*FString::Printf(TEXT("style %s loads"), Id), FFileHelper::LoadFileToString(Json, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Style) && Style.IsValid()))
		{
			continue;
		}
		auto Colour = [&Style](const TCHAR* Group, const TCHAR* Field)
		{
			const TArray<TSharedPtr<FJsonValue>>& Values = Style->GetObjectField(Group)->GetArrayField(Field);
			return FLinearColor(static_cast<float>(Values[0]->AsNumber()), static_cast<float>(Values[1]->AsNumber()),
			    static_cast<float>(Values[2]->AsNumber()));
		};
		auto Same = [](const FLinearColor& A, const FLinearColor& B)
		{ return FMath::IsNearlyEqual(A.R, B.R, 1.0e-5f) && FMath::IsNearlyEqual(A.G, B.G, 1.0e-5f) && FMath::IsNearlyEqual(A.B, B.B, 1.0e-5f); };

		FArenaLayout Styled = Base;
		// 14 arena colours, the flame from projectiles.fire, and the two sky tones.
		TestEqual(*FString::Printf(TEXT("style %s applies 17 colours"), Id), Styled.ApplyStyle(*Style), 17);
		TestEqual(*FString::Printf(TEXT("style %s counts what it applied"), Id), Styled.StyleColours, 17);
		TestTrue(*FString::Printf(TEXT("style %s sand"), Id), Same(Styled.Sand, Colour(TEXT("arena"), TEXT("sand"))));
		TestTrue(*FString::Printf(TEXT("style %s stands"), Id), Same(Styled.Stone, Colour(TEXT("arena"), TEXT("stone"))));
		TestTrue(*FString::Printf(TEXT("style %s wall"), Id), Same(Styled.WallColour, Colour(TEXT("arena"), TEXT("wall"))));
		TestTrue(*FString::Printf(TEXT("style %s brazier"), Id), Same(Styled.Brazier, Colour(TEXT("arena"), TEXT("brazier"))));
		TestTrue(*FString::Printf(TEXT("style %s banner cloth"), Id), Same(Styled.BannerCloth, Colour(TEXT("arena"), TEXT("bannerCloth"))));
		TestTrue(*FString::Printf(TEXT("style %s flame is its fire"), Id), Same(Styled.Flame, Colour(TEXT("projectiles"), TEXT("fire"))));
		TestTrue(*FString::Printf(TEXT("style %s sky horizon"), Id), Same(Styled.SkyHorizon, Colour(TEXT("sky"), TEXT("horizon"))));
		TestTrue(*FString::Printf(TEXT("style %s sky zenith"), Id), Same(Styled.SkyZenith, Colour(TEXT("sky"), TEXT("zenith"))));
		TestTrue(*FString::Printf(TEXT("style %s upper sky between them"), Id),
		    Same(Styled.SkyUpper, (Styled.SkyHorizon + Styled.SkyZenith) * 0.5f));
		TestEqual(*FString::Printf(TEXT("style %s keeps the ward arc alpha"), Id), Styled.WardArc.A, Base.WardArc.A);
		for (const TPair<FLinearColor, FLinearColor>& Light :
		    {TPair<FLinearColor, FLinearColor>(Styled.DirectionalColour, Base.DirectionalColour), TPair<FLinearColor, FLinearColor>(Styled.SkyLightColour, Base.SkyLightColour)})
		{
			TestTrue(*FString::Printf(TEXT("style %s light %s is neutral"), Id, *Light.Key.ToString()), Light.Key.R == Light.Key.G && Light.Key.G == Light.Key.B);
			TestTrue(*FString::Printf(TEXT("style %s light %s keeps the greybox luminance"), Id, *Light.Key.ToString()),
			    FMath::IsNearlyEqual(Light.Key.GetLuminance(), Light.Value.GetLuminance(), 1.0e-4f));
		}
		// Retry 1: "only the floor tint changes". Each of the big surfaces moves off the greybox.
		struct FMoved
		{
			const TCHAR* Name;
			FLinearColor Now;
			FLinearColor Was;
		};
		const FMoved Moved[] = {
		    {TEXT("sand"), Styled.Sand, Base.Sand},
		    {TEXT("stands"), Styled.Stone, Base.Stone},
		    {TEXT("wall"), Styled.WallColour, Base.WallColour},
		    {TEXT("sky horizon"), Styled.SkyHorizon, Base.SkyHorizon},
		    {TEXT("sky upper"), Styled.SkyUpper, Base.SkyUpper},
		    {TEXT("sky zenith"), Styled.SkyZenith, Base.SkyZenith},
		    {TEXT("banner cloth"), Styled.BannerCloth, Base.BannerCloth},
		    {TEXT("brazier"), Styled.Brazier, Base.Brazier},
		};
		for (const FMoved& Surface : Moved)
		{
			TestTrue(*FString::Printf(TEXT("style %s changes the %s"), Id, Surface.Name), !Same(Surface.Now, Surface.Was));
		}
		const FLinearColor Painted[] = {Styled.Sand, Styled.Stone, Styled.WallColour, Styled.Dais, Styled.PadRing, Styled.Seat, Styled.Marker,
		    Styled.Brazier, Styled.Flame, Styled.BannerCloth, Styled.BannerPole, Styled.Hand, Styled.SkyHorizon, Styled.SkyUpper, Styled.SkyZenith};
		for (const FLinearColor& Surface : Painted)
		{
			TestFalse(*FString::Printf(TEXT("style %s surface %s is not the leave red"), Id, *Surface.ToString()), IsRedRim(Surface));
		}
	}
	return true;
}

#endif
