"""Optional Qt framework/QML deployment and static Mach-O linkage audit."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

MACHO={bytes.fromhex(value) for value in ('feedface','feedfacf','cefaedfe','cffaedfe','cafebabe','cafebabf','bebafeca','bfbafeca')}
QT_DEPENDENCY=re.compile(r'(^|/)(Qt[^/]+\.framework/|libQt[56][^/]*\.dylib)')


def toolchain(path):
    lines=path.read_text().splitlines()
    if len(lines)!=2:raise ValueError('Invalid Qt tool information; rebuild with just toolkit-bundle')
    qtpaths=Path(lines[0])
    reply=subprocess.run([str(qtpaths),'--query'],check=True,capture_output=True,text=True,timeout=15)
    values=dict(line.split(':',1) for line in reply.stdout.splitlines() if ':' in line)
    if values.get('QT_VERSION')!=lines[1]:raise ValueError('Qt deployment tools do not match the viewer build version')
    for key in ('QT_INSTALL_BINS','QT_INSTALL_PREFIX'):
        if not values.get(key) or not Path(values[key]).is_absolute():raise ValueError(f'Qt did not report a valid {key}')
    deploy=Path(values['QT_INSTALL_BINS'])/'macdeployqt'
    if not deploy.is_file() or not os.access(deploy,os.X_OK):raise ValueError(f'macdeployqt is missing: {deploy}')
    return dict(version=values['QT_VERSION'],prefix=values['QT_INSTALL_PREFIX'],docs=values.get('QT_INSTALL_DOCS',''),tool=str(deploy))


def dependencies(text):
    return sorted({line.strip().split(' (compatibility version',1)[0]
        for line in text.splitlines() if line.startswith('\t') and ' (compatibility version' in line})


def rpaths(text):
    result=[];pending=False
    for line in text.splitlines():
        line=line.strip()
        if line.startswith('cmd '):pending=line=='cmd LC_RPATH'
        elif pending and line.startswith('path '):
            result.append(line[5:].rsplit(' (offset ',1)[0]);pending=False
    return result


def _expand(path,binary,viewer):
    if path.startswith('@loader_path/'):return binary.parent/path[len('@loader_path/'):]
    if path.startswith('@executable_path/'):return viewer.parent/path[len('@executable_path/'):]
    if path=='@loader_path':return binary.parent
    if path=='@executable_path':return viewer.parent
    return Path(path) if Path(path).is_absolute() else None


def _resolve(dependency,binary,viewer,search):
    if dependency.startswith('@rpath/'):
        suffix=dependency[len('@rpath/'):]
        return [root/suffix for root in search]
    path=_expand(dependency,binary,viewer)
    return [path] if path is not None else []


def audit(bundle,viewer,*,otool=Path('/usr/bin/otool')):
    bundle=bundle.resolve();viewer=viewer.resolve()
    def inspect(path,mode):
        return subprocess.run([str(otool),mode,str(path)],check=True,capture_output=True,text=True,timeout=15).stdout
    main_search=[root for value in rpaths(inspect(viewer,'-l')) if (root:=_expand(value,viewer,viewer)) is not None]
    binaries=[];external=set();failures=[];qt_links=0
    for binary in sorted(bundle.rglob('*')):
        if binary.is_symlink() or not binary.is_file():continue
        with binary.open('rb') as file:magic=file.read(4)
        if magic not in MACHO:continue
        search=[root for value in rpaths(inspect(binary,'-l')) if (root:=_expand(value,binary,viewer)) is not None]+main_search
        links=dependencies(inspect(binary,'-L'));relative=str(binary.relative_to(bundle))
        for dependency in links:
            if dependency.startswith(('/System/Library/','/usr/lib/')):continue
            candidates=_resolve(dependency,binary,viewer,search)
            internal=any(candidate.resolve().is_relative_to(bundle) and candidate.is_file() for candidate in candidates)
            if QT_DEPENDENCY.search(dependency):
                qt_links+=1
                if dependency.startswith('/') or not internal:failures.append(f'{relative}: {dependency}')
            elif not internal:external.add(dependency)
        binaries.append(dict(path=relative,dependencies=links))
    if not any(row['path']==str(viewer.relative_to(bundle)) for row in binaries):raise ValueError('Viewer is not a recognized Mach-O executable')
    if not qt_links:raise ValueError('No Qt linkage was found in the deployed bundle')
    if failures:raise ValueError('Qt dependencies remain external or unresolved:\n'+'\n'.join(failures[:20]))
    return dict(schema=1,qt_links_resolved=qt_links,binaries=binaries,external_non_qt=sorted(external),runtime_load_test='pending native Mac review')


def notices(info,payload,extra=None):
    prefix=Path(info['prefix'])
    sources=[prefix/'LICENSES',prefix/'share/qt/licenses',prefix/'share/licenses',prefix/'share/doc/qt6/LICENSES']
    if info['docs']:sources.append(Path(info['docs'])/'LICENSES')
    if extra is not None:
        if not extra.is_dir():raise ValueError(f'Qt notices directory is missing: {extra}')
        sources.insert(0,extra)
    copied=[];seen=set()
    for source in sources:
        if not source.is_dir() or source.resolve() in seen:continue
        seen.add(source.resolve())
        destination=payload/'notices/qt'/str(len(copied)+1)
        files=0
        for file in sorted(source.rglob('*')):
            if not file.is_file():continue
            target=destination/file.relative_to(source);target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(file,target);files+=1
        if files:copied.append(dict(source=str(source),destination=str(destination.relative_to(payload)),files=files))
    return copied


def deploy(bundle,payload,info,qml_directory,*,notice_directory=None):
    viewer=bundle/'Contents/MacOS/workbench-ui-study'
    descriptor,log=tempfile.mkstemp(prefix='qt-deploy-',suffix='.log',dir=bundle.parent.parent)
    try:
        with os.fdopen(descriptor,'w') as output:
            subprocess.run([info['tool'],str(bundle),'-executable='+str(viewer),'-qmldir='+str(qml_directory),
                '-always-overwrite','-no-strip'],stdout=output,stderr=subprocess.STDOUT,check=True,timeout=300)
        for path in ('Contents/Frameworks/QtCore.framework','Contents/Frameworks/QtGui.framework','Contents/Frameworks/QtQuick.framework',
                     'Contents/PlugIns/platforms/libqcocoa.dylib','Contents/Resources/qml/QtQuick/Controls/Basic/qmldir',
                     'Contents/Resources/qml/QtQuick/Dialogs/qmldir','Contents/Resources/qt.conf'):
            if not (bundle/path).exists():raise ValueError(f'Qt deployment is incomplete: missing {path}')
        report=audit(bundle,viewer)
        report.update(qt_version=info['version'],deployment_tool=info['tool'],python_bundled=False,
                      qt_notices=notices(info,payload,notice_directory))
        report['notices_review_required']=True
        (payload/'qt-dependencies.json').write_text(json.dumps(report,indent=2)+'\n')
        shutil.copy2(log,payload/'qt-deploy.log')
        return report
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        raise ValueError(f'Qt deployment failed: {error}. Deployment log: {log}') from error
