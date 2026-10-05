# Evaluation protocol

**Status: DRAFT.** It becomes binding when tagged `protocol-v1`. After the tag, nothing
below may change in response to a result; anything changed afterwards is reported as
exploratory.

The aim is the TFG's method, evaluated properly. Model structure, features, target
and hyperparameters are the TFG's. What changes is how the models are compared.

## 1. Data

- Dataset: `analysis/data/dataset_v2.csv` (see `pipeline/README.md`).
- Sensors: **P0, P1, P3, P4, P5, P6, P7, P8, Pinc** (P2 excluded: `KNOWN_ISSUES.md` M1).
- Day split: `analysis/data/split_v2.csv`: 43 training days, 15 test days. Days are the
  unit of splitting; rows of one day never straddle train and test.
- **Common evaluation mask**, applied once and identically to every model:
  `sun_altitude_deg > 5` and `qc_ok` and the sensor is in the list above.
  Training rows use the same mask. The mask is never applied per model.
- Sensitivity mask: `sun_altitude_deg > 10` (same evaluation, second table).

## 2. Models

Seven models, in three groups. Only the learned ones are trained; the references are just scored.

| ID | Name | Learned? | Inputs | Prediction | Question it answers |
|---|---|---|---|---|---|
| A | Physical simulation | no | `sim_irradiance_wm2` | `max(sim, 0)` | What does the 3D engine give on its own? |
| B | Clear-sky | no | `clearsky_ghi_wm2` | `max(cs, 0)` | What does the clear-sky physics give, with no clouds and no geometry? |
| P-cs | Parametric, clear-sky | 2 parameters | `clearsky_ghi_wm2`, `cloud_opacity` | `max(cs·(1 − a·c^b), 0)` | Clouds on top of clear-sky, no 3D, no trees |
| P-sim | Parametric, simulator | 2 parameters | `sim_irradiance_wm2`, `cloud_opacity` | `max(sim·(1 − a·c^b), 0)` | The same with 3D. Does the 3D gain survive a simple formula? |
| D | XGBoost, clear-sky | XGBoost | `clearsky_ghi_wm2`, `cloud_opacity` | `max(clip(k̂)·cs, 0)`, `k = real/(cs+10)` | **Clean ablation of H** (no 3D) |
| **H** | **XGBoost, hybrid (TFG final model)** | XGBoost | `sim_irradiance_wm2`, `cloud_opacity` | `max(clip(k̂, 0, 1.5)·sim, 0)`, `k = clip(real/(sim+10), 0, 1.5)` | **The proposed method** |
| C | XGBoost meteo (TFG baseline C) | XGBoost | `cloud_opacity`, `zenith`, `azimuth`, `precipitable_water`, sensor dummies | `real` directly | Can the data alone learn each sensor's shadow, with no 3D? |

How the pairs read:

- **H against D** is the headline: the only difference is the geometry signal
  (`sim_irradiance_wm2` against `clearsky_ghi_wm2`); learner, parameters, target form and
  rows are the same. **P-sim against P-cs** is the same comparison with a 2-parameter
  formula, so the 3D gain does not depend on the learner.
- **H against P-sim** says whether XGBoost adds anything over a 2-parameter formula.
- **H against C** says whether the 3D adds anything over a model that can memorise each
  sensor's shadow. C carries per-sensor dummies and sun angles, so it can in principle learn
  where shade falls; it is a rival, not an ablation. C cannot be scored on an unseen sensor,
  which is what E2 tests.

Details:

- P models: `c = cloud_opacity/100`. `a` in [0, 1] and `b` in [0.1, 5] are fitted by least
  squares (RMSE) on the training rows, started at `a = 0.8, b = 1`. Rows need `base ≥ 1`
  as for H. There is nothing to tune.
- Hyperparameters, inherited unchanged from the TFG and **fixed**:
  - H and D: `n_estimators=800, learning_rate=0.01, max_depth=4, min_child_weight=70,
    subsample=0.5, colsample_bytree=0.5, reg_alpha=0.1, reg_lambda=1.0, gamma=0.1,
    random_state=42`.
  - C: `n_estimators=800, learning_rate=0.01, max_depth=5, min_child_weight=60,
    subsample=0.8, colsample_bytree=0.8, reg_alpha=0.1, reg_lambda=4.0, gamma=0.0,
    random_state=42`.
