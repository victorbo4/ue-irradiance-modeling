// Pyrano.Build.cs

using System.IO;
using UnrealBuildTool;

public class Pyrano : ModuleRules
{
    public Pyrano(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        /*	PATHS	*/
        PublicIncludePaths.AddRange(
            new string[] 
			{
                Path.Combine(ModuleDirectory, "Public")
            }
            );

        PrivateIncludePaths.AddRange(
            new string[] 
			{
                Path.Combine(ModuleDirectory, "Private"),   
			}
            );

        /*	DEPENDENCIES	*/
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",

                "RHI",
                "Renderer",
                "RenderCore",
            }
            );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",
                "Slate",
                "SlateCore",
                "ImageWriteQueue",
            }
            );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {   }
            );

        bEnableUndefinedIdentifierWarnings = false; // deprecated?
        bEnforceIWYU = false; // deprecated?
        bUseUnity = true;

        PublicDefinitions.Add("RHI_RAYTRACING=1");
    }
}
