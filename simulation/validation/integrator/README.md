# Cubemap irradiance integrator — validation

CPU reference model of the GPU integrator in
`simulation/Plugins/Pyrano/Shaders/Private/IrradianceIntegrate.usf`, used to verify
the hemispherical integration without running Unreal.

## Run

```bash
python -m venv .venv && . .venv/bin/activate   # or reuse analysis/.venv
pip install numpy
python test_analytic.py        # analytic checks, exits non-zero on failure
python reference_integrator.py  # coverage / E summary at L=256 and 512
```

## What it found (2026-09-10)

The committed shader traverses each cube face with
`base = Gid.xy * (TGX, TGY)` (stride 8) while the dispatch sizes the grid for
stride 16 (`GroupsX = ceil(L / (TGX*2))`) and each group's 2×2 thread striding
spans 16 px. Net effect at L = 512:

- only **26.6 %** of each face's texels are ever visited (the low-index corner);
- the band that *is* visited by two adjacent groups is **summed twice**.

For a **uniform** field these two errors nearly cancel *for a +Z sensor* (E = 3.195
vs π = 3.1416, +1.7 %) — which is why an `IRR_DEBUG_PI` check at that orientation
looked "≈ π" and the bug went unnoticed. It does **not** cancel otherwise:

| sensor normal | current | error vs π |
|---|---|---|
| +Z | 3.195 | +1.7 % |
| +Y | 2.497 | −20.5 % |
| (1,2,3) | 2.627 | −16.4 % |
| +X | 1.800 | **−42.7 %** |

For non-uniform fields the error runs from −100 % (energy in the never-visited
corner) to +275 % (energy in the double-counted band). So the error is
**scene- and orientation-dependent**, correlated with the very geometry the signal
is meant to capture, and a single set of calibration coefficients cannot undo it.

## The fix

`reference_integrator.py` mode `"fixed"` uses `base = Gid.xy * (TGX*2, TGY*2)` →
groups tile exactly, every texel once, `E` matches π to 2e-6 for every sensor
normal. Applied to the shader in the same branch.
