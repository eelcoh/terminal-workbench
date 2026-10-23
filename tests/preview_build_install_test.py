"""No-argument build/install chooses the native bundle and never activates it."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('probe',ROOT/'scripts/toolkit-probe.py')
probe=importlib.util.module_from_spec(spec);spec.loader.exec_module(probe)


class BuildInstallTest(unittest.TestCase):
    def commands(self,platform,fail=False,mode="install"):
        calls=[]
        def run(arguments,**kwargs):
            calls.append(arguments)
            if fail and len(calls)==2:raise subprocess.CalledProcessError(1,arguments)
        with patch.object(sys,'argv',['toolkit-probe.py',mode]),patch.object(sys,'platform',platform),patch.object(probe.subprocess,'run',run):
            if fail:
                with self.assertRaises(subprocess.CalledProcessError):probe.main()
            else:probe.main()
        return calls

    def test_mac_builds_bootstrap_and_selective_bundle_before_install(self):
        calls=self.commands('darwin')
        self.assertIn('-DBAND_TERMINAL_PROBE=ON',calls[0])
        self.assertIn('band-ui',calls[1]);self.assertIn('workbench-preview-bootstrap',calls[1])
        self.assertEqual(calls[2][2:],['--bundle-qt','--selective-qml','--install'])
        self.assertEqual(len(calls),3,'unexpected launch/activation command')

    def test_linux_builds_staged_qt_and_installs_without_mac_flags(self):
        calls=self.commands('linux')
        self.assertIn('band-ui',calls[1]);self.assertNotIn('workbench-preview-bootstrap',calls[1])
        self.assertEqual(calls[2][2:],['--install'])
        self.assertEqual(len(calls),3)

    def test_existing_stage_and_bundle_recipes_keep_their_flags(self):
        for mode,expected in (('stage',[]),('bundle',['--bundle-qt']),('bundle-selective',['--bundle-qt','--selective-qml'])):
            with self.subTest(mode=mode):
                self.assertEqual(self.commands('darwin',mode=mode)[2][2:],expected)

    def test_failed_build_cannot_reach_install_or_activation(self):
        self.assertEqual(len(self.commands('linux',fail=True)),2)

    @unittest.skipUnless(sys.platform.startswith('linux'),'real staged Linux verification failure')
    def test_failed_installed_check_keeps_copies_without_activation_instructions(self):
        with tempfile.TemporaryDirectory(prefix='wb-install-rejected-',dir='/tmp') as directory:
            root=Path(directory);viewer=root/'broken-viewer';viewer.write_text('#!/bin/sh\nexit 42\n');viewer.chmod(0o755)
            state=root/'state';config=root/'config';data=root/'data'
            environment=dict(os.environ,XDG_DATA_HOME=str(data),BAND_STATE_DIR=str(state),BAND_CONFIG_DIR=str(config),QT_QPA_PLATFORM='offscreen')
            result=subprocess.run([sys.executable,str(ROOT/'scripts/stage-toolkit.py'),'--install','--parent',str(root/'stages'),'--viewer',str(viewer)],
                cwd=ROOT,env=environment,capture_output=True,text=True,timeout=60)
            self.assertNotEqual(result.returncode,0);self.assertIn('installed verification failed',result.stderr)
            self.assertNotIn('Close preview windows, then activate',result.stdout)
            copies=list((data/'terminal-workbench/previews').glob('preview-*'));self.assertEqual(len(copies),1)
            verification=json.loads((copies[0]/'installation-check.json').read_text());self.assertFalse(verification['ok'])
            self.assertEqual(len(list((root/'stages').glob('qt-preview-*/preview.json'))),1)
            self.assertFalse(state.exists());self.assertFalse(config.exists())

    @unittest.skipUnless(sys.platform.startswith('linux'),'real Linux build/install; native Mac bundle review remains separate')
    def test_real_just_recipe_installs_into_private_root_without_source_argument(self):
        with tempfile.TemporaryDirectory(prefix='wb-build-install-',dir='/tmp') as directory:
            root=Path(directory);state=root/'state';preferences=root/'config'
            environment=dict(os.environ,XDG_DATA_HOME=str(root/'data'),BAND_STATE_DIR=str(state),
                BAND_CONFIG_DIR=str(preferences),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
            result=subprocess.run(['just','--justfile',str(ROOT/'justfile'),'--working-directory',str(ROOT),'toolkit-install'],
                cwd=ROOT,env=environment,capture_output=True,text=True,timeout=180)
            self.assertEqual(result.returncode,0,(result.stdout,result.stderr))
            installs=list((root/'data/terminal-workbench/previews').glob('preview-*'))
            self.assertEqual(len(installs),1)
            installation=json.loads((installs[0]/'installation.json').read_text())
            source=Path(installation['source']);self.assertTrue(source.is_dir(),'source stage was not retained')
            launcher=installs[0]/installation['payload']/'terminal-workbench'
            self.assertIn(str(launcher),result.stdout)
            self.assertIn('Installed preview verified',result.stdout)
            verification=json.loads((installs[0]/'installation-check.json').read_text())
            self.assertTrue(verification['ok'],verification)
            self.assertTrue(verification['report']['ok'])
            self.assertEqual(verification['report']['payload'],str(launcher.parent))
            self.assertFalse(state.exists());self.assertFalse(preferences.exists())
            listed=subprocess.run([sys.executable,str(ROOT/'scripts/list-toolkit-previews.py'),'--json'],env=environment,capture_output=True,text=True,check=True,timeout=10)
            self.assertEqual(json.loads(listed.stdout)['installations'][0]['last_check']['result'],'passed')
            managed=subprocess.run([sys.executable,str(ROOT/'scripts/manage-toolkit-preview.py'),'check',installs[0].name],env=environment,capture_output=True,text=True,timeout=45)
            self.assertEqual(managed.returncode,0,(managed.stdout,managed.stderr))
            self.assertFalse(state.exists());self.assertFalse(preferences.exists())
            checked=subprocess.run([str(launcher),'--check'],env=environment,capture_output=True,text=True,timeout=40)
            self.assertEqual(checked.returncode,0,(checked.stdout,checked.stderr))
            self.assertTrue(json.loads(checked.stdout)['ok'])
            self.assertFalse(state.exists());self.assertFalse(preferences.exists())
            # A copy failure keeps the successfully built stage for retry.
            failed=root/'failed-stage'
            result=subprocess.run([sys.executable,str(ROOT/'scripts/stage-toolkit.py'),'--install','--parent',str(failed)],
                cwd=ROOT,env=dict(environment,XDG_DATA_HOME='relative'),capture_output=True,text=True,timeout=40)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('but installation failed',result.stderr)
            self.assertEqual(len(list(failed.glob('qt-preview-*/preview.json'))),1)
            self.assertEqual(list((root/'data/terminal-workbench/previews').glob('preview-*')),installs)
            self.assertFalse(state.exists());self.assertFalse(preferences.exists())


if __name__=='__main__':unittest.main()
