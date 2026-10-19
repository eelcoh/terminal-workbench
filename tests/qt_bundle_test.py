#!/usr/bin/env python3
"""Portable deployment fixtures; never claim a native macdeployqt/loader test."""
import importlib.util
import configparser
import shutil
import json
import os
from pathlib import Path
import plistlib
import re
import sys
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from platform_launchers import macos,macos_deploy as deploy
spec=importlib.util.spec_from_file_location('stage_toolkit',ROOT/'scripts/stage-toolkit.py')
stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)
REAL_AUDIT=deploy.audit


def executable(path,text):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text('#!'+sys.executable+'\n'+text);path.chmod(0o755)
    return path


class BundleTest(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='qt bundle ')
        self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name)
        self.prefix=self.root/'Qt install'
        self.bins=self.prefix/'bin';self.bins.mkdir(parents=True)
        self.tools=self.root/'tools.txt'
        qtpaths=executable(self.bins/'qtpaths',f"print('QT_VERSION:6.8.3\\nQT_INSTALL_BINS:{self.bins}\\nQT_INSTALL_PREFIX:{self.prefix}')\n")
        self.tools.write_text(str(qtpaths)+'\n6.8.3\n')
        licenses=self.prefix/'LICENSES';licenses.mkdir();(licenses/'LGPL.txt').write_text('fixture notice')
        self.macdeploy=executable(self.bins/'macdeployqt',r'''
import json,sys
from pathlib import Path
bundle=Path(sys.argv[1])
(bundle/'deploy-args.json').write_text(json.dumps(sys.argv[2:]))
for name in ('QtCore','QtGui','QtQuick'):
    binary=bundle/f'Contents/Frameworks/{name}.framework/Versions/A/{name}'
    binary.parent.mkdir(parents=True,exist_ok=True)
    binary.write_bytes(bytes.fromhex('cffaedfe')+b'fixture')
for name in ('QtQuick/Controls/Basic','QtQuick/Dialogs'):
    path=bundle/'Contents/Resources/qml'/name/'qmldir'
    path.parent.mkdir(parents=True,exist_ok=True);path.write_text('module fixture')
plugin=bundle/'Contents/PlugIns/platforms/libqcocoa.dylib'
plugin.parent.mkdir(parents=True,exist_ok=True);plugin.write_bytes(bytes.fromhex('cffaedfe'))
''')
        self.otool=executable(self.root/'otool',r'''
import sys
import subprocess
from pathlib import Path
mode,path=sys.argv[1:];binary=Path(path)
if mode=='-l':
    print('cmd LC_RPATH\npath @executable_path/../Frameworks (offset 12)')
else:
    print(path+':')
    link='@rpath/QtCore.framework/Versions/A/QtCore'
    if binary.name=='workbench-preview':link='/usr/lib/libSystem.B.dylib'
    if (binary.parent/'external-qt').exists():link='/opt/qt/QtCore.framework/Versions/A/QtCore'
    if (binary.parent/'missing-qt').exists():link='@rpath/QtMissing.framework/Versions/A/QtMissing'
    print('\t'+link+' (compatibility version 6.0.0, current version 6.8.3)')
    if (binary.parent/'external-other').exists():
        print('\t/opt/local/lib/libfixture.dylib (compatibility version 1.0.0, current version 1.0.0)')
''')
        self.source=self.root/'source/workbench-ui-study.app'
        self.viewer=self.source/'Contents/MacOS/workbench-ui-study'
        self.viewer.parent.mkdir(parents=True);self.viewer.write_bytes(bytes.fromhex('cffaedfe'))
        self.plist=self.source/'Contents/Info.plist'
        self.plist.write_bytes(plistlib.dumps({'CFBundleExecutable':'workbench-ui-study'}));self.plist.chmod(0o444)
        self.runtime=executable(self.root/'runtime',"print('fixture')\n")
        self.bootstrap=self.root/'bootstrap';self.bootstrap.write_bytes(bytes.fromhex('cffaedfe'))
        self.parent=self.root/'staged'

    def build(self,**kwargs):
        def audit(bundle,viewer):return REAL_AUDIT(bundle,viewer,otool=self.otool)
        with patch.object(stage,'sys',SimpleNamespace(platform='darwin',executable=sys.executable)),patch.object(deploy,'audit',audit):
            return stage.stage(self.parent,self.runtime,self.viewer,self.bootstrap,qt_tools=self.tools,**kwargs)

    def check_paths(self,bundle):
        config=configparser.ConfigParser();config.read(bundle/'Contents/Resources/qt.conf')
        paths=config['Paths'];self.assertEqual(paths['Prefix'],'.')
        self.assertEqual(paths['QmlImports'],'Resources/qml')
        self.assertEqual(paths['Plugins'],'PlugIns')
        self.assertEqual(paths['Libraries'],'Frameworks')
        for key in ('Plugins','QmlImports','Libraries'):
            target=(bundle/'Contents'/paths['Prefix']/paths[key]).resolve()
            self.assertTrue(target.is_relative_to(bundle.resolve()))
            self.assertTrue(target.is_dir())

    def test_missing_tool_config_is_generated_and_paths_survive_relocation(self):
        directory=self.build(bundle_qt=True,sign=False)
        bundle=directory/macos.NAME;self.check_paths(bundle)
        moved=self.root/'moved app'/macos.NAME;moved.parent.mkdir()
        shutil.move(bundle,moved);self.check_paths(moved)

    def test_tool_configs_are_replaced_without_following_copied_symlinks(self):
        resources=self.source/'Contents/Resources';resources.mkdir(parents=True)
        original=self.root/'original qt.conf';original.write_text('[Paths]\nPrefix=/installed/Qt\n')
        (resources/'qt.conf').symlink_to(original)
        versioned=resources/'qt6.conf';versioned.write_text('[Paths]\nPrefix=/old/Qt\n');versioned.chmod(0o444)
        directory=self.build(bundle_qt=True,sign=False);bundle=directory/macos.NAME
        self.check_paths(bundle)
        self.assertFalse((bundle/'Contents/Resources/qt.conf').is_symlink())
        self.assertEqual((bundle/'Contents/Resources/qt.conf').read_text(),(bundle/'Contents/Resources/qt6.conf').read_text())
        self.assertIn('/installed/Qt',original.read_text())
        self.assertIn('/old/Qt',versioned.read_text())
        self.assertEqual(versioned.stat().st_mode&0o777,0o444)

    def test_deployment_before_signing_and_source_unchanged(self):
        signed=[]
        def sign(args,*unused,**kwargs):
            bundle=Path(args[-1]);payload=bundle/'Contents/Resources/workbench'
            report=json.loads((payload/'qt-dependencies.json').read_text())
            self.assertGreater(report['qt_links_resolved'],0)
            self.assertFalse(report['python_bundled']);self.assertTrue(report['notices_review_required'])
            self.assertEqual(report['external_non_qt'],[])
            self.assertEqual(report['runtime_load_test'],'pending native Mac review')
            self.assertEqual((payload/'notices/qt/1/LGPL.txt').read_text(),'fixture notice')
            self.assertTrue((payload/'qt-deploy.log').is_file())
            self.check_paths(bundle)
            signed.append(args)
        original=self.plist.read_bytes()
        with patch.object(macos,'run_tool',sign):
            directory=self.build(bundle_qt=True)
        self.assertEqual(len(signed),1)
        bundle=directory/macos.NAME
        args=json.loads((bundle/'deploy-args.json').read_text())
        self.assertIn('-qmldir='+str(ROOT/'native/macos/qt-deployment'),args)
        self.assertNotIn('-always-overwrite',args)
        self.assertNotIn('-no-codesign',args)
        self.assertIn('-verbose=3',args)
        self.assertEqual(self.plist.read_bytes(),original)
        self.assertEqual(self.plist.stat().st_mode&0o777,0o444)
        self.assertFalse((self.source/'Contents/Frameworks').exists())
        self.assertTrue(json.loads((bundle/'Contents/Resources/workbench/preview.json').read_text())['qt_bundled'])

    def test_failure_removes_partial_bundle_keeps_log_and_never_signs(self):
        executable(self.macdeploy,"print('deployment failure')\nraise SystemExit(7)\n")
        with patch.object(macos,'run_tool',lambda *a,**kw:self.fail('signed a failed deployment')):
            with self.assertRaisesRegex(ValueError,'Deployment log:') as failure:self.build(bundle_qt=True)
        log=Path(str(failure.exception).split('Deployment log: ',1)[1])
        self.assertIn('deployment failure',log.read_text())
        self.assertEqual(list(self.parent.glob('qt-preview-*')),[])
        self.assertTrue(self.viewer.is_file())

    def test_signing_timeout_removes_stage_and_preserves_signing_log(self):
        logs=[]
        def stalled(arguments,output,label,log_path,**kwargs):
            self.assertEqual(kwargs['timeout'],120)
            output.write('signing started\n');output.flush();logs.append(Path(log_path))
            raise subprocess.TimeoutExpired(arguments,kwargs['timeout'])
        with patch.object(macos,'run_tool',stalled):
            with self.assertRaisesRegex(ValueError,'Signing log:'):self.build(bundle_qt=True)
        self.assertEqual(list(self.parent.glob('qt-preview-*')),[])
        self.assertIn('signing started',logs[0].read_text())

    def test_failed_linkage_audit_never_signs(self):
        with self.macdeploy.open('a') as file:
            file.write("\n(bundle/'Contents/MacOS/external-qt').touch()\n")
        with patch.object(macos,'run_tool',lambda *a,**kw:self.fail('signed before audit success')):
            with self.assertRaisesRegex(ValueError,'external or unresolved'):self.build(bundle_qt=True)
        self.assertEqual(list(self.parent.glob('qt-preview-*')),[])
        self.assertTrue(list(self.parent.glob('qt-deploy-*.log')))

    def test_missing_plugin_fails(self):
        with self.macdeploy.open('a') as file:file.write("\nplugin.unlink()\n")
        with self.assertRaisesRegex(ValueError,'libqcocoa'):self.build(bundle_qt=True,sign=False)
        self.assertEqual(list(self.parent.glob('qt-preview-*')),[])

    def test_audit_rejects_external_and_unresolved_qt_reports_other_libraries(self):
        directory=self.build(bundle_qt=True,sign=False);bundle=directory/macos.NAME
        viewer=bundle/'Contents/MacOS/workbench-ui-study'
        marker=viewer.parent/'external-other';marker.touch()
        report=REAL_AUDIT(bundle,viewer,otool=self.otool)
        self.assertEqual(report['external_non_qt'],['/opt/local/lib/libfixture.dylib']);marker.unlink()
        for name in ('external-qt','missing-qt'):
            marker=viewer.parent/name;marker.touch()
            with self.assertRaisesRegex(ValueError,'external or unresolved'):REAL_AUDIT(bundle,viewer,otool=self.otool)
            marker.unlink()
        binary=bundle/'Contents/Frameworks/QtCore.framework/Versions/A/QtCore'
        binary.unlink();binary.symlink_to(self.viewer)
        with self.assertRaisesRegex(ValueError,'external or unresolved'):REAL_AUDIT(bundle,viewer,otool=self.otool)

    def test_tool_version_and_missing_deployer(self):
        self.tools.write_text(str(self.bins/'qtpaths')+'\n6.9.0\n')
        with self.assertRaisesRegex(ValueError,'match'):deploy.toolchain(self.tools)
        self.tools.write_text(str(self.bins/'qtpaths')+'\n6.8.3\n');self.macdeploy.unlink()
        with self.assertRaisesRegex(ValueError,'macdeployqt is missing'):deploy.toolchain(self.tools)

    def test_existing_stage_and_linux_rejection(self):
        directory=self.build(bundle_qt=False,sign=False)
        payload=directory/macos.NAME/'Contents/Resources/workbench'
        self.assertFalse(json.loads((payload/'preview.json').read_text())['qt_bundled'])
        self.assertFalse((payload/'qt-dependencies.json').exists())
        parent=self.root/'must not exist'
        with patch.object(stage,'sys',SimpleNamespace(platform='linux')):
            with self.assertRaisesRegex(ValueError,'native Mac'):stage.stage(parent,self.runtime,self.viewer,bundle_qt=True)
        self.assertFalse(parent.exists())

    def test_parsers_and_path_tokens(self):
        self.assertEqual(deploy.dependencies('file (architecture arm64):\n\t@rpath/QtCore.framework/QtCore (compatibility version 6.0.0)\nfile (architecture x86_64):\n\t@rpath/QtCore.framework/QtCore (compatibility version 6.0.0)'),['@rpath/QtCore.framework/QtCore'])
        self.assertEqual(deploy.rpaths('cmd LC_RPATH\npath @loader_path/with spaces (offset 12)\ncmd LC_LOAD_DYLIB\npath ignored (offset 4)'),['@loader_path/with spaces'])
        binary=Path('/app/Contents/PlugIns/plugin');viewer=Path('/app/Contents/MacOS/viewer')
        self.assertEqual(deploy._resolve('@loader_path/lib.dylib',binary,viewer,[]),[binary.parent/'lib.dylib'])
        self.assertEqual(deploy._resolve('@executable_path/lib.dylib',binary,viewer,[]),[viewer.parent/'lib.dylib'])

    def test_deployment_manifest_covers_static_qml_and_dynamic_style(self):
        manifest=(ROOT/'native/macos/qt-deployment/Imports.qml').read_text()
        required={'QtQuick.Controls.Basic'}
        for qml in (ROOT/'spikes/qt-input').glob('*.qml'):
            required.update(re.findall(r'^import (Qt\w+(?:\.\w+)*)',qml.read_text(),re.MULTILINE))
        actual=set(re.findall(r'^import (\S+)',manifest,re.MULTILINE))
        self.assertTrue(required<=actual,required-actual)


if __name__=='__main__':unittest.main()
