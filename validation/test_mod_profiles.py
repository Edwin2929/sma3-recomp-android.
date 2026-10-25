#!/usr/bin/env python3
"""Exercise actual .gbamod import, persistence, failures, and rendered RGB.

No ROM, BIOS, generated game code, Android device, or network is required.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ENGINE = Path(os.environ.get('SMA3_TEST_ENGINE_ROOT', ROOT.parent / 'gbarecomp'))
GAME_SHA1 = '7352d2bd064d9ebaec579e264228aa21c7345b80'
NATIVE = None


def manifest(identifier='test.warm', options=None, version='1.0.0', name='Tono cálido 🥚'):
    text = f'''format_version = 1
id = "{identifier}"
version = "{version}"
name = "{name}"
author = "SMA3 test"
description = "Perfil de prueba sin recursos del juego."
license = "MIT"
resolver = "declarative"
save_compatibility = "shared"
[[target]]
game_id = "sma3-usa"
rom_sha1 = "{GAME_SHA1}"
[[feature]]
id = "colors"
name = "Colors"
default_enabled = false
[[plugin]]
feature = "colors"
id = "sma3.color-profile"
'''
    for key, value in (options if options is not None else {'rr': 108, 'bb': 88}).items():
        text += f'''[[option]]
feature = "colors"
id = "{key}"
label = "{key}"
type = "integer"
default = {value}
min = -200
max = 200
step = 1
'''
    return text


class Native:
    def __init__(self, root):
        self.process = subprocess.Popen([str(NATIVE), str(root)], text=True,
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        self.initial = self.read()

    def read(self):
        line = self.process.stdout.readline()
        if not line:
            raise AssertionError('Native driver exited: ' + self.process.stderr.read())
        return json.loads(line)

    def call(self, command, argument=''):
        self.process.stdin.write(command + ('\t' + str(argument) if argument else '') + '\n')
        self.process.stdin.flush()
        return self.read()

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=20)
        diagnostic = self.process.stderr.read()
        self.process.stdout.close()
        self.process.stderr.close()
        if self.process.returncode != 0:
            raise AssertionError(diagnostic)


class ModProfiles(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='sma3-mod-tests-')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.root = self.work / 'mods'
        self.native = Native(self.root)
        self.addCleanup(self.native.close)
        self.assertTrue(self.native.initial['ok'])
        self.sequence = 0

    def archive(self, text=None, entries=None, compression=zipfile.ZIP_DEFLATED):
        self.sequence += 1
        path = self.work / f'profile-{self.sequence}.gbamod'
        with zipfile.ZipFile(path, 'w', compression=compression) as archive:
            if text is not None:
                archive.writestr('manifest.toml', text)
            for name, data in entries or []:
                archive.writestr(name, data)
        return path

    def install(self, identifier='test.warm', options=None, **kwargs):
        result = self.native.call('import', self.archive(manifest(identifier, options, **kwargs)))
        self.assertTrue(result['ok'], result)
        return result

    def active(self, response):
        return [item['id'] for item in response['mods'] if item['enabled']]

    def test_import_unicode_exclusive_enable_disable_remove_and_restart(self):
        result = self.install()
        self.assertEqual(result['mods'][0]['name'], 'Tono cálido 🥚')
        self.assertEqual(self.active(result), [])
        original = self.native.call('pixels')['rgb']
        result = self.native.call('enable', 'test.warm')
        self.assertEqual(self.active(result), ['test.warm'])
        pixels = self.native.call('pixels')
        self.assertEqual(pixels['rgb'], [18, 83, 114, 255, 255, 224, 0, 0, 0])
        self.assertTrue(pixels['source_unchanged'])
        self.assertEqual(self.active(self.native.call('reload')), ['test.warm'])
        self.assertEqual(self.native.call('pixels')['rgb'], pixels['rgb'])
        self.install('test.swap', {'rr': 0, 'rg': 100, 'gg': 0, 'gb': 100, 'br': 100, 'bb': 0})
        self.assertEqual(self.active(self.native.call('enable', 'test.swap')), ['test.swap'])
        self.assertEqual(self.native.call('pixels')['rgb'][:3], [83, 129, 17])
        self.assertEqual(self.active(self.native.call('disable', 'test.swap')), [])
        self.assertEqual(self.native.call('pixels')['rgb'], original)
        self.native.call('enable', 'test.warm')
        self.assertTrue(self.native.call('remove', 'test.warm')['ok'])
        self.assertEqual(self.native.call('pixels')['rgb'], original)
        self.assertFalse((self.root / 'packages/test.warm').exists())

    def test_identity_defaults_negative_coefficients_clamping_and_snapshot_race(self):
        self.install('test.identity', {})
        self.native.call('enable', 'test.identity')
        self.assertEqual(self.native.call('pixels')['rgb'], [17, 83, 129, 255, 255, 255, 0, 0, 0])
        self.install('test.extreme', {'rr': -200, 'rg': 200, 'rb': 200, 'gg': -200, 'bb': 200})
        self.native.call('enable', 'test.extreme')
        self.assertEqual(self.native.call('pixels')['rgb'], [255, 0, 255, 255, 0, 255, 0, 0, 0])
        self.assertTrue(self.native.call('snapshot-race')['consistent'])

    def test_failed_writes_preserve_state_matrix_install_and_remove(self):
        self.install()
        self.native.call('enable', 'test.warm')
        before_state = (self.root / 'state.toml').read_bytes()
        before_pixels = self.native.call('pixels')['rgb']
        # A directory at the temporary filename deterministically blocks writes,
        # including when tests run as root and chmod cannot simulate ENOSPC.
        (self.root / 'state.toml.tmp').mkdir()
        self.assertFalse(self.native.call('disable', 'test.warm')['ok'])
        self.assertFalse(self.native.call('remove', 'test.warm')['ok'])
        self.assertTrue((self.root / 'packages/test.warm/1.0.0/manifest.toml').is_file())
        failed_import = self.native.call('import', self.archive(manifest('test.other')))
        self.assertFalse(failed_import['ok'])
        self.assertFalse((self.root / 'packages/test.other').exists())
        self.assertEqual((self.root / 'state.toml').read_bytes(), before_state)
        self.assertEqual(self.native.call('pixels')['rgb'], before_pixels)
        self.assertEqual(self.active(self.native.call('list')), ['test.warm'])

    def test_failed_atomic_rename_never_deletes_previous_output(self):
        self.install()
        state = self.root / 'state.toml'
        state.unlink()
        state.mkdir()
        sentinel = state / 'previous-state-sentinel'
        sentinel.write_text('preserve')
        self.assertFalse(self.native.call('enable', 'test.warm')['ok'])
        self.assertEqual(sentinel.read_text(), 'preserve')
        self.assertEqual(self.active(self.native.call('list')), [])

    def test_reject_unsupported_manifest_paths_options_and_duplicate_keys(self):
        good = manifest()
        malformed = {
            'wrong_game': good.replace('"sma3-usa"', '"other"'),
            'wrong_hash': good.replace(GAME_SHA1, '0' * 40),
            'code_plugin': good.replace('"sma3.color-profile"', '"arbitrary.code"'),
            'version_traversal': good.replace('version = "1.0.0"', 'version = "../../../escape"'),
            'version_absolute': good.replace('version = "1.0.0"', 'version = "/tmp/escape"'),
            'unknown_option': good.replace('id = "rr"', 'id = "write_rom"'),
            'out_of_range': good.replace('default = 108', 'default = 201'),
            'bad_step': good.replace('step = 1', 'step = 2'),
            'overflow_range': good.replace('min = -200', 'min = -9223372036854775808').replace('max = 200', 'max = 9223372036854775807'),
            'extra_feature': good + '\n[[feature]]\nid="other"\nname="Other"\n',
            'duplicate_key': good.replace('name = "Colors"', 'name = "Colors"\nname = "Hidden"'),
            'duplicate_option': good + '\n[[option]]\nfeature="colors"\nid="rr"\nlabel="Duplicate"\ntype="integer"\ndefault=0\nmin=-200\nmax=200\nstep=1\n',
            'rom_operation': good + '\n[[rom_write]]\naddress = 0\n',
            'nul': good.replace('default = 108', 'default = 108\0ignored'),
        }
        for name, text in malformed.items():
            with self.subTest(name=name):
                result = self.native.call('import', self.archive(text))
                self.assertFalse(result['ok'], result)
                self.assertEqual(result['mods'], [])
        self.assertFalse((self.work / 'escape').exists())

    def test_reject_archive_limits_payloads_crc_and_unsafe_paths(self):
        cases = [
            self.archive(manifest(), [('README.txt', b'x' * (1024 * 1024))]),
            self.archive(manifest() + '#' * (64 * 1024)),
            self.archive(manifest(), [('../escape', b'x')]),
            self.archive(manifest(), [('a/../README.txt', b'x')]),
            self.archive(manifest(), [('plugin.so', b'ELF')]),
            self.archive(manifest(), [(f'entry-{i}', b'x') for i in range(32)]),
            self.archive(None, [('README.md', b'missing manifest')]),
        ]
        for path in cases:
            self.assertFalse(self.native.call('import', path)['ok'])
        oversized = self.work / 'oversized.gbamod'
        with oversized.open('wb') as file:
            file.truncate(2 * 1024 * 1024 + 1)
        self.assertFalse(self.native.call('import', oversized)['ok'])
        corrupt = self.archive(manifest(), compression=zipfile.ZIP_STORED)
        raw = bytearray(corrupt.read_bytes())
        first_data = 30 + struct.unpack_from('<H', raw, 26)[0] + struct.unpack_from('<H', raw, 28)[0]
        raw[first_data] ^= 1
        corrupt.write_bytes(raw)
        self.assertFalse(self.native.call('import', corrupt)['ok'])
        self.assertEqual(self.native.call('list')['mods'], [])

    def test_duplicate_id_update_and_remove_id_cannot_escape(self):
        self.install()
        for version in ('1.0.0', '2.0.0'):
            self.assertFalse(self.native.call('import', self.archive(manifest(version=version)))['ok'])
        sentinel = self.work / 'keep'
        sentinel.write_text('intact')
        for identifier in ('../keep', '/tmp', '.', '..', 'test.warm/../../keep'):
            self.assertFalse(self.native.call('remove', identifier)['ok'])
        self.assertEqual(sentinel.read_text(), 'intact')

    def test_changed_or_broken_installed_manifest_cannot_enable_or_brick(self):
        self.install()
        path = self.root / 'packages/test.warm/1.0.0/manifest.toml'
        path.write_text(manifest().replace('"sma3.color-profile"', '"unsupported.plugin"'))
        self.assertFalse(self.native.call('enable', 'test.warm')['ok'])
        response = self.native.call('reload')
        self.assertTrue(response['ok'])
        self.assertTrue(response['warning'])
        self.assertFalse(response['mods'][0]['compatible'])
        self.assertEqual(self.active(response), [])
        self.assertTrue(self.native.call('remove', 'test.warm')['ok'])
        self.assertEqual(self.native.call('list')['mods'], [])

    def test_invalid_rom_reinitialization_clears_previous_authorization(self):
        self.install()
        self.native.call('enable', 'test.warm')
        self.assertFalse(self.native.call('bad-rom')['ok'])
        self.assertFalse(self.native.call('enable', 'test.warm')['ok'])
        self.assertEqual(self.native.call('pixels')['rgb'][:3], [17, 83, 129])


def build(output, sanitize=False):
    sources = [ROOT / 'validation/test_mod_profiles.cpp',
               ENGINE / 'tests/mod_runtime/ppu_provider_stub.cpp',
               *[ENGINE / 'src/runtime' / name for name in
                 ('mod_function_hooks.cpp', 'asset_picker.cpp', 'host_platform.cpp')],
               *[ENGINE / 'src/gba' / name for name in ('mod_audio.cpp', 'crc32.cpp', 'sha1.cpp')]]
    command = [os.environ.get('CXX', 'g++'), '-std=c++20', '-O1', '-g', '-pthread',
               '-DSMA3_MOD_PROFILES=1', '-I' + str(ROOT / 'src'),
               '-I' + str(ENGINE / 'src/runtime'), '-I' + str(ENGINE / 'src/gba'),
               '-I' + str(ENGINE / 'src/armv4t')]
    if sanitize:
        command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    subprocess.run(command + [str(path) for path in sources] + ['-o', str(output)], check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--native', type=Path)
    args, remaining = parser.parse_known_args()
    with tempfile.TemporaryDirectory(prefix='sma3-mod-native-') as work:
        NATIVE = args.native or Path(work) / 'mod-profile-tests'
        if not args.native:
            build(NATIVE, args.sanitize)
        unittest.main(argv=[__file__, *remaining], verbosity=2)
