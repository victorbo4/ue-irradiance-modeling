// PyranoConsoleCommands.cpp

#include "Console/PyranoConsoleCommands.h"

#include "CoreMinimal.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"

#include "Subsystems/PyranoEditorSubsystem.h"
#include "UI/PlanStorage.h"
#include "Simulation/SimulationConfig.h"
#include "Data/ValidationResult.h"
#include "Logging/IrradianceLog.h"

namespace PyranoConsoleCommands
{

namespace
{
    IConsoleObject* RunPlanCommand = nullptr;

    void HandleRunPlan(const TArray<FString>& Args)
    {
        if (Args.Num() < 1)
        {
            PYRANO_ERR(TEXT("Usage: Pyrano.RunPlan <path-to-plan.json>"));
            return;
        }

        if (!GEditor)
        {
            PYRANO_ERR(TEXT("Pyrano.RunPlan requires the editor (GEditor is null)."));
            return;
        }

        UPyranoEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UPyranoEditorSubsystem>();
        if (!Subsystem)
        {
            PYRANO_ERR(TEXT("PyranoEditorSubsystem is not available."));
            return;
        }

        // A bare filename (no path component) resolves against the default
        // plans folder, so `Pyrano.RunPlan wu16.json` works without typing
        // the full path. Anything with a slash is used as given.
        FString PlanPath = Args[0];
        if (FPaths::GetPath(PlanPath).IsEmpty())
        {
            PlanPath = FPaths::ProjectSavedDir() / TEXT("Irradiance/Pyrano_Plans") / PlanPath;
        }

        FSimConfig Config;
        if (!FPlanStorage::LoadPlan(PlanPath, Config))
        {
            PYRANO_ERR(TEXT("Failed to load plan: %s"), *PlanPath);
            return;
        }

        const FValidationResult Result = Subsystem->ValidateConfig(Config);
        if (!Result.IsValid())
        {
            PYRANO_ERR(TEXT("Plan failed validation (%d error(s)):"), Result.Errors.Num());
            for (const FString& Error : Result.Errors)
            {
                PYRANO_ERR(TEXT("  - %s"), *Error);
            }
            return;
        }

        PYRANO_INFO(TEXT("Starting simulation from plan: %s"), *PlanPath);
        Subsystem->StartSimulation(Result.OutNormalized);
    }
}

void Register()
{
    RunPlanCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("Pyrano.RunPlan"),
        TEXT("Loads a Pyrano plan JSON and starts the simulation in PIE. Usage: Pyrano.RunPlan <path-to-plan.json | bare-filename-in-Saved/Irradiance/Pyrano_Plans>"),
        FConsoleCommandWithArgsDelegate::CreateStatic(&HandleRunPlan),
        ECVF_Default
    );
}

void Unregister()
{
    if (RunPlanCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(RunPlanCommand);
        RunPlanCommand = nullptr;
    }
}

} // namespace PyranoConsoleCommands
