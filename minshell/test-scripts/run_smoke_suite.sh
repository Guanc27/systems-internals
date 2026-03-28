#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHELL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR/out"

mkdir -p "$OUT_DIR"

for batch in "$SHELL_DIR"/batch-files/*.batch; do
    name="$(basename "$batch" .batch)"
    echo "running $name"
    stderr_tmp="$OUT_DIR/$name.stderr.tmp"
    timeout 5 "$SHELL_DIR/myshell" "$batch" > "$OUT_DIR/$name.stdout" 2> "$stderr_tmp" || true
    if [ -s "$stderr_tmp" ]; then
        mv "$stderr_tmp" "$OUT_DIR/$name.stderr"
    else
        rm -f "$stderr_tmp"
    fi
done

echo "smoke suite complete: outputs in $OUT_DIR"
