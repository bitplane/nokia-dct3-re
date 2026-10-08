import unittest

from tools.noki6210_state_check import verify


class StateAcceptanceTest(unittest.TestCase):
    def fixture(self):
        return '\n'.join((
            'LAPDm Location Updating Accept acknowledged nr=1',
            'LAPDm Channel Release acknowledged nr=2',
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

    def test_divert_exact_restore(self):
        verify(self.fixture().replace('scenario=idle', 'scenario=divert'), 'divert')

    def test_wrong_scenario(self):
        with self.assertRaises(ValueError):
            verify(self.fixture(), 'call')

    def test_corrupt_snapshot(self):
        for field in ('pc=1c', 'sp=17583c', 'ram=127bd2b1', 't=19.000000'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                verify(self.fixture().replace(field, field + '1', 1), 'idle')

    def test_missing_snapshot(self):
        with self.assertRaises(ValueError):
            verify(self.fixture().replace(self.fixture().splitlines()[2], ''), 'idle')

    def test_registration_must_precede_save(self):
        for event in ('LAPDm Location Updating Accept acknowledged nr=1',
                      'LAPDm Channel Release acknowledged nr=2'):
            with self.subTest(event=event), self.assertRaises(ValueError):
                verify(self.fixture().replace(event, '') + '\n' + event, 'idle')

    def test_registration_order(self):
        with self.assertRaises(ValueError):
            verify(self.fixture().replace(
                'LAPDm Location Updating Accept acknowledged nr=1\n'
                'LAPDm Channel Release acknowledged nr=2',
                'LAPDm Channel Release acknowledged nr=2\n'
                'LAPDm Location Updating Accept acknowledged nr=1'), 'idle')

    def test_pin_must_precede_registration_and_save(self):
        pin = ('6210_security_physical: action=confirm\n'
               'SIM status ins=20 sw=9000\n')
        verify(pin + self.fixture(), 'idle', pin_enabled=True)
        for text in (self.fixture(), self.fixture() + '\n' + pin,
                     pin.replace('SIM status ins=20 sw=9000', '') + self.fixture()):
            with self.subTest(text=text), self.assertRaises(ValueError):
                verify(text, 'idle', pin_enabled=True)

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
