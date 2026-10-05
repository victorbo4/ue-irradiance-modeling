#!/usr/bin/env python3
"""Regenerate startTime/endTime in every Pyrano_Plans/campaign/*.json plan so
that the simulated window brackets a 10-minute margin around the day's real
0-degree solar-altitude crossings (morning and evening, i.e. true sunrise/
sunset), instead of the fixed per-month clock times used before. Computed
with pvlib against each plan's own lat/lon/altitude -- the timezone field
(varies 1/2 for CET/CEST across the 60 plans) is read per-file and preserved,
only startTime/endTime change.

Backs up the original plans to campaign_backup_pre_0deg_fix/ first.
Run with the analysis venv (has pvlib): analysis/.venv/bin/python3
"""
import glob
import json
import shutil
from datetime import datetime, timedelta
from pathlib import Path

import pandas as pd
import pvlib

MARGIN_MIN = 10
SAMPLE_MIN = 2
ALT_THRESHOLD_DEG = 0.0

PLANS_DIR = Path(__file__).resolve().parent.parent / "Saved/Irradiance/Pyrano_Plans/campaign"
BACKUP_DIR = PLANS_DIR.parent / "campaign_backup_pre_0deg_fix"


def find_5deg_crossings(date_utc, lat, lon, alt):
    times = pd.date_range(date_utc, date_utc + timedelta(hours=23, minutes=59), freq="1min", tz="UTC")
    solpos = pvlib.solarposition.get_solarposition(times, lat, lon, altitude=alt)
    above = solpos["apparent_elevation"] >= ALT_THRESHOLD_DEG
    if not above.any():
        raise RuntimeError(f"sun never reaches {ALT_THRESHOLD_DEG} deg on {date_utc.date()}")
    rise = above[above].index.min()
    set_ = above[above].index.max()
    return rise.to_pydatetime().replace(tzinfo=None), set_.to_pydatetime().replace(tzinfo=None)


def floor_to_grid(dt, grid_min):
    minutes = dt.hour * 60 + dt.minute
    floored = (minutes // grid_min) * grid_min
    return dt.replace(hour=0, minute=0, second=0, microsecond=0) + timedelta(minutes=floored)


def ceil_to_grid(dt, grid_min):
    minutes = dt.hour * 60 + dt.minute
    ceiled = -(-minutes // grid_min) * grid_min
    return dt.replace(hour=0, minute=0, second=0, microsecond=0) + timedelta(minutes=ceiled)


def main():
    plan_files = sorted(PLANS_DIR.glob("*.json"))
    if not plan_files:
        raise SystemExit(f"No plans found in {PLANS_DIR}")

    BACKUP_DIR.mkdir(exist_ok=True)
    for f in plan_files:
        shutil.copy2(f, BACKUP_DIR / f.name)
    print(f"Backed up {len(plan_files)} plans to {BACKUP_DIR}")

    for f in plan_files:
        cfg = json.loads(f.read_text())
        lat, lon, alt, tz = cfg["latitude"], cfg["longitude"], cfg["altitudeMeters"], cfg["timezone"]

        old_start = datetime.strptime(cfg["startTime"], "%Y.%m.%d-%H.%M.%S")
        date_utc = old_start - timedelta(hours=tz)
        date_utc = date_utc.replace(hour=0, minute=0, second=0, microsecond=0)

        rise_utc, set_utc = find_5deg_crossings(date_utc, lat, lon, alt)

        new_start_utc = floor_to_grid(rise_utc - timedelta(minutes=MARGIN_MIN), SAMPLE_MIN)
        new_end_utc = ceil_to_grid(set_utc + timedelta(minutes=MARGIN_MIN), SAMPLE_MIN)

        new_start_local = new_start_utc + timedelta(hours=tz)
        new_end_local = new_end_utc + timedelta(hours=tz)

        cfg["startTime"] = new_start_local.strftime("%Y.%m.%d-%H.%M.%S")
        cfg["endTime"] = new_end_local.strftime("%Y.%m.%d-%H.%M.%S")

        f.write_text(json.dumps(cfg, indent="\t") + "\n")

        old_end = datetime.strptime(json.loads((BACKUP_DIR / f.name).read_text())["endTime"], "%Y.%m.%d-%H.%M.%S")
        print(f"{f.stem}: [{old_start:%H:%M}-{old_end:%H:%M}] -> [{new_start_local:%H:%M}-{new_end_local:%H:%M}] local (tz={tz})")

    print(f"\nDone. {len(plan_files)} plans regenerated.")


if __name__ == "__main__":
    main()
