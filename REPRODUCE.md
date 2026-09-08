# Reproducing the results

This document is the full recipe: build the Unreal Engine simulator, run the
irradiance simulations for the ETSIDI rooftop, and reproduce the machine-learning
results in `analysis/`.

For a high-level overview of the project see [README.md](README.md).

---

## 1. Reference environment

The pipeline was last run end-to-end on:

| | |
|---|---|
| Machine | Lenovo Legion 5 15IRX10 — Intel i7-13650HX, NVIDIA RTX 5060 Laptop (Blackwell), 32 GB RAM |
| OS | Arch Linux (Omarchy), kernel 6.x, Hyprland / Wayland |
| GPU driver | `nvidia-open-dkms` 610.57.04, Vulkan 1.4 |
| Unreal Engine | **5.6.1** (Epic's precompiled Linux binary) |
| CesiumForUnreal | **v2.29.1** (build for UE 5.6) |
| Pyrano plugin | vendored as a git subtree of <https://github.com/victorbo4/Pyrano> |

Windows should also work (the project originated on Windows) but is untested for
this revision. The editor on Linux runs through XWayland; there is no native
Wayland support in UE 5.6.

---

## 2. One-time setup

### 2.1 Unreal Engine 5.6.1

Download the **precompiled Linux binary** from
<https://www.unrealengine.com/en-US/linux> (needs a free Epic account; accept the
EULA). Extract it, e.g. to `~/UnrealEngine/5.6/`. It bundles its own clang
toolchain and .NET runtime — no system `dotnet`/`clang` packages are required.

Register the install so the `.uproject` can find it:

```bash
mkdir -p ~/.config/Epic/UnrealEngine
printf '[Installations]\n5.6=%s\n' "$HOME/UnrealEngine/5.6" >> ~/.config/Epic/UnrealEngine/Install.ini
```

System packages (Arch names): `nvidia-utils vulkan-icd-loader xorg-xwayland`.

### 2.2 System packages for a clean checkout

```bash
sudo pacman -S --needed vulkan-tools lld git-lfs   # or your distro's equivalents
```

### 2.3 Clone and get the plugin

```bash
git clone <this repo> ue-irradiance-modeling
cd ue-irradiance-modeling
```

The Pyrano plugin is already vendored under `simulation/Plugins/Pyrano/` (git
subtree). To pull a newer plugin version later:

```bash
git subtree pull --prefix=simulation/Plugins/Pyrano \
  https://github.com/victorbo4/Pyrano.git main --squash
```

### 2.4 CesiumForUnreal

Not delivered through the Epic marketplace on Linux. Download the **UE 5.6**
release asset from <https://github.com/CesiumGS/cesium-unreal/releases>
(`CesiumForUnreal-*-56-*.zip`) and extract it into `simulation/Plugins/`:

```bash
unzip CesiumForUnreal-56-v2.29.1.zip -d simulation/Plugins/
```

`simulation/Plugins/CesiumForUnreal/` is git-ignored — it is a downloaded
dependency, pinned by version here, not vendored.

### 2.5 Cesium ion token

The committed `Azotea_ETSIDI` level streams **Google Photorealistic 3D Tiles**
(Cesium ion asset `2275207`), which cast the building shadows on the sensors.
The level carries a working read-only token, but it belongs to the original
author's account and may be revoked. To use your own:

1. Create a free account at <https://ion.cesium.com/> and copy a token from
   **Access Tokens** (scope `assets:read` is enough).
2. In the editor, open the `Azotea_ETSIDI` level, select each `Cesium3DTileset`
   actor, and paste the token into **Ion Access Token** in the Details panel.
   Save the level.
3. **Do not** use the in-editor *Connect to Cesium ion* button — its OAuth
   browser pop-up crashes the Vulkan swapchain on XWayland/NVIDIA.

### 2.6 Build the C++ modules

```bash
UEBF=~/UnrealEngine/5.6/Engine/Build/BatchFiles/Linux
"$UEBF/Build.sh" PyranoDemoEditor Linux Development \
  -project="$PWD/simulation/PyranoDemo.uproject" -waitmutex
```

First build takes a few minutes (a shared editor PCH dominates). Optionally
regenerate IDE/clangd files:

```bash
"$UEBF/GenerateProjectFiles.sh" -project="$PWD/simulation/PyranoDemo.uproject" -game -vscode
```

