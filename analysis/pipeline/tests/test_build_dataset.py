"""Synthetic-data tests for the dataset builder: time handling, binning and every QC flag."""
import numpy as np
import pandas as pd
import pytest

import build_dataset as bd


def _write_real(path, start_local, n, values=None, **overrides):
    """A rad_*.csv with `n` 5 s rows starting at `start_local` (Madrid wall clock)."""
    ts = pd.date_range(start_local, periods=n, freq="5s")
    data = {c: (values if values is not None else np.zeros(n)) for c in bd.REAL_COLS}
    data.update(overrides)
    df = pd.DataFrame({"timestamp": ts.strftime("%Y-%m-%d %H:%M:%S"), **data, "ws": 1.0, "temp": 20.0})
    df.to_csv(path, index=False)


def _bins(*utc):
    return pd.DatetimeIndex([pd.Timestamp(t, tz="UTC") for t in utc])


# ------------------------------------------------------------------ real data --

def test_local_time_converted_to_utc_with_dst(tmp_path):
    # 2025-10-07 is CEST (UTC+2): 14:00 local == 12:00 UTC.
    _write_real(tmp_path / "rad_20251007.csv", "2025-10-07 14:00:00", 24, values=np.full(24, 100.0))
    # 2025-12-12 is CET (UTC+1): 13:00 local == 12:00 UTC.
    _write_real(tmp_path / "rad_20251212.csv", "2025-12-12 13:00:00", 24, values=np.full(24, 200.0))
    out = bd.load_real(tmp_path, _bins("2025-10-07 12:00", "2025-12-12 12:00"))
    assert out.loc[(pd.Timestamp("2025-10-07 12:00", tz="UTC"), "P0"), "real_wm2"] == 100.0
    assert out.loc[(pd.Timestamp("2025-12-12 12:00", tz="UTC"), "P0"), "real_wm2"] == 200.0


def test_bin_is_centered_on_the_simulation_timestamp(tmp_path):
    # 5 s samples from 11:59:00 to 12:00:55 UTC (24 samples) fall in the bin labelled 12:00.
    vals = np.arange(24, dtype=float)
    _write_real(tmp_path / "rad_a.csv", "2025-10-07 13:59:00", 24, values=vals)  # 13:59 CEST = 11:59 UTC
    out = bd.load_real(tmp_path, _bins("2025-10-07 12:00", "2025-10-07 11:58", "2025-10-07 12:02"))
    row = out.loc[(pd.Timestamp("2025-10-07 12:00", tz="UTC"), "P1")]
    assert row["real_n"] == 24
    assert row["real_wm2"] == pytest.approx(vals.mean())
    assert (pd.Timestamp("2025-10-07 11:58", tz="UTC"), "P1") not in out.index or \
        out.loc[(pd.Timestamp("2025-10-07 11:58", tz="UTC"), "P1"), "real_n"] == 0


def test_duplicated_timestamps_are_averaged_and_conflicts_flagged(tmp_path):
    _write_real(tmp_path / "rad_a.csv", "2025-10-07 13:59:00", 24, values=np.full(24, 100.0))
    _write_real(tmp_path / "rad_b.csv", "2025-10-07 13:59:00", 24, values=np.full(24, 100.0),
                P3=np.full(24, 140.0))  # only P3 disagrees
    out = bd.load_real(tmp_path, _bins("2025-10-07 12:00"))
    t = pd.Timestamp("2025-10-07 12:00", tz="UTC")
    assert out.loc[(t, "P0"), "real_n"] == 24                  # duplicates collapsed, not doubled
    assert out.loc[(t, "P3"), "real_wm2"] == pytest.approx(120.0)
    assert bool(out.loc[(t, "P3"), "real_dup_conflict"]) is True
    assert bool(out.loc[(t, "P0"), "real_dup_conflict"]) is False


def test_file_straddling_midnight_is_matched_by_timestamp_not_name(tmp_path):
    # A file named for day 1 that contains day-2 rows must still contribute them.
    _write_real(tmp_path / "rad_20250404.csv", "2025-04-05 09:00:00", 24, values=np.full(24, 77.0))
    out = bd.load_real(tmp_path, _bins("2025-04-05 07:00"))  # 09:00 CEST
    assert out.loc[(pd.Timestamp("2025-04-05 07:00", tz="UTC"), "P0"), "real_wm2"] == 77.0


