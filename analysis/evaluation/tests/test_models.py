"""Synthetic-data tests for the nine protocol models."""
import numpy as np
import pandas as pd
import pytest

from evaluation import models as m
from synthetic import make_df
from evaluation.models import NotApplicable, XGBConfig

CFG = XGBConfig(max_depth=3, n_estimators=800, min_child_weight=5, colsample_bytree=1.0)
SMALL = XGBConfig(max_depth=2, n_estimators=100, min_child_weight=20, colsample_bytree=0.5)


# ---------------------------------------------------------------- references ---

def test_references_return_their_column_and_never_go_negative():
    df = make_df(50)
    df.loc[df.index[:3], ["sim_irradiance_wm2", "clearsky_ghi_wm2", "solcast_ghi_wm2"]] = -5.0
    mods = m.build_models(SMALL, SMALL)
    for mid, col in [("A", "sim_irradiance_wm2"), ("B", "clearsky_ghi_wm2"), ("S", "solcast_ghi_wm2")]:
        pred = mods[mid].fit(df).predict(df)
        np.testing.assert_allclose(pred, np.maximum(df[col].to_numpy(), 0.0))
        assert (pred >= 0).all()
        assert mods[mid].learned is False and mods[mid].fitted_params() == {}


def test_ray_cast_formula_does_not_count_the_visibility_twice():
    df = make_df(20)
    df["geometric_factor"] = [0.0] * 10 + [0.5] * 10        # first ten rows: sun hidden
    r = m.build_models(SMALL, SMALL)["R"]
    expected = df.clearsky_dni_wm2 * df.geometric_factor + df.clearsky_dhi_wm2 * df.sky_view_factor
    np.testing.assert_allclose(r.predict(df), expected)
    np.testing.assert_allclose(r.predict(df)[:10], (df.clearsky_dhi_wm2 * df.sky_view_factor)[:10])  # diffuse only
    # it does not read a visibility column at all
    assert "sun_visibility" not in df.columns


# ---------------------------------------------------------------- parametric ---

@pytest.mark.parametrize("a,b", [(0.7, 1.5), (0.9, 0.6), (0.4, 2.5)])
def test_parametric_recovers_a_known_curve(a, b):
    df = make_df(800)
    df["real_wm2"] = df.sim_irradiance_wm2 * (1 - a * (df.cloud_opacity / 100) ** b)
    p = m.Parametric("P-sim", "sim_irradiance_wm2").fit(df)
    assert p.a_ == pytest.approx(a, abs=1e-3) and p.b_ == pytest.approx(b, abs=1e-2)
    np.testing.assert_allclose(p.predict(df), df.real_wm2, rtol=1e-3, atol=0.5)


def test_parametric_respects_its_bounds():
    df = make_df(800)
    df["real_wm2"] = df.sim_irradiance_wm2 * (1 - 1.3 * (df.cloud_opacity / 100))   # a = 1.3 is out of range
    p = m.Parametric("P-sim", "sim_irradiance_wm2").fit(df)
    assert 0.0 <= p.a_ <= 1.0 and 0.1 <= p.b_ <= 5.0
    assert p.a_ == pytest.approx(1.0, abs=1e-6)
    assert (p.predict(df) >= 0).all()


def test_parametric_is_deterministic_and_uses_its_own_base():
    df = make_df(600)
    p1 = m.Parametric("P-sim", "sim_irradiance_wm2").fit(df)
    p2 = m.Parametric("P-sim", "sim_irradiance_wm2").fit(df)
    assert (p1.a_, p1.b_) == (p2.a_, p2.b_)
    pcs = m.Parametric("P-cs", "clearsky_ghi_wm2").fit(df)       # real follows sim, not clear-sky
    assert np.sqrt(np.mean((p1.predict(df) - df.real_wm2) ** 2)) < np.sqrt(np.mean((pcs.predict(df) - df.real_wm2) ** 2))
    assert set(p1.fitted_params()) == {"a", "b"}


def test_parametric_clips_cloud_opacity_to_0_100_and_needs_a_fit():
    df = make_df(100)
    p = m.Parametric("P-sim", "sim_irradiance_wm2")
    with pytest.raises(RuntimeError):
        p.predict(df)
    p.fit(df)
    hi, capped = df.copy(), df.copy()
    hi["cloud_opacity"], capped["cloud_opacity"] = 120.0, 100.0
    np.testing.assert_allclose(p.predict(hi), p.predict(capped))


# ------------------------------------------------------------------- H and D ---

def test_target_has_no_epsilon_and_is_the_inverse_of_the_reconstruction():
    df = make_df(10)
    h = m.RatioXGB("H", "sim_irradiance_wm2", SMALL)
    df["sim_irradiance_wm2"] = 33.0
    df["real_wm2"] = [33.0, 66.0, 16.5] + [0.0] * 7
    k = h.target(df)
    np.testing.assert_allclose(k[:3], [1.0, 2.0, 0.5])            # real / sim, with no constant added
    np.testing.assert_allclose(df.sim_irradiance_wm2 * k, df.real_wm2.clip(upper=2 * 33.0))


@pytest.mark.parametrize("mid,base_col", [("H", "sim_irradiance_wm2"), ("D", "clearsky_ghi_wm2")])
def test_a_perfect_k_is_reconstructed_exactly(mid, base_col):
    """The TFG trained on real/(sim+eps) but predicted k*sim, so even a perfect k under-predicted.
    Here a stand-in for XGBoost returns the exact target; the prediction must equal the truth."""
    df = make_df(300)
    df["real_wm2"] = df[base_col] * np.random.RandomState(7).uniform(0.05, 1.6, len(df))   # k inside the clip
    model = m.RatioXGB(mid, base_col, SMALL)
    model._model = type("Oracle", (), {"predict": staticmethod(lambda X: model.target(df))})()
    np.testing.assert_allclose(model.predict(df), df["real_wm2"], rtol=1e-12)


