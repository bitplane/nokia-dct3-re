import unittest

try:
    from tools.noki8210_registration_check import verify
except ModuleNotFoundError:
    from noki8210_registration_check import verify


class RegistrationTest(unittest.TestCase):
    def setUp(self):
        self.lines = [
            'TX packet type=56 payload=160 words=81 data=0004',
            'TX packet type=02 radio_phase=candidate_channel_change data=040000000000005050000004',
            'TX packet type=0c radio_phase=random_access',
            'RX enqueue type=89 payload=8 producer=001 data=0100000000000000',
            'TX packet type=1b data=0080013f4905087200f110000133080910101032547698',
            'LAPDm Location Updating Accept acknowledged nr=1',
            'TX packet type=1b data=0080032101',
            'LAPDm Channel Release acknowledged nr=2',
            'TX packet type=1b data=0080034101',
            'update-binary fid=6f7e offset=4 length=5',
            'TX packet type=02 radio_phase=release_channel_change data=040000000000001a600000040000000f00000000',
            'RX enqueue type=89 payload=8 producer=001 data=0000000000000000',
            'RX enqueue type=80 payload=34 producer=001 data=60' + '0'*18 + '1506210001f0',
        ]
        self.storage = bytearray(1611)
        self.storage[1604:1609] = bytes.fromhex('00f1100001')

    def test_accept(self):
        verify('\n'.join(self.lines), self.storage)

    def test_configured_carrier_requires_sch_and_recovered_channel_parameters(self):
        with self.assertRaises(ValueError):
            verify('\n'.join(self.lines), self.storage, configured_carrier=True)
        lines = [line.replace('data=040000', 'data=041202') for line in self.lines]
        lines.insert(1, 'RX enqueue type=80 payload=14 producer=001 data=4012000004b00004000048006100')
        text = '\n'.join(lines)
        verify(text, self.storage, configured_carrier=True)
        for broken in (text.replace('b00004000048', 'b00001000048'),
                       text.replace('data=041202', 'data=040000', 1),
                       text.replace('release_channel_change data=041202',
                                    'release_channel_change data=040000')):
            with self.subTest(text=broken), self.assertRaises(ValueError):
                verify(broken, self.storage, configured_carrier=True)

    def test_reject_sibling_capability(self):
        self.lines[4] = self.lines[4].replace('330809', '230809')
        with self.assertRaises(ValueError):
            verify('\n'.join(self.lines), self.storage)

    def test_dcs_requires_own_scan_capability_and_carrier(self):
        lines = [line.replace('data=0004', 'data=03370338')
                 .replace('data=040000', 'data=041202')
                 .replace('000004', '000337').replace('330809', '300809')
                 for line in self.lines]
        lines[-1] = 'RX enqueue type=80 payload=34 producer=001 data=601200000c4b033700001506210001f0'
        lines.insert(1, 'RX enqueue type=80 payload=14 producer=001 data=4012000006af033700004800b900')
        text = '\n'.join(lines)
        verify(text, self.storage, dcs1800=True)
        for broken in (text.replace('03370338', '00040005'),
                       text.replace('300809', '330809'),
                       text.replace('af0337000048', 'af0004000048'),
                       text.replace('1a60000337', '1a60000004'),
                       text.replace('4b033700001506', '4b000400001506')):
            with self.subTest(text=broken), self.assertRaises(ValueError):
                verify(broken, self.storage, dcs1800=True)
        with self.assertRaisesRegex(ValueError, 'mutually exclusive'):
            verify(text, self.storage, configured_carrier=True, dcs1800=True)

    def test_reject_stale_location(self):
        self.storage[1610] = 1
        with self.assertRaisesRegex(ValueError, 'not location-updated'):
            verify('\n'.join(self.lines), self.storage)

    def test_reject_missing_release(self):
        del self.lines[7]
        with self.assertRaises(ValueError):
            verify('\n'.join(self.lines), self.storage)


if __name__ == '__main__':
    unittest.main()
