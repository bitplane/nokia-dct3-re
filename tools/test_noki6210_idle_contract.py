from pathlib import Path
import unittest

from tools import noki6210_idle_contract as contract
from tools.nam2_v521_flash_contract import idle_control_contract

ROOT = Path(__file__).resolve().parents[1]
FLASH = ROOT / 'roms/noki6210/6210_556c.fls'
NAM2 = ROOT / 'roms/noki2100/2100f521sharp.fls'


class IdleContractTest(unittest.TestCase):
    def test_conditional_branch_signedness(self):
        self.assertEqual(contract.conditional_target(0x1000,0xd000),0x1004)
        self.assertEqual(contract.conditional_target(0x1000,0xd07f),0x1102)
        self.assertEqual(contract.conditional_target(0x1000,0xd080),0xf04)
        self.assertEqual(contract.conditional_target(0x1000,0xd1ff),0x1002)
        for word in (0xe000,0xde00,0xdf00):
            with self.assertRaises(ValueError):
                contract.conditional_target(0x1000,word)

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
        inventory=result['permission_branch_inventory']
        self.assertEqual(inventory['enable_predecessors'],
                         ['0x4b03b0','0x4b03c0','0x4b03c8','0x4b03d0','0x4b03d8'])
        self.assertEqual(inventory['other_permission_targets'],['0x4b03f6']*2)
        for address in [row[0] for row in contract.BLOCKS] + [0x500a54]:
            changed = bytearray(image)
            changed[address-contract.BASE] ^= 1
            with self.subTest(address=hex(address)), self.assertRaises(ValueError):
                contract.decode_blocks(changed)

    @unittest.skipUnless(FLASH.exists(), 'requires own acquired NPE-3')
    def test_permission_and_control_provenance_mutations(self):
        image=FLASH.read_bytes()
        for address in (0x4b0354,0x4b035e,0x4b0362,0x4b045c,0x4b03a8,
                        0x4b03b0,0x4b03b8,0x4b03c0,0x4b03c8,0x4b03d0,0x4b03d8,0x4b03e0):
            changed=bytearray(image)
            changed[address-contract.BASE]^=1
            with self.subTest(address=hex(address)),self.assertRaises(ValueError):
                contract.enable_predecessors(changed)
        changed=bytearray(image)
        changed[0x4b034c-contract.BASE:0x4b034e-contract.BASE]=bytes.fromhex('7329')
        with self.assertRaisesRegex(ValueError,'inventory differs'):
            contract.enable_predecessors(changed)

    @unittest.skipUnless(FLASH.exists() and NAM2.exists(), 'requires both own images')
    def test_shared_conclusion_not_shared_addresses(self):
        npe3 = contract.inspect(FLASH.read_bytes())
        nam2 = idle_control_contract(NAM2.read_bytes())
        for name in ('enable_value', 'both_helper_arms_clear_bit0',
                     'nonzero_argument_sets_bit1', 'helper_restores_global_delivery',
                     'all_idle_exit_traverses_explicit_enable'):
            self.assertEqual(npe3[name], nam2[name], name)
        self.assertNotEqual(npe3['clock_helper'], nam2['clock_helper'])

    @unittest.skipUnless(FLASH.exists(), 'requires own acquired NPE-3')
    def test_wake_source_preparation_and_mutations(self):
        image = FLASH.read_bytes()
        result = contract.inspect(image)['wake_source_preparation']
        self.assertEqual(result['source'], 3)
        self.assertEqual(result['acknowledge_address'], '0x20009')
        self.assertEqual(result['acknowledge_value'], 8)
        self.assertEqual(result['mask_address'], '0x2000b')
        self.assertEqual(result['mask_clear_bits'], 8)
        for address in (0x4b0350, 0x4b0352, 0x4b0362, 0x4b045c,
                        0x4b03f6, 0x4b03f8, 0x4b03fa, 0x4b03fc,
                        0x4b03fe, 0x4b0400):
            changed = bytearray(image)
            changed[address-contract.BASE] ^= 1
            with self.subTest(address=hex(address)), self.assertRaises(ValueError):
                contract.wake_source_preparation(changed)


if __name__ == '__main__':
    unittest.main()
