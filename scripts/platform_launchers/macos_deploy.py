"""Optional Qt framework/QML deployment and static Mach-O linkage audit."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
from .macos_tools import progress,run_tool

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
    # Supported switches vary across Qt versions; older tools keep their defaults.
    help_reply=subprocess.run([str(deploy)],capture_output=True,text=True,timeout=15)
    if help_reply.returncode not in (0,1):raise ValueError(f'Cannot inspect macdeployqt options: {deploy}')
    help_text=help_reply.stdout+'\n'+help_reply.stderr
    options=[]
    if re.search(r'(?m)^\s*-no-codesign(?:\s|$)',help_text):options.append('-no-codesign')
    if re.search(r'(?m)^\s*-exclude-plugin-types=',help_text):options.append('-exclude-plugin-types=qmltooling')
    return dict(version=values['QT_VERSION'],prefix=values['QT_INSTALL_PREFIX'],docs=values.get('QT_INSTALL_DOCS',''),tool=str(deploy),deployment_options=options)


def load_commands(text):
    """Separate dylib identity from imports; otool -L includes both."""
    result=dict(rpaths=[],install_ids=[],dependencies=[])
    imports={'LC_LOAD_DYLIB','LC_LOAD_WEAK_DYLIB','LC_REEXPORT_DYLIB','LC_LOAD_UPWARD_DYLIB','LC_LAZY_LOAD_DYLIB'}
    command=None;pending=None;seen=False
    for raw in text.splitlines():
        line=raw.strip()
        if line.startswith('cmd '):
            if pending:raise ValueError(f'Incomplete Mach-O {command} command')
            command=line[4:];seen=True
            if command=='LC_RPATH':pending=('path ','rpaths')
            elif command=='LC_ID_DYLIB':pending=('name ','install_ids')
            elif command in imports:pending=('name ','dependencies')
        elif pending and line.startswith(pending[0]):
            value=line[len(pending[0]):].rsplit(' (offset ',1)
            if len(value)!=2 or not value[0]:raise ValueError(f'Invalid Mach-O {command} path')
            result[pending[1]].append(value[0]);pending=None
    if pending:raise ValueError(f'Incomplete Mach-O {command} command')
    if not seen:raise ValueError('No Mach-O load commands were reported')
    return {key:list(dict.fromkeys(values)) for key,values in result.items()}


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
    started=time.monotonic();last=started
    progress('Checking deployed library links…')
    main=load_commands(inspect(viewer,'-l'))
    main_search=[root for value in main['rpaths'] if (root:=_expand(value,viewer,viewer)) is not None]
    binaries=[];external=set();failures=[];qt_links=0
    for binary in sorted(bundle.rglob('*')):
        if binary.is_symlink() or not binary.is_file():continue
        with binary.open('rb') as file:magic=file.read(4)
        if magic not in MACHO:continue
        now=time.monotonic()
        if not binaries or now-last>=5:
            progress(f'Checking library {len(binaries)+1}: {binary.relative_to(bundle)} ({now-started:.0f}s elapsed)');last=now
        commands=main if binary==viewer else load_commands(inspect(binary,'-l'))
        search=[root for value in commands['rpaths'] if (root:=_expand(value,binary,viewer)) is not None]+main_search
        links=commands['dependencies'];relative=str(binary.relative_to(bundle))
        for dependency in links:
            if dependency.startswith(('/System/Library/','/usr/lib/')):continue
            candidates=_resolve(dependency,binary,viewer,search)
            internal=any(candidate.resolve().is_relative_to(bundle) and candidate.is_file() for candidate in candidates)
            if QT_DEPENDENCY.search(dependency):
                qt_links+=1
                if dependency.startswith('/') or not internal:failures.append(f'{relative}: {dependency}')
            elif not internal:external.add(dependency)
        binaries.append(dict(path=relative,dependencies=links,install_ids=commands['install_ids']))
    if not any(row['path']==str(viewer.relative_to(bundle)) for row in binaries):raise ValueError('Viewer is not a recognized Mach-O executable')
    if not qt_links:raise ValueError('No Qt linkage was found in the deployed bundle')
    if failures:raise ValueError('Qt dependencies remain external or unresolved:\n'+'\n'.join(failures[:20]))
    progress(f'Library checks complete: {len(binaries)} Mach-O files, {qt_links} Qt links')
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
        progress(f'Preserving Qt notices: {source}')
        destination=payload/'notices/qt'/str(len(copied)+1)
        files=0
        for file in sorted(source.rglob('*')):
            if not file.is_file():continue
            target=destination/file.relative_to(source);target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(file,target);files+=1
        if files:copied.append(dict(source=str(source),destination=str(destination.relative_to(payload)),files=files))
    return copied


def configure_paths(bundle):
    """Own the loader paths instead of depending on macdeployqt's Qt version."""
    resources=bundle/'Contents/Resources'
    contents='[Paths]\nPrefix = .\nLibraries = Frameworks\nPlugins = PlugIns\nQmlImports = Resources/qml\nTranslations = Resources/translations\n'
    # qt6.conf takes priority if a tool/build supplied it. Keep both consistent.
    names=['qt.conf']
    versioned=resources/'qt6.conf'
    if versioned.exists() or versioned.is_symlink():names.append('qt6.conf')
    for name in names:
        descriptor,temporary=tempfile.mkstemp(prefix='.workbench-qt-',dir=resources)
        try:
            with os.fdopen(descriptor,'w') as output:output.write(contents)
            os.chmod(temporary,0o644)
            # A copied tool-generated config may be read-only or a symlink.
            # Replace its staged inode without writing through to its source.
            os.replace(temporary,resources/name)
        finally:Path(temporary).unlink(missing_ok=True)
    progress('Configured bundled Qt plugin and QML paths')


