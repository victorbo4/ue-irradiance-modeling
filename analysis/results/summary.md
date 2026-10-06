# Evaluation summary

Protocol tag `protocol-v1`, verified against it: **True**. Synthetic smoke run: **False**. Code commit `25c893dd7f78`.

All results are exploratory. A contrast is the gain of the first model over the second, `RMSE(second) - RMSE(first)`, positive when the first is better. Intervals are 95 % percentile intervals of a block bootstrap over days (2000 resamples, seed 42) and are not adjusted for the number of comparisons.

## Main comparison: H against D in E1

- All nine sensors pooled: +0.28 W/m² (95 % interval -2.62 to +4.38; 45342 rows, 15 days)
- Horizontal sensors only (Pinc excluded): -1.05 W/m² (95 % interval -2.04 to +0.13; 40304 rows, 15 days)

The nine-sensor figure includes Pinc, so it reflects the handling of orientation as well as of shadows; the horizontals-only figure separates the two.

## E1 on the test days: all models

RMSE in W/m² with its interval; S is not scored on Pinc, so it is not applicable in the pooled table.

### All nine sensors

| model | RMSE | MAE | MBE | R² |
|---|---|---|---|---|
| A Simulation | 239.2 [166.4, 305.0] | 161.6 | 137.5 | 0.334 |
| B Clear-sky | 232.0 [158.2, 298.5] | 158.5 | 130.0 | 0.374 |
| R Ray-cast shading | 238.6 [165.8, 304.5] | 160.3 | 134.1 | 0.338 |
| P-cs Formula, clear-sky | 103.3 [73.5, 128.9] | 59.7 | -13.3 | 0.876 |
| P-sim Formula, simulation | 101.2 [70.1, 128.1] | 58.6 | -11.4 | 0.881 |
| D XGBoost, clear-sky | 107.7 [78.0, 133.5] | 67.0 | -0.4 | 0.865 |
| H XGBoost, simulation (proposed) | 107.5 [76.5, 134.1] | 68.4 | -0.5 | 0.866 |
| C XGBoost, meteo + sensor | 108.9 [79.7, 133.7] | 69.9 | -4.0 | 0.862 |
| S-geo Solcast DNI/DHI + local geometry (added after protocol-v1) | 98.0 [67.2, 123.3] | 57.1 | 7.8 | 0.888 |

### Horizontal sensors only

| model | RMSE | MAE | MBE | R² |
|---|---|---|---|---|
| A Simulation | 227.5 [152.9, 293.8] | 154.0 | 130.0 | 0.384 |
| B Clear-sky | 229.6 [154.2, 296.9] | 155.0 | 131.3 | 0.373 |
| S Solcast GHI | 97.0 [67.2, 121.8] | 58.0 | 10.5 | 0.888 |
| R Ray-cast shading | 228.7 [153.3, 295.9] | 153.7 | 128.5 | 0.378 |
| P-cs Formula, clear-sky | 98.6 [67.0, 125.8] | 56.8 | -11.9 | 0.884 |
| P-sim Formula, simulation | 99.1 [67.2, 126.7] | 56.7 | -14.6 | 0.883 |
| D XGBoost, clear-sky | 102.8 [70.9, 129.8] | 64.0 | 1.0 | 0.874 |
| H XGBoost, simulation (proposed) | 103.8 [71.5, 131.3] | 65.4 | -4.5 | 0.872 |
| C XGBoost, meteo + sensor | 104.8 [75.0, 130.5] | 67.3 | -2.8 | 0.869 |
| S-geo Solcast DNI/DHI + local geometry (added after protocol-v1) | 96.6 [66.3, 121.7] | 56.9 | 8.0 | 0.889 |

## E1 contrasts

