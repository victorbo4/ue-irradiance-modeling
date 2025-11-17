/*=============================================================================
    IrradianceLog.h
/============================================================================*/

#pragma once

#include "CoreMinimal.h"

PYRANO_API DECLARE_LOG_CATEGORY_EXTERN(LogPyrano, Log, All);

// ---------------------------------------------------------
//      TIMESTAMP HELPER (HH:MM:SS.mmm)
// ---------------------------------------------------------
FORCEINLINE FString PyranoTimestamp()
{
    const FDateTime Now = FDateTime::Now();
    return Now.ToString(TEXT("%H:%M:%S.%s"));
}

// ---------------------------------------------------------
//      PREFIXES
// ---------------------------------------------------------
#define PYRANO_ICON_INFO     TEXT("▶")
#define PYRANO_ICON_WARN     TEXT("⚠")
#define PYRANO_ICON_ERR      TEXT("❌")
#define PYRANO_ICON_VERBOSE  TEXT("…")
#define PYRANO_ICON_SUCCESS TEXT("✓")
#define PYRANO_ICON_CONSOLE TEXT("◆")

// ---------------------------------------------------------
//      INTERNAL FORMATTER
// ---------------------------------------------------------
#define _PYRANO_COMPACT_LOG(Verbosity, Icon, Format, ...) \
    UE_LOG( \
        LogPyrano, Verbosity, \
        TEXT("%s %s " Format), \
        *PyranoTimestamp(), \
        Icon, \
        ##__VA_ARGS__ \
    )

// ---------------------------------------------------------
//      PUBLIC MACROS
// ---------------------------------------------------------
#define PYRANO_VERBOSE(Format, ...) _PYRANO_COMPACT_LOG(Verbose,  PYRANO_ICON_VERBOSE, Format, ##__VA_ARGS__)
#define PYRANO_INFO(Format, ...)    _PYRANO_COMPACT_LOG(Display,  PYRANO_ICON_INFO,    Format, ##__VA_ARGS__)
#define PYRANO_WARN(Format, ...)    _PYRANO_COMPACT_LOG(Warning,  PYRANO_ICON_WARN,    Format, ##__VA_ARGS__)
#define PYRANO_ERR(Format, ...)     _PYRANO_COMPACT_LOG(Error,    PYRANO_ICON_ERR,     Format, ##__VA_ARGS__)
#define PYRANO_SUCCESS(Format, ...) _PYRANO_COMPACT_LOG(Display,  PYRANO_ICON_SUCCESS, Format, ##__VA_ARGS__)
#define PYRANO_CONSOLE(Format, ...) _PYRANO_COMPACT_LOG(Display, PYRANO_ICON_CONSOLE, Format, ##__VA_ARGS__)