using UnrealBuildTool;

public class MageArenaVR : ModuleRules
{
	public MageArenaVR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Latest build settings do not put this directory on the include path.
		// Headers live in Hands/ and tests include them as Hands/Foo.h.
		PrivateIncludePaths.Add(ModuleDirectory);
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
			// T26: the audio capture records the master submix (UAudioMixerBlueprintLibrary).
			"AudioMixer"
		});
		if (Target.bBuildEditor)
		{
			// T26: the SFX import commandlet (Tools/ImportSfxCommandlet) uses the editor's asset import.
			PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "AssetTools" });
		}
	}
}
