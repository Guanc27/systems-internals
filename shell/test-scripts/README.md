This directory now contains a compact smoke harness.

- `run_smoke_suite.sh`: runs all `.batch` files in `batch-files`.
- `run_one_case.sh <name>`: runs one case by filename stem.
- `clean_outputs.sh`: removes generated outputs.

The old one-to-one `expected-output` diff harness was intentionally removed.
