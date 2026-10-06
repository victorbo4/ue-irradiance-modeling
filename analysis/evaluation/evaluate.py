"""Stage 2 of the evaluation: metrics, intervals and tables from what ``run.py`` saved.

    python -m evaluation.evaluate [--results-dir DIR] [--n-boot 2000]

Reads <results>/<variant>/<run>/{predictions.csv, fitted.json} and writes, per variant,
<results>/tables/<variant>/:

    metrics.csv         R2, MAE, RMSE, MBE (+ intervals) and the error tail, per unit, group and model
    contrasts.csv       paired gains between models with their intervals (RMSE and MAE)
    overfit.csv         RMSE on the training rows, in cross-validation and on the scored rows
    selected_configs.csv, config_stability.csv   the hyperparameters chosen in each run
    fitted_params.csv, importances.csv           the a and b of the formulas, XGBoost importances

and <results>/summary.md, a plain-text reading of the main variant.

A *unit* is what a table row summarises: E1; E3; for E2 each held-out sensor and the groups fixed in
advance (shaded, open sky, tilted); for E4 each held-out month and all of them pooled. Nothing is
trained here; running it again changes nothing but the bootstrap seed, which is fixed.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import pandas as pd

from . import metrics as mt
from . import models as M
from .common import LABELS, OPEN_SENSORS, SHADED_SENSORS, TILTED_SENSORS

RESULTS = Path(__file__).resolve().parents[1] / "results"
SMOKE_RESULTS = Path(__file__).resolve().parents[1] / "results_smoke"
EXPECTED_RUNS = 20
E2_GROUPS = {"shaded": SHADED_SENSORS, "open": OPEN_SENSORS, "tilted": TILTED_SENSORS}
ALL_MODELS = M.ALL_MODEL_IDS


# --------------------------------------------------------------------------- loading ---

def load_variant(results: Path, variant: str, allow_partial: bool = False) -> dict[str, tuple[pd.DataFrame, dict]]:
    """{run id: (predictions, fitted.json)} for the finished runs of a variant."""
    runs = {}
    for d in sorted((results / variant).glob("*")):
        if (d / "done.json").exists():
            fit = json.loads((d / "fitted.json").read_text())
            runs[fit["run"]] = (pd.read_csv(d / "predictions.csv"), fit)
    if not runs:
        raise FileNotFoundError(f"no finished runs for variant '{variant}' in {results}")
    if len(runs) != EXPECTED_RUNS and not allow_partial:
        raise RuntimeError(f"variant '{variant}' has {len(runs)} finished runs, expected {EXPECTED_RUNS}; "
                           "finish run.py or pass --allow-partial")
    return runs


# ----------------------------------------------------------------------------- units ---

def _concat(runs: dict, prefix: str) -> pd.DataFrame:
    return pd.concat([df for rid, (df, _) in runs.items() if rid == prefix or rid.startswith(prefix + ":")], ignore_index=True)


def make_units(runs: dict) -> list[tuple[str, pd.DataFrame, list[str] | None]]:
    """(name, scored rows, groups to report) for every unit of the protocol; groups None = all.
    Units whose runs are not finished are left out (only possible with --allow-partial)."""
    units = [(r, runs[r][0], None) for r in ("E1", "E3") if r in runs]
    e2_runs = sorted(r for r in runs if r.startswith("E2:"))
    units += [(r, runs[r][0], ["overall"]) for r in e2_runs]
    if e2_runs:
        e2 = _concat(runs, "E2")
        units += [(f"E2:{name}", e2[e2["sensor"].isin(sensors)].reset_index(drop=True), None)
                  for name, sensors in E2_GROUPS.items() if e2["sensor"].isin(sensors).any()]
    e4_runs = sorted(r for r in runs if r.startswith("E4:"))
    units += [(r, runs[r][0], ["overall"]) for r in e4_runs]
    if e4_runs:
        units.append(("E4:pooled", _concat(runs, "E4"), None))
    return units


def evaluate_units(units, models: list[str], n_boot: int, seed: int) -> tuple[pd.DataFrame, pd.DataFrame]:
    met, con = [], []
    for name, df, groups in units:
        present = [m for m in models if m in df.columns]
        m, c = mt.summarize(df, present, n_boot=n_boot, seed=seed, groups=groups)
        for out, t in ((met, m), (con, c)):
            if len(t):
                out.append(t.assign(unit=name, experiment=name.split(":")[0]))
    return pd.concat(met, ignore_index=True), (pd.concat(con, ignore_index=True) if con else pd.DataFrame())


# ------------------------------------------------------------ run-level bookkeeping ---

def overfit_table(runs: dict) -> pd.DataFrame:
    rows = []
    for rid, (df, fit) in runs.items():
        for m, tr in fit["train_rmse"].items():
            score = mt.point_metrics(df["real_wm2"], df[m])["rmse"] if df[m].notna().all() else np.nan
            rows.append({"run": rid, "experiment": fit["experiment"], "model": m, "train_rmse": tr,
                         "cv_rmse": fit["cv_rmse"].get(m, np.nan), "score_rmse": score})
    return pd.DataFrame(rows)


def config_tables(runs: dict) -> tuple[pd.DataFrame, pd.DataFrame]:
    rows = []
    for rid, (_, fit) in runs.items():
        for fam in ("hd", "c"):
            cfg = fit["configs"][fam]
            if cfg:
                rows.append({"run": rid, "experiment": fit["experiment"], "family": "H+D" if fam == "hd" else "C", **cfg})
    chosen = pd.DataFrame(rows)
    keys = ["max_depth", "n_estimators", "min_child_weight", "colsample_bytree"]
    stability = (chosen.groupby(["family", *keys]).size().rename("n_runs").reset_index()
                 .sort_values(["family", "n_runs"], ascending=[True, False]))
    return chosen, stability


def fitted_tables(runs: dict) -> tuple[pd.DataFrame, pd.DataFrame]:
    params, imps = [], []
    for rid, (_, fit) in runs.items():
        for m, p in fit["models"].items():
            if "a" in p:
                params.append({"run": rid, "model": m, "a": p["a"], "b": p["b"]})
            for feat, gain in p.get("feature_importances", {}).items():
                imps.append({"run": rid, "model": m, "feature": feat, "gain": gain})
    return pd.DataFrame(params), pd.DataFrame(imps)


def evaluate_variant(results: Path, variant: str, n_boot: int = mt.N_BOOT, seed: int = mt.SEED,
                     allow_partial: bool = False) -> dict[str, pd.DataFrame]:
    runs = load_variant(results, variant, allow_partial)
    models = [m for m in ALL_MODELS if any(m in df.columns for df, _ in runs.values())]
    metrics, contrasts = evaluate_units(make_units(runs), models, n_boot, seed)
    chosen, stability = config_tables(runs)
    params, imps = fitted_tables(runs)
    return {"metrics": metrics, "contrasts": contrasts, "overfit": overfit_table(runs),
            "selected_configs": chosen, "config_stability": stability, "fitted_params": params, "importances": imps}


# ---------------------------------------------------------------------------- summary ---

def _md(df: pd.DataFrame) -> str:
    head = "| " + " | ".join(map(str, df.columns)) + " |\n|" + "---|" * len(df.columns) + "\n"
    return head + "\n".join("| " + " | ".join(map(str, r)) + " |" for r in df.itertuples(index=False)) + "\n"


def _name(m: str) -> str:
    return f"{m} {LABELS[m]}"


def _gain(con: pd.DataFrame, unit: str, group: str, a: str, b: str, metric: str = "rmse") -> pd.Series | None:
    r = con[(con.unit == unit) & (con.group == group) & (con.a == a) & (con.b == b) & (con.metric == metric)]
    return r.iloc[0] if len(r) else None


def _gain_text(r: pd.Series | None) -> str:
    if r is None:
        return "not applicable"
    return f"{r.gain:+.2f} W/m² (95 % interval {r.lo:+.2f} to {r.hi:+.2f}; {int(r.n_rows)} rows, {int(r.n_days)} days)"


def _rmse_table(met: pd.DataFrame, unit: str, group: str) -> pd.DataFrame:
    t = met[(met.unit == unit) & (met.group == group)]
    t = t.set_index("model").reindex([m for m in ALL_MODELS if m in set(t.model)])
    return pd.DataFrame({"model": [_name(m) for m in t.index],
                         "RMSE": [f"{v:.1f} [{lo:.1f}, {hi:.1f}]" for v, lo, hi in zip(t.rmse, t.rmse_lo, t.rmse_hi)],
                         "MAE": t.mae.round(1).to_numpy(), "MBE": t.mbe.round(1).to_numpy(), "R²": t.r2.round(3).to_numpy()})


def _wide_rmse(met: pd.DataFrame, index_col: str, order: list[str]) -> pd.DataFrame:
    """One row per ``index_col`` value, one column per model; 'n/a' where a model is not scored."""
    piv = met.pivot(index=index_col, columns="model", values="rmse").reindex(order)
    piv = piv.reindex(columns=[m for m in ALL_MODELS if m in set(met.model)]).round(1)
    return piv.astype(object).where(piv.notna(), "n/a").reset_index()


def build_summary(tables: dict[str, dict[str, pd.DataFrame]], manifest: dict | None) -> str:
    main = tables["main"]
    met, con, over = main["metrics"], main["contrasts"], main["overfit"]
    out = ["# Evaluation summary", ""]
    if manifest:
        out += [f"Protocol tag `{manifest.get('tag')}`, verified against it: **{manifest.get('tag_verified')}**. "
                f"Synthetic smoke run: **{manifest.get('smoke')}**. Code commit `{str(manifest.get('git_commit'))[:12]}`.", ""]
    out += ["All results are exploratory. A contrast is the gain of the first model over the second, `RMSE(second) - RMSE(first)`, "
            "positive when the first is better. Intervals are 95 % percentile intervals of a block bootstrap over days "
            "(2000 resamples, seed 42) and are not adjusted for the number of comparisons.", ""]

    out += ["## Main comparison: H against D in E1", "",
            f"- All nine sensors pooled: {_gain_text(_gain(con, 'E1', 'overall', 'H', 'D'))}",
            f"- Horizontal sensors only (Pinc excluded): {_gain_text(_gain(con, 'E1', 'horizontals', 'H', 'D'))}", "",
            "The nine-sensor figure includes Pinc, so it reflects the handling of orientation as well as of shadows; "
            "the horizontals-only figure separates the two.", ""]

    out += ["## E1 on the test days: all models", "", "RMSE in W/m² with its interval; S is not scored on Pinc, so it is not applicable in the pooled table.", "",
            "### All nine sensors", "", _md(_rmse_table(met, "E1", "overall")),
            "### Horizontal sensors only", "", _md(_rmse_table(met, "E1", "horizontals"))]

    pairs = [("A", "B"), ("A", "R"), ("R", "B"), ("H", "A"), ("P-sim", "P-cs"), ("H", "C"), ("H", "P-sim"), ("H", "S")]
    rows = [{"contrast": f"{a} over {b}", "all nine sensors": _gain_text(_gain(con, "E1", "overall", a, b)),
             "horizontals only": _gain_text(_gain(con, "E1", "horizontals", a, b))} for a, b in pairs]
    out += ["## E1 contrasts", "", _md(pd.DataFrame(rows))]

    out += ["## E1 by sky class and shade (RMSE)", ""]
    groups = [g for g in ["sky:sunny", "sky:mixed", "sky:cloudy", "sky:rainy", "sky:cloudy+rainy", "shade:unshaded", "shade:shaded"]
              if g in set(met[met.unit == "E1"].group)]
    wide = _wide_rmse(met[(met.unit == "E1") & met.group.isin(groups)], "group", groups)
    out += [_md(wide)]

    out += ["## E2: an unseen sensor (RMSE by group fixed in advance)", "", "C is not applicable in E2 (its dummy for an unseen sensor is undefined). Pinc is an orientation extrapolation.", ""]
    e2 = met[met.unit.isin([f"E2:{g}" for g in E2_GROUPS]) & (met.group == "overall")]
    out += [_md(_wide_rmse(e2, "unit", [f"E2:{g}" for g in E2_GROUPS]))]

    out += ["## E3: shade rows after training without simulated occlusion", "", _md(_rmse_table(met, "E3", "overall")),
            "## E4: an unseen month (all nine months pooled)", "", _md(_rmse_table(met, "E4:pooled", "overall"))]

    h = over[(over.experiment == "E1") & over.model.isin(["H", "D", "C"])].round(2)
    out += ["## Overfitting diagnostic (E1)", "", "RMSE on the model's own training rows, in cross-validation inside the training days, and on the scored rows.", "",
            _md(h[["model", "train_rmse", "cv_rmse", "score_rmse"]])]

    stab = main["config_stability"]
    out += ["## Hyperparameters chosen across the runs", "", "How many of the runs chose each configuration.", "", _md(stab.reset_index(drop=True))]

    out += ["## Sensitivities (H against D in E1)", ""]
    rows = []
    for v, label in (("main", "main (altitude > 5°, clip 2.0)"), ("alt10", "altitude > 10°"), ("clip3", "upper clip 3.0"), ("clipnone", "no upper clip")):
        if v in tables:
            c = tables[v]["contrasts"]
            rows.append({"variant": label, "all nine sensors": _gain_text(_gain(c, "E1", "overall", "H", "D")),
                         "horizontals only": _gain_text(_gain(c, "E1", "horizontals", "H", "D"))})
    out += [_md(pd.DataFrame(rows))]
    if "sky:cloudy+rainy" in set(met[met.unit == "E1"].group):
        out += ["Cloudy and rainy pooled (E1, RMSE):", "", _md(_rmse_table(met, "E1", "sky:cloudy+rainy"))]
    return "\n".join(out)


# ------------------------------------------------------------------------------ main ---

def write_all(results: Path, n_boot: int = mt.N_BOOT, allow_partial: bool = False, log=print) -> dict:
    tables = {}
    for v in ("main", "alt10", "clip3", "clipnone"):
        if not (results / v).exists():
            continue
        log(f"evaluating {v} ...")
        tables[v] = evaluate_variant(results, v, n_boot=n_boot, allow_partial=allow_partial)
        out = results / "tables" / v
        out.mkdir(parents=True, exist_ok=True)
        for name, t in tables[v].items():
            t.to_csv(out / f"{name}.csv", index=False)
    if "main" not in tables:
        raise FileNotFoundError(f"no main variant in {results}: run run.py first")
    manifest = json.loads((results / "manifest.json").read_text()) if (results / "manifest.json").exists() else None
    (results / "summary.md").write_text(build_summary(tables, manifest))
    return tables


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Stage 2: metrics, intervals and tables from the saved predictions.")
    ap.add_argument("--smoke", action="store_true", help="read results_smoke instead of results")
    ap.add_argument("--results-dir", type=Path)
    ap.add_argument("--n-boot", type=int, default=mt.N_BOOT)
    ap.add_argument("--allow-partial", action="store_true", help="evaluate variants with fewer than 20 finished runs")
    a = ap.parse_args(argv)
    results = a.results_dir or (SMOKE_RESULTS if a.smoke else RESULTS)
    write_all(results, a.n_boot, a.allow_partial)
    print(f"wrote {results / 'summary.md'} and {results / 'tables'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
