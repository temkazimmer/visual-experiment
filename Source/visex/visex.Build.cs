using System;
using System.Diagnostics;
using UnrealBuildTool;

public class visex : ModuleRules
{
	public visex(ReadOnlyTargetRules Target) : base(Target)
	{
		// the build number on the menu (VIS/EX-0166): commits on the checked-out branch
		PublicDefinitions.Add("VISEX_BUILD=" + CountCommits());

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// each .cpp compiles on its own: in unity batches, file-local names (anonymous namespaces)
		// from different files met whenever a new file reshuffled the batches, and broke the build
		bUseUnity = false;
		// each .cpp compiles on its own: several files keep same-named helpers in anonymous
		// namespaces, and unity batches (which shift whenever a file is added) collide on them
		bUseUnity = false;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore", "CinematicCamera",
			"EnhancedInput", "AssetRegistry", "ChaosClothAssetEngine", "RHI", "RenderCore"
		});
	}

	private int CountCommits()
	{
		try
		{
			var Info = new ProcessStartInfo("git", "rev-list --count HEAD")
			{
				WorkingDirectory = ModuleDirectory,
				RedirectStandardOutput = true,
				UseShellExecute = false,
				CreateNoWindow = true
			};
			using (Process Git = Process.Start(Info))
			{
				string Out = Git.StandardOutput.ReadToEnd().Trim();
				Git.WaitForExit();
				return int.TryParse(Out, out int N) ? N : 0;
			}
		}
		catch (Exception)
		{
			return 0;
		}
	}
}
