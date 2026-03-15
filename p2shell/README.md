# p2shell Phase 1+2 Firmware-Style Extension

This implementation intentionally includes **only Phase 1 and Phase 2**:

- Phase 1: protocol-aware command layer + simulated device model
- Phase 2: bring-up scripting controls + structured trace logging + postmortem analysis

## Implemented Commands

### Protocol commands (Phase 1)

- `i2c_read <bus> <addr> <reg> <len>`
- `i2c_write <bus> <addr> <reg> <byte...>`
- `spi_xfer <bus> <cs> <tx_bytes...>`
- `mdio_read <bus> <phy> <reg>`
- `uart_send <port> <payload>`
- `dump_regs [start] [count]`

### Script/reliability commands (Phase 2)

- `sleep_ms <ms>`
- `retry <n> <command...>`
- `expect_timeout <ms> <command...>`
- `assert_eq <lhs> <rhs>` (supports `$LAST`)

## Files

- `myshell.c`: parser/dispatcher, protocol and scripting command execution
- `sim_device.c`, `sim_device.h`: simulated registers and driver-like state transitions
- `trace.c`, `trace.h`: JSONL trace emitter (enabled with `MYSHELL_TRACE`)
- `tools/analyze_trace.py`: trace summary/postmortem tool

## Build

```bash
make clean
make
```

## Demo (success path)

```bash
MYSHELL_TRACE=traces/phase2_success.jsonl ./myshell demo_phase2_success.batch
python3 tools/analyze_trace.py traces/phase2_success.jsonl
```

## Demo (failure path)

```bash
MYSHELL_TRACE=traces/phase2_failure.jsonl ./myshell demo_phase2_failure.batch
python3 tools/analyze_trace.py traces/phase2_failure.jsonl
```

## Notes

- Trace logging is opt-in via `MYSHELL_TRACE=<path>`.
- Memory allocator instrumentation/fault injection and CI/fuzz packaging are intentionally excluded (Phase 3/4).
