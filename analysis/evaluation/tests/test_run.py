"""Stage 1: the runner. Everything runs on the synthetic smoke dataset with tiny grids."""
import json
import subprocess
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

from evaluation import experiments as ex
from evaluation import models as M
from evaluation import run as R
from evaluation import selection as sel
from evaluation import smoke
from evaluation.common import SENSORS

GRID = dict(grid_hd=smoke.SMOKE_GRID_HD, grid_c=smoke.SMOKE_GRID_C, n_jobs=2)


@pytest.fixture(scope="module")
def world():
    return smoke.make_dataset()


@pytest.fixture(scope="module")
def main_dir(world, tmp_path_factory):
    out = tmp_path_factory.mktemp("results")
    R.run_all(*world, out, variants=["main"], log=lambda *a: None, **GRID)
    return out


def load(d, variant, run_id):
    p = d / variant / run_id.replace(":", "_")
    return pd.read_csv(p / "predictions.csv"), json.loads((p / "fitted.json").read_text())


# ------------------------------------------------------------------ the protocol tag ---

SAMPLE = """protocol-v1: evaluation protocol frozen

Data and code that the protocol refers to (sha256):
  analysis/protocol/PROTOCOL.md             %s
  analysis/data/split_v2.csv                %s

Last commit touching analysis/pipeline: abc
"""


def test_the_tag_message_is_parsed_into_path_and_hash_pairs():
    h1, h2 = "a" * 64, "b" * 64
    assert R.parse_tag_hashes(SAMPLE % (h1, h2)) == {"analysis/protocol/PROTOCOL.md": h1, "analysis/data/split_v2.csv": h2}
    assert R.parse_tag_hashes("nothing here") == {}


def _git(cwd, *args):
    subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args], cwd=cwd, check=True, capture_output=True)


def test_a_run_is_refused_unless_the_files_match_the_tag(tmp_path):
    (tmp_path / "analysis/data").mkdir(parents=True)
    f = tmp_path / "analysis/data/split_v2.csv"
    f.write_text("a,b\n1,2\n")
    _git(tmp_path, "init", "-q"); _git(tmp_path, "add", "."); _git(tmp_path, "commit", "-q", "-m", "x")
    _git(tmp_path, "tag", "-a", "protocol-v1", "-m", "frozen\n\n  analysis/data/split_v2.csv  " + R.sha256_file(f))
    assert R.verify_protocol_tag(tmp_path) == {"analysis/data/split_v2.csv": True}
    f.write_text("a,b\n1,3\n")                                          # one value changed after the tag
    with pytest.raises(RuntimeError, match="differ"):
        R.verify_protocol_tag(tmp_path)


def test_a_missing_tag_stops_the_run(tmp_path):
    _git(tmp_path, "init", "-q")
    with pytest.raises(RuntimeError, match="not found"):
        R.verify_protocol_tag(tmp_path)


def test_the_cli_checks_the_tag_before_reading_any_data(monkeypatch, tmp_path):
    def boom(*a, **k):
        raise AssertionError("data was read before the tag was verified")
    monkeypatch.setattr(R.pd, "read_csv", boom)
    monkeypatch.setattr(R, "verify_protocol_tag", lambda *a, **k: (_ for _ in ()).throw(RuntimeError("tag differs")))
    with pytest.raises(RuntimeError, match="tag differs"):
        R.main(["--results-dir", str(tmp_path / "r")])


# ------------------------------------------------------------------- the main variant ---

def test_every_run_leaves_the_expected_files_and_a_done_marker(main_dir):
    runs = sorted(p.name for p in (main_dir / "main").iterdir())
    assert len(runs) == 20 and "E1" in runs and "E2_Pinc" in runs and "E4_2025-12" in runs
    for r in runs:
        d = main_dir / "main" / r
        assert {"predictions.csv", "fitted.json", "selection_hd.csv", "done.json"} <= {p.name for p in d.iterdir()}
        assert (d / "selection_c.csv").exists() == (not r.startswith("E2_"))     # C is not fitted in E2


