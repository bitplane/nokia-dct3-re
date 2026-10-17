from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.verify_etsi_efr_reference import check_vectors, digest, validate_members


class EtsiEfrReferenceTest(unittest.TestCase):
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
