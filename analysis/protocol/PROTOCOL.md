# Evaluation protocol

**Status: DRAFT.** It becomes binding when tagged `protocol-v1`. After the tag nothing below
may change in response to a result; anything changed afterwards is reported as exploratory.

## Summary

This protocol evaluates a rooftop irradiance estimator built in two layers. The first is a
simulation in Unreal Engine: a 3D model of the ETSIDI rooftop and its surroundings that gives,
for every sensor and every 2 minutes, the irradiance it would receive under a clear sky,
including the shadows of nearby buildings and the sensor's orientation. It is calibrated
against a clear-sky model, not an independent physical estimate. The second layer is a small
learned correction that uses cloud cover to bring the simulated value closer to what the
pyranometers measure.

The evaluation asks two things. Is the simulation informative on its own: does it capture
shadows and orientation better than a clear-sky formula? And does the correction improve it,
and does the geometry still matter once clouds are accounted for?

- **Data:** 60 simulated days, nine pyranometers, 2-minute steps; 43 days to train and 15 to
  test, the test days fixed beforehand by sky type and shadow situation.
- **Models:** nine, from plain references to the full two-layer method **H**. The headline
  compares H with **D**, the same correction layer without the simulation (section 2).
- **Experiments:** four splits: unseen days (E1), an unseen sensor (E2), shade the model was
  not trained on (E3) and an unseen month (E4) (section 3).
- **Primary result:** the RMSE difference between D and H in E1, with an interval that treats
  each day as one observation (section 4).
- **Rules:** one evaluation mask for every model, hyperparameters chosen only on training
  days, test days never used for any choice.

## 1. Data and mask

- Dataset: `analysis/data/dataset_v2.csv` (see `pipeline/README.md`).
- Sensors: **P0, P1, P3, P4, P5, P6, P7, P8, Pinc** (P2 excluded: `KNOWN_ISSUES.md` M1).
- Day split: `analysis/data/split_v2.csv`, 43 training and 15 test days. Days are the unit of
  splitting; the rows of one day never straddle train and test.
- **Common evaluation mask**, applied once and identically to every model:
  `sun_altitude_deg > 5` and `qc_ok` and the sensor is in the list above. Training rows use
  the same mask. The mask is never applied per model.

## 2. Models

Nine models in three groups. Only the learned ones are trained; the references are just
scored. `base` below is `sim_irradiance_wm2` for H and `clearsky_ghi_wm2` for D; `k̂` is the
model's output; the details are in Appendix A.

| ID | Name | Learned? | Inputs | Prediction | Question it answers |
|---|---|---|---|---|---|
| A | Physical simulation | no | `sim_irradiance_wm2` | `max(sim, 0)` | What does the 3D engine give on its own? |
| B | Clear-sky | no | `clearsky_ghi_wm2` | `max(cs, 0)` | What does the clear-sky physics give, with no clouds and no geometry? |
| R | Analytic ray-cast shading | no | `clearsky_dni_wm2`, `clearsky_dhi_wm2`, `geometric_factor`, `sky_view_factor` | `max(dni·geometric_factor + dhi·svf, 0)` | Do the cubemaps add anything over standard shading and diffuse formulas? |
| S | Solcast GHI | no | `solcast_ghi_wm2` | `max(solcast_ghi_wm2, 0)` | Does the method beat an operational weather product? |
| P-cs | Parametric, clear-sky | 2 parameters | `clearsky_ghi_wm2`, `cloud_opacity` | `max(cs·(1 − a·c^b), 0)` | Clouds on top of clear-sky, no 3D, no trees |
| P-sim | Parametric, simulator | 2 parameters | `sim_irradiance_wm2`, `cloud_opacity` | `max(sim·(1 − a·c^b), 0)` | The same with 3D. Does the 3D gain survive a simple formula? |
| D | XGBoost, clear-sky | XGBoost | `clearsky_ghi_wm2`, `cloud_opacity` | `max(clip(k̂, 0, 2)·cs, 0)`, `k = clip(real/cs, 0, 2)` | **Clean ablation of H** (no 3D) |
| **H** | **XGBoost, hybrid (TFG final model)** | XGBoost | `sim_irradiance_wm2`, `cloud_opacity` | `max(clip(k̂, 0, 2)·sim, 0)`, `k = clip(real/sim, 0, 2)` | **The proposed method** |
| C | XGBoost meteo (TFG baseline C) | XGBoost | `cloud_opacity`, `zenith`, `azimuth`, `precipitable_water`, sensor dummies | target `real`; prediction `max(ŷ, 0)` | Can the data alone learn each sensor's shadow, with no 3D? |

