// IrradianceSubsystem.cpp

#include "Subsystems/IrradianceSubsystem.h"

#include "Engine.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PyranometerComponent.h"
#include "RHICommandList.h" // fwd
#include "RenderGraphBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"

#include "Irradiance/IrradianceCommon.h"
#include "Irradiance/IrradianceIntegrateCS.h"
#include "Subsystems/SunSkyController.h"
#include "Logging/IrradianceLog.h"

#include "Slate/SceneViewport.h"
#include "Widgets/SWindow.h"

// -----------------------------------------------------------------------------
//  FCaptureContext
// -----------------------------------------------------------------------------

void FCaptureContext::Begin(const FCaptureRequest& InRequest)
{
	Reset();
	Request = InRequest;

	// Build cubemap orientations
	if (FixedFaceRots.Num() == 0)
	{
		FixedFaceRots = IrradianceCommon::Utils::GenerateCubemapFaceQuats();
	}
}


void FCaptureContext::Reset()
{
	Request = FCaptureRequest();
	FaceIndex = 0;
	FacesCollected = 0;

	// Release stored faces
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

// -- Face management ---------------------------------------------------------

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

	// Configure the view extension to perform a single capture
	const uint32 Warmup = FMath::Max<uint32>(1, Request.WarmupFrames);
	uint32 ExtraWarmup = 0;

	if (IrradianceCommon::Defaults::bUsingCesium && FaceIndex == 0)
	{
		ExtraWarmup = IrradianceCommon::Defaults::CesiumWarmupFrames;
	}

	const uint32 TotalWarmup = Warmup + ExtraWarmup;
	const uint32 SidePx = Request.SidePx;
	VE->ArmSingleShot(TotalWarmup, SidePx);
}


// -----------------------------------------------------------------------------
//  Life Cycle
// -----------------------------------------------------------------------------

void UIrradianceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ViewExt = FSceneViewExtensions::NewExtension<FIrradianceViewExtension>();
	Exporter = nullptr;
	if (!ClearSky)
	{
		ClearSky = NewObject<UClearSkyService>(this);
		FPyranoClearSkyConfig CS;
		CS.AltitudeMeters = 650.0;
		CS.LinkeTurbidity = 2.0;
		ClearSky->Configure(CS);
	}
}


void UIrradianceSubsystem::Deinitialize()
{
	Super::Deinitialize();
	ViewExt.Reset();
}


bool UIrradianceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// This function prevents duplicated subsystems

	const UWorld* W = Cast<UWorld>(Outer);
	if (!W) 
		return false;

	// Only in PIE Worlds
	if (W->WorldType != EWorldType::PIE) 
		return false;

	// Only client PIE (not server)
	const ENetMode NM = W->GetNetMode();
	return (NM == NM_Client || NM == NM_Standalone);
}


// -----------------------------------------------------------------------------
//  Viewport Control
// -----------------------------------------------------------------------------

bool UIrradianceSubsystem::ForceSquareViewportPIE(int32 SidePx)
{
	UWorld* W = GetWorld();
	if (!IrradianceCommon::Utils::IsPIEClientWorld(W))
	{
		PYRANO_VERBOSE(TEXT("[Viewport] Ignored: not PIE client (World=%p, Type=%d, NM=%d)"),
			W, W ? (int32)W->WorldType : -1, W ? (int32)W->GetNetMode() : -1);
		return false;
	}

	if (!GEngine || !GEngine->GameViewport) 
		return false;

	if (bForcedRes) 
		return false;

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
	PYRANO_VERBOSE(TEXT("[Viewport] SetViewportSize -> %dx%d"), PrevViewportSize.X, PrevViewportSize.Y);

	return true;
}