| contrast | all nine sensors | horizontals only |
|---|---|---|
| A over B | -7.26 W/m² (95 % interval -13.51 to -2.25; 45342 rows, 15 days) | +2.15 W/m² (95 % interval +1.36 to +2.79; 40304 rows, 15 days) |
| A over R | -0.61 W/m² (95 % interval -1.70 to +0.11; 45342 rows, 15 days) | +1.23 W/m² (95 % interval +0.40 to +1.82; 40304 rows, 15 days) |
| R over B | -6.64 W/m² (95 % interval -11.97 to -2.14; 45342 rows, 15 days) | +0.91 W/m² (95 % interval +0.75 to +1.11; 40304 rows, 15 days) |
| H over A | +131.76 W/m² (95 % interval +71.44 to +187.00; 45342 rows, 15 days) | +123.62 W/m² (95 % interval +62.51 to +179.72; 40304 rows, 15 days) |
| P-sim over P-cs | +2.11 W/m² (95 % interval -0.71 to +6.68; 45342 rows, 15 days) | -0.46 W/m² (95 % interval -1.11 to +0.33; 40304 rows, 15 days) |
| H over C | +1.45 W/m² (95 % interval -4.11 to +6.83; 45342 rows, 15 days) | +0.99 W/m² (95 % interval -2.74 to +4.89; 40304 rows, 15 days) |
| H over P-sim | -6.24 W/m² (95 % interval -11.27 to -1.50; 45342 rows, 15 days) | -4.77 W/m² (95 % interval -9.69 to -0.22; 40304 rows, 15 days) |
| H over S | not applicable | -6.84 W/m² (95 % interval -11.53 to -1.86; 40304 rows, 15 days) |

## S-geo (added after protocol-v1)

Solcast's DNI and DHI with the plugin's geometry (beam times `geometric_factor`, diffuse times the sky view factor); nothing is learned. It was defined after the protocol-v1 results had been seen: see `analysis/protocol/ADDENDUM-1.md`.

| S-geo over | E1, nine sensors | E1, horizontals | E4 pooled, nine sensors | E4 pooled, horizontals |
|---|---|---|---|---|
| S | not applicable | +0.39 W/m² (95 % interval -0.07 to +1.32; 40304 rows, 15 days) | not applicable | +0.93 W/m² (95 % interval +0.59 to +1.43; 117896 rows, 43 days) |
| R | +140.58 W/m² (95 % interval +77.22 to +199.05; 45342 rows, 15 days) | +132.09 W/m² (95 % interval +68.41 to +190.39; 40304 rows, 15 days) | +138.89 W/m² (95 % interval +102.31 to +174.29; 132826 rows, 43 days) | +133.59 W/m² (95 % interval +97.50 to +168.46; 117896 rows, 43 days) |
| A | +141.19 W/m² (95 % interval +78.79 to +199.18; 45342 rows, 15 days) | +130.86 W/m² (95 % interval +67.58 to +188.63; 40304 rows, 15 days) | +139.07 W/m² (95 % interval +102.97 to +174.33; 132826 rows, 43 days) | +132.32 W/m² (95 % interval +96.40 to +167.01; 117896 rows, 43 days) |
| P-sim | +3.19 W/m² (95 % interval +0.57 to +5.81; 45342 rows, 15 days) | +2.47 W/m² (95 % interval -1.07 to +5.73; 40304 rows, 15 days) | +0.10 W/m² (95 % interval -1.68 to +1.62; 132826 rows, 43 days) | -0.64 W/m² (95 % interval -2.57 to +0.94; 117896 rows, 43 days) |
| H | +9.43 W/m² (95 % interval +5.24 to +14.14; 45342 rows, 15 days) | +7.23 W/m² (95 % interval +2.48 to +11.82; 40304 rows, 15 days) | +7.48 W/m² (95 % interval +3.62 to +12.36; 132826 rows, 43 days) | +6.00 W/m² (95 % interval +2.38 to +10.75; 117896 rows, 43 days) |
| D | +9.71 W/m² (95 % interval +4.08 to +16.31; 45342 rows, 15 days) | +6.18 W/m² (95 % interval +1.73 to +10.54; 40304 rows, 15 days) | +10.27 W/m² (95 % interval +6.12 to +15.60; 132826 rows, 43 days) | +5.49 W/m² (95 % interval +2.06 to +9.82; 117896 rows, 43 days) |

