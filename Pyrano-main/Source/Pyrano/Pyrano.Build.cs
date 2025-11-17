// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Pyrano : ModuleRules
{
    public Pyrano(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        /*	PATHS	*/
        PublicIncludePaths.AddRange(
            new string[] {

                Path.Combine(ModuleDirectory, "Public")
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {

                Path.Combine(ModuleDirectory, "Private"),
                Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Private"),
                Path.Combine(EngineDirectory, "Source", "Runtime", "RenderCore", "Private"),
                Path.Combine(EngineDirectory, "Source", "Runtime", "RHI", "Private"),
                Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Internal")
            }
            );


        /*	DEPENDENCIES	*/
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Projects",
                "RenderCore",
                "Renderer",
                "RHI",
                "ImageWriteQueue",
				"SunPosition"
                

            }
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Renderer",
                "Slate",
                "SlateCore",
                "RHICore",
                "Renderer",
                "D3D12RHI"

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