def deploy(bundle,payload,info,qml_directory,*,notice_directory=None,qml_deploy_script=None):
    viewer=bundle/'Contents/MacOS/band-ui'
    phases={};qml_report=dict(mode='macdeployqt-recursive');phase_started=time.monotonic()
    descriptor,log=tempfile.mkstemp(prefix='qt-deploy-',suffix='.log',dir=bundle.parent.parent)
    try:
        with os.fdopen(descriptor,'w') as output:
            # Every stage is fresh. Forced overwrite repeatedly replaces shared
            # libraries while Qt deploys successive QML plugins (notably Homebrew
            # ICU/OpenSSL dependencies), repeating copy/link work.
            qml_options=['-qmldir='+str(qml_directory)]
            if qml_deploy_script is not None:
                from .macos_qml import deploy as deploy_imports
                qml_report=deploy_imports(bundle,qml_deploy_script,output,log)
                phases['qml_imports']=time.monotonic()-phase_started;phase_started=time.monotonic()
                qml_options=[]
            # With no -qmldir, macdeployqt otherwise auto-scans the working directory.
            # Use an empty private directory for the selective trial.
            with tempfile.TemporaryDirectory(prefix='.qt-tools-',dir=bundle.parent) as cwd:
                run_tool([info['tool'],str(bundle),'-executable='+str(viewer),*qml_options,
                    '-no-strip','-verbose=3',*info.get('deployment_options',[])],output,
                    'Deploying Qt frameworks and QML modules',log,timeout=900,cwd=cwd)
        phases['qt_deployment']=time.monotonic()-phase_started;phase_started=time.monotonic()
        configure_paths(bundle)
        for path in ('Contents/Frameworks/QtCore.framework','Contents/Frameworks/QtGui.framework','Contents/Frameworks/QtQuick.framework',
                     'Contents/PlugIns/platforms/libqcocoa.dylib','Contents/Resources/qml/QtQuick/Controls/Basic/qmldir',
                     'Contents/Resources/qml/QtQuick/Dialogs/qmldir','Contents/Resources/qt.conf'):
            if not (bundle/path).exists():raise ValueError(f'Qt deployment is incomplete: missing {path}')
        phases['configuration']=time.monotonic()-phase_started;phase_started=time.monotonic()
        report=audit(bundle,viewer)
        phases['linkage_audit']=time.monotonic()-phase_started;phase_started=time.monotonic()
        report.update(qt_version=info['version'],deployment_tool=info['tool'],python_bundled=False,
                      qt_notices=notices(info,payload,notice_directory),deployment_options=info.get('deployment_options',[]))
        phases['notices']=time.monotonic()-phase_started
        report['phases_seconds']=phases
        report['qml_deployment']=qml_report
        report['notices_review_required']=True
        (payload/'qt-dependencies.json').write_text(json.dumps(report,indent=2)+'\n')
        shutil.copy2(log,payload/'qt-deploy.log')
        return report
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        raise ValueError(f'Qt deployment failed: {error}. Deployment log: {log}') from error