## E1 by sky class and shade (RMSE)

| group | A | B | R | P-cs | P-sim | D | H | C | S-geo |
|---|---|---|---|---|---|---|---|---|---|
| sky:sunny | 40.6 | 58.7 | 41.8 | 60.2 | 42.7 | 65.1 | 50.7 | 64.7 | 40.9 |
| sky:mixed | 166.6 | 165.9 | 166.1 | 129.3 | 127.1 | 129.4 | 128.3 | 135.7 | 120.6 |
| sky:cloudy | 323.2 | 314.4 | 322.8 | 113.4 | 114.1 | 120.8 | 123.5 | 120.8 | 112.6 |
| sky:rainy | 312.1 | 295.2 | 310.7 | 85.3 | 87.8 | 93.9 | 99.0 | 88.3 | 86.7 |
| sky:cloudy+rainy | 318.0 | 305.5 | 317.2 | 101.2 | 102.6 | 109.0 | 112.7 | 106.8 | 101.3 |
| shade:unshaded | 243.5 | 235.9 | 242.8 | 105.0 | 102.9 | 109.5 | 109.3 | 110.2 | 99.7 |
| shade:shaded | 26.0 | 50.8 | 25.3 | 29.9 | 28.9 | 30.9 | 26.2 | 65.3 | 20.6 |

## E2: an unseen sensor (RMSE by group fixed in advance)

C is not applicable in E2 (its dummy for an unseen sensor is undefined). Pinc is an orientation extrapolation.

| unit | A | B | S | R | P-cs | P-sim | D | H | S-geo |
|---|---|---|---|---|---|---|---|---|---|
| E2:shaded | 227.0 | 229.8 | 97.4 | 228.4 | 99.0 | 99.6 | 103.1 | 104.2 | 96.8 |
| E2:open | 227.9 | 229.4 | 96.6 | 229.0 | 98.3 | 98.8 | 102.7 | 103.6 | 96.4 |
| E2:tilted | 318.1 | 250.0 | n/a | 306.6 | 135.3 | 117.9 | 141.6 | 137.0 | 108.8 |

## E3: shade rows after training without simulated occlusion

| model | RMSE | MAE | MBE | R² |
|---|---|---|---|---|
| A Simulation | 26.0 [19.7, 31.8] | 19.4 | 7.5 | 0.221 |
| B Clear-sky | 50.8 [37.0, 62.4] | 37.6 | 36.0 | -1.972 |
| R Ray-cast shading | 25.3 [20.5, 29.9] | 18.3 | -7.9 | 0.263 |
| P-cs Formula, clear-sky | 29.9 [18.5, 43.3] | 18.6 | 4.6 | -0.027 |
| P-sim Formula, simulation | 28.9 [21.7, 35.3] | 18.9 | -12.3 | 0.035 |
| D XGBoost, clear-sky | 33.0 [22.2, 45.8] | 23.6 | 18.6 | -0.254 |
| H XGBoost, simulation (proposed) | 26.6 [20.0, 32.6] | 17.7 | -8.8 | 0.185 |
| C XGBoost, meteo + sensor | 76.1 [58.2, 97.7] | 66.3 | 65.5 | -5.668 |
| S-geo Solcast DNI/DHI + local geometry (added after protocol-v1) | 20.6 [16.9, 24.2] | 16.0 | 9.7 | 0.513 |

## E4: an unseen month (all nine months pooled)

