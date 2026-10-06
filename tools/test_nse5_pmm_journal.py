import unittest

from tools.nse5_pmm_journal import check_trace, replay


def sector(records):
    image = bytearray([255] * 0x2000)
    image[6:12] = b"EEPROM"
    image[0x18:0x1A] = b"\x00\x01"
    image[0x20:0x20 + len(records)] = records
    return image


def write(destination, data, short=False):
    header = (len(data) << 10) if short else 0
    return (header.to_bytes(2, "big") +
            (b"" if short else len(data).to_bytes(2, "big")) +
            destination.to_bytes(2, "big") + data +
            (b"\xff" if len(data) & 1 else b""))


class JournalTests(unittest.TestCase):
    def test_npm5_explicit_version_and_large_sector(self):
        image = bytearray(b'\xff' * 0x10000)
        image[6:12] = b'EEPROM'
        image[0x18:0x1a] = b'\x00\x03'
        record = write(0, b'\xff' * 0x3800)
        image[0x20:0x20 + len(record)] = record
        cache, writes, end = replay(image, 0x3800, sector_size=0x10000, version=3)
        self.assertEqual(cache, b'\xff' * 0x3800)
        self.assertEqual(len(writes), 1)
        self.assertEqual(end, 0x3826)
        with self.assertRaises(ValueError):
            replay(image, 0x3800)

    def test_truncated_extended_fields_are_rejected(self):
        image = sector(b'')
        image[0x20:0x22] = bytes(2)
        with self.assertRaisesRegex(ValueError, 'extended'):
            replay(image, sector_size=0x22)
        with self.assertRaisesRegex(ValueError, 'destination'):
            replay(image, sector_size=0x24)

    def test_trace_requires_complete_matching_snapshot(self):
        self.assertEqual(check_trace(b"ab", "nse5_compat_storage_cache_snapshot: bytes=6162"), 1)
        for text in ("", "nse5_compat_storage_cache_snapshot: bytes=61",
                     "nse5_compat_storage_cache_snapshot: bytes=6163"):
            with self.assertRaises(ValueError):
                check_trace(b"ab", text)

    def test_ordered_overwrites_and_odd_padding(self):
        cache, writes, end = replay(sector(write(0, b"abc") + write(1, b"z", True)), 4)
        self.assertEqual(cache, b"azc\x00")
        self.assertEqual(len(writes), 2)
        self.assertEqual(end, 0x30)

    def test_header_and_sector_rejection(self):
        for image in (bytes(10), bytearray(0x2000)):
            with self.assertRaises(ValueError):
                replay(image)
        image = sector(b"")
        image[0x18:0x1A] = b"\x00\x02"
        with self.assertRaises(ValueError):
            replay(image)

    def test_cache_bounds(self):
        with self.assertRaises(ValueError):
            replay(sector(write(3, b"ab")), 4)

    def test_delete_not_silently_interpreted_as_write(self):
        with self.assertRaisesRegex(ValueError, "deletion"):
            replay(sector(b"\x05\x00"))

    def test_explicit_stop(self):
        cache, writes, end = replay(sector(b"\x02\x00"), 4)
        self.assertEqual((cache, writes, end), (bytes(4), [], 0x20))


if __name__ == "__main__":
    unittest.main()
