/*=============================================================================
	PyranoEditorSubsystem.h
  Acts as the link between the widget and the runtime simulation workflow.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"

#include "EngineUtils.h"
#include "Engine/EngineTypes.h" 
#include "EditorSubsystem.h"

#include "PyranoConsoleLogSink.h"
#include "CaptureRequest.h"
#include "SensorInfo.h"
#include "SimulationConfig.h"   
#include "IrradianceConfiguration.h"
#include "ValidationResult.h"

#include "PyranoEditorSubsystem.generated.h"

// DELEGATE for widget's StatusBar (Running)
// The event is captured via blueprint
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

	UFUNCTION(BlueprintCallable, Category = "Pyrano|Planner")
	static bool BrowseForOutputFolder(FDirectoryPath& InOutPath);

	UFUNCTION(BlueprintCallable, Category = "Pyrano|Sensors")
	TArray<FSensorInfo> QuerySensors(bool bOnlyEnabled = false) const;

	UFUNCTION(BlueprintCallable, Category = "Pyrano|Sensors")
	void SetSensorEnabled(const FGuid& SensorGuid, bool bEnabled) const;

    UFUNCTION(BlueprintCallable, Category = "Pyrano|Parsing")
	bool TryParseDateTime(const FString& DateYMD, const FString& TimeHM, FDateTime& OutDT, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Pyrano|Parsing")
	bool TryParseIntervalString(const FString& In, FTimespan& OutInterval, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Pyrano|Validation")
	FValidationResult ValidateConfig(const FSimConfig& InConfig);

	UFUNCTION(BlueprintCallable, Category = "Pyrano|Parsing")
	FTimespan EstimateSimDuration(const FSimConfig& C, int32 EstimatedSamples, int32 EnabledSensors, float MsPerFrameRaster /*=12.f*/, float MsPerFramePath  /*=35.f*/);

	UFUNCTION(BlueprintCallable, Category = "Pyrano|Simulation")
	void CaptureOnce(const FSimConfig& InConfig);

	UFUNCTION(BlueprintCallable, Category = "Pyrano|Simulation")
	void StartSimulation(const FSimConfig& InConfig);

	UPROPERTY(BlueprintAssignable, Category = "Pyrano|Simulation")
	FOnPyranoSimulationEnded OnSimulationEnded;

private:
	// PIE
	void StartNewPIE(int32 Width, int32 Height, bool bUseCustomPos, FIntPoint CustomPos);
	void OnPostPIEStarted(bool bIsSimulating);
	void OnEndPIE(bool bIsSimulating);
	void StartPIEWithConfig(const FSimConfig& InConfig, bool bIsSimulation);

	// PIE Helpers
	static UWorld* GetActiveWorld();
	UWorld* EnsurePIEWorld();

	// OnPostPIE Helpers
	bool EnsureLocalStandalonePIE(UWorld* PlayWorld) const;
	bool EnsureViewportReady(UWorld* PlayWorld);
	void ScheduleStartAndMonitor(UWorld* PlayWorld);

	// Internal
	bool bPendingIsSimulation = false;
	bool bStartTriggered = false;
	FSimConfig PendingSimConfig;
	FTimerHandle MonitorTimerHandle;

	// CVar Config
	void ApplyCVarConfig();
	TArray<FIrradianceCVarState> SavedCVarState;
	bool bCVarConfigApplied = false;
};
