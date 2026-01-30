// PyranoConsoleLogSink.cpp

#include "Logging/PyranoConsoleLogSink.h"
#include "Logging/IrradianceLog.h" 
#include "Irradiance/IrradianceCommon.h"

TQueue<FPyranoConsoleRawLine, EQueueMode::Mpsc> GPyranoConsolePendingLines;
static TUniquePtr<FPyranoConsoleLogSink> GPyranoConsoleSink;

void FPyranoConsoleLogSink::Serialize(const TCHAR* V,
    ELogVerbosity::Type Verbosity,
    const FName& Category)
{
    if (!IrradianceCommon::Console::ShouldForwardToConsole(Category, Verbosity))
        return;
    
    FPyranoConsoleRawLine Line;
    Line.Text = V;
    Line.Verbosity = Verbosity;
    Line.Category = Category;
    Line.Timestamp = FDateTime::UtcNow();

    GPyranoConsolePendingLines.Enqueue(MoveTemp(Line));
}


void InstallPyranoConsoleLogSink()
{
    if (!GPyranoConsoleSink.IsValid() && GLog)
    {
        GPyranoConsoleSink = MakeUnique<FPyranoConsoleLogSink>();
        GLog->AddOutputDevice(GPyranoConsoleSink.Get());
    }
}


void RemovePyranoConsoleLogSink()
{
    if (GPyranoConsoleSink.IsValid() && GLog)
    {
        GLog->RemoveOutputDevice(GPyranoConsoleSink.Get());
        GPyranoConsoleSink.Reset();
    }
}