void UIrradianceSubsystem::RestoreViewportPIE()
{
	if (!bForcedRes) 
		return;

	if (GEngine && GEngine->GameViewport)
	{
		if (FSceneViewport* SceneVP = GEngine->GameViewport->GetGameViewport())
		{
			if (PrevViewportSize.X > 0 && PrevViewportSize.Y > 0)
			{
				SceneVP->SetViewportSize(PrevViewportSize.X, PrevViewportSize.Y);
			}

			if (PrevPIEWindow.IsValid() && PrevPIEWindowSize.X > 0 && PrevPIEWindowSize.Y > 0)
			{
				PrevPIEWindow.Pin()->Resize(FVector2D(PrevPIEWindowSize.X, PrevPIEWindowSize.Y));
			}
		}
	}

	bForcedRes = false;
	PrevPIEWindow.Reset();
	PrevViewportSize = FIntPoint::ZeroValue;
	PrevPIEWindowSize = FIntPoint::ZeroValue;
}


// -----------------------------------------------------------------------------
//  Camera Hijack
// -----------------------------------------------------------------------------

void UIrradianceSubsystem::EnsureCaptureCamera()
{
	/** Path Tracing only allows one view(player).
	 *  Solution: "hijack" the player's camera by creating a CameraComponent
	 *  and putting it in its place. */
	
	if (CaptureCam.IsValid()) 
		return;
	UWorld* W = IrradianceCommon::Utils::GetGameWorldSafe(); 
	if (!W) 
		return;

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
		{
			PC->SetViewTarget(PrevViewTarget.Get());
		}
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
		PYRANO_WARN(TEXT("[Subsystem] FCaptureRequest invalid (SidePx<=0 or NormalWS not normalized)"));
		return;
	}

	Capture.Begin(Req);
	StartFaceCapture(Capture.GetPosWS(), Capture.GetCurrentFaceRot());
	State = ECaptureState::Capturing;
}


void UIrradianceSubsystem::StartFaceCapture(const FVector& PosWS, const FQuat& RotWS)
{
	EnsureCaptureCamera();
	if (!CaptureCam.IsValid()) 
		return;

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
		PYRANO_WARN(TEXT("[Subsystem] Invalid face slot: %d"), Slot);
		return;
	}

	Capture.StoreFaceRT(MoveTemp(Extracted));
	PYRANO_VERBOSE(TEXT("[Subsystem] Face %d saved (%dx%d). Collected=%d/6"),
		Slot, CaptSize.X, CaptSize.Y, Capture.FacesCollected);

	if (Exporter && ExportOptions.bExportImages)
	{
		Exporter->EnqueueFaceEXR(Capture.Request, Slot, Capture.FaceRTs[Slot], CaptSize);
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
		PYRANO_ERR(TEXT("[Subsystem] Could not compute 6 faces, only %d available"), Capture.FacesCollected);
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

			PYRANO_VERBOSE(TEXT("[Subsystem] Executing compute shader with Normal=%s, Size=%d"),
				*Normal.ToString(), Size);

			for (int32 i = 0; i < FCaptureContext::NumFaces; ++i)
			{
				if (!LocalFaces[i].IsValid())
				{
					PYRANO_ERR(TEXT("[Subsystem] Face %d is not valid on Render Thread"), i);
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

			PYRANO_VERBOSE(TEXT("[Subsystem] Compute shader executed, preparing readback..."));
		});

	Capture.bReadbackEnqueued = true;
}


//--- (4) Readback from GPU -----------------------------------------------------

