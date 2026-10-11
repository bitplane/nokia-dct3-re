"""Independent integer oracle for the captured ROM4 SUBC division tail."""
import unittest


def division_tail(numerator, denominator):
    """15 quotient bits leave the remainder above bit 14 (unsigned domain)."""
    if numerator < 0 or denominator <= 0:
        raise ValueError("unsigned numerator and positive denominator required")
    quotient, remainder = divmod(numerator, denominator * 2)
    if quotient >= 0x8000:
        raise ValueError("quotient exceeds the captured 15-bit division domain")
    return (remainder << 15) | quotient


class RFDivisionTest(unittest.TestCase):
    def test_captured_normalization_operands(self):
        # Independent signed integers, not the core's masked-accumulator helpers.
        energy = 0x0d047a80
        correlation = -0x14fc0440
        exponent = 30 - (energy.bit_length() - 1)
        self.assertEqual(exponent, 3)
        energy_high = ((energy << exponent) >> 9) >> 16
        correlation_high = ((correlation << exponent) >> 9) >> 16
        self.assertEqual((energy_high, correlation_high), (52, -84))
        denominator = energy_high ** 2
        numerator = (correlation_high ** 2) << 10
        self.assertEqual(denominator, 0x0a90)
        self.assertEqual(numerator, 0x6e4000)
        self.assertEqual(division_tail(numerator, denominator), 0x800538)

    def test_current_captured_normalization_tail(self):
        # 4791 A=6e40, B=0; ADD B then SFTA A,8 before RPT #14/SUBC.
        self.assertEqual(division_tail(0x6e40 << 8, 0x0a90), 0x800538)

    def test_zero_and_exact_quotient(self):
        self.assertEqual(division_tail(0, 17), 0)
        self.assertEqual(division_tail(34 * 123, 17), 123)

    def test_remainder_encoding(self):
        self.assertEqual(division_tail(34 * 123 + 9, 17), (9 << 15) | 123)

    def test_rejects_outside_observed_unsigned_domain(self):
        for numerator, denominator in ((-1, 17), (1, 0), (1, -1), (0x10000, 1)):
            with self.subTest(numerator=numerator, denominator=denominator):
                with self.assertRaises(ValueError):
                    division_tail(numerator, denominator)


if __name__ == "__main__":
    unittest.main()
