"""Does AFromFaceUV match the orientation the capture camera actually rendered?

Findings (2026-09-10, CPU model - to be confirmed in-engine):

* Each of the 6 faces individually matches the capture camera under SOME image
  orientation, but FOUR different orientations are needed
  (F0 transpose, F1 rot90, F2/F4/F5 rot180, F3 identity).
  A single capture code path cannot store 6 faces with 4 orientations, so
  AFromFaceUV applies inconsistent per-face rotations: under ANY global
  convention at least 3 faces are mis-oriented.

* Assuming the standard pinhole convention ("identity": col -> +right,
  row -> -up), only F3 (-Y) is correct. The corrected per-face mapping,
  derived directly from GenerateCubemapFaceQuats + the camera, is CORRECTED_AFROMFACEUV.

* Impact: the integrator weights each texel's radiance by cos(theta) of the
  WRONG direction. Invisible for a uniform field (IRR_DEBUG_PI still = pi) and
  nearly invisible for a horizontal sensor under a smooth sky; corrupts tilted
  sensors (Pinc) and any directional radiance.

Run:  python test_orientation.py
"""

from __future__ import annotations

import numpy as np

from camera_projection import camera_ray, make_from_xz, FACE_FWD_UP
from reference_integrator import a_from_face_uv, texel_uv


# Corrected mapping: d = fwd + u*right - v*up, with (fwd,right,up) = MakeFromXZ.
# Worked out in closed form; _check_closed_form() verifies it against camera_ray.
def corrected_afromfaceuv(u, v, face):
    return {
        0: (1.0, u, -v),
        1: (-1.0, -u, -v),
        2: (-u, 1.0, -v),
        3: (u, -1.0, -v),
        4: (-u, v, 1.0),
        5: (-u, -v, -1.0),
    }[face]


def _unit(x):
    x = np.asarray(x, float)
    return x / np.linalg.norm(x)


def _check_closed_form(L=64):
    worst = 0.0
    for face in range(6):
        for i in range(0, L, 5):
            for j in range(0, L, 5):
                u, v = texel_uv(i, j, L)
                a = _unit(corrected_afromfaceuv(u, v, face))
                b = camera_ray(i, j, L, face, "identity      (col=+R, row=-U)")
                worst = max(worst, np.degrees(np.arccos(np.clip(a @ b, -1, 1))))
    return worst


def _integrate(faces_rgb, sensor_n, L, dir_fn):
    """Hemispherical E with a chosen texel->direction map (fixed traversal)."""
    n = _unit(sensor_n)
    k = 4.0 / (L * L)
    total = 0.0
    for face in range(6):
        tex = faces_rgb[face]
        for j in range(L):
            for i in range(L):
                u, v = texel_uv(i, j, L)
                a = np.asarray(dir_fn(u, v, face), float)
                dot_na = n @ a
                if dot_na > 0:
                    t = 1.0 + u * u + v * v
                    total += tex[j, i] * k * dot_na / (t * t)
    return total


def _render(radiance_world, L):
    """'Render' a world radiance function into six face textures via the camera."""
    out = []
    for face in range(6):
        tex = np.zeros((L, L))
        for j in range(L):
            for i in range(L):
                d = camera_ray(i, j, L, face, "identity      (col=+R, row=-U)")
                tex[j, i] = radiance_world(d)
        out.append(tex)
    return out


def main():
    cf = _check_closed_form()
    print(f"closed-form corrected mapping vs camera model: max {cf:.2e} deg  "
          f"({'OK' if cf < 1e-3 else 'MISMATCH'})")

    L = 48
    print(f"\nper-face angular error of the CURRENT AFromFaceUV vs the capture camera"
          f"  (L={L}, identity convention)")
    for face in range(6):
        errs = []
        for i in range(0, L, 3):
            for j in range(0, L, 3):
                u, v = texel_uv(i, j, L)
                a = _unit(a_from_face_uv(u, v, face))
                b = camera_ray(i, j, L, face, "identity      (col=+R, row=-U)")
                errs.append(np.degrees(np.arccos(np.clip(a @ b, -1, 1))))
        print(f"  F{face}: mean {np.mean(errs):5.1f}  max {np.max(errs):5.1f} deg"
              + ("   (correct)" if np.max(errs) < 1 else ""))

    # --- integrator impact ---
    print("\nintegrator impact (fixed traversal; 'current' = current AFromFaceUV,"
          " 'corrected' = camera-derived)")

    # (a) directional field: a smooth lobe around a world direction
    d0 = _unit([0.3, -0.7, 0.6])
    lobe = lambda d: max(0.0, float(_unit(d) @ d0)) ** 4
    faces = _render(lobe, L)
    for sensor, label in [((0, 0, 1), "horizontal sensor N=+Z"),
                          (_unit([0.5, 0.2, 0.84]), "tilted sensor (Pinc-like)")]:
        cur = _integrate(faces, sensor, L, a_from_face_uv)
        cor = _integrate(faces, sensor, L, corrected_afromfaceuv)
        rel = 100 * (cur / cor - 1) if cor else float("nan")
        print(f"  directional lobe, {label:26s}: current {cur:8.4f}  "
              f"corrected {cor:8.4f}   current off by {rel:+6.1f}%")

    # (b) smooth uniform-ish sky (why it slipped through for a flat sensor)
    sky = lambda d: 1.0 + 0.3 * _unit(d)[2]
    faces = _render(sky, L)
    for sensor, label in [((0, 0, 1), "horizontal sensor N=+Z"),
                          (_unit([0.5, 0.2, 0.84]), "tilted sensor (Pinc-like)")]:
        cur = _integrate(faces, sensor, L, a_from_face_uv)
        cor = _integrate(faces, sensor, L, corrected_afromfaceuv)
        rel = 100 * (cur / cor - 1) if cor else float("nan")
        print(f"  smooth sky,       {label:26s}: current {cur:8.4f}  "
              f"corrected {cor:8.4f}   current off by {rel:+6.1f}%")


if __name__ == "__main__":
    main()
