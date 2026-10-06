import struct
import unittest
from tools.noki5510_a00_inventory import inventory, serial_boot_inventory, section_inventory, extract_section


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

    def test_destination_overlap_preserves_record_order_and_pages(self):
        stream = struct.pack('>16H',
                             3, 2, 0xfffe, 0x1111, 0x2222, 0x3333,
                             2, 2, 0xffff, 0x2222, 0x4444,
                             1, 0, 0xffff, 0x5555, 0)
        report = section_inventory(stream)['destination_inventory']
        self.assertEqual(report['write_words'], 6)
        self.assertEqual(report['unique_words'], 4)
        self.assertEqual(report['repeated_same_words'], 1)
        self.assertEqual(report['repeated_changed_words'], 1)
        self.assertEqual(report['cross_page_records'], 2)
        self.assertEqual([row['page'] for row in report['pages']], [0, 2, 3])
        self.assertEqual(report['pages'][1]['unique_words'], 2)
        self.assertEqual(report['pages'][2]['first_word_address'], 0x30000)

    def test_destination_extent_exceeding_address_space_is_rejected(self):
        stream = struct.pack('>6H', 2, 0x7f, 0xffff, 1, 2, 0)
        with self.assertRaisesRegex(ValueError, '23-bit'):
            section_inventory(stream)

    def test_cross_segment_conflicts_are_not_hidden_by_individual_reports(self):
        stream = struct.pack('>6H', 2, 2, 0x900, 0xbeef, 0x5678, 0)
        report = inventory(segment(0xaa55, self.boot_image()) + segment(0xaa22, stream))
        combined = report['all_segment_destinations']
        self.assertEqual(combined['unique_words'], 2)
        self.assertEqual(combined['repeated_same_words'], 1)
        self.assertEqual(combined['repeated_changed_words'], 1)
        self.assertEqual(report['segments'][1]['section_stream']
                         ['destination_inventory']['repeated_changed_words'], 0)

    def test_extraction_preserves_words_and_requires_exact_selection(self):
        image = self.boot_image()
        expected = bytes.fromhex('beef1234')
        self.assertEqual(extract_section(image, 0x20900), expected)
        wrapped = segment(0xaa55, image)
        self.assertEqual(extract_section(wrapped, 0x20900, 'aa55'), expected)
        for source, address, marker in ((image, 0x900, None),
                                        (image, 0x20900, 'aa55'),
                                        (wrapped, 0x20900, None),
                                        (wrapped, 0x20900, 'aa22')):
            with self.assertRaises(ValueError):
                extract_section(source, address, marker)

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
