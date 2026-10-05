"""Rebuild the result tables of the TFG (chapter 9) with the results of the v2 evaluation.

Exploratory, run after ``run.py``: it only reads the saved E1 predictions and the TFG's own
prediction files, and trains nothing.

    python analysis/exploration/compare_tfg.py

For each TFG table it shows (1) the numbers as published, (2) the TFG's models recomputed from its
prediction files on the *same rows* the new models are scored on, and (3) the new models. The same rows
means: the three TFG test days, the TFG's four sensors (P0, P1, P4, Pinc), altitude > 5 degrees and
qc_ok. The TFG scored its baselines and its final model on different row sets (4208 against 5400
rows), so its published tables are not like for like.

Mapping of the models: TFG Final -> H, Baseline C -> C, Baseline B -> B, Baseline A -> A. The new
evaluation also has R, P-sim, D (and S on the horizontal sensors), shown as extra rows.
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
ANALYSIS = HERE.parent
sys.path.insert(0, str(ANALYSIS))
from evaluation.metrics import point_metrics  # noqa: E402

DAYS = ["2025-04-11", "2025-04-20", "2025-10-07"]
SENSORS = ["P0", "P1", "P4", "Pinc"]
KEY = ["utc", "sensor"]
TFG_FILES = {"A": "A_Physical_Simulation", "B": "B_clearsky", "C": "C_XGBoost_Meteo", "Final": "Final_sim_ML"}

# Published tables (R2, RMSE, MAE, MBE), copied from chapter 9 of the TFG.
PUBLISHED = {
    "9.1 global (3 days)": {"Final": (0.833, 123.8, 67.8, -0.03), "C": (0.711, 157.1, 105.1, -14.7), "B": (0.085, 279.5, 198.9, 119.7), "A": (-0.113, 308.3, 212.9, 157.4)},
    "9.2 clear sky 2025-10-07": {"Final": (0.990, 29.5, 17.5, 0.0), "A": (0.967, 44.2, 36.8, -11.4), "C": (0.821, 102.7, 78.7, -17.9), "B": (0.812, 105.1, 61.4, -48.4)},
    "9.3 variable sky 2025-04-20": {"Final": (0.687, 181.1, 120.1, -21.7), "C": (0.540, 201.6, 140.4, -0.2), "B": (0.411, 228.0, 183.7, 68.3), "A": (0.190, 267.4, 210.4, 106.0)},
    "9.4 overcast 2025-04-11": {"Final": (0.768, 111.1, 65.9, 21.6), "C": (0.644, 143.3, 91.4, -26.8), "B": (-1.836, 404.1, 334.1, 318.6), "A": (-2.476, 447.4, 368.4, 356.9)},
}
PUBLISHED_BY_SENSOR = {  # 2025-10-07
    "9.5 Baseline A per sensor": {"P0": (0.954, 45.0, 41.6, -39.2), "P1": (0.965, 42.5, 38.5, -29.0), "P4": (0.992, 17.9, 15.7, -10.5), "Pinc": (0.953, 60.3, 51.5, 33.2)},
    "9.6 Final model per sensor": {"P0": (0.990, 27.4, 17.1, -15.5), "P1": (0.992, 25.4, 15.4, -9.3), "P4": (0.997, 14.9, 7.9, 4.4), "Pinc": (0.986, 43.1, 29.5, 20.4)},
}


def _utc(ts: pd.Series) -> pd.Series:
    return pd.to_datetime(ts).dt.tz_localize("Europe/Madrid", ambiguous="NaT", nonexistent="NaT").dt.tz_convert("UTC")


def load_tfg(pred_dir: Path) -> dict[str, pd.DataFrame]:
    out = {}
    for name, stem in TFG_FILES.items():
        parts = []
        for d in DAYS:
            x = pd.read_csv(pred_dir / f"preds_{stem}_{d}.csv")
            if "timestamp" in x:
                ts, sensor = x["timestamp"], x["sensor_name"]
            else:
                ts = x["Unnamed: 0"]
                sensor = x[[f"sensor_{s}" for s in SENSORS]].idxmax(axis=1).str.replace("sensor_", "")
            parts.append(pd.DataFrame({"utc": _utc(ts), "sensor": sensor, "real": x["real_irradiance"], "pred": x["pred"], "day": d}))
        out[name] = pd.concat(parts).dropna(subset=["utc"]).reset_index(drop=True)
    return out


def load_new(results: Path) -> pd.DataFrame:
    new = pd.read_csv(results / "main" / "E1" / "predictions.csv")
    new["utc"] = pd.to_datetime(new["utc"], utc=True)
    return new[new["date_local"].isin(DAYS) & new["sensor"].isin(SENSORS)].reset_index(drop=True)


def common_rows(new: pd.DataFrame, tfg: dict) -> pd.DataFrame:
    """The new-mask rows that also exist in every TFG prediction file, with the TFG predictions attached."""
    df = new
    for k, v in tfg.items():
        df = df.merge(v[KEY + ["pred"]].rename(columns={"pred": f"TFG-{k}"}), on=KEY, how="inner")
    return df.reset_index(drop=True)


def metrics(y, p) -> tuple:
    r = point_metrics(y, p)
    return r["r2"], r["rmse"], r["mae"], r["mbe"]


def _fmt(t: tuple) -> str:
    return f"{t[0]:.3f} | {t[1]:.1f} | {t[2]:.1f} | {t[3]:+.1f}"


def check_reproduction(tfg: dict) -> list[str]:
    """Recompute the published tables from the TFG's own files (their own rows) and report any mismatch."""
    notes = []
    def own(model, day=None, sensor=None):
        v = tfg[model]
        if day:
            v = v[v.day == day]
        if sensor:
            v = v[v.sensor == sensor]
        return metrics(v.real.to_numpy(), v.pred.to_numpy())
    tables = {"9.1 global (3 days)": None, "9.2 clear sky 2025-10-07": "2025-10-07", "9.3 variable sky 2025-04-20": "2025-04-20", "9.4 overcast 2025-04-11": "2025-04-11"}
    for title, day in tables.items():
        for k, pub in PUBLISHED[title].items():
            got = own(k, day)
            ok = all(abs(a - b) <= tol for a, b, tol in zip(got, pub, (0.002, 0.15, 0.15, 0.15)))
            notes.append(f"{'ok ' if ok else 'DIFF'} {title} {k}: published {pub} | recomputed {tuple(round(x, 3) for x in got)}")
    return notes


