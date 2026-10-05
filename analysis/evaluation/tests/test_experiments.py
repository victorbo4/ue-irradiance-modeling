"""The experiments as row splits: every invariant of protocol section 3, plus the scoring rules."""
import numpy as np
import pandas as pd
import pytest

from evaluation import experiments as ex
from evaluation.common import SENSORS

MONTHS = [f"2025-{m:02d}" for m in range(4, 13)]


def world(seed=0, rows_per=6):
    """Per month: two training days and one test day; plus one unusable day in April."""
    rng = np.random.RandomState(seed)
    days, split = [], {}
    for mo in MONTHS:
        for d, kind in ((3, "train"), (11, "train"), (20, "test")):
            days.append(f"{mo}-{d:02d}"); split[f"{mo}-{d:02d}"] = kind
    days.append("2025-04-28"); split["2025-04-28"] = "unusable"
    sky = {d: ["sunny", "mixed", "cloudy", "rainy"][i % 4] for i, d in enumerate(days)}
    rows = [{"date_local": d, "sensor": s, "sun_altitude_deg": rng.uniform(-3, 60), "qc_ok": rng.rand() < 0.9,
             "sun_visibility": rng.choice([1.0, 1.0, 0.7, 0.3, 0.0])}
            for d in days for s in SENSORS + ["P2"] for _ in range(rows_per)]
    sp = pd.DataFrame({"split": pd.Series(split), "sky": pd.Series(sky)})
    return pd.DataFrame(rows), sp


@pytest.fixture(scope="module")
def prepared():
    df, sp = world()
    return ex.prepare(df, sp)


def sub(prepared, positions):
    return prepared.iloc[positions]


# --------------------------------------------------------------------------- prepare ---

def test_prepare_applies_the_common_mask_once_and_attaches_split_and_sky():
    df, sp = world()
    p = ex.prepare(df, sp)
    assert (p["sun_altitude_deg"] > 5.0).all() and p["qc_ok"].all()
    assert set(p["sensor"]) <= set(SENSORS) and "P2" not in set(p["sensor"])
    assert set(p["split"]) == {"train", "test"} and "2025-04-28" not in set(p["date_local"])      # unusable day gone
    assert (p["sky"] == p["date_local"].map(sp["sky"])).all()
    assert list(p.index) == list(range(len(p)))
    assert len(ex.prepare(df, sp, altitude_min=10.0)) < len(p)                                      # the robustness cut
    assert (ex.prepare(df, sp, altitude_min=10.0)["sun_altitude_deg"] > 10.0).all()


def test_the_mask_boundary_is_strict_and_qc_ok_and_the_sensor_list_are_enforced():
    df, sp = world(rows_per=1)
    df = df.iloc[:4].copy()
    df["date_local"] = "2025-05-03"
    df["sensor"] = ["P0", "P0", "P0", "P2"]
    df["sun_altitude_deg"] = [5.0, 5.01, 30.0, 30.0]
    df["qc_ok"] = [True, True, False, True]
    p = ex.prepare(df, sp)
    assert p["sun_altitude_deg"].tolist() == [5.01]       # 5.0 is out (strictly greater), qc_ok False is out, P2 is out
    assert ex.prepare(df.assign(sun_altitude_deg=[10.0, 10.01, 30.0, 30.0]), sp, altitude_min=10.0)["sun_altitude_deg"].tolist() == [10.01]


def test_prepare_refuses_days_the_split_does_not_know():
    df, sp = world()
    with pytest.raises(ValueError, match="split does not know"):
        ex.prepare(df, sp.drop(index="2025-05-03"))


# --------------------------------------------------------------------------- the runs ---

def test_there_are_twenty_runs_in_a_fixed_order(prepared):
    runs = ex.build_runs(prepared)
    ids = [r.id for r in runs]
    assert len(runs) == 20 and len(set(ids)) == 20
    assert ids == ["E1"] + [f"E2:{s}" for s in SENSORS] + ["E3"] + [f"E4:{m}" for m in MONTHS]
    assert [r.experiment for r in runs].count("E2") == 9 and [r.experiment for r in runs].count("E4") == 9


def test_in_no_run_do_train_and_score_rows_overlap_or_touch_an_unusable_day(prepared):
    for r in ex.build_runs(prepared):
        assert len(r.train) > 0 and len(r.score) > 0, r.id
        assert not set(r.train) & set(r.score), r.id
        assert "2025-04-28" not in set(sub(prepared, r.train).date_local) | set(sub(prepared, r.score).date_local)


def test_test_days_are_never_in_a_training_set(prepared):
    test_days = set(prepared.loc[prepared.split == "test", "date_local"])
    for r in ex.build_runs(prepared):
        assert not set(sub(prepared, r.train).date_local) & test_days, r.id


def test_e1_trains_on_the_training_days_and_scores_the_test_days(prepared):
    r = ex.build_runs(prepared)[0]
    tr, sc = sub(prepared, r.train), sub(prepared, r.score)
    assert set(tr.split) == {"train"} and set(sc.split) == {"test"}
    assert (tr.split == "train").sum() == (prepared.split == "train").sum() and len(sc) == (prepared.split == "test").sum()
    assert set(tr.sensor) == set(sc.sensor) == set(SENSORS)


