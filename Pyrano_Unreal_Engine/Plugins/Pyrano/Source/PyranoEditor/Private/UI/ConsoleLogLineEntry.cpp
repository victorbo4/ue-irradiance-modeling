// ConsoleLogLineEntry.cpp

#include "UI/ConsoleLogLineEntry.h"
#include "UI/ConsoleLogLineItem.h"
#include "Logging/IrradianceLog.h"

void UConsoleLogLineEntry::NativeOnListItemObjectSet(UObject* ListItemObject)
{
    // Calls interface default (--> BP OnListItemObject Event)
    IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

    const UConsoleLogLineItem* Item = Cast<UConsoleLogLineItem>(ListItemObject);
    if (!Item || !TXT_Line)
        return;

    // Text
    TXT_Line->SetIsReadOnly(true);
    TXT_Line->SetText(FText::FromString(Item->Text));

    // Color <-> Verbosity
    FSlateColor Color = FSlateColor(FLinearColor::White);

    switch (static_cast<ELogVerbosity::Type>(Item->Verbosity))
    {
    case ELogVerbosity::Warning:
        Color = FSlateColor(FLinearColor(1.f, 0.8f, 0.2f));
        break;

    case ELogVerbosity::Error:
    case ELogVerbosity::Fatal:
        Color = FSlateColor(FLinearColor::Red);
        break;

    case ELogVerbosity::VeryVerbose:
    case ELogVerbosity::Verbose:
        Color = FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
        break;

    default:

        // Detect success/console msg (shares default verbosity with INFO)
        const FString ConsoleIcon = FString::Printf(TEXT(" %s "), PYRANO_ICON_CONSOLE);
        const FString SuccessIcon = FString::Printf(TEXT(" %s "), PYRANO_ICON_SUCCESS);
      
        const bool bIsSuccess = Item->Text.Contains(SuccessIcon);
        const bool bIsConsole = Item->Text.Contains(ConsoleIcon);

        if (bIsSuccess)
        {
            Color = FSlateColor(FLinearColor(0.2f, 1.0f, 0.2f)); // success -> green
        }
        else if (bIsConsole)
        {
            Color = FSlateColor(FLinearColor(0.85, 0.80, 0.75));  // console -> grey+orange
        }
        else
        {
            Color = FSlateColor(FLinearColor::White); // info -> white
        }
        break;
    }

    // Apply style
    FTextBlockStyle Style = TXT_Line->WidgetStyle;
    Style.ColorAndOpacity = FSlateColor(Color);
    TXT_Line->SetWidgetStyle(Style);
}
