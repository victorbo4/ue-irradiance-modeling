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

---

# Face-orientation check (`camera_projection.py`, `test_orientation.py`)

Separate question: does `AFromFaceUV` map texel `(x, y)` to the world direction the
**capture camera** actually rendered into it? (`GenerateCubemapFaceQuats` +
`CaptureCam`: 90° horizontal FOV, square, local +X fwd / +Y right / +Z up.)

`camera_projection.py` models that camera and compares. Finding (2026-09-10, CPU
model — **confirm in-engine**):

- Each face individually matches the camera under *some* image orientation, but
  **four different orientations are needed** (F0 transpose, F1 rot90, F2/F4/F5
  rot180, F3 identity). One capture code path cannot store six faces four ways →
  `AFromFaceUV` applies inconsistent per-face rotations. Under **any** global
  flip convention, **≥ 3 faces are mis-oriented**.
- Under the standard pinhole convention, only **F3 (−Y)** is correct.
  `test_orientation.py::corrected_afromfaceuv` is the mapping derived directly
  from the quats + camera (verified against the camera model to 1e-6°).
- Impact (fixed traversal, `sim_comp_amb_lux` only): current vs corrected differs
  ~1 % for a sharp directional lobe, ~5 % for a smooth sky gradient, sensor- and
  scene-dependent. Invisible for `IRR_DEBUG_PI` (uniform → π regardless of
  per-face rotation), which is why it was not caught.

**In-engine confirmation before fixing:** put 6 distinctly coloured + labelled
surfaces in known world directions (+X, −X, +Y, −Y, +Z, −Z) around a sensor,
capture with `bExportImages = true`, inspect the 6 EXRs. Each face's EXR must show
the surface it points at, in the orientation `corrected_afromfaceuv` predicts.
That pins the flip convention; then apply the corrected `AFromFaceUV`.
