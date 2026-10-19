"""Run Qt's actual import deployment; this is not native Mac linkage validation."""
from pathlib import Path
import subprocess
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
    environment.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QSG_RHI_BACKEND='software')
    subprocess.run([str(ROOT/'.local/build/qt-input/workbench-qml-deployment-probe'),str(qml)],env=environment,check=True,timeout=30)
    modules=sorted(str(path.parent.relative_to(qml)) for path in qml.rglob('qmldir'))
    print('PASS: real Qt CMake import closure includes Basic/dialogs and omits unrelated modules; '+str(len(modules))+' modules deployed')
    print('Modules: '+', '.join(modules))
