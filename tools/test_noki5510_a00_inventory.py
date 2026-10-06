import struct
import unittest
from tools.noki5510_a00_inventory import inventory


def segment(marker, payload):
    return struct.pack('>HI', marker, len(payload)) + payload + bytes.fromhex('12348888')


class A00InventoryTest(unittest.TestCase):
    def test_extent_and_trailer_are_distinct(self):
        image = segment(0xaa55, b'ab') + segment(0xaa22, b'c')
        report = inventory(image)
        self.assertEqual(report['coverage_bytes'], len(image))
        self.assertEqual(report['segments'][1]['offset'], 12)
        self.assertEqual(report['segments'][0]['opaque_trailer'], '12348888')
        self.assertEqual(report['segments'][0]['payload_bytes'], 2)

    def test_truncation_is_rejected(self):
        image = segment(0xaa55, b'abc')
        for length in range(len(image)):
            with self.assertRaises(ValueError):
                inventory(image[:length])

    def test_unknown_marker_and_wrong_start_are_rejected(self):
        for marker in (0xffff, 0xaa22):
            with self.assertRaises(ValueError):
                inventory(segment(marker, b'x'))


if __name__ == '__main__':
    unittest.main()
