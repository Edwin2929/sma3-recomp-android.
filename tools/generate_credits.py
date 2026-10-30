#!/usr/bin/env python3
"""Recompile the verified Thumb routine allocated by CreditsInit.

ROM 0803430E..08034426 copies 08033A2C..08033BA8 to a dynamic IWRAM
allocation and publishes its Thumb pointer at 03006390. Only instruction and
PC-relative addresses in that audited routine are relocated. Generated code
and the source ROM remain private build inputs.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CANONICAL = 0x03004D50
SOURCE = 0x08033A2C
SIZE = 0x17C


def relocate(text):
    lines = []
    for line in text.splitlines():
        if not line.lstrip().startswith(("case ", "/*", "//")):
            line = re.sub(
                r"0x([0-9a-fA-F]{8})u",
                lambda m: "(" + m[0] + " + sma3_credits_delta)"
                if CANONICAL <= int(m[1], 16) < CANONICAL + SIZE + 4 else m[0],
                line,
            )
        line = line.replace("uint32_t _resume = g_runtime_resume_pc;",
                            "uint32_t _resume = g_runtime_resume_pc - sma3_credits_delta;")
        lines.append(line)
    return ("#include <cstdint>\nextern thread_local uint32_t sma3_credits_delta;\n"
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
    out = ROOT / "generated-credits"
    out.mkdir(exist_ok=True)
    config = out / "config.toml"
    config.write_text(f'''[program]
name="SMA3 dynamically allocated credits renderer"
id="credits"
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
source_start=0x{SOURCE:08X}
size=0x{SIZE:X}
[[extra_func]]
addr=0x{CANONICAL:08X}
mode="thumb"
name="credits_entry"
''')
    subprocess.run([str(args.recompiler.resolve()), "--rom", str(args.rom.resolve()),
                    "--config", str(config), "--out", str(out),
                    "--output-prefix", "credits_", "--codegen-shards", "2"], check=True)
    for path in out.glob("credits_recompiled_*.cpp"):
        path.write_text(relocate(path.read_text()))


if __name__ == "__main__":
    main()
