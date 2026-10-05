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

| ID | Name | Inputs | Target / prediction | Notes |
|---|---|---|---|---|
| A | Physical simulation | `sim_irradiance_wm2` | prediction = `max(sim, 0)` | no learning |
| B | Clear-sky | `clearsky_ghi_wm2` | prediction = `max(cs, 0)` | no learning |
| C | XGBoost meteo (TFG baseline) | `cloud_opacity`, `zenith`, `azimuth`, `precipitable_water`, sensor dummies | target = `real` | TFG parameters of baseline C |
| D | XGBoost clear-sky (clean ablation) | `clearsky_ghi_wm2`, `cloud_opacity` | `k = real/(cs+10)` | **identical to H except the geometry signal** |
| **H** | **Hybrid (TFG final model)** | `sim_irradiance_wm2`, `cloud_opacity` | `k = clip(real/(sim+10), 0, 1.5)`; prediction = `max(clip(k̂)·sim, 0)` | training rows need `sim ≥ 1` |

- H and D share the same learner, parameters, target form and rows. The only difference
  is `sim_irradiance_wm2` against `clearsky_ghi_wm2`. That difference is the measured
  contribution of the 3D geometry.
- C is kept as the TFG's own reference baseline. It carries per-sensor dummies, sun
  angles and water vapour, so it can in principle memorise each sensor's shadow
  pattern; it is the rival the geometry has to beat. It is *not* an ablation of H.
- Hyperparameters, inherited unchanged from the TFG and **fixed**:
  - H and D: `n_estimators=800, learning_rate=0.01, max_depth=4, min_child_weight=70,
    subsample=0.5, colsample_bytree=0.5, reg_alpha=0.1, reg_lambda=1.0, gamma=0.1,
    random_state=42`.
  - C: `n_estimators=800, learning_rate=0.01, max_depth=5, min_child_weight=60,
    subsample=0.8, colsample_bytree=0.8, reg_alpha=0.1, reg_lambda=4.0, gamma=0.0,
    random_state=42`.
- `zenith` and `azimuth` for C come from the simulator's sun position (`90 - sun_altitude_deg`,
  `sun_azimuth_deg`). `precipitable_water` comes from Solcast and is added to the dataset.

## 3. Experiments

Every model is trained and scored in each experiment, always on the common mask.

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
2. Hyperparameters re-tuned by day-blocked cross-validation inside the training days
   (a small random search of at most 30 configurations around the TFG's, the same
   budget for H, D and C), to show whether the inherited parameters matter.
3. Cloudy and rainy classes pooled (their boundary is arbitrary).
4. Horizontals only (already in every table).

## 6. What was fixed relative to the TFG

Kept: model structure, features, target, `eps`, clipping, `sim ≥ 1` training rows,
training altitude cut, hyperparameters, no sensor dummies in the final model.

Fixed:
- One evaluation mask for all models (the TFG scored baselines and hybrid on different
  row sets, and kept night rows in the test set).
- The test days are fixed before any model is run, by sky class and shadow regime.
- A clean ablation (D) alongside the TFG's baseline C.
- A dataset rebuilt from the corrected simulator, in UTC, with explicit QC flags.
- Per-sensor reporting, block-bootstrap intervals and paired differences.
- Generalisation tests (E2–E4) designed in advance.
