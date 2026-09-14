/*=============================================================================
    SimulationConfig.h
  Configuration parameters defining location, timing, 
  and capture settings used for irradiance simulation.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SimulationConfig.generated.h"

USTRUCT(BlueprintType)
struct FSimConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
    float Latitude = 0.f;  

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
    float Longitude = 0.f;  

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
    float Timezone = 0;     

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
    float NorthOffset = 0;
   
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
    FDateTime StartTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
    FDateTime EndTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
    FTimespan SampleInterval = FTimespan::FromMinutes(5);
    
    // S6: Lumen's temporal accumulation needs up to
    // r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated (engine default 10) /
    // r.Lumen.Reflections.Temporal.MaxFramesAccumulated (engine default 12) frames
    // to converge. 8 was below both. 16 gives margin without an empirically-measured
    // convergence number yet - see the Lumen-convergence check still pending for S6.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    int32 WarmupFrames = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    int32 ResolutionPx = 256;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    bool bPathTracing = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    bool bExportCSV = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
    FDirectoryPath OutputPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    bool bExportImages = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
    float MinSunAltitudeDeg = 0.f;

    // If true (default), the direct-beam term is the clear-sky model's DNI scaled by
    // cos(theta) and ray-traced sun visibility (physical, no fitted coefficients). If
    // false, the legacy fitted path is used instead (DirectionalLight lux converted via
    // DirectLinearCoeff/DirectQuadraticCoeff) - kept for comparison. See S3 in
    // ai/reports/bug-tracker.md.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    bool bUseAnalyticDirectTerm = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClearSky")
    float AltitudeMeters = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClearSky")
    float LinkeTurbidity = 2.f;


    bool IsValid() const
    {
        return Latitude >= -90.f && Latitude <= 90.f
            && Longitude >= -180.f && Longitude <= 180.f
            && ResolutionPx > 0
            && WarmupFrames > 0
            && StartTime < EndTime
            && SampleInterval.GetTotalSeconds() > 0;
    }
};

