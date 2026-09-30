#!/usr/bin/env bash
# Run the 3210 headless with a scripted key sequence and keep every LCD frame.
# Usage: games/tools/nav_run.sh RUN_DIR "key,key,..." [SECONDS] [extra RUN_ENV...]
# Unmapped key names (e.g. "x") are harmless no-op slots, useful as pauses.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
RUN_DIR="$1"; KEYS="$2"; SECONDS_TO_RUN="${3:-16}"; shift 3 2>/dev/null || shift $#
restore_default() { make --no-print-directory eeprom-profile >/dev/null; cp "roms/noki3210/3210 v600 eeprom.bin" "mame/roms/noki3210/3210 v600 eeprom.bin"; }
trap restore_default EXIT
make --no-print-directory run PHONE=noki3210 RUN_DIR="$RUN_DIR" SECONDS="$SECONDS_TO_RUN" \
  PROVISIONED_IMEI_PREFIX=49015420323751 \
  RUN_ENV="NOKIA_DCT3_POST_READY_KEYS=$KEYS NOKIA_DCT3_POST_READY_KEY_DELAY_MS=${DELAY_MS:-3000} NOKIA_DCT3_POST_READY_KEY_DURATION_MS=${DUR_MS:-70} NOKIA_DCT3_POST_READY_KEY_GAP_MS=${GAP_MS:-430} NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS=${CAP_MS:-2000} $*" \
  RUN_EXTRA_ARGS="-window ${RUN_EXTRA_ARGS:-}" 2>&1 | grep -E "input-|Average speed|error|Error" || true
grep -h "input-press\|input-error" "$RUN_DIR/error.log" | sed 's/.*\] //' | head -40
