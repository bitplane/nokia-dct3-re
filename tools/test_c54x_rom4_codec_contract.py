import unittest

from tools.c54x_rom4_codec_contract import SEQUENCES, check, check_trace


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

    def test_unreviewed_reader_rejected(self):
        image = self.image()
        image[0x200:0x206] = bytes.fromhex("74f800080021")
        with self.assertRaisesRegex(ValueError, "reader census"):
            check(image)

    def test_unreviewed_writer_rejected(self):
        image = self.image()
        image[0x200:0x206] = bytes.fromhex("75f800080021")
        with self.assertRaisesRegex(ValueError, "writer census"):
            check(image)


class CodecTraceTests(unittest.TestCase):
    def trace(self):
        events = [("data", "write", "0021", "0aaa", "0e31"),
                  ("data", "read", "0020", "0aaa", "0e5d"),
                  ("io", "write", "0021", "1482", "4555"),
                  ("io", "write", "0021", "1482", "4559"),
                  ("io", "write", "0021", "0482", "455d")]
        events += [("io", "read", "0021", "0482", "3221"),
                   ("io", "write", "0021", "0c82", "3228"),
                   ("io", "read", "0021", "0c82", "33f6"),
                   ("io", "write", "0021", "0482", "33fd")] * 3
        return "\n".join(
            f"rom4_serial_audit: space={space} direction={direction} address={address} "
            f"value={value} mask=ffff pc={pc} t=0.2"
            for space, direction, address, value, pc in events)

    def test_separate_paths(self):
        check_trace(self.trace())

    def test_stale_echo_read_rejected(self):
        with self.assertRaisesRegex(ValueError, "I/O control"):
            check_trace(self.trace().replace("value=0482 mask=ffff pc=3221",
                                             "value=0aaa mask=ffff pc=3221"))

    def test_missing_echo_rejected(self):
        with self.assertRaisesRegex(ValueError, "boot echo"):
            check_trace("\n".join(self.trace().splitlines()[1:]))

    def test_incomplete_cycles_rejected(self):
        with self.assertRaisesRegex(ValueError, "I/O control"):
            check_trace("\n".join(self.trace().splitlines()[:-1]))

    def test_lua_error_rejected(self):
        with self.assertRaisesRegex(ValueError, "Lua observation"):
            check_trace(self.trace() + "\nLua error")

if __name__ == "__main__":
    unittest.main()
