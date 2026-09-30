#!/usr/bin/env python3
"""Copy protocol headers from interfaces/c/ into the Arduino sketch folder.

Arduino IDE only compiles files inside the sketch folder (including src/), so a
relative include such as "../../interfaces/c/v2v_protocol.h" does not work.
interfaces/c/ stays the single source of truth; the ESP32 gets a generated copy.

Usage:
    python tools/sync_protocol.py          # write the copies
    python tools/sync_protocol.py --check  # exit 1 if a copy is stale (CI)
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEST_DIR = ROOT / "firmware" / "esp32-node" / "esp32-node" / "src"

# Files to mirror, relative to the repo root.
SOURCES = [
    Path("interfaces/c/v2v_protocol.h"),
]

HEADER = (
    "/* AUTO-GENERATED COPY — edit interfaces/c/ instead.\n"
    " * Source: {src}\n"
    " * Regenerate with: python tools/sync_protocol.py\n"
    " */\n"
)


def render(src: Path) -> str:
    body = (ROOT / src).read_text(encoding="utf-8").replace("\r\n", "\n")
    return HEADER.format(src=src.as_posix()) + body


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="do not write; fail if any copy is missing or stale")
    args = parser.parse_args()

    stale = []
    for src in SOURCES:
        dst = DEST_DIR / src.name
        expected = render(src)
        current = dst.read_text(encoding="utf-8") if dst.exists() else None
        if current == expected:
            print(f"up to date: {dst.relative_to(ROOT).as_posix()}")
            continue
        if args.check:
            stale.append(dst)
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        with dst.open("w", encoding="utf-8", newline="\n") as f:
            f.write(expected)
        print(f"written:    {dst.relative_to(ROOT).as_posix()}")

    if stale:
        for dst in stale:
            print(f"STALE: {dst.relative_to(ROOT).as_posix()} "
                  "(run python tools/sync_protocol.py)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