| model | RMSE | MAE | MBE | R² |
|---|---|---|---|---|
| A Simulation | 230.8 [190.9, 266.3] | 139.2 | 114.8 | 0.453 |
| B Clear-sky | 227.5 [188.0, 263.0] | 140.5 | 109.6 | 0.468 |
| R Ray-cast shading | 230.7 [190.4, 266.5] | 138.9 | 111.5 | 0.454 |
| P-cs Formula, clear-sky | 96.1 [79.2, 112.8] | 56.0 | -3.4 | 0.905 |
| P-sim Formula, simulation | 91.9 [73.5, 108.9] | 52.1 | -2.1 | 0.913 |
| D XGBoost, clear-sky | 102.0 [86.7, 117.2] | 66.7 | 2.9 | 0.893 |
| H XGBoost, simulation (proposed) | 99.2 [83.5, 114.3] | 65.6 | -0.2 | 0.899 |
| C XGBoost, meteo + sensor | 102.9 [88.4, 117.3] | 70.9 | 2.3 | 0.891 |
| S-geo Solcast DNI/DHI + local geometry (added after protocol-v1) | 91.8 [73.4, 108.7] | 50.9 | 17.3 | 0.914 |

## Overfitting diagnostic (E1)

RMSE on the model's own training rows, in cross-validation inside the training days, and on the scored rows.

| model | train_rmse | cv_rmse | score_rmse |
|---|---|---|---|
| D | 100.87 | 99.47 | 107.75 |
| H | 98.55 | 97.71 | 107.47 |
| C | 94.36 | 99.97 | 108.92 |

## Hyperparameters chosen across the runs

How many of the runs chose each configuration.

| family | max_depth | n_estimators | min_child_weight | colsample_bytree | n_runs |
|---|---|---|---|---|---|
| C | 3 | 300 | 200 | 0.8 | 9 |
| C | 2 | 800 | 200 | 0.8 | 1 |
| C | 4 | 300 | 200 | 0.8 | 1 |
| H+D | 2 | 300 | 200 | 0.5 | 18 |
| H+D | 2 | 300 | 200 | 1.0 | 2 |

## Sensitivities (H against D in E1)

| variant | all nine sensors | horizontals only |
|---|---|---|
| main (altitude > 5°, clip 2.0) | +0.28 W/m² (95 % interval -2.62 to +4.38; 45342 rows, 15 days) | -1.05 W/m² (95 % interval -2.04 to +0.13; 40304 rows, 15 days) |
| altitude > 10° | +1.27 W/m² (95 % interval -2.05 to +5.75; 41418 rows, 15 days) | -0.25 W/m² (95 % interval -0.99 to +0.62; 36816 rows, 15 days) |
| upper clip 3.0 | +0.15 W/m² (95 % interval -2.69 to +4.20; 45342 rows, 15 days) | -1.17 W/m² (95 % interval -2.21 to +0.07; 40304 rows, 15 days) |
| no upper clip | +0.11 W/m² (95 % interval -2.75 to +4.14; 45342 rows, 15 days) | -1.21 W/m² (95 % interval -2.27 to +0.05; 40304 rows, 15 days) |

Cloudy and rainy pooled (E1, RMSE):

| model | RMSE | MAE | MBE | R² |
|---|---|---|---|---|
| A Simulation | 318.0 [238.7, 382.8] | 251.0 | 240.2 | -2.006 |
| B Clear-sky | 305.5 [216.6, 376.0] | 239.7 | 227.2 | -1.775 |
| R Ray-cast shading | 317.2 [236.0, 383.1] | 248.3 | 236.4 | -1.99 |
| P-cs Formula, clear-sky | 101.2 [63.4, 129.0] | 57.5 | -7.8 | 0.696 |
| P-sim Formula, simulation | 102.6 [66.7, 129.7] | 60.1 | -4.8 | 0.687 |
| D XGBoost, clear-sky | 109.0 [64.6, 141.7] | 68.3 | 25.3 | 0.647 |
| H XGBoost, simulation (proposed) | 112.7 [71.4, 144.3] | 72.5 | 29.7 | 0.623 |
| C XGBoost, meteo + sensor | 106.8 [70.8, 135.9] | 70.1 | 19.9 | 0.661 |
| S-geo Solcast DNI/DHI + local geometry (added after protocol-v1) | 101.3 [63.8, 128.8] | 61.3 | 15.9 | 0.695 |