---

## 3. Running the editor

Use the wrapper scripts — they set the XWayland / NVIDIA workarounds
(`SDL_VIDEODRIVER=x11`, `__GL_MaxFramesAllowed=1`,
`__GL_THREADED_OPTIMIZATIONS=0`) that keep the Vulkan swapchain from crashing on
window events.

```bash
./simulation/Scripts/run-editor.sh                                  # normal
./simulation/Scripts/run-editor.sh /Game/Azotea_ETSIDI/Levels/Azotea_ETSIDI   # open a level directly
./simulation/Scripts/run-editor-debug.sh                            # + RDG debug + verbose render logs
```

`UE_ROOT` overrides the engine location (defaults to `~/UnrealEngine/5.6`).

Under the hood a script is just:
`~/UnrealEngine/5.6/Engine/Binaries/Linux/UnrealEditor  <path>/simulation/PyranoDemo.uproject  [map]`.

---

## 4. Running a simulation

1. Launch the editor and open **`Azotea_ETSIDI`**
   (`Content/Azotea_ETSIDI/Levels/`).
2. Let the Cesium tiles stream in: press **Play**, fly around the rooftop for
   10–15 s until the surrounding buildings appear, press **Esc**.
3. **Tools → Pyrano Capture Planner.**
4. **Load Plan** → browse to `config/plans/madrid_2025-10-07.json`.
   The Planner UI is known to mis-display *North Offset* and *Altitude Meters*
   after a load — verify the panel against this table and re-enter by hand if
   they read `0` / `500`:

   | Field | Value | Source |
   |---|---|---|
   | Latitude | `40.414` | level `SunSky` actor |
   | Longitude | `-3.681` | level `SunSky` actor |
   | Timezone | `2` | Europe/Madrid, DST on 2025-10-07 |
   | North Offset | `-90` | level `SunSky` actor |
   | Start / End | `2025-10-07 00:00` → `23:59` | full day |
   | Sample Interval | `2 min` | matches the ML dataset resolution |
   | Warmup Frames | `8` | |
   | Resolution Px | `512` | cubemap face size |
   | Linke Turbidity | `4.0` | thesis (§ clear-sky) |
   | Altitude Meters | `734` | site elevation |
   | Min Sun Altitude | `0` | |
   | Export CSV | on | |
   | Export Images | off | ~10 GB of EXR otherwise |

5. In the World Outliner, enable the sensors to simulate. The thesis used
   **P0 (CLASE_A), P1, P4, Pinc (Inclinado)**; the plan does **not** store the
   sensor selection.
6. **Start Simulation** — do not close the editor until it finishes
   (~30–40 min for one full day × 4 sensors).

Output: `simulation/Saved/Irradiance/irradiance_<timestamp>.csv`
(one row per sensor × timestep; column `irr_final_normalized_wm2` is the
simulated global horizontal irradiance). `simulation/Saved/` is git-ignored.

The thesis campaign covered 2025-04-11, 2025-04-20 and **2025-10-07**; the last
is the shadow-validation day.

---

## 5. Reproducing the analysis

See `analysis/README_ML.md`. Short version:

```bash
cd analysis
python -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
```

Run the notebooks in order (`notebooks/02_baselines` → `03_model_final` →
`04_model_comparison`). **Do not re-run `notebooks/01_generate_dataset.ipynb`** —
the built dataset (`data/dataset_master_tfg.csv`) is committed; the raw Solcast /
pyranometer inputs are not.

---

## 6. Known caveats

- **Cesium World Terrain / Google 3D Tiles are not version-pinned.** Cesium
  refreshes them over time, so building geometry — and therefore the exact
  shadow onset times — can drift by tens of minutes from the thesis figures.
  Cite the access date.
- **Re-runs reproduce the thesis irradiance to ~±3 %.** Engine minor version,
  ray-tracing path and tile LOD all contribute.
- **Sun position quantizes to ~2-minute steps** in the current plugin (azimuth
  repeats for pairs of 1-minute samples). Irrelevant at hourly resolution,
  visible at the 2-minute resolution used here.
- **Maximum Screen Space Error** of the Google 3D Tiles changes the fidelity of
  the shadow-casting geometry. The committed level's value is the reproduction
  baseline; do not lower it for "quick" runs if you intend to compare numbers.
