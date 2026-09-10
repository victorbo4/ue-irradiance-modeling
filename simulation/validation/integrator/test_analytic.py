"""Analytic checks for the cubemap irradiance integrator.

Run:  python test_analytic.py         (uses numpy only)

References
---------
1. Uniform radiance L0 over the whole sphere, any sensor normal:
       E = pi * L0                       (projected solid angle of a hemisphere)
   This is the IRR_DEBUG_PI case (L0 = 1 -> E = pi).

2. Uniform radiance L0 over the sensor's own hemisphere only:
       E = pi * L0                       (same; the back hemisphere contributes 0)

3. Non-uniform fields have no simple closed form here, but the *fixed*
   integrator is self-consistent and the point is to show the *current*
   integrator's error is NOT a constant factor (so calibration can't undo it).

Pass criterion for the fix: |E_fixed - pi| / pi < 1e-3 for every sensor normal.
"""

from __future__ import annotations

import numpy as np

from reference_integrator import integrate

PI = np.pi


def _row(label, L, radiance, n, ref=None):
    cur = integrate(radiance, n, L, "current")["E"]
    fix = integrate(radiance, n, L, "fixed")["E"]
    ref_s = f"{ref:8.5f}" if ref is not None else "    --  "
    d_cur = f"{100*(cur/ref - 1):+6.2f}%" if ref else "   -   "
    d_fix = f"{100*(fix/ref - 1):+6.2f}%" if ref else "   -   "
    print(f"  {label:38s} ref={ref_s}  current={cur:8.5f} ({d_cur})  "
          f"fixed={fix:8.5f} ({d_fix})")
    return cur, fix


def main():
    L = 512
    print(f"\ncubemap irradiance integrator - analytic checks (L={L})")
    print("=" * 92)

    print("\n[1] uniform radiance = 1 over the whole sphere   (IRR_DEBUG_PI; ref = pi)")
    uni = lambda f, u, v: 1.0
    worst_fix = 0.0
    for name, n in [("sensor N = +Z", (0, 0, 1)),
                    ("sensor N = +X", (1, 0, 0)),
                    ("sensor N = +Y", (0, 1, 0)),
                    ("sensor N = (1,1,1)", (1, 1, 1)),
                    ("sensor N = (1,2,3)", (1, 2, 3))]:
        _, fix = _row(name, L, uni, n, ref=PI)
        worst_fix = max(worst_fix, abs(fix / PI - 1))

    print("\n[2] uniform radiance = 1 over the sensor hemisphere only   (ref = pi)")
    for name, n in [("sensor N = +Z", (0, 0, 1)), ("sensor N = (1,2,3)", (1, 2, 3))]:
        nn = np.asarray(n, float); nn /= np.linalg.norm(nn)
        hemi = lambda f, u, v, nn=nn: 1.0 if np.dot(nn, _dir(f, u, v)) > 0 else 0.0
        _row(name, L, hemi, n, ref=PI)

    print("\n[3] non-uniform fields  (no closed form; shows current-integrator error"
          " is not a constant factor)")
    # bright band on one face vs the opposite face; same total 'energy', very
    # different placement relative to the truncated corner / double-counted band.
    for name, rad in [
        ("radiance=1 on +X face only", lambda f, u, v: 1.0 if f == 0 else 0.0),
        ("radiance=1 on -X face only", lambda f, u, v: 1.0 if f == 1 else 0.0),
        ("radiance=1 on +Y face only", lambda f, u, v: 1.0 if f == 2 else 0.0),
        ("radiance=1 near (+u,+v) corners", lambda f, u, v: 1.0 if (u > 0.6 and v > 0.6) else 0.0),
        ("radiance=1 near (-u,-v) corners", lambda f, u, v: 1.0 if (u < -0.6 and v < -0.6) else 0.0),
    ]:
        cur = integrate(rad, (0, 0, 1), L, "current")["E"]
        fix = integrate(rad, (0, 0, 1), L, "fixed")["E"]
        rel = 100 * (cur / fix - 1) if fix > 1e-9 else float("nan")
        print(f"  {name:38s} current={cur:9.5f}  fixed={fix:9.5f}  "
              f"current vs fixed = {rel:+6.2f}%")

    cov_cur = integrate(uni, (0, 0, 1), L, "current")["coverage_frac"]
    cov_fix = integrate(uni, (0, 0, 1), L, "fixed")["coverage_frac"]
    print("\n" + "=" * 92)
    print(f"unique-texel coverage:  current {cov_cur:.1%}   fixed {cov_fix:.1%}")
    ok = worst_fix < 1e-3
    print(f"fix check: max |E_fixed - pi|/pi = {worst_fix:.2e}   "
          f"-> {'PASS' if ok else 'FAIL'} (<1e-3)")
    return 0 if ok else 1


def _dir(face, u, v):
    from reference_integrator import a_from_face_uv
    a = a_from_face_uv(u, v, face)
    return a / np.linalg.norm(a)


if __name__ == "__main__":
    raise SystemExit(main())
