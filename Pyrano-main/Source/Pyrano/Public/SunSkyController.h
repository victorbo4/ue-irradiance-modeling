/*=============================================================================
    SunSkyController.h
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"
#include "SunSkyController.generated.h"

class AActor;

UCLASS()
class USunSkyController : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "SunSky")
    void ApplyStaticConfig(
        double Latitude,
        double Longitude,
        double Timezone,
        double NorthOffset,
        bool  bUseDST);

    UFUNCTION(BlueprintCallable, Category = "SunSky")
    void SetUTC(const FDateTime& UtcDateTime);

private:
    double TimezoneHours = 0.0;

    TWeakObjectPtr<AActor> CachedSunSky;

    AActor* EnsureSunSky();                        
    static AActor* FindSunSkyActor(UWorld* World); 

    // Helpers
    static bool SetDoubleProp(UObject* Obj, FName Name, double V);
    static bool SetIntProp(UObject* Obj, FName Name, int32 V);
    static bool SetBoolProp(UObject* Obj, FName Name, bool bV);

    // Push by reflection 
    void PushDateTime(AActor* SunSky, const FDateTime& UTC) const;
    static void UpdateSun(AActor* SunSky);
};
