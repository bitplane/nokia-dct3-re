from pathlib import Path
import tempfile
import unittest

from tools.dct3_trace_build import PROBES, SWITCH, check, configure


class TraceBuildTest(unittest.TestCase):
    def test_configure_modes_and_unchanged_timestamp(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            template, output = root / "template.h", root / "generated.h"
            template.write_text(SWITCH + "\n")
            configure(template, output, True)
            self.assertIn("TRACES 1", output.read_text())
            stamp = output.stat().st_mtime_ns
            configure(template, output, True)
            self.assertEqual(stamp, output.stat().st_mtime_ns)
            configure(template, output, False)
            self.assertEqual(template.read_text(), output.read_text())

    def test_invalid_template_cannot_replace_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            template, output = root / "template.h", root / "generated.h"
            output.write_text("retained\n")
            for text in ("", SWITCH + "\n" + SWITCH + "\n"):
                template.write_text(text)
                with self.assertRaises(ValueError):
                    configure(template, output, True)
                self.assertEqual(output.read_text(), "retained\n")

    def test_link_canaries_have_positive_and_negative_controls(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "binary"
            binary.write_bytes(b"executable")
            check(binary, False)
            with self.assertRaises(ValueError):
                check(binary, True)
            binary.write_bytes(b"\x00".join(PROBES))
            check(binary, True)
            with self.assertRaises(ValueError):
                check(binary, False)
            for omitted in PROBES:
                binary.write_bytes(b"\x00".join(probe for probe in PROBES if probe != omitted))
                with self.assertRaises(ValueError):
                    check(binary, True)
            binary.write_bytes(b"")
            with self.assertRaises(ValueError):
                check(binary, False)
