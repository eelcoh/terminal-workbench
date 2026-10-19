"""Observed preview references never become a deletion authorization."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'scripts'))
from preview_activation import payload_usage

SELECTED='/chosen/libexec/workbench-session'


def terminal(identity='retained',state='running',origin=None):
    return dict(id=identity,state=state,runtime=dict(origin=origin))


def health(*terminals):
    return dict(runtime_binary=SELECTED,terminals=list(terminals))


class UsageTest(unittest.TestCase):
    def test_retained_and_split_paths_without_filesystem_access(self):
        origin=dict(version=1,runtime_binary='/old build é/libexec/workbench-session',agent_bin='/other helpers/.local/bin')
        result=payload_usage(SELECTED,health(terminal(origin=origin)))
        rows={row['directory']:row for row in result['builds']}
        self.assertTrue(rows['/chosen']['selected'] and rows['/chosen']['service'])
        self.assertEqual(rows['/old build é']['runtime_terminals'],['retained'])
        self.assertEqual(rows['/other helpers']['helper_terminals'],['retained'])
        self.assertEqual(result['unknown_terminals'],[])
        self.assertIn('Not performed',result['removal_assessment'])
        self.assertIn('other state directories',result['scope'])

    def test_legacy_unavailable_and_incomplete_origins_are_explicit(self):
        for origin in (None,{},[],False,dict(version=True),dict(version=2),dict(version=1),dict(version=1,runtime_binary='relative/libexec/workbench-session',agent_bin='/custom/bin')):
            result=payload_usage(SELECTED,health(terminal(origin=origin)))
            self.assertEqual([row['id'] for row in result['unknown_terminals']],['retained'])
            self.assertEqual(len(result['builds']),1)
        current=dict(version=1,runtime_binary=SELECTED,agent_bin='/chosen/.local/bin')
        result=payload_usage(SELECTED,health(terminal(state='unavailable',origin=current)))
        self.assertEqual([row['id'] for row in result['unknown_terminals']],['retained'])
        self.assertFalse(result['builds'][0]['runtime_terminals'])
        malformed=terminal();malformed['runtime']=['unexpected']
        self.assertEqual(len(payload_usage(SELECTED,health(malformed))['unknown_terminals']),1)

    def test_stopped_origins_do_not_keep_observed_runtime_references(self):
        result=payload_usage(SELECTED,health(terminal(state='stopped',origin=dict(version=1,runtime_binary='/old/libexec/workbench-session',agent_bin='/old/.local/bin'))))
        self.assertEqual([row['directory'] for row in result['builds']],['/chosen'])
        self.assertEqual(result['unknown_terminals'],[])

    def test_absent_service_does_not_imply_absent_shells(self):
        result=payload_usage(SELECTED,None)
        self.assertFalse(result['service_available'])
        self.assertTrue(result['builds'][0]['selected'])
        self.assertFalse(result['builds'][0]['service'])
        self.assertEqual(result['removal_assessment'],'Not performed; no files are removed')


if __name__=='__main__':unittest.main()
