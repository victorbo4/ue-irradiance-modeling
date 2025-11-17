#include "IrradianceViewExtension.h"    
#include "IrradianceLog.h"

#include "SceneViewExtensionContext.h"    
#include "PostProcess/PostProcessing.h" 
#include "PostProcess/PostProcessMaterialInputs.h" 
#include "RHI.h"
#include "ScreenPass.h"

// fwd
#include "Templates/RefCounting.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"      



// ============================================================================
//		LIFE CYCLE
// ============================================================================

FIrradianceViewExtension::FIrradianceViewExtension(const FAutoRegister& AutoReg)
	: FSceneViewExtensionBase(AutoReg)
{ }


bool FIrradianceViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext&) const 
{
	return FramesUntilCapture.load() > 0;
	//return true;
}



// ============================================================================
//		CAPTURE
// ============================================================================

void FIrradianceViewExtension::ArmSingleShot(uint32 InWarmupFrames, uint32 Res)
{
	ResolutionPx.store(Res);
	FramesUntilCapture.store(InWarmupFrames); 
}


bool FIrradianceViewExtension::TryConsumeCapturedSceneRT(TRefCountPtr<IPooledRenderTarget>& OutRT, FIntPoint& OutSize)
{
	if (!CapturedSceneRT.IsValid())
		return false;

	OutRT = CapturedSceneRT;               
	OutSize = CapturedSceneSize;

	// internal reset
	CapturedSceneRT.SafeRelease();
	CapturedSceneSize = FIntPoint::ZeroValue;
	FramesUntilCapture.store(0, std::memory_order_release);
	return true;
}


void FIrradianceViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass PassId,
	const FSceneView& View,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled) 
{ 
	// before tonemap (lineal HDR) 
	if (PassId != EPostProcessingPass::ReplacingTonemapper) 
		return;

	// call CustomPostProcessing
	InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FIrradianceViewExtension::CustomPostProcessing));
}


FScreenPassTexture FIrradianceViewExtension::CustomPostProcessing(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	uint32 FramesRemaining = FramesUntilCapture.load();
	UE_LOG(LogPyrano, Verbose, TEXT("[IrradianceVE] Callback Tonemap, FramesUntilCapture = %u"), FramesRemaining);

	// waiting
	if (FramesRemaining > 1)
	{
		FramesUntilCapture.store(FramesRemaining - 1);
		return Inputs.OverrideOutput;
	}

	// capture
	if (FramesRemaining == 1)
	{
		Capture(GraphBuilder, View, Inputs, ResolutionPx);
		FramesUntilCapture.store(0);
		return Inputs.OverrideOutput;

	}
	return Inputs.OverrideOutput;

}


void FIrradianceViewExtension::Capture(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, uint32 Res)
{
	// SceneColor
	const FScreenPassTexture& SceneColor = FScreenPassTexture::CopyFromSlice(
		GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	check(SceneColor.IsValid());

	UE_LOG(LogPyrano, Display, TEXT("[IrradianceVE] *** CAPTURE SHOT ***"));


	 // === OUTPUT ===

	// output texture size
	const FIntPoint OutSize(Res, Res);

	FRDGTextureDesc OutDesc = FRDGTextureDesc::Create2D(
		OutSize,
		PF_FloatRGBA,
		FClearValueBinding::Transparent,
		TexCreate_ShaderResource | TexCreate_RenderTargetable);
	FRDGTextureRef OutTex = GraphBuilder.CreateTexture(OutDesc, TEXT("SceneColor_OutTexture"));

	const FIntRect OutRect(FIntPoint(0, 0), OutSize);
	FScreenPassViewInfo SPView(View);
	AddClearRenderTargetPass(GraphBuilder, OutTex, FLinearColor(0, 0, 0, 1)); // avoid alpha channel = 0
	AddDrawTexturePass(GraphBuilder, SPView, SceneColor.Texture, OutTex, OutRect);

	// callback has RDG_TASK_ASYNC flag, but readback = sync
	// solution --> extract persistent texture and read outside the callback

	CapturedSceneSize = OutSize;
	GraphBuilder.QueueTextureExtraction(OutTex, &CapturedSceneRT);
}