- `zenith` and `azimuth` for C come from the simulator's sun position (`90 - sun_altitude_deg`,
  `sun_azimuth_deg`). `precipitable_water` comes from Solcast.

## 3. Experiments

The four experiments are four ways of splitting the data into "what a model is trained on"
and "what it is tested on". They all start from the day split (43 training days, 15 test
days) and use the sensors, sun-visibility and months already marked in the dataset. Each one
tests a different claim:

- **E1** is the main result: does it work on days it has not seen?
- **E2** asks whether it works at a place on the roof it has never seen.
- **E3** asks whether a model that has never seen a shadow still predicts one.
- **E4** asks whether the relation with cloudiness holds across the year.

The models do not change between experiments; only the split does. In every experiment
the scored rows come from days the model was not trained on (E4 scores training days of
the held-out month; the 15 test days are not touched there). Every model is trained and
scored in each experiment, always on the common mask. The count is 16 runs per model
(E1 once, E2 five times, E3 once, E4 nine times).

| ID | Train | Score on | What it shows |
|---|---|---|---|
| **E1** | the 43 training days, all sensors | the 15 test days, all sensors | overall performance |
| **E2** (sensor-out) | the 43 training days without one sensor, for each of **P1, P3, P5, P7, Pinc** | the 15 test days, that sensor only | generalisation to an unseen position. Pinc is an orientation extrapolation (the only tilted sensor), reported as such |
| **E3** (no shade seen) | training rows with `sun_visibility == 1` only | test rows with `sun_visibility < 0.5` | a model that never saw an occlusion still predicts shade |
| **E4** (month-out) | training days outside one month, for each of the 9 months | training days of that month | stability of the cloud response across the year. The test days are not used |

C cannot be scored on an unseen sensor in E2 (its dummy for that sensor is undefined);
it is reported as "not applicable" there. That is the point of E2.

## 4. Metrics and reporting

- R², MAE, RMSE and MBE as in the TFG, plus the error tail (P90, P95, P99 of
  `|error|`, and the maximum).
- Reported in every table: overall; **per sensor**; per sky class (sunny, mixed, cloudy,
  rainy); shaded against unshaded rows (`sun_visibility < 0.5` against `== 1`); and a
  **horizontals-only** row beside the pooled one (Pinc excluded).
- The 15 test days are drawn by sky class and are not climatological. The headline also
  gets an **annual-equivalent** figure: per-class metrics recombined with each class's
  frequency over the whole pyranometer year, classified by the same rule.
- Uncertainty: 95 % intervals by block bootstrap over days (2000 resamples). Rows within
  a day are not independent, and the effective sample is the number of days.
- Differences between models are reported as paired differences with the same bootstrap.

## 5. Sensitivity analyses (reported, never used to choose anything)

1. Altitude mask 10° instead of 5°.
2. XGBoost hyperparameters (H, D, C) re-tuned by day-blocked cross-validation inside the
   training days (a small random search of at most 30 configurations around the TFG's, the
   same budget for the three), to show whether the inherited parameters matter.
3. Cloudy and rainy classes pooled (their boundary is arbitrary).
4. Horizontals only (already in every table).

## 6. What was fixed relative to the TFG

Kept: model structure, features, target, `eps`, clipping, `sim ≥ 1` training rows,
training altitude cut, hyperparameters, no sensor dummies in the final model.

Fixed:
- One evaluation mask for all models (the TFG scored baselines and hybrid on different
  row sets, and kept night rows in the test set).
- The test days are fixed before any model is run, by sky class and shadow regime.
- A clean ablation (D, and P-cs/P-sim) alongside the TFG's baseline C.
- A dataset rebuilt from the corrected simulator, in UTC, with explicit QC flags.
- Per-sensor reporting, block-bootstrap intervals and paired differences.
- Generalisation tests (E2–E4) designed in advance.
