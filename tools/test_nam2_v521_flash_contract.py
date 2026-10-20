import unittest
import struct
from pathlib import Path

from tools.nam2_v521_flash_contract import inspect, select_family, check_package, sleep_accounting_contract, idle_control_contract


class Nam2FlashContractTest(unittest.TestCase):
    def accounting_image(self):
        image=bytearray(0x10714a)
        for address,word in ((0x2fada2,0x4a28),(0x2fada6,0x3032),(0x2fadba,0x234f),(0x2fadac,0x80d0),(0x2fadce,0x80d0),
                             (0x307098,0xb43c),(0x307140,0xbc3c),(0x307146,0xbc3c)):
            struct.pack_into('>H',image,address-0x200000,word)
        struct.pack_into('>I',image,0xfae44,0x10f9f4)
        image[0xfadbe:0xfadc2]=bytes.fromhex('f00cf96b')
        image[0xfadca:0xfadce]=bytes.fromhex('f00cf965')
        return image

    def test_store_opcode_does_not_determine_memory_owner(self):
        image=self.accounting_image()
        report=sleep_accounting_contract(image)
        self.assertEqual(['0x10f9fa']*2,[row['target'] for row in report['stores']])
        struct.pack_into('>I',image,0xfae44,0x20000)
        report=sleep_accounting_contract(image)
        self.assertEqual(['0x20006']*2,[row['target'] for row in report['stores']])
        self.assertTrue(all(row['is_mad2_mmio'] for row in report['stores']))

    def test_base_register_and_callee_preservation_are_required(self):
        for offset,word in ((0xfada2,0x4b28),(0xfadac,0x80c8),
                            (0x107140,0xbc38),(0xfadc0,0xf96a)):
            image=self.accounting_image();struct.pack_into('>H',image,offset,word)
            with self.assertRaises(ValueError):sleep_accounting_contract(image)

    def test_last_match_wins(self):
        rows = [(1, 0xB0, 0xFFFF, 0xFFFF), (4, 0xB0, 0xEA, 0xFFFF)]
        self.assertEqual(1, select_family(rows, 0xB0, 0xD0, 0))
        self.assertEqual(4, select_family(rows, 0xB0, 0xEA, 0))

    def test_all_three_identifiers_must_match(self):
        rows = [(3, 0x89, 0x88C2, 1)]
        self.assertEqual(0, select_family(rows, 0x89, 0x88C2, 0))
        self.assertEqual(3, select_family(rows, 0x89, 0x88C2, 1))

    def test_rejects_unidentified_input(self):
        with self.assertRaisesRegex(ValueError, "unchanged acquired"):
            inspect(bytes(0x200000))

    def test_acquired_table_when_available(self):
        source = Path(__file__).resolve().parents[1] / "roms/noki2100/2100f521sharp.fls"
        if not source.exists():
            self.skipTest("acquired NAM-2 v5.21 image not present")
        result = inspect(source.read_bytes())
        idle = result['idle_control_flow']
        self.assertFalse(idle['helper_restores_global_delivery'])
        self.assertFalse(idle['all_idle_exit_traverses_explicit_enable'])
        self.assertEqual('0x2000d', idle['clock_store_target'])
        self.assertEqual(10, len(result["rows"]))
        self.assertEqual((1, 0xB0, 0xFFFF, 0xFFFF), result["rows"][2])
        self.assertEqual(1, result["selected_family"])
        candidate = result["intel_c3_top_candidate"]
        self.assertEqual(3, candidate["selected_family"])
        self.assertFalse(candidate["fitted_part_proven"])
        interrupt = result['interrupt_control_consumers']
        self.assertEqual(3, interrupt['irq_disable_read_bit'])
        self.assertEqual(1, interrupt['fiq_disable_read_bit'])
        self.assertFalse(interrupt['read_masking_justified'])
        event = result['idle_irq3_event']
        self.assertEqual('00ff0000ffffffff', event['rom_bytes'])
        self.assertEqual(0x331030, 0x330c68 + event['event'] * event['rom_stride'])
        self.assertEqual(0x101334, 0x100d88 + event['event'] * event['runtime_stride'])
        self.assertFalse(event['task_destination_proven'])
        self.assertFalse(event['initial_expiry_links_task'])
        self.assertEqual('0x2abcec', event['expiry'])
        timer = result['software_sleep_accounting_writers']
        self.assertEqual(['80d0', '80d0'], [store['opcode'] for store in timer['stores']])
        self.assertEqual('0x2fae44',timer['base_literal'])
        self.assertEqual('0x10f9f4',timer['base'])
        self.assertTrue(timer['divide_preserves_r2'])
        self.assertEqual(['0x10f9fa']*2,[store['target'] for store in timer['stores']])
        self.assertFalse(any(store['is_mad2_mmio'] for store in timer['stores']))

    def test_idle_contract_rejects_changed_return_or_clock_base(self):
        source = Path(__file__).resolve().parents[1] / 'roms/noki2100/2100f521sharp.fls'
        if not source.exists():
            self.skipTest('acquired NAM-2 image not present')
        for offset in (0xf868a, 0x104c56, 0x104c70):
            image = bytearray(source.read_bytes())
            image[offset] ^= 1
            with self.assertRaises(ValueError):
                idle_control_contract(image)

    def test_package_prefix_when_available(self):
        root = Path(__file__).resolve().parents[1]
        image = root / 'roms/noki2100/2100f521sharp.fls'
        installer = root / 'roms/archive-dct3-packages/NAM-2_MCU_v5.21.EXE'
        if not image.exists() or not installer.exists():
            self.skipTest('acquired NAM-2 inputs not present')
        report = check_package(installer.read_bytes(), image.read_bytes())
        self.assertEqual(0x1f0000, report['equal_prefix_bytes'])
        self.assertEqual(2, len(report['members']))
        self.assertFalse(report['pmm_provenance_claim'])
        with self.assertRaisesRegex(ValueError, 'installer'):
            check_package(b'invalid', image.read_bytes())
