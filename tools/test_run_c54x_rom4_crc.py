import unittest
import tempfile
from pathlib import Path

from tools.run_c54x_rom4_crc import COMPLETE, PASS, PACKED_COMPLETE, PACKED_PASS, FR_COMPLETE, FR_PASS, check_result
from tools.run_c54x_rom4_crc import DECODER_PASS, DECODER_COMPLETE, DECODER_COMPARISON
from tools.run_c54x_rom4_crc import ENCODER_PASS, ENCODER_COMPLETE, ENCODER_COMPARISON
from tools.run_c54x_rom4_crc import PCM_MUTE_PASS
from tools.run_c54x_rom4_crc import DECODER_REPLAY, ENCODER_REPLAY
from tools.run_c54x_rom4_crc import file_identity


class Rom4HelperResultTest(unittest.TestCase):
    def test_file_identity_tracks_actual_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input.bin"
            path.write_bytes(b"abc")
            identity = file_identity(path)
            self.assertEqual(identity["path"], str(path.resolve()))
            self.assertEqual(identity["bytes"], 3)
            self.assertEqual(identity["sha256"],
                             "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
            path.write_bytes(b"abcd")
            self.assertNotEqual(file_identity(path)["sha256"], identity["sha256"])

    def test_encoder_completion(self):
        text = "\n".join((ENCODER_COMPARISON, ENCODER_REPLAY, ENCODER_PASS, PCM_MUTE_PASS, ENCODER_COMPLETE))
        check_result(text, 3, fr_encoder=True)
        for invalid in (text.replace("mismatches=0", "mismatches=1"),
                        text.replace(ENCODER_COMPARISON, ""),
                        text.replace(ENCODER_REPLAY, ""),
                        text.replace(ENCODER_REPLAY, ENCODER_REPLAY + "\n" + ENCODER_REPLAY),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text.replace("reference_advance=once", "reference_advance=twice"),
                        text.replace(PCM_MUTE_PASS, ""),
                        text.replace(PCM_MUTE_PASS, PCM_MUTE_PASS + "\n" + PCM_MUTE_PASS),
                        text.replace(ENCODER_PASS, DECODER_PASS),
                        text.replace(ENCODER_PASS, ENCODER_PASS + "\n" + ENCODER_PASS)):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, fr_encoder=True)

    def test_decoder_completion(self):
        text = "\n".join((DECODER_COMPARISON, DECODER_REPLAY, DECODER_PASS, DECODER_COMPLETE))
        check_result(text, 3, fr_decoder=True)
        for invalid in (text.replace("mismatches=0", "mismatches=1"),
                        text.replace(DECODER_COMPARISON, ""),
                        text.replace(DECODER_REPLAY, ""),
                        text.replace(DECODER_REPLAY, DECODER_REPLAY + "\n" + DECODER_REPLAY),
                        text.replace("frames=32 ram_words", "frames=0 ram_words"),
                        text.replace("reference_advance=once", "reference_advance=twice"),
                        text.replace(DECODER_PASS, FR_PASS),
                        text.replace(DECODER_PASS, DECODER_PASS + "\n" + DECODER_PASS)):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, fr_decoder=True)

    def test_completion(self):
        check_result(PASS + "\n" + COMPLETE + "\n", 3)

    def test_packed_completion(self):
        check_result(PACKED_PASS + "\n" + PACKED_COMPLETE + "\n", 3, packed=True)
        with self.assertRaises(ValueError):
            check_result(PASS + "\n" + COMPLETE + "\n", 3, packed=True)
        with self.assertRaises(ValueError):
            check_result(PACKED_PASS + "\n" + PACKED_COMPLETE + "\n", 3)

    def test_fr_packing_completion(self):
        check_result(FR_PASS + "\n" + FR_COMPLETE + "\n", 3, fr_packing=True)
        with self.assertRaises(ValueError):
            check_result(PACKED_PASS + "\n" + PACKED_COMPLETE + "\n", 3, fr_packing=True)
        with self.assertRaises(ValueError):
            check_result(FR_PASS + "\n" + FR_COMPLETE + "\n", 3, packed=True)

    def test_reject_incomplete_or_ambiguous_results(self):
        for text, status in [(PASS, 3), (COMPLETE, 3),
                             (PASS + "\n" + COMPLETE, 0),
                             (PASS + "\n" + PASS + "\n" + COMPLETE, 3),
                             (PASS + "\n" + COMPLETE + "\nextra", 3),
                             ("WRONG CHECKSUMS\n" + PASS + "\n" + COMPLETE, 3),
                             ("Fatal error: failure\n" + PASS + "\n" + COMPLETE, 3)]:
            with self.subTest(text=text, status=status):
                with self.assertRaises(ValueError):
                    check_result(text, status)


if __name__ == "__main__":
    unittest.main()