# --------------------------------------------------------------------- meteo --

def _meteo(period_end, cloud):
    return pd.DataFrame({
        "period_end": pd.to_datetime(period_end, utc=True),
        "cloud_opacity": cloud, "ghi": 0.0, "clearsky_ghi": 0.0, "precipitable_water": 20.0, "dni": 600.0, "dhi": 80.0,
        "precipitation_rate": 0.4, "weather_type": "SUNNY",
    })


def test_solcast_is_anchored_to_the_middle_of_its_period(tmp_path):
    p = tmp_path / "m.csv"
    # Rows end at 12:05 and 12:10 -> they describe 12:02:30 and 12:07:30.
    _meteo(["2025-01-01T12:00:00Z", "2025-01-01T12:05:00Z", "2025-01-01T12:10:00Z"], [0, 10, 20]).to_csv(p, index=False)
    m = bd.load_meteo(p)
    assert list(m["center"]) == list(pd.to_datetime(
        ["2025-01-01T11:57:30Z", "2025-01-01T12:02:30Z", "2025-01-01T12:07:30Z"], utc=True))
    out = bd.meteo_at(m, _bins("2025-01-01 12:02:30", "2025-01-01 12:05:00"))
    assert out["cloud_opacity"].iloc[0] == pytest.approx(10.0)   # exactly on a centre
    assert out["cloud_opacity"].iloc[1] == pytest.approx(15.0)   # halfway between two
    assert out["meteo_gap_s"].iloc[1] == 300.0


def test_meteo_outside_the_file_is_nan_not_extrapolated(tmp_path):
    p = tmp_path / "m.csv"
    _meteo(["2025-01-01T12:00:00Z", "2025-01-01T12:05:00Z"], [5, 5]).to_csv(p, index=False)
    out = bd.meteo_at(bd.load_meteo(p), _bins("2025-01-01 11:00", "2025-01-02 12:00"))
    assert out["cloud_opacity"].isna().all() and out["meteo_gap_s"].isna().all()


# ------------------------------------------------------------------------ QC --

def _frame(**cols):
    """A one-sensor frame of `n` consecutive 2-min bins, all healthy by default."""
    n = len(next(iter(cols.values()))) if cols else 40
    base = {
        "sensor": "P0",
        "utc": pd.date_range("2025-06-01 10:00", periods=n, freq="2min", tz="UTC"),
        "real_wm2": np.linspace(500, 600, n),
        "real_n": 24, "real_std": 1.0, "real_dup_conflict": False,
        "sun_altitude_deg": 50.0, "clearsky_ghi_wm2": 800.0, "sim_irradiance_wm2": 790.0,
    }
    base.update(cols)
    return pd.DataFrame(base)


def test_healthy_data_has_no_flags():
    out = bd.add_qc(_frame())
    assert out[bd.QC_BAD].sum().sum() == 0 and out["qc_ok"].all()


def test_flatline_requires_a_long_constant_run():
    flat = np.concatenate([np.linspace(500, 520, 5), np.full(bd.FLATLINE_MIN_BINS, 550.0), np.linspace(560, 600, 20)])
    out = bd.add_qc(_frame(real_wm2=flat))
    assert out["qc_flatline"].sum() == bd.FLATLINE_MIN_BINS
    short = np.concatenate([np.linspace(500, 520, 5), np.full(bd.FLATLINE_MIN_BINS - 1, 550.0), np.linspace(560, 600, 20)])
    assert bd.add_qc(_frame(real_wm2=short))["qc_flatline"].sum() == 0


def test_zero_runs_at_night_are_not_flatlines():
    out = bd.add_qc(_frame(real_wm2=np.zeros(40), sun_altitude_deg=-10.0, clearsky_ghi_wm2=0.0, sim_irradiance_wm2=0.0))
    assert not out["qc_flatline"].any()


def test_flatline_does_not_leak_across_sensors():
    a = _frame(real_wm2=np.full(8, 300.0)).assign(sensor="P0")
    b = _frame(real_wm2=np.full(8, 300.0)).assign(sensor="P1")
    assert not bd.add_qc(pd.concat([a, b]))["qc_flatline"].any()  # 8 + 8 must not merge into 16


