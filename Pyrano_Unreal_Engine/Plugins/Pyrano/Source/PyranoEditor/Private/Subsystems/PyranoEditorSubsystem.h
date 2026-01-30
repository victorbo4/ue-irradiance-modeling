/*=============================================================================
	PyranoEditorSubsystem.h
  Acts as the link between the widget and the runtime simulation workflow.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"

#include "EngineUtils.h"
#include "Engine/EngineTypes.h" 
#include "EditorSubsystem.h"

#include "Logging/PyranoConsoleLogSink.h"
#include "Simulation/CaptureRequest.h"
#include "Data/SensorInfo.h"
#include "Simulation/SimulationConfig.h"   
#include "Data/IrradianceConfiguration.h"
#include "Data/ValidationResult.h"

#include "PyranoEditorSubsystem.generated.h"

/** 
 *  DELEGATE for widget's StatusBar (Running).
 *  The event is captured via blueprint.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPyranoSimulationEnded);

UCLASS()
class PYRANOEDITOR_API UPyranoEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:

	UPyranoEditorSubsystem() = default;
	~UPyranoEditorSubsystem() = default;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

// --- Public API ---

	/** Opens a native directory picker and updates the output folder path. */
	UFUNCTION(BlueprintCallable, Category = "Pyrano|Planner")
	static bool BrowseForOutputFolder(FDirectoryPath& InOutPath);

	/** Returns all pyranometer sensors in the level (filtered by enabled state). */
	UFUNCTION(BlueprintCallable, Category = "Pyrano|Sensors")
	TArray<FSensorInfo> QuerySensors(bool bOnlyEnabled = false) const;

	/** Enables or disables a pyranometer sensor by GUID. */
	UFUNCTION(BlueprintCallable, Category = "Pyrano|Sensors")
	void SetSensorEnabled(const FGuid& SensorGuid, bool bEnabled) const;

	/** Parses YYYY-MM-DD + HH:MM strings into a valid FDateTime. */
    UFUNCTION(BlueprintCallable, Category = "Pyrano|Parsing")
	bool TryParseDateTime(const FString& DateYMD, const FString& TimeHM, FDateTime& OutDT, FString& OutError);

	/** Parses an interval string such as "5m", "10s", "200ms" into a FTimespan. */
	UFUNCTION(BlueprintCallable, Category = "Pyrano|Parsing")
	bool TryParseIntervalString(const FString& In, FTimespan& OutInterval, FString& OutError);

	/** Validates a full simulation configuration and returns a detailed result. */
	UFUNCTION(BlueprintCallable, Category = "Pyrano|Validation")
	FValidationResult ValidateConfig(const FSimConfig& InConfig);

	/** Estimates the total simulation duration from samples, sensors and resolution. */
	UFUNCTION(BlueprintCallable, Category = "Pyrano|Parsing")
	FTimespan EstimateSimDuration(const FSimConfig& C, int32 EstimatedSamples, int32 EnabledSensors, float MsPerFrameRaster /*=12.f*/, float MsPerFramePath  /*=35.f*/);

	/** Starts a single capture pass in PIE and ends automatically. */
	UFUNCTION(BlueprintCallable, Category = "Pyrano|Simulation")
	void CaptureOnce(const FSimConfig& InConfig);

	/** Starts a full multi-sensor, multi-time simulation in PIE. */
	UFUNCTION(BlueprintCallable, Category = "Pyrano|Simulation")
	void StartSimulation(const FSimConfig& InConfig);

	/** Delegate broadcast when the simulation or capture-in-PIE finishes. */
	UPROPERTY(BlueprintAssignable, Category = "Pyrano|Simulation")
	FOnPyranoSimulationEnded OnSimulationEnded;

private:

// --- PIE Control ---

	/** Creates a new standalone PIE window with the given size. */
	void StartNewPIE(int32 Width, int32 Height, bool bUseCustomPos, FIntPoint CustomPos);

	/** Internal callback executed once PIE has fully started. */
	void OnPostPIEStarted(bool bIsSimulating);

	/** Internal callback executed when PIE ends. */
	void OnEndPIE(bool bIsSimulating);

	/** Packs config and registers delegates before launching PIE. */
	void StartPIEWithConfig(const FSimConfig& InConfig, bool bIsSimulation);

// --- Worlds ---

	/** Returns the PIE world if running, otherwise the editor world. */
	static UWorld* GetActiveWorld();

	/** Ensures that the PIE world exists and is ready; retries if not. */
	UWorld* EnsurePIEWorld();

// --- Startup conditions ---

	/** Ensures the PIE world is standalone or client (not server). */
	bool EnsureLocalStandalonePIE(UWorld* PlayWorld) const;

	/** Verifies that the game viewport is initialized and visible. */
	bool EnsureViewportReady(UWorld* PlayWorld);

	/** Delays simulation start and monitors when it finishes. */
	void ScheduleStartAndMonitor(UWorld* PlayWorld);

// --- CVar Handling ---

	/** Applies Render/Irradiance CVars before starting PIE. */
	void ApplyCVarConfig();

	TArray<FIrradianceCVarState> SavedCVarState;
	bool bCVarConfigApplied = false;

// --- Internal state ---

	/** True if the scheduled PIE run should execute a full simulation instead of a single capture. */
	bool bPendingIsSimulation = false;

	/** Guards against triggering the PIE start sequence more than once. */
	bool bStartTriggered = false;

	/** Temporary config stored until PIE is fully ready to start the capture/simulation. */
	FSimConfig PendingSimConfig;

	/** Timer used to poll PIE readiness and monitor simulation completion. */
	FTimerHandle MonitorTimerHandle;
};
