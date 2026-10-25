#!/usr/bin/env python3
"""Independent hostile-input/transaction tests of the real mod catalog.

SMA3_MOD_PROBE points to a compiled mod_adversarial_probe.cpp linked with the
pinned engine mod_runtime.cpp and SMA3_MOD_PROFILES=1. SMA3_MOD_TEST_ROM is
optional: the three production-initialization tests require the user's matching ROM;
all archive tests use the engine's explicit identity seam without game data.
"""
import hashlib
import json
import os
from pathlib import Path
import select
import struct
import subprocess
import tempfile
import unittest
import warnings
import zipfile

GAME_HASH = '7352d2bd064d9ebaec579e264228aa21c7345b80'
BASE_PIXEL = [120, 150, 200, 255, 1, 2, 0, 255, 99]
WARM_PIXEL = [130, 150, 176, 255, 1, 2, 0, 255, 87]


def manifest(identifier='review.warm', options=None, default=False):
    if options is None:
        options = {'rr': 108, 'gg': 100, 'bb': 88}
    result = f'''format_version = 1
id = "{identifier}"
version = "1.0.0"
name = "Revisión 🌈"
author = "Prueba independiente"
description = "Perfil para pruebas; no contiene datos del juego."
resolver = "declarative"
save_compatibility = "shared"

[[target]]
game_id = "sma3-usa"
rom_sha1 = "{GAME_HASH}"

[[feature]]
id = "colors"
name = "Colores"
default_enabled = {str(default).lower()}

[[plugin]]
feature = "colors"
id = "sma3.color-profile"
'''
    for key, value in options.items():
        result += f'''
[[option]]
feature = "colors"
id = "{key}"
label = "{key}"
type = "integer"
default = {value}
min = -200
max = 200
step = 1
'''
    return result


def files_snapshot(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob('*') if path.is_file()}


class CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe = os.environ.get('SMA3_MOD_PROBE')
        if not cls.probe:
            raise unittest.SkipTest('Set SMA3_MOD_PROBE to the compiled native probe')
        cls.rom = os.environ.get('SMA3_MOD_TEST_ROM')

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='sma3-mod-review-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.root = self.directory / 'catalog'
        self.process = subprocess.Popen([self.probe], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        self.addCleanup(self.stop)
        response = self.call('raw-init', self.root)
        self.assertTrue(response['initialized'])

    def stop(self):
        if self.process.poll() is None:
            self.process.stdin.write('quit\n')
            self.process.stdin.flush()
        try:
            _, error = self.process.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.kill()
            _, error = self.process.communicate()
            self.fail('Native probe failed to stop: ' + error)
        self.assertEqual(self.process.returncode, 0, error)

    def call(self, *parts):
        self.process.stdin.write('\t'.join(map(str, parts)) + '\n')
        self.process.stdin.flush()
        if not select.select([self.process.stdout], [], [], 15)[0]:
            self.fail('Native probe timed out: ' + str(parts[0]))
        response = self.process.stdout.readline()
        self.assertTrue(response, 'Native probe exited during ' + str(parts[0]))
        self.assertTrue(response.isascii(), 'JNI response must be ASCII-safe JSON')
        return json.loads(response)

    def archive(self, name, text=None, members=None, compression=zipfile.ZIP_DEFLATED):
        path = self.directory / (name + '.gbamod')
        if members is None:
            members = [('manifest.toml', (text or manifest()).encode())]
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', UserWarning)
            with zipfile.ZipFile(path, 'w', compression=compression) as archive:
                for member, content in members:
                    archive.writestr(member, content)
        return path

    def install(self, identifier='review.warm', **kwargs):
        response = self.call('import', self.archive(identifier, manifest(identifier, **kwargs)))
        self.assertTrue(response['ok'], response)
        return response

    def assert_rejected_unchanged(self, archive):
        before = files_snapshot(self.root)
        catalog = self.call('list')['mods']
        pixels = self.call('pixel')
        response = self.call('import', archive)
        self.assertFalse(response['ok'], response)
        self.assertTrue(response['error'])
        self.assertEqual(response['mods'], catalog)
        self.assertEqual(files_snapshot(self.root), before)
        self.assertEqual(self.call('pixel'), pixels)
        self.assertFalse(any(path.name.startswith('.staging-profile-')
                             for path in self.root.iterdir()))

    def test_real_profile_pixels_exclusive_activation_and_restoration(self):
        response = self.install(default=True)
        self.assertEqual(response['mods'][0]['name'], 'Revisión 🌈')
        self.assertFalse(response['mods'][0]['enabled'], 'author cannot auto-enable import')
        self.assertEqual(self.call('pixel')['pixel'], BASE_PIXEL)
        self.assertTrue(self.call('enable', 'review.warm')['ok'])
        self.assertEqual(self.call('pixel')['pixel'], WARM_PIXEL)
        self.install('review.swap', options={'rr': 0, 'rb': 100, 'bb': 0, 'br': 100})
        result = self.call('enable', 'review.swap')
        self.assertEqual([x['id'] for x in result['mods'] if x['enabled']], ['review.swap'])
        self.assertEqual(self.call('pixel')['pixel'], [200,150,120,2,1,255,99,255,0])
        self.assertTrue(self.call('disable', 'review.swap')['ok'])
        self.assertEqual(self.call('pixel'), {'pixel': BASE_PIXEL, 'identity_pointer': True})
        self.assertTrue(self.call('enable', 'review.warm')['ok'])
        self.assertTrue(self.call('remove', 'review.warm')['ok'])
        self.assertEqual(self.call('pixel')['pixel'], BASE_PIXEL)
        self.assertFalse((self.root / 'packages/review.warm').exists())

    def test_reject_manifest_schema_and_coefficients(self):
        self.install()
        self.call('enable', 'review.warm')
        good = manifest('review.bad')
        variants = {
            'format-zero': good.replace('format_version = 1', 'format_version = 0'),
            'format-two': good.replace('format_version = 1', 'format_version = 2'),
            'version-traversal': good.replace('version = "1.0.0"', 'version = "../escape"'),
            'version-absolute': good.replace('version = "1.0.0"', 'version = "/escape"'),
            'wrong-game': good.replace('game_id = "sma3-usa"', 'game_id = "other-game"'),
            'wrong-hash': good.replace(GAME_HASH, '0' * 40),
            'unknown-plugin': good.replace('sma3.color-profile', 'sma3.unknown-plugin'),
            'unknown-option': good.replace('id = "rr"', 'id = "alpha"'),
            'coefficient-high': good.replace('default = 108', 'default = 201'),
            'coefficient-low': good.replace('default = 108', 'default = -201'),
            'coefficient-overflow': good.replace('default = 108', 'default = 999999999999999999999'),
            'range-min': good.replace('min = -200', 'min = 0', 1),
            'range-max': good.replace('max = 200', 'max = 201', 1),
            'step': good.replace('step = 1', 'step = 2', 1),
            'wrong-type': good.replace('type = "integer"', 'type = "boolean"', 1),
            'duplicate-key': good.replace('default = 108', 'default = 108\ndefault = 88'),
            'duplicate-header-key': good.replace('format_version = 1', 'format_version = 1\nformat_version = 1'),
            'duplicate-option': good + good[good.index('\n[[option]]'):good.index('\n[[option]]', good.index('\n[[option]]') + 1)],
            'invalid-utf8': good.encode().replace('Revisión'.encode(), b'Revisi\xffn'),
            'missing-feature': good.replace('id = "colors"', 'id = "other"', 1),
        }
        for name, text in variants.items():
            with self.subTest(name=name):
                if isinstance(text, bytes):
                    path = self.archive(name, members=[('manifest.toml', text)])
                else:
                    path = self.archive(name, text)
                self.assert_rejected_unchanged(path)

    def test_reject_zip_paths_duplicates_crc_truncation_and_bombs(self):
        self.install()
        self.call('enable', 'review.warm')
        good = manifest('review.bad').encode()
        members = {
            'traversal': [('../manifest.toml', good)],
            'absolute': [('/manifest.toml', good)],
            'windows-path': [('C:\\manifest.toml', good)],
            'nested': [('nested/manifest.toml', good)],
            'duplicate': [('manifest.toml', good), ('manifest.toml', good)],
            'extra-binary': [('manifest.toml', good), ('payload.so', b'not executable')],
            'manifest-limit': [('manifest.toml', good + b'#' * 65536)],
            'expanded-bomb': [('manifest.toml', good), ('README.txt', b'x' * (1024*1024))],
            'directory': [('manifest.toml', good), ('folder/', b'')],
        }
        for name, entries in members.items():
            with self.subTest(name=name):
                self.assert_rejected_unchanged(self.archive(name, members=entries))
        stored = self.archive('crc', text=manifest('review.bad'), compression=zipfile.ZIP_STORED)
        raw = bytearray(stored.read_bytes())
        name_length, extra_length = struct.unpack_from('<HH', raw, 26)
        raw[30 + name_length + extra_length] ^= 1
        stored.write_bytes(raw)
        self.assert_rejected_unchanged(stored)
        truncated = self.archive('truncated', text=manifest('review.bad'))
        truncated.write_bytes(truncated.read_bytes()[:-15])
        self.assert_rejected_unchanged(truncated)
        oversized = self.directory / 'oversized.gbamod'
        oversized.write_bytes(b'x' * (2*1024*1024+1))
        self.assert_rejected_unchanged(oversized)

    def test_duplicate_id_and_invalid_operation_ids_keep_state(self):
        self.install()
        self.call('enable', 'review.warm')
        self.assert_rejected_unchanged(self.archive('duplicate-id', manifest()))
        before = files_snapshot(self.root)
        for identifier in ['../escape', '/escape', '', 'x' * 129, 'UPPERCASE']:
            for command in ['enable', 'disable', 'remove']:
                with self.subTest(identifier=identifier, command=command):
                    self.assertFalse(self.call(command, identifier)['ok'])
                    self.assertEqual(files_snapshot(self.root), before)
                    self.assertEqual(self.call('pixel')['pixel'], WARM_PIXEL)

    def test_persistence_failure_rolls_back_enable_import_and_remove(self):
        self.install()
        self.install('review.swap', options={'rr': 0, 'rb': 100, 'bb': 0, 'br': 100})
        self.call('enable', 'review.warm')
        blocker = self.root / 'state.toml.tmp'
        blocker.mkdir()
        before = files_snapshot(self.root)
        catalog = self.call('list')['mods']
        for command in [('enable', 'review.swap'), ('disable', 'review.warm'),
                        ('import', self.archive('new', manifest('review.new'))),
                        ('remove', 'review.warm')]:
            with self.subTest(command=command[0]):
                response = self.call(*command)
                self.assertFalse(response['ok'], response)
                self.assertEqual(response['mods'], catalog)
                self.assertEqual(files_snapshot(self.root), before)
                self.assertEqual(self.call('pixel')['pixel'], WARM_PIXEL)
        blocker.rmdir()
        self.assertTrue(self.call('remove', 'review.warm')['ok'])
        self.assertEqual(self.call('pixel')['pixel'], BASE_PIXEL)

    def test_production_restart_and_corrupt_catalog_fail_open(self):
        if not self.rom:
            self.skipTest('SMA3_MOD_TEST_ROM is needed for production SHA verification')
        self.install()
        self.call('enable', 'review.warm')
        self.assertTrue(self.call('init', self.root, self.rom)['initialized'])
        self.assertEqual(self.call('pixel')['pixel'], WARM_PIXEL)
        damaged = self.root / 'packages/review.warm/1.0.0/manifest.toml'
        damaged.write_text('corrupted installed manifest\n')
        response = self.call('init', self.root, self.rom)
        self.assertTrue(response['initialized'], response)
        self.assertTrue(response['catalog']['ok'])
        self.assertFalse(response['catalog']['mods'][0]['compatible'])
        self.assertEqual(self.call('pixel')['pixel'], BASE_PIXEL)
        self.assertFalse(self.call('enable', 'review.warm')['ok'])
        self.assertTrue(self.call('remove', 'review.warm')['ok'])
        self.assertEqual(self.call('list')['mods'], [])

    def test_concurrent_catalog_changes_use_one_matrix_per_frame(self):
        self.install()
        self.install('review.swap', options={'rr': 0, 'rb': 100, 'bb': 0, 'br': 100})
        response = self.call('concurrent', 'review.warm', 'review.swap')
        self.assertTrue(response['coherent'], response)
        self.assertGreaterEqual(response['frames'], 100)
        self.assertEqual(response['catalog_changes'], 100)

    def test_corrupt_selection_file_cannot_partially_activate(self):
        if not self.rom:
            self.skipTest('SMA3_MOD_TEST_ROM is needed for production SHA verification')
        self.install()
        self.call('enable', 'review.warm')
        selection = self.root / 'state.toml'
        valid = selection.read_text()
        damaged_states = {
            'unknown-format': valid.replace('format_version = 1', 'format_version = 2'),
            'duplicate-enabled': valid.replace('enabled = true', 'enabled = true\nenabled = true'),
            'truncated-version': valid.replace('version = "1.0.0"', 'version = "1.0.0'),
            'malformed-section': valid + '\n[[feature\n',
        }
        for name, state in damaged_states.items():
            with self.subTest(name=name):
                selection.write_text(state)
                response = self.call('init', self.root, self.rom)
                self.assertTrue(response['initialized'], response)
                self.assertTrue(response['catalog']['ok'])
                self.assertTrue(response['catalog']['warning'])
                self.assertFalse(any(mod['enabled'] for mod in response['catalog']['mods']))
                self.assertEqual(self.call('pixel')['pixel'], BASE_PIXEL)

    def test_wrong_rom_reinitialization_cannot_reactivate_previous_profile(self):
        if not self.rom:
            self.skipTest('SMA3_MOD_TEST_ROM is needed for production SHA verification')
        self.install()
        self.call('enable', 'review.warm')
        wrong = self.directory / 'wrong.gba'
        wrong.write_bytes(b'not a game')
        response = self.call('init', self.root, wrong)
        self.assertFalse(response['initialized'])
        self.assertEqual(self.call('pixel')['pixel'], BASE_PIXEL)
        self.assertFalse(self.call('enable', 'review.warm')['ok'])
        self.assertFalse(self.call('list')['ok'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
