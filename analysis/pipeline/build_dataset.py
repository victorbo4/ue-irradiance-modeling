"""Build the v2 training dataset from the simulation campaign, the pyranometers
and Solcast.

One row per (sensor, UTC timestamp), on the simulation's 2-minute grid. Every
time is UTC. Nothing is dropped: data-quality problems are marked in ``qc_*``
boolean columns and the *evaluation* decides, once, which mask to apply.

    python analysis/pipeline/build_dataset.py

See analysis/pipeline/README.md for the column reference and QC definitions.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
import pandas as pd

REPO = Path(__file__).resolve().parents[2]

LOCAL_TZ = "Europe/Madrid"
STEP = pd.Timedelta("2min")
HALF_STEP = STEP / 2
SAMPLES_PER_BIN = 24  # 5 s logger -> 2 min

# Simulator sensor names -> the pyranometer column they correspond to.
SENSOR_MAP = {
    "CLASE_A": "P0",
    "INCLINADO": "Pinc",
    **{f"P{i}": f"P{i}" for i in range(1, 9)},
}
REAL_COLS = list(SENSOR_MAP.values())

SIM_COLS = {
    "sensor_name": "sensor",
    "utc": "utc",
    "pos_x": "pos_x", "pos_y": "pos_y", "pos_z": "pos_z",
    "n_x": "n_x", "n_y": "n_y", "n_z": "n_z",
    "azimuth_deg": "sun_azimuth_deg",
    "altitude_deg": "sun_altitude_deg",
    "clearsky_ghi_wm2": "clearsky_ghi_wm2",
    "clearsky_dni_wm2": "clearsky_dni_wm2",
    "clearsky_dhi_wm2": "clearsky_dhi_wm2",
    "geometric_factor": "geometric_factor",
    "sun_occluded": "sun_occluded",
    "sun_hit_distance_m": "sun_hit_distance_m",
    "sun_visibility": "sun_visibility",
    "sky_view_factor": "sky_view_factor",
    # Raw readbacks keep their historical names; the `_lux` suffix is a
    # misnomer (bug-tracker S9/A6) and is documented in the README rather than
    # silently renamed here.
    "raw_r_lux": "raw_r_lux", "raw_g_lux": "raw_g_lux", "raw_b_lux": "raw_b_lux",
    "sim_comp_amb_lux": "sim_comp_amb_lux",
    "sim_comp_direct_lux": "sim_comp_direct_lux",
    "irr_final_normalized_wm2": "sim_irradiance_wm2",
}

# Solcast columns carried into the dataset (cloud_opacity is the model input;
# ghi is Solcast's own estimate, kept as a baseline to compare against).
METEO_COLS = ["cloud_opacity", "ghi", "clearsky_ghi"]
METEO_RENAME = {"ghi": "solcast_ghi_wm2", "clearsky_ghi": "solcast_clearsky_ghi_wm2"}

# --- QC thresholds (documented in the README) ---------------------------------
MIN_BIN_SAMPLES = SAMPLES_PER_BIN // 2   # fewer 5 s samples than this -> sparse
DUP_CONFLICT_WM2 = 5.0                    # duplicated timestamps disagreeing by more
NEG_LARGE_WM2 = -5.0                      # below this is not offset noise
NIGHT_NONZERO_WM2 = 10.0                  # real signal while the sun is below horizon
DEAD_DAY_MAX_WM2 = 20.0                   # sensor-day whose max never exceeds this
DEAD_DAY_SKY_WM2 = 300.0                  #   ... while the clear-sky GHI exceeded this
ZERO_DAYLIGHT_SIM_WM2 = 50.0              # exact zero while the simulation says > this
FLATLINE_MIN_BINS = 15                    # identical value for >= 30 min
FLATLINE_MIN_WM2 = 5.0
OVER_CLEARSKY_FACTOR = 1.6                # real > factor * ceiling + offset, where the
                                          # ceiling is max(clear-sky GHI, the sensor's own
                                          # simulated irradiance) so a tilted sensor is
                                          # not held to the horizontal GHI
OVER_CLEARSKY_OFFSET = 50.0

QC_BAD = [
    "qc_real_sparse", "qc_dup_conflict", "qc_sensor_dead", "qc_flatline",
    "qc_neg_large", "qc_night_nonzero", "qc_over_clearsky", "qc_zero_daylight",
]


# ------------------------------------------------------------------ loading --

def load_sim(sim_dir: Path) -> pd.DataFrame:
    files = sorted(Path(sim_dir).glob("campaign_*.csv"))
    if not files:
        raise FileNotFoundError(f"no campaign_*.csv in {sim_dir}")
    df = pd.concat((pd.read_csv(f, usecols=list(SIM_COLS)) for f in files), ignore_index=True)
    df = df.rename(columns=SIM_COLS)
    df["utc"] = pd.to_datetime(df["utc"], utc=True)
    df["sensor"] = df["sensor"].map(SENSOR_MAP)
    if df["sensor"].isna().any():
        raise ValueError("simulation contains a sensor with no pyranometer mapping")
    if df.duplicated(["sensor", "utc"]).any():
        raise ValueError("duplicated (sensor, utc) rows in the simulation output")
    return df


def load_real(real_dir: Path, wanted_bins: pd.DatetimeIndex) -> pd.DataFrame:
    """All pyranometer files -> UTC, deduplicated, binned to the 2-min grid.

    Files are read together and merged by timestamp, not matched by file name:
    some files straddle midnight or repeat another file's rows.

    Returns a long frame indexed by (utc, sensor) with ``real_wm2`` (bin mean),
    ``real_n`` (5 s samples in the bin), ``real_std`` and ``real_dup_conflict``.
    """
    files = sorted(Path(real_dir).glob("rad_*.csv"))
    if not files:
        raise FileNotFoundError(f"no rad_*.csv in {real_dir}")
    raw = pd.concat((pd.read_csv(f, usecols=["timestamp", *REAL_COLS]) for f in files), ignore_index=True)

    # Wall-clock Madrid -> UTC. 'raise' on DST ambiguity: the logger runs
    # 06:00-22:00 local, far from both transitions, so any hit is a real problem.
    local = pd.to_datetime(raw["timestamp"])
    raw["utc"] = local.dt.tz_localize(LOCAL_TZ, ambiguous="raise", nonexistent="raise").dt.tz_convert("UTC")
    raw = raw.drop(columns="timestamp")

    # Bin first, on wide data: [t - 1 min, t + 1 min) is labelled t.
    raw["bin"] = (raw["utc"] + HALF_STEP).dt.floor(STEP)
    raw = raw[raw["bin"].isin(wanted_bins)]

    g = raw.groupby(["bin", "utc"])[REAL_COLS]
    uniq = g.mean()
    spread = g.max() - g.min()              # >0 only where a timestamp was repeated
    uniq_conflict = spread > DUP_CONFLICT_WM2
    # The repeated timestamps are collapsed to their mean above; now bin.
    gb = uniq.groupby(level="bin")
    mean = gb.mean()
    count = gb.count()
    std = gb.std()
    conflict = uniq_conflict.groupby(level="bin").any()

    out = pd.concat(
        {"real_wm2": mean, "real_n": count, "real_std": std, "real_dup_conflict": conflict}, axis=1
    )
    out = out.stack(level=1, future_stack=True)  # -> (bin, sensor)
    out.index = out.index.set_names(["utc", "sensor"])
    return out


def load_meteo(path: Path) -> pd.DataFrame:
    """Solcast, re-anchored to the middle of each averaging period.

    A 5-minute row stamped ``period_end`` is the mean over the *preceding* five
    minutes, so it describes ``period_end - 2.5 min``.
    """
    m = pd.read_csv(path, parse_dates=["period_end"])
    m["center"] = m["period_end"] - pd.Timedelta("150s")
    return m.sort_values("center").reset_index(drop=True)


def meteo_at(meteo: pd.DataFrame, times: pd.DatetimeIndex) -> pd.DataFrame:
    """Linear interpolation of the numeric Solcast columns at ``times``.

    Also returns ``meteo_gap_s`` (width of the observation interval bracketing
    each time, 300 s when nothing is missing; NaN outside the file) and
    ``weather_type`` taken from the nearest observation.
    """
    c = meteo["center"].astype("int64").to_numpy() / 1e9
    t = times.astype("int64").to_numpy() / 1e9
    out = {}
    for col in METEO_COLS:
        out[METEO_RENAME.get(col, col)] = np.interp(t, c, meteo[col].to_numpy(float), left=np.nan, right=np.nan)
    hi = np.searchsorted(c, t)
    inside = (hi > 0) & (hi < len(c))
    gap = np.full(len(t), np.nan)
    gap[inside] = c[hi[inside]] - c[hi[inside] - 1]
    out["meteo_gap_s"] = gap
    nearest = np.clip(np.where(np.abs(c[np.clip(hi, 0, len(c) - 1)] - t) < np.abs(c[np.clip(hi - 1, 0, len(c) - 1)] - t), hi, hi - 1), 0, len(c) - 1)
    out["weather_type"] = meteo["weather_type"].to_numpy()[nearest]
    return pd.DataFrame(out, index=times)


# ---------------------------------------------------------------------- QC ---

def add_qc(df: pd.DataFrame) -> pd.DataFrame:
    """Add qc_* columns (True = problem) and ``qc_ok`` (no problem at all)."""
    df = df.sort_values(["sensor", "utc"]).reset_index(drop=True)
    real = df["real_wm2"]

    df["qc_real_sparse"] = df["real_n"].fillna(0) < MIN_BIN_SAMPLES
    df["qc_dup_conflict"] = df.pop("real_dup_conflict").astype("boolean").fillna(False).astype(bool)
    df["qc_neg_large"] = real < NEG_LARGE_WM2
    df["qc_night_nonzero"] = (df["sun_altitude_deg"] < 0) & (real > NIGHT_NONZERO_WM2)
    # Logger zero-fill (or a total blackout): every 5 s sample exactly 0 although
    # the simulated sun is up. A real dark cloud still reads a little above 0.
    df["qc_zero_daylight"] = (
        (real == 0) & (df["real_std"] == 0) & (df["sun_altitude_deg"] > 0)
        & (df["sim_irradiance_wm2"] > ZERO_DAYLIGHT_SIM_WM2) & ~df["qc_real_sparse"]
    )
    ceiling = np.maximum(df["clearsky_ghi_wm2"], df["sim_irradiance_wm2"])
    df["qc_over_clearsky"] = real > OVER_CLEARSKY_FACTOR * ceiling + OVER_CLEARSKY_OFFSET

    # Dead sensor: a (sensor, local day) whose reading never rises although the
    # sky model says the sun was strong. Whole day flagged.
    day = df["utc"].dt.tz_convert(LOCAL_TZ).dt.date
    g = df.assign(_day=day).groupby(["sensor", "_day"])
    day_max = g["real_wm2"].transform("max")
    sky_max = g["clearsky_ghi_wm2"].transform("max")
    df["date_local"] = day
    df["qc_sensor_dead"] = (day_max < DEAD_DAY_MAX_WM2) & (sky_max > DEAD_DAY_SKY_WM2)

    # Flatline: same non-trivial value for >= 30 consecutive minutes.
    new_run = (real != real.groupby(df["sensor"]).shift()) | (df["sensor"] != df["sensor"].shift())
    run_id = new_run.cumsum()
    run_len = real.groupby(run_id).transform("size")
    df["qc_flatline"] = (run_len >= FLATLINE_MIN_BINS) & (real > FLATLINE_MIN_WM2)

    df["qc_ok"] = ~df[QC_BAD].any(axis=1)
    return df


# --------------------------------------------------------------------- build --

def build(sim_dir: Path, real_dir: Path, meteo_path: Path) -> pd.DataFrame:
    sim = load_sim(sim_dir)
    bins = pd.DatetimeIndex(sim["utc"].unique())
    real = load_real(real_dir, bins)
    df = sim.merge(real.reset_index(), on=["utc", "sensor"], how="left")

    met = meteo_at(load_meteo(meteo_path), pd.DatetimeIndex(df["utc"].unique()))
    df = df.merge(met.rename_axis("utc").reset_index(), on="utc", how="left")

    df = add_qc(df)
    df = df.sort_values(["utc", "sensor"]).reset_index(drop=True)
    return df


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _git_commit() -> str:
    try:
        return subprocess.check_output(["git", "-C", str(REPO), "rev-parse", "HEAD"], text=True).strip()
    except Exception:
        return "unknown"


def manifest(df: pd.DataFrame, sim_dir: Path, real_dir: Path, meteo_path: Path) -> dict:
    inputs = {
        "simulation": {p.name: _sha256(p) for p in sorted(Path(sim_dir).glob("campaign_*.csv"))},
        "meteo": {Path(meteo_path).name: _sha256(meteo_path)},
        # only the real files that actually contributed rows are interesting,
        # but hashing all of them is cheap and makes the manifest self-contained
        "pyranometers": {p.name: _sha256(p) for p in sorted(Path(real_dir).glob("rad_*.csv"))},
    }
    return {
        "git_commit": _git_commit(),
        "rows": int(len(df)),
        "days": int(df["date_local"].nunique()),
        "sensors": sorted(df["sensor"].unique()),
        "utc_range": [df["utc"].min().isoformat(), df["utc"].max().isoformat()],
        "thresholds": {
            "min_bin_samples": MIN_BIN_SAMPLES, "dup_conflict_wm2": DUP_CONFLICT_WM2,
            "neg_large_wm2": NEG_LARGE_WM2, "night_nonzero_wm2": NIGHT_NONZERO_WM2, "zero_daylight_sim_wm2": ZERO_DAYLIGHT_SIM_WM2,
            "dead_day_max_wm2": DEAD_DAY_MAX_WM2, "dead_day_sky_wm2": DEAD_DAY_SKY_WM2,
            "flatline_min_bins": FLATLINE_MIN_BINS, "flatline_min_wm2": FLATLINE_MIN_WM2,
            "over_clearsky_factor": OVER_CLEARSKY_FACTOR, "over_clearsky_offset": OVER_CLEARSKY_OFFSET,
        },
        "qc_counts": {c: int(df[c].sum()) for c in [*QC_BAD, "qc_ok"]},
        "inputs": inputs,
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--sim-dir", type=Path, default=REPO / "simulation/Saved/Irradiance/campaign_output")
    ap.add_argument("--real-dir", type=Path, default=REPO / "analysis/data/pyranometers")
    ap.add_argument("--meteo", type=Path, default=REPO / "analysis/data/meteo/meteo_utc_2025.csv")
    ap.add_argument("--out", type=Path, default=REPO / "analysis/data/dataset_v2.csv")
    args = ap.parse_args()

    df = build(args.sim_dir, args.real_dir, args.meteo)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    df.to_csv(args.out, index=False, date_format="%Y-%m-%dT%H:%M:%SZ")
    man_path = args.out.with_name(args.out.stem + "_manifest.json")
    man_path.write_text(json.dumps(manifest(df, args.sim_dir, args.real_dir, args.meteo), indent=2))
    print(f"{len(df)} rows -> {args.out}\nmanifest -> {man_path}")
    print(df[[*QC_BAD, "qc_ok"]].sum().to_string())


if __name__ == "__main__":
    main()
