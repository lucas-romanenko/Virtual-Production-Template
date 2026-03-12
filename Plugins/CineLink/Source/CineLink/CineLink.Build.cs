using UnrealBuildTool;

public class CineLink : ModuleRules
{
    public CineLink(ReadOnlyTargetRules Target) : base(Target)
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
                "LiveLinkComponents",
                "CinematicCamera",
                "MediaIOCore"
            }
        );

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}