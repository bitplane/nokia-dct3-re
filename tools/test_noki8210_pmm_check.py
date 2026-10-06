import unittest

try:
    from tools.noki8210_pmm_check import checksum, replay
except ModuleNotFoundError:
    from noki8210_pmm_check import checksum, replay


class PmmReplayTest(unittest.TestCase):
    def fixture(self):
        image = bytearray(b'\xff' * 0x30000)
        image[0x10018:0x1001a] = b'\x00\x64'
        return image

    def test_short_record_and_deleted_record(self):
        image = self.fixture()
        image[0x10020:0x1002a] = bytes.fromhex('0800001201020900aabb')
        cache, records, end = replay(image)
        self.assertEqual(cache[0x12:0x14], b'\x01\x02')
        self.assertEqual(len(records), 2)
        self.assertTrue(records[1][3])
        self.assertEqual(end, 0x1002c)

    def test_checksum_excludes_two_bytes(self):
        cache = bytearray(0x8000)
        cache[0x120] = 3
        cache[0x154:0x156] = b'\xff\xff'
        self.assertEqual(checksum(cache), 3)

    def test_reject_out_of_range(self):
        image = self.fixture()
        image[0x10020:0x10026] = bytes.fromhex('08007fff0102')
        with self.assertRaisesRegex(ValueError, 'out-of-range'):
            replay(image)


if __name__ == '__main__':
    unittest.main()
