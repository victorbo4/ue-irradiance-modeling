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

### Running the check: `verify_rig_exr.py`

Automates the "inspect the 6 EXRs" step. Build the rig with a WHITE marker offset
toward each face's true camera-Up axis and a RED marker toward its true
camera-Right axis (world-space offsets — no rotation math needed; run the script
with no arguments to print the exact placement table). Capture once with that rig
as the only enabled sensor, `bExportImages = true`, then:

```bash
python verify_rig_exr.py "<OutputPath>/Images/<SensorGuid>_*_*.exr"
```

For each face/marker it locates the marker pixel by colour, converts it back to a
world direction via both the pre-fix `AFromFaceUV` and `corrected_afromfaceuv`,
and reports which one actually matches what was built — no assumption about the
image-storage convention is baked in, so it also flags a residual global flip
(e.g. an RHI render-target Y-flip) if neither formula matches.

**Confirmed in-engine 2026-09-11.** Ran the rig at 4 resolutions (128/256/512/
1024 px, independent `CaptureOnce` runs): all 4 gave the identical result —
10/12 marker checks matched `corrected_afromfaceuv` to 0.1–0.3° (pixel
quantization), the other 2 (both on face −Y) tied at the same error against the
pre-fix formula too, since −Y was already correct. Zero "neither matches" — no
extra global flip on top of the bug. `AFromFaceUV` in
`Shaders/Public/IrradianceCommon.ush` and `reference_integrator.py::a_from_face_uv`
now both implement the corrected mapping; S2 is closed.

**The rig itself is kept** as a standalone, non-production test level:
`/Game/Validation/Levels/OrientationRig` (+ `/Game/Validation/Materials/M_White`,
`M_Red`), i.e. `simulation/Content/Validation/...` — deliberately outside
`Azotea_ETSIDI/`, so it never gets mixed up with the real campaign level/content.
`build_rig.py` (re)builds the 12 markers there in one `py "build_rig.py"` console
command; re-run it any time the orientation fix needs to be re-verified (e.g.
after an engine upgrade or a further shader change).
