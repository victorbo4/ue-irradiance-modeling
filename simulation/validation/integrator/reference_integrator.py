"""CPU reference model of the Pyrano cubemap irradiance integrator.

Mirrors, line for line, the math and the thread-group traversal of
`simulation/Plugins/Pyrano/Shaders/Private/IrradianceIntegrate.usf` (+
`IrradianceCommon.ush`) and the dispatch in `IrradianceIntegrateCS.cpp`.

Purpose: reproduce the *current* shader's behaviour without Unreal, quantify the
suspected face-coverage bug, and verify a fix against closed-form references.

Traversal modes
---------------
- "current" : the pre-fix shader (kept as a regression reference)
              base = Gid.xy * (TGX, TGY)      with GroupsX = ceil(L / (TGX*2))
              Groups overlap; a texel hit by two groups is summed twice
              (each group writes its own partial and the reduce pass adds them).
- "fixed"   : base = Gid.xy * (TGX*2, TGY*2)  -> groups tile exactly, once each.
              This is what the shader does as of the fix on this branch.

Both modes keep the shader's `if (x >= L) break;` / `if (y >= L) break;` guards.
"""

from __future__ import annotations

import numpy as np

TGX = 8
TGY = 8


# --- IrradianceCommon.ush -----------------------------------------------------

def texel_uv(i, j, L):
    """TexelUV(i, j, L) -> (u, v) in [-1, 1], pixel-centre sampled."""
    u = ((i + 0.5) / L) * 2.0 - 1.0
    v = ((j + 0.5) / L) * 2.0 - 1.0
    return u, v


def a_from_face_uv(u, v, face):
    """AFromFaceUV(uv, face) -> direction, NOT normalised (|a| = sqrt(1+u^2+v^2)).

    Fixed 2026-09-11 (bug-tracker.md S2): matches
    test_orientation.py::corrected_afromfaceuv, confirmed in-engine with a
    12-marker rig at 4 resolutions (<=0.3 deg error on every face). Face 3 (-Y)
    is unchanged - it was already correct."""
    if face == 0:   # +X
        return np.array([1.0, u, -v])
    if face == 1:   # -X
        return np.array([-1.0, -u, -v])
    if face == 2:   # +Y
        return np.array([-u, 1.0, -v])
    if face == 3:   # -Y
        return np.array([u, -1.0, -v])
    if face == 4:   # +Z   (Up = -Y)
        return np.array([-u, v, 1.0])
    if face == 5:   # -Z   (Up = +Y)
        return np.array([-u, -v, -1.0])
    raise ValueError(face)


# --- traversal ---------------------------------------------------------------

def _group_bases(L, mode):
    """Yield (base_x, base_y) for every dispatched thread group, per the C++."""
    groups_x = -(-L // (TGX * 2))          # FMath::DivideAndRoundUp(L, TGX*2)
    groups_y = -(-L // (TGY * 2))
    stride_x = TGX if mode == "current" else TGX * 2
    stride_y = TGY if mode == "current" else TGY * 2
    for gy in range(groups_y):
        for gx in range(groups_x):
            yield gx * stride_x, gy * stride_y


def _group_texels(base_x, base_y, L):
    """The (x, y) a single group visits: 2x2 striding, Tid in [0,8)^2, ox/oy in {0,1}.

    Mirrors the shader's `break` on the first out-of-range column/row.
    """
    for tid_y in range(TGY):
        for oy in range(2):
            y = base_y + tid_y + oy * TGY
            if y >= L:
                break
            for tid_x in range(TGX):
                for ox in range(2):
                    x = base_x + tid_x + ox * TGX
                    if x >= L:
                        break
                    yield x, y


def integrate(radiance, sensor_n, L, mode="current"):
    """Hemispherical irradiance from six face textures, the shader's way.

    Parameters
    ----------
    radiance : callable(face:int, u:float, v:float) -> float
        Scene radiance (single channel) for a direction. For IRR_DEBUG_PI use
        `lambda f, u, v: 1.0` -> the exact integrator returns pi.
    sensor_n : (3,) array-like, will be normalised (as SensorN is on the C++ side).
    L : int, cube face side in pixels.
    mode : "current" | "fixed".

    Returns
    -------
    dict with E (irradiance), n_texel_writes, n_unique_texels, coverage_frac.
    """
    n = np.asarray(sensor_n, float)
    n = n / np.linalg.norm(n)
    k = 4.0 / (L * L)

    total = 0.0
    writes = 0
    unique = set()

    for face in range(6):
        for base_x, base_y in _group_bases(L, mode):
            for x, y in _group_texels(base_x, base_y, L):
                u, v = texel_uv(x, y, L)
                a = a_from_face_uv(u, v, face)
                dot_na = float(np.dot(n, a))          # n normalised, a is not
                if dot_na > 0.0:
                    t = 1.0 + u * u + v * v
                    w = k * dot_na / (t * t)          # k * (N.a)/t^2  == k*cos/t^1.5
                    total += radiance(face, u, v) * w
                writes += 1
                unique.add((face, x, y))

    return {
        "E": total,
        "n_texel_writes": writes,
        "n_unique_texels": len(unique),
        "coverage_frac": len(unique) / (6 * L * L),
    }


if __name__ == "__main__":
    for L in (256, 512):
        cur = integrate(lambda f, u, v: 1.0, (0, 0, 1), L, "current")
        fix = integrate(lambda f, u, v: 1.0, (0, 0, 1), L, "fixed")
        print(f"L={L}  pi={np.pi:.6f}")
        print(f"  current: E={cur['E']:.6f}  coverage={cur['coverage_frac']:.1%}  "
              f"writes={cur['n_texel_writes']} unique={cur['n_unique_texels']}")
        print(f"  fixed  : E={fix['E']:.6f}  coverage={fix['coverage_frac']:.1%}  "
              f"writes={fix['n_texel_writes']} unique={fix['n_unique_texels']}")
