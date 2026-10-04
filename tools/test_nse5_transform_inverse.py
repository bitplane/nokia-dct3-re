import random
import unittest

from tools.nse5_transform_trace_check import (
    inverse_linear_mix, inverse_nonlinear_words, inverse_transform_words,
    linear_mix, nonlinear_words, transform_words,
)


class TransformInverseTests(unittest.TestCase):
    def test_native_observed_vectors(self):
        table = (0x6521, 0x4cda, 0x4d33, 0x3bc6, 0x3342, 0x2c9e)
        schedule = (0xd0d0, 0x1616, 0x2c2c, 0x5858, 0xb0b0, 0x7171,
                    0xe2e2, 0xd5d5, 0x5a5a, 0x6767, 0xcece, 0x8d8d)
        vectors = [
            ((0x8184, 0xb191, 0x1537, 0x5a67, 0x60ed, 0xee9d),
             (0xb689, 0xcabe, 0x4b3e, 0x1266, 0x2a76, 0xc732)),
            ((0x1dd9, 0x5eb8, 0xfe6b, 0xef3c, 0x1337, 0x819e),
             (0xf409, 0x1f8d, 0xdc07, 0x324f, 0x2ad0, 0x91d9)),
        ]
        for plain, encoded in vectors:
            self.assertEqual(inverse_transform_words(encoded, table, schedule), plain)

    def test_all_linear_basis_vectors(self):
        for bit in range(96):
            words = tuple(((1 << bit) >> (16 * index)) & 0xffff for index in range(6))
            self.assertEqual(inverse_linear_mix(linear_mix(words)), words)

    def test_acquired_rom4_msid_table_agrees_with_existing_decoder(self):
        # Data-ROM b6e5/b6f7, and the independently decoded native reply.
        table = (0x57f4, 0xb241, 0x27f4, 0xe4ea, 0x55cf, 0xfea0)
        schedule = (0xb1b1, 0x7373, 0xe6e6, 0x5a5a, 0xabab, 0x4747,
                    0x8e8e, 0x0d0d, 0x1a1a, 0x3434, 0x6868, 0x0b0b)
        encoded = (0x64b0, 0x00eb, 0x8f45, 0x7e16, 0x8bd2, 0xd32a)
        plain = (0x054b, 0x7d89, 0x0016, 0x0010, 0xa8a9, 0xaa60)
        self.assertEqual(inverse_transform_words(encoded, table, schedule), plain)
        self.assertEqual(transform_words(plain, table, schedule), encoded)

    def test_nonlinear_truth_table_in_each_word_group(self):
        for parity in range(2):
            for value in range(8):
                words = [0] * 6
                for index in range(3):
                    words[parity + index * 2] = 0xffff if value & (1 << index) else 0
                self.assertEqual(inverse_nonlinear_words(nonlinear_words(tuple(words))),
                                 tuple(words))

    def test_randomized_complete_round_trips(self):
        rng = random.Random(8250)
        for _ in range(100):
            words = tuple(rng.randrange(0x10000) for _ in range(6))
            table = tuple(rng.randrange(0x10000) for _ in range(6))
            schedule = tuple(rng.randrange(0x10000) for _ in range(12))
            self.assertEqual(inverse_transform_words(
                transform_words(words, table, schedule), table, schedule), words)

    def test_invalid_operands(self):
        for words, table, schedule in [((0,) * 5, (0,) * 6, (0,) * 12),
                                       ((-1,) * 6, (0,) * 6, (0,) * 12),
                                       ((0,) * 6, (0x10000,) * 6, (0,) * 12)]:
            with self.assertRaises(ValueError):
                inverse_transform_words(words, table, schedule)


if __name__ == "__main__":
    unittest.main()
