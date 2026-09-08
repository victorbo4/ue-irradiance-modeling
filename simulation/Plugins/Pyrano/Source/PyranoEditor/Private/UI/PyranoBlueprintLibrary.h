/*=============================================================================
    PyranoBlueprintLibrary.h
  Blueprint functions for plan file save/load operations.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PyranoBlueprintLibrary.generated.h"

struct FSimConfig;

UCLASS()
class PYRANOEDITOR_API UPyranoBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    /** Opens "Save As Dialog" and saves plan in JSON. */
    UFUNCTION(BlueprintCallable, Category = "Pyrano|Plan")
    static bool SavePlanWithDialog(const FSimConfig& Config);

    /** Opens "Open File Dialog" and loads plan in JSON. */
    UFUNCTION(BlueprintCallable, Category = "Pyrano|Plan")
    static bool LoadPlanWithDialog(FSimConfig& OutConfig);
};
