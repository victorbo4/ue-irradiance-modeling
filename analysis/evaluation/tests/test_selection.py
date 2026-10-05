"""Hyperparameter selection: the grids, the folds, the one-standard-error rule and the absence of leakage."""
import numpy as np
import pandas as pd
import pytest

from evaluation import models as M
from evaluation import selection as sel
from evaluation.models import XGBConfig
from synthetic import make_df

SMALL_GRID = [XGBConfig(2, 100, 20, 0.5), XGBConfig(2, 300, 70, 1.0),
              XGBConfig(3, 300, 20, 1.0), XGBConfig(3, 100, 70, 0.5)]


# ----------------------------------------------------------------- the grids ---

def test_grids_have_the_protocol_sizes_and_values():
    hd, c = sel.make_grid(sel.HD_GRID), sel.make_grid(sel.C_GRID)
    assert len(hd) == 72 and len(c) == 36
    assert len(set(hd)) == 72 and len(set(c)) == 36
    assert {x.max_depth for x in hd} == {2, 3, 4, 5}
    assert {x.n_estimators for x in hd} == {100, 300, 800}
    assert {x.min_child_weight for x in hd} == {20, 70, 200}
    assert {x.colsample_bytree for x in hd} == {0.5, 1.0}
    assert {x.colsample_bytree for x in c} == {0.8}          # C keeps the TFG's 0.8
    assert sel.make_grid(sel.HD_GRID) == hd                    # fixed order, no random draws


# ------------------------------------------------------------ "the simplest" ---

def test_simplicity_is_fewest_trees_then_smallest_depth_then_largest_leaf_then_colsample_half_first():
    k = sel.simplicity_key
    assert k(XGBConfig(5, 100, 20, 1.0)) < k(XGBConfig(2, 300, 200, 0.5))      # trees dominate depth
    assert k(XGBConfig(2, 300, 20, 1.0)) < k(XGBConfig(3, 300, 200, 0.5))      # then depth
    assert k(XGBConfig(3, 300, 200, 1.0)) < k(XGBConfig(3, 300, 70, 0.5))      # then larger min_child_weight
    assert k(XGBConfig(3, 300, 70, 0.5)) < k(XGBConfig(3, 300, 70, 1.0))       # then colsample 0.5 before 1.0
    ordered = sorted(sel.make_grid(sel.HD_GRID), key=k)
    assert ordered[0] == XGBConfig(2, 100, 200, 0.5) and ordered[-1] == XGBConfig(5, 800, 20, 1.0)


# ------------------------------------------------------- the one-SE rule itself ---

def _cfgs():
    return [XGBConfig(5, 800, 20, 1.0),     # 0: complex
            XGBConfig(3, 300, 70, 1.0),     # 1: simpler
            XGBConfig(2, 100, 200, 0.5)]    # 2: simplest


def test_one_se_takes_the_simplest_configuration_within_one_standard_error_of_the_best():
    scores = np.array([[9.0, 11.0, 9.0, 11.0],      # best: mean 10, std(ddof=1) 1.1547 -> se 0.5774
                       [10.4, 10.4, 10.4, 10.4],    # mean 10.4, inside 10.577
                       [10.8, 10.8, 10.8, 10.8]])   # mean 10.8, outside
    r = sel.one_se_choice(_cfgs(), scores)
    assert r["best"] == 0
    assert r["se"] == pytest.approx(np.std([9, 11, 9, 11], ddof=1) / 2)
    assert r["threshold"] == pytest.approx(10 + r["se"])
    assert r["candidates"] == [0, 1] and r["chosen"] == 1      # the simplest of the candidates; config 2 is too far


def test_one_se_keeps_the_best_when_nothing_simpler_is_close_enough():
    scores = np.array([[9.0, 11.0, 9.0, 11.0], [12.0] * 4, [13.0] * 4])
    assert sel.one_se_choice(_cfgs(), scores)["chosen"] == 0


def test_one_se_prefers_the_simplest_even_when_it_is_not_the_one_with_the_lowest_mean():
    # best: mean 10.00, se 0.0816 -> threshold 10.0816; the other two (10.04 and 10.07) are inside it
    scores = np.array([[10.0, 10.2, 9.8, 10.0], [10.04, 10.24, 9.84, 10.04], [10.07, 10.27, 9.87, 10.07]])
    r = sel.one_se_choice(_cfgs(), scores)
    assert r["best"] == 0 and r["candidates"] == [0, 1, 2] and r["chosen"] == 2


def test_one_se_breaks_ties_in_the_mean_by_grid_position_and_validates_its_input():
    twin = [XGBConfig(3, 300, 70, 1.0), XGBConfig(3, 300, 70, 1.0)]
    scores = np.array([[5.0, 6.0], [5.0, 6.0]])
    assert sel.one_se_choice(twin, scores)["best"] == 0 and sel.one_se_choice(twin, scores)["chosen"] == 0
    with pytest.raises(ValueError):
        sel.one_se_choice(twin, np.array([[5.0], [6.0]]))          # one fold: no standard error
    with pytest.raises(ValueError):
        sel.one_se_choice(twin, np.array([[5.0, 6.0]]))            # scores do not match the configurations


