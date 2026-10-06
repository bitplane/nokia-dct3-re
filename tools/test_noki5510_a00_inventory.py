import struct
import unittest
from tools.noki5510_a00_inventory import inventory, serial_boot_inventory, section_inventory


def segment(marker, payload):
    return struct.pack('>HI', marker, len(payload)) + payload + bytes.fromhex('12348888')


class A00InventoryTest(unittest.TestCase):
    def boot_image(self):
        return struct.pack('>13H', 0x08aa, 0x18, 3, 0x800, 0x10,
                           1, 0x492b, 2, 2, 0x900, 0xbeef, 0x1234, 0)

    def test_serial_header_and_word_addresses(self):
        image = self.boot_image()
        report = serial_boot_inventory(image)
        self.assertEqual(report['entry_word_address'], 0x1492b)
        self.assertEqual(report['compatibility_words'], ['0018', '0003', '0800', '0010'])
        self.assertEqual(report['sections'][0]['destination_word_address'], 0x20900)
        self.assertEqual(report['sections'][0]['words'], 2)
        self.assertEqual(report['coverage_bytes'], len(image))
        self.assertEqual(inventory(segment(0xaa55, image))['segments'][0]['serial_boot'], report)

    def test_serial_truncation_and_trailing_bytes(self):
        image = self.boot_image()
        for length in range(len(image)):
            with self.assertRaises(ValueError):
                serial_boot_inventory(image[:length])
        with self.assertRaises(ValueError):
            serial_boot_inventory(image + b'\x00\x00')

    def test_serial_unsupported_page_and_signature(self):
        for offset, value in ((0, 0xffff), (10, 0x80), (16, 0x80)):
            image = bytearray(self.boot_image())
            struct.pack_into('>H', image, offset, value)
            with self.assertRaises(ValueError):
                serial_boot_inventory(image)

    def test_overlay_retains_extended_address(self):
        stream = struct.pack('>6H', 2, 2, 0x2000, 0xbeef, 0x1234, 0)
        image = segment(0xaa55, self.boot_image()) + segment(0xaa22, stream)
        report = inventory(image)['segments'][1]['section_stream']
        self.assertEqual(report['coverage_bytes'], len(stream))
        self.assertEqual(report['sections'][0]['destination_word_address'], 0x22000)
        for length in range(len(stream)):
            with self.assertRaises(ValueError):
                section_inventory(stream[:length])
        with self.assertRaises(ValueError):
            section_inventory(stream + b'\x00\x00')

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
