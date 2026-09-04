import pathlib
import tempfile
import unittest

from tools.dsp_rom4_candidate_check import parse_drom, validate_program


class DspRom4CandidateCheckTest(unittest.TestCase):
    def test_program_requires_complete_nonuniform_word_image(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "dsp_full.bin"
            path.write_bytes(bytes(0x20000))
            with self.assertRaisesRegex(ValueError, "uniform"):
                validate_program(path)
            path.write_bytes(bytes(range(256)) * 512)
            self.assertEqual(validate_program(path)[0], 0x20000)

    def test_program_accepts_historical_export_without_word_ffff(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "dsp_full.bin"
            path.write_bytes((bytes(range(256)) * 512)[:-2])
            self.assertEqual(validate_program(path)[0], 0x1FFFE)

    def test_program_rejects_other_partial_sizes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "dsp_full.bin"
            path.write_bytes((bytes(range(256)) * 512)[:-4])
            with self.assertRaisesRegex(ValueError, "historical"):
                validate_program(path)

    def test_drom_requires_complete_unique_b000_efff_map(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "dsp_drom.txt"
            path.write_text(
                "".join(
                    f"{address:04x}: {(address ^ 0x55aa):04x}\n"
                    for address in range(0xB000, 0xF000)
                ),
                encoding="ascii",
            )
            words, digest = parse_drom(path)
        self.assertEqual(len(words), 0x4000)
        self.assertEqual(len(digest), 64)

    def test_drom_rejects_partial_map(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "dsp_drom.txt"
            path.write_text("b000: 1234\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "expected complete"):
                parse_drom(path)


if __name__ == "__main__":
    unittest.main()
