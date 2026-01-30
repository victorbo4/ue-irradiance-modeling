/*=============================================================================
	PyranometerComponent.h
  A pyranometer sensor component placed in the world.
/============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PyranometerComponent.generated.h"

/**
* PYRANO_COMPONENT PROPERTIES:
*	bool		bEnabled
*	FGuid		Guid
*	FString		Name
*	FVector		PositionWS
*	FVector		NormalWS (or CustomNormalWS)
*/

UCLASS( ClassGroup=(Sensors), meta=(BlueprintSpawnableComponent) )
class PYRANO_API UPyranometerComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	UPyranometerComponent();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

// --- Properties ---

	/** Whether this sensor should participate in captures/simulations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyranometer")
	bool bEnabled = true;

	/** Unique GUID for this sensor (auto-assigned) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pyranometer")
	FGuid SensorGuid;

	/** Optional human-readable name used in CSV output and logs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyranometer")
	FString SensorName = TEXT("Pyranometer");

	/**
	 * Optional custom world-space normal.
	 * If zero, the component's world up-vector is used instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyranometer")
	FVector CustomNormalWS = FVector::ZeroVector;

	/** Cached sky view factor [0..1]. -1 means not computed yet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pyranometer|Sky", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SkyViewFactor = -1.0f;

	/** True when SkyViewFactor has been computed for current pose/normal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pyranometer|Sky")
	bool bSkyViewFactorValid = false;


// --- Getters ---

	UFUNCTION(BlueprintPure, Category="Pyranometer")
	FString GetGuidString() const;
	
	UFUNCTION(BlueprintPure, Category="Pyranometer")
	FVector GetNormalWS() const;
	
	UFUNCTION(BlueprintPure, Category="Pyranometer")
	FVector GetWorldPosition() const;

	UFUNCTION(BlueprintCallable, Category = "Pyranometer|Sky")
	void InvalidateSkyViewFactor();

	float GetSkyViewFactorCached() const { return SkyViewFactor; }
	bool IsSkyViewFactorValid() const { return bSkyViewFactorValid; }


protected:
	virtual void OnRegister() override;

private:	

	/** Ensures each sensor always has a valid GUID.  */
	void EnsureGuid(bool bForceNew = false);

#if WITH_EDITOR
	virtual void PostEditImport() override;              
	virtual void PostDuplicate(bool bDuplicateForPIE) override; 
#endif // WITH_EDITOR
};
