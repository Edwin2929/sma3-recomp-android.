#!/usr/bin/env python3
"""Exercise production timer UI/storage using controlled native evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--java-home',type=Path,required=True)
    parser.add_argument('--android-jar',type=Path,required=True)
    parser.add_argument('--dependencies-from',type=Path,required=True,
                        help='Resolved Robolectric 4.16.1 and JUnit 4.13.2 runtime dependencies')
    parser.add_argument('--robolectric-cache',type=Path,required=True)
    parser.add_argument('--work-dir',type=Path,required=True)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];work=args.work_dir.resolve()
    work.mkdir(parents=True,exist_ok=True)
    libraries=sorted(args.dependencies_from.resolve().glob('*.jar'))
    for archive in sorted(args.dependencies_from.resolve().glob('*.aar')):
        target=work/archive.with_suffix('.aar.classes.jar').name
        with zipfile.ZipFile(archive) as source:target.write_bytes(source.read('classes.jar'))
        libraries.append(target)
    classes=work/'classes';classes.mkdir(exist_ok=True)
    sources=[root/'android/java/org/gbarecomp'/name for name in
             ['TimeTrialModel.java','TimeTrialStore.java','TimeTrialUi.java','SettingsPanel.java','GbaNative.java']]
    sources += [root/'validation/TimeTrialModelTest.java',root/'validation/TimeTrialUiTest.java']
    classpath=os.pathsep.join(map(str,[classes,args.android_jar.resolve()]+libraries))
    def run(name,command):
        with (work/(name+'.log')).open('w') as log:
            result=subprocess.run(list(map(str,command)),cwd=work,stdout=log,stderr=subprocess.STDOUT)
        if result.returncode:
            print((work/(name+'.log')).read_text()[-10000:]);raise SystemExit(result.returncode)
    run('compile',[args.java_home/'bin/javac','-encoding','UTF-8','-source','17','-target','17','-cp',classpath,'-d',classes]+sources)
    run('model',[args.java_home/'bin/java','-cp',classes,'org.gbarecomp.TimeTrialModelTest'])
    run('tests',[args.java_home/'bin/java','-Xmx2g','-Djava.awt.headless=true',
        '-Duser.home='+str(args.robolectric_cache.resolve()),
        '-Drobolectric.dependency.repo.url=https://repo.maven.apache.org/maven2',
        '-Dsma3.ui.captures='+str(work/'captures'),
        '--add-opens=java.base/java.lang=ALL-UNNAMED','--add-opens=java.base/java.lang.reflect=ALL-UNNAMED',
        '-cp',classpath,'org.junit.runner.JUnitCore','org.gbarecomp.TimeTrialUiTest'])
    report={'result':'PASS','scope':'Production Android UI/store with controlled JNI snapshots; no physical phone or ARM64 game',
            'framework':'Robolectric 4.16.1','android_api':34,'physical_device':False,
            'model_output':(work/'model.log').read_text(),'test_output':(work/'tests.log').read_text(),
            'captures':[p.name for p in sorted((work/'captures').glob('*.png'))],
            'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}}
    (work/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['model_output']);print(report['test_output'])

if __name__=='__main__':main()
