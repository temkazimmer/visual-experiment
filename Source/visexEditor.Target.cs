using UnrealBuildTool;

public class visexEditorTarget : TargetRules
{
	public visexEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("visex");
		ExtraModuleNames.Add("visexEditor");   // editor-only: cloth Dataflow nodes
	}
}
