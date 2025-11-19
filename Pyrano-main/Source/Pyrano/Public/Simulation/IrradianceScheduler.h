/*=============================================================================
	IrradianceScheduler.h 
  Manages capture sequencing for sensors and time slots.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Subsystems/WorldSubsystem.h"
#include "Simulation/SimulationConfig.h"
#include "Simulation/CaptureRequest.h"
#include "IrradianceScheduler.generated.h"

class UIrradianceSubsystem;
class UPyranometerComponent;

enum class ESchedulerState : uint8 { Idle, Capturing };

UCLASS()
class PYRANO_API UIrradianceScheduler : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

	UIrradianceScheduler()  = default;
	~UIrradianceScheduler() = default;

// --- Public API ---

	/** Captures irradiance once at Sim.StartTime for all active sensors. */
	void CaptureOnce(const FSimConfig& Sim);

	/** Starts a full time-series simulation using the given configuration. */
	void StartSimulation(const FSimConfig& Sim);

	/** Immediately stops any ongoing capture or simulation and restores state. */
	void ClearQueue();

	/** Returns the current scheduler state. */
	ESchedulerState GetState() const { return State; }

// --- FTickableGameObject Interface ---

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UIrradianceScheduler, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

private:

// --- Subsystem / SunSky ---

	/** Ensures the IrradianceSubsystem pointer is valid for the current world. */
	void EnsureSubsystem();

	/** Applies static SunSky configuration (lat/lon/timezone/northoffset). */
	void ConfigureSunSkyOnce(const FSimConfig& Sim);

	/** Sets the SunSky UTC time. */
	void SetSunSkyUTC(const FDateTime& Utc);

// --- Viewport ---

	/** Forces a square viewport suitable for the simulation/capture. */
	void ApplyViewport(const FSimConfig& Sim);

	/** Restores the viewport if it was previously forced. */
	void RestoreViewport();

// --- Helpers ---

	/** Fills OutSensors with all active pyranometer components in this world. */
	void GetActiveSensors(TArray<UPyranometerComponent*>& OutSensors) const;
	
	/** Builds a capture request for the given sensor and simulation settings. */
	FCaptureRequest MakeRequest(const FSimConfig& Sim, UPyranometerComponent* Sensor) const;

	/** Consumes the latest irradiance value from the subsystem, if available. */
	bool ConsumeIrradianceResult(float& OutValue);

// --- Simulation flow ---

	/** Builds the list of time slots between StartUTC and EndUTC (inclusive). */
	void BuildTimeSlots(const FDateTime& StartUTC, const FDateTime& EndUTC, const FTimespan& Step);

	/** Launches the next capture for the current time slot and sensor index. */
	void LaunchNextSimulationCapture(const FSimConfig& Sim);

	/** Advances to the next time slot, or finalizes the simulation if finished. */
	void AdvanceTimeSlotOrFinish();

// --- CaptureOnce flow ---

	/** Launches the next pending one-shot capture request, or finalizes if none. */
	void LaunchNextOneShotCapture();

// --- Shared flow ---

	/** Prepares SunSky, export and viewport settings for a simulation. */
	void PrepareSimulation(const FSimConfig& Sim, const FDateTime& InitialTimeUTC);

	/** Starts a six-face capture for the given request. */
	void LaunchCapture(const FCaptureRequest& Req);

	/** Handles a completed capture and advances the flow. */
	void OnCaptureCompleted(float IrrValue);

	/** Returns true if running a multi-slot simulation. */
	bool IsSimulationMode() const { return Sensors.Num() > 0 && TimeSlots.Num() > 0; }

private:

	// Internal 
	ESchedulerState State =				 ESchedulerState::Idle;
	TQueue<FCaptureRequest>				 Queue;
	TOptional<FCaptureRequest>			 Current;
	TWeakObjectPtr<UIrradianceSubsystem> Irr;
	TArray<UPyranometerComponent*>		 Sensors;

	// Simulation
	FSimConfig			BaseSimConfig;
	TArray<FDateTime>	TimeSlots;
	int32 TimeIndex   = 0;
	int32 SensorIndex = 0;

	/** Whether we forced the viewport and should restore it afterwards. */	
	bool bViewportForced = false;

	/** Whether there is a capture currently in flight. */
	bool bCaptureInFlight = false;  
};
