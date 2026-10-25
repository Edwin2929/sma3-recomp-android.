#!/usr/bin/env python3
"""Compare each presented native center against an Original-mode capture.

Requires NumPy. A blend probe supplies frame IDs and full/middle classifications
in runner.log; those records are aligned by guest frame, never upload ordinal.
Only metrics and hashes enter the report. Keep input captures private.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

import numpy as np


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_uploads(path):
    records = []
    with path.open('rb') as stream:
        while header := stream.read(16):
            require(len(header) == 16, 'Truncated capture header')
            index, width, height, size = struct.unpack('<IIII', header)
            require(index == len(records) and 240 <= width <= 356 and height == 160 and
                    size == width * height * 3, 'Invalid capture dimensions or sequence')
            pixels = stream.read(size)
            require(len(pixels) == size, 'Truncated RGB capture')
            records.append(np.frombuffer(pixels, dtype=np.uint8).reshape(height, width, 3))
    require(records, 'No uploads found')
    return records


def mean_frame(previous, current):
    return ((previous.astype(np.uint16) + current.astype(np.uint16) + 1) // 2).astype(np.uint8)


def compare(reference_dir, candidate_dir, start_frame):
    reference = read_uploads(reference_dir / 'uploads.rgb')
    candidate = read_uploads(candidate_dir / 'uploads.rgb')
    require(all(frame.shape == (160, 240, 3) for frame in reference),
            'Reference must contain one unblended Original240 upload per guest frame')
    log = (candidate_dir / 'runner.log').read_text()
    metadata = [(int(index), int(frame), kind, int(blend), int(width))
                for index, frame, kind, blend, width in re.findall(
                    r'\[blend:upload\] index=(\d+) frame=(\d+) kind=(middle|full) blended=(\d) width=(\d+)', log)]
    blend_probe = bool(metadata)
    if not metadata:
        metadata = [(i, start_frame + i, 'full', 0, image.shape[1])
                    for i, image in enumerate(candidate)]
    require(len(metadata) == len(candidate), 'Upload metadata count differs from RGB count')
    full = {}
    for (index, frame, kind, blended, width), image in zip(metadata, candidate):
        require(index == len(full) if not blend_probe else index < len(candidate),
                'Unexpected full-frame ordering')
        if kind == 'full':
            require(frame not in full, 'Duplicate full presentation for one guest frame')
            full[frame] = image
    require(sorted(full) == list(range(start_frame, start_frame + len(reference))),
            'Full presentations do not align with the reference guest frames')
    full_diffs, middle_diffs, formula_errors = [], [], []
    widths, visibility = [], []
    for ordinal, ((index, frame, kind, blended, width), image) in enumerate(zip(metadata, candidate)):
        require(index == ordinal and width == image.shape[1], 'Incorrect presentation metadata')
        left = (width - 240) // 2
        native = reference[frame - start_frame]
        if kind == 'middle' and blended:
            require(frame > start_frame, 'Cannot blend without a previous reference frame')
            expected = mean_frame(reference[frame - start_frame - 1], native)
        else:
            expected = native
        count = int(np.any(image[:, left:left + 240] != expected, axis=2).sum())
        if count:
            (full_diffs if kind == 'full' else middle_diffs).append(
                dict(frame=frame, upload=index, different_center_pixels=count))
        if kind == 'middle':
            if blended:
                require(frame - 1 in full and full[frame - 1].shape == full[frame].shape,
                        'Blend crossed a missing frame or different framebuffer dimensions')
                own_expected = mean_frame(full[frame - 1], full[frame])
            else:
                own_expected = full[frame]
            if not np.array_equal(image, own_expected):
                formula_errors.append(index)
        if not widths or widths[-1]['width'] != width:
            widths.append(dict(upload=index, frame=frame, width=width))
        if kind == 'full':
            visible = bool(np.any(image[:, :left]) or np.any(image[:, left + 240:]))
            if not visibility or visibility[-1]['visible'] != visible:
                visibility.append(dict(upload=index, frame=frame, visible=visible))
    return dict(result='PASS' if not (full_diffs or middle_diffs or formula_errors) else 'FAIL',
                reference_frames=len(reference), uploads=len(candidate), full_count=len(full),
                middle_count=sum(row[2] == 'middle' for row in metadata),
                alignment='guest frame IDs and full/middle metadata' if blend_probe
                          else 'one upload per consecutive guest frame from the supplied origin',
                geometry_changes=widths, width_change_count=len(widths) - 1,
                full_center_differences=full_diffs, middle_center_differences=middle_diffs,
                blend_formula_errors=formula_errors, margin_visibility_changes=visibility,
                capture_sha256={name: hashlib.sha256((folder / 'uploads.rgb').read_bytes()).hexdigest()
                                for name, folder in (('reference', reference_dir), ('candidate', candidate_dir))})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--start-frame', type=int, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--expect-failure', action='store_true', help='Qualify a known broken baseline')
    args = parser.parse_args()
    try:
        result = compare(args.reference, args.candidate, args.start_frame)
        args.report.write_text(json.dumps(result, indent=2) + '\n')
        print(f'{result["result"]}: full={result["full_count"]}, middle={result["middle_count"]}, '
              f'center_errors={len(result["full_center_differences"])}/{len(result["middle_center_differences"])}, '
              f'width_changes={result["width_change_count"]}')
        require((result['result'] == 'FAIL') == args.expect_failure, 'Unexpected comparison result')
    except (OSError, ValueError, KeyError, IndexError, struct.error) as error:
        parser.exit(1, f'FAIL {error}\n')


if __name__ == '__main__':
    main()
