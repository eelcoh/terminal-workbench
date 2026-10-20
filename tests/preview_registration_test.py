"""App-menu following is bounded to an existing managed default-session entry."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from installed_paths import session_directory
from platform_launchers import linux


class RegistrationTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-registration-');self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name);self.env=dict(HOME=str(self.root/'home'),XDG_DATA_HOME=str(self.root/'data'))
        self.state=session_directory(self.env)
        self.old=self.root/'old/terminal-workbench';self.new=self.root/'space ü/new/terminal-workbench'
        self.new.parent.mkdir(parents=True);(self.new.parent/'preview.json').write_text(json.dumps(dict(format=1,container='build machine')))
        self.path=linux.desktop_path(self.env)

    def follow(self,**environment):return linux.follow_activation(self.new,self.state,dict(self.env,**environment))

    def test_existing_entry_follows_selected_payload_and_container_without_rewriting_current(self):
        linux.install(self.old,self.env,'old container')
        value=self.follow();self.assertEqual(value['result'],'updated')
        self.assertIn('build machine',self.path.read_text());self.assertNotIn('old container',self.path.read_text())
        self.assertIn(str(self.new),self.path.read_text());self.assertNotIn(str(self.old),self.path.read_text())
        stamp=self.path.stat().st_mtime_ns;self.assertEqual(self.follow()['result'],'already-current')
        self.assertEqual(self.path.stat().st_mtime_ns,stamp)

    def test_unregistered_session_creates_no_menu_or_state_directories(self):
        self.assertEqual(self.follow(),dict(result='skipped',reason='not-registered'))
        self.assertFalse(self.path.parent.exists());self.assertFalse(self.state.exists())

    def test_custom_namespace_cannot_retarget_global_entry(self):
        linux.install(self.old,self.env);before=self.path.read_bytes()
        value=linux.follow_activation(self.new,self.root/'custom-session',dict(self.env,WORKBENCH_STATE_DIR=str(self.root/'custom-session')))
        self.assertEqual(value,dict(result='skipped',reason='custom-session'));self.assertEqual(self.path.read_bytes(),before)
        # Explicitly naming the standard namespace still follows normally.
        self.assertEqual(self.follow(WORKBENCH_STATE_DIR=str(self.state))['result'],'updated')

    def test_unmanaged_entry_symlink_and_directory_stay_untouched(self):
        self.path.parent.mkdir(parents=True);self.path.write_text('[Desktop Entry]\nName=My custom launcher\n')
        before=self.path.read_bytes();self.assertEqual(self.follow()['reason'],'unmanaged-entry');self.assertEqual(self.path.read_bytes(),before)
        self.path.unlink();target=self.root/'target';target.write_text(linux.MARKER);self.path.symlink_to(target)
        self.assertEqual(self.follow()['reason'],'unmanaged-entry');self.assertEqual(target.read_text(),linux.MARKER)
        self.path.unlink();self.path.mkdir();self.assertEqual(self.follow()['reason'],'unmanaged-entry')

    def test_write_or_metadata_failure_is_a_warning_with_original_entry_retained(self):
        linux.install(self.old,self.env);before=self.path.read_bytes()
        with patch.object(linux,'install',side_effect=OSError('read-only app-menu directory')):
            result=self.follow()
        self.assertEqual(result['result'],'warning');self.assertIn('read-only',result['error']);self.assertEqual(self.path.read_bytes(),before)
        (self.new.parent/'preview.json').write_text('[]')
        self.assertEqual(self.follow()['result'],'warning');self.assertEqual(self.path.read_bytes(),before)


if __name__=='__main__':unittest.main()
