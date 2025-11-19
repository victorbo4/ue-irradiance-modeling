// SunSkyController.cpp

#include "Subsystems/SunSkyController.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Logging/IrradianceLog.h"

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

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


// -----------------------------------------------------------------------------
//  Solar Angle Helper
// -----------------------------------------------------------------------------

static void SunAnglesFromDirection(
    const FVector& Dir,
    double NorthOffsetDeg,
    float& OutAzimuthDeg,
    float& OutAltitudeDeg)
{
    const FVector Up(0.f, 0.f, 1.f);

    // Normalized sun direction
    const FVector D = Dir.GetSafeNormal();

    // Altitude
    const float z = FVector::DotProduct(D, Up);
    OutAltitudeDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(z, -1.f, 1.f)));

    // Horizontal projection
    FVector H = D - z * Up;    // project in X-Y plane
    if (!H.Normalize())
    {
        // Sun exactly at the zenith -> azimuth undefined, we leave it at 0
        OutAzimuthDeg = 0.f;
        return;
    }

    // World's north vector
    // We take +X as the base, and rotate it in the X-Y plane by NorthOffset around Z
    const double Rad = FMath::DegreesToRadians(NorthOffsetDeg);
    const double c = FMath::Cos(Rad);
    const double s = FMath::Sin(Rad);

    // +X = (1,0,0) → rotated in XY:
    FVector North(
        (float)c,   // X
        (float)s,   // Y
        0.f);

    North.Normalize();

    // Up x North to form an orthogonal system
    const FVector East = Up ^ North;   

    // sun coordinates in base (N, E)
    const float x = FVector::DotProduct(H, East);   // component towards E
    const float y = FVector::DotProduct(H, North);  // component towards N

    // atan2(East, North):
    // y = 0, x > 0  ->  90° (E)
    // y < 0, x = 0  -> 180° (S)
    // y = 0, x < 0  -> 270° (W)
    float Az = FMath::RadiansToDegrees(FMath::Atan2(x, y));

    if (Az < 0.f)
    {
        Az += 360.f;
    }

    OutAzimuthDeg = Az;
}


// -----------------------------------------------------------------------------
//  Search / Cache
// -----------------------------------------------------------------------------

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
    {
        return CachedSunSky.Get();
    }

    AActor* Found = FindSunSkyActor(GetWorld());
    CachedSunSky = Found;
    return Found;
}


// -----------------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------------

void USunSkyController::ApplyStaticConfig(double Latitude, double Longitude, double Timezone, double NorthOffset, bool bUseDST)
{
    if (AActor* SunSky = EnsureSunSky())
    {
        PYRANO_SUCCESS(TEXT("[SunSky] SunSky Actor detected"));

        TimezoneHours = Timezone;
        NorthOffsetDeg = NorthOffset;

        SetDoubleProp(SunSky, TEXT("Latitude"), Latitude);
        SetDoubleProp(SunSky, TEXT("Longitude"), Longitude);
        SetDoubleProp(SunSky, TEXT("NorthOffset"), NorthOffset);
        SetBoolProp(SunSky, TEXT("UseDaylightSavingTime"), bUseDST);
        UpdateSun(SunSky);

        // Check
        bool bSuccess = true;

        bool bDst = true;
        bSuccess &= GetBoolProp(SunSky, TEXT("UseDaylightSavingTime"), bDst);
        PYRANO_INFO(TEXT("[SunSky] DST = %s"), bDst ? TEXT("TRUE") : TEXT("FALSE"));

        float Lat = 0.f;
        bSuccess &= GetFloatProp(SunSky, TEXT("Latitude"), Lat);
        PYRANO_INFO(TEXT("[SunSky] Latitude = %.3f"), Lat);

        float Lon = 0.f;
        bSuccess &= GetFloatProp(SunSky, TEXT("Longitude"), Lon);
        PYRANO_INFO(TEXT("[SunSky] Longitude = %.3f"), Lon);

        float NOffset = 0.f;
        bSuccess &= GetFloatProp(SunSky, TEXT("NorthOffset"), NOffset);
        PYRANO_INFO(TEXT("[SunSky] NorthOffset = %.3f"), NOffset);

        if (!bSuccess)
        {
            PYRANO_ERR(TEXT("[SunSky] One or more SunSky reflected properties COULD NOT be read."));
        }
        else
        {
            PYRANO_SUCCESS(TEXT("[SunSky] All reflected properties validated OK."));
        }
    }
    else
    {
        PYRANO_WARN(TEXT("[SunSky] SunSky Actor not detected. Configuration may not be applied"));
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


bool USunSkyController::GetSolarAngles(float& OutAzimuthDeg, float& OutAltitudeDeg) const
{
    OutAzimuthDeg = 0.f;
    OutAltitudeDeg = 0.f;

    AActor* SunSky = const_cast<USunSkyController*>(this)->EnsureSunSky();
    if (!SunSky)
        return false;

    // SunSky usually has an internal DirectionalLightComponent
    if (UDirectionalLightComponent* Sun = SunSky->FindComponentByClass<UDirectionalLightComponent>())
    {
        // Dir from scene to Sun
        const FVector SunDir = -Sun->GetComponentTransform().GetUnitAxis(EAxis::X);
        SunAnglesFromDirection(SunDir, NorthOffsetDeg, OutAzimuthDeg, OutAltitudeDeg);
        return true;
    }

    return false;
}


// -----------------------------------------------------------------------------
//  Internal
// -----------------------------------------------------------------------------

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

