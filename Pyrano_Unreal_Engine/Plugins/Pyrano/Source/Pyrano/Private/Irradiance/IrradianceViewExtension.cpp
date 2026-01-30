// IrradianceViewExtension.cpp

#include "Irradiance/IrradianceViewExtension.h"    

#include "SceneViewExtensionContext.h"    
#include "PostProcess/PostProcessMaterialInputs.h" 
#include "RHI.h"
#include "ScreenPass.h"
#include "Templates/RefCounting.h" // fwd
#include "RenderGraphUtils.h" // fwd
#include "RenderGraphBuilder.h" // fwd
#include "Logging/IrradianceLog.h"


// -----------------------------------------------------------------------------
//  Life Cycle
// -----------------------------------------------------------------------------

FIrradianceViewExtension::FIrradianceViewExtension(const FAutoRegister& AutoReg)
	: FSceneViewExtensionBase(AutoReg)
{ }


bool FIrradianceViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext&) const 
{
	return FramesUntilCapture.load() > 0;
	//return true;
}


// -----------------------------------------------------------------------------
//  Capture
// -----------------------------------------------------------------------------

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

	// Internal reset
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
	// Before tonemap (lineal HDR) 
	if (PassId != EPostProcessingPass::ReplacingTonemapper) 
		return;

	// Call CustomPostProcessing
	InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FIrradianceViewExtension::CustomPostProcessing));
}


FScreenPassTexture FIrradianceViewExtension::CustomPostProcessing(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	uint32 FramesRemaining = FramesUntilCapture.load();

	// Waiting
	if (FramesRemaining > 1)
	{
		FramesUntilCapture.store(FramesRemaining - 1);
		return Inputs.OverrideOutput;
	}

	// Capture
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

	PYRANO_VERBOSE(TEXT("[IrradianceVE] CAPTURE SHOT"));

	 // --- OUTPUT ---

	// Output texture size
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

	// Callback has RDG_TASK_ASYNC flag, but readback = sync
	// Solution --> extract persistent texture and read outside the callback

	CapturedSceneSize = OutSize;
	GraphBuilder.QueueTextureExtraction(OutTex, &CapturedSceneRT);
}
