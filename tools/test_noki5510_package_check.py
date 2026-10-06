import tempfile
import unittest
from pathlib import Path
from tools import noki5510_package_check as checker
from tools.extract_dct3_wintesla import decode_records


class PackageCheckTest(unittest.TestCase):
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
