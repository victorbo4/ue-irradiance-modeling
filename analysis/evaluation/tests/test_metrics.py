"""Metrics, groups and the paired block bootstrap, each checked against an independent calculation."""
import numpy as np
import pandas as pd
import pytest

from evaluation import metrics as mt


def table(n_days=8, per_day=40, seed=0, sensors=("P0", "P1", "Pinc")):
    """A scored table: truth, three models of different quality (B is a constant offset of A)."""
    rng = np.random.RandomState(seed)
    rows = []
    for d in range(n_days):
        for _ in range(per_day):
            y = rng.uniform(50, 900)
            rows.append({"date_local": f"2025-05-{d + 1:02d}", "sensor": rng.choice(sensors),
                         "sky": ["sunny", "mixed", "cloudy", "rainy"][d % 4], "sun_visibility": rng.choice([1.0, 1.0, 0.3, 0.0, 0.7]),
                         "real_wm2": y, "A": y + rng.normal(0, 20), "H": y + rng.normal(0, 40)})
    df = pd.DataFrame(rows)
    df["B"] = df["A"] + 15.0
    return df


# ------------------------------------------------------------------ point metrics --

def test_point_metrics_on_a_case_worked_by_hand():
    y, p = [1.0, 2.0, 3.0, 4.0], [1.0, 3.0, 3.0, 2.0]       # errors (p - y): 0, +1, 0, -2
    r = mt.point_metrics(y, p)
    assert r["mbe"] == pytest.approx(-0.25) and r["mae"] == pytest.approx(0.75)
    assert r["rmse"] == pytest.approx(np.sqrt(5 / 4)) and r["r2"] == pytest.approx(0.0)   # SS_res 5, SS_tot 5
    assert r["max_abs"] == 2.0 and r["p90"] == pytest.approx(np.percentile([0, 1, 0, 2], 90))


def test_the_sign_of_mbe_is_prediction_minus_observation():
    assert mt.point_metrics([10.0, 10.0], [12.0, 14.0])["mbe"] == pytest.approx(3.0)     # over-prediction
    assert mt.point_metrics([10.0, 10.0], [8.0, 6.0])["mbe"] == pytest.approx(-3.0)      # under-prediction


def test_a_perfect_prediction_and_a_constant_one():
    y = np.array([1.0, 5.0, 9.0])
    r = mt.point_metrics(y, y)
    assert r["rmse"] == 0 and r["r2"] == 1 and r["max_abs"] == 0
    assert mt.point_metrics(y, np.full(3, y.mean()))["r2"] == pytest.approx(0.0)


# ----------------------------------------- sufficient statistics == the row-level truth --

def test_metrics_from_per_day_sums_match_the_row_level_metrics():
    df = table()
    codes, n = mt.encode_days(df.date_local)
    S = mt.day_stats(df.real_wm2, df.H, codes, n)
    from_sums = mt.metrics_from_sums(S.sum(axis=0))
    direct = mt.point_metrics(df.real_wm2, df.H)
    for k in mt.POINT:
        assert from_sums[k] == pytest.approx(direct[k], rel=1e-12)


def test_a_bootstrap_resample_equals_the_metrics_of_the_concatenated_days():
    df = table(n_days=6, seed=1)
    codes, n = mt.encode_days(df.date_local)
    idx = mt.bootstrap_indices(n, n_boot=5, seed=3)
    S = mt.day_stats(df.real_wm2, df.H, codes, n)
    boot = mt.metrics_from_sums(S[idx].sum(axis=1))
    days = sorted(df.date_local.unique())
    for b in range(5):                                   # explicit loop over the resampled days' rows
        rows = pd.concat([df[df.date_local == days[j]] for j in idx[b]])
        direct = mt.point_metrics(rows.real_wm2, rows.H)
        for k in mt.POINT:
            assert boot[k][b] == pytest.approx(direct[k], rel=1e-10)


