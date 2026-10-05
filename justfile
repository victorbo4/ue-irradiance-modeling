# Open the Pyrano editor.
editor:
    ./simulation/Scripts/run-editor.sh

# Open the editor directly on the Azotea ETSIDI level.
azotea:
    ./simulation/Scripts/run-editor.sh /Game/Azotea_ETSIDI/Levels/Azotea_ETSIDI

# Open the editor directly on the SimBlank (open-field, no buildings) level.
simblank:
    ./simulation/Scripts/run-editor.sh /Game/SimBlank/Levels/SimBlank

# Rebuild the v2 analysis dataset and its QC report.
dataset:
    analysis/.venv/bin/python analysis/pipeline/build_dataset.py
    analysis/.venv/bin/python analysis/pipeline/qc_report.py

# Run the dataset pipeline tests.
test-pipeline:
    analysis/.venv/bin/python -m pytest analysis/pipeline/tests -q

# Draw the 15 test days (writes analysis/data/split_v2.{csv,md}).
split:
    analysis/.venv/bin/python analysis/pipeline/split_days.py