void UIrradianceSubsystem::TickReadback()
{
	if (Capture.NeedsReadbackEnqueue())
	{
		EnqueueReadbackCopy();
	}

	if (Capture.HasActiveReadback())
	{
		EnqueueReadbackPolling();
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
				PYRANO_WARN(TEXT("[Subsystem] RHIBuffer not valid for readback"));
				return;
			}

			// Async buffer copy (1 float)
			Capture.IrradianceReadback->EnqueueCopy(RHICmdList, RHIBuf);
			PYRANO_VERBOSE(TEXT("[Subsystem] Readback enqueued (1 float)"));
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

			// Read 4 float
			const void* Ptr = Capture.IrradianceReadback->Lock(sizeof(FVector4f));
			FVector4f Value(0, 0, 0, 0);
			if (Ptr)
			{
				FMemory::Memcpy(&Value, Ptr, sizeof(FVector4f));
			}

			Capture.IrradianceReadback->Unlock();

			// Public result to GT using atomics
			LastIrradianceR.store(Value.X, std::memory_order_relaxed);
			LastIrradianceG.store(Value.Y, std::memory_order_relaxed);
			LastIrradianceB.store(Value.Z, std::memory_order_relaxed);
			LastIrradianceMean.store(Value.W, std::memory_order_relaxed);

			bIrradianceValueReady.store(true, std::memory_order_release);

			// Clean GPU resources
			Capture.IrradianceReadback.Reset();
			Capture.ExtractedIrradianceBuffer.SafeRelease();
			Capture.bReadbackEnqueued = false;

			PYRANO_VERBOSE(TEXT("[Irradiance] Readback completed in RT: %.6f"), Value.W);
		});
}


//--- (5) Publish irradiance result ---------------------------------------------


