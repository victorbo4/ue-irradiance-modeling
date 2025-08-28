// Closest Hit 
#pragma once


#include "CoreMinimal.h"

#if RHI_RAYTRACING

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RayTracingPayloadType.h"
#include "RayTracingShaderBindingLayout.h"
#include "SceneUniformBuffer.h"
#include "SceneRenderTargetParameters.h"



class FRayTracingIrradianceCHS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FRayTracingIrradianceCHS);


    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Params) {
        return ShouldCompileRayTracingShadersForProject(Params.Platform);
    }

    static ERayTracingPayloadType GetRayTracingPayloadType(const int32) {
        return ERayTracingPayloadType::Default; // options --> Minimal
    }

    static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Params) {
        return RayTracing::GetShaderBindingLayout(Params.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnv) {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnv);
    }

    // Esto es solo si no usas SHADER_USE_ROOT_PARAMETER_STRUCT
    FRayTracingIrradianceCHS() = default;
    FRayTracingIrradianceCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
    }

};

#endif