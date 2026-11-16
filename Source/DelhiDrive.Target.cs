using UnrealBuildTool;
using System.Collections.Generic;

public class DelhiDriveTarget : TargetRules
{
	public DelhiDriveTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DelhiDrive");
	}
}
