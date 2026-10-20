"""Run Qt's actual import deployment; this is not native Mac linkage validation."""
from pathlib import Path
import subprocess
import json
import os
import sys
import tempfile

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from platform_launchers.macos_qml import run_import_deployment

script=Path((ROOT/'.local/build/qt-input/workbench-qml-deploy.txt').read_text().strip())
with tempfile.TemporaryDirectory(prefix='wb-qml-imports-') as directory:
    prefix=Path(directory).resolve();log=prefix/'deployment.log'
    with log.open('w') as output:
        try:run_import_deployment(script,prefix,output,log)
        except BaseException:
            print(log.read_text(),file=sys.stderr);raise
    qml=prefix/'workbench-ui-study.app/Contents/Resources/qml' if sys.platform=='darwin' else prefix/'qml'
    for name in ('QtQuick','QtQuick/Controls','QtQuick/Controls/Basic','QtQuick/Dialogs'):
        assert (qml/name/'qmldir').is_file(),f'missing runtime module {name}'
    for name in ('QtQuick/Particles','QtQuick/Shapes/DesignHelpers','QtQml/XmlListModel','QtQml/LocalStorage'):
        assert not (qml/name).exists(),f'unused module copied: {name}'
    environment={key:value for key,value in os.environ.items() if not key.startswith(('QML_','QML2_'))}
    scratch=prefix/'scratch';scratch.mkdir()
    environment.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QSG_RHI_BACKEND='software',QML_DISABLE_DISK_CACHE='1',TMPDIR=str(scratch))
    subprocess.run([str(ROOT/'.local/build/qt-input/workbench-qml-deployment-probe'),str(qml)],env=environment,check=True,timeout=30)
    viewer=ROOT/'.local/build/qt-input/workbench-ui-study'
    if sys.platform=='darwin':viewer=viewer.with_suffix('.app')/'Contents/MacOS/workbench-ui-study'
    result=subprocess.run([str(viewer),'--check-deployment',str(qml)],env=environment,capture_output=True,text=True,timeout=30)
    try:client=json.loads(result.stdout)
    except ValueError:raise AssertionError((result.returncode,result.stdout,result.stderr))
    assert result.returncode==0 and client['qml_ok'] and client['client_ok'],(client,result.stderr)
    assert client['client']==dict(workspace_count=0,terminal_count=0,visible_window_count=0,errors=[]),client
    # Break the import closure. Neither the generic nor actual client check may
    # fall back to modules in the machine's Qt installation.
    controls=qml/'QtQuick/Controls';missing=prefix/'removed-controls';controls.rename(missing)
    broken=subprocess.run([str(viewer),'--check-deployment',str(qml)],env=environment,capture_output=True,text=True,timeout=30)
    rejected=json.loads(broken.stdout)
    assert broken.returncode!=0 and not rejected['qml_ok'] and not rejected['client_ok'],(rejected,broken.stderr)
    missing.rename(controls)
    assert not list(scratch.iterdir()),'client check left private configuration behind'
    modules=sorted(str(path.parent.relative_to(qml)) for path in qml.rglob('qmldir'))
    print('PASS: hidden Workbench and generic controls load from the real Qt import closure; broken closure rejected; '+str(len(modules))+' modules deployed')
    print('Modules: '+', '.join(modules))
