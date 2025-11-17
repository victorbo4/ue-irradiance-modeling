// PyranoEditorSubsystem.cpp

#include "PyranoEditorSubsystem.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "UnrealEdGlobals.h"
#include "Settings/LevelEditorPlaySettings.h"

#include "PyranometerComponent.h"
#include "IrradianceCommon.h"
#include "IrradianceScheduler.h" 
#include "IrradianceLog.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

namespace PyranoEditorUtils
{

#if WITH_EDITOR
static void ForceStandalonePIE()
{
    ULevelEditorPlaySettings* PS = GetMutableDefault<ULevelEditorPlaySettings>();
    if (!PS) return;

    // New Editor Window (PIE)
    PS->LastExecutedPlayModeType = EPlayModeType::PlayMode_InEditorFloating;

    // NetMode: Standalone
    PS->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
    PS->bLaunchSeparateServer = false;
    PS->SetPlayNumberOfClients(1);

    PS->PostEditChange();
    PS->SaveConfig();
}
#endif // WITH_EDITOR


static UWorld* GetPIEWorld()
{
    if (!GEngine) 
        return nullptr;

    for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
    {
        if (Ctx.WorldType == EWorldType::PIE)
        {
            return Ctx.World();
        }
    }
    // If not PIE -> null
    return nullptr;
}


static void GetPyranometerComponents(
    UWorld* World,
    bool bOnlyEnabled,
    TArray<UPyranometerComponent*>& OutComponents)
{
    
    // Returns an array of PyranometerComponents
    // if bOnlyEnabled true -> returns only enabled ones

    OutComponents.Reset();
    if (!World || !GEngine) return;

    for (TObjectIterator<UPyranometerComponent> It; It; ++It)
    {
        UPyranometerComponent* Comp = *It;
        if (!Comp)
            continue;

        UWorld* CompWorld = Comp->GetWorld();
        if (!CompWorld || CompWorld != World)
            continue;

        if (bOnlyEnabled && !Comp->bEnabled)
            continue;

        OutComponents.Add(Comp);
    }
}


static int32 CountEnabledSensors(UWorld* World)
{
    TArray<UPyranometerComponent*> Components;
    GetPyranometerComponents(World, /*bOnlyEnabled=*/true, Components);

    return Components.Num();
}


static FText FormatDurationText(const FTimespan& T)
{
    const int32 days = T.GetDays();
    const int32 hours = T.GetHours() % 24;
    const int32 mins = T.GetMinutes() % 60;
    const int32 secs = T.GetSeconds() % 60;

    if (days > 0)
        return FText::FromString(FString::Printf(TEXT("%dd %02dh %02dm"), days, hours, mins));
    if (hours > 0)
        return FText::FromString(FString::Printf(TEXT("%dh %02dm %02ds"), hours, mins, secs));
    if (mins > 0)
        return FText::FromString(FString::Printf(TEXT("%dm %02ds"), mins, secs));

    return FText::FromString(FString::Printf(TEXT("%ds"), secs));
}
}


UWorld* UPyranoEditorSubsystem::GetActiveWorld()
{
    // Returns the current PIE world if available, otherwise the editor world.
    // Used by editor-only features (sensor query, timers before PIE exists, etc.).

    if (!GEngine) 
        return nullptr;

    UWorld* EditorWorld = nullptr;
    for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
    {
        if (Ctx.WorldType == EWorldType::PIE)
            return Ctx.World();                
        if (Ctx.WorldType == EWorldType::Editor)
            EditorWorld = Ctx.World();         
    }

    return EditorWorld;
}


// -----------------------------------------------------------------------------
//  Life Cycle
// -----------------------------------------------------------------------------

void UPyranoEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    InstallPyranoConsoleLogSink();
}


void UPyranoEditorSubsystem::Deinitialize()
{
    // Clean just in case
    FEditorDelegates::PostPIEStarted.RemoveAll(this);
    FEditorDelegates::EndPIE.RemoveAll(this);

    if (UWorld* W = GetActiveWorld())
        W->GetTimerManager().ClearTimer(MonitorTimerHandle);
    
    RemovePyranoConsoleLogSink();
    Super::Deinitialize();
}


// -----------------------------------------------------------------------------
//  PIE Start / Delegates
// -----------------------------------------------------------------------------

