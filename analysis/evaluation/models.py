"""The nine models of the protocol (section 2).

Every model has the same small interface::

    model.fit(train_df)        # no-op for the references
    model.predict(df) -> numpy array of W/m2, never negative

``df`` holds rows of dataset_v2 that have already passed the common evaluation mask; the mask
is applied once, upstream, and never inside a model. Learned models are trained on whatever
rows they are given and never see a column they should not (the targets of D and H are built
here, from ``real_wm2`` and the base signal).

IDs follow the protocol: A, B, S, R (references), P-cs, P-sim (2-parameter formulas), D, H, C
(XGBoost).
"""
from __future__ import annotations

from dataclasses import asdict, dataclass

import numpy as np
import pandas as pd
import xgboost as xgb
from scipy.optimize import least_squares

from .common import SENSORS, TARGET

# Fixed XGBoost parameters, kept from the TFG for each model family (Appendix B). The grid
# searched in selection.py only varies max_depth, n_estimators, min_child_weight and, for H and
# D, colsample_bytree.
HD_FIXED = dict(learning_rate=0.01, subsample=0.5, reg_alpha=0.1, reg_lambda=1.0, gamma=0.1, random_state=42)
C_FIXED = dict(learning_rate=0.01, subsample=0.8, reg_alpha=0.1, reg_lambda=4.0, gamma=0.0, random_state=42)

K_MAX = 2.0  # upper clip of k = real/base (protocol Appendix A); None means no upper clip

# The models of the evaluation, in the order every table uses. The single place where the list lives.
ALL_MODEL_IDS = ["A", "B", "S", "R", "P-cs", "P-sim", "D", "H", "C"]


class NotApplicable(Exception):
    """The model cannot be scored on these rows (C on a sensor it was not trained on)."""


@dataclass(frozen=True)
class XGBConfig:
    """The four hyperparameters that selection.py chooses."""
    max_depth: int
    n_estimators: int
    min_child_weight: float
    colsample_bytree: float


# --------------------------------------------------------------------------- references --

class Reference:
    """A model with nothing to learn: a formula over columns of the dataset."""
    learned = False

    def __init__(self, model_id: str, fn):
        self.id = model_id
        self._fn = fn

    def fit(self, df: pd.DataFrame) -> "Reference":
        return self

    def predict(self, df: pd.DataFrame) -> np.ndarray:
        return np.maximum(np.asarray(self._fn(df), dtype=float), 0.0)

    def fitted_params(self) -> dict:
        return {}


def _ray_cast(df: pd.DataFrame) -> pd.Series:
    # geometric_factor is the plugin's incidence cosine times the sun visibility
    # (GeometricFactor = Dot * SunVisibility), so it is already zero when the sun is hidden:
    # the visibility must not be multiplied in again.
    return df["clearsky_dni_wm2"] * df["geometric_factor"] + df["clearsky_dhi_wm2"] * df["sky_view_factor"]


# ---------------------------------------------------------------------------- parametric --

class Parametric:
    """``base * (1 - a * c**b)`` with ``c = cloud_opacity / 100``; a in [0, 1], b in [0.1, 5].

    Fitted by least squares on the residual ``real - prediction`` in W/m2 with
    scipy.optimize.least_squares (method 'trf', these bounds, default tolerances), started at
    a = 0.8, b = 1. There is nothing to tune.
    """
    learned = True
    A_BOUNDS, B_BOUNDS, START = (0.0, 1.0), (0.1, 5.0), (0.8, 1.0)

    def __init__(self, model_id: str, base_col: str):
        self.id = model_id
        self.base_col = base_col
        self.a_: float | None = None
        self.b_: float | None = None

    @staticmethod
    def _c(df: pd.DataFrame) -> np.ndarray:
        return np.clip(df["cloud_opacity"].to_numpy(float), 0.0, 100.0) / 100.0

    def fit(self, df: pd.DataFrame) -> "Parametric":
        base, c, y = df[self.base_col].to_numpy(float), self._c(df), df[TARGET].to_numpy(float)

        def residual(p):
            a, b = p
            return y - base * (1.0 - a * c ** b)

        res = least_squares(residual, x0=list(self.START), method="trf",
                            bounds=([self.A_BOUNDS[0], self.B_BOUNDS[0]], [self.A_BOUNDS[1], self.B_BOUNDS[1]]))
        self.a_, self.b_ = float(res.x[0]), float(res.x[1])
        return self

    def predict(self, df: pd.DataFrame) -> np.ndarray:
        if self.a_ is None:
            raise RuntimeError(f"{self.id} is not fitted")
        base = df[self.base_col].to_numpy(float)
        return np.maximum(base * (1.0 - self.a_ * self._c(df) ** self.b_), 0.0)

    def fitted_params(self) -> dict:
        return {"a": self.a_, "b": self.b_}


# -------------------------------------------------------------------------------- XGBoost --

def _importances(model: xgb.XGBRegressor, features: list[str]) -> dict:
    score = model.get_booster().get_score(importance_type="gain")
    return {f: float(score.get(f, 0.0)) for f in features}


