// IrradianceSubsystem.cpp

#include "IrradianceSubsystem.h"
#include "IrradianceCommon.h"
#include "IrradianceIntegrateCS.h"

#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

#include "Engine.h"

#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SWindow.h"




// ============================================================================
//		FCAPTURECONTEXT
// ============================================================================

#pragma region FCaptureContext

void FCaptureContext::Begin(const FCaptureRequest& InRequest)
{
	Reset();
	Request = InRequest;
	if (FixedFaceRots.Num() == 0)
		FixedFaceRots = IrradianceCommon::Utils::GenerateCubemapFaceQuats();
}

void FCaptureContext::Reset()
{
	Request = FCaptureRequest();
	FaceIndex = 0;
	FacesCollected = 0;

	for (TRefCountPtr<IPooledRenderTarget>& FaceRT : FaceRTs)
	{
		FaceRT.SafeRelease();
	}

	ExtractedIrradianceBuffer.SafeRelease();
	IrradianceReadback.Reset();
	bReadbackEnqueued = false;
}

bool FCaptureContext::IsValid() const {	return Request.IsValid(); }

FVector FCaptureContext::GetPosWS() const { return Request.PosWS; }

FVector FCaptureContext::GetNormalWS() const { return Request.NormalWS; }

int32 FCaptureContext::GetSidePx() const { return Request.SidePx; }

uint32 FCaptureContext::GetWarmup() const { return Request.WarmupFrames; }

const FCaptureRequest& FCaptureContext::GetRequest() const  { return Request; }

const FQuat& FCaptureContext::GetCurrentFaceRot() const { return FixedFaceRots[FaceIndex]; }

bool FCaptureContext::HasMoreFaces() const { return FaceIndex >= 0 && FaceIndex < NumFaces; }

bool FCaptureContext::IsLastFace() const { return FaceIndex >= 0 && FaceIndex == (NumFaces - 1); }

int32 FCaptureContext::GetCurrentFaceIndex() const { return FaceIndex; }

void FCaptureContext::StoreFaceRT(TRefCountPtr<IPooledRenderTarget>&& InRT)
{
	check(FaceIndex >= 0 && FaceIndex < NumFaces);
	FaceRTs[FaceIndex] = MoveTemp(InRT);
	++FacesCollected;
}

void FCaptureContext::AdvanceFace() { check(FaceIndex < NumFaces); ++FaceIndex; }

void FCaptureContext::MarkFacesDone() { FaceIndex = INDEX_NONE; }

bool FCaptureContext::AreFacesReady() const
{
	if (FacesCollected != NumFaces)
		return false;

	for (int32 i = 0; i < NumFaces; ++i)
	{
		if (!FaceRTs[i].IsValid())
			return false;
	}
	return true;
}

void FCaptureContext::GatherFaces(TRefCountPtr<IPooledRenderTarget> OutFaces[NumFaces]) const
{
	for (int32 i = 0; i < NumFaces; ++i)
	{
		OutFaces[i] = FaceRTs[i];
	}
}

void FCaptureContext::ArmFace(FIrradianceViewExtension* VE) const
{
	if (!VE)
		return;

	const uint32 Warmup = FMath::Max<uint32>(1, Request.WarmupFrames);
	const uint32 SidePx = Request.SidePx;
	VE->ArmSingleShot(Warmup, SidePx);
}

#pragma endregion



// ============================================================================
//		LIFE CYCLE
// ============================================================================


void UIrradianceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ViewExt = FSceneViewExtensions::NewExtension<FIrradianceViewExtension>();
	Exporter = nullptr;
}

void UIrradianceSubsystem::Deinitialize()
{
	Super::Deinitialize();
	ViewExt.Reset();
}

bool UIrradianceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	if (!W) return false;
	if (W->WorldType != EWorldType::PIE) return false;

	const ENetMode NM = W->GetNetMode();
	return (NM == NM_Client || NM == NM_Standalone);
}



// ============================================================================
//		VIEWPORT CONTROL
// ============================================================================


