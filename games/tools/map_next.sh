#!/usr/bin/env bash
# One mapping tick: progress + worklist, then the packet for the top target.
# Usage: games/tools/map_next.sh [--top N] [--boundary] [ADDR]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
ARGS=(); TARGET=""
for x in "$@"; do case "$x" in 0x*) TARGET="$x";; *) ARGS+=("$x");; esac; done
OUT=$(python3 games/tools/worklist.py "${ARGS[@]}"); echo "$OUT"
[[ -n "$TARGET" ]] || TARGET=$(echo "$OUT" | awk '/^NEXT/{print "0x"$2}')
[[ -n "$TARGET" ]] && { echo; python3 games/tools/packet.py "$TARGET"; }