def test_the_clip_acts_only_on_k_and_k_max_can_be_changed_or_removed():
    df = make_df(4)
    df["sim_irradiance_wm2"] = 100.0
    df["real_wm2"] = [-10.0, 150.0, 250.0, 450.0]
    ks = {kmax: m.RatioXGB("H", "sim_irradiance_wm2", SMALL, kmax).target(df) for kmax in (2.0, 3.0, None)}
    np.testing.assert_allclose(ks[2.0], [0.0, 1.5, 2.0, 2.0])
    np.testing.assert_allclose(ks[3.0], [0.0, 1.5, 2.5, 3.0])
    np.testing.assert_allclose(ks[None], [0.0, 1.5, 2.5, 4.5])   # lower clip stays at zero


def test_fit_refuses_rows_where_the_base_is_not_positive():
    df = make_df(50)
    df.loc[df.index[0], "sim_irradiance_wm2"] = 0.0
    with pytest.raises(ValueError):
        m.RatioXGB("H", "sim_irradiance_wm2", SMALL).fit(df)


def test_h_and_d_read_only_their_own_two_columns():
    df = make_df(400)
    h = m.RatioXGB("H", "sim_irradiance_wm2", SMALL).fit(df)
    d = m.RatioXGB("D", "clearsky_ghi_wm2", SMALL).fit(df)
    assert h.features == ["sim_irradiance_wm2", "cloud_opacity"]
    assert d.features == ["clearsky_ghi_wm2", "cloud_opacity"]
    scrambled = df.copy()                      # columns the models must NOT read
    rng = np.random.RandomState(1)
    for c in ["sensor", "sun_azimuth_deg", "precipitable_water", "geometric_factor", "solcast_ghi_wm2"]:
        scrambled[c] = rng.permutation(scrambled[c].to_numpy())
    np.testing.assert_allclose(h.predict(df), h.predict(scrambled))
    np.testing.assert_allclose(d.predict(df), d.predict(scrambled))


def test_h_learns_a_known_cloud_law_and_predictions_are_deterministic_and_non_negative():
    train, test = make_df(2500, seed=1), make_df(500, seed=2)
    h = m.RatioXGB("H", "sim_irradiance_wm2", CFG).fit(train)
    pred = h.predict(test)
    assert (pred >= 0).all()
    assert np.mean(np.abs(pred - test.real_wm2) / np.maximum(test.real_wm2, 1.0)) < 0.06
    h2 = m.RatioXGB("H", "sim_irradiance_wm2", CFG).fit(train)
    np.testing.assert_array_equal(pred, h2.predict(test))


def test_h_and_d_in_build_models_share_one_configuration():
    mods = m.build_models(CFG, SMALL)
    assert mods["H"].config is mods["D"].config is CFG
    assert mods["H"].k_max == mods["D"].k_max == m.K_MAX == 2.0
    assert mods["C"].config is SMALL


def test_ratio_model_reports_config_and_importances():
    df = make_df(600)
    h = m.RatioXGB("H", "sim_irradiance_wm2", CFG).fit(df)
    fp = h.fitted_params()
    assert fp["config"] == {"max_depth": 3, "n_estimators": 800, "min_child_weight": 5, "colsample_bytree": 1.0}
    assert set(fp["feature_importances"]) == {"sim_irradiance_wm2", "cloud_opacity"}
    assert fp["k_max"] == 2.0


# ------------------------------------------------------------------------- C ---

def test_c_builds_zenith_from_the_simulator_sun_and_one_dummy_per_trained_sensor():
    df = make_df(300, sensors=("P0", "P1"))
    c = m.MeteoXGB(SMALL).fit(df)
    x = c._x(df)
    np.testing.assert_allclose(x["zenith"], 90.0 - df.sun_altitude_deg)
    np.testing.assert_allclose(x["azimuth"], df.sun_azimuth_deg)
    assert c.sensors_ == ["P0", "P1"]
    assert [col for col in c.features_ if col.startswith("sensor_")] == ["sensor_P0", "sensor_P1"]
    np.testing.assert_array_equal(x["sensor_P0"], (df.sensor == "P0").astype(float))
    assert set(c.fitted_params()["feature_importances"]) == set(c.features_)


def test_c_cannot_be_scored_on_a_sensor_it_was_not_trained_on():
    train = make_df(400, sensors=("P0", "P1"))
    c = m.MeteoXGB(SMALL).fit(train)
    ok = make_df(50, seed=3, sensors=("P0", "P1"))
    assert (c.predict(ok) >= 0).all()
    unseen = make_df(50, seed=3, sensors=("P3",))
    with pytest.raises(NotApplicable, match="P3"):
        c.predict(unseen)


# ----------------------------------------------------------- all nine together ---

def test_the_nine_models_have_the_protocol_ids_and_learned_flags():
    mods = m.build_models(SMALL, SMALL)
    assert list(mods) == ["A", "B", "S", "R", "P-cs", "P-sim", "D", "H", "C"]
    assert {k for k, v in mods.items() if not v.learned} == {"A", "B", "S", "R"}


def test_every_model_fits_and_predicts_finite_non_negative_values():
    train, test = make_df(800, seed=4), make_df(200, seed=5)
    for mid, model in m.build_models(SMALL, SMALL).items():
        pred = model.fit(train).predict(test)
        assert pred.shape == (200,), mid
        assert np.isfinite(pred).all() and (pred >= 0).all(), mid
