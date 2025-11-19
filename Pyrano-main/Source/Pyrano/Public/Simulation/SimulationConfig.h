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
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    int32 WarmupFrames = 8;

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

