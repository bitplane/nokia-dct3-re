from pathlib import Path
import tempfile
import unittest
import struct
from unittest.mock import patch

from tools.verify_etsi_efr_reference import check_vectors, digest, validate_members
from tools.verify_etsi_efr_reference import dtx_input, inspect_dtx, DTX_FRAMES
from tools.verify_etsi_efr_reference import erasure_input, inspect_erasures, ERASURE_BURSTS


class EtsiEfrReferenceTest(unittest.TestCase):
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
