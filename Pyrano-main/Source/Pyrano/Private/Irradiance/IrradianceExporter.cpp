// IrradianceExporter.cpp

#include "Irradiance/IrradianceExporter.h"

#include "RHI.h"
#include "RHIGPUReadback.h"
#include "RenderGraphResources.h"
#include "RHICommandList.h"
#include "RenderTargetPool.h"

#include "Simulation/CaptureRequest.h" 

#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "ImagePixelData.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// -----------------------------------------------------------------------------
//  Init
// -----------------------------------------------------------------------------

void UIrradianceExporter::Init(const FExportOptions& InOpts)
{
    // Base dir
    FString Base = InOpts.OutputDir.Path;
    if (Base.IsEmpty())
        Base = FPaths::ProjectSavedDir() / TEXT("Irradiance");

    IFileManager::Get().MakeDirectory(*Base, true);

    // CSV
    const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
    const FString CSVName = FPaths::GetBaseFilename(InOpts.CSVFilename) + TEXT("_") + Stamp + TEXT(".csv");
    CSVPathAbs = FPaths::Combine(Base, CSVName);

    // Images
    ImagesPathAbs = FPaths::Combine(Base, InOpts.ImagesSubdir);
    IFileManager::Get().MakeDirectory(*ImagesPathAbs, true);
}


// -----------------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------------

void UIrradianceExporter::AppendIrradianceRow(const FCaptureRequest& Req, float Irr, float Azimuth, float Altitude)
{
    EnsureCSVHeader();

    const FString Line = FString::Printf(
        TEXT("%s,%s,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%u,%.3f,%.3f,%.9f\n"),
        *Req.SensorName,                   
        *Req.SensorId.ToString(),
        *Req.TimestampUTC.ToIso8601(),
        Req.PosWS.X, 
        Req.PosWS.Y, 
        Req.PosWS.Z,
        Req.NormalWS.X, 
        Req.NormalWS.Y, 
        Req.NormalWS.Z,
        Req.SidePx, 
        Req.WarmupFrames, 
        Azimuth,
        Altitude,
        Irr);

    FFileHelper::SaveStringToFile(Line, *CSVPathAbs, FFileHelper::EEncodingOptions::AutoDetect,
        &IFileManager::Get(), FILEWRITE_Append);
}


void UIrradianceExporter::FlushCSVIfNeeded(bool /*bForce*/)
{

    // FFileHelper::Save... already does immediate append
    // I keep this method just in case later I wanna buffer and flush multiple rows

}


void UIrradianceExporter::EnqueueFaceEXR(const FCaptureRequest& Req, int32 FaceIdx,
    TRefCountPtr<IPooledRenderTarget> FaceRT, FIntPoint Size)
{
    if (!FaceRT.IsValid()) 
        return;

    // Little sync readback -> make RHI texture and read
    ENQUEUE_RENDER_COMMAND(Pyrano_ReadFaceToEXR)(
        [this, Req, FaceIdx, FaceRT, Size](FRHICommandListImmediate& RHICmdList)
        {
            FRHITexture* Tex = FaceRT->GetRHI();
            if (!Tex) 
                return;

            const int32 W = Size.X;
            const int32 H = Size.Y;

            TArray<FFloat16Color> Raw;
            Raw.SetNumUninitialized(W * H);

            RHICmdList.ReadSurfaceFloatData(
                Tex,
                FIntRect(0, 0, W, H),
                Raw,          // out
                (ECubeFace)0, // no cubemap
                 0,
                 0            // mip
            );

            // To LinealColor
            TArray64<FLinearColor> Pixels;
            Pixels.SetNumUninitialized((int64)W * (int64)H);
            for (int32 i = 0; i < W* H; ++i)
            {
                const FFloat16Color& C = Raw[i];
                Pixels[i] = FLinearColor(C.R.GetFloat(), C.G.GetFloat(), C.B.GetFloat(), 1.f);
            }

            // Output name
            const FString Stamp = MakeTimestampForFile(Req.TimestampUTC.GetTicks() > 0 ? Req.TimestampUTC : FDateTime::UtcNow());
            const FString BaseName = FString::Printf(TEXT("%s_%s_%dx%d"),
                *Req.SensorId.ToString(), *FaceToString(FaceIdx), W, H);

            const FString OutEXR = FPaths::Combine(ImagesPathAbs, FString::Printf(TEXT("%s_%s.exr"), *BaseName, *Stamp));

            // EXR
            IImageWriteQueueModule& ImgModule = FModuleManager::LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue");
            TUniquePtr<FImageWriteTask> Task = MakeUnique<FImageWriteTask>();
            Task->Filename = OutEXR;
            Task->Format = EImageFormat::EXR;
            Task->bOverwriteFile = true;
            Task->CompressionQuality = (int32)EImageCompressionQuality::Uncompressed;
            Task->PixelData = MakeUnique<TImagePixelData<FLinearColor>>(FIntPoint(W, H), MoveTemp(Pixels), nullptr);
            ImgModule.GetWriteQueue().Enqueue(MoveTemp(Task));
        });
}


// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

void UIrradianceExporter::EnsureCSVHeader()
{
    if (bCSVHeaderWritten) 
        return;

    const FString Header =
        TEXT("sensor_name,sensor_guid,utc,pos_x,pos_y,pos_z,n_x,n_y,n_z,side_px,warmup,azimuth_deg,altitude_deg,irradiance\n");

    FFileHelper::SaveStringToFile(Header, *CSVPathAbs, FFileHelper::EEncodingOptions::AutoDetect,
        &IFileManager::Get(), FILEWRITE_Append);
    bCSVHeaderWritten = true;
}


FString UIrradianceExporter::FaceToString(int32 FaceIdx)
{
    static const TCHAR* Names[6] = { 
        TEXT("PosX"), TEXT("NegX"), TEXT("PosY"),
        TEXT("NegY"), TEXT("PosZ"), TEXT("NegZ") };

    return (FaceIdx >= 0 && FaceIdx < 6) ? Names[FaceIdx] : TEXT("Face");
}


FString UIrradianceExporter::MakeTimestampForFile(const FDateTime& UTC)
{
    // YYYYMMDD_HHMMSS
    return UTC.ToString(TEXT("%Y%m%d_%H%M%S"));
}


