using UnrealBuildTool;

public class visex : ModuleRules
{
	public visex(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore", "CinematicCamera"
		});
	}
}
