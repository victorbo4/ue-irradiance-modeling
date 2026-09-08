#!/usr/bin/env bash
# Launch the Pyrano editor (production profile: clean logs, no RDG debug).
#
# The NVIDIA driver + UE5 Vulkan + XWayland combo crashes in the swapchain
# (FVulkanSwapChain::AcquireImageIndex) when the window is resized / loses
# focus / another window is spawned -- e.g. Cesium's "Connect to Cesium ion"
# OAuth browser. Mitigations below; also: do NOT use "Connect to Cesium ion"
# in the editor, the ion token is set in Config/DefaultEngine.ini.
set -euo pipefail

ENGINE="${UE_ROOT:-$HOME/UnrealEngine/5.6}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$PROJECT_DIR/PyranoDemo.uproject"

# Pin the pure X11 path (no Wayland surface) and calm the NVIDIA render-ahead
# queue so swapchain recreation is less crash-prone.
export SDL_VIDEODRIVER=x11
export __GL_MaxFramesAllowed=1
export __GL_THREADED_OPTIMIZATIONS=0

exec "$ENGINE/Engine/Binaries/Linux/UnrealEditor" "$PROJECT" "$@"
