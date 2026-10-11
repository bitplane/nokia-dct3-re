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
    def test_captured_energy_producer_pipeline(self):
        # SUB primes A, then SQDST squares the previous A before loading
        # the next difference. The ninth read is not a ninth square.
        differences = [-340, -1708, -1448, -968, -1708, 340, -968, 1448, 340]
        partials = [sum(value * value for value in differences[:count])
                    for count in range(1, 9)]
        self.assertEqual(partials, [0x1c390, 0x2e4720, 0x4e4560, 0x5c91a0,
                                    0x891530, 0x8ad8c0, 0x992500, 0xb92340])
        self.assertEqual(differences[-1] << 16, 0x01540000)
        self.assertNotEqual(sum(value * value for value in differences), partials[-1])

    def test_captured_window_accounting(self):
        # Energy is seeded from a separate queue before 17 adjacent-pair
        # comparisons. It is not the squared magnitude of the stored pair.
        queued_energy = 0x00b92340
        pair_dot = -(2528 ** 2 + 3784 ** 2)
        self.assertEqual(18 * queued_energy, 0x0d047a80)
        self.assertEqual(17 * pair_dot, -0x14fc0440)
        self.assertNotEqual(queued_energy, -pair_dot)

    def test_captured_correlation_sum(self):
        # 17 observed helper returns precede the first normalization entry.
        pair_energy = 0x09e0 ** 2 + 0x0ec8 ** 2
        self.assertEqual(pair_energy, 0x13c0040)
        self.assertEqual(-17 * pair_energy, -0x14fc0440)

    def test_captured_adjacent_pair_products(self):
        previous_x, previous_y = -2528, -3784
        current_x, current_y = 2528, 3784
        dot = previous_x * current_x + previous_y * current_y
        cross = previous_y * current_x - previous_x * current_y
        self.assertEqual(dot, -0x13c0040)
        self.assertEqual(cross, 0)

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
