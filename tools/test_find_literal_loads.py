import unittest

from tools.find_literal_loads import FLASH_BASE, literal_loads


class LiteralLoadsTest(unittest.TestCase):
    def test_big_endian_instruction_and_literal(self):
        image = bytes.fromhex("4a0000000016702c")
        self.assertEqual(list(literal_loads(image, 0x16702c, encoding="big")),
                         [(FLASH_BASE, 2, 0, FLASH_BASE + 4, 0x16702c)])

    def test_swap16_default_preserves_effective_literal(self):
        image = bytes.fromhex("004a000016002c70")
        self.assertEqual(list(literal_loads(image, 0x16702c)),
                         [(FLASH_BASE, 2, 0, FLASH_BASE + 4, 0x702c0016)])
        self.assertEqual(list(literal_loads(image, 0x702c0016, raw=True)),
                         list(literal_loads(image, 0x16702c)))

    def test_pc_alignment_at_second_halfword(self):
        image = bytes.fromhex("00004b0100000000000200b1")
        hits = list(literal_loads(image, 0x200b1, encoding="big"))
        self.assertEqual(hits, [(FLASH_BASE + 2, 3, 4, FLASH_BASE + 8, 0x200b1)])

    def test_wrong_encoding_is_not_a_reference(self):
        self.assertEqual(list(literal_loads(bytes.fromhex("4a0000000016702c"),
                                           0x16702c)), [])

    def test_truncated_pool_and_excluded_instruction(self):
        image = bytes.fromhex("4a0000000016702c")
        self.assertEqual(list(literal_loads(image[:-1], 0x16702c, encoding="big")), [])
        self.assertEqual(list(literal_loads(image, 0x16702c,
                                           start=FLASH_BASE + 1, encoding="big")), [])

    def test_non_literal_instruction_and_invalid_encoding(self):
        self.assertEqual(list(literal_loads(bytes.fromhex("680000000016702c"),
                                           0x16702c, encoding="big")), [])
        with self.assertRaisesRegex(ValueError, "encoding"):
            list(literal_loads(b"", 0, encoding="unknown"))


if __name__ == "__main__":
    unittest.main()
