# Analysis — dataset, model and validation

The machine-learning half of the project. It takes the Unreal Engine irradiance
simulation as a feature and predicts the irradiance measured by the real
pyranometers on the ETSIDI rooftop.

## Layout

- `data/` — the training dataset (`dataset_master_tfg.csv`), assembled from
  Solcast atmospheric data, the UE plugin's simulated irradiance, and the
  university pyranometer measurements.
- `notebooks/` — the Jupyter notebooks, in execution order:
  1. `01_generate_dataset.ipynb` — assembles `data/dataset_master_tfg.csv` from
     the raw sources.
  2. `02_baselines.ipynb` — baseline models (clear-sky, physical simulation,
     plain XGBoost on meteorology).
  3. `03_model_final.ipynb` — the final model.
  4. `04_model_comparison.ipynb` — comparison of all models.
- `artifacts/` — exported results: `predictions/` (per-model, per-day CSVs),
  `figures/`, `final_model/` (the pickled model + its metadata), and
  `iterations/` (stored metadata for 10 of the 37 tuning iterations, plus
  `analyze_iterations.ipynb`).
- `normalization/` — scripts for Pyrano's internal radiometric normalization
  (fitting the lux → W/m² coefficients).

## Running it

```bash
python -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
jupyter lab
```

**Do not re-run `notebooks/01_generate_dataset.ipynb`.** The built dataset is
committed; the raw Solcast / pyranometer inputs are not. Start from
`02_baselines.ipynb`.

Test days: 2025-04-11, 2025-04-20, 2025-10-07 (the last is the shadow-validation
day). The final model targets `k_eff` (effective clearness index) with features
`sim_irradiance_wm2` and `cloud_opacity`.

`artifacts/final_model/final_model.pkl` is a pickle — only unpickle it in an
environment you trust, and expect it to be sensitive to the scikit-learn /
xgboost versions in `requirements.txt`.
