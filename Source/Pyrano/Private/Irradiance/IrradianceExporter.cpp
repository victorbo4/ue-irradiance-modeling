// IrradianceExporter.cpp

#include "Irradiance/IrradianceExporter.h"

#include "RHI.h"
#include "RHIGPUReadback.h"
#include "RenderGraphResources.h"
#include "RHICommandList.h"
#include "RenderTargetPool.h"
#include "Logging/IrradianceLog.h"
#include "Simulation/CaptureRequest.h" 

#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "ImagePixelData.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
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

    PYRANO_INFO(TEXT("[Exporter] Init: this=%p outer=%s CSVPathAbs='%s'"),
        this,
        *GetNameSafe(GetOuter()),
        *CSVPathAbs);
}


bool UIrradianceExporter::EnsureCSVWritable() const
{
    if (CSVPathAbs.IsEmpty())
    {
        PYRANO_WARN(TEXT("[Exporter] CSVPathAbs is empty. Exporter not initialized or CSV disabled."));
        return false;
    }

    const FString Dir = FPaths::GetPath(CSVPathAbs);
    if (Dir.IsEmpty())
    {
        PYRANO_WARN(TEXT("[Exporter] CSV directory is empty (CSVPathAbs='%s')."), *CSVPathAbs);
        return false;
    }

    // Directory existence check (do not create here since this is const; creation in EnsureCSVHeader/Flush)
    return true;
}


// -----------------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------------

void UIrradianceExporter::AppendIrradianceRow(const FCaptureRequest& Req,
    float TotalIrradiance,
    float IrrMean, float IrrR, float IrrG, float IrrB,
    double ClearSkyGHI, double ClearSkyDNI, double ClearSkyDHI,
    float IrrDir,
    float Azimuth, float Altitude, float GeometricFactor,
    int32 SunOccluded, float SunHitDistanceM, float SunVisibility)
{
    if (!EnsureCSVWritable())
        return;

    EnsureCSVHeader();

    // Time / ID
    const FString& SensorName = Req.SensorName;
    const FString  SensorIdStr = Req.SensorId.ToString();
    const FString  TimeStr = Req.TimestampUTC.ToIso8601();

    // Sensor
    const FVector& Pos = Req.PosWS;
    const FVector& N = Req.NormalWS;

    // Solar Geometry
    const float SunAzDeg = Azimuth;
    const float SunAltDeg = Altitude;
    const float GeomF = GeometricFactor;

    // Clear-sky
    const double CS_GHI = ClearSkyGHI;
    const double CS_DNI = ClearSkyDNI;
    const double CS_DHI = ClearSkyDHI;

    // Occlusion
    const int32  Occluded = SunOccluded;
    const float HitDistM = SunHitDistanceM;
    const float Visibility = SunVisibility;

    // Irradiance
    const float DiffR_Lux = IrrR;
    const float DiffG_Lux = IrrG;
    const float DiffB_Lux = IrrB;
    const float Diff_Lux = IrrMean;
    const float Dir_Lux = IrrDir;
    const float Irradiance_wm2 = TotalIrradiance;

    const FString Line = FString::Printf(
        TEXT(
                // SensorName, SensorId, Timestamp
                "%s,%s,%s,"

                // --- Sensor ---
                "%.6f,%.6f,%.6f,"
                "%.6f,%.6f,%.6f,"
                "%d,%u,"

                // --- Solar Geometry ---
                "%.3f,%.3f,%.6f,"

                // --- Clear-sky ---
                "%.6f,%.6f,%.6f,"

                // --- Occlusion ---
                "%d,%.3f,%.3f,"

                // --- Sky View Factor ---
                "%.6f,"

                // --- Ambient (RGB + mean) ---
                "%.9f,%.9f,%.9f,%.9f,"

                // --- Direct ---
                "%.9f,"

                // -- Total (Normalized) --
                "%.9f\n"
            ),

            // SensorName, SensorId, Timestamp  
            *SensorName,
            *SensorIdStr,
            *TimeStr,

            // --- Sensor ---
            (double)Pos.X,
            (double)Pos.Y,
            (double)Pos.Z,
            (double)N.X,
            (double)N.Y,
            (double)N.Z,
            (int32)Req.SidePx,
            (uint32)Req.WarmupFrames,

            // --- Solar Geometry ---
            (double)SunAzDeg,
            (double)SunAltDeg,
            (double)GeomF,

            // --- Clear-sky ---
            (double)CS_GHI,
            (double)CS_DNI,
            (double)CS_DHI,

            // --- Occlusion ---
            (int32)Occluded,
            (double)HitDistM,
            (double)Visibility,

            // --- Sky View Factor ---
            (double)Req.SkyViewFactor,

            // --- Diffuse (RGB + mean) ---
            (double)DiffR_Lux,
            (double)DiffG_Lux,
            (double)DiffB_Lux,
            (double)Diff_Lux,

            // --- Direct ---
            (double)Dir_Lux,
            
            // --- Total normalized ---
            (double)Irradiance_wm2
    );

    // Buffer instead of writing every row
    PendingCSV += Line;
    PendingLineCount++;
    TotalRowsAppended++;

    // Periodic flush
    FlushCSVIfNeeded(false);
}



