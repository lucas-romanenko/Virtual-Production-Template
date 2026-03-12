using UnrealBuildTool;
using System.IO;

public class CineLinkEditor : ModuleRules
{
    public CineLinkEditor(ReadOnlyTargetRules Target) : base(Target)
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
                "CineLink"
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
                "LiveLinkEditor",
                "WorkspaceMenuStructure",
                "EditorStyle"
            }
        );

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "../CineLink/Public"));
    }
}