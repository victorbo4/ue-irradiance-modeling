/*=============================================================================
    SensorInfo.h
  Simple struct containing the displayed properties of a sensor.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SensorInfo.generated.h"

USTRUCT(BlueprintType)
struct FSensorInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid SensorGuid;

    UPROPERTY(BlueprintReadOnly)
    bool bEnabled = false;

    UPROPERTY(BlueprintReadOnly)
    FString Name;

    UPROPERTY(BlueprintReadOnly)
    FVector PositionWS = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector NormalWS = FVector::UpVector;
};
