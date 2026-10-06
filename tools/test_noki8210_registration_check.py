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

    def test_reject_sibling_capability(self):
        self.lines[4] = self.lines[4].replace('330809', '230809')
        with self.assertRaises(ValueError):
            verify('\n'.join(self.lines), self.storage)

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
