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

UCLASS()
class PYRANOEDITOR_API UConsoleLogLineEntry : public UUserWidget, public IUserObjectListEntry
{
    GENERATED_BODY()

public:
    // On BP, TextBlock should be named TXT_Line
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
    UMultiLineEditableText* TXT_Line;

protected:
    // IUserObjectListEntry
    virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
