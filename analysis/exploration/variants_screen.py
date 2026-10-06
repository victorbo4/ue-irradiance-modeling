"""A small screen of five variants of the corrector, on the 43 development days.

Exploratory and run after the protocol-v1 evaluation, with the results of that evaluation already
seen; it is a screen, not a test. Nothing here uses the 15 test days.

    python analysis/exploration/variants_screen.py

Each variant is fitted with leave-one-month-out over the 43 training days (the E4 folds: nine
folds, every day scored once by a model that did not see its month). It writes
``variants_screen_output.md`` next to this file: the pooled RMSE of each variant, and its gain over
the model it is meant to improve, with a paired block-bootstrap interval over days.

    P-exp      base * exp(-a c^b)                                      vs P-sim
    P-sim+PW   base * (1 - a c^b) * (1 + g (pw - pw0))                 vs P-sim
    H+PW       XGBoost on [sim, cloud, precipitable_water]             vs H (same fixed configuration)
    DD-sim     (1 - a1 c^b1) Dir + (1 - a2 c^b2) Dif_sim               vs P-sim
    DD-ray     (1 - a1 c^b1) Dir + (1 - a2 c^b2) Dif_ray               vs P-sim

c = cloud_opacity / 100 and pw is precipitable water (pw0 is its mean in the fitting rows).
Dir = clear-sky DNI * geometric_factor (the simulator's own analytic direct term), Dif_sim = sim - Dir
(the part that comes from the cubemaps), Dif_ray = clear-sky DHI * sky_view_factor. H and H+PW use one
fixed configuration (depth 2, 300 trees, min_child_weight 200, colsample 0.5) instead of selecting it
in every fold. S-geo, the model added after protocol-v1, is shown as a plain reference line.
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.optimize import least_squares

HERE = Path(__file__).resolve().parent
ANALYSIS = HERE.parent
sys.path.insert(0, str(ANALYSIS))
from evaluation import experiments as ex  # noqa: E402
from evaluation import metrics as mt  # noqa: E402
from evaluation import models as M  # noqa: E402

DATA = ANALYSIS / "data"
OUT = HERE / "variants_screen_output.md"
COLUMNS = ["utc", "date_local", "sensor", "qc_ok", "sun_altitude_deg", "sun_visibility", "clearsky_ghi_wm2",
           "clearsky_dni_wm2", "clearsky_dhi_wm2", "geometric_factor", "sky_view_factor", "sim_irradiance_wm2",
           "solcast_dni_wm2", "solcast_dhi_wm2", "cloud_opacity", "precipitable_water", "real_wm2"]
H_FIXED = M.XGBConfig(max_depth=2, n_estimators=300, min_child_weight=200, colsample_bytree=0.5)


def _c(df):
    return np.clip(df["cloud_opacity"].to_numpy(float), 0.0, 100.0) / 100.0


class Formula:
    """A model ``f(df, params)`` fitted by bounded least squares on the residual in W/m2."""

    def __init__(self, fn, start, lower, upper):
        self.fn, self.start, self.lower, self.upper = fn, start, lower, upper

    def fit(self, df):
        y = df["real_wm2"].to_numpy(float)
        self.ctx = {"pw0": float(df["precipitable_water"].mean())}
        res = least_squares(lambda p: y - self.fn(df, p, self.ctx), self.start, method="trf",
                            bounds=(self.lower, self.upper))
        self.params = res.x
        return self

    def predict(self, df):
        return np.maximum(self.fn(df, self.params, self.ctx), 0.0)


def _direct(df):
    return df["clearsky_dni_wm2"].to_numpy(float) * df["geometric_factor"].to_numpy(float)


def _p(base):
    return lambda df, p, ctx: df[base].to_numpy(float) * (1.0 - p[0] * _c(df) ** p[1])


def _p_exp(df, p, ctx):
    return df["sim_irradiance_wm2"].to_numpy(float) * np.exp(-p[0] * _c(df) ** p[1])


def _p_pw(df, p, ctx):
    pw = df["precipitable_water"].to_numpy(float) - ctx["pw0"]
    return df["sim_irradiance_wm2"].to_numpy(float) * (1.0 - p[0] * _c(df) ** p[1]) * (1.0 + p[2] * pw)


def _dd(dif):
    def f(df, p, ctx):
        c = _c(df)
        return (1.0 - p[0] * c ** p[1]) * _direct(df) + (1.0 - p[2] * c ** p[3]) * dif(df)
    return f


def _dif_sim(df):
    return df["sim_irradiance_wm2"].to_numpy(float) - _direct(df)


def _dif_ray(df):
    return df["clearsky_dhi_wm2"].to_numpy(float) * df["sky_view_factor"].to_numpy(float)


AB = ([0.0, 0.1], [1.0, 5.0])
DD = ([0.0, 0.1, 0.0, 0.1], [1.0, 5.0, 1.0, 5.0])


class HPlusPW(M.RatioXGB):
    """H with precipitable water as a third input, in the same fixed configuration."""

    def __init__(self, config):
        super().__init__("H+PW", "sim_irradiance_wm2", config)
        self.features = ["sim_irradiance_wm2", "cloud_opacity", "precipitable_water"]


def make_variants():
    """name -> factory of a fresh model. The first two are the references the others are compared with."""
    return {
        "P-sim": lambda: Formula(_p("sim_irradiance_wm2"), [0.8, 1.0], *AB),
        "H": lambda: M.RatioXGB("H", "sim_irradiance_wm2", H_FIXED),
        "P-exp": lambda: Formula(_p_exp, [0.8, 1.0], *AB),
        "P-sim+PW": lambda: Formula(_p_pw, [0.8, 1.0, 0.0], [0.0, 0.1, -0.05], [1.0, 5.0, 0.05]),
        "H+PW": lambda: HPlusPW(H_FIXED),
        "DD-sim": lambda: Formula(_dd(_dif_sim), [0.8, 1.0, 0.8, 1.0], *DD),
        "DD-ray": lambda: Formula(_dd(_dif_ray), [0.8, 1.0, 0.8, 1.0], *DD),
    }


COMPARED_WITH = {"P-exp": "P-sim", "P-sim+PW": "P-sim", "H+PW": "H", "DD-sim": "P-sim", "DD-ray": "P-sim"}


def development_rows(df, split):
    """The 43 training days under the common mask, with the E4 folds (one month out)."""
    d = ex.prepare(df, split)
    return d.loc[d["split"] == "train"].reset_index(drop=True)


def cross_validate(d, factories):
    """Out-of-fold predictions, one column per variant."""
    month = d["date_local"].str[:7].to_numpy()
    pred = {name: np.full(len(d), np.nan) for name in factories}
    for mo in sorted(set(month)):
        tr, va = d.loc[month != mo], month == mo
        for name, make in factories.items():
            pred[name][va] = make().fit(tr).predict(d.loc[va])
    out = d.copy()
    for name, p in pred.items():
        out[name] = p
    out["S-geo"] = np.maximum(M.solcast_geometry(d).to_numpy(float), 0.0)   # nothing to fit: a reference line
    return out


def table(out):
    rows = []
    for scope, sub in (("nine sensors", out), ("horizontals", out[out["sensor"] != "Pinc"])):
        rmse = {m: mt.point_metrics(sub["real_wm2"], sub[m])["rmse"] for m in [*make_variants(), "S-geo"]}
        for name in [*make_variants(), "S-geo"]:
            row = {"scope": scope, "model": name, "rmse": rmse[name], "versus": "", "gain": "", "interval": ""}
            if name in COMPARED_WITH:
                g = mt.paired_gain(sub, name, COMPARED_WITH[name])
                row.update(versus=COMPARED_WITH[name], gain=f"{g['gain']:+.2f}", interval=f"[{g['lo']:+.2f}, {g['hi']:+.2f}]")
            rows.append(row)
    return pd.DataFrame(rows)


def render(t, d):
    lines = [
        "# Variants screen (exploratory)", "",
        f"{d['date_local'].nunique()} development days, {len(d)} rows, leave-one-month-out (nine folds). "
        "Run after the protocol-v1 results were seen; the 15 test days are not used. "
        "Gain = RMSE of the model it is compared with minus its own RMSE, in W/m2 (positive is better), "
        "with a 95 % paired block-bootstrap interval over days.", ""]
    for scope, sub in t.groupby("scope", sort=False):
        lines += [f"## {scope}", "", "| model | RMSE | compared with | gain | 95 % interval |", "|---|---|---|---|---|"]
        lines += [f"| {r.model} | {r.rmse:.2f} | {r.versus} | {r.gain} | {r.interval} |" for r in sub.itertuples()]
        lines.append("")
    lines.append("S-geo has no parameters and is shown only as a reference line. P-sim reproduces the E4 pooled "
                 "value of the evaluation (91.86). H here uses one fixed configuration, so it is a little worse "
                 "than the evaluation's H (99.24, configuration selected in every fold); H+PW is compared with this H.")
    return "\n".join(lines) + "\n"


def main():
    df = pd.read_csv(DATA / "dataset_v2.csv", usecols=COLUMNS, low_memory=False)
    split = pd.read_csv(DATA / "split_v2.csv", index_col=0)
    d = development_rows(df, split)
    out = cross_validate(d, make_variants())
    OUT.write_text(render(table(out), d))
    print(OUT.read_text())


if __name__ == "__main__":
    main()
