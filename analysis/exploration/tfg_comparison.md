# The TFG tables, recomputed with the v2 evaluation

Exploratory (post-run). Same rows for every model of the 'common rows' blocks: the three TFG test days, P0, P1, P4 and Pinc, altitude > 5 degrees and qc_ok (4208 rows). The new models come from the E1 run (trained on the 43 training days, hyperparameters chosen inside the run); the TFG models are the TFG's own saved predictions (trained on 9 days; the model and its hyperparameters were chosen among about 37 variants while looking at these three days). Columns: R2 | RMSE | MAE | MBE, in W/m2.

The TFG published each model on its own rows (4208 for A, B, C and 5400 for the final model), so the 'published' blocks are not like for like.

### Table 9.1: global, the three test days and four sensors

| model | R2 | RMSE | MAE | MBE |
|---|---|---|---|---|
| TFG Final (published, own rows) | 0.833 | 123.8 | 67.8 | -0.0 |
| TFG Baseline C (published, own rows) | 0.711 | 157.1 | 105.1 | -14.7 |
| TFG Baseline B (published, own rows) | 0.085 | 279.5 | 198.9 | +119.7 |
| TFG Baseline A (published, own rows) | -0.113 | 308.3 | 212.9 | +157.4 |
| TFG Baseline A (recomputed, common rows) | -0.116 | 308.6 | 213.4 | +157.4 |
| TFG Baseline B (recomputed, common rows) | 0.084 | 279.5 | 198.9 | +119.7 |
| TFG Baseline C (recomputed, common rows) | 0.707 | 158.2 | 106.2 | -14.7 |
| TFG Final (recomputed, common rows) | 0.765 | 141.6 | 88.0 | -0.2 |
| **now** A simulation | -0.002 | 292.3 | 199.1 | +144.9 |
| **now** B clear-sky | 0.048 | 285.0 | 202.8 | +125.9 |
| **now** C meteo | 0.724 | 153.5 | 101.6 | -21.5 |
| **now** H proposed (sim + cloud) | 0.742 | 148.3 | 101.2 | -16.7 |
| **now** R ray-cast | 0.005 | 291.4 | 199.2 | +140.3 |
| **now** P-sim formula | 0.758 | 143.7 | 87.0 | -29.0 |
| **now** D clear-sky + cloud | 0.710 | 157.2 | 107.0 | -27.7 |

4208 rows, 3 day(s), sensors ['P0', 'P1', 'P4', 'Pinc'].

### Table 9.1 bis (extra): the same without Pinc, so that Solcast GHI can be scored

| model | R2 | RMSE | MAE | MBE |
|---|---|---|---|---|
| TFG Baseline A (recomputed, common rows) | -0.031 | 280.5 | 197.6 | +138.1 |
| TFG Baseline B (recomputed, common rows) | 0.031 | 272.1 | 186.0 | +131.6 |
| TFG Baseline C (recomputed, common rows) | 0.702 | 150.9 | 100.5 | -8.3 |
| TFG Final (recomputed, common rows) | 0.755 | 136.7 | 84.3 | -4.6 |
| **now** A simulation | 0.009 | 275.0 | 188.7 | +136.4 |
| **now** B clear-sky | -0.011 | 277.8 | 189.9 | +137.8 |
| **now** C meteo | 0.733 | 142.8 | 91.3 | -10.0 |
| **now** H proposed (sim + cloud) | 0.730 | 143.6 | 96.9 | -21.7 |
| **now** R ray-cast | -0.005 | 277.1 | 189.0 | +136.0 |
| **now** P-sim formula | 0.737 | 141.7 | 85.9 | -31.3 |
| **now** D clear-sky + cloud | 0.734 | 142.4 | 94.0 | -15.8 |
| **now** S Solcast GHI | 0.758 | 136.1 | 84.1 | +2.3 |

3156 rows, 3 day(s), sensors ['P0', 'P1', 'P4'].

