using UnrealBuildTool;

public class MageArenaVRTarget : TargetRules
{
	public MageArenaVRTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MageArenaVR");
	}
}