How the comparisons read:

- **H against D** is the headline. D is the same correction layer without the simulation: they
  share the learner, the hyperparameters (Appendix B), the target form and the rows, and
  differ only in the geometry signal. The contrast therefore measures what the simulation
  adds to a model driven by cloud cover. **P-sim against P-cs** is the same comparison with a
  2-parameter formula, so the gain does not depend on the learner.
- **A against B** is the simulation on its own against the clear-sky formula. Neither knows
  about clouds, so the difference comes from the geometry and calibration alone. It is read
  overall and especially on shaded rows and sunny days.
- **A against R** is the cubemap-integrated ambient against the standard diffuse formula. A and
  R share the same direct term (clear-sky DNI times the incidence cosine times the sun
  visibility, see Appendix A) and differ only in the diffuse part: A integrates the rendered
  cubemaps, R uses `DHI × sky view factor`. It says what the render adds over analytic
  shading, whichever way it comes out. **R against B** says how much of the benefit of
  shading is already captured by the analytic formulas.
- **H against A** is what the correction layer adds to the simulation.
- **H against P-sim** says whether XGBoost adds anything over a 2-parameter formula.
- **H against S** says whether the method beats Solcast's own GHI estimate, an external
  product (horizontal, 5-minute, interpolated to the 2-minute grid). S is a horizontal
  estimate, so **this contrast is computed on the eight horizontal sensors only**; S is not
  scored on Pinc.
- **H against C** says whether the 3D adds anything over a model that can memorise each
  sensor's shadow. C has per-sensor dummies and sun angles, so it can in principle learn where
  shade falls; it is a rival, not an ablation. It cannot be scored on an unseen sensor, which
  is what E2 tests.

## 3. Experiments

The four experiments are four ways of splitting the data into what a model is trained on and
what it is tested on. They start from the same day split and use the sensors,
sun-visibility and months already marked in the dataset. The models do not change between
experiments; only the split does. The scored rows always come from days the model was not
trained on (E4 scores training days of the held-out month; the 15 test days are not touched
there). Every model is trained and scored in each experiment, always on the common mask.

| ID | Train | Score on | What it shows |
|---|---|---|---|
| **E1** | the 43 training days, all sensors | the 15 test days, all sensors | the main result: does it work on unseen days? |
| **E2** (sensor-out) | the 43 training days without one sensor, for **each of the nine sensors** | the 15 test days, that sensor only | generalisation to an unseen position. Summarised by groups fixed in advance: shaded (P1, P3, P5, P7), open sky (P0, P4, P6, P8) and Pinc. Pinc is an orientation extrapolation (the only tilted sensor) and is reported as such |
| **E3** (no simulated occlusion seen) | training rows with `sun_visibility == 1` only | test rows with `sun_visibility < 0.5` | a model trained without any simulated occlusion still predicts rows that have one. `sun_visibility == 1` means the simulator sees no occlusion, not that no real shadow exists (`KNOWN_ISSUES.md` S1), so this is not a claim about never having seen a shadow |
| **E4** (month-out) | training days outside one month, for each of the 9 months | training days of that month | stability of the cloud response across the observed months, April–December (not a full year). The test days are not used |

That is 20 runs per model (E1 once, E2 nine times, E3 once, E4 nine times), with two
exceptions: C is not scored in E2 (its dummy for an unseen sensor is undefined; that is the
point of E2), and S is not scored on Pinc.

**Declared limitation of E2, E3 and E4.** The hyperparameters were chosen using all the
sensors, rows and months of the training days. A model's weights never see the held-out
sensor, occlusion rows or month, but its hyperparameters did, so these experiments are
slightly optimistic about them. The selection is not repeated inside each experiment.

## 4. Metrics, reporting and the primary result

- R², MAE, RMSE and MBE as in the TFG (MBE = mean(prediction − observation), so a negative
  value is under-prediction), plus the error tail (P90, P95, P99 of `|error|`, and the maximum).
- Every table reports: overall; **per sensor**; per sky class (sunny, mixed, cloudy, rainy);
  shaded against unshaded rows (`sun_visibility < 0.5` against `== 1`); and a
  **horizontals-only** row beside the pooled one (Pinc excluded).
- The 15 test days are drawn by sky class and are not climatological, so no single average
  describes typical performance; results are always shown per sky class.
