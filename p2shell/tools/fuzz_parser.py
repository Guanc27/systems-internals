#!/usr/bin/env python3
import random
import string
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHELL = ROOT / "myshell"

TOKENS = [
    "pwd",
    "cd /tmp",
    "i2c_read 0 80 0 1",
    "i2c_write 0 80 1 1",
    "spi_xfer 0 1 1 2 3",
    "mdio_read 0 1 2",
    "sleep_ms 1",
    "dump_regs 0 4",
    "memstat",
]


def rand_noise() -> str:
    size = random.randint(1, 14)
    return "".join(random.choice(string.ascii_letters + " ;>+0123") for _ in range(size))


def one_line() -> str:
    if random.random() < 0.7:
        return random.choice(TOKENS)
    return rand_noise()


def main() -> int:
    random.seed(42)
    for _ in range(100):
        content = "\n".join(one_line() for _ in range(random.randint(1, 8))) + "\n"
        with tempfile.NamedTemporaryFile("w", delete=False, encoding="utf-8") as f:
            f.write(content)
            path = f.name
        subprocess.run([str(SHELL), path], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("fuzz_parser completed 100 random batches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
