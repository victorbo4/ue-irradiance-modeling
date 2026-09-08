#!/usr/bin/env bash
# Launch the Pyrano editor with RDG debugging + verbose render logging.
# Slow and disk-heavy (dumps the render graph) -- use only when debugging the
# irradiance capture / compute passes, never for paper simulation runs.
set -euo pipefail

ENGINE="${UE_ROOT:-$HOME/UnrealEngine/5.6}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$PROJECT_DIR/PyranoDemo.uproject"

# Same swapchain-crash mitigations as run-editor.sh.
export SDL_VIDEODRIVER=x11
export __GL_MaxFramesAllowed=1
export __GL_THREADED_OPTIMIZATIONS=0

exec "$ENGINE/Engine/Binaries/Linux/UnrealEditor" "$PROJECT" \
  -rdgdebug -rdgvalidation \
  -ExecCmds="r.RDG.Debug 1, r.RDG.Debug.NamePasses 1, r.RDG.ValidateGraphs 1, r.RDG.EmitWarnings 1, r.RDG.DumpGraph 1, r.RDG.DumpGraphTrackedResources 1, r.RDG.DumpGraphInvalidResources 1, r.RDG.DumpGraphEvents 1" \
  -LogCmds="LogRenderGraph Verbose, LogRenderer Verbose, LogRHI Verbose, LogShaders Verbose" \
  "$@"
