/*=============================================================================
    ConsoleLogWidget.h
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ConsoleLogLineItem.h"
#include "PyranoConsoleLogSink.h"
#include "Blueprint/UserWidget.h"
#include "Components/ListView.h"
#include "ConsoleLogWidget.generated.h"

UCLASS()
class PYRANOEDITOR_API UConsoleLogWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // IMPORTANT UMG: ListView: LV_Console
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
    UListView* LV_Console;

    // Auto-scroll enabled by default
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Console")
    bool bAutoScroll = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Console")
    int32 MaxLines = 5000;

    UPROPERTY(BlueprintReadOnly, Category = "Console")
    bool bShowVerbose = false;

    UFUNCTION(BlueprintCallable, Category = "Console")
    void ClearConsole();

    UFUNCTION(BlueprintCallable, Category = "Console")
    void CopyAll();

    UFUNCTION(BlueprintCallable, Category = "Console")
    void ToggleShowVerbose();

    UFUNCTION(BlueprintCallable, Category = "Console")
    void ExportLogToFile();

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
