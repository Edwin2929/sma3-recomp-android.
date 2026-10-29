#!/usr/bin/env python3
"""Compile the production color UI and run Android host tests with Robolectric.

Requires JDK 17, Gradle 8.9 and an Android 35 android.jar. Dependencies are
downloaded from Maven Central/Google on first use. Captures are Android host
renders, not screenshots from a physical phone. JNI is replaced by a controlled
stand-in so UI commits/cancellation can be observed without loading ARM64 code.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--java-home', type=Path, required=True)
    parser.add_argument('--android-jar', type=Path, required=True)
    parser.add_argument('--gradle', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment['JAVA_HOME'] = str(args.java_home.resolve())
    environment['GRADLE_USER_HOME'] = str(work / 'gradle-cache')
    (work / 'settings.gradle').write_text("rootProject.name='sma3-yoshi-ui-validation'\n")
    (work / 'build.gradle').write_text("""repositories { mavenCentral(); google() }
configurations { uiRuntime {
    attributes { attribute(Attribute.of('org.gradle.jvm.environment', String), 'standard-jvm') }
} }
dependencies { uiRuntime 'org.robolectric:robolectric:4.16.1'; uiRuntime 'junit:junit:4.13.2' }
tasks.register('resolveUiRuntime', Copy) {
    from configurations.uiRuntime
    into layout.buildDirectory.dir('dependencies')
}
""")

    def run(name, command):
        with (work / (name + '.log')).open('w') as log:
            result = subprocess.run(list(map(str, command)), cwd=work, env=environment,
                                    stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print((work / (name + '.log')).read_text()[-8000:])
            raise SystemExit(result.returncode)

    run('dependencies', [args.gradle.resolve(), '--no-daemon', '--console=plain', 'resolveUiRuntime'])
    libraries = sorted((work / 'build/dependencies').glob('*.jar'))
    for archive in sorted((work / 'build/dependencies').glob('*.aar')):
        target = archive.with_suffix('.aar.classes.jar')
        with zipfile.ZipFile(archive) as source:
            target.write_bytes(source.read('classes.jar'))
        libraries.append(target)
    fixture = work / 'fixtures/org/gbarecomp'
    fixture.mkdir(parents=True, exist_ok=True)
    (fixture / 'ModsUi.java').write_text("""package org.gbarecomp;
import android.app.AlertDialog;
final class ModsUi {
    interface Translator { String text(String es, String en, String pt); }
    interface Presenter { void open(AlertDialog dialog, Runnable parent); }
}
""")
    (fixture / 'GbaNative.java').write_text("""package org.gbarecomp;
final class GbaNative {
    static int calls, rgb;
    static boolean enabled, fail;
    static void reset() { calls=0; rgb=0; enabled=false; fail=false; }
    static void setYoshiColor(boolean on, int value) {
        ++calls;
        if (fail) throw new UnsatisfiedLinkError("controlled test failure");
        enabled=on; rgb=value;
    }
}
""")
    sources = [root / 'android/java/org/gbarecomp' / name for name in
               ['YoshiColorUi.java', 'YoshiColorModel.java', 'SettingsPanel.java']]
    sources += [root / 'validation/YoshiColorUiTest.java'] + sorted(fixture.glob('*.java'))
    classes = work / 'classes'
    classes.mkdir(exist_ok=True)
    compile_path = os.pathsep.join(map(str, [args.android_jar.resolve()] + libraries))
    run('compile', [args.java_home / 'bin/javac', '-encoding', 'UTF-8', '-source', '17',
                    '-target', '17', '-cp', compile_path, '-d', classes] + sources)
    # The SDK jar supplies signatures while JUnit discovers the test class.
    # Robolectric loads its instrumented framework in the test sandbox.
    runtime_path = os.pathsep.join(map(str, [classes, args.android_jar.resolve()] + libraries))
    (work / 'jvm-home').mkdir(exist_ok=True)
    run('tests', [args.java_home / 'bin/java', '-Xmx2g', '-Djava.awt.headless=true',
                  '-Duser.home=' + str(work / 'jvm-home'),
                  '-Drobolectric.dependency.repo.url=https://repo.maven.apache.org/maven2',
                  '-Dsma3.ui.captures=' + str(work / 'captures'),
                  '--add-opens=java.base/java.lang=ALL-UNNAMED',
                  '--add-opens=java.base/java.lang.reflect=ALL-UNNAMED',
                  '-cp', runtime_path, 'org.junit.runner.JUnitCore', 'org.gbarecomp.YoshiColorUiTest'])
    report = {'result': 'PASS', 'framework': 'Robolectric 4.16.1', 'android_api': 34,
              'physical_device': False, 'native_game_loaded': False,
              'production_ui_sources': [str(p.relative_to(root)) for p in sources if p.is_relative_to(root)],
              'captures': [p.name for p in sorted((work / 'captures').glob('*.png'))],
              'test_output': (work / 'tests.log').read_text()}
    (work / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['test_output'])


if __name__ == '__main__':
    main()