def test_the_predictions_hold_exactly_the_rows_to_score(world, main_dir):
    prepared = ex.prepare(*world)
    for run in ex.build_runs(prepared):
        pred, fit = load(main_dir, "main", run.id)
        sc = prepared.iloc[run.score]
        assert len(pred) == len(sc) == fit["n_score"] and fit["n_train"] == len(run.train)
        assert pred["utc"].tolist() == sc["utc"].tolist() and pred["sensor"].tolist() == sc["sensor"].tolist()
        assert {"A", "B", "S", "R", "P-cs", "P-sim", "D", "H", "C"} <= set(pred.columns)
        assert {"utc", "date_local", "sensor", "sky", "split", "sun_visibility", "cloud_opacity", "real_wm2"} <= set(pred.columns)


def test_e2_predicts_only_the_held_out_sensor_and_e4_never_touches_a_test_day(main_dir):
    for s in SENSORS:
        pred, fit = load(main_dir, "main", f"E2:{s}")
        assert set(pred.sensor) == {s} and set(pred.split) == {"test"} and fit["held_out"] == s
        assert pred["C"].isna().all() and pred[["A", "H", "D", "P-sim"]].notna().all().all()      # C not scored in E2
    for p in (main_dir / "main").glob("E4_*"):
        pred = pd.read_csv(p / "predictions.csv")
        assert set(pred.split) == {"train"} and set(pred.date_local.str[:7]) == {p.name[3:]}


def test_s_is_blank_on_pinc_and_filled_elsewhere(main_dir):
    pred, _ = load(main_dir, "main", "E1")
    assert pred.loc[pred.sensor == "Pinc", "S"].isna().all() and pred.loc[pred.sensor != "Pinc", "S"].notna().all()


def test_fitted_json_records_configs_selection_and_the_overfitting_numbers(main_dir):
    _, fit = load(main_dir, "main", "E1")
    assert M.XGBConfig(**fit["configs"]["hd"]) in smoke.SMOKE_GRID_HD and M.XGBConfig(**fit["configs"]["c"]) in smoke.SMOKE_GRID_C
    assert set(fit["train_rmse"]) == {"P-cs", "P-sim", "D", "H", "C"}                   # the models that learn
    assert set(fit["cv_rmse"]) == {"H", "D", "C"}
    assert all(np.isfinite(v) and v >= 0 for v in {**fit["train_rmse"], **fit["cv_rmse"]}.values())
    assert set(fit["models"]["P-sim"]) == {"a", "b"} and fit["models"]["P-sim"]["a"] == pytest.approx(0.75, abs=1e-3)
    assert fit["selection"]["hd"]["months"][0] == "2025-04" and fit["k_max"] == 2.0
    assert fit["models"]["H"]["k_max"] == fit["models"]["D"]["k_max"] == 2.0
    table = pd.read_csv(main_dir / "main" / "E1" / "selection_hd.csv")
    assert table["chosen"].sum() == 1 and len(table) == len(smoke.SMOKE_GRID_HD)
    _, e2 = load(main_dir, "main", "E2:P1")
    assert e2["configs"]["c"] is None and set(e2["cv_rmse"]) == {"H", "D"} and "C" not in e2["train_rmse"]


def test_the_train_rmse_matches_a_recomputation_from_the_saved_parameters(world, main_dir):
    prepared = ex.prepare(*world)
    run = ex.build_runs(prepared)[0]
    train = prepared.iloc[run.train]
    _, fit = load(main_dir, "main", "E1")
    a, b = fit["models"]["P-cs"]["a"], fit["models"]["P-cs"]["b"]
    pred = np.maximum(train.clearsky_ghi_wm2 * (1 - a * (train.cloud_opacity / 100) ** b), 0)
    assert fit["train_rmse"]["P-cs"] == pytest.approx(np.sqrt(np.mean((pred - train.real_wm2) ** 2)))


# --------------------------------------------- nothing held out reaches the selection ---

