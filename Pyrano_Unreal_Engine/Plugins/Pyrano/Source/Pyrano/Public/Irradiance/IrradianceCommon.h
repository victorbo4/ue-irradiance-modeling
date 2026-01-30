/*=============================================================================
	IrradianceCommon.h: Shared constants and functions for all Pyrano modules.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"

namespace IrradianceCommon
{
	static constexpr int32 NumFaces = 6;

	namespace Defaults
	{
		/** Cesium Config */
		constexpr bool	bUsingCesium = true;
		constexpr int32 CesiumWarmupFrames = 0;	// 60frames ~ 1s

		 /** Path Tracing */
		constexpr int32 MaxBounces					= 2;		// >2 is not worth it (high cost - low reward)
		constexpr int32 SamplesPerPixel				= 4096;		// if too low, this would cap the warmup frames 
		constexpr int32 EnableReferenceAtmosphere	= 0;

		/** Simulation time estimation */
		constexpr float MsPerFrameRaster = 12.f;
		constexpr float MsPerFramePath	 = 35.f;

		/** Sun visibility samples */
		constexpr int32 SunVisibilitySamples = 8;

		/** Ambient Irradiance Scale factor */
		constexpr float AmbientIrradianceScale = 1.0f;

		/** Sky Factor samples */
		constexpr int32 SVFSamples = 128;

		/** Normalization coefficients */
		constexpr float DirectLinearCoeff = 2.401e-3f;
		constexpr float DirectQuadraticCoeff = 5.0299205e-8f;
		constexpr float AmbientLinearCoeff = 1.3046e-2f;
	}

	namespace Settings
	{
		constexpr bool bApplyIrradianceConfiguration = true;  // true to apply normal config (PT, auto-exposure, etc.)
		constexpr bool bEnableSunVisibility = true;
		constexpr bool bEnableSunOcclusion = true;
	}

	namespace Utils
	{
		/** Returns view rotations for the +X/-X/+Y/-Y/+Z/-Z cubemap faces */
		TArray<FQuat> GenerateCubemapFaceQuats();

		/** Returns true if the world belongs to a PIE client instance */
		bool IsPIEClientWorld(const UWorld* W);

		/** Tries to return a game world suitable for rendering / scheduling */
		UWorld* GetGameWorldSafe();
	}

	namespace Console
	{
		/** Global Console Verbosity Limit */
		PYRANO_API extern ELogVerbosity::Type GConsoleVerbosityLimit; 

		/** Set - Get */
		PYRANO_API ELogVerbosity::Type GetConsoleVerbosityLimit();
		PYRANO_API void SetConsoleVerbosityLimit(ELogVerbosity::Type NewLimit);

		/** Console categories allowed in the widget's console output */
		static const FName ConsoleAllowedCategories[] = 
		{
			TEXT("LogPyrano")
		};

		/**
		 * Returns true if a log entry for the given category / verbosity
		 * should be forwarded to the engine console.
		 */
		inline bool ShouldForwardToConsole(const FName& Category, ELogVerbosity::Type Verbosity)
		{
			// must be inline to avoid LNK2019 error in PyranoEditor module
			if (Verbosity > GetConsoleVerbosityLimit()) 
			{
				return false;
			}
			for (const FName& Allowed : ConsoleAllowedCategories)
			{
				if (Category == Allowed)
				{
					return true;
				}					
			}
			return false;
		}
	}
}