bool UIrradianceSubsystem::ConsumeLatestIrradiance(float& OutValue, float MinSunAltitude)
{
	if (!bIrradianceValueReady.exchange(false, std::memory_order_acq_rel))
		return false;

	float IrrMean = LastIrradianceMean.load(std::memory_order_relaxed);
	float IrrR = LastIrradianceR.load(std::memory_order_relaxed);
	float IrrG = LastIrradianceG.load(std::memory_order_relaxed);
	float IrrB = LastIrradianceB.load(std::memory_order_relaxed);

	float AmbientIrradiance = IrrMean * IrradianceCommon::Defaults::AmbientIrradianceScale;
	float TotalIrradiance = IrrMean;

	float DirectIrradiance = 0.0f;
	float GeometricFactor = 0.0f;

	float AzDeg = 0.f;
	float AltDeg = 0.f;

	int32 SunOccluded = -1;
	float SunHitDistanceM = -1.0f;
	float SunVisibility = 0.0f;

	FPyranoClearSkyIrradiance ClearSkyRef; 

	UWorld* W = GetWorld();
	if (W)
	{
		USunSkyController* SunController = W->GetSubsystem<USunSkyController>();
		if (SunController)
		{
			float TmpAzDeg = 0.f;
			float TmpAltDeg = 0.f;

			if (SunController->GetSolarAngles(TmpAzDeg, TmpAltDeg))
			{
				AzDeg = TmpAzDeg;
				AltDeg = TmpAltDeg;

				// Timestamp (UTC) from the capture request
				const FDateTime WhenUTC = Capture.GetRequest().TimestampUTC; // ajusta el nombre si difiere

				const float AltDegPhys = FMath::Clamp(AltDeg, -90.0f, 90.0f);
				if (AltDeg != AltDegPhys)
				{
					PYRANO_WARN(TEXT("[SunSky] AltDeg out of range: %.3f -> %.3f"), AltDeg, AltDegPhys);
				}

				if (AltDegPhys > 0.0f && ClearSky)
				{
					const double SolarZenithDeg = 90.0 - static_cast<double>(AltDegPhys);
					const double SolarZenithRad = FMath::DegreesToRadians(SolarZenithDeg);
					ClearSkyRef = ClearSky->Compute(WhenUTC, SolarZenithRad);
				}

				// Compute SunVisibility / Occlusion
				if (AltDeg > MinSunAltitude)
				{
					if (IrradianceCommon::Settings::bEnableSunVisibility)
					{
						SunVisibility = ComputeSunVisibility(
							Capture.GetRequest(),
							IrradianceCommon::Defaults::SunVisibilitySamples);
					}

					else if (IrradianceCommon::Settings::bEnableSunOcclusion)
					{
						bool bOcc = false;
						ComputeSunOcclusion(Capture.GetRequest(), bOcc, SunHitDistanceM);
						SunVisibility = bOcc ? 0.0f : 1.0f;
						SunOccluded = bOcc ? 1 : 0;
					}

					else
					{
						SunVisibility = 1.0f;
					}

					if (SunVisibility > 0.001f)
					{
						UDirectionalLightComponent* SunLightComp = nullptr;
						if (AActor* SunActor = SunController->FindSunSkyActor(W))
						{
							SunLightComp = SunActor->FindComponentByClass<UDirectionalLightComponent>();
						}

						if (SunLightComp)
						{
							float SunLux = SunLightComp->Intensity;
							FVector SunDir = -SunLightComp->GetForwardVector();

							FVector SensorNormal = Capture.GetNormalWS();
							float Dot = FVector::DotProduct(SensorNormal, SunDir);

							if (Dot > 0.0f)
							{
								DirectIrradiance = SunLux * Dot * SunVisibility;
								GeometricFactor = Dot * SunVisibility;
							}
						}
						else
						{
							PYRANO_WARN(TEXT("[Subsystem] Could not get Directional Light"));
						}
					}
				}
				// Altitude <= MinSunAltitude
				else
				{
					AmbientIrradiance = 0.0f;
				}

				float DirectLinearCoeff = IrradianceCommon::Defaults::DirectLinearCoeff;
				float DirectQuadraticCoeff = IrradianceCommon::Defaults::DirectQuadraticCoeff;
				float AmbientLinearCoeff = IrradianceCommon::Defaults::AmbientLinearCoeff;

				TotalIrradiance = (DirectLinearCoeff * DirectIrradiance)
					+ (DirectQuadraticCoeff * FMath::Square(DirectIrradiance))
					+ (AmbientLinearCoeff * AmbientIrradiance);
			}
			else
			{
				PYRANO_WARN(TEXT("[SunSky] SunSkyController could not provide solar angles"));
			}
		}
		else
		{
			PYRANO_WARN(TEXT("[Irradiance] SunSkyController subsystem not found"));
		}
	}

	OutValue = TotalIrradiance;

	if (Exporter && ExportOptions.bExportCSV)
	{
		if (!IsValid(Exporter))
		{
			PYRANO_WARN(TEXT("[Subsystem] Exporter invalid (GC'ed or not initialized). Skipping CSV export."));
			return true; // irradiance is valid, only export failed
		}

		if (SunHitDistanceM < 0.0f && IrradianceCommon::Settings::bEnableSunOcclusion)
		{
			bool bOcc = false;
			ComputeSunOcclusion(Capture.GetRequest(), bOcc, SunHitDistanceM);
			SunOccluded = bOcc ? 1 : 0;
		}

		Exporter->AppendIrradianceRow(Capture.GetRequest(), TotalIrradiance,
			IrrMean, IrrR, IrrG, IrrB, 
			ClearSkyRef.GHI_Wm2, ClearSkyRef.DNI_Wm2, ClearSkyRef.DHI_Wm2, 
			DirectIrradiance,
			AzDeg, AltDeg, GeometricFactor,
			SunOccluded, SunHitDistanceM, SunVisibility);
		Exporter->FlushCSVIfNeeded(false);
	}
	Capture.Reset();
	State = ECaptureState::Idle;
	return true;
}


