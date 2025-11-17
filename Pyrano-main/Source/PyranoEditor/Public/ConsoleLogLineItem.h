/*=============================================================================
    ConsoleLogLineItem.h
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ConsoleLogLineItem.generated.h"

UCLASS(BlueprintType)
class PYRANOEDITOR_API UConsoleLogLineItem : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Console")
    FString Text;

    UPROPERTY(BlueprintReadOnly, Category = "Console")
    uint8 Verbosity;

    UPROPERTY(BlueprintReadOnly, Category = "Console")
    FName Category;

    UPROPERTY(BlueprintReadOnly, Category = "Console")
    FDateTime Timestamp;

    void Init(const FString& InText,
        ELogVerbosity::Type InVerbosity,
        const FName& InCategory,
        const FDateTime& InTimestamp)
    {
        Text = InText;
        Verbosity = static_cast<uint8>(InVerbosity);
        Category = InCategory;
        Timestamp = InTimestamp;
    }
};
