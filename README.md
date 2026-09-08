# Solar irradiance modeling with Unreal Engine and machine learning

Code and data from the bachelor's thesis of Víctor Blázquez Otero, *Modelización
de la irradiancia solar en sistemas fotovoltaicos con motores de videojuegos*
(Universidad Politécnica de Madrid, 2026).

The idea: render a georeferenced 3D model of a real rooftop in Unreal Engine,
capture the hemispherical radiance at each virtual pyranometer, integrate it into
an irradiance value, and use that physical simulation as a feature for a
machine-learning model that predicts the measured irradiance — including the
partial shading from surrounding buildings that analytic clear-sky models miss.

## Pipeline

```
 UE 5.6 + Pyrano plugin            analysis/ (Python)
 ┌───────────────────────┐         ┌──────────────────────────┐
 │ Azotea ETSIDI scene   │  CSV    │ dataset assembly         │
 │ + Cesium 3D Tiles     ├────────►│ (sim + Solcast + sensors)│
 │ → cubemap capture     │         │ → XGBoost model          │
 │ → irradiance integral │         │ → validation vs measured │
 └───────────────────────┘         └──────────────────────────┘
```

## Layout

| Path | What |
|---|---|
| `simulation/` | The Unreal Engine 5.6 project (`PyranoDemo.uproject`), the ETSIDI rooftop scene, and the **Pyrano** plugin (git subtree of [victorbo4/Pyrano](https://github.com/victorbo4/Pyrano)) under `Plugins/Pyrano/` |
| `analysis/` | The Python / Jupyter pipeline: dataset assembly, baselines, the final model, and its validation. See `analysis/README.md` |
| `config/plans/` | Simulation plans (`FSimConfig` JSON) for the Pyrano Capture Planner |
| `paper/` | Manuscript sources — kept in a separate repository, not tracked here |

## Reproducing

See **[REPRODUCE.md](REPRODUCE.md)** for the full recipe: engine setup, building
the C++ modules, the Cesium ion token, running a simulation, and re-running the
analysis notebooks.

## The Pyrano plugin

`Pyrano` is a standalone, reusable Unreal Engine plugin — a virtual pyranometer
and irradiance-analysis framework — developed independently at
<https://github.com/victorbo4/Pyrano> and vendored here at a pinned revision.
API docs regenerate with `doxygen Doxyfile` from `simulation/Plugins/Pyrano/`.
