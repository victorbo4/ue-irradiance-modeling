/*
*  IrradianceDXR.cpp: de momento se implementa aquí tanto los comandos de consola como el Delegate para el Pase RDG.
*  En versiones futuras se tiene que refactorizar y separar consola y delegate en render thread, del game thread.
* 
*/

#include "IrradianceDXR.h"
#include "Interfaces/IPluginManager.h"   
#include "Misc/Paths.h"
#include "ShaderCore.h"                  
#include "HAL/IConsoleManager.h"
//#include "PathTracing.h"


//pyr
#include "IrradiancePass.h"
#include "RendererInterface.h"
#include "RendererPrivate.h"          
#include "RenderGraphBuilder.h"
#include "RenderGraph.h"
#include "RHICommandList.h"
//---
#define LOCTEXT_NAMESPACE "FIrradianceDXRModule"



void FIrradianceDXRModule::StartupModule()
{
    /** Añade el directorio de shaders. */
    const FString ShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("IrradianceDXR"))->GetBaseDir(),
        TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/IrradianceDXR"), ShaderDir);

}

void FIrradianceDXRModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FIrradianceDXRModule, IrradianceDXR)