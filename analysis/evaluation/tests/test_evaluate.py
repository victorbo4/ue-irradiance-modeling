"""Stage 2: tables and summary, on the results of a smoke run (synthetic data, tiny grids)."""
import json
import shutil

import numpy as np
import pandas as pd
import pytest

from evaluation import evaluate as EV
from evaluation import metrics as mt
from evaluation import run as R
from evaluation import smoke
from evaluation.common import LABELS, OPEN_SENSORS, SENSORS, SHADED_SENSORS

N_BOOT = 120


@pytest.fixture(scope="module")
def results(tmp_path_factory):
    out = tmp_path_factory.mktemp("res")
    df, split = smoke.make_dataset()
    R.run_all(df, split, out, grid_hd=smoke.SMOKE_GRID_HD, grid_c=smoke.SMOKE_GRID_C, n_jobs=2, log=lambda *a: None)
    R.write_manifest(out, ["main", "alt10", "clip3", "clipnone"], smoke=True, tag_verified=False, argv=[])
    return out


@pytest.fixture(scope="module")
def tables(results):
    return EV.write_all(results, n_boot=N_BOOT, log=lambda *a: None)


def predictions(results, run_id, variant="main"):
    return pd.read_csv(results / variant / run_id.replace(":", "_") / "predictions.csv")


def fitted(results, run_id, variant="main"):
    return json.loads((results / variant / run_id.replace(":", "_") / "fitted.json").read_text())


def rmse_row(t, unit, group, model):
    r = t["metrics"][(t["metrics"].unit == unit) & (t["metrics"].group == group) & (t["metrics"].model == model)]
    return r.iloc[0] if len(r) else None


# ----------------------------------------------------------------- the files ---

def test_every_variant_gets_its_seven_tables_and_there_is_a_summary(results, tables):
    assert set(tables) == {"main", "alt10", "clip3", "clipnone"}
    for v in tables:
        assert {p.name for p in (results / "tables" / v).iterdir()} == {
            "metrics.csv", "contrasts.csv", "overfit.csv", "selected_configs.csv", "config_stability.csv",
            "fitted_params.csv", "importances.csv"}
    assert (results / "summary.md").exists()


def test_the_units_are_the_protocol_ones(tables):
    units = set(tables["main"]["metrics"].unit)
    expected = {"E1", "E3", "E4:pooled"} | {f"E2:{s}" for s in SENSORS} | {"E2:shaded", "E2:open", "E2:tilted"} \
        | {f"E4:{m}" for m in smoke.MONTHS}
    assert units == expected and len(units) == 24
    assert set(tables["main"]["metrics"].experiment) == {"E1", "E2", "E3", "E4"}


def test_per_sensor_and_per_month_units_report_only_the_global_group_and_the_others_report_everything(tables):
    m = tables["main"]["metrics"]
    for u in [f"E2:{s}" for s in SENSORS] + [f"E4:{x}" for x in smoke.MONTHS]:
        assert set(m[m.unit == u].group) == {"overall"}, u
    for u in ("E1", "E2:shaded", "E4:pooled"):
        assert {"overall", "horizontals", "shade:shaded", "sky:sunny", "sky:cloudy+rainy"} <= set(m[m.unit == u].group), u
    assert "sensor:P0" in set(m[m.unit == "E1"].group)


# ------------------------------------------------------- numbers against recomputation ---

def test_the_e1_rows_match_a_direct_computation_on_the_saved_predictions(results, tables):
    p = predictions(results, "E1")
    for model in ("H", "D", "A"):
        direct = mt.point_metrics(p.real_wm2, p[model])
        row = rmse_row(tables["main"], "E1", "overall", model)
        assert row.rmse == pytest.approx(direct["rmse"]) and row.mbe == pytest.approx(direct["mbe"]) and row.n_rows == len(p)
        assert row.rmse_lo < row.rmse < row.rmse_hi


def test_the_intervals_are_those_of_an_independent_bootstrap_with_the_protocol_seed(results, tables):
    p = predictions(results, "E1")
    ref = mt.summarize_group(p, ["H"], n_boot=N_BOOT)[0]                    # default seed: 42
    row = rmse_row(tables["main"], "E1", "overall", "H")
    assert (row.rmse_lo, row.rmse_hi) == pytest.approx((ref["rmse_lo"], ref["rmse_hi"]))
    gain = mt.paired_gain(p, "H", "D", n_boot=N_BOOT)
    con = tables["main"]["contrasts"]
    c = con[(con.unit == "E1") & (con.group == "overall") & (con.a == "H") & (con.b == "D") & (con.metric == "rmse")].iloc[0]
    assert (c.lo, c.hi) == pytest.approx((gain["lo"], gain["hi"]))


