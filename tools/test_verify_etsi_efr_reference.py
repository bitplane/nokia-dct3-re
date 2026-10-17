from pathlib import Path
import tempfile
import unittest
import struct
from unittest.mock import patch

from tools.verify_etsi_efr_reference import check_vectors, digest, validate_members
from tools.verify_etsi_efr_reference import dtx_input, inspect_dtx, DTX_FRAMES
from tools.verify_etsi_efr_reference import erasure_input, inspect_erasures, ERASURE_BURSTS
from tools.verify_etsi_efr_reference import homing_conversation, inspect_homing, HOMING_FRAMES
from tools.verify_etsi_efr_reference import encoder_homing_input, inspect_encoder_homing
from tools.verify_etsi_efr_reference import homing_error_input, inspect_homing_errors


class EtsiEfrReferenceTest(unittest.TestCase):
    def test_bad_homing_changes_only_declared_bfi(self):
        frame = struct.pack('<247H', *([0] + [1] * 244 + [0, 1]))
        source = frame * 49
        result = homing_error_input(source)
        for index in range(49):
            self.assertEqual(result[index * 494], int(index in (0, 23, 46)))
            self.assertEqual(result[index * 494 + 1:(index + 1) * 494],
                             source[index * 494 + 1:(index + 1) * 494])
        changed = bytearray(source)
        changed[24 * 494 + 2] = 0
        for invalid in (source[:-1], b'\x01\x00' + source[2:], changed):
            with self.assertRaises(ValueError):
                homing_error_input(invalid)

    def test_homing_error_coverage_requires_ehf_and_repeated_speech(self):
        frame = struct.pack('<247H', *([0] * 247))
        source = homing_error_input(frame * 49)
        pcm = bytearray(b'\x01\x00' * (49 * 160))
        for index in (2, 25, 48):
            pcm[index * 320:(index + 1) * 320] = b'\x08\x00' * 160
        self.assertEqual(inspect_homing_errors(source, pcm)['bad_homing_frames'], [0, 23, 46])
        changed = bytearray(pcm)
        changed[26 * 320] = 2
        for data in (pcm[:-1], bytes(len(pcm)), changed):
            with self.assertRaises(ValueError):
                inspect_homing_errors(source, data)

    def test_encoder_homing_input_preserves_repeated_speech(self):
        speech = bytes(i % 251 for i in range(52352))
        result = encoder_homing_input(speech)
        self.assertEqual(len(result), 46 * 320)
        for start in (0, 22, 44):
            self.assertEqual(result[start * 320:(start + 2) * 320], b'\x08\x00' * 320)
        for start in (2, 24):
            self.assertEqual(result[start * 320:(start + 20) * 320], speech[:20 * 320])
        with self.assertRaises(ValueError):
            encoder_homing_input(speech[:-1])

    def test_encoder_homing_requires_reproducible_nondegenerate_speech(self):
        canonical = struct.pack('<246H', *([0] * 244 + [1, 1]))
        speech = struct.pack('<246H', *([1] * 246)) * 163
        result = homing_conversation(canonical, speech)
        self.assertEqual(inspect_encoder_homing(result)['repeated_speech_frames'], 20)
        mutated = bytearray(result)
        mutated[24 * 492] = 0
        for data in (result[:-1], b'\x02\x00' + result[2:], mutated,
                     canonical * 46, result[:23 * 492] + speech[:492] + result[24 * 492:]):
            with self.assertRaises(ValueError):
                inspect_encoder_homing(data)

    def test_homing_conversation_preserves_encoded_speech(self):
        canonical = struct.pack('<246H', *([0] * 244 + [1, 1]))
        speech = bytes(i % 251 for i in range(80196))
        output = homing_conversation(canonical, speech)
        self.assertEqual(len(output), HOMING_FRAMES * 492)
        for start in (0, 22, 44):
            self.assertEqual(output[start * 492:(start + 2) * 492], canonical * 2)
        for start in (2, 24):
            self.assertEqual(output[start * 492:(start + 20) * 492], speech[:20 * 492])
        for first, second in ((canonical[:-1], speech), (canonical, speech[:-1]),
                              (b'\x02\x00' + canonical[2:], speech)):
            with self.assertRaises(ValueError):
                homing_conversation(first, second)

    def test_homing_requires_reset_output_and_repeated_nontrivial_speech(self):
        decoder = bytes(HOMING_FRAMES * 494)
        pattern = struct.pack('<160h', *([8] * 160))
        pcm = bytearray(pattern * HOMING_FRAMES)
        speech = bytes([1, 0]) * (20 * 160)
        pcm[2 * 320:22 * 320] = speech
        pcm[24 * 320:44 * 320] = speech
        self.assertEqual(inspect_homing(decoder, pcm)['repeated_speech_frames'], 20)
        changed = bytearray(pcm)
        changed[24 * 320] = 2
        for first, second in ((decoder[:-1], pcm), (decoder, pcm[:-1]),
                              (b'\x01\x00' + decoder[2:], pcm),
                              (decoder, bytes(len(pcm))), (decoder, pattern * HOMING_FRAMES),
                              (decoder, changed), (decoder, b'\x00' + pcm[1:])):
            with self.assertRaises(ValueError):
                inspect_homing(first, second)

    def test_erasure_input_changes_only_scheduled_bfi(self):
        original = b''.join(struct.pack('<247H', *([0] + [i % 2] * 244 + [0, i % 2]))
                            for i in range(163))
        generated = erasure_input(original)
        expected = {f for start, count in ERASURE_BURSTS for f in range(start, start + count)}
        self.assertEqual(len(expected), 28)
        for i in range(163):
            self.assertEqual(generated[i * 494], int(i in expected))
            self.assertEqual(generated[i * 494 + 1:(i + 1) * 494], original[i * 494 + 1:(i + 1) * 494])
        for bad in (original[:-1], b'\x01\x00' + original[2:],
                    original[:490] + b'\x02\x00' + original[492:]):
            with self.assertRaises(ValueError):
                erasure_input(bad)

    def test_erasure_coverage_requires_nontrivial_concealment(self):
        source = struct.pack('<247H', *([0] * 247)) * 163
        decoder = erasure_input(source)
        clean = bytes(163 * 320)
        pcm = bytearray(clean)
        for start, count in ERASURE_BURSTS:
            pcm[start * 320] = 1
        self.assertEqual(inspect_erasures(decoder, pcm, clean)['changed_frames'], 4)
        for values in ((source, pcm, clean), (decoder, clean, clean),
                       (decoder, pcm[:-1], clean), (decoder[:-1], pcm, clean),
                       (b'\x02\x00' + decoder[2:], pcm, clean)):
            with self.assertRaises(ValueError):
                inspect_erasures(*values)

    def test_dtx_input_preserves_speech_and_uses_deterministic_quiet_noise(self):
        speech = bytes((i % 251 for i in range(163 * 320)))
        generated = dtx_input(speech + bytes(192))
        self.assertEqual(generated, dtx_input(speech + bytes([255]) * 192))
        self.assertEqual(generated[96 * 320:259 * 320], speech)
        self.assertEqual(generated[355 * 320:], speech)
        noise = generated[:96 * 320] + generated[259 * 320:355 * 320]
        self.assertTrue(any(noise))
        self.assertTrue(all(-64 <= x <= 56 and x % 8 == 0 for (x,) in struct.iter_unpack('<h', noise)))
        self.assertEqual(len(generated), DTX_FRAMES * 320)
        with self.assertRaises(ValueError):
            dtx_input(speech)

    def test_dtx_coverage_requires_real_transitions_and_valid_flags(self):
        coded, decoder = [], []
        for i in range(DTX_FRAMES):
            speech = int(96 <= i < 259 or i >= 355)
            coded.append(struct.pack('<246H', *([0] * 244 + [speech, speech])))
            decoder.append(struct.pack('<247H', *([int(not speech)] + [0] * 244 + [0 if speech else 2, int(i % 24 == 0)])))
        cod, dec, pcm = b''.join(coded), b''.join(decoder), b'\x01\x00' + bytes(DTX_FRAMES * 320 - 2)
        coverage = inspect_dtx(cod, dec, pcm)
        self.assertEqual(coverage['speech_resumptions'], 2)
        self.assertEqual(coverage['inactive_frames'], 192)
        self.assertEqual(coverage['nonzero_cni_frames'], 1)
        for values in ((cod[:-1], dec, pcm), (bytes(len(cod)), dec, pcm),
                       (cod, b'\x02\x00' + dec[2:], pcm), (cod, dec, pcm[:-2]),
                       (b'\x02\x00' + cod[2:], dec, pcm)):
            with self.subTest(lengths=list(map(len, values))), self.assertRaises(ValueError):
                inspect_dtx(*values)
        with self.assertRaises(ValueError):
            inspect_dtx(cod, dec, bytes(len(pcm)))

    def test_flat_archive_only(self):
        validate_members(["CODER.C", "SPCH_DOS.INP"])
        for names in (["../coder.c"], ["/coder.c"], ["sub/coder.c"],
                      ["sub\\coder.c"], ["."], [""], ["CODER.C", "coder.c"]):
            with self.subTest(names=names), self.assertRaises(ValueError):
                validate_members(names)

    def test_output_requires_exact_reference_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            expected = {suffix: (len(suffix), digest(suffix.encode()))
                        for suffix in ("cod", "dec", "out")}
            for suffix in expected:
                (root / f"spch_dos.{suffix}").write_bytes(suffix.encode())
                (root / f"verified.{suffix}").write_bytes(suffix.encode())
            with patch("tools.verify_etsi_efr_reference.EXPECTED", expected):
                self.assertEqual(set(check_vectors(root)), set(expected))
                (root / "verified.cod").write_bytes(b"bad")
                with self.assertRaisesRegex(ValueError, "not bit-exact"):
                    check_vectors(root)

    def test_changed_reference_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "spch_dos.cod").write_bytes(b"bad")
            (root / "verified.cod").write_bytes(b"bad")
            with self.assertRaisesRegex(ValueError, "verification input differs"):
                check_vectors(root)


if __name__ == "__main__":
    unittest.main()
