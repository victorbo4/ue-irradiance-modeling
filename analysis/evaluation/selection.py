"""Hyperparameter selection (protocol Appendix B).

Run inside every training run (E1, each E2 fold, E3, each E4 fold) on *that run's training data
only*; nothing held out can influence it.

    select_hd(train_df)  -> one configuration shared by H and D
    select_c(train_df)   -> a configuration for C, chosen on its own

Procedure: leave-one-month-out folds over the training data (blocked cross-validation, since
the days of a month share weather). A configuration's score on a fold is the pooled RMSE on the
held-out month (for H and D jointly, the mean of the two models' RMSEs); its score overall is the
mean over the folds. One-standard-error rule: among the configurations whose mean is within one
standard error (across folds) of the best, take the simplest.
"""
from __future__ import annotations

import itertools
from dataclasses import dataclass

import numpy as np
import pandas as pd
from joblib import Parallel, delayed

from . import models as M
from .common import TARGET
from .models import XGBConfig

# Complete grids, in a fixed order (no random draws). Every other parameter keeps the TFG value.
_COMMON = dict(max_depth=[2, 3, 4, 5], n_estimators=[100, 300, 800], min_child_weight=[20, 70, 200])
HD_GRID = {**_COMMON, "colsample_bytree": [0.5, 1.0]}   # 72 configurations (two inputs)
C_GRID = {**_COMMON, "colsample_bytree": [0.8]}         # 36 configurations (many inputs)


def make_grid(spec: dict) -> list[XGBConfig]:
    keys = ["max_depth", "n_estimators", "min_child_weight", "colsample_bytree"]
    return [XGBConfig(**dict(zip(keys, vals))) for vals in itertools.product(*(spec[k] for k in keys))]


def simplicity_key(cfg: XGBConfig) -> tuple:
    """Smaller is simpler: fewest trees, then smallest depth, then largest min_child_weight,
    then colsample_bytree 0.5 before 1.0."""
    return (cfg.n_estimators, cfg.max_depth, -cfg.min_child_weight, cfg.colsample_bytree)


def one_se_choice(configs: list[XGBConfig], scores: np.ndarray) -> dict:
    """The one-standard-error rule on a (n_configs, n_folds) matrix of fold scores.

    The best configuration is the one with the lowest mean (the first one on a tie); its
    standard error is std(ddof=1)/sqrt(n_folds) of its own fold scores. Every configuration whose
    mean is not above best_mean + SE is a candidate; the simplest candidate is chosen (ties on
    simplicity broken by position in the grid).
    """
    scores = np.asarray(scores, dtype=float)
    n_configs, n_folds = scores.shape
    if len(configs) != n_configs:
        raise ValueError("one configuration per row of scores is required")
    if n_folds < 2:
        raise ValueError("a standard error needs at least two folds")
    means = scores.mean(axis=1)
    best = int(np.argmin(means))
    se = float(scores[best].std(ddof=1) / np.sqrt(n_folds))
    threshold = float(means[best] + se)
    candidates = [i for i in range(n_configs) if means[i] <= threshold]
    chosen = min(candidates, key=lambda i: (simplicity_key(configs[i]), i))
    return {"chosen": chosen, "best": best, "se": se, "threshold": threshold, "candidates": candidates}


def month_folds(df: pd.DataFrame) -> list[tuple[str, np.ndarray, np.ndarray]]:
    """Leave-one-month-out: a list of (month, train_positions, validation_positions)."""
    month = df["date_local"].astype(str).str[:7].to_numpy()
    months = sorted(set(month))
    if len(months) < 2:
        raise ValueError("leave-one-month-out needs at least two months of training data")
    pos = np.arange(len(df))
    return [(mo, pos[month != mo], pos[month == mo]) for mo in months]


@dataclass
class Selection:
    config: XGBConfig
    index: int                     # position of the chosen configuration in the grid
    best_index: int                # the configuration with the lowest mean score
    threshold: float               # best mean + one standard error
    months: list[str]              # the folds
    table: pd.DataFrame            # one row per configuration: parameters, mean, se, chosen, ...
    fold_scores: np.ndarray        # (n_configs, n_folds), the score the rule works on
    fold_rmse: dict                # model id -> (n_configs, n_folds) pooled RMSE per model


def _rmse(pred: np.ndarray, real: np.ndarray) -> float:
    return float(np.sqrt(np.mean((pred - real) ** 2)))


def _score_hd(cfg: XGBConfig, tr: pd.DataFrame, va: pd.DataFrame, k_max) -> dict:
    out = {}
    for mid, base in (("H", "sim_irradiance_wm2"), ("D", "clearsky_ghi_wm2")):
        model = M.RatioXGB(mid, base, cfg, k_max, n_jobs=1).fit(tr)
        out[mid] = _rmse(model.predict(va), va[TARGET].to_numpy(float))
    return out


def _score_c(cfg: XGBConfig, tr: pd.DataFrame, va: pd.DataFrame, k_max) -> dict:
    model = M.MeteoXGB(cfg, n_jobs=1).fit(tr)
    return {"C": _rmse(model.predict(va), va[TARGET].to_numpy(float))}


def _select(df: pd.DataFrame, grid: list[XGBConfig], score_fn, ids: list[str], k_max, n_jobs: int) -> Selection:
    df = df.reset_index(drop=True)
    folds = month_folds(df)
    tasks = [(ci, fi) for ci in range(len(grid)) for fi in range(len(folds))]

    def run(ci, fi):
        _, tr_pos, va_pos = folds[fi]
        return score_fn(grid[ci], df.iloc[tr_pos], df.iloc[va_pos], k_max)

    results = Parallel(n_jobs=n_jobs, prefer="threads")(delayed(run)(ci, fi) for ci, fi in tasks)
    fold_rmse = {i: np.zeros((len(grid), len(folds))) for i in ids}
    for (ci, fi), r in zip(tasks, results):
        for i in ids:
            fold_rmse[i][ci, fi] = r[i]
    scores = np.mean([fold_rmse[i] for i in ids], axis=0)      # the mean of the models' RMSEs (one model: itself)

    rule = one_se_choice(grid, scores)
    table = pd.DataFrame([vars(c) for c in grid])
    table["mean_score"] = scores.mean(axis=1)
    table["se"] = scores.std(axis=1, ddof=1) / np.sqrt(scores.shape[1])
    table["within_one_se"] = [i in rule["candidates"] for i in range(len(grid))]
    table["chosen"] = [i == rule["chosen"] for i in range(len(grid))]
    return Selection(config=grid[rule["chosen"]], index=rule["chosen"], best_index=rule["best"],
                     threshold=rule["threshold"], months=[f[0] for f in folds], table=table,
                     fold_scores=scores, fold_rmse=fold_rmse)


def select_hd(train_df: pd.DataFrame, grid: list[XGBConfig] | None = None, k_max=M.K_MAX, n_jobs: int = 1) -> Selection:
    """One configuration for H and D together (they must differ only in the geometry signal)."""
    return _select(train_df, grid or make_grid(HD_GRID), _score_hd, ["H", "D"], k_max, n_jobs)


def select_c(train_df: pd.DataFrame, grid: list[XGBConfig] | None = None, n_jobs: int = 1) -> Selection:
    """A configuration for C, chosen on its own with the same procedure."""
    return _select(train_df, grid or make_grid(C_GRID), _score_c, ["C"], None, n_jobs)
