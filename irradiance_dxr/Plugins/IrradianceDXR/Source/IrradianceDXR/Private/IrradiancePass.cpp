/*=============================================================================
	IrradiancePass.cpp
=============================================================================*/
/*
*  Falta refactorizar y aislar RDG de DXR (e incluso estructuras de PSO, SBT, Dispatch)
*/

//-- Ray Tracing --------------------------------------------------------------
#include "IrradiancePass.h"
#include "RayTracingIrradianceCHS.h"
#include "RayTracingIrradianceRGS.h"
#include "RayTracingPayloadType.h"
#include "RayTracingDefinitions.h"
#include "RayTracingTypes.h"
#include "RayTracing/RayTracingShaderBindingTable.h"
#include "RayTracingShaderBindingLayout.h"
#include "Nanite/NaniteRayTracing.h"
#include "BuiltInRayTracingShaders.h"

//-- RDG ----------------------------------------------------------------------
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderGraphEvent.h"

//-- RHI ----------------------------------------------------------------------
#include "RHIResources.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"

//-- Scene --------------------------------------------------------------------
#include "ScenePrivate.h"
#include "SceneRenderTargetParameters.h"

//-- Pipeline -----------------------------------------------------------------
#include "PipelineStateCache.h"

//-- Shaders ------------------------------------------------------------------
#include "GlobalShader.h"
#include "RHIShaderPlatform.h"
#include "DataDrivenShaderPlatformInfo.h"


DEFINE_LOG_CATEGORY_STATIC(LogIrradiance, Log, All);


FIrradiancePass::FIrradiancePass() {}
FIrradiancePass::~FIrradiancePass() { TextureReadback.Reset(); }

/** Pase del AddPass */
struct FIrradianceRTDispatch
{
	FRayTracingIrradianceRGS::FParameters* Params = nullptr;
	FRHIRayTracingShader* RayGen = nullptr;
	FRHIRayTracingShader* ClosestHit = nullptr;
	FRHIUniformBuffer* SceneUB = nullptr;
	FRHIUniformBuffer* NaniteUB = nullptr;
	FIntPoint Resolution = FIntPoint::ZeroValue;
};


struct FRTBuiltPipeline
{
	FRayTracingPipelineState* Pipeline = nullptr;
	FShaderBindingTableRHIRef  SBT;
};

/** Construye PSO y SBT */
static FRTBuiltPipeline BuildRTPSOAndSBT(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	FRHIRayTracingShader* RayGen,
	FRHIRayTracingShader* ClosestHit,
	FScene* Scene)
{
	// Shader tables
	FRHIRayTracingShader* RayGenTable[] = { RayGen };
	FRHIRayTracingShader* HitGroupTable[] = { ClosestHit };
	FRHIRayTracingShader* MissTable[] = { View.ShaderMap->GetShader<FDefaultPayloadMS>().GetRayTracingShader() }; // miss default del UE

	FRayTracingPipelineStateInitializer Init;
	Init.MaxPayloadSizeInBytes = GetRayTracingPayloadTypeMaxSize(ERayTracingPayloadType::Default); // OJO mismo payload que shaders

	if (const FShaderBindingLayout* Layout = RayTracing::GetShaderBindingLayout(View.GetShaderPlatform())){
		Init.ShaderBindingLayout = &Layout->RHILayout;
	}

	Init.SetRayGenShaderTable(RayGenTable);
	Init.SetHitGroupTable(HitGroupTable);
	Init.SetMissShaderTable(MissTable);

	// Builds PSO + SBT
	FRTBuiltPipeline Out;
	Out.Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Init); 
	Out.SBT = Scene->RayTracingSBT.AllocateTransientRHI( // AllocateRHI deprecated in 5.6.1
		RHICmdList,
		ERayTracingShaderBindingMode::RTPSO,
		ERayTracingHitGroupIndexingMode::Disallow,
		Init.GetMaxLocalBindingDataSize()); 

	return Out;
}

