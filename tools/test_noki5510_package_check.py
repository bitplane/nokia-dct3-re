import tempfile
import unittest
from pathlib import Path
from tools import noki5510_package_check as checker
from tools.extract_dct3_wintesla import decode_records


class PackageCheckTest(unittest.TestCase):
    def test_dsp_rx_router_uses_big_endian_addresses_and_full_type_extent(self):
        table = bytes.fromhex('003a53b2') * 13
        routes = checker.decode_dsp_rx_routes(table)
        self.assertEqual([row['type'] for row in routes], list(range(0x83, 0x90)))
        self.assertEqual(routes[11], {'type': 0x8e, 'branch': 0x3a53b2})
        self.assertNotEqual(checker.decode_dsp_rx_routes(bytes.fromhex('b2533a00') * 13), routes)

    def test_dsp_rx_router_rejects_incomplete_extent(self):
        with self.assertRaisesRegex(ValueError, 'extent'):
            checker.decode_dsp_rx_routes(bytes(12 * 4))

    def test_thumb_reference_census_signed_calls_and_big_endian_pointers(self):
        image = bytes.fromhex('f000f804f7fffffc0020000d')
        report = checker.thumb_reference_census(image, (0x200000, 0x20000c))
        self.assertEqual(report['aligned_pairs_scanned'], 5)
        self.assertEqual(report['plausible_bl_pairs'], 2)
        self.assertEqual(report['direct_calls'], {0x200000: [0x200004], 0x20000c: [0x200000]})
        self.assertEqual(report['thumb_pointers'][0x20000c], [0x200008])
        self.assertIn('computed calls', report['exclusions'])
        swapped = b''.join(image[index:index + 2][::-1] for index in range(0, len(image), 2))
        self.assertEqual(checker.thumb_reference_census(swapped, (0x20000c,))['direct_calls'],
                         {0x20000c: []})

    def test_thumb_reference_census_short_images_have_no_pairs(self):
        for length in range(4):
            report = checker.thumb_reference_census(bytes(length), (0x200000,))
            self.assertEqual(report['aligned_pairs_scanned'], 0)
            self.assertEqual(report['direct_calls'][0x200000], [])

    def test_class_router_preserves_task_and_function_destinations(self):
        table = bytearray(39 * 8)
        table[:8] = bytes.fromhex('d20000000000001d')
        table[8:16] = bytes.fromhex('0c000000003b0199')
        routes = checker.decode_class_routes(table)
        self.assertEqual(len(routes), 39)
        self.assertEqual(routes[0], {'class': 0xd2, 'destination': 29})
        self.assertEqual(routes[1]['destination'], 0x3b0199)

    def test_class_router_rejects_incomplete_extent(self):
        with self.assertRaisesRegex(ValueError, 'extent'):
            checker.decode_class_routes(bytes(38 * 8))

    def test_wrong_archive_fails_before_extraction(self):
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / 'package.exe'
            package.write_bytes(b'not the acquired archive')
            with self.assertRaisesRegex(ValueError, 'SHA-256'):
                checker.normalize(package)

    def test_wrong_image_is_not_a_product(self):
        with self.assertRaises(ValueError):
            checker.assess_flash(bytes(0x350000))

    def test_bootstrap_rejects_changed_consumer(self):
        with self.assertRaisesRegex(ValueError, 'consumer code'):
            checker.assess_bootstrap(bytes(0x350000), [])

    def test_input_rejects_changed_consumer(self):
        with self.assertRaisesRegex(ValueError, 'input consumer code'):
            checker.assess_input(bytes(0x350000))

    def test_existing_different_artifact_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'image'
            checker.write_derived(path, b'original')
            checker.write_derived(path, b'original')
            with self.assertRaises(ValueError):
                checker.write_derived(path, b'changed')
            self.assertEqual(path.read_bytes(), b'original')

    def test_byte_stream_uses_existing_record_grammar(self):
        source = bytes.fromhex('0b2000000000000200') + b'ab'
        self.assertEqual(decode_records(source), (0x200000, b'ab'))
        with self.assertRaises(ValueError):
            decode_records(source[:-1])


if __name__ == '__main__':
    unittest.main()
