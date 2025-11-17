/*=============================================================================
    ValidationResult.h
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ValidationResult.generated.h"

USTRUCT(BlueprintType)
struct FValidationResult
{
    GENERATED_BODY()

public:
    // True if the configuration is valid 
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    bool bOk = false;

    // Normalized version of the validated config 
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    FSimConfig OutNormalized;

    // List of global error messages 
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    TArray<FString> Errors;

    // FieldName -> Error message 
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    TMap<FString, FString> FieldErrors;

    // Estimated number of samples for UI display 
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    int32 EstimatedSamples = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    FText EstimatedSimDuration = FText::FromString(FString::Printf(TEXT("0d 0m 0s")));

public:
    FValidationResult() {}

    FValidationResult(bool bInOk, const FSimConfig& InConfig)
        : bOk(bInOk), OutNormalized(InConfig) {
    }

    void AddError(const FString& InMessage)
    {
        bOk = false;
        Errors.Add(InMessage);
    }

    void AddFieldError(const FString& Field, const FString& Message)
    {
        bOk = false;
        FieldErrors.Add(Field, Message);
        Errors.Add(Message);
    }

    bool IsValid() const { return bOk && Errors.Num() == 0; }
};
