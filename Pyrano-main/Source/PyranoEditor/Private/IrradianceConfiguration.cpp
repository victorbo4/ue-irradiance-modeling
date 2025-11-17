// IrradianceConfiguration.cpp

#include "IrradianceConfiguration.h"

#include "IrradianceCommon.h"
#include "HAL/IConsoleManager.h"

// -----------------------------------------------------------------------------
//  Internal Config Tables
// -----------------------------------------------------------------------------

namespace
{
    struct FIntCVarOverride
    {
        const TCHAR* Name;
        int32 Value;
    };

    struct FFloatCVarOverride
    {
        const TCHAR* Name;
        float Value;
    };

    // --- EXPOSURE / LUMINANCE ---

    constexpr FIntCVarOverride GExposureIntCVars[] =
    {
        { TEXT("r.EyeAdaptationQuality"), 0 },
        { TEXT("r.MotionBlurQuality"),    0 },
        { TEXT("r.DepthOfFieldQuality"),  0 },
        { TEXT("r.BloomQuality"),         0 },
        { TEXT("r.LensFlareQuality"),     0 },
        { TEXT("r.LightShaftQuality"),    0 },

        { TEXT("r.DefaultFeature.LensFlare"),          0 },
        { TEXT("r.DefaultFeature.MotionBlur"),         0 },
        { TEXT("r.DefaultFeature.Bloom"),              0 },
        { TEXT("r.DefaultFeature.AmbientOcclusion"),   0 },
        { TEXT("r.DefaultFeature.AutoExposure"),       0 },
        { TEXT("r.DefaultFeature.AutoExposure.Method"),0 },
    };

    constexpr FFloatCVarOverride GExposureFloatCVars[] =
    {
        { TEXT("r.ExposureOffset"), 0.0f },
        { TEXT("r.FilmGrain"),      0.0f },
    };


    // --- ANTI-ALIASING ---

    constexpr FIntCVarOverride GAACVars[] =
    {
        { TEXT("r.AntiAliasingMethod"),   0 },
        { TEXT("r.PostProcessAAQuality"), 0 },
    };


    // --- PATH TRACING BASE ---

    constexpr FIntCVarOverride GPathTracingBaseIntCVars[] =
    {
        { TEXT("r.PathTracing.Enable"),                     1 },
        { TEXT("r.PathTracing.SamplesPerPixel"),            IrradianceCommon::Defaults::SamplesPerPixel },
        { TEXT("r.PathTracing.MaxBounces"),                 IrradianceCommon::Defaults::MaxBounces },
        { TEXT("r.PathTracing.EnableReferenceAtmosphere"),  IrradianceCommon::Defaults::EnableReferenceAtmosphere },

        { TEXT("r.PathTracing.Denoiser"),           0 },
        { TEXT("r.PathTracing.TemporalDenoiser"),   0 },
        { TEXT("r.PathTracing.AdaptiveSampling"),   0 },
        { TEXT("r.PathTracing.FrameIndependentTemporalSeed"), 1 },
    };

    constexpr FFloatCVarOverride GPathTracingFloatCVars[] =
    {
        { TEXT("r.PathTracing.MaxPathIntensity"), 0.0f },
    };


    // -----------------------------------------------------------------------------
    //  Helpers
    // -----------------------------------------------------------------------------

    void SaveAndSetInt(const TCHAR* Name, int32 NewValue, TArray<FIrradianceCVarState>& OutPreviousValues)
    {
        IConsoleManager& CM = IConsoleManager::Get();
        if (IConsoleVariable* CVar = CM.FindConsoleVariable(Name))
        {
            FIrradianceCVarState State;
            State.Name = Name;
            State.IntValue = CVar->GetInt();
            OutPreviousValues.Add(MoveTemp(State));

            CVar->Set(NewValue, ECVF_SetByCode);
        }
    }


    void SaveAndSetFloat(const TCHAR* Name, float NewValue, TArray<FIrradianceCVarState>& OutPreviousValues)
    {
        IConsoleManager& CM = IConsoleManager::Get();
        if (IConsoleVariable* CVar = CM.FindConsoleVariable(Name))
        {
            FIrradianceCVarState State;
            State.Name = Name;
            State.FloatValue = CVar->GetFloat();
            OutPreviousValues.Add(MoveTemp(State));

            CVar->Set(NewValue, ECVF_SetByCode);
        }
    }


    void ApplyGroup(const FIntCVarOverride* Group, int32 Num, TArray<FIrradianceCVarState>& OutPrev)
    {
        for (int32 i = 0; i < Num; ++i)
        {
            SaveAndSetInt(Group[i].Name, Group[i].Value, OutPrev);
        }
    }

    void ApplyGroup(const FFloatCVarOverride* Group, int32 Num, TArray<FIrradianceCVarState>& OutPrev)
    {
        for (int32 i = 0; i < Num; ++i) 
        {
            SaveAndSetFloat(Group[i].Name, Group[i].Value, OutPrev);
        }
    }
}


// -----------------------------------------------------------------------------
//  Apply Configuration
// -----------------------------------------------------------------------------


void FIrradianceRenderConfig::ApplyIrradianceConfig(
    const FSimConfig& Config,
    TArray<FIrradianceCVarState>& OutPreviousValues)
{
    OutPreviousValues.Reset();

    // --- EXPOSURE / LUMINANCE ---
    ApplyGroup(GExposureIntCVars, UE_ARRAY_COUNT(GExposureIntCVars), OutPreviousValues);
    ApplyGroup(GExposureFloatCVars, UE_ARRAY_COUNT(GExposureFloatCVars), OutPreviousValues);

    // --- ANTI-ALIASING ---
    ApplyGroup(GAACVars, UE_ARRAY_COUNT(GAACVars), OutPreviousValues);

    // --- PATH TRACING ---
    if (Config.bPathTracing)
    {
        ApplyGroup(GPathTracingBaseIntCVars, UE_ARRAY_COUNT(GPathTracingBaseIntCVars), OutPreviousValues);
        ApplyGroup(GPathTracingFloatCVars, UE_ARRAY_COUNT(GPathTracingFloatCVars), OutPreviousValues);
    }
    else
    {
        SaveAndSetInt(TEXT("r.PathTracing.Enable"), 0, OutPreviousValues);
    }
}


// -----------------------------------------------------------------------------
// Restore configuration
// -----------------------------------------------------------------------------

void FIrradianceRenderConfig::RestoreIrradianceConfig(const TArray<FIrradianceCVarState>& PreviousValues)
{
    IConsoleManager& CM = IConsoleManager::Get();

    for (const FIrradianceCVarState& State : PreviousValues)
    {
        if (IConsoleVariable* CVar = CM.FindConsoleVariable(*State.Name))
        {
            if (State.IntValue.IsSet())
            {
                CVar->Set(State.IntValue.GetValue(), ECVF_SetByCode);
            }
            else if (State.FloatValue.IsSet())
            {
                CVar->Set(State.FloatValue.GetValue(), ECVF_SetByCode);
            }
        }
    }
}