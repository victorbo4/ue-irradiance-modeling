// ConsoleLogWidget.cpp

#include "ConsoleLogWidget.h"

#include "IrradianceLog.h"
#include "IrradianceCommon.h"

#include "Blueprint/UserWidget.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformApplicationMisc.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"

void UConsoleLogWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!LV_Console)
        return;

    // Drain global queue
    FPyranoConsoleRawLine Raw;
    bool bAnyAdded = false;

    while (GPyranoConsolePendingLines.Dequeue(Raw))
    {
        UConsoleLogLineItem* Item = NewObject<UConsoleLogLineItem>(this);
        Item->Init(Raw.Text, Raw.Verbosity, Raw.Category, Raw.Timestamp);

        LV_Console->AddItem(Item);
        bAnyAdded = true;
    }

    if (!bAnyAdded)
        return;
    
    // Max lines limit
    if (MaxLines > 0)
    {
        while (LV_Console->GetNumItems() > MaxLines)
        {
            LV_Console->RemoveItem(LV_Console->GetItemAt(0));
        }
    }

    // Auto-scroll at the end
    if (bAutoScroll && LV_Console->GetNumItems() > 0)
    {
        const int32 LastIndex = LV_Console->GetNumItems() - 1;
        LV_Console->ScrollIndexIntoView(LastIndex);
    }
}


void UConsoleLogWidget::ClearConsole()
{
    if (LV_Console)
    {
        LV_Console->ClearListItems();
    }

    // Clean queue
    FPyranoConsoleRawLine Dummy;
    while (GPyranoConsolePendingLines.Dequeue(Dummy)) {}
}


void UConsoleLogWidget::CopyAll()
{
    if (!LV_Console)
    {
        return;
    }

    TArray<UObject*> Items = LV_Console->GetListItems();
    FString Out;

    for (UObject* Obj : Items)
    {
        if (const UConsoleLogLineItem* Line = Cast<UConsoleLogLineItem>(Obj))
        {
            Out += Line->Text;
            Out += LINE_TERMINATOR;
        }
    }

    if (!Out.IsEmpty())
    {
        FPlatformApplicationMisc::ClipboardCopy(*Out);
    }
}


void UConsoleLogWidget::ToggleShowVerbose()
{
    bShowVerbose = !bShowVerbose;

    if (bShowVerbose)
    {
        IrradianceCommon::Console::SetConsoleVerbosityLimit(ELogVerbosity::VeryVerbose);
        PYRANO_INFO(TEXT("[Console] Verbose logs ENABLED"));
    }
    else
    {
        IrradianceCommon::Console::SetConsoleVerbosityLimit(ELogVerbosity::Log);
        PYRANO_INFO(TEXT("[Console] Verbose logs DISABLED"));
    }
}


void UConsoleLogWidget::ExportLogToFile()
{
    if (!LV_Console)
        return;

    const TArray<UObject*>& Items = LV_Console->GetListItems();
    if (Items.Num() == 0)
    {
        UE_LOG(LogPyrano, Display, TEXT("Console: nothing to export"));
        return;
    }

    // Prepare text
    FString Out;
    for (const UObject* Obj : Items)
    {
        if (const UConsoleLogLineItem* Line = Cast<UConsoleLogLineItem>(Obj))
        {
            Out += Line->Text + LINE_TERMINATOR;
        }
    }

    // Save file dialog
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        UE_LOG(LogPyrano, Error, TEXT("Console: DesktopPlatform not available."));
        return;
    }

    void* ParentWindowHandle = nullptr;
#if WITH_EDITOR
    // Recover main window
    const TSharedPtr<SWindow> MainWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr);
    if (MainWindow.IsValid())
    {
        ParentWindowHandle = MainWindow->GetNativeWindow()->GetOSWindowHandle();
    }
#endif // WITH_EDITOR

    // Default configuration
    FString DefaultPath = FPaths::ProjectSavedDir() / TEXT("Logs");
    FString DefaultFile = TEXT("PyranoLog.txt");
    FString FileTypes = TEXT("Text Files (*.txt)|*.txt|All Files (*.*)|*.*");

    TArray<FString> OutFiles;
    bool bSaved = DesktopPlatform->SaveFileDialog(
        ParentWindowHandle,
        TEXT("Export Pyrano Console Log"),
        DefaultPath,
        DefaultFile,
        FileTypes,
        EFileDialogFlags::None,
        OutFiles
    );

    if (!bSaved || OutFiles.Num() == 0)
    {
        PYRANO_CONSOLE(TEXT("[Console] Export cancelled by user"));
        return;
    }

    FString ChosenPath = OutFiles[0];
    FString Dir = FPaths::GetPath(ChosenPath);
    FString Base = FPaths::GetCleanFilename(ChosenPath);
    FString Short = FPaths::GetCleanFilename(Dir) / Base;

    // Save file
    if (FFileHelper::SaveStringToFile(Out, *ChosenPath))
    {
        PYRANO_CONSOLE(TEXT("[Console] Exported log to %s"), *Short);
    }
    else
    {
        PYRANO_ERR(TEXT("[Console] Failed to export log to %s"), *Short);
    }
}
