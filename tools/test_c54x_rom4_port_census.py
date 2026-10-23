import unittest

from tools.c54x_rom4_port_census import census, direct_callers, caller_contexts, classify_candidates


def words(*values):
    return b"".join(value.to_bytes(2, "big") for value in values)


class C54xRom4PortCensusTest(unittest.TestCase):
    def test_linear_census_rejects_call_operand_and_keeps_io(self):
        image=words(0xf274,0x74fa,0x771a,0x000a,0x7482,0x0036,0xfc00)
        source=('00:0000: F274 74FA CALLD $0074FA\n'
                '00:0002: 771A 000A STM #$000A, $1A\n'
                '00:0004: 7482 0036 PORTR $0036, *AR2\n'
                '00:0006: FC00 RET\n')
        rows=classify_candidates(image,[source])
        self.assertEqual([(row['address'],row['classification']) for row in rows],
                         [('0001','operand_word'),('0004','linear_io')])
        self.assertEqual(classify_candidates(image,[source],{0x36})[0]['address'],'0004')

    def test_conflicting_stale_and_missing_decode_are_explicit(self):
        image=words(0xf274,0x74fa,0x000a,0x000b,0xfc00)
        parent='00:0000: F274 74FA CALLD $0074FA\n00:0002: 000A ADD $0A,A\n'
        competing='00:0001: 74FA 000A 000B PORTR $000B, *($000A)\n00:0004: FC00 RET\n'
        self.assertEqual(classify_candidates(image,[])[0]['classification'],'uncovered')
        self.assertEqual(classify_candidates(image,[parent,competing])[0]['classification'],'conflicting')
        stale=parent.replace('F274','F074')
        self.assertEqual(classify_candidates(image,[stale])[0]['classification'],'stale_disassembly')

    def test_all_absolute_aliases_put_address_before_port(self):
        for family, direction in ((0x7400, "R"), (0x7500, "W")):
            for mode in range(0xf8, 0x100):
                with self.subTest(direction=direction, mode=mode):
                    self.assertEqual(census(words(family | mode, 0x0008, 0x0036)),
                                     {(direction, 0x36): [0]})
                    self.assertEqual(census(words(family | mode, 0x0008)), {})

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