bool UIrradianceSubsystem::ForceSquareViewportPIE(int32 SidePx)
{
	UWorld* W = GetWorld();
	if (!IrradianceCommon::Utils::IsPIEClientWorld(W))
	{
		UE_LOG(LogPyrano, Verbose, TEXT("[Viewport] Ignored: not PIE client (World=%p, Type=%d, NM=%d)"),
			W, W ? (int32)W->WorldType : -1, W ? (int32)W->GetNetMode() : -1);
		return false;
	}

	if (!GEngine || !GEngine->GameViewport) return false;
	if (bForcedRes) return false;

	FSceneViewport* SceneVP = GEngine->GameViewport->GetGameViewport();
	PrevViewportSize = SceneVP->GetSize();
	TSharedPtr<SWindow> Win = SceneVP->FindWindow();
	PrevPIEWindow = Win;
	if (Win.IsValid())
	{
		const FVector2D WinSize = Win->GetSizeInScreen();
		PrevPIEWindowSize = { (int32)WinSize.X, (int32)WinSize.Y };
	}

	// Forces 1:1 in viewport render buffer (over SceneColor)
	SceneVP->SetViewportSize(SidePx, SidePx);

	// Resize real PIE window
	if (Win.IsValid())
	{
		Win->Resize(FVector2D(SidePx, SidePx));
		Win->BringToFront(true);
	}

	// Avoid rescaling that may change the render target
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(IrradianceCommon::Utils::GetGameWorldSafe(), 0))
	{
		PC->ConsoleCommand(TEXT("r.ScreenPercentage 100"), true);
		PC->ConsoleCommand(TEXT("r.TemporalAA.Upsampling 0"), true);
	}

	bForcedRes = true;
	UE_LOG(LogPyrano, Display, TEXT("[Viewport] SetViewportSize -> %dx%d"), PrevViewportSize.X, PrevViewportSize.Y);

	return true;

	
}

void UIrradianceSubsystem::RestoreViewportPIE()
{
	if (!bForcedRes) return;
	if (GEngine && GEngine->GameViewport)
	{
		if (FSceneViewport* SceneVP = GEngine->GameViewport->GetGameViewport())
		{
			if (PrevViewportSize.X > 0 && PrevViewportSize.Y > 0)
				SceneVP->SetViewportSize(PrevViewportSize.X, PrevViewportSize.Y);

			if (PrevPIEWindow.IsValid() && PrevPIEWindowSize.X > 0 && PrevPIEWindowSize.Y > 0)
				PrevPIEWindow.Pin()->Resize(FVector2D(PrevPIEWindowSize.X, PrevPIEWindowSize.Y));
		}
	}

	bForcedRes = false;
	PrevPIEWindow.Reset();
	PrevViewportSize = FIntPoint::ZeroValue;
	PrevPIEWindowSize = FIntPoint::ZeroValue;

}



// ============================================================================
//		CAMERA HIJACK
// ============================================================================

void UIrradianceSubsystem::EnsureCaptureCamera()
{
	// Path Tracing only allows one view (player).
	// Solution: "hijack" the player's camera by creating a CameraComponent
	// and putting it in its place.

	if (CaptureCam.IsValid()) return;
	UWorld* W = IrradianceCommon::Utils::GetGameWorldSafe(); 
	if (!W) return;

	CaptureCam = W->SpawnActor<ACameraActor>();

	if (UCameraComponent* Cam = CaptureCam->GetCameraComponent())
	{
		Cam->bConstrainAspectRatio = true;
		Cam->AspectRatio = 1.0f;
		Cam->FieldOfView = 90.f;
	}
}

void UIrradianceSubsystem::EndHijackView()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(IrradianceCommon::Utils::GetGameWorldSafe(), 0))
	{
		if (PrevViewTarget.IsValid())
			PC->SetViewTarget(PrevViewTarget.Get()); 
	}

	PrevViewTarget.Reset();
	bIsHijacked = false;
}




/*==============================================================================
 *                                CAPTURE PROCESS
 *------------------------------------------------------------------------------
 *  This block contains the full sequential capture pipeline:
 *      (1) Begin capture
 *      (2) Per-face handling
 *      (3) GPU integration
 *      (4) Readback
 *      (5) Publishing
 *============================================================================*/

 //--- (0) State Machine --------------------------------------------------------
void UIrradianceSubsystem::Tick(float DeltaTime)
{
	// State machine:
	//  - Capturing: waiting for a face to be delivered by the view extension.
	//  - WaitingReadback: GPU integration finished; request readback.
	//  - Idle: no active capture.

	if (!ViewExt.IsValid())
		return;

	switch (State)
	{
	case ECaptureState::Idle:
		// nothing to do
		break;

	case ECaptureState::Capturing:
		TickCapturing();
		break;

	case ECaptureState::WaitingReadback:
		TickReadback();
		break;
	}
}


 //--- (1) Begin capture --------------------------------------------------------
