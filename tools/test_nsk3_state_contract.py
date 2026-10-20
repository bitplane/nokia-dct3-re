import unittest
from pathlib import Path
from tools.nsk3_state_contract import decode


class StateContractTest(unittest.TestCase):
    def test_own_image_and_mutations(self):
        path=Path('roms/noki6130/6130-v561-boot4-a.fls')
        if not path.exists():self.skipTest('local own-ROM qualification fixture unavailable')
        image=path.read_bytes()
        self.assertEqual(decode(image)['selector'],0x834)
        for address in (0x2536bc,0x2544bc,0x219136,0x21a674,0x21a6a4,
                        0x253994,0x254868,0x2192e4,0x20af86,0x20e9da,
                        0x208df8,0x20ed20,0x208f34,0x295c22,0x21ac36,
                        0x21ac9a,0x21d670,0x2881e0,0x2880b6,0x2883ec,0x2883d4,
                        0x248616,0x248b9e,0x249292,0x24709c,0x24753e):
            changed=bytearray(image)
            changed[address-0x200000]^=1
            with self.subTest(address=hex(address)),self.assertRaises(ValueError):decode(changed)


if __name__=='__main__':unittest.main()
