"""A small synthetic dataset with the structure of dataset_v2, for ``run --smoke`` and for tests.

It has every column the models and splits read, nine sensors, nine months (April-December),
two training days and one test day per month, and a deterministic law (``real = sim * (1 - a *
cloud)``) so the pipeline can be exercised end to end in seconds. It carries no real data.
"""
from __future__ import annotations

import numpy as np
import pandas as pd

from .common import SENSORS
from .models import XGBConfig

MONTHS = [f"2025-{m:02d}" for m in range(4, 13)]

# Tiny grids, so a smoke run takes seconds. Real runs use selection.HD_GRID and C_GRID.
SMOKE_GRID_HD = [XGBConfig(2, 100, 20, 0.5), XGBConfig(3, 300, 20, 1.0)]
SMOKE_GRID_C = [XGBConfig(2, 100, 20, 0.8), XGBConfig(3, 100, 70, 0.8)]


def make_dataset(seed: int = 0, rows_per: int = 6, a: float = 0.0075) -> tuple[pd.DataFrame, pd.DataFrame]:
    """Returns (dataset, split) shaped like dataset_v2 and split_v2."""
    rng = np.random.RandomState(seed)
    days, split = [], {}
    for mo in MONTHS:
        for d, kind in ((3, "train"), (11, "train"), (20, "test")):
            days.append(f"{mo}-{d:02d}")
            split[days[-1]] = kind
    sky = {d: ["sunny", "mixed", "cloudy", "rainy"][i % 4] for i, d in enumerate(days)}
    rows = []
    for d in days:
        for s in SENSORS:
            for k in range(rows_per):
                alt = rng.uniform(6, 70)
                cs = 900 * np.sin(np.radians(alt)) + 30
                cloud = rng.uniform(0, 100)
                sim = cs * rng.uniform(0.85, 1.0)
                vis = rng.choice([1.0, 1.0, 1.0, 0.7, 0.3, 0.0])
                rows.append({
                    "utc": f"{d}T{10 + k:02d}:00:00Z", "date_local": d, "sensor": s, "qc_ok": True,
                    "sun_altitude_deg": alt, "sun_azimuth_deg": rng.uniform(80, 280), "sun_visibility": vis,
                    "clearsky_ghi_wm2": cs, "clearsky_dni_wm2": 0.8 * cs, "clearsky_dhi_wm2": 0.2 * cs,
                    "geometric_factor": np.sin(np.radians(alt)) * (vis == 1.0), "sky_view_factor": 0.95,
                    "sim_irradiance_wm2": sim, "solcast_ghi_wm2": 0.9 * cs, "cloud_opacity": cloud,
                    "precipitable_water": rng.uniform(10, 30), "real_wm2": sim * (1 - a * cloud),
                })
    return pd.DataFrame(rows), pd.DataFrame({"split": pd.Series(split), "sky": pd.Series(sky)})
