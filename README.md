# sys-internals

This repo is a systems programming project built around two core pieces:

- `malloc/`: a small custom heap allocator (`smalloc`)
- `shell/`: a minimal shell used as a command driver and test harness

The goal was to practice low-level memory management and process control in C, then use those pieces together in a way that is testable and debuggable.

## What was implemented

- A page-backed allocator using `mmap` with an explicit free list
- First-fit allocation, block splitting, and adjacent block coalescing on free
- Heap/introspection helpers (`get_head_pointer`, `get_next_pointer`, `get_prev_pointer`) for trace-style validation
- A shell with:
  - built-ins (`cd`, `pwd`, `exit`)
  - external command execution via `fork`/`execvp`
  - output redirection with both `>` and custom `>+` behavior
  - semicolon-separated command execution
- Simulated protocol commands in shell (I2C/SPI/MDIO/UART) backed by a small device model
- Command tracing to JSONL and basic post-run analysis scripts
- A pluggable allocation backend in shell (`libc` or `smalloc`) plus memory fault injection knobs

## Pain points

Some of the harder parts were less about writing a lot of code and more about getting edge behavior right:

- Redirection parsing was easy to break with malformed input or ambiguous spacing
- Free-list pointer updates during split/coalesce were fragile until insertion order was made consistent
- Error handling had to stay strict and predictable (`An error has occurred`) across built-ins and external commands
- Failure-injection and timeout flows were useful, but they made test outcomes noisier until tracing was added

This is still a simple systems project, not a polished production shell or allocator. I hope to come back to this sometime and expand test cases, allocator robustness, and maybe more.
