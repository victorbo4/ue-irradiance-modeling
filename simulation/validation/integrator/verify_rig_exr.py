"""Read the 6 face EXRs from the in-engine orientation rig and check them against
`a_from_face_uv` (current, buggy) and `corrected_afromfaceuv` (candidate fix).

Rig (see ../../ai/reports/bug-tracker.md S2 / this dir's README): each of the 6
faces has a neutral dark-gray backdrop plus two small markers placed at fixed
WORLD-SPACE offsets from the sensor:

  - a WHITE marker offset along that face's true camera "Up" axis
  - a RED   marker offset along that face's true camera "Right" axis

("Up"/"Right" = FRotationMatrix::MakeFromXZ(Fwd, ConfiguredUp) applied to
GenerateCubemapFaceQuats' per-face (Fwd, Up) - see FACE_AXES below, and
build_rig_coords() in this file for the exact placement offsets to build the rig.)

Because the markers are placed at literal world offsets, this script does not
assume any convention: it finds where each marker actually landed in the
captured pixels, turns that back into a direction via `a_from_face_uv` and via
`corrected_afromfaceuv`, and reports which one (if either) points at the axis
the marker was actually built along. No in-engine step is required to *run*
this script - only to produce the EXRs.

Usage:
    pip install numpy imageio[freeimage]        # (or `pip install OpenEXR`)
    python verify_rig_exr.py /path/to/Images/<SensorGuid>_*_*.exr

Expects exactly 6 files, one per face, matching *_PosX_*, *_NegX_*, *_PosY_*,
*_NegY_*, *_PosZ_*, *_NegZ_* in the filename (as written by
IrradianceExporter::FaceToString).
"""

from __future__ import annotations

import glob
import sys

import numpy as np

from camera_projection import FACE_FWD_UP, make_from_xz
from reference_integrator import a_from_face_uv, texel_uv
from test_orientation import corrected_afromfaceuv

FACE_NAMES = ["PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ"]
FACE_LABELS = ["+X", "-X", "+Y", "-Y", "+Z", "-Z"]

# True camera (Right, Up) world axis per face, from MakeFromXZ(Fwd, ConfiguredUp).
# This is also what build_rig_coords() offsets the markers along - the ground
# truth this script checks the reconstructed directions against.
FACE_AXES = []
for fwd, up in FACE_FWD_UP:
    f, right, u = make_from_xz(fwd, up)
    FACE_AXES.append((right, u))


def build_rig_coords(sensor_pos=(0.0, 0.0, 0.0), R=300.0, marker_offset=150.0):
    """Print the world-space actor placement table for the rig (UE units, cm)."""
    sx, sy, sz = sensor_pos
    print(f"{'face':4s} {'backdrop center':24s} {'WHITE (Up) marker':24s} {'RED (Right) marker':24s}")
    for name, (fwd, up) in zip(FACE_LABELS, FACE_FWD_UP):
        fwd = np.asarray(fwd, float)
        right, u = FACE_AXES[FACE_LABELS.index(name)]
        center = np.array([sx, sy, sz]) + fwd * R
        white = center + u * marker_offset
        red = center + right * marker_offset
        print(f"{name:4s} ({center[0]:+.0f},{center[1]:+.0f},{center[2]:+.0f})        "
              f"({white[0]:+.0f},{white[1]:+.0f},{white[2]:+.0f})            "
              f"({red[0]:+.0f},{red[1]:+.0f},{red[2]:+.0f})")


def _load_exr(path: str) -> np.ndarray:
    # 1) OpenEXR >= 3.2's new pythonic API (`pip install OpenEXR`, no separate
    #    Imath module) - channels come back interleaved, e.g. one "RGBA" channel.
    try:
        import OpenEXR

        f = OpenEXR.File(path)
        chans = f.channels()
        if "RGBA" in chans:
            img = chans["RGBA"].pixels[..., :3]
        elif "RGB" in chans:
            img = chans["RGB"].pixels[..., :3]
        else:
            img = np.stack([chans[c].pixels for c in "RGB"], axis=-1)
        return np.asarray(img, dtype=np.float64)
    except ImportError:
        pass

    # 2) Legacy OpenEXR (< 3.2) + Imath API.
    try:
        import Imath
        import OpenEXR

        f = OpenEXR.InputFile(path)
        dw = f.header()["dataWindow"]
        w, h = dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1
        pt = Imath.PixelType(Imath.PixelType.FLOAT)
        chans = [np.frombuffer(f.channel(c, pt), dtype=np.float32).reshape(h, w) for c in "RGB"]
        return np.stack(chans, axis=-1).astype(np.float64)
    except ImportError:
        pass

    # 3) imageio (needs a backend that actually decodes EXR, e.g. `imageio[pyav]`
    #    or a working freeimage plugin - not guaranteed on every install).
    import imageio.v3 as iio
    img = iio.imread(path)
    return np.asarray(img, dtype=np.float64)[..., :3]


