# Known issues and limitations

What is known to be imperfect in the data, the simulator and the evaluation design,
with the evidence and how the analysis deals with each point. Numbers come from
`dataset_v2` (60 campaign days, 10 simulated sensors, 2-minute grid, UTC); the QC
flags are defined in [`pipeline/README.md`](pipeline/README.md).

Nothing here is silently corrected. Data problems are marked by `qc_*` columns and
masked once, identically for every model; simulator problems are documented and
reported, because they bound what the method can claim.

## 1. Measurements (pyranometers)

| # | Issue | Evidence | Treatment |
|---|---|---|---|
| M1 | **P2 is dead from 2025-07-06** and unreliable before | Max < 20 W/m² on 38 of 60 days. Even on "valid" days P2 reads 0.50 of its peers on 04-11, 0.66 on 05-02, 0.89 on 06-22; only 22 usable sensor-days, all Apr–1 Jul; 6 of them fall in the test set | **Excluded from all analysis** (train and test). Kept in the dataset, flagged `qc_sensor_dead` |
| M2 | **P1 dead on 4 days** (07-06, 07-12, 07-16, 07-21) | Same signature as M1; P1 is fine on its other 54 days | Those days masked; P1 stays (it is the seasonal-shadow sensor) |
| M3 | **Truncated real data** | 2025-04-04 (from 11:58 local), 2025-06-13 (from 16:28, 29 % coverage); 2025-04-03 starts 10:08 (85 %) | 04-04 and 06-13 unusable for every sensor; 04-03 kept, training only |
| M4 | **Logger blackout on 2025-07-21** | All working sensors read exactly 0.0 with 24/24 samples for ~4 h in full sun (1211 rows) | `qc_zero_daylight` (needs every working sensor at zero at the same instant) |
| M5 | **Single-channel dropouts** | 58 rows (07-31: P4, P6, P7, P8; 09-26: P7, P8) read exactly 0 while the other sensors read 296–650 W/m² | `qc_channel_dropout` (lone zero with peers ≥ 100 W/m²) |
| M6 | **Duplicated timestamps** | 4 files repeat rows (04-30, 05-01, 06-13, 11-19); ~180 sensor-bins disagree by > 5 W/m² | Duplicates averaged; disagreements flagged `qc_dup_conflict` |
| M7 | **Inter-sensor calibration** is only good to ~2 % | On 45 clear days the horizontal sensors agree within ±2 %; over the year P4 drifts −3.5 %, P3 +2 % | Not corrected; part of the irreducible error |
| M8 | **Storm days** read a few W/m² | 2025-06-17 and 06-30: all sensors read a few up to ~40 W/m². Some read exactly 0.0 (quantisation) while others read 1–2 | Kept — they are real. Solcast saw both storms (cloud 73–95 %) |

Real timestamps are Madrid wall-clock time and are converted to UTC on load. Clear-day
alignment against the simulation is exact to the grid: median shift 0 min, IQR 0 to 2 min
over 26 clear days.

## 2. Simulator

| # | Issue | Evidence | Treatment |
|---|---|---|---|
| S1 | **A shadow measured at some sensors is not produced by the simulator** (cause not confirmed) | P5 (also P3, P1, P7) reads 0.3–0.7× its peers, at the same instant, in the evening with the sun in the west (azimuth 257–290°, altitude 10–16°), while the simulator says the sun is fully visible. The comparison is sensor against sensor, no model involved. It follows the sun position, not the date (April to September), which points to a fixed obstacle; 123 of the 293 outlier rows are P5 | Not masked (the data look good). The likeliest explanation is an obstacle missing from the 3D model, but it has not been checked on site or against the plans, and a cause specific to P5's mounting is not excluded |
| S2 | **Shade depth is unreliable at low sun** (below ~9°) | P0 evening, 5–7°: simulated drop −27 %, measured −16 %; 7–9°: −31 % vs −4 %. P4 on 2025-10-07 at 17:12 UTC: simulated P4/P0 0.71, measured 0.31. Unshaded mornings below 7°: the simulator overestimates by 20–30 % | Main mask: sun altitude > 5°. Altitude > 10° as a sensitivity analysis. A low-sun stratum (2–10°) reported separately in absolute W/m² |
| S3 | **An occluder 3.6–5.5 m from P0 (April–September)** produces shade at sun < 9° | `sun_hit_distance_m`: 3.6–5.5 m April–Sep, ~28 m in October, ~170 m November–December. Present in the TFG data too | Whether it exists on the real roof is **not verified**; the measured P0 does dip, by less than simulated (S2) |
| S4 | **Penumbra** | The TFG used 8 sun-visibility samples, this campaign 32 (33 levels confirmed in the data) | None needed. Against the TFG, occlusion agrees on 99.6–99.9 % of 5972 matched rows, differences only at penumbra edges |
| S5 | **`sim_irradiance_wm2` is calibrated, not an independent physical estimate** | Fitted to a clear-sky model (bug tracker S4); the direct term is analytic | The claim is a *geometry-aware calibrated signal*, not first-principles irradiance |
| S6 | `_lux` column suffixes are a misnomer | Bug tracker S9/A6 | Names kept for traceability; documented in `pipeline/README.md` |