bool UIrradianceSubsystem::ComputeSunOcclusion(
	const FCaptureRequest& Req,
	bool& bOutSunOccluded,
	float& OutHitDistanceM) const
{
	bOutSunOccluded = false;
	OutHitDistanceM = -1.0f;

	UWorld* World = GetWorld();
	if (!World)
		return false;

	// 1) Find SunSky actor (tag "SunSky" or name contains "SunSky")
	AActor* SunSkyActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;

		if (A->ActorHasTag(FName("SunSky")) || A->GetName().Contains(TEXT("SunSky")))
		{
			SunSkyActor = A;
			break;
		}
	}
	if (!SunSkyActor)
	{
		PYRANO_WARN(TEXT("[Occlusion] SunSky not found"));
		return false;
	}

	// 2) Get the sun directional light component from SunSky
	UDirectionalLightComponent* SunDirComp = SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();

	// Fallback: SunSky may own a child actor that is the directional light
	if (!SunDirComp)
	{
		TArray<UChildActorComponent*> ChildActorComps;
		SunSkyActor->GetComponents<UChildActorComponent>(ChildActorComps);

		for (UChildActorComponent* CAC : ChildActorComps)
		{
			if (!CAC) continue;

			AActor* Child = CAC->GetChildActor();
			if (ADirectionalLight* DL = Cast<ADirectionalLight>(Child))
			{
				SunDirComp = Cast<UDirectionalLightComponent>(DL->GetLightComponent());
				if (SunDirComp) break;
			}
		}
	}

	if (!SunDirComp)
	{
		PYRANO_WARN(TEXT("[Occlusion] Could not find Directional Light component on/under SunSky"));
		return false;
	}

	// 3) Direction from point towards the sun: opposite of light forward
	const FVector ToSunDir = (-SunDirComp->GetForwardVector()).GetSafeNormal();
	if (ToSunDir.IsNearlyZero())
	{
		PYRANO_WARN(TEXT("[Occlusion] Sun direction is nearly zero"));
		return false;
	}

	// 4) Resolve sensor owner (so we can ignore self hits)
	AActor* SensorOwner = nullptr;
	for (TObjectIterator<UPyranometerComponent> It; It; ++It)
	{
		UPyranometerComponent* C = *It;
		if (!C || C->GetWorld() != World) continue;

		if (C->SensorGuid == Req.SensorId)
		{
			SensorOwner = C->GetOwner();
			break;
		}
	}

	// 5) Trace setup
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Pyrano_SunOcclusion), /*bTraceComplex*/ false);
	Params.bReturnPhysicalMaterial = false;
	Params.bFindInitialOverlaps = false;

	Params.AddIgnoredActor(SunSkyActor);
	if (SensorOwner) Params.AddIgnoredActor(SensorOwner);
	if (CaptureCam.IsValid()) Params.AddIgnoredActor(CaptureCam.Get());

	// Use a robust start: push along trace direction (main) + a small normal offset (secondary)
	const FVector N = Req.NormalWS.GetSafeNormal(); // if (0,0,1) it's fine; if zero -> (0,0,0)
	const FVector Start0 = Req.PosWS + ToSunDir * 50.0f + N * 5.0f; // 50 cm to sun + 5 cm up
	const float TraceLenCm = 10000000.0f;                           // 100 km in cm
	const FVector End0 = Start0 + ToSunDir * TraceLenCm;

	FHitResult Hit;
	bool bHit = World->LineTraceSingleByChannel(Hit, Start0, End0, ECC_Visibility, Params);

	// Retry if we got a degenerate hit at the start (distance 0 / penetrating)
	if (bHit && (Hit.Distance <= KINDA_SMALL_NUMBER || Hit.bStartPenetrating))
	{
		const FVector Start1 = Start0 + ToSunDir * 200.0f; // move 2m further toward sun
		const FVector End1 = Start1 + ToSunDir * TraceLenCm;

		Hit = FHitResult();
		bHit = World->LineTraceSingleByChannel(Hit, Start1, End1, ECC_Visibility, Params);
	}

	// Optional debug
	if (bHit && (Hit.Distance <= KINDA_SMALL_NUMBER || Hit.bStartPenetrating))
	{
		PYRANO_WARN(
			TEXT("[Occlusion] ZeroDist hit persists. StartPen=%d Actor=%s Comp=%s"),
			Hit.bStartPenetrating ? 1 : 0,
			Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("None"),
			Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("None")
		);
	}

	if (bHit)
	{
		bOutSunOccluded = true;
		OutHitDistanceM = Hit.Distance / 100.0f; // cm -> m
	}
	else
	{
		bOutSunOccluded = false;
		OutHitDistanceM = -1.0f;
	}

	return true;
}


