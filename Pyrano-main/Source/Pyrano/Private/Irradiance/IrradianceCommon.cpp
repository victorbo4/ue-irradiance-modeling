// IrradianceCommon.cpp

#include "Irradiance/IrradianceCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

// -----------------------------------------------------------------------------
//  Utils
// -----------------------------------------------------------------------------

TArray<FQuat> IrradianceCommon::Utils::GenerateCubemapFaceQuats()
{
    auto Q = [](const FVector& Fwd, const FVector& Up)
        {
            return FRotationMatrix::MakeFromXZ(Fwd.GetSafeNormal(), Up.GetSafeNormal()).ToQuat();
        };

    TArray<FQuat> R;
    R.Reserve(6);

    // UE: +X forward, +Z up
    R.Add(Q(FVector(+1, 0, 0), FVector(0, 0, +1))); // +X
    R.Add(Q(FVector(-1, 0, 0), FVector(0, 0, +1))); // -X
    R.Add(Q(FVector(0, +1, 0), FVector(0, 0, +1))); // +Y
    R.Add(Q(FVector(0, -1, 0), FVector(0, 0, +1))); // -Y
    R.Add(Q(FVector(0, 0, +1), FVector(0, -1, 0))); // +Z
    R.Add(Q(FVector(0, 0, -1), FVector(0, +1, 0))); // -Z

    return R;
}


bool IrradianceCommon::Utils::IsPIEClientWorld(const UWorld* W)
{
    if (!W) 
        return false;

    if (W->WorldType != EWorldType::PIE) 
        return false;

    const ENetMode NM = W->GetNetMode();
    return (NM == NM_Client || NM == NM_Standalone);
}


UWorld* IrradianceCommon::Utils::GetGameWorldSafe()
{
    if (!GEngine) 
        return nullptr;

    // Prioritize PIE / Game
    for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
    {
        if (Ctx.World() &&
            (Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE))
        {
            return Ctx.World();
        }
    }

    // Fallback just in case
    return GEngine->GetCurrentPlayWorld();
}


// -----------------------------------------------------------------------------
//  Console
// -----------------------------------------------------------------------------

ELogVerbosity::Type IrradianceCommon::Console::GConsoleVerbosityLimit = ELogVerbosity::Log;

ELogVerbosity::Type IrradianceCommon::Console::GetConsoleVerbosityLimit()
{
    return GConsoleVerbosityLimit;
}


void IrradianceCommon::Console::SetConsoleVerbosityLimit(ELogVerbosity::Type NewLimit)
{
    GConsoleVerbosityLimit = NewLimit;
}