- **Uncertainty:** 95 % percentile intervals (2.5th–97.5th) by block bootstrap over days
  (2000 resamples, seed 42). Rows within a day are not independent, and the effective sample
  is the number of days. Differences between models are paired differences with the same
  bootstrap.
- **Row-level predictions are saved** for every model and experiment, with the sensor, time
  and fold of each scored row, so that later (exploratory) analyses need no retraining.
- **Overfitting diagnostic:** every table for a learned model also reports its RMSE on its
  own training rows, in cross-validation and on the scored rows; the gap between the three
  is the evidence for or against overfitting.

### Primary result (declared before any model is run)

- **Contrast:** H against D in E1.
- **Metric:** `RMSE(D) − RMSE(H)` in W/m², paired, over the common mask.
- **Population:** the nine sensors pooled; every row of the mask has equal weight.
- **Uncertainty:** 95 % percentile interval by block bootstrap over the 15 test days (2000
  resamples, seed 42).

The nine-sensor headline includes Pinc, and D (like every clear-sky-based model) estimates
horizontal irradiance, so the gain on Pinc reflects the geometry's handling of orientation as
well as of shadows. The horizontals-only contrast separates the two and is reported right next
to the headline.

Everything else is secondary: the same contrast on the horizontals only, per sensor, per sky
class, shaded against unshaded rows, the contrasts A − B, A − R, R − B, H − A, P-sim − P-cs, H − C, H − P-sim and H − S,
and E2–E4. Secondary results are reported in full and are not used to make the headline claim.

## 5. Sensitivity analyses (reported, never used to choose anything)

1. **Stricter altitude cut.** Repeat everything with `sun_altitude_deg > 10` instead of `> 5`.
   The hyperparameters chosen at 5° are reused, the models are retrained on the rows above
   10° and scored; results go in a second set of tables. Between 5° and 10° the simulator is
   least reliable (`KNOWN_ISSUES.md` S2), and the 5° cut is a choice, not a fact: if the
   conclusions hold at 10°, they do not depend on where the cut was put.
2. **The TFG's own configuration** for H, D and C: `ε = 10` with target and prediction using
   `base + ε`, `k` clipped to [0, 1.5], and the TFG's parameters (H and D: depth 4, 800 trees,
   `min_child_weight` 70, `colsample_bytree` 0.5; C: depth 5, 800 trees, `min_child_weight` 60,
   `colsample_bytree` 0.8). Do the conclusions depend on the choices made here?
3. **Cloudy and rainy classes pooled**, since their boundary is arbitrary.
4. **Training variants for H and D.** (a) Upper clip of `k` at 3.0 and with no upper clip,
   against 2.0 in the main runs: it does not bind equally in H and D (Appendix A). (b)
   Weighted training, with sample weight `base²` rescaled to mean 1 so that
   `min_child_weight` keeps its scale: for targets the clip does not touch, it makes the
   training loss equal to the squared error in W/m² that RMSE measures, whereas unweighted
   training gives every hour the same weight.

## 6. Differences from the TFG

Kept: model structure, features, the ratio target, the training altitude cut, no sensor
dummies in the final model.

Changed:
- **One evaluation mask for all models.** The TFG scored baselines and hybrid on different
  row sets and kept night rows in the test set.
- **Test days fixed before any model is run**, by sky class and shadow regime.
- **A consistent target and prediction.** The TFG trained on `real/(sim+ε)` with ε = 10 W/m²
  but predicted `k̂·sim`, so even a perfect `k̂` under-predicted by 7.2 W/m² on average (RMSE
  8.1), and the references A and B did not carry that error. The mismatch is removed by using
  the same denominator in the target and in the reconstruction. Dropping ε as well removes an
  unneeded constant (Appendix A).
- **A clip of `k` at 2 instead of 1.5**, and no `sim ≥ 1` training rule (redundant with the mask).
- **Hyperparameters chosen by blocked cross-validation inside the training days**, with a
  simplicity rule, before any test day is scored, and including `colsample_bytree`
  (Appendix B).
- **A clean ablation** (D, and P-cs/P-sim) alongside the TFG's baseline C.
- **A rebuilt dataset:** corrected simulator, UTC, explicit QC flags.
- **Per-sensor reporting, block-bootstrap intervals and paired differences**, and the
  generalisation tests E2–E4 designed in advance.

## 7. Freezing

