#include "RayTracingIrradianceRGS.h"


#if RHI_RAYTRACING


IMPLEMENT_GLOBAL_SHADER(
    FRayTracingIrradianceRGS,
    "/Plugin/IrradianceDXR/RayTracingIrradiance.usf",
    "RayTracingIrradianceRGS",
    SF_RayGen
);

#endif