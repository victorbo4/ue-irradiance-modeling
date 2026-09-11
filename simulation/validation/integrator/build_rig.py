"""Build the 12-sphere orientation rig in the currently open Unreal level.

Run from the editor's console command bar (bottom of the main window), ONE
command, no drag-and-drop:

    py "/home/vic/Dev/ue-irradiance-modeling/simulation/validation/integrator/build_rig.py"

Requires /Game/Validation/Materials/M_White and .../M_Red (Unlit materials,
Emissive = white / red respectively) to already exist - create them once by
hand if they don't. The rig level itself lives at
/Game/Validation/Levels/OrientationRig - a standalone test level, kept
separate from the production Azotea_ETSIDI content.

Edit SENSOR below if your PyranometerComponent isn't at the world origin.
"""

import unreal

MATERIAL_WHITE = "/Game/Validation/Materials/M_White"
MATERIAL_RED = "/Game/Validation/Materials/M_Red"

SENSOR = (0.0, 0.0, 0.0)   # <-- world-space location of your sensor
R = 300.0                  # distance from sensor to each screen (cm)
OFFSET = 150.0             # marker offset within its screen (cm)

# (label, Fwd, Right, Up) - Right/Up = the real capture-camera axes per face
# (FRotationMatrix::MakeFromXZ(Fwd, ConfiguredUp), see camera_projection.py).
FACES = [
    ("+X", (1, 0, 0), (0, 1, 0), (0, 0, 1)),
    ("-X", (-1, 0, 0), (0, -1, 0), (0, 0, 1)),
    ("+Y", (0, 1, 0), (-1, 0, 0), (0, 0, 1)),
    ("-Y", (0, -1, 0), (1, 0, 0), (0, 0, 1)),
    ("+Z", (0, 0, 1), (-1, 0, 0), (0, -1, 0)),
    ("-Z", (0, 0, -1), (-1, 0, 0), (0, 1, 0)),
]


def add3(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def scl(v, s):
    return (v[0] * s, v[1] * s, v[2] * s)


def load_material(path):
    for candidate in (f"{path}.{path.rsplit('/', 1)[-1]}", path):
        mat = unreal.load_asset(candidate)
        if mat:
            return mat
    raise RuntimeError(f"Could not load material at '{path}' - create it first.")


def main():
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    sphere_mesh = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
    if not sphere_mesh:
        raise RuntimeError("Could not load /Engine/BasicShapes/Sphere - check the path.")

    white_mat = load_material(MATERIAL_WHITE)
    red_mat = load_material(MATERIAL_RED)

    spawned = 0
    for name, fwd, right, up in FACES:
        center = add3(SENSOR, scl(fwd, R))
        for tag, axis, mat in [("White", up, white_mat), ("Red", right, red_mat)]:
            loc = add3(center, scl(axis, OFFSET))
            actor = actor_subsystem.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(0, 0, 0))
            if actor is None:
                print(f"FAILED to spawn marker {tag} for face {name}")
                continue
            actor.set_actor_label(f"Rig{tag}_{name}")
            smc = actor.static_mesh_component
            smc.set_static_mesh(sphere_mesh)
            smc.set_material(0, mat)
            try:
                smc.set_mobility(unreal.ComponentMobility.STATIC)
            except Exception as e:
                print(f"  (non-fatal) could not set mobility on {tag}_{name}: {e}")
            actor.set_actor_scale3d(unreal.Vector(0.3, 0.3, 0.3))
            spawned += 1

    print(f"Rig build done: {spawned}/12 markers spawned.")

    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        les.save_current_level()
    except Exception:
        try:
            unreal.EditorLevelLibrary.save_current_level()
        except Exception as e:
            print(f"Could not auto-save the level ({e}) - save manually with Ctrl+S.")


main()
