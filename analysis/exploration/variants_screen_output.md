# Variants screen (exploratory)

43 development days, 132826 rows, leave-one-month-out (nine folds). Run after the protocol-v1 results were seen; the 15 test days are not used. Gain = RMSE of the model it is compared with minus its own RMSE, in W/m2 (positive is better), with a 95 % paired block-bootstrap interval over days.

## nine sensors

| model | RMSE | compared with | gain | 95 % interval |
|---|---|---|---|---|
| P-sim | 91.86 |  |  |  |
| H | 101.78 |  |  |  |
| P-exp | 106.55 | P-sim | -14.68 | [-23.09, -7.51] |
| P-sim+PW | 90.68 | P-sim | +1.19 | [+0.41, +2.05] |
| H+PW | 113.84 | H | -12.07 | [-15.90, -8.81] |
| DD-sim | 91.74 | P-sim | +0.12 | [-0.29, +0.52] |
| DD-ray | 91.72 | P-sim | +0.15 | [-0.35, +0.62] |
| S-geo | 91.77 |  |  |  |

## horizontals

| model | RMSE | compared with | gain | 95 % interval |
|---|---|---|---|---|
| P-sim | 90.14 |  |  |  |
| H | 99.01 |  |  |  |
| P-exp | 103.63 | P-sim | -13.49 | [-21.56, -6.63] |
| P-sim+PW | 88.94 | P-sim | +1.20 | [+0.43, +2.03] |
| H+PW | 110.57 | H | -11.57 | [-15.39, -8.30] |
| DD-sim | 90.00 | P-sim | +0.14 | [-0.27, +0.54] |
| DD-ray | 90.11 | P-sim | +0.03 | [-0.47, +0.49] |
| S-geo | 90.78 |  |  |  |

S-geo has no parameters and is shown only as a reference line. P-sim reproduces the E4 pooled value of the evaluation (91.86). H here uses one fixed configuration, so it is a little worse than the evaluation's H (99.24, configuration selected in every fold); H+PW is compared with this H.
