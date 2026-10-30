#!/usr/bin/env python3
"""Exercise the real patch applicator on disposable local engine clones.

No ROM, BIOS, network, or modification of the supplied engine is needed.
Set SMA3_TEST_ENGINE_ROOT when the pinned engine is not ../gbarecomp.
"""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
PIN = 'e7728148c6829ba526f682876430a0c9022dc6c0'
PATCH_NAMES = ('interpolation', 'controls', 'localization', 'touch-design',
               'widescreen-objects', 'widescreen-window', 'panorama-android',
               'panorama-stability', 'image-filters', 'mod-profiles',
               'frame-pacing', 'touch-appearance', 'render-performance',
               'cooperative-presentation', 'yoshi-color', 'obj-color-effects',
               'achievement-suspend', 'performance-120')
PATHS = tuple('src/runtime/' + name for name in
              ('host_platform.cpp', 'host_platform.h', 'host_window.cpp', 'host_window.h', 'runtime.cpp',
               'runtime_bus_bridge.cpp', 'mod_runtime.cpp')) + (
                  'src/gba/gba_ppu.h', 'src/gba/gba_ppu.cpp')


def git(folder, *args):
    return subprocess.run(['git', '-C', str(folder), *args], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def snapshot(folder):
    """Observe every worktree file, including the unrelated sentinel."""
    return {str(path.relative_to(folder)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in folder.rglob('*')
            if '.git' not in path.relative_to(folder).parts and path.is_file()}


class WindowPatchStackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = Path(os.environ.get('SMA3_TEST_ENGINE_ROOT', ROOT.parent / 'gbarecomp'))
        if not (cls.source / '.git').exists():
            raise unittest.SkipTest('Pinned engine unavailable; set SMA3_TEST_ENGINE_ROOT')
        # Read immutable objects, not the caller's possibly patched worktree.
        cls.base = {path: git(cls.source, 'show', PIN + ':' + path) for path in PATHS}
        cls.parent = git(cls.source, 'rev-parse', PIN + '^').decode().strip()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='sma3-window-patch-test-')
        self.addCleanup(self.temp.cleanup)
        self.engine = Path(self.temp.name) / 'engine'
        # --shared borrows immutable objects. All refs and working files belong
        # to this disposable clone; no reset/checkout ever targets the source.
        subprocess.run(['git', 'clone', '--shared', '--no-checkout', '--quiet',
                        str(self.source), str(self.engine)], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        git(self.engine, 'update-ref', '--no-deref', 'HEAD', PIN)
        self.unrelated = self.engine / 'local-notes.txt'
        self.unrelated.write_text('Unrelated local content must survive unchanged.\n')
        self.restore_base()

    def restore_base(self):
        for path, content in self.base.items():
            target = self.engine / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)

    def apply_prefix(self, count):
        for name in PATCH_NAMES[:count]:
            git(self.engine, 'apply', str(ROOT / 'patches' / ('gbarecomp-' + name + '.patch')))

    def run_applicator(self):
        return subprocess.run([sys.executable, '-B',
                               str(ROOT / 'tools/apply_interpolation_patch.py'), str(self.engine)],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def test_all_supported_prefixes_complete_and_are_idempotent(self):
        self.apply_prefix(len(PATCH_NAMES))
        expected = snapshot(self.engine)
        prefixes = set()
        for count in range(len(PATCH_NAMES) + 1):
            with self.subTest(applied_prefix=count):
                self.restore_base()
                self.apply_prefix(count)
                prefixes.add(tuple(sorted(snapshot(self.engine).items())))
                first = self.run_applicator()
                self.assertEqual(first.returncode, 0, first.stderr)
                self.assertEqual(snapshot(self.engine), expected)
                second = self.run_applicator()
                self.assertEqual(second.returncode, 0, second.stderr)
                self.assertEqual(snapshot(self.engine), expected)
        self.assertEqual(len(prefixes), len(PATCH_NAMES) + 1)

    def test_unknown_edits_to_each_guarded_file_are_rejected_without_writes(self):
        for path in PATHS:
            with self.subTest(edited_path=path):
                self.restore_base()
                self.apply_prefix(len(PATCH_NAMES) - 1)
                target = self.engine / path
                target.write_bytes(target.read_bytes() + b'\n// unrelated local edit\n')
                before = snapshot(self.engine)
                result = self.run_applicator()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Unrecognized local engine changes', result.stderr)
                self.assertEqual(snapshot(self.engine), before)

    def test_wrong_engine_pin_is_rejected_without_writes(self):
        git(self.engine, 'update-ref', '--no-deref', 'HEAD', self.parent)
        before = snapshot(self.engine)
        result = self.run_applicator()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('SMA3 requires GBARecomp commit ' + PIN, result.stderr)
        self.assertEqual(snapshot(self.engine), before)


if __name__ == '__main__':
    unittest.main(verbosity=2)
