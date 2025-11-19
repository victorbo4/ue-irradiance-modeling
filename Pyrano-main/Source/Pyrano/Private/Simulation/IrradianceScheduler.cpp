// IrradianceScheduler.cpp

#include "Simulation/IrradianceScheduler.h"
#include "EngineUtils.h"  
#include "Subsystems/IrradianceSubsystem.h"
#include "Components/PyranometerComponent.h"
#include "Subsystems/SunSkyController.h"
#include "Logging/IrradianceLog.h"

// -----------------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------------

void UIrradianceScheduler::CaptureOnce(const FSimConfig& Sim)
{
    PYRANO_INFO(TEXT("[Scheduler] CaptureOnce requested"));

    ClearQueue();
    PrepareSimulation(Sim, Sim.StartTime);

    // One request per sensor
    TArray<UPyranometerComponent*> LocalSensors;
    GetActiveSensors(LocalSensors);
    for (UPyranometerComponent* S : LocalSensors)
    {
        Queue.Enqueue(MakeRequest(Sim, S));
    }

    PYRANO_INFO(TEXT("[Scheduler] CaptureOnce queued for %d sensors"), LocalSensors.Num());
    LaunchNextOneShotCapture();
}


void UIrradianceScheduler::StartSimulation(const FSimConfig& Sim)
{
    PYRANO_INFO(TEXT("[Scheduler] StartSimulation requested"));
    ClearQueue();

    // Get active sensors
    Sensors.Reset();
    GetActiveSensors(Sensors);

    // Time slots
    BuildTimeSlots(Sim.StartTime, Sim.EndTime, Sim.SampleInterval);
    TimeIndex   = 0;
    SensorIndex = 0;

    if (TimeSlots.Num() == 0 || Sensors.Num() == 0)
    {
        // Nothing to simulate
        PYRANO_WARN(TEXT("[Scheduler] Simulation aborted (Sensors=%d, TimeSlots=%d)"),
            Sensors.Num(), TimeSlots.Num());
        RestoreViewport();
        State = ESchedulerState::Idle;
        return;
    }

    PrepareSimulation(Sim, TimeSlots[0]);

    PYRANO_INFO(TEXT("[Scheduler] Simulation starting: Sensors=%d, TimeSlots=%d"),
        Sensors.Num(), TimeSlots.Num());
    LaunchNextSimulationCapture(BaseSimConfig);
}


void UIrradianceScheduler::ClearQueue()
{
    // Empty pending requests
    FCaptureRequest Dump;
    int32 Count = 0;
    while (Queue.Dequeue(Dump))
        Count++;

    // Clear state
    Current.Reset();
    TimeSlots.Reset();
    Sensors.Reset();
    TimeIndex   = 0;
    SensorIndex = 0;

    State               = ESchedulerState::Idle;
    bCaptureInFlight    = false;
    bViewportForced     = false;

    BaseSimConfig = FSimConfig();
    PYRANO_VERBOSE(TEXT("[Scheduler] Queue cleared (removed %d pending captures)"), Count);
}


// -----------------------------------------------------------------------------
//  Subsystem - SunSky 
// -----------------------------------------------------------------------------

void UIrradianceScheduler::EnsureSubsystem()
{
    if (!Irr.IsValid())
    {
        if (UWorld* W = GetWorld())
        {
            Irr = W->GetSubsystem<UIrradianceSubsystem>();
            if (Irr.IsValid())
            {
                PYRANO_VERBOSE(TEXT("[Scheduler] IrradianceSubsystem acquired"));
            }
            else
            {
                PYRANO_WARN(TEXT("[Scheduler] IrradianceSubsystem not found for World=%s"), *W->GetName());
            }
        }
        else
        {
            PYRANO_WARN(TEXT("[Scheduler] GetWorld() returned null; World not found"));
        }
    }
}


void UIrradianceScheduler::ConfigureSunSkyOnce(const FSimConfig& Sim)
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
    else
    {
        PYRANO_WARN(TEXT("[Scheduler] SunSkyController subsystem not found; using existing SunSky settings"));
    }
}


