#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"

preset_name=$1

valgrind --tool=memcheck \
  --gen-suppressions=all \
  --leak-check=full \
  --show-leak-kinds=all \
  --leak-resolution=med \
  --track-origins=yes \
  --vgdb=no \
  --error-exitcode=1 \
  --suppressions="${SCRIPT_DIR}/valgrind.suppressions" \
  "build/${preset_name}/test/tests"
