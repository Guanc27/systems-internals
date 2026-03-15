# p2shell Firmware-Style Bring-up Extension

[![CI](./.github/workflows/ci.yml/badge.svg)](./.github/workflows/ci.yml)

This project extends a teaching shell into a firmware-style control and validation harness:

- protocol-aware commands (`i2c_*`, `spi_xfer`, `mdio_read`, `uart_send`)
- simulated register-backed device state machine
- bring-up scripting helpers (`retry`, `sleep_ms`, `expect_timeout`, `assert_eq`, `dump_regs`)
- structured JSONL tracing and Python postmortem analyzer
- optional `p3malloc` allocator backend with memory fault injection

## Architecture

- `myshell.c`: parser, command dispatch, scripting controls, and execution engine
- `sim_device.c/.h`: protocol simulation and register state transitions
- `trace.c/.h`: JSON trace emission for command-level observability
- `allocator.c/.h`: allocator abstraction (`libc` or `smalloc`) + fault controls
- `tools/analyze_trace.py`: postmortem summaries from trace files
- `tools/fuzz_parser.py`: parser robustness smoke fuzzing

## Protocol Command Reference

- `i2c_read <bus> <addr> <reg> <len>`
- `i2c_write <bus> <addr> <reg> <byte...>`
- `spi_xfer <bus> <cs> <tx_bytes...>`
- `mdio_read <bus> <phy> <reg>`
- `uart_send <port> <payload>`
- `dump_regs [start] [count]`

Driver-like behavior in the simulator includes:

- status register (`READY`, `BUSY`, `ERROR` bits)
- control register transitions (`CONTROL_START` sets BUSY then returns READY)
- deterministic state progression via internal ticks

## Scripting and Reliability Commands

- `sleep_ms <ms>`
- `retry <n> <command...>`
- `expect_timeout <ms> <command...>`
- `assert_eq <lhs> <rhs>` (supports `$LAST` result token)
- `memstat`
- `memfault on|off`
- `memfault rate <0.0..1.0>`

## Build

```bash
cd p2shell
make clean
make
```

## Demo Runs

Success path:

```bash
MYSHELL_TRACE=traces/demo_success.jsonl ./myshell tests/demo_success.batch
python3 tools/analyze_trace.py traces/demo_success.jsonl
```

Failure-injection path:

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

This runs:

1. success and failure demo batches
2. trace analyzer reports
3. key output assertions

## Sanitizer + Fuzz Smoke

```bash
make sanitizers
python3 tools/fuzz_parser.py
```

## Interview Positioning

- Firmware/Embedded: register protocol workflows, retries/timeouts, diagnostics
- Systems Software: process execution, observability, memory behavior under fault
- General SWE: robust parsing, automation scripts, CI, and reproducible testing
