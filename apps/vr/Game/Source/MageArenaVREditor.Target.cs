using UnrealBuildTool;

public class MageArenaVREditorTarget : TargetRules
{
	public MageArenaVREditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MageArenaVR");
	}
}
