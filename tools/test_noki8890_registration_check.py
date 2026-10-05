import unittest
from tools.noki8890_registration_check import verify

PCS_LOG = '\n'.join((
    'TX packet type=56 payload=160 data=003c',
    'TX packet type=55 payload=4 data=01140000',
    'TX packet type=55 payload=4 data=04080000',
    'RX enqueue type=8b data=0010025800c3025900b9',
    'TX packet type=56 payload=160 data=02580259',
    'TX packet type=02 payload=20 radio_phase=candidate_channel_change data=041202000000005050000258',
    'RX enqueue type=80 payload=34 data=501200000990025800005906198f2c00000000000000000000000000000000006b00',
    'TX packet type=0c radio_phase=random_access',
    'RX enqueue type=89 payload=8 data=0100000000000000',
    'TX packet type=1b data=0080013f4905087000f000fffe20080910101032547698',
    'RX enqueue type=80 payload=34 data=8000000000000000000001734905087000f000fffe20080910101032547698',
    'LAPDm Location Updating Accept acknowledged nr=1',
    'TX packet type=1b data=0080032101',
    'LAPDm Channel Release acknowledged nr=2',
    'TX packet type=1b data=0080034101',
    'sim_device: update-binary fid=6f7e offset=4 length=5',
    'sim_device: update-binary fid=6f7e offset=10 length=1',
    'TX packet type=02 payload=20 radio_phase=release_channel_change data=041202000000001a600002580000000f00000000',
    'RX enqueue type=89 payload=8 data=0000000000000000',
    'RX enqueue type=80 payload=34 data=600000000000000000001506210001f0',
))


class RegistrationTest(unittest.TestCase):
    def test_missing_candidate(self):
        with self.assertRaisesRegex(ValueError, 'candidate window'):
            verify('')

    def test_candidate_alone_insufficient(self):
        with self.assertRaisesRegex(ValueError, 'candidate channel'):
            verify('TX packet type=56 payload=160 data=003c')

    def test_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')

    def test_complete_pcs_lifecycle(self):
        verify(PCS_LOG, pcs1900=True)

    def test_pcs_requires_real_candidate_selection(self):
        with self.assertRaisesRegex(ValueError, 'firmware PCS candidate window'):
            verify(PCS_LOG.replace('data=02580259', 'data=003c0006'), pcs1900=True)

    def test_pcs_requires_band_indicator(self):
        with self.assertRaisesRegex(ValueError, 'PCS SI1 band indicator'):
            verify(PCS_LOG.replace('6b00', '2b00'), pcs1900=True)

    def test_pcs_requires_scan_order(self):
        lines = PCS_LOG.splitlines()
        lines[1], lines[2] = lines[2], lines[1]
        with self.assertRaisesRegex(ValueError, 'PCS scan request'):
            verify('\n'.join(lines), pcs1900=True)

    def test_pcs_requires_own_power_class(self):
        with self.assertRaisesRegex(ValueError, 'Location Updating Request'):
            verify(PCS_LOG.replace('fffe200809', 'fffe230809'), pcs1900=True)
