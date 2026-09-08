// PyranoBlueprintLibrary.cpp

#include "UI/PyranoBlueprintLibrary.h"

#include "UI/PlanStorage.h"
#include "Simulation/SimulationConfig.h" // fwd
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"

bool UPyranoBlueprintLibrary::SavePlanWithDialog(const FSimConfig& Config)
{
#if WITH_EDITOR
    IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
    if (!Desktop)
        return false;

    const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

    FString DefaultPath = FPaths::ProjectSavedDir() / TEXT("Irradiance") / TEXT("Pyrano_Plans");
    IFileManager::Get().MakeDirectory(*DefaultPath, /*Tree=*/true);

    TArray<FString> OutFiles;
    const bool bOk = Desktop->SaveFileDialog(
        ParentWindowHandle,
        TEXT("Save Pyrano Plan"),
        DefaultPath,
        TEXT("MyPlan.json"),
        TEXT("Pyrano Plan (*.json)|*.json|All Files (*.*)|*.*"),
        EFileDialogFlags::None,
        OutFiles
    );

    if (!bOk || OutFiles.Num() == 0)
        return false;

    return FPlanStorage::SavePlan(OutFiles[0], Config);
#else
    return false;
#endif // WITH_EDITOR
}


bool UPyranoBlueprintLibrary::LoadPlanWithDialog(FSimConfig& OutConfig)
{
#if WITH_EDITOR
    IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
    if (!Desktop)
        return false;

    const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

    FString DefaultPath = FPaths::ProjectSavedDir() / TEXT("Irradiance") / TEXT("Pyrano_Plans");
    IFileManager::Get().MakeDirectory(*DefaultPath, /*Tree=*/true);

    TArray<FString> OutFiles;
    const bool bOk = Desktop->OpenFileDialog(
        ParentWindowHandle,
        TEXT("Load Pyrano Plan"),
        DefaultPath,
        TEXT(""),
        TEXT("Pyrano Plan (*.json)|*.json|All Files (*.*)|*.*"),
        EFileDialogFlags::None,
        OutFiles
    );

    if (!bOk || OutFiles.Num() == 0)
        return false;

    return FPlanStorage::LoadPlan(OutFiles[0], OutConfig);
#else
    return false;
#endif // WITH_EDITOR
}
