/*=============================================================================
	IrradianceCommon.h: Shared constants and functions for all Pyrano modules.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"

namespace IrradianceCommon
{
	namespace Defaults
	{
		 // Path Tracing
		constexpr int32 MaxBounces = 2;
		constexpr int32 SamplesPerPixel = 4096;
		constexpr int32 EnableReferenceAtmosphere = 0;

		// Simulation Time Estimation
		constexpr float MsPerFrameRaster = 12.f;
		constexpr float MsPerFramePath = 35.f;
	}

	namespace Settings
	{
		 // Debug purposes
		constexpr bool bExportImages = true;
		constexpr bool bApplyIrradianceConfiguration = true;  // true to apply normal config (PT, auto-exposure, etc.)
	}

	namespace Utils
	{
		TArray<FQuat> GenerateCubemapFaceQuats();
		bool IsPIEClientWorld(const UWorld* W);
		UWorld* GetGameWorldSafe();
	}

	namespace Console
	{
		PYRANO_API ELogVerbosity::Type GetConsoleVerbosityLimit();
		PYRANO_API void SetConsoleVerbosityLimit(ELogVerbosity::Type NewLimit);
		static ELogVerbosity::Type GConsoleVerbosityLimit = ELogVerbosity::Log; // global console verbosity limit

		static const FName ConsoleAllowedCategories[] = 
		{
			TEXT("LogPyrano")
		};

		inline bool ShouldForwardToConsole(const FName& Category, ELogVerbosity::Type Verbosity)
		{
			// if not declared inline --> LNK2019 error in PyranoEditor module
			if (Verbosity > GConsoleVerbosityLimit) 
			{
				return false;
			}
			for (const FName& Allowed : ConsoleAllowedCategories)
			{
				if (Category == Allowed)
					return true;
			}
			return false;
		}
	}
}