void UIrradianceScheduler::SetSunSkyUTC(const FDateTime& Utc)
{
    if (USunSkyController* Sun = GetWorld()->GetSubsystem<USunSkyController>())
    {
        Sun->SetUTC(Utc);
    }
    else
    {
        PYRANO_WARN(TEXT("[Scheduler] Cannot SetSunUTC: SunSkyController not found"));
    }
}


// -----------------------------------------------------------------------------
//  Viewport
// -----------------------------------------------------------------------------

void UIrradianceScheduler::ApplyViewport(const FSimConfig& Sim)
{
    EnsureSubsystem();
    if (!Irr.IsValid())
    {
        PYRANO_WARN(TEXT("[Viewport] Cannot apply viewport: IrradianceSubsystem not valid"));
        return;

    }

    const int32 Side = FMath::Max(32, Sim.ResolutionPx);
    if (Irr->ForceSquareViewportPIE(Side))
    {
        bViewportForced = true;
        PYRANO_INFO(TEXT("[Viewport] Viewport forced successfully"));
    }
    else
    {
        PYRANO_VERBOSE(TEXT("[Viewport] ForceViewportPIE returned false (maybe already forced or not PIE client)"));
    }
}


void UIrradianceScheduler::RestoreViewport()
{
    EnsureSubsystem();
    if (!Irr.IsValid())
    {
        PYRANO_WARN(TEXT("[Viewport] Cannot restore viewport: IrradianceSubsystem not valid"));
        return;

    }

    Irr->EndHijackView();
    if (bViewportForced)
    {
        Irr->RestoreViewportPIE();
        bViewportForced = false;
        PYRANO_INFO(TEXT("[Viewport] Viewport restored successfully"));
    }
}


// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

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

    PYRANO_VERBOSE(TEXT("[Scheduler] GetActiveSensors -> %d sensor(s)"), OutSensors.Num());
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


bool UIrradianceScheduler::ConsumeIrradianceResult(float& OutValue)
{
    EnsureSubsystem();
    if (!Irr.IsValid()) 
        return false;

    const bool bOK = Irr->ConsumeLatestIrradiance(OutValue, BaseSimConfig.MinSunAltitudeDeg);
    if (!bOK)
        return false;

    FString SensorName = TEXT("<unknown>");
    FString Timestamp = TEXT("<none>");

    if (Current.IsSet())
    {
        SensorName = Current->SensorName;
        Timestamp = Current->TimestampUTC.ToIso8601();
    }

    PYRANO_SUCCESS(TEXT("[RESULT] Sensor='%s'  UTC=%s  Irradiance=%.6f"), *SensorName, *Timestamp, OutValue);
	return true;
}


// -----------------------------------------------------------------------------
//  Shared flow
// -----------------------------------------------------------------------------

void UIrradianceScheduler::Tick(float DeltaTime)
{
    if (State == ESchedulerState::Idle) 
        return;
    if (!bCaptureInFlight) 
        return;

    // @TODO: Create Public Delegate in ConsumeLatestIrradiance (Subsystem)
    // and replace bCaptureInFlight with it.
    
    float Value = 0.f;
    if (!ConsumeIrradianceResult(Value))
        return;

    OnCaptureCompleted(Value);
    
}


void UIrradianceScheduler::PrepareSimulation(const FSimConfig& Sim, const FDateTime& InitialTimeUTC)
{
    BaseSimConfig = Sim;
    PYRANO_INFO(TEXT("[Scheduler] Preparing simulation..."));

    // Config SunSky
    ConfigureSunSkyOnce(Sim);
    SetSunSkyUTC(InitialTimeUTC);

    // Export
    EnsureSubsystem();
    if (Irr.IsValid())
    {
        Irr->ConfigureExport(Sim.bExportCSV, Sim.bExportImages, Sim.OutputPath.Path);
        PYRANO_VERBOSE(
            TEXT("[Scheduler] Export configured (CSV=%s, Images=%s, Path=%s)"),
            Sim.bExportCSV ? TEXT("true") : TEXT("false"),
            Sim.bExportImages ? TEXT("true") : TEXT("false"),
            *Sim.OutputPath.Path);
    }
    else
    {
        PYRANO_WARN(TEXT("[Scheduler] Cannot configure export: IrradianceSubsystem not valid"));
    }

    // Viewport
    ApplyViewport(Sim);

    State = ESchedulerState::Capturing;
    bCaptureInFlight = false;

    PYRANO_INFO(TEXT("[Scheduler] Simulation prepared (mode=%s)"),
        TimeSlots.Num() > 0 ? TEXT("Simulation") : TEXT("CaptureOnce"));
}


