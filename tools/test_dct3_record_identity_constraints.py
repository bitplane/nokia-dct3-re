import random
import struct
import unittest

from tools.dct3_record_identity_constraints import inverse_coefficients, symbolic_inverse, SCHEDULE
from tools.nse5_transform_trace_check import inverse_transform_words

try:
    import z3
except ImportError:
    z3 = None


class IdentityConstraintsTests(unittest.TestCase):
    def test_nonlinear_inverse_polynomial(self):
        coefficients = inverse_coefficients()
        for value in range(8):
            encoded = sum((((value >> i) & 1) ^
                           (((value >> ((i + 1) % 3)) & 1) |
                            (((value >> ((i + 2) % 3)) & 1) ^ 1))) << i
                          for i in range(3))
            output = 0
            for bit, polynomial in enumerate(coefficients):
                recovered = 0
                for mask, active in enumerate(polynomial):
                    if active:
                        recovered ^= int((encoded & mask) == mask)
                output |= recovered << bit
            self.assertEqual(output, value)

    @unittest.skipIf(z3 is None, "optional z3-solver experiment")
    def test_symbolic_codec_against_native_word_model(self):
        rng = random.Random(8250)
        for _ in range(12):
            encoded = rng.randbytes(12)
            table = rng.randbytes(12)
            actual = bytes(z3.simplify(value).as_long()
                           for value in symbolic_inverse(z3, encoded, table))
            expected = struct.pack(">6H", *inverse_transform_words(
                struct.unpack(">6H", encoded), struct.unpack(">6H", table),
                tuple(value * 257 for value in SCHEDULE)))
            self.assertEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
