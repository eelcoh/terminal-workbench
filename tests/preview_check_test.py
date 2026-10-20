"""Portable installed-check failures and native-Mac loader policy fixtures."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
import preview_check
from platform_launchers import macos


class PreviewCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb check ')
        self.addCleanup(self.directory.cleanup)
        self.bundle=Path(self.directory.name)/'Moved Preview.app'
        self.payload=self.bundle/'Contents/Resources/workbench';self.payload.mkdir(parents=True)
        (self.payload/'preview.json').write_text(json.dumps(dict(format=1,qt_bundled=True,source='fixture')))
        self.qml=self.bundle/'Contents/Resources/qml';self.qml.mkdir()
        plugin=self.bundle/'Contents/PlugIns/platforms/libqcocoa.dylib';plugin.parent.mkdir(parents=True);plugin.touch()
        self.report=dict(schema=1,qml_ok=True,client_ok=True,import_paths=[str(self.qml),'qrc:/qt-project.org/imports'],
            loaded_images=[str(self.bundle/'Contents/Frameworks/QtCore.framework/Versions/A/QtCore'),str(plugin),'/usr/lib/libSystem.B.dylib'])

    def invoke(self,*,runtime_code=2,viewer_report=None,viewer_code=0,signature=True):
        def run(arguments,**kwargs):
            if arguments==['runtime']:
                return subprocess.CompletedProcess(arguments,runtime_code,'','usage: workbench-session SOCKET [SHELL]')
            self.assertEqual(arguments[:2],['viewer','--check-deployment'])
            if len(arguments)>2:self.assertEqual(arguments[2:],[str(self.qml)])
            return subprocess.CompletedProcess(arguments,viewer_code,json.dumps(self.report if viewer_report is None else viewer_report),'fixture failure' if viewer_code else '')
        with patch.object(preview_check.subprocess,'run',side_effect=run),patch.object(macos,'verify_signature',return_value=dict(ok=signature)):
            return preview_check.check(self.payload,Path('runtime'),Path('viewer'),platform='darwin')

    def test_success_reports_independent_checks_without_payload_writes(self):
        before={str(path):path.read_bytes() for path in self.bundle.rglob('*') if path.is_file()}
        result=self.invoke()
        self.assertTrue(result['ok'],result)
        self.assertEqual([row['name'] for row in result['checks']],['metadata','runtime_startup','qml_startup','app_signature','bundled_qt_loading'])
        self.assertEqual(before,{str(path):path.read_bytes() for path in self.bundle.rglob('*') if path.is_file()})

    def test_loaded_external_qt_rejected_but_non_qt_dependencies_reported(self):
        self.report['loaded_images'].append('/opt/homebrew/lib/libicuuc.dylib')
        self.assertTrue(self.invoke()['ok'])
        self.report['loaded_images'].append('/opt/Qt/QtQuick.framework/Versions/A/QtQuick')
        result=self.invoke();self.assertFalse(result['ok'])
        self.assertIn('outside app',result['checks'][-1]['errors'][0])

    def test_external_plugin_with_bundled_name_rejected(self):
        self.report['loaded_images'].append('/opt/qt/plugins/platforms/libqcocoa.dylib')
        self.assertFalse(self.invoke()['ok'])

    def test_escaping_qml_symlink_rejected(self):
        outside=Path(self.directory.name)/'outside';outside.mkdir()
        (self.qml/'escape').symlink_to(outside,target_is_directory=True)
        self.report['import_paths'].append(str(self.qml/'escape'))
        self.assertFalse(self.invoke()['ok'])

    def test_no_loader_evidence_cannot_pass(self):
        self.report['loaded_images']=[]
        self.assertFalse(self.invoke()['ok'])

    def test_external_import_fallback_rejected(self):
        self.report['import_paths'].append('/opt/Qt/qml')
        self.assertFalse(self.invoke()['ok'])

    def test_failed_runtime_or_viewer_or_signature_keeps_other_results(self):
        for kwargs in (dict(runtime_code=1),dict(viewer_code=1),dict(viewer_report={'schema':1,'qml_ok':False}),dict(signature=False)):
            with self.subTest(kwargs=kwargs):
                result=self.invoke(**kwargs)
                self.assertFalse(result['ok']);self.assertEqual(len(result['checks']),5)

    def test_qml_failure_preserves_component_errors(self):
        self.report.update(qml_ok=False,qml_errors=['module QtQuick.Dialogs is not installed'])
        result=self.invoke(viewer_code=1)
        self.assertFalse(result['ok'])
        qml=next(row for row in result['checks'] if row['name']=='qml_startup')
        self.assertEqual(qml['report']['qml_errors'],self.report['qml_errors'])
        self.assertEqual(qml['exit_code'],1)

    def test_generic_qml_success_does_not_hide_client_failure(self):
        self.report.update(client_ok=False,client={'errors':['qrc:/About.qml: missing UI resource']})
        result=self.invoke()
        self.assertFalse(result['ok'])
        startup=next(row for row in result['checks'] if row['name']=='qml_startup')
        self.assertTrue(startup['report']['qml_ok'])
        self.assertEqual(startup['report']['client']['errors'],self.report['client']['errors'])

    def test_older_generic_only_report_cannot_pass_real_client_check(self):
        del self.report['client_ok']
        result=self.invoke()
        self.assertFalse(result['ok'])
        startup=next(row for row in result['checks'] if row['name']=='qml_startup')
        self.assertIn('valid QML diagnostic',startup['error'])

    def test_invalid_metadata_is_a_failed_check(self):
        (self.payload/'preview.json').write_text('[]')
        result=self.invoke()
        self.assertFalse(result['ok']);self.assertEqual(result['checks'][0]['name'],'metadata')

    def test_signature_check_never_signs(self):
        with patch.object(macos.subprocess,'run',return_value=subprocess.CompletedProcess([],1,'','invalid signature')) as run:
            result=macos.verify_signature(self.bundle)
        arguments=run.call_args.args[0]
        self.assertIn('--verify',arguments);self.assertNotIn('--sign',arguments)
        self.assertFalse(result['ok'])


if __name__=='__main__':unittest.main()
