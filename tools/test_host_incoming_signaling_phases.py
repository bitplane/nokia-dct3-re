import unittest

from tools.run_host_incoming_signaling_gate import verify_phases


class IncomingPhasesTest(unittest.TestCase):
    def test_signaling_and_explicit_bearer_closure(self):
        prefix = ['queued', 'paging', 'alerting', 'connected']
        verify_phases(prefix + ['ended'])
        verify_phases(prefix + ['media_closed', 'ended'])

    def test_missing_reversed_duplicate_or_extra_states_rejected(self):
        for phases in (['queued', 'paging', 'alerting', 'ended'],
                       ['queued', 'paging', 'alerting', 'media_closed', 'connected', 'ended'],
                       ['queued', 'paging', 'alerting', 'connected', 'media_closed', 'media_closed', 'ended'],
                       ['queued', 'paging', 'alerting', 'connected', 'unexpected', 'ended']):
            with self.subTest(phases=phases), self.assertRaises(RuntimeError):
                verify_phases(phases)


if __name__ == '__main__':
    unittest.main()
