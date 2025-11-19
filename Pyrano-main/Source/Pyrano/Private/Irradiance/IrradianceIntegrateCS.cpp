// IrradianceIntegrateCS.cpp

#include "Irradiance/IrradianceIntegrateCS.h"

#include "RenderGraphUtils.h"
#include "ShaderCompilerCore.h"
#include "RenderGraphBuilder.h"
#include "GlobalShader.h"
#include "RHIStaticStates.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RHI.h"
#include "Logging/IrradianceLog.h"
#include "Irradiance/IrradianceCommon.h"

IMPLEMENT_GLOBAL_SHADER(FIrradianceIntegrateCS, "/Plugin/Pyrano/Private/IrradianceIntegrate.usf", "CS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FIrradianceReduceCS, "/Plugin/Pyrano/Private/IrradianceIntegrate.usf", "ReduceCS", SF_Compute);

/** Creates a 2D texture array from the six cubemap face render targets. */
static FRDGTextureRef BuildFacesArray(FRDGBuilder& GraphBuilder, TRefCountPtr<IPooledRenderTarget> InFaces[6], int32 L)
{
    if (!InFaces[0].IsValid())
    {
        PYRANO_ERR(TEXT("[Compute] Missing face 0 when building faces array"));
        return nullptr;
    }

    const EPixelFormat FaceFmt = InFaces[0]->GetDesc().Format;
    const FIntPoint Size(L, L);

    FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
        Size, FaceFmt, FClearValueBinding::None,
        TexCreate_ShaderResource /* | UAV? */, IrradianceCommon::NumFaces, 1);

    FRDGTextureRef ArrayTex = GraphBuilder.CreateTexture(Desc, TEXT("Irr.FacesArray"));

    // Faces check
    for (int32 i = 0; i < IrradianceCommon::NumFaces; ++i)
    {
        check(InFaces[i].IsValid());
        const auto& D = InFaces[i]->GetDesc();
        check(D.Extent.X == L && D.Extent.Y == L);
        check(D.Format == InFaces[0]->GetDesc().Format);
    }

    for (int32 Face = 0; Face < IrradianceCommon::NumFaces; ++Face)
    {
        check(InFaces[Face].IsValid());
        FRDGTextureRef Src = GraphBuilder.RegisterExternalTexture(
            InFaces[Face],
            *FString::Printf(TEXT("Irr.Face.%d"), Face));

        FRHICopyTextureInfo Info;
        Info.DestSliceIndex = Face;
        AddCopyTexturePass(GraphBuilder, Src, ArrayTex, Info);
    }

    return ArrayTex;
}


