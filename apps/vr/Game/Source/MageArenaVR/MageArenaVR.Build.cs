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
			"HeadMountedDisplay"
		});
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"RHI",
			"ImageWrapper"
		});
	}
}