def test_the_bootstrap_indices_are_seeded_and_cover_the_days():
    a, b = mt.bootstrap_indices(15), mt.bootstrap_indices(15)
    assert a.shape == (2000, 15) and a.min() == 0 and a.max() == 14
    np.testing.assert_array_equal(a, b)
    assert not np.array_equal(a, mt.bootstrap_indices(15, seed=7))
    assert mt.N_BOOT == 2000 and mt.SEED == 42 and mt.CI == (2.5, 97.5)


# ------------------------------------------------------------------- the interval --

def test_the_interval_is_the_2_5_to_97_5_percentile_of_the_resampled_metric():
    df = table(seed=2)
    row = mt.summarize_group(df, ["H"])[0]
    codes, n = mt.encode_days(df.date_local)
    boot = mt.metrics_from_sums(mt.day_stats(df.real_wm2, df.H, codes, n)[mt.bootstrap_indices(n)].sum(axis=1))["rmse"]
    assert row["rmse_lo"] == pytest.approx(np.percentile(boot, 2.5)) and row["rmse_hi"] == pytest.approx(np.percentile(boot, 97.5))
    assert row["rmse_lo"] < row["rmse"] < row["rmse_hi"]


def test_the_interval_narrows_with_more_days():
    short, long_ = mt.summarize_group(table(n_days=5, seed=3), ["H"])[0], mt.summarize_group(table(n_days=60, seed=3), ["H"])[0]
    assert (long_["rmse_hi"] - long_["rmse_lo"]) < (short["rmse_hi"] - short["rmse_lo"])


def test_a_single_day_has_no_interval():
    r = mt.summarize_group(table(n_days=1), ["H"])[0]
    assert r["n_days"] == 1 and np.isnan(r["rmse_lo"]) and np.isnan(r["rmse_hi"]) and r["rmse"] > 0


# -------------------------------------------------------------------- the contrasts --

def test_the_gain_of_a_over_b_is_rmse_b_minus_rmse_a_and_positive_when_a_is_better():
    df = table(seed=4)
    g = mt.paired_gain(df, "A", "H")                     # A has the smaller noise
    assert g["gain"] == pytest.approx(mt.point_metrics(df.real_wm2, df.H)["rmse"] - mt.point_metrics(df.real_wm2, df.A)["rmse"])
    assert g["gain"] > 0
    assert mt.paired_gain(df, "H", "A")["gain"] == pytest.approx(-g["gain"])
    assert mt.CONTRASTS[0] == ("H", "D")                 # the main comparison, RMSE(D) - RMSE(H)


def test_a_model_against_itself_gains_exactly_nothing_with_a_degenerate_interval():
    g = mt.paired_gain(table(seed=5), "A", "A")
    assert g["gain"] == 0 and g["lo"] == 0 and g["hi"] == 0


def test_pairing_makes_the_interval_of_a_difference_narrower_than_independent_intervals():
    df = table(n_days=12, seed=6)
    g = mt.paired_gain(df, "A", "B")                     # B is A shifted by a constant: strongly paired
    ra, rb = (mt.summarize_group(df, [m])[0] for m in ("A", "B"))
    independent = np.sqrt((ra["rmse_hi"] - ra["rmse_lo"]) ** 2 + (rb["rmse_hi"] - rb["rmse_lo"]) ** 2)
    assert (g["hi"] - g["lo"]) < independent


def test_paired_models_are_resampled_on_the_same_days():
    df = table(n_days=7, seed=7)
    codes, n = mt.encode_days(df.date_local)
    idx = mt.bootstrap_indices(n)
    Sa, Sb = (mt.day_stats(df.real_wm2, df[m], codes, n) for m in ("A", "H"))
    expected = mt.metrics_from_sums(Sb[idx].sum(axis=1))["rmse"] - mt.metrics_from_sums(Sa[idx].sum(axis=1))["rmse"]
    g = mt.paired_gain(df, "A", "H")
    assert (g["lo"], g["hi"]) == pytest.approx((np.percentile(expected, 2.5), np.percentile(expected, 97.5)))


# ------------------------------------------------------------------- groups and rules --

