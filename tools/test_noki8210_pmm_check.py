import unittest

try:
    from tools.noki8210_pmm_check import base_record_fixture, checksum, replay
except ModuleNotFoundError:
    from noki8210_pmm_check import base_record_fixture, checksum, replay


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

    def test_base_fixture_preserves_acquired_bytes_outside_journal(self):
        image = self.fixture()
        image[0x10020:0x10026] = bytes.fromhex('005980000000')
        image[0x10026:0x18026] = bytes(0x8000)
        image[0x18026:0x1802c] = bytes.fromhex('080002540001')
        fixture = base_record_fixture(bytes(image))
        self.assertEqual(fixture[:0x18026], image[:0x18026])
        self.assertEqual(fixture[0x20000:], image[0x20000:])
        self.assertEqual(fixture[0x18026:0x20000], b'\xff' * 0x7fda)
        self.assertEqual(len(replay(fixture)[1]), 1)

    def test_original_journal_retains_checksum_failure(self):
        image = self.fixture()
        image[0x10020:0x10026] = bytes.fromhex('005980000000')
        image[0x10026:0x18026] = bytes(0x8000)
        image[0x18026:0x1802c] = bytes.fromhex('080002540001')
        original = bytes(image)
        cache, records, _ = replay(original)
        self.assertEqual(len(records), 2)
        self.assertNotEqual(checksum(cache), int.from_bytes(cache[0x254:0x256], 'big'))
        repaired, _, _ = replay(base_record_fixture(original))
        self.assertEqual(checksum(repaired), int.from_bytes(repaired[0x254:0x256], 'big'))
        self.assertEqual(bytes(image), original)

    def test_invalid_base_checksum_is_not_repaired(self):
        image = self.fixture()
        image[0x10020:0x10026] = bytes.fromhex('005980000000')
        image[0x10026:0x18026] = bytes(0x8000)
        image[0x10146] = 1
        with self.assertRaisesRegex(ValueError, 'not checksum-valid'):
            base_record_fixture(image)


if __name__ == '__main__':
    unittest.main()
