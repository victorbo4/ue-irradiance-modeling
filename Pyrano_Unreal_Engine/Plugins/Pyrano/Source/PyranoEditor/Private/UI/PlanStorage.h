/*=============================================================================
    PlanStorage.h
  Provides JSON-based save/load utilities for PyranoBlueprintLibrary.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimulationConfig.h"

class FPlanStorage
{

public:

    static bool SavePlan(const FString& FilePath, const FSimConfig& Config);
    static bool LoadPlan(const FString& FilePath, FSimConfig& OutConfig);
};
