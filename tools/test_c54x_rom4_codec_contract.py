import unittest

from tools.c54x_rom4_codec_contract import SEQUENCES, check


class CodecContractTests(unittest.TestCase):
    def image(self):
        image = bytearray(0x10000 * 2)
        for address, words in SEQUENCES.values():
            for offset, word in enumerate(words):
                start = 2 * (address + offset)
                image[start:start + 2] = word.to_bytes(2, "big")
        return image

    def test_reviewed_sequences(self):
        self.assertEqual(check(self.image()), list(SEQUENCES))

    def test_every_word_is_checked(self):
        for name, (address, words) in SEQUENCES.items():
            for offset in range(len(words)):
                with self.subTest(sequence=name, offset=offset):
                    image = self.image()
                    image[2 * (address + offset)] ^= 1
                    with self.assertRaisesRegex(ValueError, name):
                        check(image)

    def test_truncation(self):
        with self.assertRaises(ValueError):
            check(self.image()[:0x100])

    def test_partial_word(self):
        with self.assertRaisesRegex(ValueError, "complete"):
            check(b"\x00")


if __name__ == "__main__":
    unittest.main()