def test_the_selection_only_ever_sees_the_rows_of_its_own_run(world, tmp_path, monkeypatch):
    calls = []
    real_hd = sel.select_hd

    def spy(train_df, *a, **k):
        calls.append({"splits": set(train_df.split), "sensors": set(train_df.sensor), "vis": set(train_df.sun_visibility)})
        return real_hd(train_df, *a, **k)

    monkeypatch.setattr(sel, "select_hd", spy)
    R.run_all(*world, tmp_path, variants=["main"], only=["E1", "E2:P1", "E3", "E4:2025-05"], log=lambda *a: None, **GRID)
    e1, e2, e3, e4 = calls
    assert e1["splits"] == {"train"} and e1["sensors"] == set(SENSORS)
    assert e2["splits"] == {"train"} and e2["sensors"] == set(SENSORS) - {"P1"}
    assert e3["splits"] == {"train"} and e3["vis"] == {1.0}
    assert e4["splits"] == {"train"}


def test_no_model_is_ever_fitted_on_a_row_that_belongs_to_the_scored_set_or_to_what_is_held_out(world, tmp_path, monkeypatch):
    calls, state = [], {"selecting": False}
    real_select = sel._select

    def flagged(*a, **k):                      # fits made by the selection itself are not the final fits
        state["selecting"] = True
        try:
            return real_select(*a, **k)
        finally:
            state["selecting"] = False

    monkeypatch.setattr(sel, "_select", flagged)
    for cls in (M.Parametric, M.RatioXGB, M.MeteoXGB):
        real_fit = cls.fit

        def spy(self, df, _real=real_fit):
            if state["selecting"]:
                return _real(self, df)
            calls.append({"model": self.id, "splits": set(df.split), "sensors": set(df.sensor), "vis": set(df.sun_visibility),
                          "months": set(df.date_local.str[:7])})
            return _real(self, df)

        monkeypatch.setattr(cls, "fit", spy)
    R.run_all(*world, tmp_path, variants=["main"], only=["E1", "E2:P1", "E3", "E4:2025-05"], log=lambda *a: None, **GRID)
    e1, e2, e3, e4 = calls[:5], calls[5:9], calls[9:14], calls[14:19]
    assert [c["model"] for c in e1] == ["P-cs", "P-sim", "D", "H", "C"] and len(calls) == 19      # C is not fitted in E2
    assert all(c["splits"] == {"train"} for c in calls)                                         # no test day is ever fitted on
    assert all("P1" not in c["sensors"] for c in e2) and all(c["sensors"] == set(SENSORS) for c in e1)
    assert all(c["vis"] == {1.0} for c in e3) and all("2025-05" not in c["months"] for c in e4)


# ------------------------------------------------- resuming, and interruptions ---

def test_finished_runs_are_skipped_and_force_redoes_them(world, tmp_path, monkeypatch):
    n = []
    real_hd = sel.select_hd
    monkeypatch.setattr(sel, "select_hd", lambda *a, **k: (n.append(1), real_hd(*a, **k))[1])
    kw = dict(variants=["main"], only=["E1", "E3"], log=lambda *a: None, **GRID)
    assert R.run_all(*world, tmp_path, **kw) == {"main": ["E1", "E3"]} and len(n) == 2
    assert R.run_all(*world, tmp_path, **kw) == {"main": []} and len(n) == 2            # nothing recomputed
    assert R.run_all(*world, tmp_path, force=True, **kw) == {"main": ["E1", "E3"]} and len(n) == 4
    assert (tmp_path / "main" / "E1" / "done.json").exists()


def test_an_interrupted_run_has_no_done_marker_and_is_redone_on_resume(world, tmp_path, monkeypatch):
    fit = M.MeteoXGB.fit
    state = {"armed": True}

    def flaky(self, df):
        if state["armed"] and df["sun_visibility"].eq(1.0).all():                       # only the E3 training rows
            raise RuntimeError("simulated crash")
        return fit(self, df)

    monkeypatch.setattr(M.MeteoXGB, "fit", flaky)
    kw = dict(variants=["main"], only=["E1", "E3"], log=lambda *a: None, **GRID)
    with pytest.raises(RuntimeError, match="simulated"):
        R.run_all(*world, tmp_path, **kw)
    assert (tmp_path / "main" / "E1" / "done.json").exists() and not (tmp_path / "main" / "E3" / "done.json").exists()
    state["armed"] = False
    assert R.run_all(*world, tmp_path, **kw) == {"main": ["E3"]}                        # E1 kept, E3 redone
    assert (tmp_path / "main" / "E3" / "done.json").exists()


