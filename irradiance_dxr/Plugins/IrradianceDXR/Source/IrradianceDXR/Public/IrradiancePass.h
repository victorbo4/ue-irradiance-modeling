#pragma once
#include "RHIGPUReadback.h"

class FIrradiancePass
{
public:
    FIrradiancePass();
    ~FIrradiancePass();

    static FRDGTextureRef AddPass_Render(
        FRDGBuilder& GraphBuilder,
        FScene* Scene,
        const FViewInfo& View,
        FIntPoint Resolution,
        FVector3f SensorOriginWS,
        FVector3f SensorNormalWS,
        uint32 MaxBounces,
        uint32 TemporalSeed);

    void EnqueuePassAndReadback(
        FRDGBuilder& GraphBuilder,
        FRDGTextureRef OutTex /* el que produces en AddPass_Render */);

    void RenderAndEnqueueReadback(
        FRDGBuilder& GraphBuilder,
        FScene* Scene,
        const FViewInfo& View,
        FIntPoint Resolution,
        const FVector3f& SensorOriginWS,
        const FVector3f& SensorNormalWS,
        uint32 MaxBounces,
        uint32 TemporalSeed);

    bool TryConsumeReadback(FLinearColor&);

private:
    TUniquePtr<FRHIGPUTextureReadback> TextureReadback;
    FLinearColor LastSample = FLinearColor::Black;
    bool bHasNewSample = false;
    
};

