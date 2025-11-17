/*=============================================================================
	IrradianceSubsystem.h
  Performs 6-face cubemap capture, manages the capture camera and viewport, 
  and computes final irradiance values from GPU results.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SharedPointer.h"
#include "Tickable.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"    
#include "IrradianceViewExtension.h"
#include "IrradianceExporter.h"
#include "IrradianceLog.h"
#include "CaptureRequest.h"
#include "IrradianceSubsystem.generated.h"

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

	FCaptureRequest Request;
	int32 FaceIndex = 0;
	int32 FacesCollected = 0;
	TArray<FQuat> FixedFaceRots;

	TStaticArray<TRefCountPtr<IPooledRenderTarget>, NumFaces> FaceRTs;

	TRefCountPtr<FRDGPooledBuffer>          ExtractedIrradianceBuffer;
	TUniquePtr<FRHIGPUBufferReadback>       IrradianceReadback;
	bool                                    bReadbackEnqueued = false;

	 // API 
	void Begin(const FCaptureRequest& InRequest);
	void Reset();
	bool IsValid() const;

	FVector   GetPosWS()      const;
	FVector   GetNormalWS()   const;
	int32     GetSidePx()     const;
	uint32    GetWarmup()     const;
	const FCaptureRequest& GetRequest() const;

	 // Faces
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

	 // State
	bool HasActiveReadback() const { return bReadbackEnqueued && IrradianceReadback.IsValid(); }
	bool NeedsReadbackEnqueue() const { return bReadbackEnqueued && ExtractedIrradianceBuffer.IsValid() && !IrradianceReadback; }
	bool IsFinished()        const { return !HasMoreFaces() && !bReadbackEnqueued; }
};


//------ IRRADIANCE SUBSYSTEM ------
UCLASS()
class UIrradianceSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	 // Life Cycle
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;	
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;

	 // Tickable
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UIrradianceSubsystem, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

	 // Public API
	void StartSixFaceCapture(const FCaptureRequest& Req);
	void ConfigureExport(bool bCSV, const FString& OutputDirPath);
	bool ConsumeLatestIrradiance(float& OutValue);

	 // Viewport
	bool ForceSquareViewportPIE(int32 SidePx);
	void RestoreViewportPIE();
	void EndHijackView();

private:
	 // Internal 
	FCaptureContext Capture;
	ECaptureState State = ECaptureState::Idle;
	TSharedPtr<class FIrradianceViewExtension, ESPMode::ThreadSafe> ViewExt;

	std::atomic<bool> bIrradianceValueReady{ false };
	std::atomic<float> LastIrradianceValue{ 0.0f };

	 // Capture
	void TickCapturing();
	void TickReadback();
	void EnqueueReadbackCopy();
	void EnqueueReadbackPolling();

	void EnsureCaptureCamera();
	void StartFaceCapture(const FVector& PosWS, const FQuat& RotWS);
	void ComputeFinalIrradiance();

	 // Viewport
	bool bForcedRes = false;
	TWeakPtr<class SWindow> PrevPIEWindow;
	FIntPoint PrevViewportSize = FIntPoint::ZeroValue;
	FIntPoint PrevPIEWindowSize = FIntPoint::ZeroValue;

	 // Export 
	UIrradianceExporter* Exporter = nullptr;
	FExportOptions ExportOptions;

	 // View Hijack
	bool bIsHijacked = false;
	TWeakObjectPtr<AActor> PrevViewTarget = nullptr;
	TWeakObjectPtr<ACameraActor> CaptureCam = nullptr;
};