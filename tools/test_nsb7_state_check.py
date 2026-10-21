"""Executable acceptance checks for exact NSB-7 idle replay."""
import unittest

from tools.nsb7_state_check import check


def fixture():
    cpu = ','.join(['00000000'] * 37)
    state = f'pc=0000001c sp=00130df8 ram=87f4614d cpu={cpu} t=27.000000000'
    return '\n'.join([
        'nsb7_state: event=saved ' + state,
        'state_replay: phase=reference event=begin t=27.000000000',
        'RX enqueue type=80 payload=34 data=6000 t=27.1',
        'state_replay: phase=reference event=end t=30.000000000',
        'nsb7_state: event=restored ' + state,
        'state_roundtrip: result=pass scenario=nsb7_idle requested_at=27.000000000 t=27.000000000',
        'state_replay: phase=restored event=begin t=27.000000000',
        'RX enqueue type=80 payload=34 data=6000 t=27.1',
        'state_replay: phase=restored event=end t=30.000000000',
        'nsb7_state_physical: key=Menu state=1',
        'nsb7_key_decode: key=19 observer_writes=0',
        'nsb7_state_physical: key=Menu state=0',
    ])


class StateCheckTest(unittest.TestCase):
    def test_complete(self):
        check(fixture())

    def test_architecture_mismatch(self):
        text = fixture().replace('event=restored pc=0000001c', 'event=restored pc=00000020')
        with self.assertRaises(ValueError):
            check(text)

    def test_incomplete_cpu(self):
        with self.assertRaises(ValueError):
            check(fixture().replace(',00000000', '', 1))

    def test_packet_replay_mismatch(self):
        with self.assertRaises(ValueError):
            check(fixture().replace('data=6000', 'data=6001', 1))

    def test_missing_post_load_decode(self):
        with self.assertRaises(ValueError):
            check(fixture().replace('key=19 observer_writes=0', 'key=18 observer_writes=0'))

    def test_incomplete_fixture(self):
        with self.assertRaises(ValueError):
            check(fixture() + '\nnsb7_state: FAIL incomplete')


if __name__ == '__main__':
    unittest.main()
