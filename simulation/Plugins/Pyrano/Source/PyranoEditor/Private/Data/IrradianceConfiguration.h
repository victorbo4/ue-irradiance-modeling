/*=============================================================================
    IrradianceConfiguration.h
  Applies all render settings and CVars needed for irradiance capture.
/============================================================================*/

#pragma once

#include "Simulation/SimulationConfig.h"
#include "Containers/Array.h"
#include "Misc/Optional.h"

struct FIrradianceCVarState
{
    FString Name;
    TOptional<int32> IntValue;
    TOptional<float> FloatValue;
};


namespace FIrradianceRenderConfig
{
	void ApplyIrradianceConfig(const FSimConfig& Config, TArray<FIrradianceCVarState>& OutPreviousValues);
    void RestoreIrradianceConfig(const TArray<FIrradianceCVarState>& PreviousValues);
}

