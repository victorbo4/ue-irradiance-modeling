#include "PyranometerComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"



UPyranometerComponent::UPyranometerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}



// ============================================================================
// GETTERS
// ============================================================================

FString UPyranometerComponent::GetGuidString() const
{
	return SensorGuid.ToString();;
}


FVector UPyranometerComponent::GetNormalWS() const
{
	if (!CustomNormalWS.IsNearlyZero())		// if custom normal not setted return world normal
		return CustomNormalWS.GetSafeNormal();
	return GetUpVector();
}


FVector UPyranometerComponent::GetWorldPosition() const
{
	return GetComponentLocation();
}



// ============================================================================
// GUID ASSIGNMENT
// ============================================================================

void UPyranometerComponent::EnsureGuid(bool bForceNew)
{
	if (bForceNew || !SensorGuid.IsValid())
	{
		SensorGuid = FGuid::NewGuid();	// auto-generate if missing
	}
}


void UPyranometerComponent::OnRegister()
{
	Super::OnRegister();
	EnsureGuid(false);	
}


#if WITH_EDITOR
void UPyranometerComponent::PostEditImport()
{
	Super::PostEditImport();
	EnsureGuid(true);
}


void UPyranometerComponent::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	EnsureGuid(true);
}
#endif
