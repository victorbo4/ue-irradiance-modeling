"""Constants shared by the evaluation modules. They mirror analysis/protocol/PROTOCOL.md; if
the two ever disagree, the protocol wins and this file is the bug."""
from __future__ import annotations

SENSORS = ["P0", "P1", "P3", "P4", "P5", "P6", "P7", "P8", "Pinc"]
HORIZONTAL_SENSORS = [s for s in SENSORS if s != "Pinc"]

# Sensor groups for the E2 summary, fixed in advance (protocol section 3).
SHADED_SENSORS = ["P1", "P3", "P5", "P7"]
OPEN_SENSORS = ["P0", "P4", "P6", "P8"]
TILTED_SENSORS = ["Pinc"]

MIN_ALTITUDE_DEG = 5.0          # common evaluation mask (protocol section 1)
ROBUSTNESS_ALTITUDE_DEG = 10.0  # sensitivity 1

TARGET = "real_wm2"
