#!/usr/bin/env python3
"""Package a small, deterministic color-mod archive from editable text files.

The Android loader performs the authoritative target and profile validation.
This helper restricts the archive to manifest.toml and optional README.txt /
LICENSE.txt; it never recursively includes a ROM, assets, or build products.
"""
import argparse
from pathlib import Path
import tempfile
import zipfile

ALLOWED = ('manifest.toml', 'README.txt', 'LICENSE.txt')
MAX_MANIFEST = 64 * 1024
MAX_EXPANDED = 1024 * 1024


def package(source: Path, destination: Path, replace: bool = False):
    if destination.suffix.lower() != '.gbamod':
        raise ValueError('The output filename must end in .gbamod')
    if not source.is_dir():
        raise ValueError('The source must be a directory containing manifest.toml')
    if destination.exists() and not replace:
        raise FileExistsError('Output exists; use --replace to replace it explicitly')
    members = []
    expanded = 0
    for child in sorted(source.iterdir()):
        if child.name not in ALLOWED or child.is_symlink() or not child.is_file():
            raise ValueError('Unsupported package input: ' + child.name)
        size = child.stat().st_size
        if size > MAX_EXPANDED or (child.name == 'manifest.toml' and size > MAX_MANIFEST):
            raise ValueError('Package input is too large: ' + child.name)
        raw = child.read_bytes()
        raw.decode('utf-8')
        if len(raw) != size or b'\x00' in raw:
            raise ValueError('Package input changed or contains NUL bytes: ' + child.name)
        expanded += len(raw)
        if expanded > MAX_EXPANDED:
            raise ValueError('Expanded package exceeds 1 MiB')
        members.append((child.name, raw))
    if not any(name == 'manifest.toml' for name, _ in members):
        raise ValueError('Missing manifest.toml')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix='.color-mod-', suffix='.tmp',
                                     dir=destination.parent, delete=False) as file:
        temporary = Path(file.name)
    try:
        with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED,
                             compresslevel=9, allowZip64=False) as archive:
            for name, raw in members:
                info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = 0o100644 << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, raw, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
        # Refuse an unexpected late replacement unless the caller opted in.
        if destination.exists() and not replace:
            raise FileExistsError('Output appeared while packaging')
        temporary.replace(destination)
    finally:
        if temporary.exists():
            temporary.unlink()
    return len(members), expanded, destination.stat().st_size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    try:
        count, expanded, compressed = package(args.source, args.output, args.replace)
    except (OSError, ValueError, UnicodeError) as error:
        parser.exit(1, 'Cannot package mod: ' + str(error) + '\n')
    print(f'{args.output.name}: {count} text files, {expanded} expanded bytes, {compressed} archive bytes')


if __name__ == '__main__':
    main()
