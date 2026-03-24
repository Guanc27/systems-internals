#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
rm -rf "$SCRIPT_DIR/out"
rm -f "$SCRIPT_DIR"/../shell_redir_a.txt "$SCRIPT_DIR"/../shell_redir_b.txt

echo "cleaned shell smoke outputs"
