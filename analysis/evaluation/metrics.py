"""Metrics, groups and block-bootstrap intervals (protocol section 4).

Metrics: R2, MAE, RMSE and MBE, plus the error tail (P90, P95, P99 of |error| and the maximum).
Sign of the error: ``error = prediction - observation``, so MBE < 0 is under-prediction.

Uncertainty: 95 % percentile intervals (2.5th-97.5th) from a block bootstrap over days (2000
resamples, seed 42). The days of a group are resampled with replacement and every model is scored
on the *same* resampled rows, so differences between models are paired.

A model is scored on a group only if it has a prediction for every row of the group; there is no
partial scoring (that is how two models end up on different rows, the TFG's 4208 against 5400).

Contrasts are reported as the *gain of a over b*: ``RMSE(b) - RMSE(a)`` in W/m2, positive when a
is better. The main comparison "H against D" is therefore ``RMSE(D) - RMSE(H)``.
"""
from __future__ import annotations

import numpy as np
import pandas as pd

N_BOOT = 2000
SEED = 42
CI = (2.5, 97.5)

POINT = ["r2", "mae", "rmse", "mbe"]
TAILS = ["p90", "p95", "p99", "max_abs"]

# (a, b): the gain of a over b. The first is the main comparison (protocol section 4).
CONTRASTS = [("H", "D"), ("A", "B"), ("A", "R"), ("R", "B"), ("H", "A"),
             ("P-sim", "P-cs"), ("H", "C"), ("H", "P-sim"), ("H", "S")]
# Contrasts of S-geo, the model added after protocol-v1 (ADDENDUM-1).
POSTHOC_CONTRASTS = [("S-geo", "S"), ("S-geo", "R"), ("S-geo", "A"), ("S-geo", "P-sim"), ("S-geo", "H"), ("S-geo", "D")]
SKY_CLASSES = ["sunny", "mixed", "cloudy", "rainy"]


# ------------------------------------------------------------------- point metrics --

def point_metrics(y, pred) -> dict:
    y, pred = np.asarray(y, float), np.asarray(pred, float)
    e = pred - y
    ss_tot = float(np.sum((y - y.mean()) ** 2))
    abs_e = np.abs(e)
    return {
        "r2": float(1.0 - np.sum(e ** 2) / ss_tot) if ss_tot > 0 else float("nan"),
        "mae": float(abs_e.mean()), "rmse": float(np.sqrt(np.mean(e ** 2))), "mbe": float(e.mean()),
        "p90": float(np.percentile(abs_e, 90)), "p95": float(np.percentile(abs_e, 95)),
        "p99": float(np.percentile(abs_e, 99)), "max_abs": float(abs_e.max()),
    }


# ---------------------------------------------- sufficient statistics and the bootstrap --
# Everything but the tail is a function of six sums per day, so a resample is a sum of rows of a
# (n_days, 6) matrix instead of a pass over the data.

_COLS = ["n", "sum_e", "sum_abs_e", "sum_e2", "sum_y", "sum_y2"]


def day_stats(y, pred, day_codes: np.ndarray, n_days: int) -> np.ndarray:
    y, pred = np.asarray(y, float), np.asarray(pred, float)
    e = pred - y
    out = np.zeros((n_days, len(_COLS)))
    for j, v in enumerate([np.ones_like(y), e, np.abs(e), e ** 2, y, y ** 2]):
        out[:, j] = np.bincount(day_codes, weights=v, minlength=n_days)
    return out


def metrics_from_sums(S: np.ndarray) -> dict:
    """R2, MAE, RMSE, MBE from summed statistics of shape (..., 6)."""
    n, se, sae, se2, sy, sy2 = (S[..., i] for i in range(6))
    ss_tot = sy2 - sy ** 2 / n
    with np.errstate(divide="ignore", invalid="ignore"):
        return {"r2": 1.0 - se2 / ss_tot, "mae": sae / n, "rmse": np.sqrt(se2 / n), "mbe": se / n}


def bootstrap_indices(n_days: int, n_boot: int = N_BOOT, seed: int = SEED) -> np.ndarray:
    """Which days each resample draws, shape (n_boot, n_days). The same for every model of a group."""
    return np.random.RandomState(seed).randint(0, n_days, size=(n_boot, n_days))


def _percentile(values: np.ndarray) -> tuple[float, float]:
    lo, hi = np.percentile(values, CI)
    return float(lo), float(hi)


def encode_days(days) -> tuple[np.ndarray, int]:
    codes, uniques = pd.factorize(pd.Series(days).astype(str), sort=True)
    return codes, len(uniques)


# --------------------------------------------------------------------------- groups --

