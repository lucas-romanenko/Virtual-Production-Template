using UnrealBuildTool;
using System.IO;

public class DobotLiveLinkEditor : ModuleRules
{
    public DobotLiveLinkEditor(ReadOnlyTargetRules Target) : base(Target)
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
                "DobotLiveLink"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "InputCore",
                "UnrealEd",
                "PropertyEditor",
                "LiveLinkEditor"
            }
        );

        // Add Runtime module's public headers to include path
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "../DobotLiveLink/Public"));
    }
}