using UnrealBuildTool;

// Editor-only helpers for Tools/*.py: things the 5.7 Python API can't do (AnimGraph nodes).
public class visexTools : ModuleRules
{
	public visexTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "UnrealEd", "BlueprintGraph", "AnimGraph"
		});
	}
}
