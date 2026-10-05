# Evaluation protocol

**Status: DRAFT.** It becomes binding when tagged `protocol-v1`. After the tag, nothing
below may change in response to a result; anything changed afterwards is reported as
exploratory.

The aim is the TFG's method, evaluated properly. Model structure, features and target are
the TFG's. What changes is how the hyperparameters are chosen (section 2) and how the models
are compared.

## 1. Data

- Dataset: `analysis/data/dataset_v2.csv` (see `pipeline/README.md`).
- Sensors: **P0, P1, P3, P4, P5, P6, P7, P8, Pinc** (P2 excluded: `KNOWN_ISSUES.md` M1).
- Day split: `analysis/data/split_v2.csv`: 43 training days, 15 test days. Days are the
  unit of splitting; rows of one day never straddle train and test.
- **Common evaluation mask**, applied once and identically to every model:
  `sun_altitude_deg > 5` and `qc_ok` and the sensor is in the list above.
  Training rows use the same mask. The mask is never applied per model.
- **Robustness check:** the whole evaluation is run a second time with a stricter altitude
  cut, `sun_altitude_deg > 10` instead of `> 5`, and its results go in a second set of tables
  next to the main ones. Nothing else changes (same split, same models). Between 5° and 10°
  the simulator is least reliable (`KNOWN_ISSUES.md` S2), and the 5° cut is a choice, not a
  fact. If the conclusions are the same at 10°, they do not depend on where the cut was put;
  if they differ, that is reported. The 5° result is always the main one, and the 10° result
  is never used to choose anything.

## 2. Models

Eight models, in three groups. Only the learned ones are trained; the references are just scored.

| ID | Name | Learned? | Inputs | Prediction | Question it answers |
|---|---|---|---|---|---|
| A | Physical simulation | no | `sim_irradiance_wm2` | `max(sim, 0)` | What does the 3D engine give on its own? |
| B | Clear-sky | no | `clearsky_ghi_wm2` | `max(cs, 0)` | What does the clear-sky physics give, with no clouds and no geometry? |
| S | Solcast GHI | no | `solcast_ghi_wm2` | `max(solcast_ghi_wm2, 0)` | Does the method beat an operational weather product? |
| P-cs | Parametric, clear-sky | 2 parameters | `clearsky_ghi_wm2`, `cloud_opacity` | `max(cs·(1 − a·c^b), 0)` | Clouds on top of clear-sky, no 3D, no trees |
| P-sim | Parametric, simulator | 2 parameters | `sim_irradiance_wm2`, `cloud_opacity` | `max(sim·(1 − a·c^b), 0)` | The same with 3D. Does the 3D gain survive a simple formula? |
| D | XGBoost, clear-sky | XGBoost | `clearsky_ghi_wm2`, `cloud_opacity` | `max(clip(k̂, 0, 1.5)·(cs+ε), 0)`, `k = clip(real/(cs+ε), 0, 1.5)` | **Clean ablation of H** (no 3D) |
| **H** | **XGBoost, hybrid (TFG final model)** | XGBoost | `sim_irradiance_wm2`, `cloud_opacity` | `max(clip(k̂, 0, 1.5)·(sim+ε), 0)`, `k = clip(real/(sim+ε), 0, 1.5)` | **The proposed method** |
| C | XGBoost meteo (TFG baseline C) | XGBoost | `cloud_opacity`, `zenith`, `azimuth`, `precipitable_water`, sensor dummies | `real` directly | Can the data alone learn each sensor's shadow, with no 3D? |

How the pairs read:

- **H against D** is the headline: the only difference is the geometry signal
  (`sim_irradiance_wm2` against `clearsky_ghi_wm2`); learner, parameters, target form and
  rows are the same. **P-sim against P-cs** is the same comparison with a 2-parameter
  formula, so the 3D gain does not depend on the learner.
- **H against P-sim** says whether XGBoost adds anything over a 2-parameter formula.
- **H against S** says whether the method beats Solcast's own GHI estimate, an external
  operational product (horizontal, 5-minute, interpolated to the 2-minute grid). Because S
  is a horizontal estimate, **this contrast is computed on the eight horizontal sensors
  only**; S is not scored on Pinc.