def test_the_contrast_is_consistent_with_the_two_rmse_rows_it_compares(tables):
    con = tables["main"]["contrasts"]
    c = con[(con.unit == "E1") & (con.group == "overall") & (con.a == "H") & (con.b == "D") & (con.metric == "rmse")].iloc[0]
    d, h = (rmse_row(tables["main"], "E1", "overall", m).rmse for m in ("D", "H"))
    assert c.gain == pytest.approx(d - h) and c.lo <= c.gain <= c.hi
    assert set(con.metric) == {"rmse", "mae"}


def test_e2_groups_pool_the_runs_of_their_sensors_and_nothing_else(results, tables):
    for name, sensors in (("shaded", SHADED_SENSORS), ("open", OPEN_SENSORS)):
        pooled = pd.concat([predictions(results, f"E2:{s}") for s in sensors])
        row = rmse_row(tables["main"], f"E2:{name}", "overall", "H")
        assert row.n_rows == len(pooled) == sum(fitted(results, f"E2:{s}")["n_score"] for s in sensors)
        assert row.rmse == pytest.approx(mt.point_metrics(pooled.real_wm2, pooled["H"])["rmse"])
        assert set(pooled.sensor) == set(sensors)


def test_c_is_never_scored_in_e2_and_s_is_never_scored_on_pinc(tables):
    m = tables["main"]["metrics"]
    e2 = m[m.experiment == "E2"]
    assert "C" not in set(e2.model)
    assert rmse_row(tables["main"], "E2:shaded", "overall", "S") is not None and rmse_row(tables["main"], "E2:open", "overall", "S") is not None
    assert rmse_row(tables["main"], "E2:tilted", "overall", "S") is None and rmse_row(tables["main"], "E1", "overall", "S") is None
    assert rmse_row(tables["main"], "E1", "horizontals", "S") is not None
    assert rmse_row(tables["main"], "E1", "overall", "C") is not None


def test_e4_pooled_covers_every_training_day_exactly_once(results, tables):
    pooled = pd.concat([predictions(results, f"E4:{mo}") for mo in smoke.MONTHS])
    row = rmse_row(tables["main"], "E4:pooled", "overall", "H")
    assert row.n_rows == len(pooled) and row.n_days == pooled.date_local.nunique() == 18
    assert row.rmse == pytest.approx(mt.point_metrics(pooled.real_wm2, pooled["H"])["rmse"])
    assert set(pooled.split) == {"train"}


# ---------------------------------------------------- the run-level bookkeeping ---

def test_the_overfitting_table_has_train_cv_and_score_rmse(results, tables):
    o = tables["main"]["overfit"]
    h = o[(o.run == "E1") & (o.model == "H")].iloc[0]
    p = predictions(results, "E1")
    assert h.score_rmse == pytest.approx(mt.point_metrics(p.real_wm2, p["H"])["rmse"])
    assert h.train_rmse == pytest.approx(fitted(results, "E1")["train_rmse"]["H"]) and h.cv_rmse == pytest.approx(fitted(results, "E1")["cv_rmse"]["H"])
    assert o[o.model == "P-sim"].cv_rmse.isna().all()                       # the formulas have no cross-validation
    assert set(o[o.experiment == "E2"].model) == {"P-cs", "P-sim", "D", "H"}   # no C in E2


def test_the_chosen_configurations_and_their_stability_are_tabulated(tables):
    c, s = tables["main"]["selected_configs"], tables["main"]["config_stability"]
    assert (c.family == "H+D").sum() == 20 and (c.family == "C").sum() == 11          # C is not fitted in the nine E2 runs
    assert s.groupby("family").n_runs.sum().to_dict() == {"C": 11, "H+D": 20}
    grid = {(g.max_depth, g.n_estimators, g.min_child_weight, g.colsample_bytree) for g in smoke.SMOKE_GRID_HD}
    assert {tuple(r) for r in c[c.family == "H+D"][["max_depth", "n_estimators", "min_child_weight", "colsample_bytree"]].itertuples(index=False)} <= grid


