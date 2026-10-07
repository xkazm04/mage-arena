#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Greybox/ArenaLayout.h"
#include "Hands/MageArenaPawn.h"

namespace
{
// A pawn with no world, the way GreyboxTests makes one. The caller removes it from the root.
AMageArenaPawn* MakeVignettePawn(FAutomationTestBase& Test, FArenaLayout& Layout)
{
	FString Error;
	if (!Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error))
	{
		Test.AddError(Error);
		return nullptr;
	}
	AMageArenaPawn* Pawn = NewObject<AMageArenaPawn>(GetTransientPackage());
	if (!Test.TestNotNull(TEXT("pawn constructs"), Pawn))
	{
		return nullptr;
	}
	Pawn->AddToRoot();
	Pawn->ConfigureFromLayout(Layout);
	return Pawn;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBlinkVignetteMapping, "MageArena.Hands.BlinkVignette.Mapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBlinkVignetteMapping::RunTest(const FString& Parameters)
{
	constexpr double Peak = 1.15;
	const AMageArenaPawn::FVignetteAperture Open = AMageArenaPawn::VignetteAperture(0.0, Peak);
	TestEqual(TEXT("intensity 0 has no opacity"), Open.Opacity, 0.0);
	const AMageArenaPawn::FVignetteAperture Full = AMageArenaPawn::VignetteAperture(Peak, Peak);
	TestTrue(TEXT("the peak has opacity"), Full.Opacity > 0.0);
	TestTrue(TEXT("the peak is the most opaque"), Full.Opacity >= Open.Opacity);
	TestTrue(TEXT("the peak narrows the opening"), Full.InnerHalfAngleDeg < Open.InnerHalfAngleDeg);

	AMageArenaPawn::FVignetteAperture Previous = Open;
	for (int32 Step = 1; Step <= 11; ++Step)
	{
		const double Intensity = Peak * static_cast<double>(Step) / 11.0;
		const AMageArenaPawn::FVignetteAperture Now = AMageArenaPawn::VignetteAperture(Intensity, Peak);
		TestTrue(*FString::Printf(TEXT("step %d opacity does not fall"), Step), Now.Opacity >= Previous.Opacity);
		TestTrue(*FString::Printf(TEXT("step %d opening does not widen"), Step), Now.InnerHalfAngleDeg <= Previous.InnerHalfAngleDeg);
		TestTrue(*FString::Printf(TEXT("step %d opacity rises"), Step), Now.Opacity > Previous.Opacity);
		Previous = Now;
	}

	const AMageArenaPawn::FVignetteAperture Over = AMageArenaPawn::VignetteAperture(Peak * 3.0, Peak);
	TestEqual(TEXT("above the peak the opacity clamps"), Over.Opacity, Full.Opacity);
	TestEqual(TEXT("above the peak the opening clamps"), Over.InnerHalfAngleDeg, Full.InnerHalfAngleDeg);
	TestEqual(TEXT("below zero is open"), AMageArenaPawn::VignetteAperture(-1.0, Peak).Opacity, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBlinkVignetteComponent, "MageArena.Hands.BlinkVignette.Component",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBlinkVignetteComponent::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	AMageArenaPawn* Pawn = MakeVignettePawn(*this, Layout);
	if (!Pawn)
	{
		return false;
	}
	const double Peak = Layout.BlinkVignettePeak;
	TestFalse(TEXT("idle pawn draws no vignette"), Pawn->IsVignetteVisible());

	Pawn->SetVignette(0.0);
	TestFalse(TEXT("zero is hidden"), Pawn->IsVignetteVisible());

	Pawn->SetVignette(Peak * 0.5);
	TestTrue(TEXT("half the peak is visible"), Pawn->IsVignetteVisible());
	const double Want = AMageArenaPawn::VignetteAperture(Peak * 0.5, Peak).Opacity;
	TestTrue(TEXT("the material holds the mapped opacity"), FMath::IsNearlyEqual(Pawn->GetVignetteOpacity(), Want, 1.0e-4));

	Pawn->SetVignette(0.0);
	TestFalse(TEXT("back at zero it is hidden again"), Pawn->IsVignetteVisible());

	Pawn->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBlinkVignetteNoPostProcess, "MageArena.Hands.BlinkVignette.NoPostProcess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBlinkVignetteNoPostProcess::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	AMageArenaPawn* Pawn = MakeVignettePawn(*this, Layout);
	if (!Pawn)
	{
		return false;
	}
	const UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
	if (!TestNotNull(TEXT("pawn has a camera"), Camera))
	{
		Pawn->RemoveFromRoot();
		return false;
	}
	const double Peak = Layout.BlinkVignettePeak;
	for (int32 Step = 0; Step <= 11; ++Step)
	{
		Pawn->SetVignette(Peak * static_cast<double>(Step) / 11.0);
	}
	for (int32 Step = 11; Step >= 0; --Step)
	{
		Pawn->SetVignette(Peak * static_cast<double>(Step) / 11.0);
	}
	TestFalse(TEXT("a full ramp leaves the camera vignette override off"), Camera->PostProcessSettings.bOverride_VignetteIntensity);
	TestEqual(TEXT("and the camera vignette intensity untouched"), Camera->PostProcessSettings.VignetteIntensity, FPostProcessSettings().VignetteIntensity);

	Pawn->RemoveFromRoot();
	return true;
}

#endif