- **H against C** says whether the 3D adds anything over a model that can memorise each
  sensor's shadow. C carries per-sensor dummies and sun angles, so it can in principle learn
  where shade falls; it is a rival, not an ablation. C cannot be scored on an unseen sensor,
  which is what E2 tests.

Details:

- `ε = 10 W/m²`, as in the TFG. The target and the prediction use the **same** denominator
  `base + ε`, so they are inverses of each other. (The TFG trained on `real/(sim+ε)` but
  predicted `k̂·sim`; even a perfect `k̂` then under-predicted by 7.2 W/m² on average, RMSE
  8.1, and the references A and B did not carry that error.)
- P models: `c = cloud_opacity/100`. `a` in [0, 1] and `b` in [0.1, 5] are fitted by least
  squares (RMSE) on the training rows, started at `a = 0.8, b = 1`. Rows need `base ≥ 1`
  as for H. There is nothing to tune.
- **Hyperparameters (H, D, C) are chosen once, inside the training days, and then frozen**
  for every experiment. Nothing is tuned on a test day.
  - **Grid (36 configurations, complete, no random draws):** `max_depth` {2, 3, 4, 5} ×
    `n_estimators` {100, 300, 800} × `min_child_weight` {20, 70, 200}. Every other parameter
    keeps the TFG's value for that model:
    - H and D: `learning_rate=0.01, subsample=0.5, colsample_bytree=0.5, reg_alpha=0.1,
      reg_lambda=1.0, gamma=0.1, random_state=42`.
    - C: `learning_rate=0.01, subsample=0.8, colsample_bytree=0.8, reg_alpha=0.1,
      reg_lambda=4.0, gamma=0.0, random_state=42`.
  - **Selection:** leave-one-month-out folds over the 43 training days (blocked
    cross-validation, because the days of a month share weather); score = mean over the folds
    of the pooled RMSE on the common mask. **One-standard-error rule:** among the
    configurations whose mean RMSE is within one standard error (across folds) of the best,
    choose the simplest: fewest trees, then smallest depth, then largest `min_child_weight`.
  - Same grid and same procedure for H, D and C, so the comparison is fair.
  - The TFG's own parameters (H and D: depth 4, 800 trees, `min_child_weight` 70; C: depth 5,
    800 trees, `min_child_weight` 60) are run as a reference variant (section 5).
  - **Overfitting diagnostic:** every table for a learned model also reports its RMSE on its
    own training rows, in cross-validation and on the scored rows. The gap between the three
    is the evidence for or against overfitting.
  - E4 holds one month out, but that month was already seen when the hyperparameters were
    chosen, so E4 is slightly optimistic about the hyperparameters. This is stated with E4.
- `zenith` and `azimuth` for C come from the simulator's sun position (`90 - sun_altitude_deg`,
  `sun_azimuth_deg`). `precipitable_water` comes from Solcast.

## 3. Experiments

The four experiments are four ways of splitting the data into "what a model is trained on"
and "what it is tested on". They all start from the day split (43 training days, 15 test
days) and use the sensors, sun-visibility and months already marked in the dataset. Each one
tests a different claim:

- **E1** is the main result: does it work on days it has not seen?
- **E2** asks whether it works at a place on the roof it has never seen.
- **E3** asks whether a model trained with no simulated occlusion still predicts rows where the simulator shows one.
- **E4** asks whether the relation with cloudiness holds across the observed months (April–December).

The models do not change between experiments; only the split does. In every experiment
the scored rows come from days the model was not trained on (E4 scores training days of
the held-out month; the 15 test days are not touched there). Every model is trained and
scored in each experiment, always on the common mask. The count is 20 runs per model
(E1 once, E2 nine times, E3 once, E4 nine times).

| ID | Train | Score on | What it shows |
|---|---|---|---|
| **E1** | the 43 training days, all sensors | the 15 test days, all sensors | overall performance |
| **E2** (sensor-out) | the 43 training days without one sensor, for **each of the nine sensors** | the 15 test days, that sensor only | generalisation to an unseen position. Summarised by group, fixed in advance: shaded (P1, P3, P5, P7), open sky (P0, P4, P6, P8) and Pinc. Pinc is an orientation extrapolation (the only tilted sensor), reported as such |
| **E3** (no simulated occlusion seen) | training rows with `sun_visibility == 1` only | test rows with `sun_visibility < 0.5` | a model trained without any simulated occlusion still predicts rows that have one. `sun_visibility == 1` means the simulator sees no occlusion, not that no real shadow exists (`KNOWN_ISSUES.md` S1), so this is not a claim about "never having seen a shadow" |
| **E4** (month-out) | training days outside one month, for each of the 9 months | training days of that month | stability of the cloud response across the observed months, April–December (not a full year). The test days are not used |