def test_config_stability_counts_how_often_each_configuration_was_chosen(tables):
    c, s = tables["main"]["selected_configs"], tables["main"]["config_stability"]
    keys = ["max_depth", "n_estimators", "min_child_weight", "colsample_bytree"]
    expected = c.groupby(["family", *keys]).size()
    got = s.set_index(["family", *keys]).n_runs
    assert got.sort_index().to_dict() == expected.sort_index().to_dict()
    assert len(s) == len(expected) and s.n_runs.min() >= 1


def test_fitted_parameters_and_importances_are_tabulated(tables):
    fp, imp = tables["main"]["fitted_params"], tables["main"]["importances"]
    assert len(fp) == 40 and set(fp.model) == {"P-cs", "P-sim"} and fp[fp.model == "P-sim"].a.between(0.7, 0.8).all()
    assert set(imp.model) == {"D", "H", "C"} and {"sim_irradiance_wm2", "cloud_opacity"} <= set(imp[imp.model == "H"].feature)
    assert (imp.gain >= 0).all()


def test_the_clip_variants_only_contain_h_and_d(tables):
    for v in ("clip3", "clipnone"):
        assert set(tables[v]["metrics"].model) == {"D", "H"}
        assert {(a, b) for a, b in zip(tables[v]["contrasts"].a, tables[v]["contrasts"].b)} == {("H", "D")}


# ------------------------------------------------------------------- the summary ---

def test_the_summary_states_the_main_comparison_with_the_numbers_of_the_table(results, tables):
    text = (results / "summary.md").read_text()
    section = text.split("## Main comparison: H against D in E1")[1].split("\n## ")[0]
    c = tables["main"]["contrasts"]
    for group, label in (("overall", "All nine sensors pooled"), ("horizontals", "Horizontal sensors only")):
        r = c[(c.unit == "E1") & (c.group == group) & (c.a == "H") & (c.b == "D") & (c.metric == "rmse")].iloc[0]
        line = next(l for l in section.splitlines() if l.startswith(f"- {label}"))
        assert f"{r.gain:+.2f} W/m²" in line and f"{r.lo:+.2f} to {r.hi:+.2f}" in line and f"{int(r.n_rows)} rows" in line
        mae = c[(c.unit == "E1") & (c.group == group) & (c.a == "H") & (c.b == "D") & (c.metric == "mae")].iloc[0]
        assert f"{mae.gain:+.2f} W/m²" not in line                         # it is the RMSE gain, not the MAE one
    for needle in ("Main comparison: H against D in E1", "RMSE(second) - RMSE(first)", "positive when the first is better",
                   "exploratory", "not adjusted", "Overfitting diagnostic", "Hyperparameters chosen across the runs",
                   "Sensitivities", "Cloudy and rainy pooled", "E2: an unseen sensor", "E4: an unseen month"):
        assert needle in text, needle
    assert "Synthetic smoke run: **True**" in text and "verified against it: **False**" in text
    assert "not applicable" in text and "n/a" in text                          # S on Pinc, C in E2


def test_every_model_has_a_descriptive_label():
    assert set(LABELS) == set(EV.ALL_MODELS) and all(LABELS.values())


# ---------------------------------------------------------- incompleteness and cli ---

def test_an_incomplete_variant_is_refused_unless_partial_is_allowed(results, tmp_path):
    copy = tmp_path / "r"
    shutil.copytree(results / "main", copy / "main")
    (copy / "main" / "E3" / "done.json").unlink()
    with pytest.raises(RuntimeError, match="19 finished runs"):
        EV.evaluate_variant(copy, "main", n_boot=20)
    t = EV.evaluate_variant(copy, "main", n_boot=20, allow_partial=True)
    assert "E3" not in set(t["metrics"].unit) and "E1" in set(t["metrics"].unit)


def test_evaluating_twice_gives_identical_tables(results, tmp_path):
    a = EV.evaluate_variant(results, "main", n_boot=60)["metrics"]
    b = EV.evaluate_variant(results, "main", n_boot=60)["metrics"]
    pd.testing.assert_frame_equal(a, b)


def test_the_cli_writes_the_summary_and_complains_when_there_is_no_main_variant(results, tmp_path):
    copy = tmp_path / "r"
    shutil.copytree(results, copy)
    assert EV.main(["--results-dir", str(copy), "--n-boot", "30"]) == 0 and (copy / "summary.md").exists()
    with pytest.raises(FileNotFoundError):
        EV.write_all(tmp_path / "empty", n_boot=10, log=lambda *a: None)
    assert EV.SMOKE_RESULTS != EV.RESULTS
