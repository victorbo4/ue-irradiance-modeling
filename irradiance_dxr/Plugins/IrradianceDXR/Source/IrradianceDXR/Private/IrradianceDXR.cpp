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

//pyr
static TUniquePtr<FIrradiancePass> GIrradiancePass;
static FDelegateHandle GPostOpaqueHandle;
static std::atomic<bool> GRunPyranoOnce{ false };
//---

// Delegate: se llama cada frame despues de opacos
static void OnPostOpaqueRender(FPostOpaqueRenderParameters& Params)
{
#if RHI_RAYTRACING
    if (!GIrradiancePass || !GRunPyranoOnce.load()) return;
    GRunPyranoOnce.store(false);

    FRDGBuilder& GraphBuilder = *Params.GraphBuilder;

    // 1) Tomar la View del parametro y castear a FViewInfo
    const FSceneView* View = Params.View;
    if (!View) { UE_LOG(LogTemp, Warning, TEXT("[Irradiance] View nula")); return; }

    const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(*View);

    // 2) Derivar Scene desde la ViewFamily
    FSceneInterface* SceneIF = (ViewInfo.Family) ? ViewInfo.Family->Scene : nullptr;
    FScene* Scene = SceneIF ? SceneIF->GetRenderScene() : nullptr;
    if (!Scene) { UE_LOG(LogTemp, Warning, TEXT("[Irradiance] Scene nula")); return; }

    UE_LOG(LogTemp, Warning, TEXT("OnPostOpaque, a punto de Pass"));
    // 3) Lanza tu pass y encola el readback
    GIrradiancePass->RenderAndEnqueueReadback(
        GraphBuilder,
        Scene,
        ViewInfo,
        FIntPoint(1, 1),
        FVector3f(0, 0, 0),
        FVector3f(0, 0, 1),
        1,
        FMath::Rand()
    );
    // No llames a Execute(); lo hara el renderer.
#else
    /*(void)Params;*/
    unimplemented();
#endif
}


void FIrradianceDXRModule::StartupModule()
{
    /** Añade el directorio de shaders. */
    const FString ShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("IrradianceDXR"))->GetBaseDir(),
        TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/IrradianceDXR"), ShaderDir);

#if RHI_RAYTRACING
    GIrradiancePass = MakeUnique<FIrradiancePass>();

    // Registrar el Post‑Opaque delegate
    IRendererModule& RendererModule = FModuleManager::LoadModuleChecked<IRendererModule>("Renderer");
    GPostOpaqueHandle = RendererModule.RegisterPostOpaqueRenderDelegate(
        FPostOpaqueRenderDelegate::CreateStatic(&OnPostOpaqueRender));

    IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("TestPyrano"),
        TEXT("Encola el pass de Pyranometer en el proximo frame (PostOpaque)."),
        FConsoleCommandDelegate::CreateLambda([]()
            {
                if (!GIrradiancePass) { UE_LOG(LogTemp, Warning, TEXT("GIrradiancePass nulo.")); return; }
                GRunPyranoOnce.store(true);
                UE_LOG(LogTemp, Display, TEXT("[TestPyrano] Solicitado. Se ejecutará en el siguiente PostOpaque."));
            }),
        ECVF_Default
    );

    //  Comando: intenta leer el resultado del readback
    IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("TestPyranoRead"),
        TEXT("Intenta consumir el readback del Pyranometer."),
        FConsoleCommandDelegate::CreateLambda([]()
            {
                if (!GIrradiancePass) { UE_LOG(LogTemp, Warning, TEXT("GIrradiancePass nulo.")); return; }
                FLinearColor Result;
                if (GIrradiancePass->TryConsumeReadback(Result))
                {
                    UE_LOG(LogTemp, Display, TEXT("[TestPyranoRead] Resultado = %s"), *Result.ToString());
                }
                else
                {
                    UE_LOG(LogTemp, Display, TEXT("[TestPyranoRead] Aún no listo. Repite tras 1-2 frames."));
                }
            }),
        ECVF_Default
    );
#endif
}

void FIrradianceDXRModule::ShutdownModule()
{
#if RHI_RAYTRACING
    if (GPostOpaqueHandle.IsValid())
    {
        if (IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>("Renderer"))
        {
            RendererModule->RemovePostOpaqueRenderDelegate(GPostOpaqueHandle);
        }
        GPostOpaqueHandle.Reset();
    }

    /** Limpia comandos de consola si quieres ser estricto. */
    if (IConsoleManager::Get().IsNameRegistered(TEXT("TestPyrano")))
        IConsoleManager::Get().UnregisterConsoleObject(TEXT("TestPyrano"), false);
    if (IConsoleManager::Get().IsNameRegistered(TEXT("TestPyranoRead")))
        IConsoleManager::Get().UnregisterConsoleObject(TEXT("TestPyranoRead"), false);

    GIrradiancePass.Reset();
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FIrradianceDXRModule, IrradianceDXR)