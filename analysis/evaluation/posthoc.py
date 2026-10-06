"""Add the S-geo column to predictions that were saved before S-geo existed.

    python -m evaluation.posthoc [--results-dir DIR]

S-geo has nothing to fit, so it does not need the hour-long run: it is computed from the dataset
columns for the rows each saved run already scored, and written as one more column of that run's
predictions.csv. The text of the columns already there is not touched (the new value is appended to each
line), and running it again changes nothing. Only the variants that hold all the models get the column (main and alt10); the
two clip variants refit just H and D. See analysis/protocol/ADDENDUM-1.md.
"""
from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
import pandas as pd

from . import models as M

REPO = Path(__file__).resolve().parents[2]
NEEDED = ["utc", "sensor", "solcast_dni_wm2", "solcast_dhi_wm2", "geometric_factor", "sky_view_factor"]
VARIANTS = ("main", "alt10")


def add_s_geo(results: Path, dataset: pd.DataFrame, variants=VARIANTS) -> dict:
    """Write the S-geo column into every finished run of the variants. Returns what was done."""
    ds = dataset[NEEDED]
    if ds.duplicated(["utc", "sensor"]).any():
        raise ValueError("the dataset has duplicated (utc, sensor) rows")
    done = {"added": 0, "unchanged": 0}
    for v in variants:
        for d in sorted((results / v).glob("*")):
            if not (d / "done.json").exists():
                continue
            path = d / "predictions.csv"
            pred = pd.read_csv(path)
            merged = pred[["utc", "sensor"]].merge(ds, on=["utc", "sensor"], how="left", validate="many_to_one")
            if len(merged) != len(pred) or merged[NEEDED[2:]].isna().any().any():
                raise ValueError(f"{path}: rows without the Solcast or geometry columns in the dataset")
            s_geo = np.maximum(M.solcast_geometry(merged).to_numpy(float), 0.0)
            if "S-geo" in pred:
                if not np.allclose(pred["S-geo"].to_numpy(float), s_geo, rtol=1e-9, atol=1e-9):
                    raise ValueError(f"{path}: an S-geo column is already there and differs from the formula")
                done["unchanged"] += 1
                continue
            # Append the column to the text of the file. Going through pandas would re-parse and re-print
            # the other columns, which can move the last digit of a float.
            lines = path.read_text().splitlines()
            if len(lines) - 1 != len(pred):
                raise ValueError(f"{path}: unexpected number of lines")
            rows = [f"{line},{value!r}" for line, value in zip(lines[1:], s_geo.tolist())]
            path.write_text("\n".join([lines[0] + ",S-geo", *rows]) + "\n")
            done["added"] += 1
    return done


def record_in_manifest(results: Path, done: dict) -> None:
    path = results / "manifest.json"
    if not path.exists():
        return
    m = json.loads(path.read_text())
    m["posthoc"] = {"S-geo": {"added": datetime.now(timezone.utc).isoformat(), "runs_written": done["added"],
                              "runs_already_had_it": done["unchanged"], "note": "analysis/protocol/ADDENDUM-1.md"}}
    path.write_text(json.dumps(m, indent=2))


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Add the S-geo column to the saved predictions.")
    ap.add_argument("--results-dir", type=Path, default=REPO / "analysis" / "results")
    ap.add_argument("--dataset", type=Path, default=REPO / "analysis" / "data" / "dataset_v2.csv")
    a = ap.parse_args(argv)
    done = add_s_geo(a.results_dir, pd.read_csv(a.dataset, usecols=NEEDED, low_memory=False))
    record_in_manifest(a.results_dir, done)
    print(f"S-geo: written in {done['added']} runs, already present and identical in {done['unchanged']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
