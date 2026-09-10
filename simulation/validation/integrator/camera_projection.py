"""CPU model of the per-face capture camera, to check it against AFromFaceUV.

The integrator assumes: texel (x, y) of face F holds the scene radiance coming
from world direction `AFromFaceUV(TexelUV(x, y, L), F)`.

That is only true if it matches what the capture camera actually rendered into
that texel. The capture uses (IrradianceCommon.cpp / IrradianceSubsystem.cpp):

    rot   = FRotationMatrix::MakeFromXZ(Fwd, Up).ToQuat()      # per face
    camera: FieldOfView = 90 (horizontal), AspectRatio = 1, looks down local +X,
            local +Y = right, local +Z = up.

Face (Fwd, Up):
    +X (1,0,0)(0,0,1)   -X (-1,0,0)(0,0,1)
    +Y (0,1,0)(0,0,1)   -Y (0,-1,0)(0,0,1)
    +Z (0,0,1)(0,-1,0)  -Z (0,0,-1)(0,1,0)

Unknowns that a pure-CPU check can't pin down (need the engine): whether the
captured texture is flipped in x and/or y relative to NDC. We enumerate the
4 plausible conventions and report which, if any, makes the camera ray equal
`AFromFaceUV` for *all six* faces.
"""

from __future__ import annotations

import numpy as np

from reference_integrator import a_from_face_uv, texel_uv

# (Fwd, Up) per face, exactly as GenerateCubemapFaceQuats().
FACE_FWD_UP = [
    ((1, 0, 0), (0, 0, 1)),
    ((-1, 0, 0), (0, 0, 1)),
    ((0, 1, 0), (0, 0, 1)),
    ((0, -1, 0), (0, 0, 1)),
    ((0, 0, 1), (0, -1, 0)),
    ((0, 0, -1), (0, 1, 0)),
]


def _normalize(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def make_from_xz(x_axis, z_axis):
    """FRotationMatrix::MakeFromXZ -> (fwd, right, up) basis vectors (world)."""
    new_x = _normalize(x_axis)
    z = np.asarray(z_axis, float)
    new_z = _normalize(z - np.dot(z, new_x) * new_x)
    new_y = np.cross(new_z, new_x)          # UE: NewY = NewZ ^ NewX
    return new_x, new_y, new_z              # fwd, right, up


# The 8 in-plane orientations of a square image: how (a, b) = (col, row) NDC
# coordinates map onto (right, up) screen axes. "identity" is a standard pinhole
# camera (col -> +right, row -> -up i.e. down). The others are 90/180/270 deg
# rotations, optionally mirrored - the ways a capture can end up stored.
IMAGE_ORIENTATIONS = {
    "identity      (col=+R, row=-U)": lambda a, b: (a, -b),
    "flip_x        (col=-R, row=-U)": lambda a, b: (-a, -b),
    "flip_y        (col=+R, row=+U)": lambda a, b: (a, b),
    "rot180        (col=-R, row=+U)": lambda a, b: (-a, b),
    "transpose     (row=+R, col=-U)": lambda a, b: (b, -a),
    "transpose+fx  (row=-R, col=-U)": lambda a, b: (-b, -a),
    "transpose+fy  (row=+R, col=+U)": lambda a, b: (b, a),
    "rot90/rot270  (row=-R, col=+U)": lambda a, b: (-b, a),
}


def camera_ray(i, j, L, face, orient="identity      (col=+R, row=-U)"):
    """World direction the capture camera sends through texel (i, j) of `face`.

    90 deg horizontal FOV, square aspect -> tan(fov/2) = 1.
    `orient` selects one of the 8 ways the stored image can be laid out relative
    to the (right, up) camera axes.
    """
    fwd, right, up = make_from_xz(*FACE_FWD_UP[face])
    a = 2.0 * (i + 0.5) / L - 1.0
    b = 2.0 * (j + 0.5) / L - 1.0
    nx, ny = IMAGE_ORIENTATIONS[orient](a, b)
    d = fwd + nx * right + ny * up
    return d / np.linalg.norm(d)


def afromfaceuv_dir(i, j, L, face):
    u, v = texel_uv(i, j, L)
    return _normalize(a_from_face_uv(u, v, face))


def max_angle_deg(L, face, orient, step=9):
    worst = 0.0
    for i in range(0, L, step):
        for j in range(0, L, step):
            a = camera_ray(i, j, L, face, orient)
            b = afromfaceuv_dir(i, j, L, face)
            ang = np.degrees(np.arccos(np.clip(np.dot(a, b), -1, 1)))
            worst = max(worst, ang)
    return worst


if __name__ == "__main__":
    L = 128
    print(f"max angle (deg) between the capture-camera ray and AFromFaceUV, per face"
          f"  (L={L})")
    print("A convention with all faces ~0 means AFromFaceUV matches a camera whose"
          " stored image\nhas that orientation. 'identity' = a plain pinhole camera.\n")
    hdr = "  " + " ".join(f"F{f}" for f in range(6))
    print(f"  {'image orientation':32s}{hdr}    worst")
    for name in IMAGE_ORIENTATIONS:
        errs = [max_angle_deg(L, f, name) for f in range(6)]
        flag = "   <- consistent match" if max(errs) < 1.0 else ""
        print(f"  {name:32s}" + " ".join(f"{e:5.1f}" for e in errs)
              + f"  {max(errs):6.1f}{flag}")

    print("\nPer-face: which image orientation (if any) makes AFromFaceUV match the"
          " capture camera")
    face_names = ["+X", "-X", "+Y", "-Y", "+Z", "-Z"]
    n_ok = 0
    for f in range(6):
        best = min(IMAGE_ORIENTATIONS, key=lambda o: max_angle_deg(L, f, o))
        err = max_angle_deg(L, f, best)
        ok = err < 1.0
        n_ok += ok
        print(f"  F{f} ({face_names[f]}) : "
              + (f"matches '{best.split()[0]}'  (err {err:.2f} deg)" if ok
                 else f"NO orientation matches  (best '{best.split()[0]}', err {err:.1f} deg)"))
    print(f"\n  -> {n_ok}/6 faces reconcilable. "
          + ("all consistent." if n_ok == 6 else
             "AFromFaceUV is NOT consistent with GenerateCubemapFaceQuats + a"
             " standard capture camera."))
