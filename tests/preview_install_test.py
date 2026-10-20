"""Installation never replaces a previous payload or starts user sessions."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('preview_install',ROOT/'scripts/install-toolkit-preview.py')
installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)


def files(root):
    return {str(path.relative_to(root)):('link',os.readlink(path)) if path.is_symlink() else ('file',path.read_bytes())
        for path in root.rglob('*') if path.is_file() or path.is_symlink()}


class PreviewInstallTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-install-',dir='/tmp')
        self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)

    def fixture(self,platform):
        source=self.root/platform
        app=source/installer.MAC_APP if platform=='darwin' else source
        payload=app/'Contents/Resources/workbench' if platform=='darwin' else app
        payload.mkdir(parents=True)
        (payload/'preview.json').write_text(json.dumps(dict(format=1,source='a'*40,source_dirty=True,platform=platform)))
        for name in ('terminal-workbench','libexec/workbench-session','scripts/toolkit-ui.py'):
            path=payload/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('fixture executable');path.chmod(0o755)
        viewer=app/'Contents/MacOS/workbench-ui-study' if platform=='darwin' else payload/'libexec/workbench-ui-study'
        viewer.parent.mkdir(parents=True,exist_ok=True);viewer.write_text('fixture viewer');viewer.chmod(0o755)
        if platform=='darwin':
            bootstrap=app/'Contents/MacOS/workbench-preview';bootstrap.write_text('fixture bootstrap');bootstrap.chmod(0o755)
            (app/'Contents/Info.plist').write_text('fixture plist')
            (payload/'python.path').write_text(sys.executable+'\n')
        (payload/'alias').symlink_to('libexec',target_is_directory=True)
        return source,app,payload

    def test_same_build_installs_twice_without_changing_source_or_first_copy(self):
        source,_,_=self.fixture('linux');before=files(source)
        first,launcher=installer.install(source,self.root/'installed',platform='linux')
        installed=files(first)
        second,_=installer.install(source,self.root/'installed',platform='linux')
        self.assertNotEqual(first,second);self.assertTrue(launcher.is_file())
        self.assertEqual(before,files(source));self.assertEqual(installed,files(first))
        self.assertTrue((first/'payload/alias').is_symlink())
        self.assertFalse(list((self.root/'installed').glob('.install-*')))

    def test_mac_bundle_seal_files_and_links_are_unchanged(self):
        source,app,payload=self.fixture('darwin')
        seal=app/'Contents/_CodeSignature/CodeResources';seal.parent.mkdir();seal.write_bytes(b'fixture signature')
        before=files(app)
        installed,launcher=installer.install(source,self.root/'mac installs',platform='darwin')
        self.assertEqual(before,files(installed/installer.MAC_APP))
        self.assertEqual(launcher,installed/installer.MAC_APP/'Contents/Resources/workbench/terminal-workbench')
        self.assertTrue((installed/'installation.json').is_file())
        self.assertFalse((installed/installer.MAC_APP/'installation.json').exists())
        another,_=installer.install(app,self.root/'mac installs',platform='darwin')
        self.assertEqual(before,files(another/installer.MAC_APP))

    def test_partial_copy_failure_removes_only_temporary_install(self):
        from unittest.mock import patch
        source,_,_=self.fixture('linux')
        installed,_=installer.install(source,self.root/'installed',platform='linux');before=files(installed)
        def partial(source,destination,**kwargs):
            destination.mkdir();(destination/'partial').write_text('partial copy')
            raise OSError('copy fixture failed')
        with patch.object(installer.shutil,'copytree',side_effect=partial):
            with self.assertRaises(OSError):installer.install(source,self.root/'installed',platform='linux')
        self.assertEqual(before,files(installed))
        self.assertEqual(list((self.root/'installed').iterdir()),[installed])

    def test_platform_and_missing_executable_fail_before_creating_install_root(self):
        source,_,payload=self.fixture('linux');target=self.root/'must not create'
        metadata=payload/'preview.json';original=metadata.read_text()
        value=json.loads(original);value['platform']='darwin';metadata.write_text(json.dumps(value))
        with self.assertRaisesRegex(ValueError,'platform'):installer.install(source,target,platform='linux')
        metadata.write_text(original)
        (payload/'libexec/workbench-ui-study').unlink()
        with self.assertRaisesRegex(ValueError,'executable'):installer.install(source,target,platform='linux')
        self.assertFalse(target.exists())

    def test_recursive_destination_is_rejected(self):
        source,_,_=self.fixture('linux')
        with self.assertRaisesRegex(ValueError,'outside'):installer.install(source,source/'installs',platform='linux')
        self.assertFalse((source/'installs').exists())

    @unittest.skipUnless(sys.platform.startswith('linux'),'native CLI fixture uses Linux layout')
    def test_cli_prints_usable_commands_without_running_payload(self):
        import shlex
        source,_,payload=self.fixture('linux');marker=self.root/'must not launch'
        (payload/'terminal-workbench').write_text('#!/bin/sh\ntouch '+shlex.quote(str(marker))+'\n')
        target=self.root/'installed with spaces'
        result=subprocess.run([sys.executable,str(ROOT/'scripts/install-toolkit-preview.py'),str(source),'--root',str(target)],
            capture_output=True,text=True,check=True,timeout=10)
        check_line=next(line.strip() for line in result.stdout.splitlines() if line.strip().endswith(' --check'))
        command=shlex.split(check_line)
        self.assertEqual(len(command),2);self.assertTrue(Path(command[0]).is_file())
        self.assertTrue(Path(command[0]).is_relative_to(target))
        self.assertFalse(marker.exists(),'installer launched the payload')

    def test_default_roots_respect_terminal_configuration_conventions(self):
        self.assertEqual(installer.installation_root({'HOME':'/Users/test'},'darwin'),Path('/Users/test/Applications/Terminal Workbench Previews'))
        self.assertEqual(installer.installation_root({'HOME':'/home/test','XDG_DATA_HOME':'/data'},'linux'),Path('/data/terminal-workbench/previews'))
        with self.assertRaises(ValueError):installer.installation_root({'XDG_DATA_HOME':'relative'},'linux')

    @unittest.skipUnless(sys.platform.startswith('linux'),'real Linux installed launch; native Mac review remains separate')
    def test_real_copied_installed_launcher_has_no_checkout_or_state_dependency(self):
        sys.path.insert(0,str(ROOT/'scripts'))
        stage_spec=importlib.util.spec_from_file_location('stage',ROOT/'scripts/stage-toolkit.py')
        staging=importlib.util.module_from_spec(stage_spec);stage_spec.loader.exec_module(staging)
        source=staging.stage(self.root/'build',ROOT/'target/debug/workbench-session',ROOT/'.local/build/qt-input/workbench-ui-study')
        installed,launcher=installer.install(source,self.root/'installed',platform='linux')
        shutil.rmtree(source)
        state=self.root/'state';home=self.root/'home';home.mkdir()
        environment={key:value for key,value in os.environ.items() if not key.startswith(('WORKBENCH_','XDG_'))}
        environment.update(HOME=str(home),WORKBENCH_STATE_DIR=str(state),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',PYTHONDONTWRITEBYTECODE='1')
        def run(action):
            result=subprocess.run([str(launcher),action],env=environment,cwd=self.root,capture_output=True,text=True,timeout=40)
            self.assertEqual(result.returncode,0,(result.stdout,result.stderr));return json.loads(result.stdout)
        before=files(installed)
        paths=run('--paths')
        self.assertTrue(Path(paths['runtime']).is_relative_to(installed))
        self.assertTrue(Path(paths['viewer']).is_relative_to(installed))
        self.assertTrue(run('--check')['ok'])
        self.assertFalse(state.exists());self.assertFalse((home/'.config').exists())
        self.assertEqual(before,files(installed),'installed diagnostic changed payload files')


if __name__=='__main__':unittest.main()
