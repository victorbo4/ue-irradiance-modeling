/*=============================================================================
	IrradianceViewExtension.h 
  SceneColor compute and extraction.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"      
//#include "PostProcess/PostProcessMaterial.h"

class FRDGBuilder;
struct FPostProcessMaterialInputs;
struct FScreenPassTexture;

class FIrradianceViewExtension final : public FSceneViewExtensionBase
{

public:

// --- Life Cycle ---

	explicit FIrradianceViewExtension(const FAutoRegister& AutoReg);
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

// --- Post-Process ---

	/** Register callback for executing work during a specific post-processing pass. */
	virtual void SubscribeToPostProcessingPass(
		EPostProcessingPass PassId,
		const FSceneView& View,
		FAfterPassCallbackDelegateArray& InOutPassCallbacks,
		bool bIsPassEnabled) override;

	/** Custom post-process step: captures SceneColor for this frame. */
	FScreenPassTexture CustomPostProcessing(
		FRDGBuilder& GraphBuilder, 
		const FSceneView& View, 
		const FPostProcessMaterialInputs& Inputs);

// --- Public API ---

	/** Schedule a one-shot capture after a warmup period. */
	void ArmSingleShot(uint32 InWarmupFrames, uint32 Res);

	/** Retrieve the captured SceneColor RT if available (one-time consume). */
	bool TryConsumeCapturedSceneRT(TRefCountPtr<IPooledRenderTarget>& OutRT, FIntPoint& OutSize);

	/** Size of the last captured SceneColor. */
	const FIntPoint GetCapturedSceneSize() { return CapturedSceneSize; }

	/** Raw pointer to the last captured RT (may be null). */
	IPooledRenderTarget* GetCapturedSceneRTPtr() const { return CapturedSceneRT.GetReference(); }

private:

// --- Internal ---

	/** Frames left before capture triggers. */
	std::atomic<uint32> FramesUntilCapture{ 0 };

	/** Desired capture resolution. */
	std::atomic<uint32> ResolutionPx{ 1024 };

// --- Capture ---

	/** Stored captured render target. */
	TRefCountPtr<IPooledRenderTarget> CapturedSceneRT;

	/** Resolution of CapturedSceneRT. */
	FIntPoint CapturedSceneSize = FIntPoint::ZeroValue;

	/** Copy SceneColor into CapturedSceneRT at the given resolution. */
	void Capture(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, uint32 Res);
};


