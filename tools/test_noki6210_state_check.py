import unittest

from tools.noki6210_state_check import verify


class StateAcceptanceTest(unittest.TestCase):
    def fixture(self):
        return '\n'.join((
            '6210_state: scenario=idle event=saved pc=1c sp=17583c ram=127bd2b1 t=19.000000',
            'state_replay: phase=reference event=begin t=19.000000',
            'TX packet type=70 payload=01 t=19.100000',
            'state_replay: phase=reference event=end t=20.000000',
            '6210_state: scenario=idle event=restored pc=1c sp=17583c ram=127bd2b1 t=19.000000',
            'state_roundtrip: result=pass requested_at=19.000000 t=19.000000',
            'state_replay: phase=restored event=begin t=19.000000',
            'TX packet type=70 payload=01 t=19.100000',
            'state_replay: phase=restored event=end t=20.000000'))

    def test_exact_restore(self):
        verify(self.fixture(), 'idle')

    def test_wrong_scenario(self):
        with self.assertRaises(ValueError):
            verify(self.fixture(), 'call')

    def test_corrupt_snapshot(self):
        for field in ('pc=1c', 'sp=17583c', 'ram=127bd2b1', 't=19.000000'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                verify(self.fixture().replace(field, field + '1', 1), 'idle')

    def test_missing_snapshot(self):
        with self.assertRaises(ValueError):
            verify('\n'.join(self.fixture().splitlines()[1:]), 'idle')

    def test_protocol_divergence(self):
        with self.assertRaises(ValueError):
            verify(self.fixture().replace('payload=01', 'payload=02', 1), 'idle')

    def test_empty_replay_is_not_evidence(self):
        with self.assertRaises(ValueError):
            verify(self.fixture().replace('TX packet', 'unobserved'), 'idle')

    def test_fixture_failure(self):
        for error in ('[LUA ERROR]', '6210_state: FAIL incomplete'):
            with self.assertRaises(ValueError):
                verify(self.fixture() + '\n' + error, 'idle')


if __name__ == '__main__':
    unittest.main()
