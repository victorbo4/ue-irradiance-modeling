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
                Path.Combine(EngineDirectory, "Source", "Runtime", "D3D12RHI", "Private")	// DX12

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
                "RHI"

            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Renderer",
                "Slate",
                "SlateCore",
                "RHICore"

            }
            );
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{	}
			);

    }
}
