// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pyrano.h"
#include "Interfaces/IPluginManager.h"   
#include "Misc/Paths.h"
#include "ShaderCore.h"                  
#include "HAL/IConsoleManager.h"


#include "RendererInterface.h"
#include "RendererPrivate.h"          
#include "RenderGraphBuilder.h"
#include "RenderGraph.h"
#include "RHICommandList.h"


#define LOCTEXT_NAMESPACE "FPyranoModule"

void FPyranoModule::StartupModule()
{
    
    const FString ShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("Pyrano"))->GetBaseDir(),
        TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/Pyrano"), ShaderDir);


}

void FPyranoModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPyranoModule, Pyrano)