void UIrradianceExporter::FlushCSVIfNeeded(bool bForce)
{
    if (!EnsureCSVWritable())
        return;

    if (PendingLineCount <= 0)
        return;

    if (!bForce && PendingLineCount < FlushThresholdLines)
        return;

    const FString Dir = FPaths::GetPath(CSVPathAbs);
    if (!IFileManager::Get().DirectoryExists(*Dir))
    {
        const bool bDirOk = IFileManager::Get().MakeDirectory(*Dir, true);
        if (!bDirOk)
        {
            PYRANO_ERR(TEXT("[Exporter] Failed to create CSV directory '%s'. Skipping flush."), *Dir);
            // Drop buffer to avoid unbounded growth
            PendingCSV.Reset();
            PendingLineCount = 0;
            return;
        }
        PYRANO_WARN(TEXT("[Exporter] CSV directory was missing; created '%s'."), *Dir);
    }

    const bool bOk = FFileHelper::SaveStringToFile(
        PendingCSV,
        *CSVPathAbs,
        FFileHelper::EEncodingOptions::AutoDetect,
        &IFileManager::Get(),
        FILEWRITE_Append);

    if (!bOk)
    {
        PYRANO_ERR(TEXT("[Exporter] Failed to flush CSV buffer (lines=%d, total_rows=%llu) to '%s'."),
            PendingLineCount, TotalRowsAppended, *CSVPathAbs);

        // Drop buffer to avoid repeated attempts + memory growth
        PendingCSV.Reset();
        PendingLineCount = 0;
        return;
    }

    // Success
    PYRANO_INFO(TEXT("[Exporter] Flushed CSV buffer (lines=%d, total_rows=%llu) -> '%s'."),
        PendingLineCount, TotalRowsAppended, *CSVPathAbs);

    PendingCSV.Reset();
    PendingLineCount = 0;

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
        TEXT("sensor_name,sensor_guid,utc,")
        TEXT("pos_x,pos_y,pos_z,")
        TEXT("n_x,n_y,n_z,")
        TEXT("side_px,warmup,")
        TEXT("azimuth_deg,altitude_deg,geometric_factor,")
        TEXT("clearsky_ghi_wm2,clearsky_dni_wm2,clearsky_dhi_wm2,")
        TEXT("sun_occluded,sun_hit_distance_m,sun_visibility,")
        TEXT("sky_view_factor,")
        TEXT("raw_r_lux,raw_g_lux,raw_b_lux,")
        TEXT("sim_comp_amb_lux,sim_comp_direct_lux,")
        TEXT("irr_final_normalized_wm2\n");

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


