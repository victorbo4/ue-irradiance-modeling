"""Stage 1 of the evaluation: train and predict (protocol sections 2, 3 and 5).

    python -m evaluation.run [--smoke] [--only E1,E2:P1] [--variants main,alt10,clip3,clipnone]

For every run of E1-E4 (experiments.py): choose the hyperparameters with that run's own training
rows (selection.py), train the models (models.py), predict the rows to score and save everything.
Metrics, intervals and tables are stage 2 (evaluate.py) and read what this stage writes.

Variants (the sensitivities that need new fits; the pooled cloudy+rainy group only regroups):

    main      altitude > 5, hyperparameters selected in each run, all nine models
    alt10     altitude > 10, the models retrained with the configurations the main run selected
    clip3     H and D with the upper clip of k at 3.0, same configurations
    clipnone  H and D with no upper clip, same configurations

Layout:  <results>/<variant>/<run id>/{predictions.csv, fitted.json, selection_hd.csv,
selection_c.csv, done.json}  and  <results>/manifest.json.  A run with a done.json is skipped, so
an interrupted job resumes where it stopped; done.json is the last file written.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
import pandas as pd

from . import experiments as ex
from . import models as M
from . import selection as sel
from .common import TARGET

REPO = Path(__file__).resolve().parents[2]
DATA = REPO / "analysis" / "data"
RESULTS = REPO / "analysis" / "results"
SMOKE_RESULTS = REPO / "analysis" / "results_smoke"
TAG = "protocol-v1"

NEEDED = ["utc", "date_local", "sensor", "qc_ok", "sun_altitude_deg", "sun_azimuth_deg", "sun_visibility",
          "clearsky_ghi_wm2", "clearsky_dni_wm2", "clearsky_dhi_wm2", "geometric_factor", "sky_view_factor",
          "sim_irradiance_wm2", "solcast_ghi_wm2", "cloud_opacity", "precipitable_water", TARGET]
KEEP = ["utc", "date_local", "sensor", "sky", "split", "sun_altitude_deg", "sun_azimuth_deg", "sun_visibility",
        "cloud_opacity", "sim_irradiance_wm2", "clearsky_ghi_wm2", TARGET]
ALL_MODELS = ["A", "B", "S", "R", "P-cs", "P-sim", "D", "H", "C"]


@dataclass(frozen=True)
class Variant:
    name: str
    altitude_min: float
    models: tuple
    k_max: float | None
    reuse_main_configs: bool


VARIANTS = {
    "main": Variant("main", 5.0, tuple(ALL_MODELS), M.K_MAX, False),
    "alt10": Variant("alt10", 10.0, tuple(ALL_MODELS), M.K_MAX, True),
    "clip3": Variant("clip3", 5.0, ("D", "H"), 3.0, True),
    "clipnone": Variant("clipnone", 5.0, ("D", "H"), None, True),
}


# ------------------------------------------------------------- the protocol tag ---

def parse_tag_hashes(message: str) -> dict[str, str]:
    """The ``path  sha256`` lines of the protocol-v1 tag message."""
    return {m.group(1): m.group(2) for m in re.finditer(r"^\s+(analysis/\S+)\s+([0-9a-f]{64})\s*$", message, re.M)}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def verify_hashes(expected: dict[str, str], root: Path) -> dict[str, bool]:
    return {p: (root / p).exists() and sha256_file(root / p) == h for p, h in expected.items()}


def verify_protocol_tag(root: Path = REPO, tag: str = TAG) -> dict[str, bool]:
    """Compare the files on disk with the hashes recorded in the tag. Raises if the tag is missing
    or any file differs: a run must not start on data that is not the data the protocol froze."""
    try:
        message = subprocess.check_output(["git", "-C", str(root), "cat-file", "-p", tag], text=True, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"tag {tag} not found; create it first, or pass --allow-untagged for a development run") from e
    expected = parse_tag_hashes(message)
    if not expected:
        raise RuntimeError(f"tag {tag} lists no file hashes")
    result = verify_hashes(expected, root)
    bad = [p for p, ok in result.items() if not ok]
    if bad:
        raise RuntimeError(f"files differ from the hashes in {tag}: {bad}")
    return result


# ---------------------------------------------------------------------- one run ---

def _rmse(pred, real) -> float:
    return float(np.sqrt(np.mean((np.asarray(pred, float) - np.asarray(real, float)) ** 2)))


def _json_default(o):
    if isinstance(o, (np.integer,)):
        return int(o)
    if isinstance(o, (np.floating,)):
        return float(o)
    if isinstance(o, np.ndarray):
        return o.tolist()
    raise TypeError(type(o))


def _write_json(path: Path, obj) -> None:
    path.write_text(json.dumps(obj, indent=2, default=_json_default))


def _configs_from_main(results: Path, run_id: str) -> tuple[M.XGBConfig, M.XGBConfig | None]:
    path = results / "main" / run_id.replace(":", "_") / "fitted.json"
    if not path.exists():
        raise FileNotFoundError(f"{path} is missing: run the main variant first")
    cfg = json.loads(path.read_text())["configs"]
    return M.XGBConfig(**cfg["hd"]), (M.XGBConfig(**cfg["c"]) if cfg["c"] else None)


def run_one(run: ex.Run, prepared: pd.DataFrame, variant: Variant, out: Path, results: Path,
            grid_hd, grid_c, n_jobs: int) -> None:
    t0 = time.time()
    out.mkdir(parents=True, exist_ok=True)
    train, score = prepared.iloc[run.train], prepared.iloc[run.score]
    if len(score) == 0:
        raise ValueError(f"{run.id} ({variant.name}) has no rows to score")
    wants_c = "C" in variant.models and "C" not in run.skip_models

    selection_info, cv_rmse = {}, {}
    if variant.reuse_main_configs:
        hd_cfg, c_cfg = _configs_from_main(results, run.id)
    else:
        s_hd = sel.select_hd(train, grid=grid_hd, k_max=variant.k_max, n_jobs=n_jobs)
        hd_cfg = s_hd.config
        s_hd.table.to_csv(out / "selection_hd.csv", index=False)
        selection_info["hd"] = {"index": s_hd.index, "best_index": s_hd.best_index, "threshold": s_hd.threshold, "months": s_hd.months}
        cv_rmse.update({m: float(v[s_hd.index].mean()) for m, v in s_hd.fold_rmse.items()})
        c_cfg = None
        if wants_c:
            s_c = sel.select_c(train, grid=grid_c, n_jobs=n_jobs)
            c_cfg = s_c.config
            s_c.table.to_csv(out / "selection_c.csv", index=False)
            selection_info["c"] = {"index": s_c.index, "best_index": s_c.best_index, "threshold": s_c.threshold, "months": s_c.months}
            cv_rmse.update({m: float(v[s_c.index].mean()) for m, v in s_c.fold_rmse.items()})

    built = M.build_models(hd_cfg, c_cfg or hd_cfg, variant.k_max, n_jobs=1)
    pred = score[KEEP].reset_index(drop=True)
    train_rmse, fitted = {}, {}
    for mid in variant.models:
        if mid in run.skip_models:
            pred[mid] = np.nan
            continue
        model = built[mid].fit(train)
        pred[mid] = model.predict(score)
        if model.learned:
            train_rmse[mid] = _rmse(model.predict(train), train[TARGET])
        fitted[mid] = model.fitted_params()
    pred = ex.apply_scoring_rules(pred, run, list(variant.models))
    pred.to_csv(out / "predictions.csv", index=False)

    _write_json(out / "fitted.json", {
        "run": run.id, "experiment": run.experiment, "variant": variant.name, "held_out": run.held_out,
        "altitude_min": variant.altitude_min, "k_max": variant.k_max,
        "n_train": len(train), "n_score": len(score),
        "train_days": int(train["date_local"].nunique()), "score_days": int(score["date_local"].nunique()),
        "configs": {"hd": vars(hd_cfg), "c": vars(c_cfg) if c_cfg else None},
        "selection": selection_info, "cv_rmse": cv_rmse, "train_rmse": train_rmse, "models": fitted})
    _write_json(out / "done.json", {"finished": datetime.now(timezone.utc).isoformat(), "seconds": round(time.time() - t0, 1)})


# --------------------------------------------------------------------- all the runs ---

def _selected(run: ex.Run, only: list[str] | None) -> bool:
    return not only or any(run.id == o or run.experiment == o for o in only)


def run_all(df: pd.DataFrame, split: pd.DataFrame, results: Path, variants: list[str] | None = None,
            only: list[str] | None = None, force: bool = False, n_jobs: int = 1,
            grid_hd=None, grid_c=None, log=print) -> dict:
    """Run the requested variants; returns {variant: [run ids executed in this call]}."""
    names = variants or list(VARIANTS)
    executed = {}
    for name in names:
        v = VARIANTS[name]
        prepared = ex.prepare(df, split, v.altitude_min)
        runs = [r for r in ex.build_runs(prepared) if _selected(r, only)]
        executed[name] = []
        for r in runs:
            out = results / name / r.id.replace(":", "_")
            if (out / "done.json").exists() and not force:
                log(f"[{name}] {r.id}: already done, skipped")
                continue
            if (out / "done.json").exists():
                (out / "done.json").unlink()
            t = time.time()
            run_one(r, prepared, v, out, results, grid_hd, grid_c, n_jobs)
            executed[name].append(r.id)
            log(f"[{name}] {r.id}: train {len(r.train)} / score {len(r.score)} rows, {time.time() - t:.0f}s")
    return executed


# ----------------------------------------------------------------------- manifest ---

def _git(args: list[str]) -> str:
    try:
        return subprocess.check_output(["git", "-C", str(REPO), *args], text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return "unknown"


def write_manifest(results: Path, variants: list[str], smoke: bool, tag_verified: bool, argv: list[str]) -> None:
    import platform
    import scipy
    import xgboost
    path = results / "manifest.json"
    old = json.loads(path.read_text()) if path.exists() else {}
    _write_json(path, {
        "smoke": smoke, "tag": TAG, "tag_verified": tag_verified,
        "git_commit": _git(["rev-parse", "HEAD"]),
        "evaluation_code_uncommitted_changes": bool(_git(["status", "--porcelain", "--", "analysis/evaluation"])),
        "finished": datetime.now(timezone.utc).isoformat(), "argv": argv,
        "variants": sorted(set(old.get("variants", [])) | set(variants)),
        "versions": {"python": platform.python_version(), "numpy": np.__version__, "pandas": pd.__version__,
                     "scipy": scipy.__version__, "xgboost": xgboost.__version__},
    })


# --------------------------------------------------------------------------- cli ---

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Stage 1: train and predict for every run of the protocol.")
    ap.add_argument("--smoke", action="store_true", help="synthetic data and tiny grids; writes to results_smoke")
    ap.add_argument("--only", help="comma-separated run ids or experiments, e.g. E1,E2:P1")
    ap.add_argument("--variants", default=",".join(VARIANTS), help="comma-separated, in order; main must come first")
    ap.add_argument("--results-dir", type=Path)
    ap.add_argument("--n-jobs", type=int, default=os.cpu_count() or 1)
    ap.add_argument("--force", action="store_true", help="redo runs that already have a done.json")
    ap.add_argument("--allow-untagged", action="store_true", help="skip the protocol-tag check (development only)")
    a = ap.parse_args(argv)

    variants = [v for v in a.variants.split(",") if v]
    unknown = [v for v in variants if v not in VARIANTS]
    if unknown:
        ap.error(f"unknown variant(s): {unknown}")
    only = [o for o in (a.only or "").split(",") if o] or None
    results = a.results_dir or (SMOKE_RESULTS if a.smoke else RESULTS)

    if a.smoke:
        from . import smoke
        df, split = smoke.make_dataset()
        grid_hd, grid_c, tag_verified = smoke.SMOKE_GRID_HD, smoke.SMOKE_GRID_C, False
    else:
        tag_verified = False
        if not a.allow_untagged:
            verify_protocol_tag()
            tag_verified = True
        df = pd.read_csv(DATA / "dataset_v2.csv", usecols=NEEDED, low_memory=False)
        split = pd.read_csv(DATA / "split_v2.csv", index_col=0)
        grid_hd = grid_c = None

    results.mkdir(parents=True, exist_ok=True)
    executed = run_all(df, split, results, variants, only, a.force, a.n_jobs, grid_hd, grid_c)
    write_manifest(results, variants, a.smoke, tag_verified, sys.argv[1:] if argv is None else argv)
    print(f"done: {sum(len(v) for v in executed.values())} runs executed -> {results}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
