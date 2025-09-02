
// IrradianceViewExtension.h
#pragma once
#include "SceneViewExtension.h"            // UE 5.6
#include "RenderGraphBuilder.h"            // FRDGBuilder
#include "CoreMinimal.h"
#include "IrradiancePass.h"
#include "RenderGraphUtils.h"
#include "HAL/PlatformAtomics.h" // TAtomic
#include "PostProcess/PostProcessMaterial.h"
#include "DataDrivenShaderPlatformInfo.h"

class FIrradianceViewExtension final : public FSceneViewExtensionBase
{
public:
	explicit FIrradianceViewExtension(const FAutoRegister& AutoReg);


	virtual void PrePostProcessPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessingInputs& Inputs) override;

	// --- API llamada desde el Subsystem (game thread) ---
	void RequestOneShot() { bRunOnce.Store(true); }
	bool TryConsumeReadback(FLinearColor& OutColor);


private:
	void RunIrradiancePass_PrePostProcess(FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessingInputs& Inputs);

	TUniquePtr<FIrradiancePass> IrradiancePass; // si reutilizas tu pass para readback
	TAtomic<bool> bRunOnce{ false };
	TAtomic<bool> bAutoEveryFrame{ false };
};