/** Añade RDG Pass con dispatch de rayos */
FRDGTextureRef FIrradiancePass::AddPass_Render(
	FRDGBuilder& GraphBuilder,
	FScene* Scene,
	const FViewInfo& View,
	FIntPoint Resolution,
	FVector3f SensorOriginWS,
	FVector3f SensorNormalWS,
	uint32 MaxBounces,
	uint32 TemporalSeed)
{

#if RHI_RAYTRACING


	// Creates output texture
	FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
		Resolution,
		PF_FloatRGBA,                          
		FClearValueBinding::Black,
		TexCreate_UAV | TexCreate_ShaderResource);
	FRDGTextureRef OutTex = GraphBuilder.CreateTexture(Desc, TEXT("Pyranometer.Out"));


	// Get shaders
	TShaderRef<FRayTracingIrradianceRGS> RayGen = View.ShaderMap->GetShader<FRayTracingIrradianceRGS>();
	TShaderRef<FRayTracingIrradianceCHS> ClosestHit = View.ShaderMap->GetShader<FRayTracingIrradianceCHS>();


	// Parameter binding
	FRayTracingIrradianceRGS::FParameters* Params = GraphBuilder.AllocParameters<FRayTracingIrradianceRGS::FParameters>();
	Params->OutRadiance = GraphBuilder.CreateUAV(OutTex);
	Params->TLAS = Scene->RayTracingScene.GetLayerView(ERayTracingSceneLayer::Base);
	Params->View = View.ViewUniformBuffer;
	Params->SceneTextures = CreateSceneTextureShaderParameters(
			GraphBuilder, View, ESceneTextureSetupMode::All);

	// Uniform buffers
	FRHIUniformBuffer* SceneUniformBuffer = GetSceneUniformBufferRef(GraphBuilder, View)->GetRHI();
	FRHIUniformBuffer* NaniteUB = Nanite::GetPublicGlobalRayTracingUniformBuffer()->GetRHI(); // necesario en UE 5.6.1

	// AddPass Dispatch
	FIrradianceRTDispatch* Dispatch = GraphBuilder.AllocObject<FIrradianceRTDispatch>();
	Dispatch->Params = Params;
	Dispatch->RayGen = RayGen.GetRayTracingShader();
	Dispatch->ClosestHit = ClosestHit.GetRayTracingShader();
	Dispatch->SceneUB = SceneUniformBuffer;   
	Dispatch->NaniteUB = NaniteUB;             
	Dispatch->Resolution = Resolution;

	/* RDG PASS */
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("RayTracingIrradianceRGS"),
		Params,
		ERDGPassFlags::Compute,
		[Dispatch, Scene, &View]
		(FRDGAsyncTask, FRHICommandList& RHICmdList)
		{

			FRTBuiltPipeline Built = BuildRTPSOAndSBT(RHICmdList, View, Dispatch->RayGen, Dispatch->ClosestHit, Scene);

			FRHIBatchedShaderParameters& Globals = RHICmdList.GetScratchShaderParameters(); // necesario en 5.6.1; non-batched parameters planned for deprecation
			SetShaderParameters(Globals, View.ShaderMap->GetShader<FRayTracingIrradianceRGS>(), *Dispatch->Params);

			// Binding
			TOptional<FScopedUniformBufferStaticBindings> StaticUniformBufferScope =
				RayTracing::BindStaticUniformBufferBindings(View, Dispatch->SceneUB, Dispatch->NaniteUB, RHICmdList); // w/o nanite -> error

			// Dispatch
			RHICmdList.SetDefaultRayTracingHitGroup(Built.SBT, Built.Pipeline, 0);
			RHICmdList.SetRayTracingMissShader(Built.SBT, 0, Built.Pipeline, 0, 0, nullptr, 0);
			RHICmdList.CommitShaderBindingTable(Built.SBT);
			RHICmdList.RayTraceDispatch(
				Built.Pipeline,
				Dispatch->RayGen,
				Built.SBT,
				Globals,
				Dispatch->Resolution.X,
				Dispatch->Resolution.Y); // API 5.6

		});

	return OutTex;

#else // !RHI_RAYTRACING
	{
		unimplemented();
		return nullptr;
	}
#endif
}


/** ReadBack */
void FIrradiancePass::EnqueuePassAndReadback(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef OutTex)
{
	// Lee solo 1x1: si es mayor, ajustar Rect
	const FResolveRect Rect(0, 0, 1, 1); // x1,y1 = ancho/alto absolutos

	AddReadbackTexturePass(
		GraphBuilder, 
		RDG_EVENT_NAME("Pyranometer_Readback"), 
		OutTex,                                    
		[this, OutTex, Rect](FRHICommandListImmediate& RHICmdList)
		{
			if (!TextureReadback.IsValid()){
				TextureReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("PyranometerReadback"));
			}

			TextureReadback->EnqueueCopy(RHICmdList, OutTex->GetRHI(), Rect);
		});
}


/** AddPass + Readback */
void FIrradiancePass::RenderAndEnqueueReadback(
	FRDGBuilder& GraphBuilder,
	FScene* Scene,
	const FViewInfo& View,
	FIntPoint Resolution,
	const FVector3f& SensorOriginWS,
	const FVector3f& SensorNormalWS,
	uint32 MaxBounces,
	uint32 TemporalSeed)
{
#if RHI_RAYTRACING

	FRDGTextureRef Out = AddPass_Render(
		GraphBuilder, Scene, View, Resolution,
		SensorOriginWS, SensorNormalWS, MaxBounces, TemporalSeed);

	if (Out){
		EnqueuePassAndReadback(GraphBuilder, Out);
	}

#else
	/*(void)GraphBuilder; (void)Scene; (void)View; (void)Resolution;
	(void)SensorOriginWS; (void)SensorNormalWS; (void)MaxBounces; (void)TemporalSeed;*/
	unimplemented();
#endif
}


/** Consume Readback en CPU y devuelve log */
bool FIrradiancePass::TryConsumeReadback(FLinearColor& OutPixelRGBA)
{

	if (!TextureReadback){ return false; }
	if (!TextureReadback->IsReady()){ return false; }

	uint32 RowPitch = 0;
	void* DataPtr = TextureReadback->Lock(RowPitch);
	if (!DataPtr)
	{
		UE_LOG(LogIrradiance, Warning, TEXT("Readback Lock devolvio puntero nulo."));
		return false;
	}

	const FFloat16Color* Pixel16 = reinterpret_cast<const FFloat16Color*>(DataPtr);
	OutPixelRGBA = FLinearColor(
		Pixel16->R,
		Pixel16->G,
		Pixel16->B,
		Pixel16->A
	);

	TextureReadback->Unlock();

	// Log de validacion rapida: deberia ser (1,0,0,1)
	UE_LOG(LogIrradiance, Display, TEXT("Pyranometer Readback 1x1 = R:%f G:%f B:%f A:%f"),
		OutPixelRGBA.R, OutPixelRGBA.G, OutPixelRGBA.B, OutPixelRGBA.A);

	return true;

}


