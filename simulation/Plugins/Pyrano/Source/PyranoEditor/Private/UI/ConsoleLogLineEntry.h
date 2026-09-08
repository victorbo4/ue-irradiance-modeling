/*=============================================================================
    ConsoleLogLineEntry.h
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Components/MultiLineEditableText.h"
#include "ConsoleLogLineEntry.generated.h"

class UConsoleLogLineItem;

/** Displays a single log line inside the editor log list entry widget. */
UCLASS()
class PYRANOEDITOR_API UConsoleLogLineEntry : public UUserWidget, public IUserObjectListEntry
{
    GENERATED_BODY()

public:

    /** Bound text widget where the final formatted log line is displayed. 
     *  On BP, TextBlock should be named TXT_Line. */
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
    UMultiLineEditableText* TXT_Line;

protected:

    /** Fills the widget when a new list item (log line) is assigned. */    
    virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
