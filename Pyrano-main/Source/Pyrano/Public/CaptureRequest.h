/*=============================================================================
	CaptureRequest.h: Parameters to perform a single cubemap capture.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "CaptureRequest.generated.h"

USTRUCT()
struct FCaptureRequest
{
	GENERATED_BODY()

public:
	// Capture data
	FVector		PosWS			= FVector::ZeroVector;
	FVector		NormalWS		= FVector::UpVector;
	int32		SidePx			= 256;
	uint32		WarmupFrames	=  0u;
	FGuid		RequestId;
	FGuid		SensorId;
	FString		SensorName;
	FDateTime	TimestampUTC	= FDateTime(0);

public:
	// Constructors
	FCaptureRequest()
	{
		RequestId = FGuid::NewGuid();
		NormalWS  = NormalWS.GetSafeNormal();
	}

	FCaptureRequest(
		const FVector&	InPosWS,
		const FVector&	InNormalWS,
		int32			InSidePx,
		uint32			InWarmupFrames,
		FGuid			InSensorId,
		const FString&	InSensorName,
		FDateTime		InTimestampUTC	= FDateTime(0),
		FGuid			InRequestId		= FGuid())
		: PosWS				(InPosWS)
		, NormalWS			(InNormalWS.GetSafeNormal())
		, SidePx			(InSidePx)
		, WarmupFrames		(InWarmupFrames)
		, RequestId			(InRequestId.IsValid() ? InRequestId : FGuid::NewGuid())
		, SensorId			(InSensorId)
		, SensorName		(InSensorName)
		, TimestampUTC		(InTimestampUTC)
	{}

	// Factory method
	static FORCEINLINE FCaptureRequest Make(
		const FVector&	InPosWS,
		const FVector&	InNormalWS,
		int32			InSidePx,
		uint32			InWarmupFrames,
		FGuid			InSensorId,
		FString			InSensorName,
		FDateTime		InTimestampUTC	= FDateTime(0),
		FGuid			InRequestId		= FGuid())
	{
		return FCaptureRequest
		(InPosWS, InNormalWS, InSidePx, InWarmupFrames, InSensorId, InSensorName, InTimestampUTC, InRequestId);
	}

	// Setters & Getters
	FORCEINLINE bool IsValid() const 
	{
		return SidePx > 0 && NormalWS.IsNormalized();
	}

	FORCEINLINE FCaptureRequest WithSidePx(int32 InSidePx) const
	{
		FCaptureRequest R = *this; R.SidePx = InSidePx; return R;
	}

	FORCEINLINE FCaptureRequest WithWarmup(uint32 InWarmup) const
	{
		FCaptureRequest R = *this; R.WarmupFrames = InWarmup; return R;
	}

	FORCEINLINE FCaptureRequest WithTimestamp(FDateTime InUTC) const
	{
		FCaptureRequest R = *this; R.TimestampUTC = InUTC; return R;
	}

	FORCEINLINE FCaptureRequest WithSensorId(FGuid InId) const
	{
		FCaptureRequest R = *this; R.SensorId = InId; return R;
	}

	bool operator==(const FCaptureRequest& Other) const
	{
		return PosWS.Equals(Other.PosWS, KINDA_SMALL_NUMBER)
			&& NormalWS.Equals(Other.NormalWS, KINDA_SMALL_NUMBER)
			&& SidePx == Other.SidePx
			&& WarmupFrames == Other.WarmupFrames
			&& SensorId == Other.SensorId
			&& TimestampUTC == Other.TimestampUTC;
	}

	bool operator!=(const FCaptureRequest& Other) const { return !(*this == Other); }

	FString ToString() const
	{
		return FString::Printf(TEXT("[Req %s] Pos=(%.1f,%.1f,%.1f) N=(%.3f,%.3f,%.3f) Px=%d Warmup=%u Sensor=%s UTC=%s"),
			*RequestId.ToString(EGuidFormats::DigitsWithHyphensInBraces),
			PosWS.X, PosWS.Y, PosWS.Z,
			NormalWS.X, NormalWS.Y, NormalWS.Z,
			SidePx, WarmupFrames,
			*SensorId.ToString(),
			*TimestampUTC.ToString());
	}
};