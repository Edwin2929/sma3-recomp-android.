#!/usr/bin/env python3
"""Recompile the two byte-verified, dynamically allocated Fuzzy ARM routines.

The original game publishes their current allocations at 03007264/03007260.
Only instruction addresses and PC-relative literals in each audited routine are
relocated. Resume labels remain canonical; execution timing uses the normal
generated ARM implementation. ROM-derived output remains a private build input.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CANONICAL = 0x03004D50
VARIANTS = (("tiles", 0x0800091C, 0xD8), ("offsets", 0x08000A08, 0x78))


def relocate(text, size):
    lines = []
    for line in text.splitlines():
        if not line.lstrip().startswith(("case ", "/*", "//")):
            line = re.sub(
                r"0x([0-9a-fA-F]{8})u",
                lambda m: "(" + m[0] + " + sma3_effect_delta)"
                if CANONICAL <= int(m[1], 16) < CANONICAL + size + 12 else m[0],
                line,
            )
        line = line.replace("uint32_t _resume = g_runtime_resume_pc;",
                            "uint32_t _resume = g_runtime_resume_pc - sma3_effect_delta;")
        lines.append(line)
    return ("#include <cstdint>\nextern thread_local uint32_t sma3_effect_delta;\n"
            + "\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--recompiler", type=Path,
                        default=ROOT.parent / "gbarecomp/build/gba_recompile")
    args = parser.parse_args()
    raw = args.rom.read_bytes()
    if len(raw) != 0x400000 or hashlib.sha1(raw).hexdigest() != "7352d2bd064d9ebaec579e264228aa21c7345b80":
        parser.error("Unsupported ROM identity")
    for ident, source, size in VARIANTS:
        out = ROOT / "generated-effects" / ident
        out.mkdir(parents=True, exist_ok=True)
        prefix = f"effect_{ident}_"
        config = out / "config.toml"
        config.write_text(f'''[program]
name="SMA3 dynamically allocated ARM effect {ident}"
id="{prefix}"
load_address=0x08000000
size=0x00400000
entry_pc=0x{CANONICAL:08X}
speculative_literal_harvest=false
static_resume_all=true
codegen_shards=2
[identity]
sha1="7352d2bd064d9ebaec579e264228aa21c7345b80"
[[code_copy]]
runtime_start=0x{CANONICAL:08X}
source_start=0x{source:08X}
size=0x{size:X}
[[extra_func]]
addr=0x{CANONICAL:08X}
mode="arm"
name="effect_entry"
''')
        subprocess.run([str(args.recompiler.resolve()), "--rom", str(args.rom.resolve()),
                        "--config", str(config), "--out", str(out),
                        "--output-prefix", prefix, "--codegen-shards", "2"], check=True)
        for path in out.glob(prefix + "recompiled_*.cpp"):
            path.write_text(relocate(path.read_text(), size))


if __name__ == "__main__":
    main()