def test_offset_noise_is_not_flagged_but_large_negatives_are():
    r = np.linspace(500, 600, 40); r[3] = -1.5; r[7] = -20.0
    out = bd.add_qc(_frame(real_wm2=r))
    assert list(out.index[out["qc_neg_large"]]) == [7]


def test_night_signal_is_flagged():
    alt = np.full(40, -5.0); r = np.zeros(40); r[10] = 50.0; r[11] = 3.0
    out = bd.add_qc(_frame(real_wm2=r, sun_altitude_deg=alt, clearsky_ghi_wm2=0.0, sim_irradiance_wm2=0.0))
    assert list(out.index[out["qc_night_nonzero"]]) == [10]


def test_dead_sensor_day_is_flagged_but_a_cloudy_day_is_not():
    dead = bd.add_qc(_frame(real_wm2=np.full(40, 1.0) + np.arange(40) * 0.01))
    assert dead["qc_sensor_dead"].all()
    # Low reading AND a dim sky model: a genuinely dark day, not a dead sensor.
    dark = bd.add_qc(_frame(real_wm2=np.full(40, 5.0) + np.arange(40) * 0.01, clearsky_ghi_wm2=100.0, sim_irradiance_wm2=90.0))
    assert not dark["qc_sensor_dead"].any()


def _multi(zeros_at, n_sensors=5, n=20):
    """`n_sensors` sensors over `n` bins; `zeros_at[sensor]` = bin indices reading exactly 0."""
    frames = []
    for i in range(n_sensors):
        r = np.linspace(500, 600, n); std = np.full(n, 1.0)
        for k in zeros_at.get(i, []):
            r[k], std[k] = 0.0, 0.0
        frames.append(_frame(real_wm2=r, real_std=std).assign(sensor=f"P{i}"))
    return pd.concat(frames, ignore_index=True)


def test_unanimous_zero_in_daylight_is_a_blackout():
    out = bd.add_qc(_multi({i: [4, 5] for i in range(5)}))
    assert out.groupby("utc")["qc_zero_daylight"].all().sum() == 2    # bins 4 and 5, every sensor
    assert out["qc_zero_daylight"].sum() == 2 * 5


def test_zeros_on_only_some_sensors_are_a_storm_not_a_blackout():
    out = bd.add_qc(_multi({0: [4], 1: [4], 2: [4]}))                 # 3 of 5 read 0, the rest read ~500
    assert not out["qc_zero_daylight"].any()


def test_a_dead_sensor_does_not_veto_a_blackout():
    # P0 is dead all day (always 0); the four working sensors black out at bin 7.
    frames = [_frame(real_wm2=np.zeros(20) + np.arange(20) * 0.01).assign(sensor="P0")]
    for i in range(1, 5):
        r = np.linspace(500, 600, 20); std = np.full(20, 1.0); r[7], std[7] = 0.0, 0.0
        frames.append(_frame(real_wm2=r, real_std=std).assign(sensor=f"P{i}"))
    out = bd.add_qc(pd.concat(frames, ignore_index=True))
    assert out.loc[out["sensor"] != "P0", "qc_zero_daylight"].sum() == 4


def test_lone_zero_with_strong_peers_is_a_channel_dropout():
    out = bd.add_qc(_multi({2: [4, 5]}))                              # P2 reads 0 on bins 4-5, peers ~500
    assert out.loc[out["sensor"] == "P2", "qc_channel_dropout"].sum() == 2
    assert out.loc[out["sensor"] != "P2", "qc_channel_dropout"].sum() == 0
    assert not out["qc_zero_daylight"].any()                           # not a blackout: peers are fine


def test_storm_zeros_with_weak_peers_are_not_a_dropout():
    r = np.full(20, 4.0)                                               # everyone reads a few W/m2
    frames = []
    for i in range(5):
        x = r.copy(); std = np.full(20, 0.5)
        if i < 3:
            x[6], std[6] = 0.0, 0.0
        frames.append(_frame(real_wm2=x, real_std=std, clearsky_ghi_wm2=300.0, sim_irradiance_wm2=300.0).assign(sensor=f"P{i}"))
    out = bd.add_qc(pd.concat(frames, ignore_index=True))
    assert not out["qc_channel_dropout"].any() and not out["qc_zero_daylight"].any()


