import unittest

from tools.c54x_rom4_port_census import census, direct_callers


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

    def test_direct_callers_include_delayed_calls_only_to_target(self):
        result = direct_callers(words(
            0xf074, 0x7b0a,
            0xf274, 0x7b0a,
            0xf073, 0x7b0a,
            0xf274, 0x410e,
        ), 0x7b0a)
        self.assertEqual(result, [(0, False), (2, True)])


if __name__ == "__main__":
    unittest.main()