### Table 9.2 clear sky 2025-10-07, aggregated over the four sensors

| model | R2 | RMSE | MAE | MBE |
|---|---|---|---|---|
| TFG Final (published, own rows) | 0.990 | 29.5 | 17.5 | +0.0 |
| TFG Baseline C (published, own rows) | 0.821 | 102.7 | 78.7 | -17.9 |
| TFG Baseline B (published, own rows) | 0.812 | 105.1 | 61.4 | -48.4 |
| TFG Baseline A (published, own rows) | 0.967 | 44.2 | 36.8 | -11.4 |
| TFG Baseline A (recomputed, common rows) | 0.967 | 44.3 | 37.2 | -11.4 |
| TFG Baseline B (recomputed, common rows) | 0.814 | 104.4 | 61.0 | -48.5 |
| TFG Baseline C (recomputed, common rows) | 0.819 | 103.1 | 79.1 | -18.0 |
| TFG Final (recomputed, common rows) | 0.979 | 34.8 | 23.9 | -1.2 |
| **now** A simulation | 0.986 | 28.6 | 23.7 | -17.4 |
| **now** B clear-sky | 0.815 | 104.2 | 61.4 | -50.9 |
| **now** C meteo | 0.821 | 102.6 | 66.3 | -19.1 |
| **now** H proposed (sim + cloud) | 0.947 | 55.7 | 46.5 | -43.9 |
| **now** R ray-cast | 0.978 | 36.3 | 29.2 | -23.1 |
| **now** P-sim formula | 0.979 | 35.3 | 26.2 | -20.1 |
| **now** D clear-sky + cloud | 0.762 | 118.3 | 75.2 | -69.4 |

1260 rows, 1 day(s), sensors ['P0', 'P1', 'P4', 'Pinc'].

### Table 9.3 variable sky 2025-04-20, aggregated over the four sensors

| model | R2 | RMSE | MAE | MBE |
|---|---|---|---|---|
| TFG Final (published, own rows) | 0.687 | 181.1 | 120.1 | -21.7 |
| TFG Baseline C (published, own rows) | 0.540 | 201.6 | 140.4 | -0.2 |
| TFG Baseline B (published, own rows) | 0.411 | 228.0 | 183.7 | +68.3 |
| TFG Baseline A (published, own rows) | 0.190 | 267.4 | 210.4 | +106.0 |
| TFG Baseline A (recomputed, common rows) | 0.186 | 267.4 | 210.9 | +105.9 |
| TFG Baseline B (recomputed, common rows) | 0.411 | 227.6 | 183.6 | +68.3 |
| TFG Baseline C (recomputed, common rows) | 0.540 | 201.1 | 141.5 | -0.2 |
| TFG Final (recomputed, common rows) | 0.552 | 198.3 | 145.6 | -25.9 |
| **now** A simulation | 0.329 | 242.8 | 194.0 | +88.6 |
| **now** B clear-sky | 0.388 | 231.9 | 186.1 | +78.2 |
| **now** C meteo | 0.541 | 200.9 | 145.8 | -63.7 |
| **now** H proposed (sim + cloud) | 0.570 | 194.4 | 144.3 | -62.2 |
| **now** R ray-cast | 0.335 | 241.7 | 192.7 | +84.5 |
| **now** P-sim formula | 0.547 | 199.6 | 144.3 | -64.8 |
| **now** D clear-sky + cloud | 0.567 | 195.1 | 140.6 | -66.4 |

1496 rows, 1 day(s), sensors ['P0', 'P1', 'P4', 'Pinc'].

### Table 9.4 overcast 2025-04-11, aggregated over the four sensors

