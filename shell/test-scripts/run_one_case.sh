#!/bin/bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: ./run_one_case.sh <case_name_without_extension>"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHELL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR/out"
CASE_NAME="$1"
BATCH_FILE="$SHELL_DIR/batch-files/$CASE_NAME.batch"

if [ ! -f "$BATCH_FILE" ]; then
    echo "missing case file: $BATCH_FILE"
    exit 1
fi

mkdir -p "$OUT_DIR"
"$SHELL_DIR/myshell" "$BATCH_FILE" > "$OUT_DIR/$CASE_NAME.stdout" 2> "$OUT_DIR/$CASE_NAME.stderr" || true
echo "finished $CASE_NAME"