void UIrradianceSubsystem::StartSixFaceCapture(const FCaptureRequest& Req)
{
	// Begins a full 6-face cubemap capture using the parameters in Req.
	// This only triggers the pipeline; the actual capture happens asynchronously
	// via the view extension and the subsystem Tick() state machine.

	if (!Req.IsValid())
	{
		UE_LOG(LogPyrano, Warning, TEXT("[Subsystem] FCaptureRequest invalid (SidePx<=0 or NormalWS not normalized)"));
		return;
	}

	Capture.Begin(Req);
	StartFaceCapture(Capture.GetPosWS(), Capture.GetCurrentFaceRot());
	State = ECaptureState::Capturing;
}

void UIrradianceSubsystem::StartFaceCapture(const FVector& PosWS, const FQuat& RotWS)
{
	EnsureCaptureCamera();
	if (!CaptureCam.IsValid()) return;

	CaptureCam->SetActorLocation(PosWS);
	CaptureCam->SetActorRotation(RotWS);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(IrradianceCommon::Utils::GetGameWorldSafe(), 0))
	{
		if (!bIsHijacked)
		{
			PrevViewTarget = PC->GetViewTarget();
			bIsHijacked = true;
		}
		PC->SetViewTarget(CaptureCam.Get());
	}

	Capture.ArmFace(ViewExt.Get());
}


//--- (2) Per-face capture ------------------------------------------------------
void UIrradianceSubsystem::TickCapturing()
{

	// Called when the view extension produces a new captured RT (one face).
	// Stores the RT, exports it if requested, and advances to the next face.
	// When all 6 faces are collected, the pipeline proceeds to GPU integration.

	TRefCountPtr<IPooledRenderTarget> Extracted;
	FIntPoint CaptSize;

	if (!ViewExt->TryConsumeCapturedSceneRT(Extracted, CaptSize))
		return;

	const int32 Slot = Capture.GetCurrentFaceIndex();
	if (Slot < 0 || Slot >= Capture.NumFaces)
	{
		UE_LOG(LogPyrano, Warning, TEXT("[Subsystem] Invalid face slot: %d"), Slot);
		return;
	}

	Capture.StoreFaceRT(MoveTemp(Extracted));

	UE_LOG(LogPyrano, Display, TEXT("[Subsystem] Face %d saved (%dx%d). Collected=%d/6"),
		Slot, CaptSize.X, CaptSize.Y, Capture.FacesCollected);

	if (Exporter && ExportOptions.bExportImages)
	{
		Exporter->EnqueueFaceEXR(Capture.Request, Slot, Capture.FaceRTs[Slot], CaptSize);
		UE_LOG(LogPyrano, Display, TEXT("[Subsystem] Image Saved"));
	}

	Capture.AdvanceFace();
	if (Capture.HasMoreFaces())
	{
		StartFaceCapture(Capture.GetPosWS(), Capture.GetCurrentFaceRot());
		return;
	}

	Capture.MarkFacesDone();
	ComputeFinalIrradiance();
	State = ECaptureState::WaitingReadback;
}


//--- (3) GPU integration -------------------------------------------------------
void UIrradianceSubsystem::ComputeFinalIrradiance()
{
	if (!Capture.AreFacesReady())
	{
		UE_LOG(LogPyrano, Error, TEXT("[Subsystem] Could not compute 6 faces, only %d available"), Capture.FacesCollected);
		return;
	}

	// IMPORTANT: capture by value so that TRefCountPtr keeps the textures alive
	TRefCountPtr<IPooledRenderTarget> LocalFaces[FCaptureContext::NumFaces];  // local var to use in the lambda
	Capture.GatherFaces(LocalFaces);

	const int32 Size = Capture.GetSidePx();
	const FVector3f Normal = FVector3f(Capture.GetNormalWS());

	ENQUEUE_RENDER_COMMAND(ComputeIrradianceFromFaces)(
		[this, LocalFaces, Size, Normal](FRHICommandListImmediate& RHICmdList) mutable
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			UE_LOG(LogPyrano, Display, TEXT("[Subsystem] Executing compute shader with Normal=%s, Size=%d"),
				*Normal.ToString(), Size);

			for (int32 i = 0; i < FCaptureContext::NumFaces; ++i)
			{
				if (!LocalFaces[i].IsValid())
				{
					UE_LOG(LogPyrano, Error, TEXT("[Subsystem] Face %d is not valid on Render Thread"), i);
					return;
				}
			}
			// The faces will be registered as external within BuildFacesArray
			// using GraphBuilder.RegisterExternalTexture()

			TRefCountPtr<FRDGPooledBuffer>* ExtractTarget = &this->Capture.ExtractedIrradianceBuffer;
			IrradianceCompute::ComputeAndExtractIrradiance(
				GraphBuilder,
				LocalFaces,
				Size,
				Normal,
				ExtractTarget);
			GraphBuilder.Execute();

			UE_LOG(LogPyrano, Display, TEXT("[Subsystem] Compute shader executed, preparing readback..."));
		});

	Capture.bReadbackEnqueued = true;
}