def test_groups_follow_the_protocol_definitions():
    df = table()
    masks = mt.group_masks(df)
    assert {"overall", "horizontals", "shade:shaded", "shade:unshaded", "sensor:P0", "sensor:Pinc", "sky:sunny", "sky:rainy"} <= set(masks)
    assert (df.loc[masks["shade:shaded"], "sun_visibility"] < 0.5).all() and (df.loc[masks["shade:unshaded"], "sun_visibility"] == 1).all()
    partial = df.sun_visibility.between(0.5, 0.99)
    assert partial.any() and not (partial.to_numpy() & masks["shade:shaded"]).any() and not (partial.to_numpy() & masks["shade:unshaded"]).any()
    assert not (df.loc[masks["horizontals"], "sensor"] == "Pinc").any()
    assert masks["overall"].all()


def test_the_shade_groups_treat_visibility_exactly_one_half_as_neither_shaded_nor_unshaded():
    df = table(n_days=2, per_day=6)
    df["sun_visibility"] = [0.0, 0.49, 0.5, 0.51, 0.99, 1.0] * 2
    masks = mt.group_masks(df)
    assert df.loc[masks["shade:shaded"], "sun_visibility"].tolist() == [0.0, 0.49] * 2
    assert df.loc[masks["shade:unshaded"], "sun_visibility"].tolist() == [1.0] * 2


def test_the_mae_gain_is_a_gain_in_mae_and_differs_from_the_rmse_gain():
    df = table(seed=10)
    mae = lambda c: mt.point_metrics(df.real_wm2, df[c])["mae"]
    g = mt.paired_gain(df, "A", "H", metric="mae")
    assert g["metric"] == "mae" and g["gain"] == pytest.approx(mae("H") - mae("A"))
    assert g["gain"] != pytest.approx(mt.paired_gain(df, "A", "H", metric="rmse")["gain"])


def test_cloudy_and_rainy_can_be_pooled_into_one_group():
    df = table(n_days=8)
    masks = mt.group_masks(df)
    assert (masks["sky:cloudy+rainy"] == (masks["sky:cloudy"] | masks["sky:rainy"])).all()
    assert masks["sky:cloudy+rainy"].sum() == masks["sky:cloudy"].sum() + masks["sky:rainy"].sum()
    only_sunny = df.assign(sky="sunny")
    assert "sky:cloudy+rainy" not in mt.group_masks(only_sunny)


def test_a_model_is_scored_on_a_group_only_if_it_has_every_prediction_in_it():
    df = table(seed=8)
    df.loc[df.sensor == "Pinc", "H"] = np.nan             # like S, which is not scored on the tilted sensor
    met, con = mt.summarize(df, ["A", "H"], contrasts=[("A", "H")], n_boot=50)
    scored = lambda g: set(met[met.group == g].model)
    assert scored("overall") == {"A"}                     # H would be on fewer rows: not scored at all
    assert scored("horizontals") == {"A", "H"} and scored("sensor:P0") == {"A", "H"} and scored("sensor:Pinc") == {"A"}
    assert "overall" not in set(con.group) and {"horizontals", "sensor:P0"} <= set(con.group)


def test_summarize_returns_metrics_and_contrasts_with_intervals():
    df = table(n_days=8, seed=9)
    met, con = mt.summarize(df, ["A", "B", "H"], contrasts=[("A", "B"), ("A", "H")], n_boot=200)
    o = met[(met.group == "overall") & (met.model == "A")].iloc[0]
    assert o.n_rows == len(df) and o.n_days == 8 and o.rmse_lo < o.rmse < o.rmse_hi
    assert {"r2_lo", "mae_hi", "mbe_lo", "p90", "p99", "max_abs"} <= set(met.columns)
    c = con[(con.group == "overall") & (con.a == "A") & (con.b == "B") & (con.metric == "rmse")].iloc[0]
    assert c.lo <= c.gain <= c.hi
    assert set(con.metric) == {"rmse", "mae"}


def test_summarize_can_be_limited_to_some_groups():
    df = table(seed=11)
    met, con = mt.summarize(df, ["A", "H"], contrasts=[("A", "H")], n_boot=30, groups=["overall"])
    assert set(met.group) == {"overall"} and set(con.group) == {"overall"}
    full, _ = mt.summarize(df, ["A", "H"], contrasts=[], n_boot=30)
    assert len(set(full.group)) > 3
