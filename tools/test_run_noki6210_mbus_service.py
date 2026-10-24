from pathlib import Path
import tempfile
import unittest

from tools.run_noki6210_mbus_service import storage


class MbusStorageTest(unittest.TestCase):
    def test_compares_names_and_complete_content_not_timestamps(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first, second = root / 'a', root / 'b'
            first.mkdir()
            second.mkdir()
            (first / 'flash').write_bytes(b'abc')
            (second / 'flash').write_bytes(b'abc')
            self.assertEqual(storage(first), storage(second))
            (second / 'flash').write_bytes(b'abd')
            self.assertNotEqual(storage(first), storage(second))
            (second / 'flash').unlink()
            (second / 'sim').write_bytes(b'abc')
            self.assertNotEqual(storage(first), storage(second))
