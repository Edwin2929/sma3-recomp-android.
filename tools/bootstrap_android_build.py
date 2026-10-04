#!/usr/bin/env python3
"""Install pinned Android build tools locally and assemble the ARM64 debug APK.

Requires JDK 17, Python 3, curl and git. Downloads official SDK/Gradle packages.
Generated game and BIOS sources must already exist in this checkpoint.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import urllib.parse
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--tools-only', action='store_true')
parser.add_argument('--native-only', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
cache = root.parent / 'build-tools'
cache.mkdir(exist_ok=True)
sdk = cache / 'android-sdk'
env = os.environ.copy()
java = shutil.which('java')
if not java: raise SystemExit('Install JDK 17 first')
java_home = Path(java).resolve().parents[1]
env['JAVA_HOME'] = str(java_home)
proxy = urllib.parse.urlparse(env.get('HTTPS_PROXY', ''))
flags = []
if proxy.hostname:
    for scheme in ['http', 'https']:
        flags += [f'-D{scheme}.proxyHost={proxy.hostname}', f'-D{scheme}.proxyPort={proxy.port or 80}']
cert = env.get('CODEX_PROXY_CERT')
if cert and Path(cert).is_file():
    trust = cache / 'android-truststore'
    shutil.copyfile(java_home/'lib/security/cacerts', trust)
    subprocess.run(['keytool', '-importcert', '-noprompt', '-alias', 'codex-proxy', '-file', cert,
                    '-keystore', str(trust), '-storepass', 'changeit'], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    flags += [f'-Djavax.net.ssl.trustStore={trust}', '-Djavax.net.ssl.trustStorePassword=changeit']
env['JAVA_TOOL_OPTIONS'] = ' '.join(flags)
env['ANDROID_HOME'] = str(sdk)
env['GRADLE_USER_HOME'] = str(cache/'gradle-cache')
def download(url, destination):
    if not destination.exists():
        subprocess.run(['curl', '-fL', '--retry', '2', url, '-o', str(destination)], check=True)
def merge(src, dst):
    if src.is_dir() and dst.exists():
        for item in list(src.iterdir()): merge(item, dst/item.name)
        src.rmdir()
    else: shutil.move(str(src), str(dst))
cli = sdk/'cmdline-tools/latest'
if not (cli/'bin/sdkmanager').exists():
    archive = cache/'android-cli.zip'
    download('https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip', archive)
    with zipfile.ZipFile(archive) as z: z.extractall(sdk/'cmdline-tools')
    (sdk/'cmdline-tools/cmdline-tools').rename(cli)
    for p in (cli/'bin').iterdir(): p.chmod(0o755)
subprocess.run([str(cli/'bin/sdkmanager'), f'--sdk_root={sdk}', 'platforms;android-35',
                'build-tools;35.0.0', 'ndk;27.1.12297006', 'cmake;3.22.1'],
               input='y\n'*100, env=env, text=True, check=True)
# Some SDK installations retain the archive's top-level platform directory.
platform = sdk/'platforms/android-35'
nested_platform = platform/'android-35'
if not (platform/'source.properties').exists() and (nested_platform/'source.properties').exists():
    for p in list(nested_platform.iterdir()): merge(p, platform/p.name)
    nested_platform.rmdir()
if not (platform/'android.jar').is_file(): raise SystemExit('Android platform installation incomplete')
ndk = sdk/'ndk/27.1.12297006'
nested = ndk/'android-ndk-r27b'
if not (ndk/'source.properties').exists() and (nested/'source.properties').exists():
    for p in list(nested.iterdir()): merge(p, ndk/p.name)
    nested.rmdir()
if not (ndk/'source.properties').exists(): raise SystemExit('NDK installation incomplete')
for version, archive_root in [('35.0.0', 'android-15'), ('34.0.0', 'android-14')]:
    build_tool = sdk/'build-tools'/version
    nested_tool = build_tool/archive_root
    if not (build_tool/'source.properties').exists() and (nested_tool/'source.properties').exists():
        for p in list(nested_tool.iterdir()): merge(p, build_tool/p.name)
        nested_tool.rmdir()
gradle = cache/'gradle-8.9/bin/gradle'
if not gradle.exists():
    archive = cache/'gradle-8.9-bin.zip'
    download('https://services.gradle.org/distributions/gradle-8.9-bin.zip', archive)
    with zipfile.ZipFile(archive) as z: z.extractall(cache)
gradle.chmod(0o755)
subprocess.run(['git', '-C', str(root.parent/'gbarecomp'), 'submodule', 'update', '--init',
                'platform/android/third_party/SDL'], check=True)
(root/'android/local.properties').write_text(f'sdk.dir={sdk}\n')
# Each developer owns a local signing key; never commit it.
key = root/'android/debug-development.keystore'
if not key.exists():
    subprocess.run(['keytool', '-genkeypair', '-keystore', str(key), '-storepass', 'android',
                    '-keypass', 'android', '-alias', 'androiddebugkey', '-dname',
                    'CN=Android Debug,O=Android,C=US', '-keyalg', 'RSA', '-keysize', '2048',
                    '-validity', '10000'], check=True)
if args.tools_only: raise SystemExit(0)
subprocess.run([str(gradle), *flags, '--no-daemon', '--max-workers=2', (':app:externalNativeBuildDebug' if args.native_only else ':app:assembleDebug'),
                '-PgbaAbis=arm64-v8a', '-PgbaNativeJobs=2',
                ],
               cwd=root/'android', env=env, check=True)
