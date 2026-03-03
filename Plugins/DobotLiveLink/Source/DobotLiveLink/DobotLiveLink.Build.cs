using UnrealBuildTool;

public class DobotLiveLink : ModuleRules
{
	public DobotLiveLink(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"LiveLink",
				"LiveLinkInterface",
				"Networking",
				"Sockets",
				"CinematicCamera"
			}
		);
	}
}