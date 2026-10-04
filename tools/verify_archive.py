#!/usr/bin/env python3
"""Check generated archive integrity and coverage of declared functions."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('archive', type=Path)
args = parser.parse_args()
archive = args.archive.resolve()
members = subprocess.check_output(['ar', 't', str(archive)], text=True).splitlines()
expected = [f'recompiled_{i:03}.cpp.o' for i in range(64)] + [
    'dispatch_table.cpp.o', 'symbol_map.cpp.o']
if sorted(members) != sorted(expected):
    parser.error('Archive does not contain exactly the expected 66 members')
for member in members:
    data = subprocess.check_output(['ar', 'p', str(archive), member])
    if not data.startswith(b'\x7fELF'):
        parser.error(f'Invalid or empty ELF object: {member}')
symbols = subprocess.check_output(['nm', '-g', '--defined-only', str(archive)], text=True)
defined = set(re.findall(r'\bT (gf_\w+)\b', symbols))
header = (root / 'generated-project/generated/recompiled.h').read_text()
declared = set(re.findall(r'void (gf_\w+)\(void\)', header))
missing = declared - defined
if missing:
    parser.error(f'{len(missing)} generated functions lack definitions: {sorted(missing)[:5]}')
with archive.open('rb') as stream:
    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
report = dict(archive_bytes=archive.stat().st_size, archive_sha256=digest,
              elf_members=len(members), declared_functions=len(declared),
              defined_functions=len(defined), missing_functions=0,
              platform='Linux x86_64', gameplay_validated=False,
              android_validated=False, performance_measured=False)
(root / 'build-verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
