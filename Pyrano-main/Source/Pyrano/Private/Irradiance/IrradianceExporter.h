/*=============================================================================
    IrradianceExporter.h
  Handles exporting irradiance capture results for debugging and data analysis.
  Supports saving cubemap face images (EXR) and irradiance values to CSV files.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "IrradianceExporter.generated.h"

struct FCaptureRequest;
struct IPooledRenderTarget;

/** Export configuration for irradiance results */
USTRUCT(BlueprintType)
struct FExportOptions
{
    GENERATED_BODY()

public:

    /** Enables exporting irradiance results to CSV */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExportCSV = false;

    /** Enables saving cubemap faces as EXR images */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bExportImages = false;    

    /** Base directory for all exported data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDirectoryPath OutputDir;      

    /** Subdirectory inside OutputDir used for image exports */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ImagesSubdir = TEXT("Images");

    /** Base CSV file name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CSVFilename = TEXT("irradiance.csv");
};


UCLASS()
class UIrradianceExporter : public UObject
{
    GENERATED_BODY()

public:

    void Init(const FExportOptions& InOpts);

    /** CSV export utils */
    void AppendIrradianceRow(const FCaptureRequest& Req, float Irradiance, float Azimuth, float Altitude);
    void FlushCSVIfNeeded(bool bForce = false);

    /** Image export */
    void EnqueueFaceEXR(const FCaptureRequest& Req, int32 FaceIdx,
        TRefCountPtr<IPooledRenderTarget> FaceRT, FIntPoint Size);

private:

    // Absolute path to the CSV file
    FString CSVPathAbs;

    // Absolute path to the images folder
    FString ImagesPathAbs;

    // Ensures header is written once
    bool bCSVHeaderWritten = false;

    /** Writes the CSV header */
    void EnsureCSVHeader();

    /** Returns a short name for a cubemap face (PosX, NegX...) */
    static FString FaceToString(int32 FaceIdx); // +X,-X,+Y,-Y,+Z,-Z

    /** Formats UTC as a compact timestamp for export */
    static FString MakeTimestampForFile(const FDateTime& UTC);
};