float UIrradianceSubsystem::ComputeSunVisibility(
	const FCaptureRequest& Req,
	int32 NumSamples) const
{
	if (NumSamples <= 0)
		return 0.0f;

	UWorld* World = GetWorld();
	if (!World)
		return 0.0f;

	// Find SunSky
	AActor* SunSkyActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		if (A->ActorHasTag(FName("SunSky")) || A->GetName().Contains(TEXT("SunSky")))
		{
			SunSkyActor = A;
			break;
		}
	}
	if (!SunSkyActor)
		return 0.0f;

	UDirectionalLightComponent* SunDirComp = SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
	if (!SunDirComp)
	{
		TArray<UChildActorComponent*> CACs;
		SunSkyActor->GetComponents<UChildActorComponent>(CACs);
		for (UChildActorComponent* CAC : CACs)
		{
			if (ADirectionalLight* DL = Cast<ADirectionalLight>(CAC->GetChildActor()))
			{
				SunDirComp = Cast<UDirectionalLightComponent>(DL->GetLightComponent());
				if (SunDirComp) break;
			}
		}
	}
	if (!SunDirComp)
		return 0.0f;

	const FVector ToSunDir = (-SunDirComp->GetForwardVector()).GetSafeNormal();
	if (ToSunDir.IsNearlyZero())
		return 0.0f;

	// Resolve sensor owner (ignore self)
	AActor* SensorOwner = nullptr;
	for (TObjectIterator<UPyranometerComponent> It; It; ++It)
	{
		UPyranometerComponent* C = *It;
		if (!C || C->GetWorld() != World) continue;
		if (C->SensorGuid == Req.SensorId)
		{
			SensorOwner = C->GetOwner();
			break;
		}
	}

	// Trace params
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Pyrano_SunVisibility), false);
	Params.bReturnPhysicalMaterial = false;
	Params.bFindInitialOverlaps = false;
	Params.AddIgnoredActor(SunSkyActor);
	if (SensorOwner) Params.AddIgnoredActor(SensorOwner);
	if (CaptureCam.IsValid()) Params.AddIgnoredActor(CaptureCam.Get());

	// Robust start (igual que en sun_occluded)
	const FVector N = Req.NormalWS.GetSafeNormal();
	const FVector Start = Req.PosWS + ToSunDir * 50.0f + N * 5.0f;
	const float TraceLenCm = 10000000.0f;

	// --- Sampling ---
	int32 ClearCount = 0;

	// Cone semi-angle (0.265 degrees ≈ solar angular radius)
	const float ConeAngleRad = FMath::DegreesToRadians(0.5f);

	// Determinist seed
	const int32 Seed =
		GetTypeHash(Req.SensorId) ^
		GetTypeHash(Req.TimestampUTC.GetTicks());

	FRandomStream RNG(Seed);

	for (int32 i = 0; i < NumSamples; ++i)
	{
		// Uniform sampling of a cone
		const float U = RNG.GetFraction();
		const float V = RNG.GetFraction();

		const float Theta = 2.0f * PI * U;
		const float CosPhi = 1.0f - V * (1.0f - FMath::Cos(ConeAngleRad));
		const float SinPhi = FMath::Sqrt(1.0f - CosPhi * CosPhi);

		// Local vector around +Z
		const FVector LocalDir(
			SinPhi * FMath::Cos(Theta),
			SinPhi * FMath::Sin(Theta),
			CosPhi
		);

		// Rotate to align +Z with ToSunDir 
		const FQuat Rot = FQuat::FindBetweenNormals(FVector::UpVector, ToSunDir);
		const FVector SampleDir = Rot.RotateVector(LocalDir);

		const FVector End = Start + SampleDir * TraceLenCm;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(
			Hit, Start, End, ECC_Visibility, Params);

		if (!bHit)
		{
			ClearCount++;
		}
	}

	return float(ClearCount) / float(NumSamples);
}