namespace IrradianceCompute
{
    FRDGBufferRef ComputeIrradiance(
        FRDGBuilder& GraphBuilder,
        TRefCountPtr<IPooledRenderTarget> FaceRTs[6],
        int32 CubemapSize,
        const FVector3f& SensorNormal)
    {
        // Faces check
        for (int32 i = 0; i < IrradianceCommon::NumFaces; ++i)
        {
            if (!FaceRTs[i].IsValid())
            {
                PYRANO_ERR(TEXT("[Compute] Face %d not valid"), i);
                return nullptr;
            }
        }

        const uint32 L = CubemapSize;

        // Calculate necessary groups (2x2 striding)
        const uint32 GroupsX = FMath::DivideAndRoundUp(L, FIrradianceIntegrateCS::ThreadGroupSizeX * 2);
        const uint32 GroupsY = FMath::DivideAndRoundUp(L, FIrradianceIntegrateCS::ThreadGroupSizeY * 2);
        const uint32 TotalPartials = GroupsX * GroupsY * IrradianceCommon::NumFaces;

        // Solid angle weight for each texel in a cubemap.
        // A cubemap face spans exactly 4 steradians. With L x L texels per face,
        // each texel represents:
        //     dOmega = 4.0 / (L * L)
        // This converts the discrete sum of (radiance * cosTheta) into a proper
        // approximation of the hemispherical irradiance integral.
        const float k = 4.0f / (L * L);

        PYRANO_VERBOSE(TEXT("[Compute] L=%d, GroupsX=%d, GroupsY=%d, TotalPartials=%d, k=%f"),
            L, GroupsX, GroupsY, TotalPartials, k);
        
        // ------- STEP 1: INTEGRATE - Calculate subtotals by group -------

        // Build array textures
        FRDGTextureRef FacesArray = BuildFacesArray(GraphBuilder, FaceRTs, L);
        FRDGTextureSRVRef FacesSRV = GraphBuilder.CreateSRV(
            FRDGTextureSRVDesc::Create(FacesArray));

        // Subtotals buffer
        FRDGBufferRef PartialSumsBuffer = GraphBuilder.CreateBuffer(
            FRDGBufferDesc::CreateStructuredDesc(sizeof(float), TotalPartials),
            TEXT("IrradiancePartialSums"));

        // Determine whether to use wave-ops or not
        const EShaderPlatform Platform = GMaxRHIShaderPlatform;
        const bool bSupportsWaveOps = RHISupportsWaveOperations(Platform);  

        FIrradianceIntegrateCS::FPermutationDomain PermutationVector;
        PermutationVector.Set<FIrradianceIntegrateCS::FUseWaveOps>(bSupportsWaveOps);

        TShaderMapRef<FIrradianceIntegrateCS> IntegrateShader(
            GetGlobalShaderMap(GMaxRHIFeatureLevel),
            PermutationVector);

        // Set shader parameters
        FIrradianceIntegrateCS::FParameters* IntegrateParams =
            GraphBuilder.AllocParameters<FIrradianceIntegrateCS::FParameters>();
        IntegrateParams->L = L;
        IntegrateParams->GroupsX = GroupsX;
        IntegrateParams->GroupsY = GroupsY;
        IntegrateParams->SensorN = SensorNormal;
        IntegrateParams->k = k;
        IntegrateParams->Faces = FacesSRV;
        IntegrateParams->PartialSums = GraphBuilder.CreateUAV(PartialSumsBuffer);

        // Queue RDG Pass
        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("IrradianceIntegrate"),
            IntegrateShader,
            IntegrateParams,
            FIntVector(GroupsX, GroupsY, IrradianceCommon::NumFaces));

        PYRANO_VERBOSE(TEXT("[Compute] Step 1 (Integrate) queued"));

        // ------- STEP 2: REDUCE --------

        FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
            FRDGBufferDesc::CreateStructuredDesc(sizeof(float), 1),
            TEXT("IrradianceOutput"));

        TShaderMapRef<FIrradianceReduceCS> ReduceShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

        FIrradianceReduceCS::FParameters* ReduceParams =
            GraphBuilder.AllocParameters<FIrradianceReduceCS::FParameters>();
        ReduceParams->TotalPartials = TotalPartials;
        ReduceParams->InPartialSums = GraphBuilder.CreateSRV(PartialSumsBuffer);
        ReduceParams->OutY = GraphBuilder.CreateUAV(OutputBuffer);

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("IrradianceReduce"),
            ReduceShader,
            ReduceParams,
            FIntVector(1, 1, 1));

        PYRANO_VERBOSE(TEXT("[IrradianceCompute] Step 2 (Reduce) queued"));

        return OutputBuffer;
    }


    void ComputeAndExtractIrradiance(
        FRDGBuilder& GraphBuilder,
        TRefCountPtr<IPooledRenderTarget> FaceRTs[6],
        int32 CubemapSize,
        const FVector3f& SensorNormal,
        TRefCountPtr<FRDGPooledBuffer>* OutResultBuffer)
    {
        // Run shader
        FRDGBufferRef ResultBuffer = ComputeIrradiance(
            GraphBuilder,
            FaceRTs,
            CubemapSize,
            SensorNormal);

        // Extract buffer
        if (ResultBuffer)
        {
            GraphBuilder.QueueBufferExtraction(ResultBuffer, OutResultBuffer);
            PYRANO_VERBOSE(TEXT("[Compute] Result buffer extracted"));
        }
    }
}