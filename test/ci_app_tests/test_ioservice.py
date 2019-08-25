# IoService tests

import unittest

import calipertest as calitest

class CaliperIoServiceTest(unittest.TestCase):
    """ Caliper I/O service test case """

    def test_memusage(self):
        target_cmd = [ './ci_test_io' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,io,trace,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, { 'io.bytes.read': '16',
                         'io.region':     'read',
                         'function':      'main' }))

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'io.bytes.read',
                         'io.region',
                         'io.filesystem',
                         'io.mount.point' }))

if __name__ == "__main__":
    unittest.main()
