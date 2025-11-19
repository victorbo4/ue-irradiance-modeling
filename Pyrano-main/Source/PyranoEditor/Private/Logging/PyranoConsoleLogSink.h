/*=============================================================================
    PyranoConsoleLogSink.h 
  Custom log sink for Pyrano.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

struct FPyranoConsoleRawLine
{
    FString Text;
    ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
    FName Category = NAME_None;
    FDateTime Timestamp = FDateTime::UtcNow();
};

/** Global Queue(Multiple Producer / Consumer). */
extern TQueue<FPyranoConsoleRawLine, EQueueMode::Mpsc> GPyranoConsolePendingLines;

/** Output device linked with GLog. */
class FPyranoConsoleLogSink : public FOutputDevice
{
public:
    virtual void Serialize(const TCHAR* V,
        ELogVerbosity::Type Verbosity,
        const FName& Category) override;
};

// --- Helpers ---

void InstallPyranoConsoleLogSink();
void RemovePyranoConsoleLogSink();