float UIrradianceSubsystem::ComputeSkyViewFactor(
	const FCaptureRequest& Req,
	int32 NumSamples) const
{
	if (NumSamples <= 0) return 0.0f;

	UWorld* World = GetWorld();
	if (!World) return 0.0f;

	const FVector N = Req.NormalWS.GetSafeNormal();
	if (N.IsNearlyZero()) return 0.0f;

	// Find SunSky
	AActor* SunSkyActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		if (A->ActorHasTag(FName("SunSky")) || A->GetName().Contains(TEXT("SunSky")))
		{
			SunSkyActor = A;
			break;
		}
	}

	// Same as SunVisibility
	AActor* SensorOwner = nullptr;
	for (TObjectIterator<UPyranometerComponent> It; It; ++It)
	{
		UPyranometerComponent* C = *It;
		if (!C || C->GetWorld() != World) continue;
		if (C->SensorGuid == Req.SensorId)
		{
			SensorOwner = C->GetOwner();
			break;
		}
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(Pyrano_SkyViewFactor), false);
	Params.bReturnPhysicalMaterial = false;
	Params.bFindInitialOverlaps = false;

	if (SunSkyActor)   Params.AddIgnoredActor(SunSkyActor);
	if (SensorOwner)  Params.AddIgnoredActor(SensorOwner);

	// Use UIrradianceSubsystem::CaptureCam (ACameraActor)
	if (CaptureCam.IsValid())
	{
		Params.AddIgnoredActor(CaptureCam.Get());
	}

	// Robust start
	const FVector Start = Req.PosWS + N * 5.0f;
	const float TraceLenCm = 10000000.0f;

	// UpVector to Normal rotation
	const FQuat Rot = FQuat::FindBetweenNormals(FVector::UpVector, N);

	// Fibonacci in hemisphere
	const double GoldenRatio = (1.0 + FMath::Sqrt(5.0)) * 0.5;

	double Sum = 0.0;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const double u = (i + 0.5) / double(NumSamples);   // (0,1)
		const double v = FMath::Frac(i / GoldenRatio);     // (0,1)

		const double z = u;                                // cos(theta)
		const double r = FMath::Sqrt(FMath::Max(0.0, 1.0 - z * z));
		const double theta = 2.0 * PI * v;

		const FVector LocalDir(
			float(r * FMath::Cos(theta)),
			float(r * FMath::Sin(theta)),
			float(z)
		);

		const FVector DirWS = Rot.RotateVector(LocalDir).GetSafeNormal();
		const FVector End = Start + DirWS * TraceLenCm;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

		const double V = bHit ? 0.0 : 1.0;
		Sum += V * z;
	}

	const double Svf = 2.0 * (Sum / double(NumSamples));
	return float(FMath::Clamp(Svf, 0.0, 1.0));
}


// -----------------------------------------------------------------------------
//  Export
// -----------------------------------------------------------------------------

void UIrradianceSubsystem::ConfigureExport(bool bCSV, bool bExportImages, const FString& OutputDirPath)
{
	if (!Exporter)
	{
		Exporter = NewObject<UIrradianceExporter>(this);
		PYRANO_INFO(TEXT("[Subsystem] Created Exporter: %p (exporter_outer=%s, subsystem=%s)"),
			Exporter.Get(),
			*GetNameSafe(Exporter ? Exporter->GetOuter() : nullptr),
			*GetNameSafe(this));
	}
	
	ExportOptions.bExportCSV = bCSV;
	ExportOptions.OutputDir.Path = OutputDirPath;	// if empty -> Saved/Irradiance

	ExportOptions.bExportImages = bExportImages;
	if (Exporter) 
	{
		Exporter->Init(ExportOptions);
	}
}


void UIrradianceSubsystem::FlushExporter()
{
	if (Exporter)
	{
		Exporter->FlushCSVIfNeeded(true);
	}
}




