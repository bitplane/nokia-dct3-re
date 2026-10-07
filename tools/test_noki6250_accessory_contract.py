import unittest
from pathlib import Path

from tools.noki6250_accessory_contract import verify


class AccessoryContractTest(unittest.TestCase):
    def test_foreign_image(self):
        with self.assertRaisesRegex(ValueError, 'NHM-3'):
            verify(b'foreign firmware')

    def test_own_contract(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki6250/6250-503mcuppmc.fls'
        if not path.exists():
            self.skipTest('acquired firmware unavailable')
        contract = verify(path.read_bytes())
        self.assertEqual(contract['selector'], 0)
        self.assertEqual(contract['high_threshold'], 0x312)
        self.assertEqual(contract['electrical_unattached_level'], 'VBB pull-up; raw scale unmeasured')


if __name__ == '__main__':
    unittest.main()
