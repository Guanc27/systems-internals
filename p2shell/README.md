# p2shell Firmware-Style Bring-up Extension

This project extends a teaching shell into a firmware-style control and validation harness:

- protocol-aware commands (`i2c_*`, `spi_xfer`, `mdio_read`, `uart_send`)
- simulated register-backed device state machine
- bring-up scripting helpers (`retry`, `sleep_ms`, `expect_timeout`, `assert_eq`, `dump_regs`)
- structured JSONL tracing and Python postmortem analyzer
- optional `p3malloc` allocator backend with memory fault injection
- test/fuzz scripts and CI workflow for regression checks

## Implemented Commands

### Protocol commands (Phase 1)

- `i2c_read <bus> <addr> <reg> <len>`
- `i2c_write <bus> <addr> <reg> <byte...>`
- `spi_xfer <bus> <cs> <tx_bytes...>`
- `mdio_read <bus> <phy> <reg>`
- `uart_send <port> <payload>`
- `dump_regs [start] [count]`

### Script/reliability commands

- `sleep_ms <ms>`
- `retry <n> <command...>`
- `expect_timeout <ms> <command...>`
- `assert_eq <lhs> <rhs>` (supports `$LAST`)
- `memstat`
- `memfault on|off`
- `memfault rate <0.0..1.0>`

## Files

- `myshell.c`: parser/dispatcher, protocol and scripting command execution
- `sim_device.c`, `sim_device.h`: simulated registers and driver-like state transitions
- `trace.c`, `trace.h`: JSONL trace emitter (enabled with `MYSHELL_TRACE`)
- `allocator.c`, `allocator.h`: allocator abstraction (`libc`/`smalloc`) and fault controls
- `tools/analyze_trace.py`: trace summary/postmortem tool
- `tools/fuzz_parser.py`: parser robustness smoke fuzzing
- `tests/run_tests.sh`: end-to-end local verification

## Build

```bash
make clean
make
```

## Demo Runs

```bash
MYSHELL_TRACE=traces/demo_success.jsonl ./myshell tests/demo_success.batch
python3 tools/analyze_trace.py traces/demo_success.jsonl
```

```bash
MYSHELL_TRACE=traces/demo_failure.jsonl ./myshell tests/demo_failure.batch
python3 tools/analyze_trace.py traces/demo_failure.jsonl
```

Allocator backend switch:

```bash
MYSHELL_ALLOCATOR=smalloc MYSHELL_TRACE=traces/demo_success.jsonl ./myshell tests/demo_success.batch
```

## One-command Verification

```bash
make test
```

## Sanitizer + Fuzz Smoke

```bash
make sanitizers
python3 tools/fuzz_parser.py
```

## Notes

- Trace logging is opt-in via `MYSHELL_TRACE=<path>`.
- CI pipeline is defined in `.github/workflows/ci.yml`.