def test_e2_leaves_one_sensor_out_of_training_and_scores_only_it_on_the_test_days(prepared):
    runs = {r.id: r for r in ex.build_runs(prepared) if r.experiment == "E2"}
    for s in SENSORS:
        r = runs[f"E2:{s}"]
        tr, sc = sub(prepared, r.train), sub(prepared, r.score)
        assert s not in set(tr.sensor) and set(tr.sensor) == set(SENSORS) - {s}
        assert set(sc.sensor) == {s} and set(sc.split) == {"test"} and set(tr.split) == {"train"}
        assert r.held_out == s and r.skip_models == ("C",)
        assert len(sc) == ((prepared.split == "test") & (prepared.sensor == s)).sum()
        assert len(tr) == ((prepared.split == "train") & (prepared.sensor != s)).sum()


def test_e3_trains_only_on_unoccluded_rows_and_scores_only_the_clearly_shaded_ones(prepared):
    r = next(r for r in ex.build_runs(prepared) if r.id == "E3")
    tr, sc = sub(prepared, r.train), sub(prepared, r.score)
    assert (tr.sun_visibility == 1.0).all() and set(tr.split) == {"train"}
    assert (sc.sun_visibility < 0.5).all() and set(sc.split) == {"test"}
    assert len(tr) == ((prepared.split == "train") & (prepared.sun_visibility == 1.0)).sum()
    assert len(sc) == ((prepared.split == "test") & (prepared.sun_visibility < 0.5)).sum()
    assert not sc.sun_visibility.between(0.5, 1.0).any()          # partial shade is scored nowhere in E3


def test_e3_edges_visibility_one_half_is_in_neither_set_and_one_is_only_trained_on():
    df, sp = world(seed=3)
    df["sun_visibility"] = np.random.RandomState(1).choice([1.0, 0.5, 0.49, 0.51, 0.0], len(df))
    p = ex.prepare(df, sp)
    r = next(r for r in ex.build_runs(p) if r.id == "E3")
    tr, sc = p.iloc[r.train], p.iloc[r.score]
    assert 0.5 in set(p.sun_visibility) and 0.49 in set(p.sun_visibility)
    assert set(tr.sun_visibility) == {1.0} and set(sc.sun_visibility) == {0.0, 0.49}


def test_e4_leaves_one_month_out_of_training_and_never_touches_the_test_days(prepared):
    for r in (r for r in ex.build_runs(prepared) if r.experiment == "E4"):
        tr, sc = sub(prepared, r.train), sub(prepared, r.score)
        mo = r.held_out
        assert mo not in set(tr.date_local.str[:7]) and set(sc.date_local.str[:7]) == {mo}
        assert set(tr.split) == {"train"} and set(sc.split) == {"train"}                 # test days unused in both
        assert len(tr) + len(sc) == (prepared.split == "train").sum()
        assert r.skip_models == ()


def test_a_sensor_without_test_rows_cannot_be_left_out_silently():
    df, sp = world()
    df = df[~((df.sensor == "P4") & df.date_local.isin([d for d in sp.index if sp.loc[d, "split"] == "test"]))]
    with pytest.raises(ValueError, match="P4"):
        ex.build_runs(ex.prepare(df, sp))


# ------------------------------------------------------------------ the scoring rules ---

def _pred(prepared, n=40):
    p = prepared.iloc[:n][["sensor", "date_local"]].copy().reset_index(drop=True)
    p.loc[:5, "sensor"] = "Pinc"
    for m in ["A", "S", "C", "H"]:
        p[m] = 100.0
    return p


def test_s_is_never_scored_on_pinc_and_c_is_not_scored_in_e2(prepared):
    runs = {r.id: r for r in ex.build_runs(prepared)}
    p = _pred(prepared)
    e1 = ex.apply_scoring_rules(p, runs["E1"], ["A", "S", "C", "H"])
    assert e1.loc[e1.sensor == "Pinc", "S"].isna().all() and e1.loc[e1.sensor != "Pinc", "S"].eq(100.0).all()
    assert e1[["A", "C", "H"]].notna().all().all()                                  # nothing else is touched
    e2 = ex.apply_scoring_rules(p, runs["E2:P1"], ["A", "S", "C", "H"])
    assert e2["C"].isna().all() and e2[["A", "H"]].notna().all().all()
    assert p["S"].notna().all()                                                       # the input is not modified


def test_the_blanked_predictions_make_the_metrics_report_not_applicable(prepared):
    from evaluation import metrics as mt
    p = _pred(prepared, 60).assign(real_wm2=90.0, sun_visibility=1.0, sky="sunny")
    p["sensor"] = ["Pinc"] * 20 + ["P0"] * 40
    p["date_local"] = ["2025-05-03"] * 30 + ["2025-05-11"] * 30
    ruled = ex.apply_scoring_rules(p, ex.build_runs(prepared)[0], ["A", "S"])
    met, _ = mt.summarize(ruled, ["A", "S"], contrasts=[], n_boot=20)
    assert set(met[met.group == "overall"].model) == {"A"}                            # S would be on fewer rows
    assert set(met[met.group == "horizontals"].model) == {"A", "S"}
