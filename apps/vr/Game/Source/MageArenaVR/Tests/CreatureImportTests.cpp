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
	// The soft crack bloom (docs/research/CREATURE-CAST-UE-2026-10-24.md): the same key over a ring of taps. A crack texel
	// with the hide all round it glows a little, so the glow spreads onto the hide; the hide away from a crack stays dark.
	const FLinearColor BodyTexel(0.239f, 0.145f, 0.110f);
	const FLinearColor CrackTexel(0.518f, 0.247f, 0.102f);
	TArray<FLinearColor> Taps;
	Taps.Init(BodyTexel, 12);
	TestTrue(TEXT("the soft bloom leaves the plain hide dark"), CreatureImport::EmberSoftMask(Taps, Ember) < 0.02);
	for (int32 Tap = 0; Tap < 3; ++Tap)
	{
		Taps[Tap] = CrackTexel;
	}
	const double Edge = CreatureImport::EmberSoftMask(Taps, Ember);
	TestTrue(*FString::Printf(TEXT("hide beside a crack takes part of its glow: %.3f"), Edge), Edge > 0.2 && Edge < 0.3);
	const FString SoftHlsl = CreatureImport::EmberSoftHlsl();
	for (const TCHAR* Name : {TEXT("Tex"), TEXT("TexSampler"), TEXT("UV"), TEXT("SoftRadiusUv"), TEXT("HueCentreDeg"), TEXT("HueWidthDeg"), TEXT("SatLow"), TEXT("SatHigh"),
	         TEXT("ValLow"), TEXT("ValHigh")})
	{
		TestTrue(*FString::Printf(TEXT("the soft HLSL reads %s"), Name), SoftHlsl.Contains(Name, ESearchCase::CaseSensitive));
	}
	// The glow at 8 m keeps the cracks orange: the camera has no tonemapper, so a channel above 1 clips, and a green
	// above 0.6 (linear) turns a crack toward yellow-white. The bloom lights the hide around a crack (x (1 - mask)), so a
	// crack's core takes the crack term and the rim, and the hide beside it the bloom and the rim.
	const double Core = Ember.EmberColour.G * (Ember.EmberIntensity + Ember.RimIntensity);
	const double Halo = Ember.EmberColour.G * (Ember.SoftIntensity + Ember.RimIntensity);
	TestTrue(*FString::Printf(TEXT("a fully lit crack keeps its green under 0.6: %.3f"), Core), Core < 0.6);
	TestTrue(*FString::Printf(TEXT("the bloom beside a crack keeps its green under 0.6: %.3f"), Halo), Halo < 0.6);
	TestTrue(TEXT("the chosen glow is the soft crack bloom"), Ember.SoftIntensity > 0.0 && Ember.RimIntensity == 0.0);
	TestTrue(TEXT("the glow terms are not negative"), Ember.SoftIntensity >= 0.0 && Ember.RimIntensity >= 0.0 && Ember.SoftRadiusUv > 0.0 && Ember.RimExponent > 0.0);

	// The cast: Brennic, the hound and the mire maw (Athena 20:46Z).
	TestEqual(TEXT("three creatures are imported"), CreatureImport::Specs().Num(), 3);
	const CreatureImport::FCreatureSpec* Maw = CreatureImport::FindSpec(TEXT("MireMaw"));
	if (!TestNotNull(TEXT("the maw has a spec"), Maw))
	{
		return false;
	}
	TestTrue(TEXT("only the hound takes the ember material"),
	    CreatureImport::FindSpec(TEXT("CinderHound"))->bEmber && !CreatureImport::FindSpec(TEXT("Brennic"))->bEmber && !Maw->bEmber);
	TestEqual(TEXT("the maw is MAW-MV's height"), Maw->HeightM, 1.2509);
	TestTrue(TEXT("the maw's tongue slot takes a flat material"), Maw->FlatSlot && FString(Maw->FlatSlot) == TEXT("Tongue"));
	TestTrue(TEXT("the tongue is a dark red"), Maw->FlatColour.R > 4.0f * Maw->FlatColour.G && Maw->FlatColour.R < 0.5f && Maw->FlatColour.B < 0.1f);
	// Nobody stands on the centre line (the near-centre spawn marker's post) and no two overlap: the maw is 3.1 m across
	// when turned to face the player, Brennic 1.0 m, the hound 0.9 m.
	for (const CreatureImport::FCreatureSpec& Spec : CreatureImport::Specs())
	{
		TestTrue(*FString::Printf(TEXT("%s stands off the centre post: %.1f m"), Spec.Name, Spec.SceneLateralM), FMath::Abs(Spec.SceneLateralM) >= 1.0);
	}
	TestTrue(TEXT("the maw stands clear of Brennic"), CreatureImport::FindSpec(TEXT("Brennic"))->SceneLateralM - Maw->SceneLateralM >= 1.55 + 0.52 + 0.3);
	TestTrue(TEXT("Brennic and the hound stand apart"),
	    CreatureImport::FindSpec(TEXT("CinderHound"))->SceneLateralM - CreatureImport::FindSpec(TEXT("Brennic"))->SceneLateralM >= 2.0);
	return true;
}

#endif
