import unittest

from tools.c54x_rom4_port_census import census, direct_callers, caller_contexts


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

    def test_long_offset_port_precedes_address_extension(self):
        result = census(words(
            0x74ea, 0x0039, 0x0005,
            0x75ea, 0x0032, 0xfffb,
        ))
        self.assertEqual(result, {("R", 0x39): [0], ("W", 0x32): [3]})

    def test_missing_port_operand_is_not_a_candidate(self):
        self.assertEqual(census(words(0x74ea)), {})
        self.assertEqual(census(words(0x75f8, 0x0008)), {})

    def test_missing_long_offset_extension_is_not_a_candidate(self):
        self.assertEqual(census(words(0x74ea, 0x0039)), {})
        self.assertEqual(census(words(0x75ea, 0x0032)), {})
        self.assertEqual(census(words(0x7492, 0x0039)), {("R", 0x39): [0]})

    def test_direct_callers_include_delayed_calls_only_to_target(self):
        result = direct_callers(words(
            0xf074, 0x7b0a,
            0xf274, 0x7b0a,
            0xf073, 0x7b0a,
            0xf274, 0x410e,
        ), 0x7b0a)
        self.assertEqual(result, [(0, False), (2, True)])

    def test_context_preserves_big_endian_words_and_clamps_bounds(self):
        image = words(0xf074, 0x45c2, 0xe908, 0xf274, 0x45c2)
        self.assertEqual(caller_contexts(image, 0x45c2, 2), [
            (0, False, 0, [0xf074, 0x45c2, 0xe908, 0xf274]),
            (3, True, 1, [0x45c2, 0xe908, 0xf274, 0x45c2]),
        ])

    def test_context_rejects_negative_radius_and_partial_words(self):
        with self.assertRaisesRegex(ValueError, "nonnegative"):
            caller_contexts(b"", 0x45c2, -1)
        with self.assertRaisesRegex(ValueError, "complete 16-bit"):
            caller_contexts(b"\x00", 0x45c2, 1)


if __name__ == "__main__":
    unittest.main()
