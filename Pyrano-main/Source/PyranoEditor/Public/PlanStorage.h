/*=============================================================================
    PlanStorage.h
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SimulationConfig.h"

class FPlanStorage
{

public:
    static bool SavePlan(const FString& FilePath, const FSimConfig& Config);
    static bool LoadPlan(const FString& FilePath, FSimConfig& OutConfig);
};