def test_two_runs_give_byte_identical_predictions(world, tmp_path):
    for name in ("a", "b"):
        R.run_all(*world, tmp_path / name, variants=["main"], only=["E1"], log=lambda *a: None, **GRID)
    assert (tmp_path / "a/main/E1/predictions.csv").read_bytes() == (tmp_path / "b/main/E1/predictions.csv").read_bytes()


# ------------------------------------------------------------------------ variants ---

def test_a_reusing_variant_needs_the_main_run_first(world, tmp_path):
    with pytest.raises(FileNotFoundError, match="main variant first"):
        R.run_all(*world, tmp_path, variants=["alt10"], only=["E1"], log=lambda *a: None, **GRID)


def test_alt10_reuses_the_main_configurations_on_rows_above_ten_degrees(world, main_dir, tmp_path):
    out = tmp_path / "r"
    import shutil
    shutil.copytree(main_dir, out)
    R.run_all(*world, out, variants=["alt10"], only=["E1", "E4:2025-05"], log=lambda *a: None, **GRID)
    for rid in ("E1", "E4:2025-05"):
        p10, f10 = load(out, "alt10", rid)
        _, fm = load(out, "main", rid)
        assert (p10.sun_altitude_deg > 10).all() and f10["altitude_min"] == 10.0
        assert f10["configs"] == fm["configs"] and f10["selection"] == {} and f10["cv_rmse"] == {}
        assert {"A", "C", "P-sim", "H"} <= set(p10.columns)


@pytest.mark.parametrize("variant,k_max", [("clip3", 3.0), ("clipnone", None)])
def test_clip_variants_refit_only_h_and_d_with_the_main_configurations(world, main_dir, tmp_path, variant, k_max):
    import shutil
    out = tmp_path / "r"
    shutil.copytree(main_dir, out)
    R.run_all(*world, out, variants=[variant], only=["E1"], log=lambda *a: None, **GRID)
    pred, fit = load(out, variant, "E1")
    _, fm = load(out, "main", "E1")
    assert fit["k_max"] == k_max and fit["configs"] == fm["configs"] and set(fit["models"]) == {"D", "H"}
    assert fit["models"]["H"]["k_max"] == k_max and fit["models"]["D"]["k_max"] == k_max          # what the models actually used
    assert not ({"A", "B", "S", "R", "P-cs", "P-sim", "C"} & set(pred.columns)) and {"D", "H"} <= set(pred.columns)


# ---------------------------------------------------------------- manifest and cli ---

def test_the_manifest_accumulates_variants_and_records_what_was_checked(tmp_path):
    R.write_manifest(tmp_path, ["main"], smoke=True, tag_verified=False, argv=["--smoke"])
    R.write_manifest(tmp_path, ["alt10"], smoke=True, tag_verified=False, argv=["--smoke", "--variants", "alt10"])
    m = json.loads((tmp_path / "manifest.json").read_text())
    assert m["variants"] == ["alt10", "main"] and m["smoke"] is True and m["tag_verified"] is False
    assert {"python", "numpy", "pandas", "scipy", "xgboost"} <= set(m["versions"]) and len(m["git_commit"]) >= 7
    assert m["argv"] == ["--smoke", "--variants", "alt10"]


def test_the_smoke_cli_runs_end_to_end_into_its_own_directory(tmp_path):
    assert R.main(["--smoke", "--only", "E1", "--variants", "main", "--results-dir", str(tmp_path), "--n-jobs", "2"]) == 0
    assert (tmp_path / "main" / "E1" / "done.json").exists()
    manifest = json.loads((tmp_path / "manifest.json").read_text())
    assert manifest["smoke"] is True and manifest["tag_verified"] is False          # synthetic data never claims the tag check
    with pytest.raises(SystemExit):
        R.main(["--smoke", "--variants", "nonsense", "--results-dir", str(tmp_path)])
    assert R.SMOKE_RESULTS != R.RESULTS                                                # smoke can never overwrite real results
