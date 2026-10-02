using UnrealBuildTool;

public class MageArenaVR : ModuleRules
{
	public MageArenaVR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
