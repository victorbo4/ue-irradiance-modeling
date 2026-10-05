"""The four experiments as row splits (protocol section 3).

``prepare`` applies the common evaluation mask once and attaches each day's split and sky class.
``build_runs`` then turns the prepared rows into the runs of E1-E4: for every run, which rows a
model is trained on and which rows it is scored on. Nothing here fits a model.

    E1  train: the training days, all sensors          score: the test days, all sensors
    E2  train: the training days without one sensor    score: the test days, that sensor   (x9)
    E3  train: training-day rows with sun_visibility == 1
        score: test-day rows with sun_visibility < 0.5
    E4  train: the training days outside one month     score: the training days of it      (x9)

The 15 test days are never in a training set, in any run; in E4 they are not used at all. Days the
split marks as unusable are dropped in ``prepare`` and appear nowhere.
"""
from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np
import pandas as pd

from .common import MIN_ALTITUDE_DEG, SENSORS

# Models that are not scored on some rows (protocol sections 2 and 3). A prediction that is not
# scored is stored as NaN, which makes the metric code report "not applicable" instead of scoring
# the model on fewer rows than the others.
NOT_SCORED_ON_SENSOR = {"S": ["Pinc"]}      # Solcast GHI is a horizontal estimate
NOT_SCORED_IN_EXPERIMENT = {"E2": ["C"]}    # C's dummy for a sensor it never saw is undefined


@dataclass(frozen=True)
class Run:
    id: str                      # "E1", "E2:P1", "E3", "E4:2025-05"
    experiment: str              # "E1" .. "E4"
    train: np.ndarray            # row positions in the prepared frame
    score: np.ndarray
    held_out: str | None = None  # the sensor (E2) or the month (E4) left out
    skip_models: tuple = field(default_factory=tuple)


# ------------------------------------------------------------------------ the rows ---

def prepare(df: pd.DataFrame, split: pd.DataFrame, altitude_min: float = MIN_ALTITUDE_DEG) -> pd.DataFrame:
    """The common evaluation mask, then each day's ``split`` and ``sky``.

    Mask: ``sun_altitude_deg > altitude_min``, ``qc_ok`` and a sensor of the protocol list.
    Days whose split is "unusable" are removed. ``split`` is indexed by ``date_local`` and has the
    columns ``split`` and ``sky``. The result is re-indexed 0..n-1 so that runs can hold positions.
    """
    keep = df["sensor"].isin(SENSORS) & (df["sun_altitude_deg"] > altitude_min) & df["qc_ok"].astype(bool)
    out = df.loc[keep].copy()
    out["date_local"] = out["date_local"].astype(str)
    out["split"] = out["date_local"].map(split["split"])
    out["sky"] = out["date_local"].map(split["sky"])
    if out["split"].isna().any():
        raise ValueError("rows on days that the split does not know: "
                         f"{sorted(out.loc[out['split'].isna(), 'date_local'].unique())[:5]}")
    return out.loc[out["split"].isin(["train", "test"])].reset_index(drop=True)


def _positions(mask) -> np.ndarray:
    return np.flatnonzero(np.asarray(mask))


def build_runs(prepared: pd.DataFrame) -> list[Run]:
    """The runs of E1-E4, in a fixed order."""
    d = prepared
    is_train, is_test = (d["split"] == "train").to_numpy(), (d["split"] == "test").to_numpy()
    sensor, vis = d["sensor"].to_numpy(), d["sun_visibility"].to_numpy(float)
    month = d["date_local"].str[:7].to_numpy()
    runs = [Run("E1", "E1", _positions(is_train), _positions(is_test))]

    for s in SENSORS:                                                       # E2: one sensor out
        score = _positions(is_test & (sensor == s))
        if len(score) == 0:
            raise ValueError(f"E2: sensor {s} has no test-day rows to score")
        runs.append(Run(f"E2:{s}", "E2", _positions(is_train & (sensor != s)), score, held_out=s,
                        skip_models=tuple(NOT_SCORED_IN_EXPERIMENT["E2"])))

    runs.append(Run("E3", "E3", _positions(is_train & (vis == 1.0)), _positions(is_test & (vis < 0.5))))

    for mo in sorted(set(month[is_train])):                                 # E4: one month out, test days untouched
        runs.append(Run(f"E4:{mo}", "E4", _positions(is_train & (month != mo)), _positions(is_train & (month == mo)), held_out=mo))
    return runs


# ------------------------------------------------------------------ scoring rules ---

def apply_scoring_rules(pred: pd.DataFrame, run: Run, models: list[str]) -> pd.DataFrame:
    """Blank the predictions that must not be scored: a model skipped in this experiment, and S on Pinc."""
    out = pred.copy()
    for m in models:
        if m in run.skip_models:
            out[m] = np.nan
    for m, sensors in NOT_SCORED_ON_SENSOR.items():
        if m in out.columns:
            out.loc[out["sensor"].isin(sensors), m] = np.nan
    return out