void UPyranoEditorSubsystem::StartNewPIE(int32 Width, int32 Height, bool bUseCustomPos, FIntPoint CustomPos)
{
    if (!GEditor) 
        return;

    // Ensure New Window Mode and NM Standalone
    PyranoEditorUtils::ForceStandalonePIE();

    ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
    if (!PlaySettings) 
        return;

    // PIE window dimensions
    PlaySettings->NewWindowWidth = Width;
    PlaySettings->NewWindowHeight = Height;
    if (bUseCustomPos)
    {
        PlaySettings->NewWindowPosition = CustomPos;
    }
    PlaySettings->PostEditChange();
    PlaySettings->SaveConfig();

    // Start PIE
    FRequestPlaySessionParams Params;
    Params.EditorPlaySettings = PlaySettings;
    GEditor->RequestPlaySession(Params);
}


void UPyranoEditorSubsystem::StartPIEWithConfig(const FSimConfig& InConfig, bool bIsSimulation)
{
    PendingSimConfig = InConfig;
    bPendingIsSimulation = bIsSimulation;

    // Clean previous delegates
    FEditorDelegates::PostPIEStarted.RemoveAll(this);
    FEditorDelegates::EndPIE.RemoveAll(this);

    // Re-suscribe
    FEditorDelegates::PostPIEStarted.AddUObject(this, &UPyranoEditorSubsystem::OnPostPIEStarted);
    FEditorDelegates::EndPIE.AddUObject(this, &UPyranoEditorSubsystem::OnEndPIE);

    bStartTriggered = false;
    const int32 SidePx = InConfig.ResolutionPx;

    PYRANO_VERBOSE(TEXT("[Planner] StartPIEWithConfig (%s), res %d"),
        bIsSimulation ? TEXT("Simulation") : TEXT("CaptureOnce"),
        SidePx);

    StartNewPIE(SidePx, SidePx, false, FIntPoint(0, 0));
}


void UPyranoEditorSubsystem::ApplyCVarConfig()
{
    if (!bCVarConfigApplied && IrradianceCommon::Settings::bApplyIrradianceConfiguration)
    {
        FIrradianceRenderConfig::ApplyIrradianceConfig(PendingSimConfig, SavedCVarState);
        bCVarConfigApplied = true;
    }
}


UWorld* UPyranoEditorSubsystem::EnsurePIEWorld()
{
    UWorld* PlayWorld = PyranoEditorUtils::GetPIEWorld();
    if (!PlayWorld)
    {
        PYRANO_VERBOSE(TEXT("[Planner] PostPIEStarted but PIE is not ready (waiting)"));
        if (UWorld* W = GetActiveWorld())
        {
            W->GetTimerManager().SetTimer(
                MonitorTimerHandle,
                [this]() { OnPostPIEStarted(false); },
                0.10f,
                false);
        }

        return nullptr;
    }

    return PlayWorld;
}


bool UPyranoEditorSubsystem::EnsureLocalStandalonePIE(UWorld* PlayWorld) const
{
    const ENetMode NM = PlayWorld->GetNetMode();
    if (!(NM == NM_Standalone || NM == NM_Client))
    {
        PYRANO_VERBOSE(TEXT("[Planner] Not a local/standalone PIE world (NetMode=%d)"),
            static_cast<int32>(NM));
        return false;
    }

    return true;
}


bool UPyranoEditorSubsystem::EnsureViewportReady(UWorld* PlayWorld)
{
    FWorldContext* Ctx = GEngine ? GEngine->GetWorldContextFromWorld(PlayWorld) : nullptr;
    if (!Ctx || !Ctx->GameViewport || !Ctx->GameViewport->Viewport)
    {
        PYRANO_VERBOSE(TEXT("[Planner] PIE world without viewport yet, retrying..."));

        PlayWorld->GetTimerManager().SetTimer(
            MonitorTimerHandle,
            [this]() { OnPostPIEStarted(false); },
            0.10f,
            false);
        return false;
    }

    return true;
}


