/*=============================================================================
    IrradianceExporter.h
  Handles exporting irradiance capture results for debugging and data analysis.
  Supports saving cubemap face images (EXR) and irradiance values to CSV files.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "RenderGraphResources.h"
#include "Templates/SharedPointer.h"
#include "CaptureRequest.h" 
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "IrradianceExporter.generated.h"

//------ EXPORT OPTIONS STRUCT ------
USTRUCT(BlueprintType)
struct FExportOptions
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExportCSV = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExportImages = false;    // cubemap faces (debug)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDirectoryPath OutputDir;      // base dir

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ImagesSubdir = TEXT("Images");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CSVFilename = TEXT("irradiance.csv");
};

//------ EXPORTER CLASS ------
UCLASS()
class UIrradianceExporter : public UObject
{
    GENERATED_BODY()

public:
     // Init
    void Init(const FExportOptions& InOpts);

     // CSV
    void AppendIrradianceRow(const FCaptureRequest& Req, float Irradiance);
    void FlushCSVIfNeeded(bool bForce = false);

     // Image
    void EnqueueFaceEXR(const FCaptureRequest& Req, int32 FaceIdx,
        TRefCountPtr<IPooledRenderTarget> FaceRT, FIntPoint Size);

private:
    FString CSVPathAbs;
    bool bCSVHeaderWritten = false;

    FString ImagesPathAbs;

    void EnsureCSVHeader();
    static FString FaceToString(int32 FaceIdx); // +X,-X,+Y,-Y,+Z,-Z
    static FString MakeTimestampForFile(const FDateTime& UTC);
};
