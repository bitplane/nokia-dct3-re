import pathlib
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/extract_dct3_wintesla.py"


def records(address, payload):
    result = bytearray()
    for offset in range(0, len(payload), 0x2000):
        block = payload[offset : offset + 0x2000]
        result += bytes([0x0B])
        result += (address + offset).to_bytes(3, "big")
        result += b"\x00"
        result += len(block).to_bytes(3, "big")
        result += b"\x00"
        result += block
    return bytes(result)


class ExtractDct3WinteslaTest(unittest.TestCase):
    def test_flash_only_mode_preserves_gap_and_needs_no_pmm(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            mcu = b"M" * 0x2100
            ppm = b"P" * 0x0100
            (root / "mcu").write_bytes(records(0x200000, mcu))
            (root / "ppm").write_bytes(records(0x203000, ppm))
            output = root / "flash"
            subprocess.run(
                [
                    sys.executable,
                    str(TOOL),
                    "--mcu", str(root / "mcu"),
                    "--ppm", str(root / "ppm"),
                    "--flash-output", str(output),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertEqual(mcu + b"\xff" * 0x0F00 + ppm, output.read_bytes())

    def test_pmm_and_eeprom_output_remain_a_pair(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "mcu").write_bytes(records(0x200000, b"M"))
            (root / "ppm").write_bytes(records(0x200001, b"P"))
            result = subprocess.run(
                [
                    sys.executable,
                    str(TOOL),
                    "--mcu", str(root / "mcu"),
                    "--ppm", str(root / "ppm"),
                    "--pmm", str(root / "pmm"),
                    "--flash-output", str(root / "flash"),
                ],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertIn(
                "--pmm and --eeprom-output must be supplied together",
                result.stderr,
            )


if __name__ == "__main__":
    unittest.main()