void UPyranoEditorSubsystem::ScheduleStartAndMonitor(UWorld* PlayWorld)
{
    // Preventive wait so that the first render frame stabilizes SceneColor and resize
    constexpr float BootDelaySec = 2.25f;

    PlayWorld->GetTimerManager().SetTimer(
        MonitorTimerHandle,
        [this]()
        {
            UWorld* PW = PyranoEditorUtils::GetPIEWorld();
            if (!PW) 
                return;

            // Scheduler start
            if (auto* Scheduler = PW->GetSubsystem<UIrradianceScheduler>())
            {
                PYRANO_VERBOSE(TEXT("[Planner] Start after delay"));
                Scheduler->ClearQueue();

                if (bPendingIsSimulation)
                {
                    PYRANO_VERBOSE(TEXT("[Planner] StartSimulation start"));
                    Scheduler->StartSimulation(PendingSimConfig);
                }
                else
                {
                    PYRANO_VERBOSE(TEXT("[Planner] CaptureOnce start"));
                    Scheduler->CaptureOnceForActiveSensors(PendingSimConfig);
                }
            }

            // Monitor the end and close PIE when idle
            PW->GetTimerManager().SetTimer(
                MonitorTimerHandle,
                [this]()
                {
                    UWorld* PW2 = nullptr;
                    if (GEditor)
                    {
                        for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
                        {
                            if (Ctx.WorldType == EWorldType::PIE)
                            {
                                PW2 = Ctx.World();
                                break;
                            }
                        }
                    }

                    if (!PW2) return;

                    if (auto* Scheduler = PW2->GetSubsystem<UIrradianceScheduler>())
                    {
                        if (Scheduler->GetState() == ESchedulerState::Idle)
                        {
                            PW2->GetTimerManager().ClearTimer(MonitorTimerHandle);
                            if (GEditor)
                            {
                                GEditor->RequestEndPlayMap();
                            }
                        }
                    }
                },
                0.20f,
                /*loop=*/true);

        },
        BootDelaySec,
        false);
}


void UPyranoEditorSubsystem::OnPostPIEStarted(bool /*bIsSimulating*/)
{
    // Apply CVar config
    ApplyCVarConfig();

    if (bStartTriggered)
    {
        PYRANO_VERBOSE(TEXT("[Planner] Kickoff ignored (already done)"));
        return;
    }

    bStartTriggered = true;
    FEditorDelegates::PostPIEStarted.RemoveAll(this);
    PYRANO_INFO(TEXT("[Planner] BeginPIE"));

    // CONDITION: PIE started
    UWorld* PlayWorld = EnsurePIEWorld();
    if (!PlayWorld)
        return;

    // CONDITION: Only standalone and client 
    if (!EnsureLocalStandalonePIE(PlayWorld))
        return;

    // CONDITION: Viewport active
    if (!EnsureViewportReady(PlayWorld))
        return;

    // Preventive wait so that the first render frame stabilizes SceneColor and resize
    ScheduleStartAndMonitor(PlayWorld);
}


void UPyranoEditorSubsystem::OnEndPIE(bool /*bIsSimulating*/)
{
    FEditorDelegates::EndPIE.RemoveAll(this);
    bStartTriggered = true;

    OnSimulationEnded.Broadcast();  // Reset StatusBar (widget) from Running to Idle

    // Restore CVar config
    if (bCVarConfigApplied && IrradianceCommon::Settings::bApplyIrradianceConfiguration)
    {
        FIrradianceRenderConfig::RestoreIrradianceConfig(SavedCVarState);
        SavedCVarState.Reset();
        bCVarConfigApplied = false;
    }

    // Clean timer and tmp state
    if (UWorld* W = GetActiveWorld())
    {
        W->GetTimerManager().ClearTimer(MonitorTimerHandle);
    }
    
    PendingSimConfig = FSimConfig{};
}


// -----------------------------------------------------------------------------
//  Capture / Start Simulation
// -----------------------------------------------------------------------------

void UPyranoEditorSubsystem::CaptureOnce(const FSimConfig& InConfig)
{
    StartPIEWithConfig(InConfig, /*bIsSimulation=*/false);
}


void UPyranoEditorSubsystem::StartSimulation(const FSimConfig& InConfig)
{
    StartPIEWithConfig(InConfig, /*bIsSimulation=*/true);
}


// -----------------------------------------------------------------------------
//  Blueprint API
// -----------------------------------------------------------------------------

void UPyranoEditorSubsystem::SetSensorEnabled(const FGuid& SensorGuid, bool bEnabled) const
{
    UWorld* World = GetActiveWorld();
    if (!World) return;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TArray<UPyranometerComponent*> Components;
        It->GetComponents(Components);

        for (UPyranometerComponent* Comp : Components)
        {
            if (Comp && Comp->SensorGuid == SensorGuid)
            {
                Comp->bEnabled = bEnabled;
                return;
            }
        }
    }
}


