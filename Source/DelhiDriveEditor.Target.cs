using UnrealBuildTool;
using System.Collections.Generic;

public class DelhiDriveEditorTarget : TargetRules
{
	public DelhiDriveEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DelhiDrive");
	}
}
