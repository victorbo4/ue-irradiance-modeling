// SunSkyController.cpp

#include "SunSkyController.h"
#include "EngineUtils.h"
#include "IrradianceLog.h"



// ============================================================================
//      HELPERS
// ============================================================================

bool USunSkyController::SetDoubleProp(UObject* Obj, FName Name, double V)
{
    if (!Obj) return false;
    if (FDoubleProperty* P = FindFProperty<FDoubleProperty>(Obj->GetClass(), Name))
    {
        void* Addr = P->ContainerPtrToValuePtr<void>(Obj);
        P->SetFloatingPointPropertyValue(Addr, V);
        return true;
    }
    return false;
}

bool USunSkyController::SetIntProp(UObject* Obj, FName Name, int32 V)
{
    if (!Obj) return false;
    if (FIntProperty* P = FindFProperty<FIntProperty>(Obj->GetClass(), Name))
    {
        void* Addr = P->ContainerPtrToValuePtr<void>(Obj);
        P->SetIntPropertyValue(Addr, (int64)V);
        return true;
    }
    return false;
}

bool USunSkyController::SetBoolProp(UObject* Obj, FName Name, bool bV)
{
    if (!Obj) return false;
    if (FBoolProperty* P = FindFProperty<FBoolProperty>(Obj->GetClass(), Name))
    {
        void* Addr = P->ContainerPtrToValuePtr<void>(Obj);
        P->SetPropertyValue(Addr, bV);
        return true;
    }
    return false;
}


static bool GetIntProp(UObject* Obj, FName Name, int32& OutValue)
{
    if (!Obj) return false;

    if (FIntProperty* P = FindFProperty<FIntProperty>(Obj->GetClass(), Name))
    {
        void* Addr = P->ContainerPtrToValuePtr<void>(Obj);
        OutValue = (int32)P->GetSignedIntPropertyValue(Addr);
        return true;
    }
    return false;
}

static bool GetFloatProp(UObject* Obj, FName Name, float& OutValue)
{
    if (!Obj) return false;

    if (FFloatProperty* P = FindFProperty<FFloatProperty>(Obj->GetClass(), Name))
    {
        void* Addr = P->ContainerPtrToValuePtr<void>(Obj);
        OutValue = P->GetFloatingPointPropertyValue(Addr);
        return true;
    }

    if (FDoubleProperty* P = FindFProperty<FDoubleProperty>(Obj->GetClass(), Name))
    {
        void* Addr = P->ContainerPtrToValuePtr<void>(Obj);
        OutValue = (float)P->GetFloatingPointPropertyValue(Addr);
        return true;
    }

    return false;
}


static bool GetBoolProp(UObject* Obj, FName Name, bool& OutValue)
{
    if (!Obj) return false;
    if (FBoolProperty* P = FindFProperty<FBoolProperty>(Obj->GetClass(), Name))
    {
        void* Addr = P->ContainerPtrToValuePtr<void>(Obj);
        OutValue = P->GetPropertyValue(Addr);
        return true;
    }
    return false;
}



// ============================================================================
//      SEARCH / CACHE
// ============================================================================

AActor* USunSkyController::FindSunSkyActor(UWorld* World)
{
    // Find SunSky by tag or name
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It;
        if (A->ActorHasTag(FName("SunSky"))) return A;
        if (A->GetName().Contains(TEXT("SunSky"))) return A;
    }
    return nullptr;
}

AActor* USunSkyController::EnsureSunSky()
{
    if (CachedSunSky.IsValid() && CachedSunSky->GetWorld() == GetWorld())
        return CachedSunSky.Get();

    AActor* Found = FindSunSkyActor(GetWorld());
    CachedSunSky = Found;
    return Found;
}



// ============================================================================
//      PUBLIC API
// ============================================================================

void USunSkyController::ApplyStaticConfig(double Latitude, double Longitude, double Timezone, double NorthOffset, bool bUseDST)
{
    if (AActor* SunSky = EnsureSunSky())
    {
        TimezoneHours = Timezone;

        SetDoubleProp(SunSky, TEXT("Latitude"), Latitude);
        SetDoubleProp(SunSky, TEXT("Longitude"), Longitude);
        SetDoubleProp(SunSky, TEXT("NorthOffset"), NorthOffset);
        SetBoolProp(SunSky, TEXT("UseDaylightSavingTime"), bUseDST);
        UpdateSun(SunSky);

        // Check
        bool bDst = true;
        if (GetBoolProp(SunSky, TEXT("UseDaylightSavingTime"), bDst))
            UE_LOG(LogTemp, Warning, TEXT("[SunSky] DST = %s"), bDst ? TEXT("TRUE") : TEXT("FALSE"));

        float Lat = 0.f;
        if (GetFloatProp(SunSky, TEXT("Latitude"), Lat))
            UE_LOG(LogTemp, Warning, TEXT("[SunSky] Latitude = %.3f"), Lat);

        float Lon = 0.f;
        if (GetFloatProp(SunSky, TEXT("Longitude"), Lon))
            PYRANO_INFO(TEXT("[SunSky] Longitude = %.3f"), Lon);

    }
}


void USunSkyController::SetUTC(const FDateTime& UTC)
{
    if (AActor* SunSky = EnsureSunSky())
    {
        PushDateTime(SunSky, UTC);
        UpdateSun(SunSky);
    }
}



// ============================================================================
//      INTERNAL
// ============================================================================

void USunSkyController::PushDateTime(AActor* SunSky, const FDateTime& UTC) const
{
    // SolarTime = UTC + Timezone
    const FDateTime Local = UTC + FTimespan::FromHours(TimezoneHours);

    const int32 Year = UTC.GetYear();
    const int32 Month = UTC.GetMonth();
    const int32 Day = UTC.GetDay();
    const double SolarTimeHours = double(Local.GetHour()) + double(Local.GetMinute()) / 60.0 + double(Local.GetSecond()) / 3600.0;

    SetIntProp(SunSky, TEXT("Year"), Year);
    SetIntProp(SunSky, TEXT("Month"), Month);
    SetIntProp(SunSky, TEXT("Day"), Day);
    SetDoubleProp(SunSky, TEXT("SolarTime"), SolarTimeHours); 

}


void USunSkyController::UpdateSun(AActor* SunSky)
{
     // This updates rendering (Property value is updated in real time)
     SunSky->RerunConstructionScripts();
}