TArray<FSensorInfo> UPyranoEditorSubsystem::QuerySensors(bool bOnlyEnabled) const
{
    TArray<FSensorInfo> OutSensors;

    UWorld* World = GetActiveWorld();
    if (!World) 
    {
        PYRANO_WARN(TEXT("[Planner] QuerySensors: No valid world found"));
        return OutSensors;
    }

    TArray<UPyranometerComponent*> Components;
    PyranoEditorUtils::GetPyranometerComponents(World, bOnlyEnabled, Components);

    for (UPyranometerComponent* Comp : Components)
    {
        if (!Comp)
            continue;

        FSensorInfo Info;
        Info.SensorGuid = Comp->SensorGuid;
        Info.bEnabled = Comp->bEnabled;

        // Name
        if (Comp->GetOwner())
        {
            Info.Name = Comp->SensorName;
        }
        else
        {
            Info.Name = TEXT("Unnamed");
        }

        Info.PositionWS = Comp->GetComponentLocation();
        Info.NormalWS = Comp->GetNormalWS().GetSafeNormal();

        OutSensors.Add(Info);
    }

    OutSensors.Sort([](const FSensorInfo& A, const FSensorInfo& B) {
        return A.Name < B.Name;
        });

    return OutSensors;
}


bool UPyranoEditorSubsystem::BrowseForOutputFolder(FDirectoryPath& InOutPath)
{
    IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
    if (!Desktop) 
        return false;

    auto* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
    FString OutFolder;

    const bool bSelected = Desktop->OpenDirectoryDialog(ParentWindowHandle, TEXT("Select Output Folder"), InOutPath.Path, OutFolder);

    if (bSelected) InOutPath.Path = OutFolder; 
    return bSelected;
}


bool UPyranoEditorSubsystem::TryParseDateTime(const FString& DateYMD, const FString& TimeHM, FDateTime& OutDT, FString& OutError)
{
    // Enforce "YYYY-MM-DD"
    FRegexPattern PDate(TEXT(R"(^\d{4}-\d{2}-\d{2}$)"));
    FRegexMatcher MDate(PDate, DateYMD);

    // Enforce "HH:MM"
    FRegexPattern PTime(TEXT(R"(^\d{2}:\d{2}$)"));
    FRegexMatcher MTime(PTime, TimeHM);

    if (!MDate.FindNext())
    {
        OutError = TEXT("Date must be YYYY-MM-DD.");
        return false;
    }
    if (!MTime.FindNext())
    {
        OutError = TEXT("Time must be HH:MM.");
        return false;
    }

    // Split and convert
    TArray<FString> DParts; DateYMD.ParseIntoArray(DParts, TEXT("-"), true);
    TArray<FString> TParts; TimeHM.ParseIntoArray(TParts, TEXT(":"), true);

    const int32 Y = FCString::Atoi(*DParts[0]);
    const int32 Mo = FCString::Atoi(*DParts[1]);
    const int32 Da = FCString::Atoi(*DParts[2]);
    const int32 H = FCString::Atoi(*TParts[0]);
    const int32 Mi = FCString::Atoi(*TParts[1]);

    // Date validation
    if (!FDateTime::Validate(Y, Mo, Da, H, Mi, 0, 0))
    {
        OutError = TEXT("Invalid calendar date or time.");
        return false;
    }

    OutDT = FDateTime(Y, Mo, Da, H, Mi);
    return true;
}


bool UPyranoEditorSubsystem::TryParseIntervalString(const FString& In, FTimespan& OutInterval, FString& OutError)
{
    FString S = In;
    S.TrimStartAndEndInline();
    S.ToLowerInline();

    if (S.IsEmpty())
    {
        OutError = TEXT("Empty interval string.");
        return false;
    }

    if (S.EndsWith(TEXT("ms")))
    {
        const FString Num = S.LeftChop(2);
        const double V = FCString::Atod(*Num);
        if (V <= 0) { OutError = TEXT("Interval must be > 0."); return false; }
        OutInterval = FTimespan::FromMilliseconds(V);
        return true;
    }

    const TCHAR Unit = S[S.Len() - 1];
    const FString Num = S.LeftChop(1);
    const double V = FCString::Atod(*Num);
    if (V <= 0) { OutError = TEXT("Interval must be > 0."); return false; }

    switch (Unit)
    {
    case 's': OutInterval = FTimespan::FromSeconds(V);  return true;
    case 'm': OutInterval = FTimespan::FromMinutes(V);  return true;
    case 'h': OutInterval = FTimespan::FromHours(V);    return true;

    default:
        OutError = FString::Printf(TEXT("Unknown unit '%c' (use s, m, or h)."), Unit);
        return false;
    }
}


