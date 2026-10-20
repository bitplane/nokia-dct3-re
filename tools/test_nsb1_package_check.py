import contextlib
import io
from pathlib import Path
import tempfile
import struct
import unittest
from unittest.mock import patch
from zipfile import ZipFile

from tools import nsb1_package_check as checker


class PackageTest(unittest.TestCase):
    def test_post_coverage_preserves_unknown_arguments(self):
        calls = [{'callsite': index, 'arguments': {'task': task, 'event': event}}
                 for index, (task, event) in enumerate(((20, 33), (None, 33), (20, None), (1, 20)))]
        report = checker.post_coverage(calls)
        self.assertEqual((4, 2, 2), (report['direct_calls'], report['both_arguments_recovered'],
                                     report['incomplete_arguments']))
        self.assertEqual(calls[:2], report['event_21_candidates'])
        self.assertEqual([calls[1]], report['unknown_destination_calls'])
        self.assertFalse(report['absence_proved'])

    def test_security_directory_uses_32_bit_ids(self):
        image = bytearray(0xcc400)
        struct.pack_into('>IHH', image, 0xcc3d8, 0x2cc0a8, 2, 0)
        struct.pack_into('>IHH', image, 0xcc0b0, 0x701, 0x4a8, 8)
        struct.pack_into('>IHH', image, 0xcc0b8, 0x702, 0x4b0, 0x30)
        result = checker.security_record_directory(image)
        self.assertEqual([{'id': '0x701', 'offset': '0x4a8', 'length': 8},
                          {'id': '0x702', 'offset': '0x4b0', 'length': 48}], result['records'])
        self.assertFalse(result['field_semantics_proved'])
        struct.pack_into('>I', image, 0xcc0b0, 0x10701)
        with self.assertRaisesRegex(ValueError, 'identity'):
            checker.security_record_directory(image)
        struct.pack_into('>IHH', image, 0xcc3d8, 0x1fffff, 2, 0)
        with self.assertRaisesRegex(ValueError, 'bounds'):
            checker.security_record_directory(image)
        with self.assertRaisesRegex(ValueError, 'truncated'):
            checker.security_record_directory(b'')

    def test_flash_integrity_word_order_and_bounds(self):
        image = bytearray(0x40)
        image[0x24:0x2a] = bytes.fromhex('20003020003f')
        image[0x2a:0x30] = b'\xff' * 6
        struct.pack_into('>8H', image, 0x30, *range(1, 9))
        struct.pack_into('>H', image, 0x22, 36)
        report = checker.flash_integrity(image)
        self.assertEqual('0x24', report['computed'])
        self.assertFalse(report['whole_flash_authentication'])
        image[0x30] = 1
        with self.assertRaisesRegex(ValueError, 'sum mismatch'):
            checker.flash_integrity(image)
        image[0x27:0x2a] = bytes.fromhex('200040')
        with self.assertRaisesRegex(ValueError, 'span'):
            checker.flash_integrity(image)
    def test_eeprom_descriptor_fields(self):
        geometry = checker.eeprom_geometry(0xf4)
        self.assertEqual((2, 32, 8192), (geometry['address_bytes'], geometry['page_bytes'], geometry['capacity_bytes']))
        self.assertFalse(geometry['fitted_part_proved'])
        self.assertEqual(1, checker.eeprom_geometry(0xb4)['address_bytes'])
        with self.assertRaises(ValueError):
            checker.eeprom_geometry(0)

    def test_unknown_archive(self):
        with self.assertRaisesRegex(ValueError, 'acquired'):
            checker.inspect(b'not a package')

    def test_unknown_ppm(self):
        with self.assertRaisesRegex(ValueError, 'selection'):
            checker.inspect(b'', 'C')

    def test_cannot_overwrite_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'original.zip'
            source.write_bytes(b'original')
            with patch('sys.argv', ['check', str(source), '--output', str(source)]), \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                checker.main()
            self.assertEqual(b'original', source.read_bytes())

    def source(self):
        path = Path(__file__).resolve().parents[1] / 'roms/archive-dct3-packages/5190e671.zip'
        if not path.exists():
            self.skipTest('acquired NSB-1 package unavailable')
        return path.read_bytes()

    def test_both_own_ppms_and_absent_gap(self):
        source = self.source()
        a, report = checker.inspect(source, 'A')
        b, other = checker.inspect(source, 'B')
        self.assertEqual(0x100000, len(a))
        self.assertEqual(a[:0xe0000], b[:0xe0000])
        self.assertNotEqual(a[0xe0000:], b[0xe0000:])
        self.assertEqual(b'\xff' * 0xd300, a[0xd2d00:0xe0000])
        self.assertEqual('27b8f0cb8640e5b423960b206e9aee065f32d726', report['normalized_sha1'])
        self.assertFalse(other['boot_claim'])
        self.assertFalse(report['resident_rom4_supplied'])
        self.assertFalse(report['provisioning_supplied'])

    def test_member_inventory_is_closed(self):
        source = self.source()
        altered = io.BytesIO()
        with ZipFile(io.BytesIO(source)) as original, ZipFile(altered, 'w') as output:
            for item in original.infolist():
                if item.filename != 'nsb1___6.71b':
                    output.writestr(item.filename, original.read(item.filename))
        with patch.object(checker.hashlib, 'sha256') as digest:
            digest.return_value.hexdigest.return_value = checker.SHA256
            with self.assertRaisesRegex(ValueError, 'inventory'):
                checker.inspect(altered.getvalue())