//--- (4) Readback from GPU -----------------------------------------------------
void UIrradianceSubsystem::TickReadback()
{
	if (Capture.NeedsReadbackEnqueue())
		EnqueueReadbackCopy();

	if (Capture.HasActiveReadback())
	{
		EnqueueReadbackPolling();
		State = ECaptureState::Idle;
	}
}

void UIrradianceSubsystem::EnqueueReadbackCopy()
{
	// Copy buffer to a local variable for RT
	TRefCountPtr<FRDGPooledBuffer> LocalBuf = Capture.ExtractedIrradianceBuffer;

	ENQUEUE_RENDER_COMMAND(EnqueueIrradianceReadback)(
		[this, LocalBuf](FRHICommandListImmediate& RHICmdList)
		{
			if (!Capture.IrradianceReadback)
			{
				Capture.IrradianceReadback = MakeUnique<FRHIGPUBufferReadback>(TEXT("IrradianceReadback"));
			}

			FRHIBuffer* RHIBuf = LocalBuf->GetRHI();
			if (!RHIBuf)
			{
				UE_LOG(LogPyrano, Warning, TEXT("[Subsystem] RHIBuffer not valid for readback"));
				return;
			}

			// Async buffer copy (1 float)
			Capture.IrradianceReadback->EnqueueCopy(RHICmdList, RHIBuf);
			UE_LOG(LogPyrano, Display, TEXT("[Subsystem] Readback enqueued (1 float)"));
		});
}

void UIrradianceSubsystem::EnqueueReadbackPolling()
{
	ENQUEUE_RENDER_COMMAND(ReadIrradianceIfReady)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			if (!Capture.IrradianceReadback)
				return;

			if (!Capture.IrradianceReadback->IsReady())
				return;

			// Read 1 float
			const void* Ptr = Capture.IrradianceReadback->Lock(sizeof(float));
			float Value = 0.0f;
			if (Ptr)
				FMemory::Memcpy(&Value, Ptr, sizeof(float));

			Capture.IrradianceReadback->Unlock();

			// Public result to GT using atomics
			LastIrradianceValue.store(Value, std::memory_order_relaxed);
			bIrradianceValueReady.store(true, std::memory_order_release);

			// Clean GPU resources
			Capture.IrradianceReadback.Reset();
			Capture.ExtractedIrradianceBuffer.SafeRelease();
			Capture.bReadbackEnqueued = false;

			UE_LOG(LogPyrano, Display, TEXT("[Irradiance] Readback completed in RT: %.6f"), Value);
		});
}


//--- (5) Publish irradiance result ---------------------------------------------
bool UIrradianceSubsystem::ConsumeLatestIrradiance(float& OutValue)
{
	if (!bIrradianceValueReady.exchange(false, std::memory_order_acq_rel))
		return false;

	OutValue = LastIrradianceValue.load(std::memory_order_relaxed);

	if (Exporter && ExportOptions.bExportCSV)
	{
		Exporter->AppendIrradianceRow(Capture.GetRequest(), OutValue);
		Exporter->FlushCSVIfNeeded();
	}
	return true;
}

void UIrradianceSubsystem::ConfigureExport(bool bCSV, const FString& OutputDirPath)
{
	if (!Exporter) 
		Exporter = NewObject<UIrradianceExporter>(this);
	
	ExportOptions.bExportCSV = bCSV;
	ExportOptions.OutputDir.Path = OutputDirPath;	// if empty -> Saved/Irradiance

	ExportOptions.bExportImages = IrradianceCommon::Settings::bExportImages;
	if (Exporter) 
		Exporter->Init(ExportOptions);
	
}




