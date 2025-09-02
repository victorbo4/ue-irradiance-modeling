
#include "IrradianceViewExtension.h"
#include "IrradiancePass.h"
#include "RendererPrivate.h"              // FViewInfo
#include "SceneViewExtensionContext.h"    // si defines IsActiveThisFrame fuera del .h
#include "PostProcess/PostProcessing.h"   // FPostProcessingInputs

FIrradianceViewExtension::FIrradianceViewExtension(const FAutoRegister& AutoReg)
	: FSceneViewExtensionBase(AutoReg)
{
#if RHI_RAYTRACING
	IrradiancePass = MakeUnique<FIrradiancePass>();
#endif
}

bool FIrradianceViewExtension::TryConsumeReadback(FLinearColor& OutColor)
{
	return IrradiancePass ? IrradiancePass->TryConsumeReadback(OutColor) : false;
}



void FIrradianceViewExtension::PrePostProcessPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessingInputs& Inputs)
{
#if RHI_RAYTRACING
	const bool bDoNow = bAutoEveryFrame.Load() || bRunOnce.Exchange(false);
	if (!bDoNow) return;
	RunIrradiancePass_PrePostProcess(GraphBuilder, View, Inputs);
#endif
}


void FIrradianceViewExtension::RunIrradiancePass_PrePostProcess(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessingInputs& Inputs)
{
#if RHI_RAYTRACING
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);

	FSceneInterface* SceneIF = (ViewInfo.Family) ? ViewInfo.Family->Scene : nullptr;
	FScene* Scene = SceneIF ? SceneIF->GetRenderScene() : nullptr;
	if (!Scene || !IrradiancePass) return;

	IrradiancePass->RenderAndEnqueueReadback(
		GraphBuilder,
		Scene,
		ViewInfo,
		/*Resolution*/ FIntPoint(1, 1),
		/*SensorOriginWS*/ FVector3f(0, 0, 0),
		/*SensorNormalWS*/ FVector3f(0, 0, 1),
		/*MaxBounces*/ 1,
		/*TemporalSeed*/ FMath::Rand());
#endif
}