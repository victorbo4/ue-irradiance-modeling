#include "RayTracingIrradianceCHS.h"


#if RHI_RAYTRACING


IMPLEMENT_GLOBAL_SHADER(
	FRayTracingIrradianceCHS,
	"/Plugin/IrradianceDXR/RayTracingIrradiance.usf",
	"RayTracingIrradianceCHS",
	SF_RayHitGroup
);

#endif