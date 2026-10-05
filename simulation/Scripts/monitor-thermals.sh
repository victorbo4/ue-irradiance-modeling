#!/usr/bin/env bash
# Logs CPU/GPU temperature, utilization and power draw to a CSV while the
# campaign runs. Run alongside run-campaign-offscreen.sh, not instead of it.
# Usage: ./monitor-thermals.sh [output.csv] [interval_seconds]
set -euo pipefail

OUT="${1:-$HOME/campaign-thermals.csv}"
INTERVAL="${2:-30}"

echo "timestamp,cpu_pkg_c,gpu_temp_c,gpu_util_pct,gpu_power_w,gpu_power_limit_w" > "$OUT"
echo "Logging a $OUT cada ${INTERVAL}s. Ctrl+C para parar."

while true; do
    ts="$(date +%s)"
    cpu="$(sensors 2>/dev/null | grep -m1 'Package id 0' | grep -oP '\+\K[0-9.]+(?=°C)' | head -1)"
    cpu="${cpu:-NA}"
    gpu="$(nvidia-smi --query-gpu=temperature.gpu,utilization.gpu,power.draw,power.limit --format=csv,noheader,nounits 2>/dev/null)"
    gpu="${gpu:-NA, NA, NA, NA}"
    echo "${ts},${cpu},${gpu// /}" >> "$OUT"
    sleep "$INTERVAL"
done
