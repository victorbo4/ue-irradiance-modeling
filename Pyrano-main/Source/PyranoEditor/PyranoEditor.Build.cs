// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;  
public class PyranoEditor : ModuleRules
{
    public PyranoEditor(ReadOnlyTargetRules Target) : base(Target)
    {
	
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        /*	PATHS	*/
        PublicIncludePaths.AddRange(
            new string[] {

                Path.Combine(ModuleDirectory, "Public"),
				
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {

                Path.Combine(ModuleDirectory, "Private"),

                // patch bc of EditorSubsystem-->scheduler-->view extension (private includes) ---> REPARE
				Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Private"),
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

				"Slate",
				"SlateCore",
				
                // viewext patch
				"Renderer",
                "UnrealEd",

				"UMG",
				"Blutility",
				"Pyrano",
                "EditorSubsystem",
                "ApplicationCore"



            }
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Pyrano",
				"UnrealEd",
				"LevelEditor",
				"ToolMenus",
				
				"UMGEditor",
				"Projects",
                "Json",
                "JsonUtilities"

            }
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {   }
            );

    }
}
