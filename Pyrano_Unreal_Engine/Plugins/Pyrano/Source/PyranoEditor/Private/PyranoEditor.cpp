// PyranoEditor.cpp

#include "PyranoEditor.h"

#include "LevelEditor.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "ToolMenus.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FPyranoEditorModule, PyranoEditor);

void FPyranoEditorModule::StartupModule()
{
    // Register plugin in Tools Menu
#if WITH_EDITOR
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPyranoEditorModule::RegisterMenus));
#endif // WITH_EDITOR

	UE_LOG(LogTemp, Display, TEXT("[PyranoEditor] Module loaded"));
}


void FPyranoEditorModule::ShutdownModule()
{
	UE_LOG(LogTemp, Display, TEXT("[PyranoEditor] Module unloaded"));
}


void FPyranoEditorModule::RegisterMenus()
{
#if WITH_EDITOR
    UToolMenus* ToolMenus = UToolMenus::Get();
    if (!ToolMenus)
        return;

    // "Window" Menu
    UToolMenu* ToolMenu = ToolMenus->ExtendMenu("LevelEditor.MainMenu.Tools");
    if (!ToolMenu)
    {
        return;
    }

    // Create Pyrano section
    FToolMenuSection& Section = ToolMenu->AddSection("Pyrano", FText::FromString("Pyrano"));

    Section.AddMenuEntry(
        "OpenPyranoPlanner",
        FText::FromString("Pyrano Capture Planner"),
        FText::FromString("Open the Pyrano Capture Planner"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FPyranoEditorModule::OpenPyranoPlanner))
    );
#endif // WITH_EDITOR
}


void FPyranoEditorModule::OpenPyranoPlanner()
{
#if WITH_EDITOR
    if (!GEditor)
        return;

    UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
    if (!EditorUtilitySubsystem)
        return;

    // PyranoPlanner Path
    const FSoftObjectPath PlannerPath(TEXT("/Pyrano/EditorUI/Widgets/Pyrano_Capture_Planner.Pyrano_Capture_Planner"));

    UObject* Asset = PlannerPath.TryLoad();
    if (!Asset)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pyrano] Could not load planner widget at path: %s"), *PlannerPath.ToString());
        return;
    }

    UEditorUtilityWidgetBlueprint* PlannerBP = Cast<UEditorUtilityWidgetBlueprint>(Asset);
    if (!PlannerBP)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pyrano] Asset is not an EditorUtilityWidgetBlueprint: %s"), *PlannerPath.ToString());
        return;
    }

    EditorUtilitySubsystem->SpawnAndRegisterTab(PlannerBP);
#endif // WITH_EDITOR
}