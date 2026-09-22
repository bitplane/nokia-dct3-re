import unittest

from tools.c54x_rom4_port_census import census


def words(*values):
    return b"".join(value.to_bytes(2, "big") for value in values)


class C54xRom4PortCensusTest(unittest.TestCase):
    def test_indirect_and_absolute_port_operands(self):
        result = census(words(
            0x7492, 0x0039,
            0x74f8, 0x0008, 0x0038,
            0x7593, 0x0031,
            0x75f8, 0x0008, 0x0032,
        ))
        self.assertEqual(result[("R", 0x39)], [0])
        self.assertEqual(result[("R", 0x38)], [2])
        self.assertEqual(result[("W", 0x31)], [5])
        self.assertEqual(result[("W", 0x32)], [7])
        self.assertNotIn(("R", 0x08), result)
        self.assertNotIn(("W", 0x08), result)

    def test_rejects_partial_word(self):
        with self.assertRaisesRegex(ValueError, "complete 16-bit words"):
            census(b"\x74")


if __name__ == "__main__":
    unittest.main()