void UIrradianceScheduler::LaunchCapture(const FCaptureRequest& Req)
{
    EnsureSubsystem();
    if (!Irr.IsValid())
    {
        PYRANO_ERR(TEXT("[Scheduler] Cannot launch capture: IrradianceSubsystem not found"));
        return;
    }

    Current          = Req;
    bCaptureInFlight = true;

    PYRANO_VERBOSE(TEXT("[Scheduler] StartSixFaceCapture -> %s"), *Req.ToString());
    Irr->StartSixFaceCapture(Req); 
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
            LaunchNextSimulationCapture(BaseSimConfig);
        }
        else
        {
            // Last sensor in this slot -> move to the next slot or finish
            AdvanceTimeSlotOrFinish(); 
        }
    }
    else 
    {
        // CaptureOnce
        LaunchNextOneShotCapture();
    }
}


// -----------------------------------------------------------------------------
//  CaptureOnce flow
// -----------------------------------------------------------------------------

void UIrradianceScheduler::LaunchNextOneShotCapture()
{
    if (State != ESchedulerState::Capturing) 
        return;
    if (bCaptureInFlight) 
        return;

    FCaptureRequest Next;
    if (Queue.Dequeue(Next))
    {
        LaunchCapture(Next);
    }
    else
    {
        // Empty queue = end of CaptureOnce
        PYRANO_INFO(TEXT("[Scheduler] CaptureOnce completed (queue empty)"));
        RestoreViewport();
        State = ESchedulerState::Idle;
        Current.Reset();
    }
}


// -----------------------------------------------------------------------------
//  Simulation flow
// -----------------------------------------------------------------------------

void UIrradianceScheduler::BuildTimeSlots(const FDateTime& StartUTC, const FDateTime& EndUTC, const FTimespan& Step)
{
    TimeSlots.Reset();

    if (EndUTC <= StartUTC || Step.GetTicks() <= 0)
        return;

    // StartTime and EndTime included
    for (FDateTime t = StartUTC; t <= EndUTC; t += Step)
    {
        TimeSlots.Add(t);
    }

    PYRANO_VERBOSE(TEXT("[Scheduler] BuildTimeSlots: %d slot(s) from %s to %s (Δ=%s)"),
        TimeSlots.Num(), *StartUTC.ToIso8601(), *EndUTC.ToIso8601(), *Step.ToString());
}


void UIrradianceScheduler::LaunchNextSimulationCapture(const FSimConfig& Sim)
{
    if (State != ESchedulerState::Capturing) 
        return;
    if (bCaptureInFlight) 
        return;

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


void UIrradianceScheduler::AdvanceTimeSlotOrFinish()
{
    TimeIndex++;
    if (TimeIndex < TimeSlots.Num())
    {
        // Advance time slot
        SetSunSkyUTC(TimeSlots[TimeIndex]); // new solar time
        SensorIndex = 0;
        State = ESchedulerState::Capturing;
        LaunchNextSimulationCapture(BaseSimConfig);
    }
    else
    {
        // End of simulation
        PYRANO_SUCCESS(TEXT("[Scheduler] Simulation completed (TimeSlots=%d, Sensors=%d)"),
            TimeSlots.Num(), Sensors.Num());
        RestoreViewport();
        State = ESchedulerState::Idle;
        Current.Reset();
    }
}

