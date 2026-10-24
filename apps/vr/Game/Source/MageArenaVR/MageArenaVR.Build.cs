using UnrealBuildTool;

public class MageArenaVR : ModuleRules
{
	public MageArenaVR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Latest build settings do not put this directory on the include path.
		// Headers live in Hands/ and tests include them as Hands/Foo.h.
		PrivateIncludePaths.Add(ModuleDirectory);
		// D-G4: the stress scene spawns and counts Niagara systems (Budget/).
		PrivateDependencyModuleNames.Add("Niagara");
		if (Target.bBuildEditor)
		{
			// D-G4: Tools/CreateBudgetFxCommandlet builds the NS_Budget_ systems from engine templates.
			PrivateDependencyModuleNames.Add("NiagaraEditor");
		}
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Json",
			"HeadMountedDisplay",
			"UMG",
			"Slate",
			"SlateCore"
		});
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"RHI",
			"ImageWrapper",
			// Greybox/CreatureImport finds the imported idle clip in the scratch folder.
			"AssetRegistry",
			// T26: the audio capture records the master submix (UAudioMixerBlueprintLibrary).
			"AudioMixer"
		});
		if (Target.bBuildEditor)
		{
			// T26: the SFX import commandlet (Tools/ImportSfxCommandlet) uses the editor's asset import.
			// Tools/ImportCreatureMeshCommandlet imports FBX through the same AssetTools path (UnrealEd's FbxFactory).
			PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "AssetTools" });
		}
	}
}
// touch
