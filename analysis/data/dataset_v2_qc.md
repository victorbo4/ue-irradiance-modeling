# dataset_v2 — QC report

234,540 rows = 60 days x 10 sensors on a 2-min UTC grid. QC **marks** rows, it never removes them; the evaluation applies one common mask.

## Rows flagged, by sensor (daylight rows, sun > 5 deg)

| sensor | qc_real_sparse | qc_dup_conflict | qc_sensor_dead | qc_flatline | qc_neg_large | qc_night_nonzero | qc_over_clearsky | n_daylight | qc_ok_% |
|---|---|---|---|---|---|---|---|---|---|
| P0 | 467 | 15 | 0 | 0 | 0 | 0 | 1 | 20969 | 97.7 |
| P1 | 467 | 18 | 1640 | 0 | 0 | 0 | 0 | 20969 | 89.9 |
| P2 | 467 | 13 | 12299 | 0 | 0 | 0 | 0 | 20969 | 39.1 |
| P3 | 467 | 21 | 0 | 0 | 0 | 0 | 1 | 20969 | 97.7 |
| P4 | 467 | 19 | 0 | 0 | 0 | 0 | 0 | 20969 | 97.7 |
| P5 | 467 | 14 | 0 | 0 | 0 | 0 | 3 | 20969 | 97.7 |
| P6 | 467 | 21 | 0 | 0 | 0 | 0 | 2 | 20969 | 97.7 |
| P7 | 467 | 19 | 0 | 0 | 0 | 0 | 2 | 20969 | 97.7 |
| P8 | 467 | 22 | 0 | 0 | 0 | 0 | 1 | 20969 | 97.7 |
| Pinc | 467 | 18 | 0 | 0 | 0 | 0 | 5 | 20969 | 97.7 |


## Usable (sensor, day) pairs

A pair is usable when >= 80% of its daylight rows are `qc_ok`.

| sensor | usable_days |
|---|---|
| P0 | 58 |
| P1 | 54 |
| P2 | 20 |
| P3 | 58 |
| P4 | 58 |
| P5 | 58 |
| P6 | 58 |
| P7 | 58 |
| P8 | 58 |
| Pinc | 58 |


- **P0** unusable: 2025-04-04, 2025-06-13
- **P1** unusable: 2025-04-04, 2025-06-13, 2025-07-06, 2025-07-12, 2025-07-16, 2025-07-21
- **P2** unusable: 2025-04-04, 2025-06-13, 2025-07-06, 2025-07-12, 2025-07-16, 2025-07-21, 2025-07-26, 2025-07-31, 2025-08-01, 2025-08-02, 2025-08-03, 2025-08-04, 2025-08-05, 2025-08-06, 2025-09-07, 2025-09-13, 2025-09-17, 2025-09-21, 2025-09-26, 2025-09-28, 2025-09-30, 2025-10-07, 2025-10-19, 2025-10-21, 2025-10-23, 2025-10-28, 2025-10-29, 2025-10-31, 2025-11-01, 2025-11-07, 2025-11-13, 2025-11-15, 2025-11-19, 2025-11-24, 2025-12-10, 2025-12-12, 2025-12-14, 2025-12-16, 2025-12-19, 2025-12-23
- **P3** unusable: 2025-04-04, 2025-06-13
- **P4** unusable: 2025-04-04, 2025-06-13
- **P5** unusable: 2025-04-04, 2025-06-13
- **P6** unusable: 2025-04-04, 2025-06-13
- **P7** unusable: 2025-04-04, 2025-06-13
- **P8** unusable: 2025-04-04, 2025-06-13
- **Pinc** unusable: 2025-04-04, 2025-06-13

## Days with truncated real data

| date_local | real_coverage_% | first_real_utc | last_real_utc |
|---|---|---|---|
| 2025-04-03 | 84.7 | 08:10 | 18:12 |
| 2025-04-04 | 69.8 | 10:00 | 18:12 |
| 2025-06-13 | 28.8 | 14:28 | 19:12 |


## Time alignment (real vs simulation, P0, clear days)

26 clear days. Median shift +0 min, IQR [+0, +2] min, range [-4, +8] min. No systematic offset means the local-to-UTC conversion is right.

## Simulated vs measured on clear daylight (P0, qc_ok, cloud_opacity < 8 %)

9,524 rows. real/sim ratio: median 1.021, P10 0.895, P90 1.129. A calibrated, unbiased simulator would centre on 1.
