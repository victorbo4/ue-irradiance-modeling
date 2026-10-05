"""Summarise the QC of dataset_v2.csv into a markdown report.

    python analysis/pipeline/qc_report.py

Reads analysis/data/dataset_v2.csv, writes analysis/data/dataset_v2_qc.md. Only
aggregates go in the report (no raw measurements), so it can be committed.
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

from build_dataset import QC_BAD, REPO

DAYLIGHT_DEG = 5.0
USABLE_DAY_FRACTION = 0.8   # a (sensor, day) is usable if >= 80 % of its daylight rows are qc_ok
MAX_LAG_BINS = 15
CLEAR_CLOUD_MAX = 8.0       # median Solcast cloud_opacity (%) for a "clear" day


def md(df: pd.DataFrame, index: bool = True) -> str:
    d = df.reset_index() if index else df
    head = "| " + " | ".join(map(str, d.columns)) + " |\n|" + "---|" * len(d.columns) + "\n"
    return head + "\n".join("| " + " | ".join(map(str, r)) + " |" for r in d.itertuples(index=False)) + "\n"


def lag_by_day(df: pd.DataFrame) -> pd.DataFrame:
    """Best real-vs-simulation time shift per clear day, on the open-sky reference P0.

    The shift minimises the mean absolute difference between the two curves
    after normalising each to its own daily maximum. Positive = the real signal
    must be moved later to match.
    """
    p = df[(df.sensor == "P0") & (df.sun_altitude_deg > 8) & ~df.qc_real_sparse]
    rows = []
    for day, g in p.groupby("date_local"):
        if len(g) < 200 or g["cloud_opacity"].median() > CLEAR_CLOUD_MAX:
            continue
        g = g.set_index("utc").sort_index()
        sim = g["sim_irradiance_wm2"] / g["sim_irradiance_wm2"].max()
        best = min(
            ((L, (g["real_wm2"].shift(-L).dropna() / g["real_wm2"].max() - sim).abs().dropna().mean())
             for L in range(-MAX_LAG_BINS, MAX_LAG_BINS + 1)),
            key=lambda x: x[1],
        )
        rows.append({"day": day, "lag_min": best[0] * 2, "mae": round(best[1], 3)})
    return pd.DataFrame(rows)


def report(df: pd.DataFrame) -> str:
    day_rows = df[df.sun_altitude_deg > DAYLIGHT_DEG]
    out = ["# dataset_v2 — QC report", "",
           f"{len(df):,} rows = {df.date_local.nunique()} days x {df.sensor.nunique()} sensors on a 2-min UTC grid. "
           "QC **marks** rows, it never removes them; the evaluation applies one common mask.", ""]

    out += ["## Rows flagged, by sensor (daylight rows, sun > 5 deg)", ""]
    t = day_rows.groupby("sensor")[QC_BAD].sum()
    t["n_daylight"] = day_rows.groupby("sensor").size()
    t["qc_ok_%"] = (100 * day_rows.groupby("sensor")["qc_ok"].mean()).round(1)
    out += [md(t), ""]

    out += ["## Usable (sensor, day) pairs", "",
            f"A pair is usable when >= {USABLE_DAY_FRACTION:.0%} of its daylight rows are `qc_ok`.", ""]
    frac = day_rows.groupby(["sensor", "date_local"])["qc_ok"].mean().rename("f").reset_index()
    frac["usable"] = frac.f >= USABLE_DAY_FRACTION
    u = frac.groupby("sensor")["usable"].sum().astype(int).to_frame("usable_days")
    out += [md(u), ""]
    bad = frac[~frac.usable].groupby("sensor")["date_local"].apply(lambda s: ", ".join(map(str, s)))
    for s, days in bad.items():
        out.append(f"- **{s}** unusable: {days}")
    out.append("")

    out += ["## Days with truncated real data", ""]
    cov = day_rows[day_rows.sensor == "P0"].groupby("date_local").apply(
        lambda g: pd.Series({"real_coverage_%": round(100 * (~g.qc_real_sparse).mean(), 1),
                             "first_real_utc": g.loc[~g.qc_real_sparse, "utc"].min().strftime("%H:%M") if (~g.qc_real_sparse).any() else "-",
                             "last_real_utc": g.loc[~g.qc_real_sparse, "utc"].max().strftime("%H:%M") if (~g.qc_real_sparse).any() else "-"}),
        include_groups=False)
    out += [md(cov[cov["real_coverage_%"] < 95]), ""]

    out += ["## Time alignment (real vs simulation, P0, clear days)", ""]
    lag = lag_by_day(df)
    if len(lag):
        out += [f"{len(lag)} clear days. Median shift {lag.lag_min.median():+.0f} min, "
                f"IQR [{lag.lag_min.quantile(.25):+.0f}, {lag.lag_min.quantile(.75):+.0f}] min, "
                f"range [{lag.lag_min.min():+.0f}, {lag.lag_min.max():+.0f}] min. "
                "No systematic offset means the local-to-UTC conversion is right.", ""]

    out += ["## Simulated vs measured on clear daylight (P0, qc_ok, cloud_opacity < 8 %)", ""]
    c = day_rows[(day_rows.sensor == "P0") & day_rows.qc_ok & (day_rows.cloud_opacity < CLEAR_CLOUD_MAX)
                 & (day_rows.sim_irradiance_wm2 > 100)]
    r = c.real_wm2 / c.sim_irradiance_wm2
    out += [f"{len(c):,} rows. real/sim ratio: median {r.median():.3f}, P10 {r.quantile(.1):.3f}, P90 {r.quantile(.9):.3f}. "
            "A calibrated, unbiased simulator would centre on 1.", ""]
    return "\n".join(out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dataset", type=Path, default=REPO / "analysis/data/dataset_v2.csv")
    ap.add_argument("--out", type=Path, default=REPO / "analysis/data/dataset_v2_qc.md")
    args = ap.parse_args()
    df = pd.read_csv(args.dataset, parse_dates=["utc"], dtype={"weather_type": "string"})
    df["date_local"] = pd.to_datetime(df["date_local"]).dt.date
    args.out.write_text(report(df))
    print(f"report -> {args.out}")


if __name__ == "__main__":
    main()
