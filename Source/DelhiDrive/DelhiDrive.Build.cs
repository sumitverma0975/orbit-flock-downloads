using UnrealBuildTool;

public class DelhiDrive : ModuleRules
{
	public DelhiDrive(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"ChaosVehicles", "PhysicsCore",
			"ProceduralMeshComponent", "Json"
		});
	}
}
