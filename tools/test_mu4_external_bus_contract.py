import unittest
from pathlib import Path

from tools.mu4_external_bus_contract import contract, decode_bscr, wait_states


class ExternalBusContractTest(unittest.TestCase):
    SOURCE = Path(__file__).resolve().parents[1] / 'roms/5510-mu4-reference/InitData_R060.a00'

    @unittest.skipUnless(SOURCE.exists(), 'private original R060 source unavailable')
    def test_original_source_requests_and_scope(self):
        report = contract(self.SOURCE.read_bytes())
        self.assertEqual([row['register_address'] for row in report['writers']], ['0028', '0029'])
        self.assertEqual(report['wait_prediction_assuming_reset_swcr'], {'program': 7, 'data': 7, 'io': 7})
        self.assertFalse(report['exact_da150_identity'])
        self.assertEqual(report['reset_hypothesis']['bscr'], 'f000')

    def test_other_source_rejected(self):
        with self.assertRaises(ValueError):
            contract(b'not the original source')

    def test_resident_bscr_uses_c5416_dialect(self):
        value = decode_bscr(0x8006)
        self.assertTrue(value['consecutive_external_reads'])
        self.assertEqual(value['external_read_framing_cycles_when_consecutive'], 3)
        self.assertEqual(value['clkout_divisor'], 1)
        self.assertFalse(value['iack_output_disabled'])
        self.assertTrue(value['hpi_bus_holder'])
        self.assertTrue(value['data_bus_holder'])
        self.assertEqual(value['reserved_written_bits'], '0000')

    def test_bscr_fields_and_reserved_bits_exhaustive(self):
        for value in range(0x10000):
            result = decode_bscr(value)
            self.assertEqual(int(result['writable_value'], 16), value & 0xf006)
            self.assertEqual(int(result['reserved_written_bits'], 16), value & 0x0ff9)
            self.assertEqual(result['clkout_divisor'], 1 + ((value >> 13) & 3))
            self.assertEqual(result['consecutive_external_reads'], bool(value & 0x8000))
            self.assertEqual(result['external_read_framing_cycles_when_consecutive'], 3 if value & 0x8000 else None)
            self.assertEqual(result['iack_output_disabled'], bool(value & 0x1000))
            self.assertEqual(result['hpi_bus_holder'], bool(value & 4))
            self.assertEqual(result['data_bus_holder'], bool(value & 2))
            self.assertEqual(result['nonconsecutive_bank_boundary_words'], 0x8000)

    def test_wait_field_selection_exhaustive(self):
        for value in range(0x10000):
            for multiplier in (0, 1):
                for space, address, shift in (
                        ('io', 0, 12), ('data', 0x7fff, 6), ('data', 0x8000, 9),
                        ('program', 0x3fffff if value & 0x8000 else 0x277fff, 0),
                        ('program', 0x400000 if value & 0x8000 else 0x278000, 3)):
                    self.assertEqual(wait_states(value, multiplier, space, address, external=True),
                                     ((value >> shift) & 7) * (multiplier + 1))

    def test_internal_access_ignores_external_wait_fields(self):
        for space in ('program', 'data'):
            self.assertEqual(wait_states(0xffff, 0xffff, space, 0, external=False), 0)

    def test_swcr_reserved_bits_do_not_multiply_waits(self):
        self.assertEqual(wait_states(0x7fff, 0xfffe, 'io', 0x4000, external=True), 7)
        self.assertEqual(wait_states(0x7fff, 0xffff, 'io', 0x4000, external=True), 14)

    def test_invalid_inputs_rejected(self):
        for value in (-1, 0x10000, '7fff'):
            with self.assertRaises(ValueError):
                decode_bscr(value)
        for arguments in ((0, 0, 'unknown', 0, True), (0, 0, 'data', 0x10000, True),
                          (0, 0, 'program', 0x800000, True), (0, 0, 'io', -1, True),
                          (0, 0, 'io', 0, None), (0, 0, 'io', 0, False),
                          (-1, 0, 'io', 0, True)):
            with self.assertRaises(ValueError):
                wait_states(*arguments[:4], external=arguments[4])


if __name__ == '__main__':
    unittest.main()