| model | R2 | RMSE | MAE | MBE |
|---|---|---|---|---|
| TFG Final (published, own rows) | 0.768 | 111.1 | 65.9 | +21.6 |
| TFG Baseline C (published, own rows) | 0.644 | 143.3 | 91.4 | -26.8 |
| TFG Baseline B (published, own rows) | -1.836 | 404.1 | 334.1 | +318.6 |
| TFG Baseline A (published, own rows) | -2.476 | 447.4 | 368.4 | +356.9 |
| TFG Baseline A (recomputed, common rows) | -2.481 | 447.8 | 368.9 | +356.9 |
| TFG Baseline B (recomputed, common rows) | -1.839 | 404.5 | 334.4 | +318.6 |
| TFG Baseline C (recomputed, common rows) | 0.625 | 147.0 | 93.5 | -26.8 |
| TFG Final (recomputed, common rows) | 0.713 | 128.5 | 84.3 | +27.0 |
| **now** A simulation | -2.232 | 431.5 | 356.6 | +343.7 |
| **now** B clear-sky | -1.960 | 413.0 | 342.9 | +328.5 |
| **now** C meteo | 0.696 | 132.4 | 86.8 | +19.9 |
| **now** H proposed (sim + cloud) | 0.617 | 148.6 | 104.1 | +53.7 |
| **now** R ray-cast | -2.206 | 429.8 | 353.5 | +339.7 |
| **now** P-sim formula | 0.693 | 133.0 | 80.5 | +0.1 |
| **now** D clear-sky + cloud | 0.649 | 142.3 | 99.9 | +48.3 |

1452 rows, 1 day(s), sensors ['P0', 'P1', 'P4', 'Pinc'].

### Table 9.5 Baseline A (the simulation alone) per sensor, 2025-10-07

'now' is model A; B is the clear-sky formula, shown for reference.

| sensor | source | R2 | RMSE | MAE | MBE |
|---|---|---|---|---|---|
| P0 | TFG published (own rows) | 0.954 | 45.0 | 41.6 | -39.2 |
| P0 | TFG recomputed (common rows) | 0.954 | 45.0 | 41.5 | -39.2 |
| P0 | **now** A | 0.985 | 25.3 | 24.3 | -24.1 |
| P0 | **now** B clear-sky | 0.985 | 25.8 | 24.8 | -24.8 |
| P1 | TFG published (own rows) | 0.965 | 42.5 | 38.5 | -29.0 |
| P1 | TFG recomputed (common rows) | 0.967 | 41.2 | 38.2 | -29.0 |
| P1 | **now** A | 0.986 | 27.1 | 23.6 | -15.7 |
| P1 | **now** B clear-sky | 0.970 | 39.6 | 27.6 | -7.6 |
| P4 | TFG published (own rows) | 0.992 | 17.9 | 15.7 | -10.5 |
| P4 | TFG recomputed (common rows) | 0.992 | 17.7 | 15.5 | -10.5 |
| P4 | **now** A | 0.997 | 11.2 | 9.5 | +6.7 |
| P4 | **now** B clear-sky | 0.993 | 16.2 | 14.5 | +6.5 |
| Pinc | TFG published (own rows) | 0.953 | 60.3 | 51.5 | +33.2 |
| Pinc | TFG recomputed (common rows) | 0.951 | 61.9 | 53.4 | +33.0 |
| Pinc | **now** A | 0.977 | 42.1 | 37.5 | -36.3 |
| Pinc | **now** B clear-sky | 0.474 | 202.4 | 178.6 | -177.5 |

315 rows per sensor.

### Table 9.6 Final model per sensor, 2025-10-07

'now' is model H; B is the clear-sky formula, shown for reference.

