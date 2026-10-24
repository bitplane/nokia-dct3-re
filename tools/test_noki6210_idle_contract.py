from pathlib import Path
import unittest

from tools import noki6210_idle_contract as contract
from tools.nam2_v521_flash_contract import idle_control_contract

ROOT = Path(__file__).resolve().parents[1]
FLASH = ROOT / 'roms/noki6210/6210_556c.fls'
NAM2 = ROOT / 'roms/noki2100/2100f521sharp.fls'


class IdleContractTest(unittest.TestCase):
    def test_unknown_image_rejected(self):
        with self.assertRaisesRegex(ValueError, 'unchanged acquired'):
            contract.inspect(b'unknown')

    @unittest.skipUnless(FLASH.exists(), 'requires own acquired NPE-3')
    def test_own_idle_exits_and_literal_bases(self):
        image = FLASH.read_bytes()
        result = contract.inspect(image)
        self.assertEqual(result['clock_store_targets'], ['0x2000d'] * 2)
        self.assertEqual(result['final_idle_predicate_constant'], 1)
        self.assertFalse(result['physical_clock_semantics_established'])
        for address in [row[0] for row in contract.BLOCKS] + [0x500a54]:
            changed = bytearray(image)
            changed[address-contract.BASE] ^= 1
            with self.subTest(address=hex(address)), self.assertRaises(ValueError):
                contract.decode_blocks(changed)

    @unittest.skipUnless(FLASH.exists() and NAM2.exists(), 'requires both own images')
    def test_shared_conclusion_not_shared_addresses(self):
        npe3 = contract.inspect(FLASH.read_bytes())
        nam2 = idle_control_contract(NAM2.read_bytes())
        for name in ('enable_value', 'both_helper_arms_clear_bit0',
                     'nonzero_argument_sets_bit1', 'helper_restores_global_delivery',
                     'all_idle_exit_traverses_explicit_enable'):
            self.assertEqual(npe3[name], nam2[name], name)
        self.assertNotEqual(npe3['clock_helper'], nam2['clock_helper'])


if __name__ == '__main__':
    unittest.main()
