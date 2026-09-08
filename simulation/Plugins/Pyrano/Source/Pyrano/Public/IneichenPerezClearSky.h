#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IneichenPerezClearSky.generated.h"

USTRUCT(BlueprintType)
struct FPyranoClearSkyConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyrano|ClearSky")
	double LinkeTurbidity = 2.0; // typical clear

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyrano|ClearSky")
	double AltitudeMeters = 0.0; // ASL
};

USTRUCT(BlueprintType)
struct FPyranoClearSkyIrradiance
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pyrano|ClearSky")
	double DNI_Wm2 = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pyrano|ClearSky")
	double DHI_Wm2 = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pyrano|ClearSky")
	double GHI_Wm2 = 0.0;

	bool IsValid() const
	{
		return FMath::IsFinite(DNI_Wm2) && FMath::IsFinite(DHI_Wm2) && FMath::IsFinite(GHI_Wm2);
	}
};

UCLASS()
class PYRANO_API UClearSkyService : public UObject
{
	GENERATED_BODY()

public:
	void Configure(const FPyranoClearSkyConfig& InConfig);

	/** Computes clear-sky irradiance for a given sun zenith and timestamp (UTC). */
	FPyranoClearSkyIrradiance Compute(const FDateTime& WhenUTC, double SolarZenithRad) const;

private:
	FPyranoClearSkyConfig Config;

	// helpers
	static double ExtraterrestrialIrradiance_Wm2(int32 DayOfYear);
	static double AirMassKastenYoung(double SolarZenithRad);
};