class RatioXGB:
    """H (base = simulator) and D (base = clear-sky GHI): XGBoost on ``[base, cloud_opacity]``.

    Target ``k = clip(real / base, 0, k_max)`` and prediction ``max(clip(k_hat, 0, k_max) * base, 0)``.
    No additive constant in the denominator, so the two are exact inverses of each other (except
    where the clip acts). H and D share one configuration, chosen jointly by selection.py.
    """
    learned = True

    def __init__(self, model_id: str, base_col: str, config: XGBConfig, k_max: float | None = K_MAX, n_jobs: int = 1):
        self.id = model_id
        self.base_col = base_col
        self.config = config
        self.k_max = k_max
        self.features = [base_col, "cloud_opacity"]
        self._model = xgb.XGBRegressor(objective="reg:squarederror", n_jobs=n_jobs, verbosity=0,
                                       **HD_FIXED, **asdict(config))

    def _clip(self, k: np.ndarray) -> np.ndarray:
        return np.clip(k, 0.0, self.k_max) if self.k_max is not None else np.maximum(k, 0.0)

    def target(self, df: pd.DataFrame) -> np.ndarray:
        return self._clip(df[TARGET].to_numpy(float) / df[self.base_col].to_numpy(float))

    def fit(self, df: pd.DataFrame) -> "RatioXGB":
        base = df[self.base_col].to_numpy(float)
        if not (base > 0).all():
            raise ValueError(f"{self.id}: training rows with {self.base_col} <= 0; "
                             "the common mask must remove them before fitting")
        self._model.fit(df[self.features], self.target(df))
        return self

    def predict(self, df: pd.DataFrame) -> np.ndarray:
        k_hat = self._clip(self._model.predict(df[self.features]))
        return np.maximum(k_hat * df[self.base_col].to_numpy(float), 0.0)

    def fitted_params(self) -> dict:
        return {"config": asdict(self.config), "k_max": self.k_max,
                "feature_importances": _importances(self._model, self.features)}


class MeteoXGB:
    """C, the TFG's meteorological baseline: XGBoost on cloud opacity, sun angles, precipitable
    water and one dummy per sensor, predicting the irradiance directly.

    ``zenith`` and ``azimuth`` come from the simulator's sun position. It cannot be scored on a
    sensor it was not trained on (its dummy would be undefined): ``predict`` raises NotApplicable.
    """
    learned = True
    id = "C"

    def __init__(self, config: XGBConfig, n_jobs: int = 1):
        self.config = config
        self._model = xgb.XGBRegressor(objective="reg:squarederror", n_jobs=n_jobs, verbosity=0,
                                       **C_FIXED, **asdict(config))
        self.sensors_: list[str] | None = None
        self.features_: list[str] | None = None

    def _x(self, df: pd.DataFrame) -> pd.DataFrame:
        x = pd.DataFrame({
            "cloud_opacity": df["cloud_opacity"].to_numpy(float),
            "zenith": 90.0 - df["sun_altitude_deg"].to_numpy(float),
            "azimuth": df["sun_azimuth_deg"].to_numpy(float),
            "precipitable_water": df["precipitable_water"].to_numpy(float),
        }, index=df.index)
        for s in self.sensors_:
            x[f"sensor_{s}"] = (df["sensor"].to_numpy() == s).astype(float)
        return x

    def fit(self, df: pd.DataFrame) -> "MeteoXGB":
        seen = set(df["sensor"].unique())
        self.sensors_ = [s for s in SENSORS if s in seen]
        x = self._x(df)
        self.features_ = list(x.columns)
        self._model.fit(x, df[TARGET].to_numpy(float))
        return self

    def predict(self, df: pd.DataFrame) -> np.ndarray:
        if self.sensors_ is None:
            raise RuntimeError("C is not fitted")
        unseen = sorted(set(df["sensor"].unique()) - set(self.sensors_))
        if unseen:
            raise NotApplicable(f"C was not trained on sensor(s) {unseen}")
        return np.maximum(self._model.predict(self._x(df)), 0.0)

    def fitted_params(self) -> dict:
        return {"config": asdict(self.config), "sensors": self.sensors_,
                "feature_importances": _importances(self._model, self.features_)}


# ------------------------------------------------------------------------------- factory --

def build_models(hd_config: XGBConfig, c_config: XGBConfig, k_max: float | None = K_MAX, n_jobs: int = 1) -> dict:
    """The nine models, keyed by protocol ID. H and D get the same configuration."""
    return {
        "A": Reference("A", lambda d: d["sim_irradiance_wm2"]),
        "B": Reference("B", lambda d: d["clearsky_ghi_wm2"]),
        "S": Reference("S", lambda d: d["solcast_ghi_wm2"]),
        "R": Reference("R", _ray_cast),
        "P-cs": Parametric("P-cs", "clearsky_ghi_wm2"),
        "P-sim": Parametric("P-sim", "sim_irradiance_wm2"),
        "D": RatioXGB("D", "clearsky_ghi_wm2", hd_config, k_max, n_jobs),
        "H": RatioXGB("H", "sim_irradiance_wm2", hd_config, k_max, n_jobs),
        "C": MeteoXGB(c_config, n_jobs),
    }
