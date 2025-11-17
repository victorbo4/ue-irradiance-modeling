// IrradianceScheduler.cpp

#include "IrradianceScheduler.h"
#include "EngineUtils.h"  

// fwd
#include "IrradianceSubsystem.h"
#include "PyranometerComponent.h"

/*     
#include "SunPosition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
*/




// ============================================================================
//      SUBSYSTEM / SUN / VIEWPORT
// ============================================================================

void UIrradianceScheduler::EnsureSubsystem()
{
    if (!Irr.IsValid())
    {
        if (UWorld* W = GetWorld())
            Irr = W->GetSubsystem<UIrradianceSubsystem>();
    }
}

void UIrradianceScheduler::EnsureSunLocationOnce(const FSimConfig& Sim)
{
    if (USunSkyController* Sun = GetWorld()->GetSubsystem<USunSkyController>())
    {
        Sun->ApplyStaticConfig(
            Sim.Latitude,
            Sim.Longitude,
            Sim.Timezone,
            Sim.NorthOffset,
            false
        );
    }
}

void UIrradianceScheduler::SetSunUTC(const FDateTime& Utc)
{
    if (USunSkyController* Sun = GetWorld()->GetSubsystem<USunSkyController>())
        Sun->SetUTC(Utc);
    
}

void UIrradianceScheduler::ApplyViewportForSim(const FSimConfig& Sim)
{
    EnsureSubsystem();
    if (!Irr.IsValid()) return;

    const int32 Side = FMath::Max(32, Sim.ResolutionPx);
    if (Irr->ForceSquareViewportPIE(Side))
        bViewportForced = true;
}

void UIrradianceScheduler::RestoreViewportIfNeeded()
{
    EnsureSubsystem();
    if (!Irr.IsValid()) return;
    Irr->EndHijackView();
    if (bViewportForced)
    {
        Irr->RestoreViewportPIE();
        bViewportForced = false;
    }
}



// ============================================================================
//  TICK / STATE MACHINE
// ============================================================================

void UIrradianceScheduler::Tick(float DeltaTime)
{
    if (State == ESchedulerState::Idle) return;
    if (!bCaptureInFlight) return;

    // @TODO: Create Public Delegate in ConsumeLatestIrradiance (Subsystem)
    // and replace bCaptureInFlight with it.
    
    float Value = 0.f;
    if (!ConsumeIrradianceResult(Value))
        return;

    OnCaptureCompleted(Value);
    
}

void UIrradianceScheduler::OnCaptureCompleted(float IrrValue)
{
    bCaptureInFlight = false;

    if (IsSimulationMode())
    {
        // Simulation
        if (SensorIndex + 1 < Sensors.Num())
        {
            ++SensorIndex;
            LaunchNextForCurrentTimeSlot(BaseSimConfig);
        }
        else 
            AdvanceTimeSlotOrFinish(); // Last sensor in this slot -> move to the next slot or finish
    }
    else 
    {
        // CaptureOnce
        LaunchNextInQueue();
    }
}



// ============================================================================
//      PUBLIC API
// ============================================================================

void UIrradianceScheduler::CaptureOnceForActiveSensors(const FSimConfig& Sim)
{
    ClearQueue();

    // Set Location and SolarTime just once
    EnsureSunLocationOnce(Sim);
    SetSunUTC(Sim.StartTime);

    // One request per sensor
    TArray<UPyranometerComponent*> LocalSensors;
    GetActiveSensors(LocalSensors);
    for (UPyranometerComponent* S : LocalSensors)
        Queue.Enqueue(MakeRequest(Sim, S));


    EnsureSubsystem();
    if (Irr.IsValid())
    {
        const bool bCSV = Sim.bExportCSV;
        const FString OutDir = Sim.OutputPath.Path;
        Irr->ConfigureExport(bCSV, OutDir);
    }

    // Force viewport
    ApplyViewportForSim(Sim);

    // Start
    State = ESchedulerState::Capturing;
    LaunchNextInQueue();
}

void UIrradianceScheduler::StartSimulation(const FSimConfig& Sim)
{
    ClearQueue();                 

    BaseSimConfig = Sim;

    // Get active sensors
    Sensors.Reset();
    GetActiveSensors(Sensors);

    // Time slots
    BuildTimeSlots(Sim.StartTime, Sim.EndTime, Sim.SampleInterval);
    TimeIndex = 0;
    SensorIndex = 0;

    // Set latitude/longitude/timezone/north_offset just once
    EnsureSunLocationOnce(Sim);
    if (TimeSlots.Num() > 0)
        SetSunUTC(TimeSlots[0]);
    

    // Configure export
    EnsureSubsystem();
    if (Irr.IsValid())
        Irr->ConfigureExport(Sim.bExportCSV, Sim.OutputPath.Path);
    
    // Set viewport
    ApplyViewportForSim(Sim);

    State = ESchedulerState::Capturing;
    bCaptureInFlight = false;

    // Launch first capture
    if (Sensors.Num() > 0 && TimeSlots.Num() > 0)
        LaunchNextForCurrentTimeSlot(BaseSimConfig);
    else
    {
        RestoreViewportIfNeeded();
        State = ESchedulerState::Idle;
    }
}

