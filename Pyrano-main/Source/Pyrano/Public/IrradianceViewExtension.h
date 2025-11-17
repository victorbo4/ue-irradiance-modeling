/*=============================================================================
	IrradianceViewExtension.h 
  SceneColor compute and extraction.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"      
#include "PostProcess/PostProcessMaterial.h"

class FRDGBuilder;
struct FPostProcessMaterialInputs;
struct FScreenPassTexture;

class FIrradianceViewExtension final : public FSceneViewExtensionBase
{

public:
	// Life Cycle
	explicit FIrradianceViewExtension(const FAutoRegister& AutoReg);
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	// Post-Process
	virtual void SubscribeToPostProcessingPass(
		EPostProcessingPass PassId,
		const FSceneView& View,
		FAfterPassCallbackDelegateArray& InOutPassCallbacks,
		bool bIsPassEnabled) override;

	FScreenPassTexture CustomPostProcessing(
		FRDGBuilder& GraphBuilder, 
		const FSceneView& View, 
		const FPostProcessMaterialInputs& Inputs);

	// Public API 
	void ArmSingleShot(uint32 InWarmupFrames, uint32 Res);
	bool TryConsumeCapturedSceneRT(TRefCountPtr<IPooledRenderTarget>& OutRT, FIntPoint& OutSize);

	const FIntPoint GetCapturedSceneSize() { return CapturedSceneSize; }
	IPooledRenderTarget* GetCapturedSceneRTPtr() const { return CapturedSceneRT.GetReference(); }

private:
	// Internal
	std::atomic<uint32> FramesUntilCapture{ 0 };
	std::atomic<uint32> ResolutionPx{ 1024 };

	// Capture
	TRefCountPtr<IPooledRenderTarget> CapturedSceneRT;
	FIntPoint CapturedSceneSize = FIntPoint::ZeroValue;
	void Capture(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, uint32 Res);
};


