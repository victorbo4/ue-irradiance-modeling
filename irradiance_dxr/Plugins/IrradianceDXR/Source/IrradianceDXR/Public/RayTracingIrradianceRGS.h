// RayGen
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



class FRayTracingIrradianceRGS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRayTracingIrradianceRGS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingIrradianceRGS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutRadiance)
	END_SHADER_PARAMETER_STRUCT()


	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Params) {
		return ShouldCompileRayTracingShadersForProject(Params.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 /*PermutationId*/) {
		return ERayTracingPayloadType::Default; // options --> Minimal
	}

	static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Params) {
		return RayTracing::GetShaderBindingLayout(Params.Platform);
	}

	//static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnv){
	//	FGlobalShader::ModifyCompilationEnvironment(Params, OutEnv);
	//	//OutEnv.ShaderBindingLayout = RayTracing::GetShaderBindingLayout(Params.Platform);
	//	//OutEnv.CompilerFlags.Add(CFLAG_HLSL2021);
	//	/*OutEnv.SetDefine(TEXT("PATH_TRACING"), 1);*/
	//}



};

#endif