# ------------------------------------------------------------------- the folds ---

def test_leave_one_month_out_folds_partition_the_rows_by_month():
    df = make_df(400, seed=1, months=("2025-04", "2025-05", "2025-06"))
    folds = sel.month_folds(df)
    assert [f[0] for f in folds] == ["2025-04", "2025-05", "2025-06"]
    month = df["date_local"].str[:7].to_numpy()
    for mo, tr, va in folds:
        assert set(month[va]) == {mo} and mo not in set(month[tr])
        assert len(tr) + len(va) == len(df) and not set(tr) & set(va)


def test_a_single_month_cannot_be_cross_validated():
    with pytest.raises(ValueError):
        sel.month_folds(make_df(100, months=("2025-04",)))


# --------------------------------------------------------- the whole selection ---

def test_hd_selection_returns_a_grid_configuration_and_a_consistent_table():
    df = make_df(900, seed=2)
    r = sel.select_hd(df, grid=SMALL_GRID)
    assert r.config in SMALL_GRID and SMALL_GRID[r.index] == r.config
    assert r.months == ["2025-04", "2025-05", "2025-06", "2025-07"]
    assert r.fold_scores.shape == (4, 4) and set(r.fold_rmse) == {"H", "D"}
    assert len(r.table) == 4 and r.table["chosen"].sum() == 1 and r.table.loc[r.index, "chosen"]
    assert r.table.loc[r.index, "mean_score"] <= r.threshold                       # chosen is within one SE
    assert sel.simplicity_key(r.config) <= sel.simplicity_key(SMALL_GRID[r.best_index])


def test_the_hd_score_is_the_mean_of_the_h_and_d_rmse_on_the_held_out_month():
    df = make_df(900, seed=3).reset_index(drop=True)
    r = sel.select_hd(df, grid=SMALL_GRID[:1])
    mo, tr, va = sel.month_folds(df)[0]
    cfg = SMALL_GRID[0]
    rm = {}
    for mid, base in (("H", "sim_irradiance_wm2"), ("D", "clearsky_ghi_wm2")):
        model = M.RatioXGB(mid, base, cfg).fit(df.iloc[tr])
        rm[mid] = np.sqrt(np.mean((model.predict(df.iloc[va]) - df.iloc[va].real_wm2) ** 2))
    assert r.fold_rmse["H"][0, 0] == pytest.approx(rm["H"]) and r.fold_rmse["D"][0, 0] == pytest.approx(rm["D"])
    assert r.fold_scores[0, 0] == pytest.approx((rm["H"] + rm["D"]) / 2)


def test_selection_is_deterministic_and_does_not_depend_on_the_number_of_workers():
    df = make_df(900, seed=4)
    a, b, c = sel.select_hd(df, SMALL_GRID, n_jobs=1), sel.select_hd(df, SMALL_GRID, n_jobs=1), sel.select_hd(df, SMALL_GRID, n_jobs=4)
    np.testing.assert_array_equal(a.fold_scores, b.fold_scores)
    np.testing.assert_array_equal(a.fold_scores, c.fold_scores)
    assert a.config == b.config == c.config


def test_the_held_out_month_is_never_seen_when_fitting(monkeypatch):
    seen = []
    fit, predict = M.RatioXGB.fit, M.RatioXGB.predict

    def spy_fit(self, df):
        self._fit_months = set(df["date_local"].str[:7])
        return fit(self, df)

    def spy_predict(self, df):
        seen.append((self._fit_months, set(df["date_local"].str[:7])))
        return predict(self, df)

    monkeypatch.setattr(M.RatioXGB, "fit", spy_fit)
    monkeypatch.setattr(M.RatioXGB, "predict", spy_predict)
    sel.select_hd(make_df(600, seed=5), SMALL_GRID[:2])
    assert len(seen) == 2 * 4 * 2                                  # configs x folds x {H, D}
    for fitted_on, scored_on in seen:
        assert len(scored_on) == 1 and not (fitted_on & scored_on)


def test_c_is_selected_on_its_own_on_its_own_grid():
    df = make_df(900, seed=6)
    grid = [XGBConfig(2, 100, 20, 0.8), XGBConfig(3, 300, 70, 0.8)]
    r = sel.select_c(df, grid=grid)
    assert set(r.fold_rmse) == {"C"} and r.fold_scores.shape == (2, 4)
    np.testing.assert_array_equal(r.fold_scores, r.fold_rmse["C"])      # one model: its own RMSE
    assert r.config in grid
    assert {x.colsample_bytree for x in sel.make_grid(sel.C_GRID)} == {0.8}


def test_the_default_grids_are_the_protocol_ones(monkeypatch):
    got = {}
    monkeypatch.setattr(sel, "_select", lambda df, grid, score_fn, ids, k_max, n_jobs: got.update(grid=grid, ids=ids, k_max=k_max))
    df = make_df(50)
    sel.select_hd(df)
    assert got["grid"] == sel.make_grid(sel.HD_GRID) and got["ids"] == ["H", "D"] and got["k_max"] == M.K_MAX
    sel.select_c(df)
    assert got["grid"] == sel.make_grid(sel.C_GRID) and got["ids"] == ["C"] and len(got["grid"]) == 36