def test_zeros_at_night_are_not_a_blackout():
    out = bd.add_qc(_frame(real_wm2=np.zeros(40), real_std=0.0, sun_altitude_deg=-10.0,
                           clearsky_ghi_wm2=0.0, sim_irradiance_wm2=0.0))
    assert not out["qc_zero_daylight"].any()


def test_sparse_and_missing_bins_are_flagged():
    n = np.full(40, 24.0); n[0] = 11; n[1] = 12; n[2] = np.nan
    r = np.linspace(500, 600, 40); r[2] = np.nan
    out = bd.add_qc(_frame(real_n=n, real_wm2=r))
    assert list(out.index[out["qc_real_sparse"]]) == [0, 2]


def test_over_clearsky_uses_the_sensors_own_ceiling():
    r = np.linspace(500, 600, 40); r[5] = 1500.0
    horizontal = bd.add_qc(_frame(real_wm2=r))                           # ceiling 800 -> limit 1330
    assert list(horizontal.index[horizontal["qc_over_clearsky"]]) == [5]
    tilted = bd.add_qc(_frame(real_wm2=r, sim_irradiance_wm2=1006.0))   # ceiling 1006 -> limit 1660
    assert not tilted["qc_over_clearsky"].any()
    # A diagnostic, not a fault: the flagged row stays in qc_ok.
    assert horizontal.loc[5, "qc_over_clearsky"] and horizontal.loc[5, "qc_ok"]
    assert "qc_over_clearsky" not in bd.QC_BAD


# ----------------------------------------------------------------- end to end --

def test_build_end_to_end_keeps_every_simulated_row(tmp_path):
    sim_dir, real_dir = tmp_path / "sim", tmp_path / "real"
    sim_dir.mkdir(); real_dir.mkdir()
    times = pd.date_range("2025-10-07 10:00", periods=10, freq="2min", tz="UTC")
    rows = []
    for name in ["CLASE_A", "INCLINADO", "P1"]:
        for t in times:
            rows.append({
                "sensor_name": name, "utc": t.strftime("%Y-%m-%dT%H:%M:%S.000Z"),
                "pos_x": 0, "pos_y": 0, "pos_z": 0, "n_x": 0, "n_y": 0, "n_z": 1,
                "azimuth_deg": 180.0, "altitude_deg": 40.0, "clearsky_ghi_wm2": 700.0,
                "clearsky_dni_wm2": 800.0, "clearsky_dhi_wm2": 100.0, "geometric_factor": 0.6,
                "sun_occluded": 0, "sun_hit_distance_m": -1.0, "sun_visibility": 1.0,
                "sky_view_factor": 0.99, "raw_r_lux": 1.0, "raw_g_lux": 1.0, "raw_b_lux": 1.0,
                "sim_comp_amb_lux": 1.0, "sim_comp_direct_lux": 1.0, "irr_final_normalized_wm2": 650.0,
            })
    pd.DataFrame(rows).to_csv(sim_dir / "campaign_20251007.csv", index=False)
    # Real data covers only the first half of the window: the rest must stay, flagged as sparse.
    _write_real(real_dir / "rad_20251007.csv", "2025-10-07 11:59:00", 240, values=np.linspace(500, 600, 240))
    _meteo(pd.date_range("2025-10-07 09:55", periods=20, freq="5min", tz="UTC"), 30.0).to_csv(tmp_path / "m.csv", index=False)

    df = bd.build(sim_dir, real_dir, tmp_path / "m.csv")
    assert len(df) == 30
    assert set(df["sensor"]) == {"P0", "Pinc", "P1"}               # simulator names mapped to pyranometer names
    assert not df.duplicated(["sensor", "utc"]).any()
    assert df["utc"].is_monotonic_increasing
    covered = df["utc"] < pd.Timestamp("2025-10-07 10:20", tz="UTC")
    assert (df.loc[covered, "real_n"] == 24).all()
    assert df.loc[~covered, "qc_real_sparse"].all() and not df.loc[covered, "qc_real_sparse"].any()
    assert df["cloud_opacity"].eq(30.0).all()
    assert df["precipitable_water"].eq(20.0).all()
    assert df["solcast_dni_wm2"].eq(600.0).all() and df["solcast_dhi_wm2"].eq(80.0).all()
    assert df["precipitation_rate"].eq(0.4).all()