def _find_marker(img: np.ndarray, target_rgb, max_dist=0.35):
    """Distance-weighted centroid (col, row) of all pixels within `max_dist` of
    `target_rgb`. Falls back to the single closest pixel (reported dist will be
    > max_dist, which the caller treats as "not found") if nothing is close enough."""
    d = np.linalg.norm(img - np.array(target_rgb), axis=-1)
    mask = d < max_dist
    if not mask.any():
        row, col = np.unravel_index(np.argmin(d), d.shape)
        return float(col), float(row), float(d.min())
    rows, cols = np.nonzero(mask)
    weights = 1.0 - d[mask] / max_dist
    col = float(np.average(cols, weights=weights))
    row = float(np.average(rows, weights=weights))
    return col, row, float(d[mask].mean())


def _ang(a, b):
    a = a / np.linalg.norm(a)
    b = np.asarray(b, float) / np.linalg.norm(b)
    return np.degrees(np.arccos(np.clip(a @ b, -1, 1)))


def analyze(paths: list[str], R: float = 300.0, marker_offset: float = 150.0):
    """`R` / `marker_offset` must match what was actually built in-engine (see
    build_rig_coords()) - only their ratio matters, the true marker direction
    from the sensor is normalize(Fwd + (marker_offset/R) * Up_or_Right)."""
    by_face = {}
    for p in paths:
        for i, n in enumerate(FACE_NAMES):
            if f"_{n}_" in p:
                by_face[i] = p
    missing = [FACE_NAMES[i] for i in range(6) if i not in by_face]
    if missing:
        print(f"WARNING: no file matched for face(s) {missing}; skipping them.")

    k = marker_offset / R
    print(f"\n{'face':4s} {'marker':6s} {'px (col,row)':14s} {'half':10s} "
          f"{'err current':12s} {'err corrected':13s} verdict")

    tally = {"current": 0, "corrected": 0, "neither": 0}
    for i in sorted(by_face):
        img = _load_exr(by_face[i])
        h, w, _ = img.shape
        fwd = np.asarray(FACE_FWD_UP[i][0], float)
        right_axis, up_axis = FACE_AXES[i]
        true_up = fwd + k * np.asarray(up_axis)
        true_right = fwd + k * np.asarray(right_axis)

        for marker_name, rgb, true_dir in [
            ("WHITE(Up)", (1, 1, 1), true_up),
            ("RED(Right)", (1, 0, 0), true_right),
        ]:
            col, row, dist = _find_marker(img, rgb)
            if dist > 0.25:
                print(f"{FACE_LABELS[i]:4s} {marker_name:10s} NOT FOUND (closest dist {dist:.2f}) "
                      f"- check the marker color/material in-engine")
                continue

            u, v = texel_uv(int(col), int(row), w)
            d_cur = a_from_face_uv(u, v, i)
            d_cor = corrected_afromfaceuv(u, v, i)
            e_cur = _ang(d_cur, true_dir)
            e_cor = _ang(d_cor, true_dir)

            half = ("top" if row < h / 2 else "bottom") + "-" + ("left" if col < w / 2 else "right")
            if e_cur < 15 and e_cur <= e_cor:
                verdict, tally["current"] = "CURRENT AFromFaceUV matches", tally["current"] + 1
            elif e_cor < 15 and e_cor < e_cur:
                verdict, tally["corrected"] = "corrected_afromfaceuv matches", tally["corrected"] + 1
            else:
                verdict, tally["neither"] = "NEITHER matches (extra flip?)", tally["neither"] + 1

            print(f"{FACE_LABELS[i]:4s} {marker_name:10s} ({col:5.0f},{row:5.0f})   {half:10s} "
                  f"{e_cur:7.1f} deg    {e_cor:7.1f} deg    {verdict}")

    print(f"\nSummary: current={tally['current']}  corrected={tally['corrected']}  "
          f"neither={tally['neither']}  (12 checks expected: 6 faces x 2 markers)")
    if tally["corrected"] == 12:
        print("-> corrected_afromfaceuv matches the real capture on every face. Apply it as-is.")
    elif tally["neither"] > 0:
        print("-> some markers match neither formula: there is likely an additional global flip "
              "(e.g. RHI render-target Y-flip) on top of the AFromFaceUV bug. Look at the "
              "'half' column: if WHITE lands in 'bottom-*' everywhere instead of 'top-*', or RED "
              "in '*-left' everywhere instead of '*-right', that is a single global Y or X flip - "
              "compose it with corrected_afromfaceuv and re-run.")


if __name__ == "__main__":
    args = sys.argv[1:]
    R, marker_offset = 300.0, 150.0
    globs = []
    for a in args:
        if a.startswith("--R="):
            R = float(a.split("=", 1)[1])
        elif a.startswith("--offset="):
            marker_offset = float(a.split("=", 1)[1])
        else:
            globs.append(a)

    if not globs:
        print(__doc__)
        print(f"\n--- rig placement table (sensor at origin, R={R:.0f}, "
              f"marker_offset={marker_offset:.0f}) ---\n")
        build_rig_coords(R=R, marker_offset=marker_offset)
        print("\nOptional flags: --R=<cm> --offset=<cm> (must match what you built)")
        sys.exit(0)

    paths = []
    for arg in globs:
        paths.extend(glob.glob(arg))
    if not paths:
        print("No files matched.")
        sys.exit(1)
    analyze(paths, R=R, marker_offset=marker_offset)