def group_masks(df: pd.DataFrame) -> dict[str, np.ndarray]:
    """Every group of rows a table reports (protocol section 4), as boolean masks.

    ``shade:shaded`` is ``sun_visibility < 0.5`` and ``shade:unshaded`` is ``== 1``; the partial
    rows in between belong to neither. ``horizontals`` leaves Pinc out.
    """
    masks = {"overall": np.ones(len(df), bool)}
    for s in sorted(df["sensor"].unique()):
        masks[f"sensor:{s}"] = (df["sensor"] == s).to_numpy()
    for c in SKY_CLASSES:
        if (df["sky"] == c).any():
            masks[f"sky:{c}"] = (df["sky"] == c).to_numpy()
    if all((df["sky"] == c).any() for c in ("cloudy", "rainy")):       # sensitivity 3: their boundary is arbitrary
        masks["sky:cloudy+rainy"] = df["sky"].isin(["cloudy", "rainy"]).to_numpy()
    vis = df["sun_visibility"].to_numpy(float)
    masks["shade:shaded"], masks["shade:unshaded"] = vis < 0.5, vis == 1.0
    masks["horizontals"] = (df["sensor"] != "Pinc").to_numpy()
    return {k: v for k, v in masks.items() if v.any()}


def _scored(df_g: pd.DataFrame, model: str) -> bool:
    return model in df_g.columns and bool(df_g[model].notna().all())


# --------------------------------------------------------------------- the tables --

def summarize_group(df_g: pd.DataFrame, models: list[str], n_boot: int = N_BOOT, seed: int = SEED) -> list[dict]:
    """One row per model that can be scored on this group: point metrics, intervals, tail."""
    codes, n_days = encode_days(df_g["date_local"])
    idx = bootstrap_indices(n_days, n_boot, seed) if n_days >= 2 else None
    y = df_g["real_wm2"].to_numpy(float)
    rows = []
    for m in models:
        if not _scored(df_g, m):
            continue
        pred = df_g[m].to_numpy(float)
        row = {"model": m, "n_rows": len(df_g), "n_days": n_days, **point_metrics(y, pred)}
        S = day_stats(y, pred, codes, n_days)
        boot = metrics_from_sums(S[idx].sum(axis=1)) if idx is not None else None
        for k in POINT:
            lo, hi = _percentile(boot[k]) if boot is not None else (float("nan"), float("nan"))
            row[f"{k}_lo"], row[f"{k}_hi"] = lo, hi
        rows.append(row)
    return rows


def paired_gain(df_g: pd.DataFrame, a: str, b: str, metric: str = "rmse",
                n_boot: int = N_BOOT, seed: int = SEED) -> dict | None:
    """The gain of ``a`` over ``b`` in ``metric`` (lower is better): metric(b) - metric(a), with a
    paired block-bootstrap interval. None when either model cannot be scored on the group."""
    if not (_scored(df_g, a) and _scored(df_g, b)):
        return None
    codes, n_days = encode_days(df_g["date_local"])
    y = df_g["real_wm2"].to_numpy(float)
    Sa = day_stats(y, df_g[a].to_numpy(float), codes, n_days)
    Sb = day_stats(y, df_g[b].to_numpy(float), codes, n_days)
    est = metrics_from_sums(Sb.sum(axis=0))[metric] - metrics_from_sums(Sa.sum(axis=0))[metric]
    out = {"a": a, "b": b, "metric": metric, "n_rows": len(df_g), "n_days": n_days, "gain": float(est),
           "lo": float("nan"), "hi": float("nan")}
    if n_days >= 2:
        idx = bootstrap_indices(n_days, n_boot, seed)
        g = metrics_from_sums(Sb[idx].sum(axis=1))[metric] - metrics_from_sums(Sa[idx].sum(axis=1))[metric]
        out["lo"], out["hi"] = _percentile(g)
    return out


def summarize(df: pd.DataFrame, models: list[str], contrasts=CONTRASTS, n_boot: int = N_BOOT, seed: int = SEED,
              groups: list[str] | None = None):
    """The metric table and the contrast table of one run.

    ``df`` has one row per scored (sensor, time): ``date_local``, ``sensor``, ``sky``,
    ``sun_visibility``, ``real_wm2`` and one prediction column per model (NaN where the model
    was not scored). ``groups`` limits the report to the named groups. Returns ``(metrics, contrasts)``
    as DataFrames with a ``group`` column.
    """
    met, con = [], []
    for name, mask in group_masks(df).items():
        if groups is not None and name not in groups:
            continue
        sub = df.loc[mask]
        met += [{"group": name, **r} for r in summarize_group(sub, models, n_boot, seed)]
        for a, b in contrasts:
            for metric in ("rmse", "mae"):
                r = paired_gain(sub, a, b, metric, n_boot, seed)
                if r is not None:
                    con.append({"group": name, **r})
    return pd.DataFrame(met), pd.DataFrame(con)
