// IrradianceIntegrateCS.cpp

#include "IrradianceIntegrateCS.h"
#include "RenderGraphUtils.h"
#include "ShaderCompilerCore.h"
#include "RenderGraphBuilder.h"
#include "GlobalShader.h"
#include "RHIStaticStates.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RHI.h"
#include "IrradianceLog.h"


IMPLEMENT_GLOBAL_SHADER(FIrradianceIntegrateCS, "/Plugin/Pyrano/Private/IrradianceIntegrate.usf", "CS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FIrradianceReduceCS, "/Plugin/Pyrano/Private/IrradianceIntegrate.usf", "ReduceCS", SF_Compute);


static FRDGTextureRef BuildFacesArray(FRDGBuilder& GraphBuilder, TRefCountPtr<IPooledRenderTarget> InFaces[6], int32 L)
{
    check(InFaces[0].IsValid());
    const EPixelFormat FaceFmt = InFaces[0]->GetDesc().Format;
    const FIntPoint Size(L, L);

    FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
        Size, FaceFmt, FClearValueBinding::None,
        TexCreate_ShaderResource /* | UAV? */, 6, 1);

    FRDGTextureRef ArrayTex = GraphBuilder.CreateTexture(Desc, TEXT("Irr.FacesArray"));

    // Faces check
    for (int32 i = 0; i < 6; ++i)
    {
        check(InFaces[i].IsValid());
        const auto& D = InFaces[i]->GetDesc();
        check(D.Extent.X == L && D.Extent.Y == L);
        check(D.Format == InFaces[0]->GetDesc().Format);
    }

    for (int32 Face = 0; Face < 6; ++Face)
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
        check(FaceRTs[0].IsValid());

        // Faces check
        for (int32 i = 0; i < 6; ++i)
        {
            if (!FaceRTs[i].IsValid())
            {
                UE_LOG(LogPyrano, Error, TEXT("[IrradianceCompute] Face %d not valid"), i);
                return nullptr;
            }
        }

        const uint32 L = CubemapSize;

        // Calculate necessary groups (2x2 striding)
        const uint32 GroupsX = FMath::DivideAndRoundUp(L, FIrradianceIntegrateCS::ThreadGroupSizeX * 2);
        const uint32 GroupsY = FMath::DivideAndRoundUp(L, FIrradianceIntegrateCS::ThreadGroupSizeY * 2);
        const uint32 TotalPartials = GroupsX * GroupsY * 6;

        const float k = 4.0f / (L * L);

        UE_LOG(LogPyrano, Display, TEXT("[IrradianceCompute] L=%d, GroupsX=%d, GroupsY=%d, TotalPartials=%d, k=%f"),
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

        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("IrradianceIntegrate"),
            IntegrateShader,
            IntegrateParams,
            FIntVector(GroupsX, GroupsY, 6));

        UE_LOG(LogPyrano, Display, TEXT("[IrradianceCompute] Step 1 (Integrate) queued"));


       
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

        UE_LOG(LogPyrano, Display, TEXT("[IrradianceCompute] Step 2 (Reduce) queued"));

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
            UE_LOG(LogPyrano, Display, TEXT("[IrradianceCompute] Result buffer extracted"));
        }
    }
}