import unittest
import tempfile
from pathlib import Path

from tools.run_c54x_rom4_crc import COMPLETE, PASS, PACKED_COMPLETE, PACKED_PASS, FR_COMPLETE, FR_PASS, check_result
from tools.run_c54x_rom4_crc import DECODER_PASS, DECODER_COMPLETE, DECODER_COMPARISON
from tools.run_c54x_rom4_crc import ENCODER_PASS, ENCODER_COMPLETE, ENCODER_COMPARISON
from tools.run_c54x_rom4_crc import PCM_MUTE_PASS
from tools.run_c54x_rom4_crc import DECODER_REPLAY, ENCODER_REPLAY
from tools.run_c54x_rom4_crc import file_identity
from tools.run_c54x_rom4_crc import EFR_CRC_PASS, EFR_CRC_COMPLETE
from tools.run_c54x_rom4_crc import EFR_PACKING_PASS, EFR_PACKING_COMPLETE
from tools.run_c54x_rom4_crc import EFR_DECODER_PASS, EFR_DECODER_COMPLETE
from tools.run_c54x_rom4_crc import DELAYED_REPEAT_PASS
from tools.run_c54x_rom4_crc import EFR_DECODER_REPLAY
from tools.run_c54x_rom4_crc import EFR_ENCODER_PASS, EFR_ENCODER_COMPLETE, EFR_ENCODER_REPLAY
from tools.run_c54x_rom4_crc import EFR_DTX_DECODER_PASS, EFR_DTX_DECODER_COMPLETE, EFR_DTX_DECODER_REPLAY
from tools.run_c54x_rom4_crc import EFR_DTX_ENCODER_PASS, EFR_DTX_ENCODER_COMPLETE, EFR_DTX_ENCODER_REPLAY
from tools.run_c54x_rom4_crc import EFR_ERASURE_PASS, EFR_ERASURE_COMPLETE, EFR_ERASURE_REPLAY
from tools.run_c54x_rom4_crc import EFR_HOMING_PASS, EFR_HOMING_COMPLETE, EFR_HOMING_REPLAY
from tools.run_c54x_rom4_crc import EFR_ENCODER_HOMING_PASS, EFR_ENCODER_HOMING_COMPLETE, EFR_ENCODER_HOMING_REPLAY


