#!/usr/bin/env bash
set -euo pipefail

preset_name=$1

# Configure CMake
cmake -S . \
  --preset "${preset_name}" -G Ninja \
  -D CT_TREAT_WARNINGS_AS_ERRORS=ON

# Build
cmake --build "build/${preset_name}" -j
