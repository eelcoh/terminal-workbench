"""Dependency evidence keeps static links, observed mappings and unknowns distinct."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
from preview_dependencies import summarize


class DependencyTest(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='wb-dependencies-');self.addCleanup(self.directory.cleanup)
        self.bundle=Path(self.directory.name)/'Moved Preview.app'
        self.payload=self.bundle/'Contents/Resources/workbench';self.payload.mkdir(parents=True)

    def test_mac_reports_external_paths_and_keeps_build_links_separate(self):
        audit=self.payload/'qt-dependencies.json'
        audit.write_text(json.dumps(dict(schema=1,external_non_qt=['@rpath/liboptional.dylib'],secret='do not report')))
        before=audit.read_bytes()
        value=summarize(self.payload,dict(qt_bundled=True,python=dict(executable='/build/python',version='3.13.1',secret='hidden')),
            dict(loaded_images=[str(self.bundle/'Contents/MacOS/workbench-ui-study'),
                str(self.bundle/'Contents/Frameworks/QtCore.framework/Versions/A/QtCore'),
                '/usr/lib/libSystem.B.dylib','/System/Library/Frameworks/AppKit.framework/AppKit',
                '/opt/homebrew/lib/libicuuc.dylib','/opt/homebrew/lib/libicuuc.dylib']),platform='darwin')
        mappings=value['viewer_mappings']
        self.assertTrue(mappings['available']);self.assertEqual(mappings['bundled_path_count'],2)
        self.assertEqual(mappings['system_path_count'],2)
        self.assertEqual(mappings['outside_preview_paths'],['/opt/homebrew/lib/libicuuc.dylib'])
        self.assertEqual(value['build_audit']['external_non_qt_links'],['@rpath/liboptional.dylib'])
        self.assertFalse(value['python']['bundled']);self.assertEqual(value['python']['executable'],sys.executable)
        self.assertEqual(value['build_python'],dict(available=True,executable='/build/python',version='3.13.1'))
        self.assertNotIn('do not report',json.dumps(value));self.assertNotIn('hidden',json.dumps(value))
        self.assertEqual(audit.read_bytes(),before)

    def test_symlink_escaping_bundle_is_external_and_prefix_lookalikes_are_not_system(self):
        outside=Path(self.directory.name)/'outside.dylib';outside.touch()
        alias=self.payload/'looks-bundled.dylib';alias.symlink_to(outside)
        mappings=summarize(self.payload,{},dict(loaded_images=[str(alias),'/usr/library/lookalike.dylib','@rpath/unresolved.dylib']),platform='darwin')['viewer_mappings']
        self.assertEqual(set(mappings['outside_preview_paths']),{str(outside),'/usr/library/lookalike.dylib'})
        self.assertEqual(mappings['unclassified_paths'],['@rpath/unresolved.dylib'])
        self.assertEqual(mappings['bundled_path_count'],0)

    def test_missing_or_invalid_evidence_is_unknown_not_an_empty_dependency_set(self):
        for report in (None,{},dict(loaded_images=[]),dict(loaded_images=['/usr/lib/libSystem.B.dylib',17])):
            value=summarize(self.payload,{},report,platform='darwin')
            self.assertFalse(value['viewer_mappings']['available'])
            self.assertFalse(value['build_audit']['available']);self.assertFalse(value['build_python']['available'])
        audit=self.payload/'qt-dependencies.json'
        for text in ('{','[]',json.dumps(dict(schema=1,external_non_qt='invalid'))):
            audit.write_text(text)
            value=summarize(self.payload,{},None,platform='darwin')
            self.assertIn('error',value['build_audit'])
        audit.unlink();outside=Path(self.directory.name)/'other.json';outside.write_text('{}');audit.symlink_to(outside)
        self.assertIn('symlink',summarize(self.payload,{},None,platform='darwin')['build_audit']['error'])

    def test_linux_describes_mapping_scope_and_does_not_create_files(self):
        before=set(self.payload.iterdir())
        value=summarize(self.payload,{},dict(loaded_images=[str(self.payload/'libexec/viewer'),'/usr/lib64/libc.so.6','/opt/toolkit/libQt6Core.so.6','/data/fonts/custom.ttf']),platform='linux')
        self.assertIn('non-library',value['viewer_mappings']['source'])
        self.assertEqual(value['viewer_mappings']['outside_preview_paths'],['/data/fonts/custom.ttf','/opt/toolkit/libQt6Core.so.6'])
        self.assertEqual(set(self.payload.iterdir()),before)


if __name__=='__main__':unittest.main()