Tagging `protocol-v1` fixes this document and the data it refers to. The tag message records
the sha256 of `analysis/data/dataset_v2.csv`, `analysis/data/split_v2.csv` and
`analysis/data/dataset_v2_manifest.json`, and the commit of `analysis/pipeline`, so that
regenerated data cannot change a result unnoticed.

The evaluation code is written after the tag. It must implement this document as written; a
deviation, or any analysis added after seeing a result, is reported as exploratory.

## Appendix A. Model details

- **Target of H and D.** The learned models predict a factor `k = real / base` and reconstruct
  `base · k̂`. There is no additive constant ε in the denominator. Inside the common mask `sim`
  never drops below 32.9 W/m², and the clear-sky GHI (the base of D) never below 21.8 W/m² on
  the training rows, so no constant is needed to keep `k` stable, and the transformation is
  exactly interpretable. Stability at low irradiance is checked with the 10° run (section 5).
- **Clip of `k` to [0, 2].** It only guards against absurd values. On the training rows it
  acts on 35 rows of H (0.026 %) but on 771 rows of D (0.58 %, 299 of them Pinc), because
  `k` relative to the clear-sky GHI is larger for a tilted plane and at low sun. The clip
  therefore does not bind equally in H and D, which sensitivity 4 examines.
- **P models.** `c = cloud_opacity/100`. `a` in [0, 1] and `b` in [0.1, 5] are fitted on the
  training rows by least squares on the residual `real − prediction` in W/m², with
  `scipy.optimize.least_squares` (method `trf`, those bounds, default tolerances), started
  at `a = 0.8, b = 1`. There is nothing to tune.
- **Model R.** `geometric_factor` is the plugin's incidence cosine times the sun visibility
  (`GeometricFactor = Dot * SunVisibility`), so it is already zero when the sun is hidden and
  the visibility must not be multiplied in again. `sky_view_factor` is computed per sensor pose
  and normal. `clearsky_dni_wm2` and `clearsky_dhi_wm2` are the same Ineichen–Pérez values as B.
  R uses the same analytic direct term as the simulator (`DNI × GeometricFactor`) and replaces
  the cubemap-integrated ambient by `DHI × sky view factor`. R is scored on all nine sensors,
  with no learning.
- **Inputs of C.** `zenith` and `azimuth` come from the simulator's sun position
  (`90 - sun_altitude_deg`, `sun_azimuth_deg`); `precipitable_water` comes from Solcast.

## Appendix B. Hyperparameter selection (H, D, C)

Chosen once, inside the training days, and then frozen for every experiment. Nothing is tuned
on a test day.

- **Grid, complete (no random draws).** For H and D (two inputs): `max_depth` {2, 3, 4, 5} ×
  `n_estimators` {100, 300, 800} × `min_child_weight` {20, 70, 200} × `colsample_bytree`
  {0.5, 1.0} = 72 configurations. For C (many inputs) `colsample_bytree` stays at the TFG's
  0.8: 36 configurations. Every other parameter keeps the TFG's value for that model:
  - H and D: `learning_rate=0.01, subsample=0.5, reg_alpha=0.1, reg_lambda=1.0, gamma=0.1,
    random_state=42`.
  - C: `learning_rate=0.01, subsample=0.8, reg_alpha=0.1, reg_lambda=4.0, gamma=0.0,
    random_state=42`.
- **Why `colsample_bytree` is in the grid.** With two inputs, 0.5 gives every tree one of
  them, which makes the predictor of `k` additive, `f(sim) + g(cloud)`; the final prediction
  `sim·[f(sim) + g(cloud)]` keeps a multiplicative cloud effect, but `g` cannot change with
  `sim`. That can act as a regulariser or as a limitation; cross-validation inside the
  training days decides, not an assumption.
- **Selection for H and D, jointly.** H and D must differ only in the geometry signal, so they
  share **one** configuration. Leave-one-month-out folds over the 43 training days (blocked
  cross-validation, because the days of a month share weather). For each configuration both H
  and D are validated on every fold; the configuration's score on a fold is the mean of the
  two RMSEs on the common mask, and its score overall is the mean over the folds.
  **One-standard-error rule** on that joint score: among the configurations whose mean is
  within one standard error (across folds) of the best, choose the simplest, defined as an
  order: fewest trees, then smallest depth, then largest `min_child_weight`, then
  `colsample_bytree` 0.5 before 1.0. The chosen configuration is used for both models.
- **C is selected on its own**, with the same procedure on its own 36-configuration grid.
- The TFG's own configuration (section 5) is run as a reference variant, shared by H and D.
