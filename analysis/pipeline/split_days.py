"""Choose the 15 held-out test days.

    python analysis/pipeline/split_days.py

Writes analysis/data/split_v2.csv (one row per day: split + descriptors) and
analysis/data/split_v2.md (review tables). Deterministic for a given seed.

The split is made on *days*, never on rows or sensors: the rows of a day share
one sky and one set of clouds. Days are described by the measured clearness of
the sky and by whether the P1 shadow is present, then 15 are drawn so that the
test set covers every sky class, both P1-shadow regimes and as many months as
possible. The three days used to tune the TFG are always in the test set.
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

from build_dataset import REPO

SEED = 42
N_DRAWS = 20000

QUOTA = {"sunny": 3, "mixed": 4, "cloudy": 4, "rainy": 4}   # sums to 15
TFG_DAYS = ["2025-04-11", "2025-04-20", "2025-10-07"]

OPEN_SENSORS = ["P0", "P3", "P4", "P6", "P8"]   # horizontal, little shade above 20 deg
SHADE_SENSORS = ["P1", "P3", "P5", "P7"]
REQUIRED_OK_SENSORS = ["P0", "P1", "P3", "P5", "P7"]
CLEARNESS_MIN_ALT = 20.0                         # deg, for the sky classification
MIN_DAY_COVERAGE = 0.8      # share of daylight bins with real data to be usable at all
MIN_TEST_OK = 0.9           # share of qc_ok daylight rows, per required sensor, for a test day
P1_SHADE_ROWS = 10          # >= this many P1 rows with sun_visibility < 0.5 -> "P1 shaded" day


def classify(k_med: float, clear: float) -> str:
    """Sky class from the day's median clearness and its share of clear-sky time."""
    if k_med < 0.4:
        return "rainy"
    if clear >= 0.85:
        return "sunny"
    if clear >= 0.25:
        return "mixed"
    return "cloudy"


def day_table(df: pd.DataFrame) -> pd.DataFrame:
    day = df[df.sun_altitude_deg > 5]

    cov = day[day.sensor == "P0"].groupby("date_local")["qc_real_sparse"].apply(lambda s: 1 - s.mean())

    # Sky: clearness k = measured / clear-sky GHI, median over the open sensors per
    # timestamp (one k per instant), then summarised per day.
    o = day[day.sensor.isin(OPEN_SENSORS) & day.qc_ok & (day.sun_altitude_deg > CLEARNESS_MIN_ALT)]
    k = (o.assign(k=o.real_wm2 / o.clearsky_ghi_wm2)
          .groupby(["date_local", "utc"])["k"].median().reset_index())
    g = k.groupby("date_local")["k"]
    out = pd.DataFrame({
        "k_med": g.median(), "clear_frac": g.apply(lambda s: (s > 0.85).mean()),
        "dark_frac": g.apply(lambda s: (s < 0.3).mean()),
    })
    out["real_coverage"] = cov
    out["sky"] = [classify(r.k_med, r.clear_frac) for r in out.itertuples()]

    shaded = day[day.sun_visibility < 0.5].groupby(["date_local", "sensor"]).size().unstack(fill_value=0)
    for s in SHADE_SENSORS:
        out[f"shade_{s}"] = shaded[s].reindex(out.index).fillna(0).astype(int) if s in shaded else 0
    out["p1_shaded"] = out["shade_P1"] >= P1_SHADE_ROWS

    for s in REQUIRED_OK_SENSORS:
        out[f"ok_{s}"] = day[day.sensor == s].groupby("date_local")["qc_ok"].mean().reindex(out.index)
    out["blackout_rows"] = df[df.qc_zero_daylight].groupby("date_local").size().reindex(out.index).fillna(0).astype(int)
    out["month"] = [d[:7] for d in out.index]
    out["tfg_day"] = out.index.isin(TFG_DAYS)
    out["usable"] = out["real_coverage"] >= MIN_DAY_COVERAGE
    out["test_eligible"] = (out["usable"] & (out["blackout_rows"] == 0)
                            & (out[[f"ok_{s}" for s in REQUIRED_OK_SENSORS]] >= MIN_TEST_OK).all(axis=1))
    return out


