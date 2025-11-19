/*=============================================================================
	IrradianceSubsystem.h
  Performs 6-face cubemap capture, manages the capture camera and viewport, 
  and computes final irradiance values from GPU results.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "RHIGPUReadback.h"    
#include "RenderGraphResources.h"
#include "Irradiance/IrradianceViewExtension.h"
#include "Irradiance/IrradianceExporter.h"
#include "Simulation/CaptureRequest.h"
#include "IrradianceSubsystem.generated.h"

struct IPooledRenderTarget;

//------ CAPTURE STATE MACHINE ------
enum class ECaptureState : uint8
{
	Idle,
	Capturing,
	WaitingReadback
};


//------ CAPTURE CONTEXT ------
struct FCaptureContext
{
	static constexpr int32 NumFaces = 6;

	/** Original capture request parameters. */
	FCaptureRequest Request;

	/** Current face index being captured [0..NumFaces-1], or INDEX_NONE when done. */
	int32 FaceIndex = 0;

	/** Number of faces successfully collected. */
	int32 FacesCollected = 0;

	/** Fixed rotations for each cubemap face (world space). */
	TArray<FQuat> FixedFaceRots;

	/** Per-face render targets collected from the view extension. */
	TStaticArray<TRefCountPtr<IPooledRenderTarget>, NumFaces> FaceRTs;

	/** GPU buffer containing the extracted irradiance scalar (1 float). */
	TRefCountPtr<FRDGPooledBuffer> ExtractedIrradianceBuffer;

	/** Helper object used to asynchronously read back the irradiance buffer. */
	TUniquePtr<FRHIGPUBufferReadback> IrradianceReadback;

	/** Whether a GPU readback pass has been enqueued. */
	bool bReadbackEnqueued = false;

// --- API ---

	/** Initialize the context with a new request and reset all state. */
	void Begin(const FCaptureRequest& InRequest);

	/** Completely reset the context. */
	void Reset();

	/** Returns true if there is a valid capture request. */
	bool IsValid() const;

// --- Request getters ---

	FVector   GetPosWS()      const;
	FVector   GetNormalWS()   const;
	int32     GetSidePx()     const;
	uint32    GetWarmup()     const;
	const FCaptureRequest& GetRequest() const;

// --- Face management ---

	const FQuat& GetCurrentFaceRot() const;
	bool HasMoreFaces() const;
	bool IsLastFace() const;
	int32 GetCurrentFaceIndex() const;
	void StoreFaceRT(TRefCountPtr<IPooledRenderTarget>&& InRT);
	void AdvanceFace();
	void MarkFacesDone();
	bool AreFacesReady() const;
	void GatherFaces(TRefCountPtr<IPooledRenderTarget> OutFaces[NumFaces]) const;
	void ArmFace(class FIrradianceViewExtension* VE) const;

// --- Readback state helpers ---

	/** True if a readback has been enqueued and the RHI readback is valid. */
	bool HasActiveReadback() const { return bReadbackEnqueued && IrradianceReadback.IsValid(); }
	
	/** True if the GPU buffer exists but the readback helper has not been created. */
	bool NeedsReadbackEnqueue() const { return bReadbackEnqueued && ExtractedIrradianceBuffer.IsValid() && !IrradianceReadback; }
	
	/** True if all faces are done and no readback is pending. */
	bool IsFinished() const { return !HasMoreFaces() && !bReadbackEnqueued; }
};


//------ IRRADIANCE SUBSYSTEM ------
UCLASS()
class UIrradianceSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

// --- Life Cycle ---

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;	
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;

// --- Tickable ---

	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UIrradianceSubsystem, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

// --- Public API --- 

	/** Begin a new 6-face capture using the given request description. */
	void StartSixFaceCapture(const FCaptureRequest& Req);

	/**
	 * Configure optional export of capture data.
	 *
	 * @param bCSV           Enable/disable CSV export of irradiance samples.
	 * @param bExportImages  Enable/disable EXR export of cubemap faces.
	 * @param OutputDirPath  Optional custom output directory (empty = Saved/Irradiance).
	 */
	void ConfigureExport(bool bCSV, bool bExportImages, const FString& OutputDirPath);

	/**
	 * Consume the latest available irradiance value.
	 *
	 * @param OutValue       Output irradiance value (possibly clamped by sun altitude).
	 * @param MinSunAltitude Minimum sun altitude (deg); below this, the value is clamped to 0.
	 * @return               True if a new value was available and consumed.
	 */
	bool ConsumeLatestIrradiance(float& OutValue, float MinSunAltitude = 0.f);

// --- Viewport management ---

	/** Force the PIE client viewport and window to be square (SidePx x SidePx). */
	bool ForceSquareViewportPIE(int32 SidePx);

	/** Restore previous PIE viewport and window size if it had been forced. */
	void RestoreViewportPIE();

	/** Restore the previous player view target if the camera was hijacked. */
	void EndHijackView();

private:

// --- Internal state ---

	/** Per-capture context. */
	FCaptureContext Capture;

	/** Current high-level capture state. */
	ECaptureState State = ECaptureState::Idle;

	/** View extension used to intercept the scene render target and produce cubemap faces. */
	TSharedPtr<class FIrradianceViewExtension, ESPMode::ThreadSafe> ViewExt;

	/** Atomic flag indicating a new irradiance value is available. */
	std::atomic<bool> bIrradianceValueReady{ false };

	/** Last irradiance value produced by the GPU path. */
	std::atomic<float> LastIrradianceValue{ 0.0f };

// --- Capture - GPU Pipeline ---

	/** Handle per-frame logic while faces are being captured. */
	void TickCapturing();

	/** Handle per-frame logic while waiting for GPU readback. */
	void TickReadback();

	/** Enqueue a GPU readback copy of the extracted irradiance buffer. */
	void EnqueueReadbackCopy();

	/** Enqueue a polling command to read back the irradiance value when ready. */
	void EnqueueReadbackPolling();

	/** Ensure that the capture camera exists and is configured. */
	void EnsureCaptureCamera();

	/** Start capture of a single face at PosWS / RotWS. */
	void StartFaceCapture(const FVector& PosWS, const FQuat& RotWS);

	/** Dispatch the irradiance compute shader using the captured faces. */
	void ComputeFinalIrradiance();

// --- Viewport state ---

	bool bForcedRes = false;
	TWeakPtr<class SWindow> PrevPIEWindow;
	FIntPoint PrevViewportSize  = FIntPoint::ZeroValue;
	FIntPoint PrevPIEWindowSize = FIntPoint::ZeroValue;

// --- Export ---

	UIrradianceExporter* Exporter = nullptr;
	FExportOptions       ExportOptions;

// --- View hijack state --- 

	bool bIsHijacked = false;
	TWeakObjectPtr<AActor> PrevViewTarget   = nullptr;
	TWeakObjectPtr<ACameraActor> CaptureCam = nullptr;
};