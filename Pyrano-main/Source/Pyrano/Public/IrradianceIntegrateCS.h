/*=============================================================================
    IrradianceIntegrateCS.h: Compute shaders to integrate/reduce irradiance
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"

// ------ INTEGRATE C++ SHADER IMPLEMENTATION ------
class FIrradianceIntegrateCS : public FGlobalShader
{

public:
    DECLARE_GLOBAL_SHADER(FIrradianceIntegrateCS);
    SHADER_USE_PARAMETER_STRUCT(FIrradianceIntegrateCS, FGlobalShader);

    static constexpr uint32 ThreadGroupSizeX = 8;
    static constexpr uint32 ThreadGroupSizeY = 8;

    class FUseWaveOps : SHADER_PERMUTATION_BOOL("USE_WAVE_OPS");
    using FPermutationDomain = TShaderPermutationDomain<FUseWaveOps>;

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(uint32, L)
        SHADER_PARAMETER(uint32, GroupsX)
        SHADER_PARAMETER(uint32, GroupsY)
        SHADER_PARAMETER(FVector3f, SensorN)
        SHADER_PARAMETER(float, k)
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float4>, Faces)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, PartialSums)

    END_SHADER_PARAMETER_STRUCT()

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("TGX"), ThreadGroupSizeX);
        OutEnvironment.SetDefine(TEXT("TGY"), ThreadGroupSizeY);
    }
};


// ------ REDUCE SHADER C++ IMPLEMENTATION ------
class FIrradianceReduceCS : public FGlobalShader
{

public:
    DECLARE_GLOBAL_SHADER(FIrradianceReduceCS);
    SHADER_USE_PARAMETER_STRUCT(FIrradianceReduceCS, FGlobalShader);

    static constexpr uint32 ThreadGroupSize = 256;

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(uint32, TotalPartials)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, InPartialSums)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, OutY)
    END_SHADER_PARAMETER_STRUCT()

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
    }
};


// ------ HELPERS ------
namespace IrradianceCompute
{
    // Runs the shader and returns a buffer with the result
    FRDGBufferRef ComputeIrradiance(
        FRDGBuilder& GraphBuilder,
        TRefCountPtr<IPooledRenderTarget> FaceRTs[6],
        int32 CubemapSize,
        const FVector3f& SensorNormal);

    // Extracts the result to a persistent buffer for readback
    void ComputeAndExtractIrradiance(
        FRDGBuilder& GraphBuilder,
        TRefCountPtr<IPooledRenderTarget> FaceRTs[6],
        int32 CubemapSize,
        const FVector3f& SensorNormal,
        TRefCountPtr<FRDGPooledBuffer>* OutResultBuffer);
}