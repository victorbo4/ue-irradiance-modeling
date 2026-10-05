# Dataset pipeline (v2)

Builds `analysis/data/dataset_v2.csv` from the 60-day simulation campaign, the
university pyranometers and Solcast.

```bash
analysis/.venv/bin/python analysis/pipeline/build_dataset.py   # dataset + manifest
analysis/.venv/bin/python analysis/pipeline/qc_report.py       # QC summary
analysis/.venv/bin/python -m pytest analysis/pipeline/tests    # tests
```

## Design rules

- **Everything is UTC.** The pyranometer files are Madrid wall-clock time; they
  are converted once, DST-aware, on load.
- **One row per (sensor, 2-min UTC timestamp)**, the simulation's grid. 10 sensors
  (`P1`–`P8`, `P0` = CLASE_A, `Pinc` = INCLINADO).
- **Nothing is dropped.** QC problems are `qc_*` columns; the evaluation applies a
  single mask, identical for every model.
- Pyranometer files are read together and merged by timestamp, not matched to
  days by file name (some straddle midnight or repeat each other's rows).
- Real 5 s samples are averaged over `[t − 1 min, t + 1 min)` for each simulation
  timestamp `t`; `real_n` is the number of samples in the bin (24 when complete).
- Solcast's 5-min rows are the mean over the *preceding* five minutes, so each is
  anchored to `period_end − 2.5 min` and linearly interpolated to `t`.

## Columns

| Group | Columns |
|---|---|
| Key | `utc`, `sensor`, `date_local` (Europe/Madrid day) |
| Geometry | `pos_*`, `n_*`, `sun_azimuth_deg`, `sun_altitude_deg`, `sky_view_factor` |
| Sun visibility | `sun_visibility` (0–1, 32 samples), `sun_occluded`, `sun_hit_distance_m`, `geometric_factor` |
| Simulator | `sim_irradiance_wm2`, `sim_comp_amb_lux`, `sim_comp_direct_lux`, `raw_{r,g,b}_lux` |
| Clear-sky | `clearsky_{ghi,dni,dhi}_wm2` (Ineichen, from the simulator) |
| Target | `real_wm2` (bin mean), `real_n`, `real_std` |
| Meteo | `cloud_opacity` (%), `precipitable_water`, `solcast_ghi_wm2`, `solcast_clearsky_ghi_wm2`, `solcast_dni_wm2`, `solcast_dhi_wm2`, `precipitation_rate`, `weather_type`, `meteo_gap_s` |

`solcast_dni_wm2`, `solcast_dhi_wm2` and `precipitation_rate` are diagnostic only: no model in
the protocol uses them.

The `_lux` suffix on the simulator columns is a historical misnomer (bug-tracker
S9/A6): they are not photometric lux. They keep their names for traceability.
`sim_irradiance_wm2` is the calibrated output, not an independent physical
estimate (see the simulator-is-calibrated note).

## QC flags (True = problem)

| Flag | Meaning |
|---|---|
| `qc_real_sparse` | fewer than 12 of 24 expected 5 s samples in the bin (includes no data) |
| `qc_dup_conflict` | a repeated timestamp had values differing by more than 5 W/m² |
| `qc_sensor_dead` | the sensor's maximum that local day stayed below 20 W/m² although clear-sky GHI exceeded 300 |
| `qc_flatline` | the same value above 5 W/m² for 15+ consecutive bins (30 min) |
| `qc_neg_large` | below −5 W/m² (small negatives are offset noise and not flagged) |
| `qc_night_nonzero` | above 10 W/m² while the sun is below the horizon |
| `qc_over_clearsky` | **diagnostic only, not part of `qc_ok`**: above 1.6 × max(clear-sky GHI, the sensor's own simulated irradiance) + 50 |
| `qc_zero_daylight` | at one instant every working sensor (4+) reads exactly 0 although the simulated sun is up: logger zero-fill or blackout. Zeros on only some sensors (a dark storm quantised to 0) are not flagged |
| `qc_channel_dropout` | one sensor reads exactly 0 while the other working sensors read at least 100 W/m² at the same instant: a lost channel, not a cloud |
| `qc_ok` | none of the hard faults above (every flag except `qc_over_clearsky`) |

Cloud-edge enhancement can legitimately exceed clear-sky GHI, and a tilted plane at low
sun can exceed the horizontal estimate, so `qc_over_clearsky` is a diagnostic: its 25 rows
were inspected (24/24 samples, several sensors at the same instants, smooth sequences) and
look like real measurements. Excluding them would trim the upper tail of the error.

Thresholds live at the top of `build_dataset.py` and are recorded in
`dataset_v2_manifest.json` together with input hashes and the git commit.
