/*=============================================================================
    SunSkyController.h
  Manages Unreal's SunSky Actor.
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

// --- Public API ---

    /** Applies static SunSky configuration (location, timezone, north offset, DST flag). */
    UFUNCTION(BlueprintCallable, Category = "SunSky")
    void ApplyStaticConfig(
        double Latitude,
        double Longitude,
        double Timezone,
        double NorthOffset,
        bool  bUseDST);

    /** Sets the SunSky date and time from a UTC timestamp(applies configured timezone). */
    UFUNCTION(BlueprintCallable, Category = "SunSky")
    void SetUTC(const FDateTime& UtcDateTime);

    /**
     * Returns solar azimuth/altitude using solar-standard convention:
     * azimuth = clockwise from North (0°=N, 90°=E, 180°=S, 270°=W),
     * altitude = angle above horizon. North direction includes NorthOffset.
     */
    UFUNCTION(BlueprintCallable, Category = "SunSky")
    bool GetSolarAngles(float& OutAzimuthDeg, float& OutAltitudeDeg) const;

private:

    /** Timezone offset in hours used to convert UTC to local solar time. */
    double TimezoneHours = 0.0;

    /** North offset in degrees applied to adjust world-space North. */
    double NorthOffsetDeg = 0.0;

    /** Cached reference to the SunSky actor in the current world. */
    TWeakObjectPtr<AActor> CachedSunSky;

    /** Returns the cached SunSky actor or performs a search if missing. */
    AActor* EnsureSunSky();  

    /** Locates a SunSky actor in the world (by tag or name). */
    static AActor* FindSunSkyActor(UWorld* World); 

// --- Helpers ---

    static bool SetDoubleProp(UObject* Obj, FName Name, double V);
    static bool SetIntProp(UObject* Obj, FName Name, int32 V);
    static bool SetBoolProp(UObject* Obj, FName Name, bool bV);

// --- Pushers ---

    /** Pushes date and solar-time fields into the SunSky actor from UTC. */
    void PushDateTime(AActor* SunSky, const FDateTime& UTC) const;

    /** Forces SunSky to rebuild via RerunConstructionScripts. */
    static void UpdateSun(AActor* SunSky);
};