| sensor | source | R2 | RMSE | MAE | MBE |
|---|---|---|---|---|---|
| P0 | TFG published (own rows) | 0.990 | 27.4 | 17.1 | -15.5 |
| P0 | TFG recomputed (common rows) | 0.976 | 32.4 | 23.5 | -22.9 |
| P0 | **now** H | 0.925 | 57.3 | 50.6 | -50.5 |
| P0 | **now** B clear-sky | 0.985 | 25.8 | 24.8 | -24.8 |
| P1 | TFG published (own rows) | 0.992 | 25.4 | 15.4 | -9.3 |
| P1 | TFG recomputed (common rows) | 0.985 | 28.2 | 20.1 | -13.8 |
| P1 | **now** H | 0.940 | 55.6 | 49.4 | -41.8 |
| P1 | **now** B clear-sky | 0.970 | 39.6 | 27.6 | -7.6 |
| P4 | TFG published (own rows) | 0.997 | 14.9 | 7.9 | +4.4 |
| P4 | TFG recomputed (common rows) | 0.992 | 17.5 | 10.6 | +5.8 |
| P4 | **now** H | 0.981 | 27.8 | 21.0 | -19.7 |
| P4 | **now** B clear-sky | 0.993 | 16.2 | 14.5 | +6.5 |
| Pinc | TFG published (own rows) | 0.986 | 43.1 | 29.5 | +20.4 |
| Pinc | TFG recomputed (common rows) | 0.966 | 51.8 | 41.3 | +26.3 |
| Pinc | **now** H | 0.933 | 72.5 | 64.8 | -63.7 |
| Pinc | **now** B clear-sky | 0.474 | 202.4 | 178.6 | -177.5 |

315 rows per sensor.

### Check: the published tables are reproduced from the TFG's own files

- ok  9.1 global (3 days) Final: published (0.833, 123.8, 67.8, -0.03) | recomputed (0.833, 123.832, 67.818, -0.034)
- ok  9.1 global (3 days) C: published (0.711, 157.1, 105.1, -14.7) | recomputed (0.711, 157.136, 105.014, -14.686)
- ok  9.1 global (3 days) B: published (0.085, 279.5, 198.9, 119.7) | recomputed (0.086, 279.524, 198.993, 119.693)
- ok  9.1 global (3 days) A: published (-0.113, 308.3, 212.9, 157.4) | recomputed (-0.113, 308.329, 212.943, 157.413)
- ok  9.2 clear sky 2025-10-07 Final: published (0.99, 29.5, 17.5, 0.0) | recomputed (0.99, 29.468, 17.45, 0.002)
- ok  9.2 clear sky 2025-10-07 A: published (0.967, 44.2, 36.8, -11.4) | recomputed (0.967, 44.154, 36.845, -11.369)
- ok  9.2 clear sky 2025-10-07 C: published (0.821, 102.7, 78.7, -17.9) | recomputed (0.821, 102.669, 78.702, -17.937)
- ok  9.2 clear sky 2025-10-07 B: published (0.812, 105.1, 61.4, -48.4) | recomputed (0.812, 105.131, 61.429, -48.429)
- ok  9.3 variable sky 2025-04-20 Final: published (0.687, 181.1, 120.1, -21.7) | recomputed (0.687, 181.065, 120.14, -21.723)
- ok  9.3 variable sky 2025-04-20 C: published (0.54, 201.6, 140.4, -0.2) | recomputed (0.54, 201.636, 140.352, -0.192)
- ok  9.3 variable sky 2025-04-20 B: published (0.411, 228.0, 183.7, 68.3) | recomputed (0.411, 227.953, 183.689, 68.281)
- ok  9.3 variable sky 2025-04-20 A: published (0.19, 267.4, 210.4, 106.0) | recomputed (0.19, 267.374, 210.394, 105.954)
- ok  9.4 overcast 2025-04-11 Final: published (0.768, 111.1, 65.9, 21.6) | recomputed (0.768, 111.131, 65.863, 21.62)
- ok  9.4 overcast 2025-04-11 C: published (0.644, 143.3, 91.4, -26.8) | recomputed (0.644, 143.257, 91.438, -26.797)
- ok  9.4 overcast 2025-04-11 B: published (-1.836, 404.1, 334.1, 318.6) | recomputed (-1.836, 404.115, 334.134, 318.553)
- ok  9.4 overcast 2025-04-11 A: published (-2.476, 447.4, 368.4, 356.9) | recomputed (-2.476, 447.395, 368.382, 356.896)
