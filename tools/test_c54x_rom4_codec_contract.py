import unittest

from tools.c54x_rom4_codec_contract import SEQUENCES, check, check_trace, check_restore, check_tone


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
        events = [("data", "write", "0022", "c008", "0e22"),
                  ("data", "write", "0022", "c0c8", "0e24"),
                  ("data", "write", "0021", "0aaa", "0e31"),
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
            check_trace(self.trace().replace("address=0021 value=0aaa", "address=0021 value=0000"))

    def test_incomplete_cycles_rejected(self):
        with self.assertRaisesRegex(ValueError, "I/O control"):
            check_trace("\n".join(self.trace().splitlines()[:-1]))

    def test_lua_error_rejected(self):
        with self.assertRaisesRegex(ValueError, "Lua observation"):
            check_trace(self.trace() + "\nLua error")

class CodecRestoreTests(unittest.TestCase):
    def trace(self):
        state = "t=7.000000000 pc=30bf st0=0000 st1=2900 sp=1000 io21=0482 bspc22=c008"
        return (f"rom4_codec_state: phase=saved {state}\n"
                f"rom4_codec_state: phase=restored {state}\n"
                "state_roundtrip: result=pass")

    def test_exact_restoration(self):
        check_restore(self.trace())

    def test_every_field_mismatch_rejected(self):
        for field in ("t=7.000000000", "pc=30bf", "st0=0000", "st1=2900",
                      "sp=1000", "io21=0482", "bspc22=c008"):
            with self.subTest(field=field):
                changed = self.trace().splitlines()
                changed[1] = changed[1].replace(field, field[:-1] + "1")
                with self.assertRaisesRegex(ValueError, "exactly"):
                    check_restore("\n".join(changed))

    def test_missing_snapshot_rejected(self):
        with self.assertRaisesRegex(ValueError, "snapshot pair"):
            check_restore("\n".join(self.trace().splitlines()[1:]))

    def test_failed_harness_rejected(self):
        with self.assertRaisesRegex(ValueError, "harness failed"):
            check_restore(self.trace().replace("result=pass", "result=fail"))


class NativeToneTests(unittest.TestCase):
    def trace(self):
        return CodecTraceTests().trace() + "\n" + "\n".join([
            "input-press: t=8.0 name=1 port=1f",
            "rom4_tone_access: owner=mcu direction=write address=0100ac value=e10000 mask=ffff0000 pc=272034 t=8.07",
            "rom4_tone_access: owner=dsp direction=read address=000856 value=00e1 mask=ffff pc=00a59a t=8.08",
            "rom4_tone_access: owner=dsp direction=write address=0000fe value=00e1 mask=ffff pc=00a5de t=8.09",
            "input-release: t=8.22 name=1 port=1d",
            "rom4_tone_summary: tx_words=1 rx_reads=1 tone_reads=29 tone_copies=7 t=11.01",
        ])

    def test_organic_tone_boundary(self):
        check_tone(self.trace())

    def test_missing_command_rejected(self):
        with self.assertRaisesRegex(ValueError, "organic tone"):
            check_tone(self.trace().replace("value=e10000", "value=0000"))

    def test_early_boot_copy_does_not_prove_key_response(self):
        with self.assertRaisesRegex(ValueError, "organic tone"):
            check_tone(self.trace().replace("t=8.09", "t=1.59"))

    def test_new_audio_activity_requires_review(self):
        with self.assertRaisesRegex(ValueError, "boundary changed"):
            check_tone(self.trace().replace("tx_words=1", "tx_words=2"))

    def test_missing_uncapped_counts_rejected(self):
        with self.assertRaisesRegex(ValueError, "uncapped"):
            check_tone("\n".join(self.trace().splitlines()[:-1]))


if __name__ == "__main__":
    unittest.main()