class Rom4HelperResultTest(unittest.TestCase):
    def test_efr_encoder_homing_requires_every_result_and_replay(self):
        frames = "\n".join(f"ROM4 EFR homing encoder comparison: frame={frame} mismatches=0" for frame in range(46))
        text = "\n".join((frames, EFR_ENCODER_HOMING_REPLAY, EFR_ENCODER_HOMING_PASS, EFR_ENCODER_HOMING_COMPLETE))
        check_result(text, 3, efr_homing_encoder=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_homing_encoder=True)
        for invalid in (text.replace("frame=45 mismatches=0", "frame=45 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace(EFR_ENCODER_HOMING_REPLAY, ""), text.replace("homing=6", "homing=0"),
                        text.replace("parameters=2622", "parameters=0"),
                        text.replace("packed_words=782", "packed_words=0"),
                        text.replace("repeated_speech=20", "repeated_speech=0"),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_homing_encoder=True)

    def test_efr_homing_decoder_requires_every_frame_and_replay(self):
        frames = "\n".join(f"ROM4 EFR homing decoder comparison: frame={frame} mismatches=0" for frame in range(46))
        text = "\n".join((frames, EFR_HOMING_REPLAY, EFR_HOMING_PASS, EFR_HOMING_COMPLETE))
        check_result(text, 3, efr_homing_decoder=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_homing_decoder=True)
        for invalid in (text.replace("frame=45 mismatches=0", "frame=45 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace(EFR_HOMING_REPLAY, ""), text.replace("homing=6", "homing=0"),
                        text.replace("steady_ehf=4", "steady_ehf=0"),
                        text.replace("repeated_speech=20", "repeated_speech=0"),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_homing_decoder=True)

    def test_efr_erasure_decoder_completion(self):
        frames = "\n".join(f"ROM4 EFR erasure decoder comparison: frame={frame} mismatches=0" for frame in range(163))
        text = "\n".join((frames, EFR_ERASURE_REPLAY, EFR_ERASURE_PASS, EFR_ERASURE_COMPLETE))
        check_result(text, 3, efr_erasure_decoder=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_erasure_decoder=True)
        for invalid in (text.replace("frame=9 mismatches=0", "frame=9 mismatches=127"),
                        text.replace("frame=162 mismatches=0", "frame=0 mismatches=0"),
                        text.replace(EFR_ERASURE_REPLAY, ""), text.replace("erased=28", "erased=0"),
                        text.replace("bursts=1,3,8,16", "bursts=1"),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_erasure_decoder=True)

    def test_efr_dtx_encoder_completion(self):
        frames = "\n".join(f"ROM4 EFR DTX encoder comparison: frame={frame} mismatches=0" for frame in range(518))
        text = "\n".join((frames, EFR_DTX_ENCODER_REPLAY, EFR_DTX_ENCODER_PASS, EFR_DTX_ENCODER_COMPLETE))
        check_result(text, 3, efr_dtx_encoder=True)
        for invalid in (text.replace("frame=517 mismatches=0", "frame=517 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace("flags=1036", "flags=0"), text.replace("packed_words=8806", "packed_words=0"),
                        text.replace(EFR_DTX_ENCODER_REPLAY, ""),
                        text.replace("ram_words=65440", "ram_words=160"), text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_dtx_encoder=True)

    def test_efr_dtx_decoder_completion(self):
        frames = "\n".join(f"ROM4 EFR DTX decoder comparison: frame={frame} mismatches=0" for frame in range(518))
        text = "\n".join((frames, EFR_DTX_DECODER_REPLAY, EFR_DTX_DECODER_PASS, EFR_DTX_DECODER_COMPLETE))
        check_result(text, 3, efr_dtx_decoder=True)
        check_result(text + "\nAverage speed: 1234.56% (20 seconds)", 3, efr_dtx_decoder=True)
        for invalid in (text.replace("frame=517 mismatches=0", "frame=517 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace("erased=135", "erased=0"), text.replace("sid=8", "sid=0"),
                        text.replace("nonzero_cni_frames=46", "nonzero_cni_frames=0"),
                        text.replace(EFR_DTX_DECODER_REPLAY, ""),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_dtx_decoder=True)

    def test_efr_encoder_completion(self):
        frames = "\n".join(f"ROM4 EFR encoder comparison: frame={frame} mismatches=0" for frame in range(163))
        text = "\n".join((frames, EFR_ENCODER_REPLAY, EFR_ENCODER_PASS, EFR_ENCODER_COMPLETE))
        check_result(text, 3, efr_encoder=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_encoder=True)
        for invalid in (text.replace("frame=162 mismatches=0", "frame=162 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace(EFR_ENCODER_REPLAY, ""),
                        text.replace(EFR_ENCODER_REPLAY, EFR_ENCODER_REPLAY + "\n" + EFR_ENCODER_REPLAY),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text.replace("flags=326", "flags=0"),
                        text.replace("parameters=9291", "parameters=0"),
                        text.replace("packed_words=2771", "packed_words=0"),
                        text.replace(EFR_ENCODER_PASS, EFR_DECODER_PASS),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_encoder=True)
        with self.assertRaises(ValueError):
            check_result(text, 0, efr_encoder=True)

    def test_efr_decoder_completion(self):
        frames = "\n".join(f"ROM4 EFR decoder comparison: frame={frame} mismatches=0" for frame in range(163))
        text = DELAYED_REPEAT_PASS + "\n" + frames + "\n" + EFR_DECODER_REPLAY + "\n" + EFR_DECODER_PASS + "\n" + EFR_DECODER_COMPLETE
        check_result(text, 3, efr_decoder=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_decoder=True)
        for invalid in (text.replace("frame=162 mismatches=0", "frame=162 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace(EFR_DECODER_PASS, EFR_PACKING_PASS),
                        text.replace(DELAYED_REPEAT_PASS, ""),
                        text.replace(EFR_DECODER_REPLAY, ""),
                        text + "\nunexpected trailer",
                        text + "\nAverage speed: 1234.56% (6 seconds)\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_decoder=True)

    def test_efr_packing_completion(self):
        text = EFR_PACKING_PASS + "\n" + EFR_PACKING_COMPLETE
        check_result(text, 3, efr_packing=True)
        for invalid in (text.replace("corrupt_frames=277", "corrupt_frames=0"),
                        text.replace(EFR_PACKING_PASS, EFR_CRC_PASS),
                        text.replace(EFR_PACKING_PASS, EFR_PACKING_PASS + "\n" + EFR_PACKING_PASS)):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_packing=True)

    def test_efr_crc_completion(self):
        text = EFR_CRC_PASS + "\n" + EFR_CRC_COMPLETE
        check_result(text, 3, efr_crc=True)
        for invalid in (text.replace("cases=554", "cases=0"),
                        text.replace(EFR_CRC_PASS, EFR_CRC_PASS + "\n" + EFR_CRC_PASS),
                        text.replace(EFR_CRC_PASS, FR_PASS)):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_crc=True)

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
