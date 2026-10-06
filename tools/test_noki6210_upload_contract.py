from pathlib import Path
import unittest
from tools.noki6210_upload_contract import assess

ROOT = Path(__file__).resolve().parents[1]
FLASH = ROOT / 'roms/noki6210/6210_556c.fls'
PMM = ROOT / 'roms/noki6210/6210 virgin eeprom 005fa000.fls'


class UploadContractTest(unittest.TestCase):
    def test_unknown_flash_rejected(self):
        with self.assertRaisesRegex(ValueError, 'pinned 6210'):
            assess(b'unknown', b'unknown')

    @unittest.skipUnless(FLASH.exists() and PMM.exists(), 'requires acquired product images')
    def test_acquired_upload_extents_and_original_journal(self):
        result = assess(FLASH.read_bytes(), PMM.read_bytes())
        self.assertEqual([row['words'] for row in result['uploads']], [104, 629])
        self.assertEqual(result['pmm_records'], 295)
        self.assertEqual(result['pmm_initial_length'], 0x9c4)
        self.assertEqual(result['base_computed'], '6d85')
        self.assertEqual(result['base_stored'], '6d85')
        self.assertEqual(result['journal_computed'], '6d85')
        self.assertEqual(result['journal_stored'], '6d85')

    @unittest.skipUnless(FLASH.exists(), 'requires acquired product image')
    def test_foreign_pmm_rejected(self):
        with self.assertRaisesRegex(ValueError, 'NPE-3 PMM'):
            assess(FLASH.read_bytes(), b'foreign')


if __name__ == '__main__':
    unittest.main()
