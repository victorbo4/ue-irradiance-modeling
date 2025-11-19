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


// --- Getters ---

	UFUNCTION(BlueprintPure, Category="Pyranometer")
	FString GetGuidString() const;
	
	UFUNCTION(BlueprintPure, Category="Pyranometer")
	FVector GetNormalWS() const;
	
	UFUNCTION(BlueprintPure, Category="Pyranometer")
	FVector GetWorldPosition() const;

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
