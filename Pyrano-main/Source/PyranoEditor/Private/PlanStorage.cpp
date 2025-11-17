// PlanStorage.cpp

#include "PlanStorage.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"

bool FPlanStorage::SavePlan(const FString& FilePath, const FSimConfig& Config)
{
    FString OutputJson;
    if (!FJsonObjectConverter::UStructToJsonObjectString(Config, OutputJson))
        return false;

    return FFileHelper::SaveStringToFile(OutputJson, *FilePath);
}


bool FPlanStorage::LoadPlan(const FString& FilePath, FSimConfig& OutConfig)
{
    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *FilePath))
        return false;

    return FJsonObjectConverter::JsonObjectStringToUStruct(
        JsonStr,
        &OutConfig,
        0, 0);
}
