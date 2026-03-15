#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p traces
rm -f traces/demo_success.jsonl traces/demo_failure.jsonl

echo "[1/4] Running success bring-up script"
MYSHELL_TRACE=traces/demo_success.jsonl ./myshell tests/demo_success.batch > traces/demo_success.out

echo "[2/4] Running failure-injection script"
MYSHELL_TRACE=traces/demo_failure.jsonl ./myshell tests/demo_failure.batch > traces/demo_failure.out || true

echo "[3/4] Analyzing traces"
python3 tools/analyze_trace.py traces/demo_success.jsonl > traces/demo_success_report.txt
python3 tools/analyze_trace.py traces/demo_failure.jsonl > traces/demo_failure_report.txt

echo "[4/4] Validating key signals"
python3 - <<'PY'
from pathlib import Path

success = Path("traces/demo_success.out").read_text()
failure = Path("traces/demo_failure.out").read_text()
assert "TIMEOUT_EXPECTED" in success, "expect_timeout path missing"
assert "alloc_calls=" in success, "memstat output missing"
assert "An error has occurred" in failure, "failure mode did not trigger errors"
print("Validation checks passed.")
PY

echo "All tests passed."
