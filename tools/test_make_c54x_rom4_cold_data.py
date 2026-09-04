import pathlib
import subprocess
import sys
import tempfile
import unittest


class ColdDataImageTest(unittest.TestCase):
    def test_materializes_only_complete_drom_range(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "drom.txt"
            output = root / "data.bin"
            source.write_text("".join(
                f"{address:04x}: {address ^ 0x5a5a:04x}\n"
                for address in range(0xB000, 0xF000)
            ))
            subprocess.run(
                [sys.executable, "tools/make_c54x_rom4_cold_data.py",
                 str(source), str(output)], check=True)
            image = output.read_bytes()
            self.assertEqual(len(image), 0x20000)
            self.assertEqual(image[0xAFFF * 2:0xB000 * 2], b"\0\0")
            self.assertEqual(image[0xB000 * 2:0xB000 * 2 + 2], b"\xeaZ")
            self.assertEqual(image[0xEFFF * 2:0xEFFF * 2 + 2], b"\xb5\xa5")


if __name__ == "__main__":
    unittest.main()
