"""Managed preview selection refuses ambiguity and runs only the requested action."""
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
sys.path.insert(0,str(ROOT/'scripts'))
spec=importlib.util.spec_from_file_location('preview_commands',ROOT/'scripts/manage-toolkit-preview.py')
commands=importlib.util.module_from_spec(spec);spec.loader.exec_module(commands)


class CommandTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-preview-command-');self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name);self.installs=self.root/'with spaces'
        self.marker=self.root/'action.json'

    def fixture(self,name,platform=None):
        platform=sys.platform if platform is None else platform
        directory=self.installs/name
        payload=directory/('Terminal Workbench Preview.app/Contents/Resources/workbench' if platform=='darwin' else 'payload')
        payload.mkdir(parents=True)
        (directory/'installation.json').write_text(json.dumps(dict(schema=1,revision='a'*40,payload=str(payload.relative_to(directory)))))
        (payload/'preview.json').write_text(json.dumps(dict(format=1,source='a'*40,platform=platform)))
        launcher=payload/'terminal-workbench'
        launcher.write_text('#!'+sys.executable+'\nimport json,sys\nfrom pathlib import Path\npayload=Path(__file__).parent\n'
            +"assert sys.argv[1:] in (['--check'],['--activate'],['--desktop'])\nPath("+repr(str(self.marker))+").write_text(json.dumps(sys.argv[1:]))\n"
            +"print(json.dumps(dict(schema=1,ok=True,payload=str(payload),checks=[dict(name='qml_startup',ok=True)])))\n")
        launcher.chmod(0o755)
        return directory,payload

    def test_unique_prefix_and_exact_id_without_contacting_a_service(self):
        directory,payload=self.fixture('preview-aaaaaaaaaaaa-one')
        row=commands.select(self.installs,'aaaaaaaaaaaa-o')
        self.assertEqual(row['payload'],str(payload));self.assertFalse(self.marker.exists())
        self.fixture('preview-aaaaaaaaaaaa-two')
        with self.assertRaisesRegex(ValueError,'Ambiguous'):commands.select(self.installs,'aaaaaaaaaaaa')
        self.assertEqual(commands.select(self.installs,directory.name)['directory'],str(directory))
        for identity in ('../escape','unknown','a/b'):
            with self.assertRaises(ValueError):commands.select(self.installs,identity)
        self.assertFalse(self.marker.exists())

    def test_invalid_and_linked_installations_cannot_be_executed(self):
        directory,payload=self.fixture('preview-aaaaaaaaaaaa-one')
        (self.installs/'preview-linked').symlink_to(directory,target_is_directory=True)
        with self.assertRaises(ValueError):commands.select(self.installs,'linked')
        with self.assertRaisesRegex(ValueError,'platform'):commands.select(self.installs,directory.name,platform='not-native')
        (directory/'installation.json').write_text(json.dumps(dict(schema=1,revision='a'*40,payload='../outside')))
        with self.assertRaises(ValueError):commands.select(self.installs,directory.name)
        self.assertFalse(self.marker.exists())

    def test_cli_check_writes_receipt_outside_payload_and_activate_has_no_other_action(self):
        directory,payload=self.fixture('preview-aaaaaaaaaaaa-one')
        before={p.name:p.read_bytes() for p in payload.iterdir()}
        result=subprocess.run([sys.executable,str(ROOT/'scripts/manage-toolkit-preview.py'),'check',directory.name,'--root',str(self.installs)],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,(result.stdout,result.stderr));self.assertEqual(json.loads(self.marker.read_text()),['--check'])
        self.assertTrue(json.loads((directory/'installation-check.json').read_text())['ok'])
        self.assertEqual({p.name:p.read_bytes() for p in payload.iterdir()},before)
        self.assertEqual(commands.execute('activate',commands.select(self.installs,directory.name)),0)
        self.assertEqual(json.loads(self.marker.read_text()),['--activate'])

    def test_open_dispatches_to_selected_linux_or_mac_payload_without_activation(self):
        for platform in ('linux','darwin'):
            directory,payload=self.fixture('preview-'+platform,platform)
            row=commands.select(self.installs,directory.name,platform=platform)
            with patch.object(commands.subprocess,'run',return_value=subprocess.CompletedProcess([],0)) as run:
                self.assertEqual(commands.execute('open',row,platform=platform),0)
            expected=['/usr/bin/open','-n',str(payload.parents[2])] if platform=='darwin' else [str(payload/'terminal-workbench'),'--desktop']
            self.assertEqual(run.call_args.args[0],expected);self.assertFalse(self.marker.exists())


if __name__=='__main__':unittest.main()
