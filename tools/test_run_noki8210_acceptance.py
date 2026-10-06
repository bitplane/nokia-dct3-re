import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import run_noki8210_acceptance as runner


class IsolatedAcceptanceTest(unittest.TestCase):
    def test_wrong_product_rejected_before_creating_run(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory) / 'run'
            with self.assertRaisesRegex(ValueError, 'MCU/PPM'):
                runner.prepare_run(run, b'wrong', b'wrong')
            self.assertFalse(run.exists())

    def test_wrong_pmm_rejected_before_creating_run(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory) / 'run'
            with patch.object(runner, 'MCU_SHA1', hashlib.sha1(b'mcu').hexdigest()):
                with self.assertRaisesRegex(ValueError, 'PMM image'):
                    runner.prepare_run(run, b'mcu', b'wrong')
            self.assertFalse(run.exists())

    def test_fresh_storage_and_existing_run_refusal(self):
        mcu, pmm = b'mcu', b'pmm'
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory) / 'run'
            with patch.object(runner, 'MCU_SHA1', hashlib.sha1(mcu).hexdigest()), \
                    patch.object(runner, 'PMM_SHA256', hashlib.sha256(pmm).hexdigest()), \
                    patch.object(runner, 'base_record_fixture', return_value=b'fixture'):
                runner.prepare_run(run, mcu, pmm)
                flash = run / 'nvram/nsm3hle/flash'
                self.assertEqual(flash.read_bytes(), b'mcufixture')
                flash.write_bytes(b'existing state')
                with self.assertRaises(FileExistsError):
                    runner.prepare_run(run, mcu, pmm)
                self.assertEqual(flash.read_bytes(), b'existing state')
                self.assertEqual(pmm, b'pmm')


if __name__ == '__main__':
    unittest.main()