def build_report(results: Path, pred_dir: Path) -> str:
    tfg, new = load_tfg(pred_dir), load_new(results)
    df = common_rows(new, tfg)
    new_models = [("A", "A simulation"), ("B", "B clear-sky"), ("C", "C meteo"), ("H", "H proposed (sim + cloud)"), ("R", "R ray-cast"), ("P-sim", "P-sim formula"), ("D", "D clear-sky + cloud")]
    out = ["# The TFG tables, recomputed with the v2 evaluation", "",
           "Exploratory (post-run). Same rows for every model of the 'common rows' blocks: the three TFG test days, P0, P1, P4 and Pinc, "
           f"altitude > 5 degrees and qc_ok ({len(df)} rows). The new models come from the E1 run (trained on the 43 training days, hyperparameters "
           "chosen inside the run); the TFG models are the TFG's own saved predictions (trained on 9 days; the model and its hyperparameters were chosen "
           "among about 37 variants while looking at these three days). Columns: R2 | RMSE | MAE | MBE, in W/m2.", "",
           "The TFG published each model on its own rows (4208 for A, B, C and 5400 for the final model), so the 'published' blocks are not like for like.", ""]

    def block(title, d, published=None, horizontals_only=False):
        out.append(f"### {title}")
        out.append("")
        out.append("| model | R2 | RMSE | MAE | MBE |\n|---|---|---|---|---|")
        if published:
            for k, label in (("Final", "TFG Final"), ("C", "TFG Baseline C"), ("B", "TFG Baseline B"), ("A", "TFG Baseline A")):
                if k in published:
                    out.append(f"| {label} (published, own rows) | " + _fmt(published[k]) + " |")
        for k, label in (("A", "TFG Baseline A"), ("B", "TFG Baseline B"), ("C", "TFG Baseline C"), ("Final", "TFG Final")):
            out.append(f"| {label} (recomputed, common rows) | " + _fmt(metrics(d.real_wm2.to_numpy(), d[f"TFG-{k}"].to_numpy())) + " |")
        for k, label in new_models:
            out.append(f"| **now** {label} | " + _fmt(metrics(d.real_wm2.to_numpy(), d[k].to_numpy())) + " |")
        if "S" in d and d["S"].notna().all():
            out.append("| **now** S Solcast GHI | " + _fmt(metrics(d.real_wm2.to_numpy(), d["S"].to_numpy())) + " |")
        out.extend(["", f"{len(d)} rows, {d.date_local.nunique()} day(s), sensors {sorted(d.sensor.unique())}.", ""])

    block("Table 9.1: global, the three test days and four sensors", df, PUBLISHED["9.1 global (3 days)"])
    hor = df[df.sensor != "Pinc"]
    block("Table 9.1 bis (extra): the same without Pinc, so that Solcast GHI can be scored", hor)
    for title, day in (("9.2 clear sky 2025-10-07", "2025-10-07"), ("9.3 variable sky 2025-04-20", "2025-04-20"), ("9.4 overcast 2025-04-11", "2025-04-11")):
        block(f"Table {title}, aggregated over the four sensors", df[df.date_local == day], PUBLISHED[title])
    clear = df[df.date_local == "2025-10-07"]
    for title, model, tfgcol in (("9.5 Baseline A (the simulation alone) per sensor, 2025-10-07", "A", "TFG-A"), ("9.6 Final model per sensor, 2025-10-07", "H", "TFG-Final")):
        key = "9.5 Baseline A per sensor" if model == "A" else "9.6 Final model per sensor"
        out.extend([f"### Table {title}", "", f"'now' is model {model}; B is the clear-sky formula, shown for reference.", "",
                    "| sensor | source | R2 | RMSE | MAE | MBE |", "|---|---|---|---|---|---|"])
        for s_ in SENSORS:
            g = clear[clear.sensor == s_]
            rows = [("TFG published (own rows)", PUBLISHED_BY_SENSOR[key][s_]),
                    ("TFG recomputed (common rows)", metrics(g.real_wm2.to_numpy(), g[tfgcol].to_numpy())),
                    (f"**now** {model}", metrics(g.real_wm2.to_numpy(), g[model].to_numpy())),
                    ("**now** B clear-sky", metrics(g.real_wm2.to_numpy(), g["B"].to_numpy()))]
            for label, t in rows:
                out.append(f"| {s_} | {label} | {t[0]:.3f} | {t[1]:.1f} | {t[2]:.1f} | {t[3]:+.1f} |")
        out.extend(["", f"{len(g)} rows per sensor.", ""])
    out += ["### Check: the published tables are reproduced from the TFG's own files", ""] + [f"- {n}" for n in check_reproduction(tfg)] + [""]
    return "\n".join(out)


def main() -> None:
    results = ANALYSIS / "results"
    text = build_report(results, ANALYSIS / "artifacts" / "predictions")
    (HERE / "tfg_comparison.md").write_text(text)
    print(text)


if __name__ == "__main__":
    main()