FValidationResult UPyranoEditorSubsystem::ValidateConfig(const FSimConfig& InConfig)
{
    FValidationResult Result;
    Result.bOk = true;
    Result.OutNormalized = InConfig;

    FSimConfig& out = Result.OutNormalized;

    // Setup basic config
    if (!out.bPathTracing)
    {
        out.WarmupFrames = 8;
    }

    if (!out.bExportCSV)
    {
        out.OutputPath.Path.Empty(); // if export = false, ignore path
    }


    // Validate start < end
    if (out.EndTime <= out.StartTime)
    {
        Result.AddFieldError(TEXT("EndTime"), TEXT("End time must be greater than Start time."));
    }

    if (out.EndTime > out.StartTime)
    {
        const double TotalSeconds = (out.EndTime - out.StartTime).GetTotalSeconds();
        const double StepSeconds = out.SampleInterval.GetTotalSeconds();

        // t0 --> sample +1
        // (ex.: Start = 10:00; End = 10:10, dt = 5m --> floor(600/300)+1 = 3 samples
        int32 NumSamples = 0;
        if (StepSeconds > 0.0)
        {
            NumSamples = FMath::Max(0, static_cast<int32>(FMath::FloorToDouble(TotalSeconds / StepSeconds)) + 1);
        }

        Result.EstimatedSamples = NumSamples;
    }

    // Validate export
    if (out.bExportCSV)
    {
        const FString RawPath = out.OutputPath.Path.TrimStartAndEnd();

        if (RawPath.IsEmpty())
        {
            Result.AddFieldError(TEXT("OutputPath"), TEXT("Output path is empty."));
        }
        else
        {
            FString AbsPath = RawPath;
            FPaths::NormalizeDirectoryName(AbsPath);
            if (FPaths::IsRelative(AbsPath))
            {
                AbsPath = FPaths::ConvertRelativePathToFull(AbsPath);
            }

            // If dir does not exist make it
            if (!IFileManager::Get().DirectoryExists(*AbsPath))
            {
                IFileManager::Get().MakeDirectory(*AbsPath, /*Tree=*/true);
            }
            else
            {
                out.OutputPath.Path = AbsPath;  // Normalize path
            }
            
        }
    }

    // Validate enabled sensors
    UWorld* World = GetActiveWorld();
    const int32 EnabledCount = PyranoEditorUtils::CountEnabledSensors(World);
    if (EnabledCount <= 0)
    {
        Result.AddFieldError(TEXT("Sensors"), TEXT("No enabled sensors found in the current level."));
    }
        

    // Validate SunSky
    bool bHasSunSky = false;
    if (World)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* A = *It;
            if (!A) continue;


            if (A->ActorHasTag(FName("SunSky")) ||
                A->GetName().Contains(TEXT("SunSky")))
            {
                bHasSunSky = true;
                break;
            }
        }
    }
    if (!bHasSunSky)
    {
        Result.AddFieldError(
            TEXT("SunSky"),
            TEXT("No SunSky actor found in the current level.")
        );
    }

    // Estimated simulation time
    FTimespan EstimatedSimTS = EstimateSimDuration(out, Result.EstimatedSamples, EnabledCount, 
        IrradianceCommon::Defaults::MsPerFrameRaster, 
        IrradianceCommon::Defaults::MsPerFramePath);
    Result.EstimatedSimDuration = PyranoEditorUtils::FormatDurationText(EstimatedSimTS);

    Result.bOk = (Result.Errors.Num() == 0);
    return Result;
}


FTimespan UPyranoEditorSubsystem::EstimateSimDuration(
    const FSimConfig& C,
    int32 EstimatedSamples,
    int32 EnabledSensors,
    float MsPerFrameRaster,
    float MsPerFramePath)
{
    if (EstimatedSamples <= 0 || EnabledSensors <= 0)
    {
        return FTimespan::Zero();
    }

    const int32 FacesPerSample = 6;
    const int32 FramesPerFace = C.bPathTracing ? (FMath::Max(1, C.WarmupFrames) + 1) : 1;

    const float resScale = FMath::Square(FMath::Max(1, C.ResolutionPx) / 256.f);
    const float msPerFrameBase = C.bPathTracing ? MsPerFramePath : MsPerFrameRaster;
    const int64 totalFrames = (int64)EstimatedSamples * EnabledSensors * FacesPerSample * FramesPerFace;
    const double totalMsD = FMath::Clamp(
        (double)totalFrames * msPerFrameBase * resScale,
        0.0,
        (double)MAX_int64 
    );
    return FTimespan::FromMilliseconds(totalMsD);
}

