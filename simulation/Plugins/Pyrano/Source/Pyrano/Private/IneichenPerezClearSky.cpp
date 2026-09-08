#include "IneichenPerezClearSky.h"

static constexpr double Pyrano_PI = 3.14159265358979323846;

void UClearSkyService::Configure(const FPyranoClearSkyConfig& InConfig)
{
	Config.LinkeTurbidity = FMath::Clamp(InConfig.LinkeTurbidity, 1.0, 10.0);
	Config.AltitudeMeters = FMath::Clamp(InConfig.AltitudeMeters, -500.0, 9000.0);
}

double UClearSkyService::ExtraterrestrialIrradiance_Wm2(int32 DayOfYear)
{
	DayOfYear = FMath::Clamp(DayOfYear, 1, 366);
	const double Gamma = 2.0 * Pyrano_PI * (static_cast<double>(DayOfYear) / 365.0);
	return 1367.0 * (1.0 + 0.033 * FMath::Cos(Gamma));
}

double UClearSkyService::AirMassKastenYoung(double SolarZenithRad)
{
	const double Zdeg = FMath::RadiansToDegrees(SolarZenithRad);
	const double Zc = FMath::Clamp(Zdeg, 0.0, 90.0);

	const double CosZ = FMath::Cos(FMath::DegreesToRadians(Zc));
	const double Den = CosZ + 0.50572 * FMath::Pow(96.07995 - Zc, -1.6364);
	if (!FMath::IsFinite(Den) || Den <= 0.0) return 0.0;

	return 1.0 / Den;
}

FPyranoClearSkyIrradiance UClearSkyService::Compute(const FDateTime& WhenUTC, double SolarZenithRad) const
{
	FPyranoClearSkyIrradiance Out;
	Out.DNI_Wm2 = 0.0;
	Out.DHI_Wm2 = 0.0;
	Out.GHI_Wm2 = 0.0;

	// Sun below horizon
	if (SolarZenithRad >= (Pyrano_PI * 0.5))
	{
		return Out;
	}

	const int32 DayOfYear = WhenUTC.GetDayOfYear();
	const double dni_extra = ExtraterrestrialIrradiance_Wm2(DayOfYear);

	const double cosZ = FMath::Max(0.0, FMath::Cos(SolarZenithRad));
	if (cosZ <= 1e-6)
	{
		return Out;
	}

	const double AM_rel = AirMassKastenYoung(SolarZenithRad);
	if (AM_rel <= 0.0)
	{
		return Out;
	}

	const double Alt = FMath::Clamp(Config.AltitudeMeters, -500.0, 9000.0);
	const double TL = FMath::Clamp(Config.LinkeTurbidity, 1.0, 10.0);

	// Absolute air mass (pressure correction)
	const double AM = AM_rel * FMath::Exp(-Alt / 8434.5);

	// Altitude factors
	const double fh1 = FMath::Exp(-Alt / 8000.0);
	const double fh2 = FMath::Exp(-Alt / 1250.0);
	const double cg1 = 5.09e-5 * Alt + 0.868;
	const double cg2 = 3.92e-5 * Alt + 0.0387;

	// --- GHI (clear sky, Ineichen–Perez) ---
	double ghi_factor =
		FMath::Exp(-cg2 * AM * (fh1 + fh2 * (TL - 1.0)));

	ghi_factor = FMath::Max(0.0, ghi_factor);
	const double GHI = cg1 * dni_extra * cosZ * ghi_factor;

	// --- DNI (clear sky) ---
	const double b = 0.664 + 0.163 / fh1;

	double bnci = b * FMath::Exp(-0.09 * AM * (TL - 1.0));
	bnci = dni_extra * FMath::Max(0.0, bnci);

	double bnci_2_factor =
		(1.0 - (0.1 - 0.2 * FMath::Exp(-TL)) / (0.1 + 0.882 / fh1)) / cosZ;

	bnci_2_factor = FMath::Min(FMath::Max(bnci_2_factor, 0.0), 1e20);
	const double bnci_2 = GHI * bnci_2_factor;

	const double DNI = FMath::Min(bnci, bnci_2);

	// --- DHI (residual) ---
	double DHI = GHI - DNI * cosZ;

	DHI = FMath::Max(0.0, DHI);

	Out.GHI_Wm2 = FMath::Max(0.0, GHI);
	Out.DNI_Wm2 = FMath::Max(0.0, DNI);
	Out.DHI_Wm2 = DHI;

	return Out;

}
