using UnrealBuildTool;

// Editor-only: custom Dataflow nodes for the cloth assets (Docs/Research/STUDY_SKIRT_SIM.md).
public class visexEditor : ModuleRules
{
	public visexEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "PhysicsCore", "Chaos", "DataflowCore", "DataflowEngine", "ChaosClothAsset"
		});
	}
}