C cannot be scored on an unseen sensor in E2 (its dummy for that sensor is undefined);
it is reported as "not applicable" there. That is the point of E2.

## 4. Metrics and reporting

- R², MAE, RMSE and MBE as in the TFG, plus the error tail (P90, P95, P99 of
  `|error|`, and the maximum).
- Reported in every table: overall; **per sensor**; per sky class (sunny, mixed, cloudy,
  rainy); shaded against unshaded rows (`sun_visibility < 0.5` against `== 1`); and a
  **horizontals-only** row beside the pooled one (Pinc excluded).
- The 15 test days are drawn by sky class and are not climatological, so no single
  average describes "typical" performance; results are always shown per sky class.
- Uncertainty: 95 % percentile intervals (2.5th–97.5th) by block bootstrap over days (2000
  resamples, seed 42). Rows within a day are not independent, and the effective sample is
  the number of days.
- Differences between models are reported as paired differences with the same bootstrap.

### Primary result (declared before any model is run)

- **Contrast:** H against D in E1.
- **Metric:** the difference in RMSE, `RMSE(D) − RMSE(H)` in W/m², paired, over the common mask.
- **Population:** the nine sensors pooled; every row of the mask has equal weight.
- **Uncertainty:** 95 % percentile interval (2.5th–97.5th) by block bootstrap over the 15
  test days (2000 resamples, seed 42).

The nine-sensor headline includes Pinc, and D (like every clear-sky-based model) estimates
horizontal irradiance, so the gain on Pinc reflects the geometry's handling of orientation as
well as of shadows. The horizontals-only contrast separates the two and is reported right
next to the headline.

Everything else is secondary: the same contrast on the horizontals only, per sensor, per sky
class, shaded against unshaded rows, the contrasts P-sim − P-cs, H − C, H − P-sim and H − S, and
E2–E4. Secondary results are reported in full but are not used to make the headline claim.

## 5. Sensitivity analyses (reported, never used to choose anything)

1. **Stricter altitude cut.** Repeat everything with `sun_altitude_deg > 10` instead of
   `> 5`, for the training rows, the test rows and every model. Only the cut changes. It
   answers: do the conclusions hold when the low-sun hours, where the simulator is least
   reliable, are left out?
2. The TFG's own XGBoost parameters for H, D and C (listed in section 2) instead of the
   selected ones: do the conclusions depend on the hyperparameter choice?
3. Cloudy and rainy classes pooled (their boundary is arbitrary).
4. `ε` in {1, 10, 50} and the upper clip bound in {1.2, 1.5, 2.0}, for H and D: does the
   H-against-D conclusion move?
5. Horizontals only (already in every table).

## 6. What was fixed relative to the TFG

Kept: model structure, features, target, `eps`, clipping, `sim ≥ 1` training rows,
training altitude cut, no sensor dummies in the final model.

Fixed:
- Hyperparameters chosen by blocked cross-validation inside the training days, with a
  simplicity rule, before any test day is scored.
- A consistent target/prediction pair for H and D (see section 2).
- One evaluation mask for all models (the TFG scored baselines and hybrid on different
  row sets, and kept night rows in the test set).
- The test days are fixed before any model is run, by sky class and shadow regime.
- A clean ablation (D, and P-cs/P-sim) alongside the TFG's baseline C.
- A dataset rebuilt from the corrected simulator, in UTC, with explicit QC flags.
- Per-sensor reporting, block-bootstrap intervals and paired differences.
- Generalisation tests (E2–E4) designed in advance.

## 7. Freezing

Tagging `protocol-v1` fixes this document and the data it refers to. The tag message
records the sha256 of `analysis/data/dataset_v2.csv`, `analysis/data/split_v2.csv` and
`analysis/data/dataset_v2_manifest.json`, and the commit of `analysis/pipeline`, so that
regenerated data cannot change a result unnoticed.

The evaluation code is written after the tag. It must implement this document as written; a
deviation, or any analysis added after seeing a result, is reported as exploratory.
