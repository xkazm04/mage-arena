#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Greybox/CreatureImport.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/ObjectRedirector.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreatureImportCharacters, "MageArena.CreatureImport.CharactersResolve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreatureImportCharacters::RunTest(const FString& Parameters)
{
	// The committed DINOv3 assets live under /Game/Characters/<Name>/ (docs/research/CREATURE-UE-IMPORT-2026-10-24.md,
	// "Imported"), no longer in a scratch folder: the runtime paths point there, and every asset loads from them.
	TestEqual(TEXT("the runtime root is /Game/Characters"), FString(CreatureImport::CharactersRoot), FString(TEXT("/Game/Characters")));
	for (const TCHAR* Name : {TEXT("Brennic"), TEXT("CinderHound"), TEXT("MireMaw")})
	{
		const FString Folder = FString::Printf(TEXT("/Game/Characters/%s"), Name);
		TestEqual(*FString::Printf(TEXT("%s folder"), Name), CreatureImport::FolderPath(Name), Folder);
		TestEqual(*FString::Printf(TEXT("%s mesh path"), Name), CreatureImport::MeshObjectPath(Name), FString::Printf(TEXT("%s/SK_%s.SK_%s"), *Folder, Name, Name));
		const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/SK_%s.SK_%s"), *Folder, Name, Name));
		TestNotNull(*FString::Printf(TEXT("%s skeletal mesh loads from /Game/Characters"), Name), Mesh);
		TestNotNull(*FString::Printf(TEXT("%s texture loads"), Name), LoadObject<UTexture2D>(nullptr, *CreatureImport::TextureObjectPath(Name)));
		TestNotNull(*FString::Printf(TEXT("%s material loads"), Name), LoadObject<UMaterialInterface>(nullptr, *CreatureImport::MaterialObjectPath(Name)));
		if (Mesh)
		{
			const CreatureImport::FCreatureSpec* Spec = CreatureImport::FindSpec(Name);
			const double HeightM = Mesh->GetBounds().BoxExtent.Z * 2.0 / 100.0;
			TestTrue(*FString::Printf(TEXT("%s stands the researched height (%.3f m)"), Name, HeightM), Spec && FMath::Abs(HeightM - Spec->HeightM) <= 0.05 * Spec->HeightM);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreatureImportReplace, "MageArena.CreatureImport.ReplaceOnePackagePerName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreatureImportReplace::RunTest(const FString& Parameters)
{
	// T70: the mire maw was re-imported with -replace (the commandlet deletes the creature's package files, then imports
	// the same names). The delete takes only the .uasset files directly in the folder, never Source/ (the FBX and PNG of
	// record) or another file.
	const FString Scratch = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() / TEXT("ReplaceTest"));
	IFileManager::Get().DeleteDirectory(*Scratch, false, true);
	for (const TCHAR* File : {TEXT("SK_A.uasset"), TEXT("M_A.uasset"), TEXT("notes.txt"), TEXT("Source/SK_B.uasset"), TEXT("Source/A.fbx")})
	{
		FFileHelper::SaveStringToFile(TEXT("x"), *(Scratch / File));
	}
	const TArray<FString> Replaceable = CreatureImport::ReplaceablePackageFiles(Scratch);
	TestEqual(TEXT("-replace deletes the two packages in the folder itself"), Replaceable, TArray<FString>{Scratch / TEXT("M_A.uasset"), Scratch / TEXT("SK_A.uasset")});
	IFileManager::Get().DeleteDirectory(*Scratch, false, true);

	// The committed maw after the re-import: exactly one package per expected name, nothing else, no redirector.
	const CreatureImport::FCreatureSpec* Maw = CreatureImport::FindSpec(TEXT("MireMaw"));
	if (!TestNotNull(TEXT("the maw has a spec"), Maw))
	{
		return false;
	}
	const TArray<FString> Expected = CreatureImport::ExpectedAssetNames(*Maw, {TEXT("Attack"), TEXT("Death"), TEXT("Hit"), TEXT("Idle")});
	TestEqual(TEXT("nine maw assets: texture, two materials, mesh, skeleton, four clips"), Expected.Num(), 9);
	const FString Folder = CreatureImport::FolderPath(Maw->Name);
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	Registry.ScanPathsSynchronous({Folder}, true);
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*Folder), Assets, false);
	TArray<FString> Names;
	TMap<FString, int32> PackagesPerName;
	for (const FAssetData& Asset : Assets)
	{
		const FString AssetName = Asset.AssetName.ToString();
		Names.Add(AssetName);
		++PackagesPerName.FindOrAdd(AssetName);
		TestFalse(*FString::Printf(TEXT("%s is not a redirector"), *Asset.PackageName.ToString()), Asset.AssetClassPath == UObjectRedirector::StaticClass()->GetClassPathName());
		TestEqual(*FString::Printf(TEXT("%s sits in a package of its own name"), *AssetName), Asset.PackageName.ToString(), Folder / AssetName);
	}
	Names.Sort();
	TestEqual(TEXT("the maw folder holds exactly the expected assets"), Names, Expected);
	for (const TPair<FString, int32>& Pair : PackagesPerName)
	{
		TestEqual(*FString::Printf(TEXT("one package named %s"), *Pair.Key), Pair.Value, 1);
	}
	FString Dir;
	TestTrue(TEXT("the maw folder is on disk"), FPackageName::TryConvertLongPackageNameToFilename(Folder + TEXT("/"), Dir));
	TArray<FString> Files = CreatureImport::ReplaceablePackageFiles(FPaths::ConvertRelativePathToFull(Dir));
	for (FString& File : Files)
	{
		File = FPaths::GetBaseFilename(File);
	}
	TestEqual(TEXT("one .uasset file per expected name, no leftover"), Files, Expected);
	return true;
}

#endif
