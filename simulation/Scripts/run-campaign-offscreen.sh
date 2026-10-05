#!/usr/bin/env bash
# Runs every plan in Pyrano_Plans/campaign/ sequentially, offscreen, waiting
# for "[Scheduler] Simulation completed" in the log before moving to the next.
# Safe to Ctrl+C and re-run later: plans with a completed log are skipped.
set -uo pipefail

ENGINE="${UE_ROOT:-$HOME/UnrealEngine/5.6}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LEVEL="${LEVEL:-/Game/Azotea_ETSIDI/Levels/Azotea_ETSIDI}"
PLANS_DIR="$PROJECT_DIR/Saved/Irradiance/Pyrano_Plans/campaign"
LOG_DIR="$PROJECT_DIR/Saved/Irradiance/campaign_logs"
RESULTS_DIR="$PROJECT_DIR/Saved/Irradiance/campaign_output"
TIMEOUT_SEC="${TIMEOUT_SEC:-10800}"  # per-plan safety cap (3h -- a full day at 10 sensors needs ~80min, leave margin for the longest summer days)

mkdir -p "$LOG_DIR" "$RESULTS_DIR"

shopt -s nullglob
PLANS=("$PLANS_DIR"/*.json)
shopt -u nullglob

if [[ ${#PLANS[@]} -eq 0 ]]; then
    echo "No hay planes en $PLANS_DIR" >&2
    exit 1
fi

for PLAN in "${PLANS[@]}"; do
    NAME="$(basename "$PLAN" .json)"
    LOGFILE="$LOG_DIR/$NAME.log"

    if grep -q "Simulation completed" "$LOGFILE" 2>/dev/null; then
        echo "[$NAME] ya completado, salto."
        continue
    fi

    echo "[$NAME] lanzando..."
    stdbuf -oL -eL \
    env UE_ROOT="$ENGINE" "$PROJECT_DIR/Scripts/run-editor.sh" "$LEVEL" \
        -RenderOffscreen \
        -ExecCmds="Pyrano.RunPlan $PLAN" \
        -Unattended -log -FORCELOGFLUSH \
        > "$LOGFILE" 2>&1 &
    PID=$!

    START=$(date +%s)
    DONE=0
    while kill -0 "$PID" 2>/dev/null; do
        if grep -q "Simulation completed" "$LOGFILE" 2>/dev/null; then
            DONE=1
            break
        fi
        if (( $(date +%s) - START > TIMEOUT_SEC )); then
            echo "[$NAME] TIMEOUT tras ${TIMEOUT_SEC}s, matando proceso."
            break
        fi
        sleep 5
    done

    # deja un margen para que el export a CSV/EXR termine de volcar a disco
    sleep 5
    kill "$PID" 2>/dev/null
    wait "$PID" 2>/dev/null

    if [[ "$DONE" == "1" ]]; then
        # The exporter names its CSV "irradiance_<timestamp>.csv" regardless of
        # which plan produced it (hardcoded in IrradianceExporter.h, not wired
        # to the plan) -- copy it under the plan's own name so it's identifiable.
        SRC_CSV="$(grep -a -o -e "-> '[^']*\.csv'" "$LOGFILE" | tail -1 | sed "s/^-> '//; s/'$//")"
        if [[ -n "$SRC_CSV" && -f "$SRC_CSV" ]]; then
            cp "$SRC_CSV" "$RESULTS_DIR/$NAME.csv"
            echo "[$NAME] OK -> $RESULTS_DIR/$NAME.csv"
        else
            echo "[$NAME] OK (aviso: no encontré el CSV de origen en el log para renombrarlo)"
        fi
    else
        echo "[$NAME] FALLO o timeout -- revisa $LOGFILE"
    fi
done

echo "Campana terminada."
