// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class IrradianceDXR : ModuleRules
{
	public IrradianceDXR(ReadOnlyTargetRules Target) : base(Target)
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
                "RHI"

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
			{	}
			);

        bEnableUndefinedIdentifierWarnings = false; // deprecated?
        bUseUnity = true;
        bEnforceIWYU = false; // deprecated?

        // asegura de activar RT
        PublicDefinitions.Add("RHI_RAYTRACING=1");

    }
}
