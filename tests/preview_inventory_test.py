"""Installed preview inventory is read-only and keeps unknown origins explicit."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
spec=importlib.util.spec_from_file_location('inventory',ROOT/'scripts/list-toolkit-previews.py')
inventory=importlib.util.module_from_spec(spec);spec.loader.exec_module(inventory)
from session_service import exchange,PROTOCOL,Unavailable


def files(root):
    return {str(path):path.read_bytes() for path in root.rglob('*') if path.is_file() and not path.is_symlink()}


class PreviewInventoryTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-list-',dir='/tmp');self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name);self.installs=self.root/'installs';self.state=self.root/'state'

    def install(self,name,revision):
        directory=self.installs/('preview-'+name);payload=directory/'payload';payload.mkdir(parents=True)
        (directory/'installation.json').write_text(json.dumps(dict(schema=1,revision=revision*40,payload='payload',source_dirty=False)))
        (payload/'preview.json').write_text(json.dumps(dict(format=1,source=revision*40,platform=sys.platform,qt_bundled=False)))
        return directory,payload

    def origin(self,payload):
        return dict(version=1,runtime_binary=str(payload/'libexec/workbench-session'),agent_bin=str(payload/'.local/bin'))

    def inspect(self,health):
        with patch.object(inventory,'inspect',return_value=health):return inventory.inventory(self.installs,self.state)

    def test_absent_roots_and_namespace_are_not_created(self):
        value=inventory.inventory(self.installs,self.state)
        self.assertEqual(value['installations'],[]);self.assertFalse(value['service_available'])
        self.assertFalse(self.installs.exists());self.assertFalse(self.state.exists())

    def test_service_retained_new_stopped_unknown_and_foreign_references(self):
        _,old=self.install('old','a');_,new=self.install('new','b');manual=self.root/'manual/payload'
        terminals=[dict(id='retained',state='running',runtime=dict(origin=self.origin(old))),
            dict(id='fresh',state='running',runtime=dict(origin=self.origin(new))),
            dict(id='stopped',state='stopped',runtime=dict(origin=self.origin(old))),
            dict(id='legacy',state='running',runtime=None),
            dict(id='foreign',state='running',runtime=dict(origin=self.origin(manual)))]
        before=files(self.root)
        value=self.inspect(dict(runtime_binary=str(new/'libexec/workbench-session'),terminals=terminals))
        rows={row['payload']:row for row in value['installations']}
        self.assertFalse(rows[str(old)]['service']);self.assertTrue(rows[str(new)]['service'])
        self.assertEqual(rows[str(old)]['runtime_terminals'],['retained'])
        self.assertEqual(rows[str(old)]['helper_terminals'],['retained'])
        self.assertEqual(rows[str(new)]['runtime_terminals'],['fresh'])
        self.assertEqual([row['id'] for row in value['unknown_terminals']],['legacy'])
        self.assertEqual(value['references_outside_root'][0]['directory'],str(manual))
        self.assertEqual(before,files(self.root))

    def test_runtime_and_helper_origins_can_reference_different_installs(self):
        _,old=self.install('old','a');_,new=self.install('new','b')
        origin=self.origin(old);origin['agent_bin']=str(new/'.local/bin')
        value=self.inspect(dict(terminals=[dict(id='split',state='running',runtime=dict(origin=origin))]))
        rows={row['payload']:row for row in value['installations']}
        self.assertEqual(rows[str(old)]['runtime_terminals'],['split']);self.assertEqual(rows[str(old)]['helper_terminals'],[])
        self.assertEqual(rows[str(new)]['runtime_terminals'],[]);self.assertEqual(rows[str(new)]['helper_terminals'],['split'])

    def test_unavailable_service_does_not_claim_zero_dependencies(self):
        self.install('one','a');value=self.inspect(None)
        self.assertFalse(value['installations'][0]['service_available'])
        output=io.StringIO()
        with contextlib.redirect_stdout(output):inventory.display(value)
        self.assertIn('references unknown',output.getvalue())

    def test_installer_record_from_group_writable_umask_remains_readable(self):
        directory,_=self.install('one','a');directory.chmod(0o700)
        (directory/'installation.json').chmod(0o664)
        value=self.inspect(None)
        self.assertEqual(len(value['installations']),1);self.assertEqual(value['invalid_installations'],[])

    def test_malformed_install_does_not_hide_valid_siblings(self):
        broken,_=self.install('bad','b');self.install('good','a')
        (broken/'installation.json').write_text('[]')
        value=self.inspect(None)
        self.assertEqual(len(value['installations']),1);self.assertEqual(len(value['invalid_installations']),1)

    def test_external_record_and_payload_symlinks_are_not_followed(self):
        directory,_=self.install('bad','a');external=self.root/'external';external.mkdir()
        marker=directory/'installation.json';contents=marker.read_text();marker.unlink()
        (external/'record').write_text(contents);marker.symlink_to(external/'record')
        self.assertEqual(len(self.inspect(None)['invalid_installations']),1)
        marker.unlink();marker.write_text(json.dumps(dict(schema=1,revision='a'*40,payload='escape')))
        (directory/'escape').symlink_to(external,target_is_directory=True)
        self.assertEqual(len(self.inspect(None)['invalid_installations']),1)

    def test_live_service_health_does_not_seed_catalog_or_launch_runtime(self):
        _,payload=self.install('one','a');runtime=payload/'libexec/workbench-session';runtime.parent.mkdir()
        marker=self.root/'runtime must not start'
        runtime.write_text('#!/bin/sh\ntouch '+str(marker)+'\n');runtime.chmod(0o755)
        self.state.mkdir(mode=0o700)
        child=subprocess.Popen([sys.executable,str(ROOT/'scripts/session_service.py'),'serve','--state-dir',str(self.state),
            '--runtime',str(runtime),'--cwd',str(self.root)],stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            deadline=time.monotonic()+5
            while True:
                try:started=exchange(self.state,dict(protocol=PROTOCOL,method='health'));break
                except Unavailable:
                    if child.poll() is not None or time.monotonic()>deadline:raise AssertionError('fixture service did not start')
                    time.sleep(.02)
            before=files(self.installs)
            value=inventory.inventory(self.installs,self.state)
            self.assertTrue(value['service_available']);self.assertTrue(value['installations'][0]['service'])
            self.assertEqual(value['installations'][0]['runtime_terminals'],[])
            self.assertEqual(exchange(self.state,dict(protocol=PROTOCOL,method='health'))['service_pid'],started['service_pid'])
            self.assertEqual(before,files(self.installs));self.assertFalse(marker.exists())
            self.assertFalse((self.state/'workspaces.json').exists())
        finally:
            try:exchange(self.state,dict(protocol=PROTOCOL,method='service.shutdown'))
            except Unavailable:child.terminate()
            child.wait(timeout=5)

    def test_cli_json_leaves_missing_state_uncreated(self):
        self.install('one','a')
        reply=subprocess.run([sys.executable,str(ROOT/'scripts/list-toolkit-previews.py'),'--root',str(self.installs),'--state-dir',str(self.state),'--json'],
            capture_output=True,text=True,check=True,timeout=10)
        self.assertEqual(len(json.loads(reply.stdout)['installations']),1)
        self.assertFalse(self.state.exists())


if __name__=='__main__':unittest.main(argv=[sys.argv[0]])
