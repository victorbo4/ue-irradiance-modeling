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

# Evaluation stage 1: train and predict for every run of the protocol (about an hour; resumable).
run *ARGS:
    cd analysis && .venv/bin/python -m evaluation.run {{ARGS}}

# The same pipeline on synthetic data and tiny grids, in seconds. Writes to analysis/results_smoke.
run-smoke:
    cd analysis && .venv/bin/python -m evaluation.run --smoke

# Run the evaluation package tests.
test-evaluation:
    analysis/.venv/bin/python -m pytest analysis/evaluation/tests -q

# Evaluation stage 2: metrics, intervals and tables from the saved predictions (minutes).
evaluate *ARGS:
    cd analysis && .venv/bin/python -m evaluation.evaluate {{ARGS}}

# Add the S-geo column to the saved predictions (no models are refitted; see protocol/ADDENDUM-1.md).
posthoc *ARGS:
    cd analysis && .venv/bin/python -m evaluation.posthoc {{ARGS}}

# Stage 2 on the synthetic smoke results.
evaluate-smoke:
    cd analysis && .venv/bin/python -m evaluation.evaluate --smoke
