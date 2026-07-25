import pathlib
import tempfile
import unittest

from tools.dsp_rom_audit import audit, classify


class DspRomAuditTest(unittest.TestCase):
    def test_fill_regions_are_not_reported_as_firmware(self):
        self.assertEqual(classify(bytes([0xff]) * 32), "placeholder-fill-ff")
        self.assertEqual(classify(bytes([0x00]) * 32), "placeholder-fill-00")

    def test_nonuniform_region_is_distinguished(self):
        self.assertEqual(classify(bytes([0xff, 0xfe])), "nonuniform-data")

    def test_audit_reports_size_and_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "dsp_prom"
            path.write_bytes(b"\xff" * 7)
            kind, size, digest = audit(path)
        self.assertEqual((kind, size), ("placeholder-fill-ff", 7))
        self.assertEqual(len(digest), 64)


if __name__ == "__main__":
    unittest.main()
