import unittest

from tools.dct3_pmm_catalog import find_catalogs, parse_catalog


class Dct3PmmCatalogTest(unittest.TestCase):
    def test_parses_linked_records(self):
        image = bytearray(b"\xff" * 0x60)
        image[6:12] = b"PMMCAT"
        image[0x20:0x2e] = bytes.fromhex("f482000155ffffff0002002e1234")
        image[0x2e:0x3f] = bytes.fromhex("f48b000355ffffff0005ffffaabbccddee")

        records = parse_catalog(bytes(image))

        self.assertEqual([0xf482, 0xf48b], [record.entry_type for record in records])
        self.assertEqual([1, 3], [record.index for record in records])
        self.assertEqual(bytes.fromhex("1234"), records[0].payload)
        self.assertEqual(bytes.fromhex("aabbccddee"), records[1].payload)

    def test_rejects_noncontiguous_link(self):
        image = bytearray(b"\xff" * 0x40)
        image[6:12] = b"PMMCAT"
        image[0x20:0x2e] = bytes.fromhex("f482000155ffffff000200301234")

        with self.assertRaisesRegex(ValueError, "next"):
            parse_catalog(bytes(image))

    def test_finds_multiple_catalogs(self):
        image = bytearray(b"\xff" * 0x80)
        for base, index in ((0, 1), (0x40, 2)):
            image[base + 6:base + 12] = b"PMMCAT"
            image[base + 0x20:base + 0x2e] = (
                bytes.fromhex(f"f482{index:04x}55ffffff0002ffff1234"))

        catalogs = find_catalogs(bytes(image))

        self.assertEqual([0, 0x40], [base for base, _entries in catalogs])
        self.assertEqual([1, 2], [entries[0].index for _base, entries in catalogs])


if __name__ == "__main__":
    unittest.main()