## 3. Orientation and cloud model

| # | Issue | Evidence | Treatment |
|---|---|---|---|
| O1 | **A single `f(cloud)` curve is not orientation-invariant** | Pinc (tilted 30°) vs the horizontals, real/sim ratio relative to them: 0.98 in clear sky, 0.86 at 25–50 % cloud, 0.67 at 50–75 %. Overcast: Pinc reads 17 % of the simulated value, horizontals 27 %. Physical: a tilted plane has a different direct/diffuse mix, and the simulator's cloud attenuation is the same for every sensor | **Pinc stays in the pooled model**, as in the TFG, and its results are expected to be worse. Always reported per sensor, with a horizontals-only row beside the pooled one. A sensor-out test on Pinc is an orientation extrapolation, not a generalisation claim |

An untested extension: separate cloud curves for the simulator's direct and diffuse
components (`f_direct·direct + f_diffuse·diffuse`) would let orientation enter through
the components with no tilt variable. A tilt feature is not an option: with one tilted
sensor it would just identify that sensor.

## 4. Meteorological input (Solcast)

| # | Issue | Evidence | Treatment |
|---|---|---|---|
| W1 | **5-minute source, 2-minute grid** | Linear interpolation of `cloud_opacity` | Each row is anchored to `period_end − 2.5 min` (the period is assumed to be the mean over the preceding five minutes). Timing is consistent with the measurements: on 06-30 Solcast cloud rises from 20 % to 73 % between 14:15 and 14:30 UTC and P0 falls from 936 W/m² at 14:08 to 43 at 14:48 |
| W2 | **Solcast's own GHI is a different estimate** | During the 06-30 storm real ≈ 23 W/m², Solcast ≈ 46 W/m² | Kept (`solcast_ghi_wm2`) as a baseline, not as an input |
| W3 | Winter under-represented | Solcast covers 2025-01-01 to 12-23; the pyranometers start in April, so the working set is April–December | Stated as a scope limit |

## 5. Evaluation design

| # | Issue | Treatment |
|---|---|---|
| E1 | **The test set does not represent the annual climate.** 15 days, chosen by sky class (sunny 3, mixed 4, cloudy 4, rainy 4) and P1-shadow regime; 8 of 15 have P1 shaded against 35 % in training. Cloudy and rainy days are over-represented | Report per sky class; recombine with each class's real annual frequency to estimate an annual-equivalent figure. A more sunny climate favours the simulator in relative terms but not in W/m² (absolute errors grow with irradiance), so "worse than the annual scenario" is not claimed without that calculation |
| E2 | **Class boundaries are arbitrary.** Test days 05-05 (k = 0.40), 11-24 (0.38) and 12-12 (0.39) sit within 0.02 of the cloudy/rainy boundary | Report cloudy + rainy combined as a check |
| E3 | **Few cloudy training days.** Cloudy has only 6 | Stated; wide intervals expected for that class |
| E4 | **One roof, one year, 58 days.** Sensors share weather; days are autocorrelated | No geographic or climatic generalisation is claimed. Leave-one-sensor-out measures spatial generalisation within one roof. Intervals by block bootstrap over days |
| E5 | **P1's test sample is small:** 193 shaded bins in the test set | Wide intervals for P1 shade metrics |

## 6. Open questions (not verified)

- Whether the occluder near P0 (S3) exists on the real roof, and what causes the measured evening shadow of S1.
  A site photo or the Civil 3D plans would settle both.
- Possible double atmospheric-refraction correction between `BP_SunSky` and the
  apparent-altitude fix (bug tracker S14); judged unlikely.
- Whether the direct/diffuse extension of section 3 recovers Pinc.