void UIrradianceScheduler::ClearQueue()
{
    FCaptureRequest Dump;
    while (Queue.Dequeue(Dump)) {}

    // Clear state
    Current.Reset();
    TimeSlots.Reset();
    Sensors.Reset();
    TimeIndex   = 0;
    SensorIndex = 0;

    State               = ESchedulerState::Idle;
    bCaptureInFlight    = false;
    bViewportForced     = false;

}



// ============================================================================
//      STATE MACHINE LOGIC
// ============================================================================


void UIrradianceScheduler::LaunchNextInQueue()
{
    if (State != ESchedulerState::Capturing) return;
    if (bCaptureInFlight) return;

    FCaptureRequest Next;
    if (Queue.Dequeue(Next))
        LaunchCapture(Next);
    else
    {
        // Empty queue = end of CaptureOnce
        RestoreViewportIfNeeded();
        State = ESchedulerState::Idle;
        Current.Reset();
    }
}

void UIrradianceScheduler::LaunchCapture(const FCaptureRequest& Req)
{
    EnsureSubsystem();
    if (!Irr.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[Scheduler] IrradianceSubsystem not found."));
        return;
    }

    Current          = Req;
    bCaptureInFlight = true;

    Irr->StartSixFaceCapture(Req); 
}

void UIrradianceScheduler::AdvanceTimeSlotOrFinish()
{
    TimeIndex++;
    if (TimeIndex < TimeSlots.Num())
    {
        // Advance time slot
        SetSunUTC(TimeSlots[TimeIndex]);   // new solar time
        SensorIndex = 0;
        State = ESchedulerState::Capturing;
        LaunchNextForCurrentTimeSlot(BaseSimConfig);
    }
    else
    {
        // End of simulation
        RestoreViewportIfNeeded();

        State = ESchedulerState::Idle;
        Current.Reset();
    }
}

void UIrradianceScheduler::LaunchNextForCurrentTimeSlot(const FSimConfig& Sim)
{
    if (State != ESchedulerState::Capturing) return;
    if (bCaptureInFlight) return;

    if (SensorIndex < Sensors.Num())
    {
        UPyranometerComponent* S = Sensors[SensorIndex];
        FCaptureRequest Req = MakeRequest(Sim, S).WithTimestamp(TimeSlots[TimeIndex]);
        LaunchCapture(Req);
    }
    else
    {
        // No sensors left: advance or finish
        AdvanceTimeSlotOrFinish();
    }
}



// ============================================================================
//      HELPERS
// ============================================================================

void UIrradianceScheduler::GetActiveSensors(TArray<UPyranometerComponent*>& OutSensors) const
{
    OutSensors.Reset();
    for (TObjectIterator<UPyranometerComponent> It; It; ++It)
    {
        if (It->GetWorld() != GetWorld()) continue;
        if (!It->IsRegistered()) continue;
        if (!It->bEnabled) continue;            
        OutSensors.Add(*It);
    }
}

FCaptureRequest UIrradianceScheduler::MakeRequest(const FSimConfig& Sim, UPyranometerComponent* Sensor) const
{
    // Clean getters (without FTransform)
    const FVector PosWS = Sensor->GetWorldPosition();
    const FVector Normal = Sensor->GetNormalWS();

    const int32  SidePx = Sim.ResolutionPx;
    const uint32 Warmup = static_cast<uint32>(Sim.WarmupFrames);

    // Timestamp: during Simulation, we overwrite it with the current slot
    const FDateTime WhenUTC = Sim.StartTime;

    // FCaptureRequest: PosWS, NormalWS, SidePx, Warmup, SensorId(Guid), Timestamp
    return FCaptureRequest::Make(
        PosWS, Normal, SidePx, Warmup,
        Sensor->SensorGuid,
        Sensor->SensorName,
        WhenUTC);

}

void UIrradianceScheduler::BuildTimeSlots(const FDateTime& StartUTC, const FDateTime& EndUTC, const FTimespan& Step)
{
    TimeSlots.Reset();

    if (EndUTC <= StartUTC || Step.GetTicks() <= 0)
        return;

    // StartTime and EndTime included
    for (FDateTime t = StartUTC; t <= EndUTC; t += Step)
        TimeSlots.Add(t);
}

bool UIrradianceScheduler::ConsumeIrradianceResult(float& OutValue)
{
    EnsureSubsystem();
    if (!Irr.IsValid()) 
        return false;
    return Irr->ConsumeLatestIrradiance(OutValue);
}