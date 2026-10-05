"""Synthetic rows shared by the evaluation tests."""
import numpy as np
import pandas as pd


def make_df(n=1500, seed=0, sensors=("P0", "P1", "P3"), a=0.0075, months=("2025-04", "2025-05", "2025-06", "2025-07")):
    """Rows with a known law: real = sim * (1 - a * cloud_opacity)."""
    rng = np.random.RandomState(seed)
    alt = rng.uniform(6, 70, n)
    cs = 900 * np.sin(np.radians(alt)) + 30
    cloud = rng.uniform(0, 100, n)
    sim = cs * rng.uniform(0.85, 1.0, n)
    day = rng.randint(1, 29, n)
    month = rng.choice(months, n)
    return pd.DataFrame({
        "sensor": rng.choice(sensors, n),
        "date_local": [f"{mo}-{d:02d}" for mo, d in zip(month, day)],
        "sun_altitude_deg": alt, "sun_azimuth_deg": rng.uniform(80, 280, n),
        "clearsky_ghi_wm2": cs, "clearsky_dni_wm2": 0.8 * cs, "clearsky_dhi_wm2": 0.2 * cs,
        "geometric_factor": np.sin(np.radians(alt)) * (rng.rand(n) < 0.9),
        "sky_view_factor": 0.95, "sim_irradiance_wm2": sim, "solcast_ghi_wm2": cs * 0.9,
        "cloud_opacity": cloud, "precipitable_water": rng.uniform(10, 30, n),
        "real_wm2": sim * (1 - a * cloud),
    })
