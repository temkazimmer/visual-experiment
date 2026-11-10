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
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore", "CinematicCamera",
			"EnhancedInput", "AssetRegistry", "ChaosClothAssetEngine"
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