def select_test_days(days: pd.DataFrame, seed: int = SEED, n_draws: int = N_DRAWS) -> list[str]:
    """Random search under hard constraints, keeping the draw with the most months.

    Hard: class quotas; TFG days included; each class has at least one P1-shaded
    and one P1-unshaded test day (when its quota allows both).
    Preference: more distinct months, then a flatter month histogram.
    """
    rng = np.random.RandomState(seed)
    pool = days[days.test_eligible]
    forced = [d for d in TFG_DAYS]
    missing = [d for d in forced if d not in pool.index]
    if missing:
        raise ValueError(f"TFG days not eligible as test days: {missing}")

    per_class = {}
    for cls, n in QUOTA.items():
        f = [d for d in forced if pool.loc[d, "sky"] == cls]
        rest = [d for d in pool.index[pool.sky == cls] if d not in f]
        if len(f) > n or len(f) + len(rest) < n:
            raise ValueError(f"class {cls}: quota {n}, forced {len(f)}, available {len(rest)}")
        per_class[cls] = (f, rest, n - len(f))

    best, best_score = None, None
    for _ in range(n_draws):
        chosen = []
        for cls, (f, rest, m) in per_class.items():
            picks = f + list(rng.choice(rest, size=m, replace=False))
            shaded = days.loc[picks, "p1_shaded"]
            if len(picks) >= 2 and (shaded.all() or not shaded.any()):
                break
            chosen += picks
        else:
            months = pd.Series([days.loc[d, "month"] for d in chosen]).value_counts()
            score = (len(months), -months.max())
            if best_score is None or score > best_score:
                best, best_score = sorted(chosen), score
    if best is None:
        raise RuntimeError("no draw satisfied the constraints")
    return best


def build_split(df: pd.DataFrame, seed: int = SEED) -> pd.DataFrame:
    days = day_table(df)
    test = set(select_test_days(days, seed))
    days["split"] = np.where(~days.usable, "unusable", np.where(days.index.isin(test), "test", "train"))
    return days


def _md(d: pd.DataFrame, index: bool = True) -> str:
    d = d.reset_index() if index else d
    head = "| " + " | ".join(map(str, d.columns)) + " |\n|" + "---|" * len(d.columns) + "\n"
    return head + "\n".join("| " + " | ".join(map(str, r)) + " |" for r in d.itertuples(index=False)) + "\n"


def review(days: pd.DataFrame, seed: int) -> str:
    u = days[days.usable]
    out = [f"# Day split v2 (seed {seed})", "",
           f"{len(u)} usable days ({(~days.usable).sum()} dropped for lack of real data: "
           f"{', '.join(days.index[~days.usable])}). {(u.split == 'test').sum()} test / "
           f"{(u.split == 'train').sum()} train. The test days were chosen to cover every sky class "
           "and both P1-shadow regimes; the three TFG days are always included.", ""]

    out += ["## Days per sky class and split", "",
            _md(pd.crosstab(u.sky, u.split).reindex(list(QUOTA)).fillna(0).astype(int)), ""]
    out += ["## P1-shadow regime (P1 shaded = at least "
            f"{P1_SHADE_ROWS} bins with the sun more than half hidden)", "",
            _md(pd.crosstab([u.sky, u.p1_shaded], u.split)), ""]
    out += ["## Months", "", _md(pd.crosstab(u.month, u.split)), ""]
    shade = u.groupby("split")[[f"shade_{s}" for s in SHADE_SENSORS]].sum()
    out += ["## Shaded bins (sun > 5 deg, sun_visibility < 0.5) per split", "", _md(shade), ""]
    t = u[u.split == "test"].sort_values(["sky", "month"])
    cols = ["sky", "k_med", "clear_frac", "dark_frac", "p1_shaded", "shade_P1", "shade_P5", "shade_P7",
            "ok_P0", "ok_P1", "tfg_day"]
    out += ["## The test days, for review", "", _md(t[cols].round(2)), ""]
    flagged = days[(days.usable) & (~days.test_eligible)]
    out += ["## Usable days kept out of the test set (training only)", "",
            "A day needs >= 90 % `qc_ok` daylight rows on P0, P1, P3, P5, P7 and no all-sensor blackout.", "",
            _md(flagged[["sky", "blackout_rows", "ok_P0", "ok_P1", "ok_P3", "ok_P5", "ok_P7"]].round(2)), ""]
    return "\n".join(out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dataset", type=Path, default=REPO / "analysis/data/dataset_v2.csv")
    ap.add_argument("--out-dir", type=Path, default=REPO / "analysis/data")
    ap.add_argument("--seed", type=int, default=SEED)
    args = ap.parse_args()
    df = pd.read_csv(args.dataset, parse_dates=["utc"], dtype={"weather_type": "string"})
    days = build_split(df, args.seed)
    days.round(4).to_csv(args.out_dir / "split_v2.csv", index_label="date_local")
    (args.out_dir / "split_v2.md").write_text(review(days, args.seed))
    print(days.loc[days.split == "test", ["sky", "month", "p1_shaded"]].to_string())


if __name__ == "__main__":
    main()
