// PyranometerComponent.cpp

#include "Components/PyranometerComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UPyranometerComponent::UPyranometerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


// -----------------------------------------------------------------------------
//  Getters
// -----------------------------------------------------------------------------

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


// -----------------------------------------------------------------------------
//  GUID Assignment
// -----------------------------------------------------------------------------

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
	InvalidateSkyViewFactor();
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


// -----------------------------------------------------------------------------
//  Sky Factor
// -----------------------------------------------------------------------------


void UPyranometerComponent::InvalidateSkyViewFactor()
{
	bSkyViewFactorValid = false;
	SkyViewFactor = -1.0f;
}

#if WITH_EDITOR
void UPyranometerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.Property
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropName == GET_MEMBER_NAME_CHECKED(UPyranometerComponent, CustomNormalWS))
	{
		InvalidateSkyViewFactor();
	}
}
#endif