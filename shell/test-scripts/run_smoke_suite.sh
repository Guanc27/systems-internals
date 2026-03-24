#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHELL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR/out"

mkdir -p "$OUT_DIR"

for batch in "$SHELL_DIR"/batch-files/*.batch; do
    name="$(basename "$batch" .batch)"
    echo "running $name"
    "$SHELL_DIR/myshell" "$batch" > "$OUT_DIR/$name.stdout" 2> "$OUT_DIR/$name.stderr" || true
done

echo "smoke suite complete: outputs in $OUT_DIR"
