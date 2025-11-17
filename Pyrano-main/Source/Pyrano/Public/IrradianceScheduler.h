/*=============================================================================
	IrradianceScheduler.h: Manages capture sequencing for sensors and time slots.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimulationConfig.h"
#include "CaptureRequest.h"
#include "SunSkyController.h"
#include "Misc/Optional.h"
#include "IrradianceScheduler.generated.h"

class UIrradianceSubsystem;
class UPyranometerComponent;

enum class ESchedulerState : uint8 { Idle, Capturing };


UCLASS()
class PYRANO_API UIrradianceScheduler : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UIrradianceScheduler() = default;
	~UIrradianceScheduler() = default;

	// Tickable
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UIrradianceScheduler, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

	// Public API
	void CaptureOnceForActiveSensors(const FSimConfig& Sim);
	void StartSimulation(const FSimConfig& Sim);
	ESchedulerState GetState() const { return State; }
	void ClearQueue();

private:
	// Internal
	ESchedulerState State = ESchedulerState::Idle;
	TQueue<FCaptureRequest> Queue;
	TOptional<FCaptureRequest> Current;

	// Simulation
	FSimConfig BaseSimConfig;
	TArray<FDateTime> TimeSlots;
	TArray<UPyranometerComponent*> Sensors;
	int32 TimeIndex = 0;
	int32 SensorIndex = 0;

	// Flags
	bool bViewportForced = false;
	bool bCaptureInFlight = false;  // ← true if capture in progress

	TWeakObjectPtr<UIrradianceSubsystem> Irr;

	// Tick / Flow
	void LaunchNextInQueue();
	void LaunchCapture(const FCaptureRequest& Req);
	void LaunchNextForCurrentTimeSlot(const FSimConfig& Sim);
	void AdvanceTimeSlotOrFinish();
	void OnCaptureCompleted(float IrrValue);

	bool IsSimulationMode() const { return Sensors.Num() > 0 && TimeSlots.Num() > 0; }

	// Subsystem / View / Viewport
	void EnsureSubsystem();
	void EnsureSunLocationOnce(const FSimConfig& Sim);
	void SetSunUTC(const FDateTime& Utc);

	void ApplyViewportForSim(const FSimConfig& Sim);
	void RestoreViewportIfNeeded();

	// Helpers
	void GetActiveSensors(TArray<UPyranometerComponent*>& OutSensors) const;
	FCaptureRequest MakeRequest(const FSimConfig& Sim, UPyranometerComponent* Sensor) const;
	void BuildTimeSlots(const FDateTime& StartUTC, const FDateTime& EndUTC, const FTimespan& Step);

	bool ConsumeIrradianceResult(float& OutValue);
};
