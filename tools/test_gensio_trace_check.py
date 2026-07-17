import unittest

from tools.gensio_trace_check import (
    check_accesses,
    check_adc,
    check_charger_irq,
    check_ccont_boot_status,
    check_select_contract,
    decode_transactions,
    parse_accesses,
)


class GensioTraceCheckTest(unittest.TestCase):
    def test_complete_read_and_write_transactions(self):
        text = """
[:] gensio: W off=2d data=25 old=21 pc=1 t=0
[:] gensio: R off=6d data=03 pc=1 t=0
[:] gensio: W off=2c data=54 old=00 pc=1 t=0
[:] gensio: R off=6d data=07 pc=1 t=0
[:] gensio: R off=6c data=0f pc=1 t=0
[:] gensio: W off=2d data=25 old=25 pc=1 t=0
[:] gensio: W off=2c data=28 old=54 pc=1 t=0
[:] gensio: W off=2c data=31 old=28 pc=1 t=0
"""
        errors, counts = check_accesses(parse_accesses(text))
        self.assertEqual([], errors)
        self.assertEqual({"accesses": 8, "reads": 1, "writes": 1}, counts)

    def test_read_requires_ready_status(self):
        accesses = [
            ("W", 0x2D, 0x25),
            ("W", 0x2C, 0x54),
            ("R", 0x6C, 0x00),
            ("W", 0x2D, 0x25),
            ("W", 0x2C, 0x28),
            ("W", 0x2C, 0x31),
        ]
        errors, _ = check_accesses(accesses)
        self.assertIn("CCONT data consumed before GENSIO status 0x07", errors)

    def test_rejects_unpaired_read(self):
        accesses = [
            ("W", 0x2D, 0x25),
            ("R", 0x6C, 0x00),
        ]
        errors, _ = check_accesses(accesses)
        self.assertIn("CCONT data read without a pending read command", errors)

    def test_adc_result_packing(self):
        transactions = []
        values = tuple((selector << 8) | selector for selector in range(8))
        for selector, value in enumerate(values):
            transactions.extend(
                (
                    ("W", 0, selector << 4),
                    ("R", 2, value & 0xFF),
                    ("R", 3, 0xB0 | ((value >> 8) & 0x03)),
                )
            )
        errors, counts = check_adc(transactions, values)
        self.assertEqual([], errors)
        self.assertEqual({"adc_reads": 16, "adc_selectors": 8}, counts)

    def test_adc_detects_bad_upper_bits(self):
        transactions = [("W", 0, selector << 4) for selector in range(8)]
        transactions.extend((("W", 0, 0x20), ("R", 3, 0xB0)))
        errors, _ = check_adc(transactions, (0, 0, 0x300, 0, 0, 0, 0, 0))
        self.assertIn(
            "ADC selector 2 register 3 returned 0xb0, expected 0xb3", errors
        )

    def test_charger_irq_lifecycle(self):
        transactions = [
            ("W", 0x0F, 0xF0),
            ("W", 0x00, 0x58), ("R", 0x02, 0xFF), ("R", 0x03, 0xB3),
            ("R", 0x0E, 0x0A),
            ("W", 0x0E, 0x08),
            ("R", 0x0E, 0x02),
            ("W", 0x00, 0x5C), ("R", 0x02, 0x00), ("R", 0x03, 0xB0),
        ]
        summary = "irq_seen=04\nfinal_irq_status=00\n"
        errors, counts = check_charger_irq(transactions, summary)
        self.assertEqual([], errors)
        self.assertEqual(
            {"serial_status": 1, "serial_ack": 1, "serial_clear_read": 1,
             "vchar_samples": 2},
            counts,
        )

    def test_charger_irq_requires_firmware_serial_service(self):
        errors, counts = check_charger_irq(
            [("W", 0x0F, 0xF0)],
            "irq_seen=04\nfinal_irq_status=00\n",
        )
        self.assertIn("firmware did not read the asserted CCONT charger source", errors)
        self.assertIn("firmware did not acknowledge CCONT charger source bit 3", errors)
        self.assertIn("CCONT charger source did not read clear after acknowledgement", errors)
        self.assertEqual(
            {"serial_status": 0, "serial_ack": 0, "serial_clear_read": 0,
             "vchar_samples": 0},
            counts,
        )

    def test_charger_present_startup_does_not_require_removal(self):
        transactions = [
            ("W", 0x0F, 0xF0),
            ("W", 0x00, 0x58), ("R", 0x02, 0xFF), ("R", 0x03, 0xB3),
            ("R", 0x0E, 0x0A), ("W", 0x0E, 0x08), ("R", 0x0E, 0x02),
        ]
        errors, _ = check_charger_irq(
            transactions, "irq_seen=04\nfinal_irq_status=00\n", False
        )
        self.assertEqual([], errors)

    def test_ccont_reset_status(self):
        errors, counts = check_ccont_boot_status([("R", 0x0E, 0x03)])
        self.assertEqual([], errors)
        self.assertEqual({"boot_status_reads": 1}, counts)

    def test_ccont_reset_status_rejects_missing_ready_bit(self):
        errors, _ = check_ccont_boot_status([("R", 0x0E, 0x02)])
        self.assertIn("CCONT reset status was 0x02, expected 0x03", errors)

    def test_decoder_exposes_register_transactions(self):
        accesses = [
            ("W", 0x2D, 0x25),
            ("W", 0x2C, 0x54),
            ("R", 0x6D, 0x07),
            ("R", 0x6C, 0x0F),
            ("W", 0x2D, 0x25),
            ("W", 0x2C, 0x28),
            ("W", 0x2C, 0x31),
        ]
        errors, _, transactions = decode_transactions(accesses)
        self.assertEqual([], errors)
        self.assertEqual([("R", 0x0A, 0x0F), ("W", 0x05, 0x31)], transactions)

    def test_cross_rom_select_contract(self):
        accesses = [
            ("W", 0xAF, 0x00), ("W", 0x6F, 0x00), ("W", 0xEF, 0x00),
            ("W", 0xAD, 0xC4), ("W", 0xED, 0x21), ("W", 0xAE, 0x20),
            ("W", 0xEE, 0x80), ("R", 0xAF, 0x00), ("W", 0xAF, 0x00),
            ("R", 0x6F, 0x00), ("W", 0x6F, 0x01),
        ]
        errors, counts = check_select_contract(accesses)
        self.assertEqual([], errors)
        self.assertEqual({"select_registers": 7, "select_reads": 2}, counts)


if __name__ == "__main__":
    unittest.main()
