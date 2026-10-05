# Day split v2 (seed 42)

58 usable days (2 dropped for lack of real data: 2025-04-04, 2025-06-13). 15 test / 43 train. The test days were chosen to cover every sky class and both P1-shadow regimes; the three TFG days are always included.

## Days per sky class and split

| sky | test | train |
|---|---|---|
| sunny | 3 | 12 |
| mixed | 4 | 16 |
| cloudy | 4 | 6 |
| rainy | 4 | 9 |


## P1-shadow regime (P1 shaded = at least 10 bins with the sun more than half hidden)

| sky | p1_shaded | test | train |
|---|---|---|---|
| cloudy | False | 1 | 3 |
| cloudy | True | 3 | 3 |
| mixed | False | 3 | 13 |
| mixed | True | 1 | 3 |
| rainy | False | 1 | 2 |
| rainy | True | 3 | 7 |
| sunny | False | 2 | 10 |
| sunny | True | 1 | 2 |


## Months

| month | test | train |
|---|---|---|
| 2025-04 | 2 | 4 |
| 2025-05 | 2 | 5 |
| 2025-06 | 1 | 5 |
| 2025-07 | 1 | 6 |
| 2025-08 | 1 | 5 |
| 2025-09 | 2 | 5 |
| 2025-10 | 2 | 5 |
| 2025-11 | 2 | 4 |
| 2025-12 | 2 | 4 |


## Shaded bins (sun > 5 deg, sun_visibility < 0.5) per split

| split | shade_P1 | shade_P3 | shade_P5 | shade_P7 |
|---|---|---|---|---|
| test | 193 | 256 | 407 | 408 |
| train | 332 | 778 | 1063 | 1114 |


## The test days, for review

| date_local | sky | k_med | clear_frac | dark_frac | p1_shaded | shade_P1 | shade_P5 | shade_P7 | ok_P0 | ok_P1 | tfg_day |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 2025-05-05 | cloudy | 0.4 | 0.14 | 0.42 | False | 0 | 20 | 26 | 1.0 | 1.0 | False |
| 2025-09-28 | cloudy | 0.49 | 0.0 | 0.09 | True | 31 | 29 | 32 | 1.0 | 1.0 | False |
| 2025-09-30 | cloudy | 0.51 | 0.17 | 0.01 | True | 30 | 29 | 32 | 1.0 | 1.0 | False |
| 2025-12-14 | cloudy | 0.48 | 0.02 | 0.07 | True | 12 | 28 | 25 | 1.0 | 1.0 | False |
| 2025-04-20 | mixed | 0.83 | 0.49 | 0.0 | False | 0 | 27 | 30 | 1.0 | 1.0 | True |
| 2025-05-26 | mixed | 1.01 | 0.81 | 0.0 | False | 0 | 23 | 26 | 1.0 | 1.0 | False |
| 2025-06-01 | mixed | 0.86 | 0.54 | 0.0 | False | 0 | 21 | 25 | 1.0 | 1.0 | False |
| 2025-10-21 | mixed | 0.8 | 0.49 | 0.21 | True | 23 | 32 | 27 | 1.0 | 1.0 | False |
| 2025-04-11 | rainy | 0.28 | 0.12 | 0.52 | False | 0 | 34 | 26 | 1.0 | 1.0 | True |
| 2025-11-07 | rainy | 0.2 | 0.08 | 0.62 | True | 30 | 34 | 32 | 1.0 | 1.0 | False |
| 2025-11-24 | rainy | 0.38 | 0.0 | 0.22 | True | 21 | 34 | 24 | 1.0 | 1.0 | False |
| 2025-12-12 | rainy | 0.39 | 0.0 | 0.2 | True | 13 | 28 | 25 | 1.0 | 1.0 | False |
| 2025-07-01 | sunny | 0.99 | 0.96 | 0.0 | False | 0 | 19 | 23 | 1.0 | 1.0 | False |
| 2025-08-05 | sunny | 0.97 | 1.0 | 0.0 | False | 0 | 19 | 27 | 1.0 | 1.0 | False |
| 2025-10-07 | sunny | 1.03 | 1.0 | 0.0 | True | 33 | 30 | 28 | 1.0 | 1.0 | True |


## Usable days kept out of the test set (training only)

A day needs >= 90 % `qc_ok` daylight rows on P0, P1, P3, P5, P7 and no all-sensor blackout.

| date_local | sky | blackout_rows | ok_P0 | ok_P1 | ok_P3 | ok_P5 | ok_P7 |
|---|---|---|---|---|---|---|---|
| 2025-04-03 | rainy | 0 | 0.85 | 0.85 | 0.85 | 0.85 | 0.85 |
| 2025-07-06 | mixed | 0 | 1.0 | 0.0 | 1.0 | 1.0 | 1.0 |
| 2025-07-12 | mixed | 0 | 1.0 | 0.0 | 1.0 | 1.0 | 1.0 |
| 2025-07-16 | sunny | 0 | 1.0 | 0.0 | 1.0 | 1.0 | 1.0 |
| 2025-07-21 | sunny | 1211 | 0.62 | 0.0 | 0.62 | 0.62 | 0.62 |

