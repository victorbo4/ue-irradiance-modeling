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
            new string[] 
            {
                // Empty
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
            }
            );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",

                "UMG",
                "Blutility",
                "EditorSubsystem",
                "ApplicationCore",
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
