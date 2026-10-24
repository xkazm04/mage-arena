#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Greybox/CreatureImport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreatureImportEmberKey, "MageArena.CreatureImport.EmberKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreatureImportEmberKey::RunTest(const FString& Parameters)
{
	// The texel classes measured on T_CinderHound_BaseColor.png (sRGB, island texels, median of each value band;
	// docs/research/CREATURE-UE-IMPORT-2026-10-24.md section 4). The key must leave the body dark and light the cracks.
	const CreatureImport::FEmberParams& Ember = CreatureImport::DefaultEmber();
	const double Body = CreatureImport::EmberMask(FLinearColor(0.239f, 0.145f, 0.110f), Ember);
	const double BodyLight = CreatureImport::EmberMask(FLinearColor(0.337f, 0.176f, 0.102f), Ember);
	const double CrackDim = CreatureImport::EmberMask(FLinearColor(0.443f, 0.216f, 0.102f), Ember);
	const double Crack = CreatureImport::EmberMask(FLinearColor(0.518f, 0.247f, 0.102f), Ember);
	const double Eye = CreatureImport::EmberMask(FLinearColor(0.847f, 0.671f, 0.235f), Ember);
	TestTrue(*FString::Printf(TEXT("the body (V 0.2 band) stays dark: %.3f"), Body), Body < 0.02);
	TestTrue(*FString::Printf(TEXT("the lit body (V 0.3 band) stays dark: %.3f"), BodyLight), BodyLight < 0.05);
	TestTrue(*FString::Printf(TEXT("a dim crack (V 0.4 band) glows: %.3f"), CrackDim), CrackDim > 0.8);
	TestTrue(*FString::Printf(TEXT("a crack (V 0.5 band) glows fully: %.3f"), Crack), Crack > 0.95);
	TestTrue(*FString::Printf(TEXT("the yellow eye glows: %.3f"), Eye), Eye > 0.8);
	// Grey, blue and a dark-red texel are not cracks.
	TestTrue(TEXT("grey is not a crack"), CreatureImport::EmberMask(FLinearColor(0.5f, 0.5f, 0.5f), Ember) < 0.01);
	TestTrue(TEXT("blue is not a crack"), CreatureImport::EmberMask(FLinearColor(0.1f, 0.3f, 0.8f), Ember) < 0.01);
	TestTrue(TEXT("a saturated magenta is not a crack"), CreatureImport::EmberMask(FLinearColor(0.8f, 0.1f, 0.5f), Ember) < 0.01);
	// The emissive is warm orange, not a threat colour's red rim, and the body tint is dark red.
	TestTrue(TEXT("the ember is warm orange"), Ember.EmberColour.R > 0.9f && Ember.EmberColour.G > 0.15f && Ember.EmberColour.G < 0.45f && Ember.EmberColour.B < 0.15f);
	TestTrue(TEXT("the body tint is dark red"), Ember.BodyTint.R > 4.0f * Ember.BodyTint.G && Ember.BodyTint.R < 0.5f);
	// The HLSL names every parameter the material feeds it.
	const FString Hlsl = CreatureImport::EmberHlsl();
	for (const TCHAR* Name : {TEXT("HueCentreDeg"), TEXT("HueWidthDeg"), TEXT("SatLow"), TEXT("SatHigh"), TEXT("ValLow"), TEXT("ValHigh"), TEXT("BodyTint"),
			 TEXT("BodyTintAmount"), TEXT("BodyGain")})
	{
		TestTrue(*FString::Printf(TEXT("the HLSL reads %s"), Name), Hlsl.Contains(Name, ESearchCase::CaseSensitive));
	}
	TestEqual(TEXT("two creatures are imported"), CreatureImport::Specs().Num(), 2);
	TestTrue(TEXT("only the hound takes the ember material"), CreatureImport::FindSpec(TEXT("CinderHound"))->bEmber && !CreatureImport::FindSpec(TEXT("Brennic"))->bEmber);
	return true;
}

#endif
