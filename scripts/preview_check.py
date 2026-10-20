"""One-shot installed payload checks; no service or terminal creation."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

QT_IMAGE=re.compile(r'(^|/)(Qt[^/]+\.framework/|libQt[56][^/]*\.(?:dylib|so)(?:\.|$))')


def bundled_qt_errors(bundle,report):
    """Validate observed Qt images and strict QML paths, not install IDs."""
    bundle=bundle.resolve()
    images=report.get('loaded_images',[])
    if not isinstance(images,list) or not all(isinstance(path,str) for path in images):
        return ['Viewer did not report a valid loaded-image list']
    plugins={path.name for path in (bundle/'Contents/PlugIns').rglob('*.dylib')}
    observed=[path for path in images if QT_IMAGE.search(path) or Path(path).name in plugins]
    errors=[]
    if not any('QtCore.framework/' in path for path in observed):errors.append('No loaded QtCore framework was observed')
    for path in observed:
        if not Path(path).resolve().is_relative_to(bundle):errors.append('Qt library/plugin loaded outside app: '+path)
    qml=(bundle/'Contents/Resources/qml').resolve()
    paths=report.get('import_paths',[])
    if not isinstance(paths,list) or not paths:errors.append('No QML import paths were reported')
    else:
        for path in paths:
            if not isinstance(path,str):errors.append('Invalid QML import path');continue
            if path.startswith(('qrc:/',':/')):continue
            if not Path(path).resolve().is_relative_to(qml):errors.append('QML import path outside bundled tree: '+path)
    return errors


def check(payload,runtime,viewer,*,platform=None):
    platform=sys.platform if platform is None else platform
    checks=[]
    def record(name,ok,**details):checks.append(dict(name=name,ok=bool(ok),**details))
    metadata={}
    try:
        metadata=json.loads((payload/'preview.json').read_text())
        if not isinstance(metadata,dict) or metadata.get('format')!=1:raise ValueError('Unsupported preview metadata')
        record('metadata',True,source=metadata.get('source'),qt_bundled=metadata.get('qt_bundled',False))
    except (OSError,ValueError) as error:
        metadata={}
        record('metadata',False,error=str(error))
    try:
        # The no-argument usage path exits before opening sockets or PTYs.
        reply=subprocess.run([str(runtime)],capture_output=True,text=True,timeout=15)
        record('runtime_startup',reply.returncode==2 and 'usage: workbench-session ' in reply.stderr,
               exit_code=reply.returncode,stderr=reply.stderr)
    except (OSError,subprocess.SubprocessError) as error:record('runtime_startup',False,error=str(error))
    report=None
    try:
        arguments=[str(viewer),'--check-deployment']
        bundle=payload.parents[2] if platform=='darwin' else None
        if metadata.get('qt_bundled'):
            if bundle is None:raise ValueError('Bundled Qt checking currently requires Mac')
            arguments.append(str(bundle/'Contents/Resources/qml'))
        reply=subprocess.run(arguments,capture_output=True,text=True,timeout=30,
                             env=dict(os.environ,QML_DISABLE_DISK_CACHE='1'))
        try:report=json.loads(reply.stdout)
        except ValueError as error:
            raise ValueError(f'Viewer exited {reply.returncode} without a JSON diagnostic: {reply.stderr}') from error
        if not isinstance(report,dict) or report.get('schema')!=1 or not isinstance(report.get('qml_ok'),bool) or not isinstance(report.get('client_ok'),bool):
            report=None
            raise ValueError('Viewer did not return a valid QML diagnostic')
        record('qml_startup',reply.returncode==0 and report['qml_ok'] and report['client_ok'],report=report,
               exit_code=reply.returncode,stderr=reply.stderr)
    except (OSError,ValueError,subprocess.SubprocessError) as error:record('qml_startup',False,error=str(error))
    if platform=='darwin':
        from platform_launchers.macos import verify_signature
        record('app_signature',**verify_signature(payload.parents[2]))
        if metadata.get('qt_bundled'):
            errors=bundled_qt_errors(payload.parents[2],report) if report else ['Viewer startup failed; loaded Qt could not be checked']
            record('bundled_qt_loading',not errors,errors=errors)
    return dict(schema=1,ok=all(item['ok'] for item in checks),checks=checks,
        python=sys.executable,payload=str(payload),
        scope='One-shot runtime, generic QML and hidden Workbench startup with observed loader paths. No terminals or service changes. Does not open native dialogs or prove rendering, notarization, or standalone Python support.')
