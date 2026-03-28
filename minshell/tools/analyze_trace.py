#!/usr/bin/env python3
import json
import sys
from collections import Counter


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: analyze_trace.py <trace.jsonl>")
        return 1

    path = sys.argv[1]
    total = 0
    failures = 0
    elapsed_total = 0
    cmd_counter: Counter[str] = Counter()
    detail_counter: Counter[str] = Counter()

    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            event = json.loads(line)
            total += 1
            elapsed_total += int(event.get("elapsed_ms", 0))
            if int(event.get("status", 1)) != 0:
                failures += 1
                detail_counter[str(event.get("detail", "unknown"))] += 1
            cmd = str(event.get("cmd", "")).split(" ")[0]
            if cmd:
                cmd_counter[cmd] += 1

    avg = (elapsed_total / total) if total else 0
    print(f"trace_file={path}")
    print(f"total_commands={total}")
    print(f"failed_commands={failures}")
    print(f"average_elapsed_ms={avg:.2f}")
    print("top_commands=" + ", ".join(f"{k}:{v}" for k, v in cmd_counter.most_common(5)))
    if detail_counter:
        print("failure_details=" + ", ".join(f"{k}:{v}" for k, v in detail_counter.most_common(5)))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
