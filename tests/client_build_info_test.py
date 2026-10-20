"""Client identity uses preview metadata only; development/missing cases stay explicit."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from client_build_info import capture


class BuildInfoTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-build-info-');self.addCleanup(self.directory.cleanup)
        self.root=Path(self.directory.name)

    def test_checkout_without_metadata_is_explicit_and_read_only(self):
        self.assertEqual(capture(self.root)['kind'],'development checkout')
        self.assertEqual(list(self.root.iterdir()),[])

    def test_preview_fields_are_allowlisted(self):
        metadata=dict(format=1,source='a'*40,source_dirty=True,qt_bundled=True,selective_qml=True,catalog='private workspaces',terminal='secret output')
        path=self.root/'preview.json';path.write_text(json.dumps(metadata));before=path.read_bytes()
        value=capture(self.root,installed=True)
        self.assertEqual(value['revision'],'a'*40);self.assertTrue(value['source_dirty'])
        self.assertNotIn('private workspaces',json.dumps(value));self.assertNotIn('secret output',json.dumps(value))
        self.assertEqual(path.read_bytes(),before)

    def test_missing_and_malformed_installed_metadata_are_not_guessed(self):
        self.assertIn('metadata_error',capture(self.root,installed=True))
        for value in ('[]','{',json.dumps(dict(format=1,source='not a revision'))):
            (self.root/'preview.json').write_text(value)
            info=capture(self.root,installed=True)
            self.assertIn('metadata_error',info);self.assertNotIn('revision',info)


if __name__=='__main__':unittest.main()
