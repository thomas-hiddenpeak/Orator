#!/usr/bin/env bash
# Sets up the environment so the Jetson torch 2.11 (cu12) wheel can load on a
# CUDA-13 system using the pip-installed cu12 runtime wheels + NVPL + system cuDNN.
# Usage: source tools/torchenv.sh
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/.venv/bin/activate"
SP="$HERE/.venv/lib/python3.12/site-packages"
WHEEL_LIBS="$(find "$SP/nvidia" -name 'lib' -type d 2>/dev/null | tr '\n' ':')"
export LD_LIBRARY_PATH="${HERE}/extralibs:${WHEEL_LIBS}${SP}/nvpl/lib:/usr/lib/aarch64-linux-gnu:/usr/local/cuda-13.3/targets/sbsa-linux/lib:${LD_LIBRARY_PATH}"
