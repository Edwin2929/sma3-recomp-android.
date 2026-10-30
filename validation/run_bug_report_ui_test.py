#!/usr/bin/env python3
"""Qualify the local report composer with production Java and Robolectric.

Requires JDK 17, Android 35 signatures, Robolectric 4.16.1/JUnit 4.13.2
dependencies and the API 34 instrumented framework cache. The catalog JNI is a
controlled fixture. Browser/share intents are intercepted; nothing is posted.
Captures are Android host renders, not physical-device screenshots.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--java-home', type=Path, required=True)
    parser.add_argument('--android-jar', type=Path, required=True)
    parser.add_argument('--dependencies-from', type=Path, required=True)
    parser.add_argument('--robolectric-cache', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = args.work_dir.resolve()
    if work.is_relative_to(root):
        parser.error('Keep generated fixtures, logs and screenshots outside the repository')
    work.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment['JAVA_HOME'] = str(args.java_home.resolve())

    def run(name, command):
        with (work / (name + '.log')).open('w') as log:
            result = subprocess.run(list(map(str, command)), cwd=work, env=environment,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print((work / (name + '.log')).read_text()[-8000:])
            raise SystemExit(result.returncode)

    libraries = sorted(p for p in args.dependencies_from.resolve().glob('*.jar')
                       if not p.name.endswith('.aar.classes.jar'))
    if not any(p.name == 'robolectric-4.16.1.jar' for p in libraries):
        parser.error('Expected resolved Robolectric 4.16.1 dependencies')
    for archive in sorted(args.dependencies_from.resolve().glob('*.aar')):
        target = work / archive.with_suffix('.aar.classes.jar').name
        with zipfile.ZipFile(archive) as source:
            target.write_bytes(source.read('classes.jar'))
        libraries.append(target)
    fixture = work / 'fixtures/org/gbarecomp'
    fixture.mkdir(parents=True, exist_ok=True)
    (fixture / 'GbaNative.java').write_text('''package org.gbarecomp;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
final class GbaNative {
    static volatile int calls;
    static volatile boolean fail, block;
    static volatile String catalog;
    static CountDownLatch entered, release, finished;
    static void reset() {
        calls=0; fail=false; block=false; catalog="{\\"ok\\":true,\\"mods\\":[]}";
        entered=new CountDownLatch(1); release=new CountDownLatch(1); finished=new CountDownLatch(1);
    }
    static String listMods() {
        ++calls; entered.countDown();
        try {
            if(block && !release.await(5, TimeUnit.SECONDS)) throw new IllegalStateException("controlled fixture timeout");
            if(fail) throw new UnsatisfiedLinkError("controlled missing JNI");
            return catalog;
        } catch(InterruptedException interrupted) {
            Thread.currentThread().interrupt(); return "{\\"ok\\":false,\\"mods\\":[]}";
        } finally { finished.countDown(); }
    }
}
''')
    names = ['BugReportModel.java', 'BugReportCollector.java', 'BugReportUi.java',
             'GbaGameConfig.java', 'PresentationStats.java', 'SettingsPanel.java']
    sources = [root / 'android/java/org/gbarecomp' / name for name in names]
    sources += [root / 'validation/BugReportUiTest.java'] + sorted(fixture.glob('*.java'))
    classes = work / 'classes'
    classes.mkdir(exist_ok=True)
    compile_path = os.pathsep.join(map(str, [args.android_jar.resolve()] + libraries))
    run('compile', [args.java_home / 'bin/javac', '-encoding', 'UTF-8', '-source', '17', '-target', '17',
                    '-cp', compile_path, '-d', classes] + sources)
    run('model-compile', [args.java_home / 'bin/javac', '-encoding', 'UTF-8', '-d', classes,
                          root / 'android/java/org/gbarecomp/BugReportModel.java',
                          root / 'validation/BugReportModelTest.java'])
    run('model-test', [args.java_home / 'bin/java', '-cp', classes, 'org.gbarecomp.BugReportModelTest'])
    runtime_path = os.pathsep.join(map(str, [classes, args.android_jar.resolve()] + libraries))
    run('tests', [args.java_home / 'bin/java', '-Xmx2g', '-Djava.awt.headless=true',
                  '-Duser.home=' + str(args.robolectric_cache.resolve()),
                  '-Drobolectric.dependency.repo.url=https://repo.maven.apache.org/maven2',
                  '-Dsma3.ui.captures=' + str(work / 'captures'),
                  '--add-opens=java.base/java.lang=ALL-UNNAMED',
                  '--add-opens=java.base/java.lang.reflect=ALL-UNNAMED',
                  '-cp', runtime_path, 'org.junit.runner.JUnitCore', 'org.gbarecomp.BugReportUiTest'])
    model_output = (work / 'model-test.log').read_text()
    ui_output = (work / 'tests.log').read_text()
    count = int(re.search(r'OK \((\d+) tests\)', ui_output).group(1))
    public_sources = [p for p in sources if p.is_relative_to(root)] + [
        root / 'validation/BugReportModelTest.java', Path(__file__).resolve()]
    report = {
        'result': 'PASS', 'version': '0.10.16-beta', 'scope': 'Android host UI and pure report composer; no physical phone or game JNI execution',
        'framework': 'Robolectric 4.16.1', 'android_api': 34,
        'physical_device': False, 'native_game_loaded': False,
        'external_intents_intercepted': True, 'reports_sent_or_issues_submitted': 0,
        'model_checks': int(re.search(r'checks=(\d+)', model_output).group(1)),
        'ui_tests': count,
        'cases': [
            'Explicit preference allowlist excludes arbitrary strings and private paths',
            'UTF-8, control characters, immutable snapshots and size boundaries',
            'Requested FPS and latest observed sample distinguished',
            'Editable complete preview preserved across navigation and share return',
            'Copy and ACTION_SEND contain exactly the reviewed text, with no attachment',
            'GitHub draft URL preserves Unicode; large reports use full clipboard without body truncation',
            'Setup screen works without JNI; native catalog contributes aggregate counts only',
            'Abandoned preparation does not reopen a page; background completion waits for resume',
            'English, Spanish and Portuguese form/actions/template',
            'Portrait and landscape keep navigation outside the scroll body'
        ],
        'captures': [p.name for p in sorted((work / 'captures').glob('*.png'))],
        'source_sha256': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in public_sources},
        'native_fixture_sha256': hashlib.sha256((fixture / 'GbaNative.java').read_bytes()).hexdigest(),
        'model_output': model_output, 'test_output': ui_output,
        'limits': ['Host framework intercepts external apps; actual clipboard, share destinations and browser behavior require physical Android testing.']
    }
    (work / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(model_output.strip()); print(ui_output.strip())


if __name__ == '__main__':
    main()
