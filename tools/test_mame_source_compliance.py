import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
DRIVER = ROOT / "driver"


class MameSourceComplianceTest(unittest.TestCase):
    def test_sources_have_mame_attribution_headers(self):
        sources = list(DRIVER.glob("*.cpp")) + list(DRIVER.glob("*.h"))
        sources.append(DRIVER / "nokia_3310_trace.inc")
        for source in sources:
            with self.subTest(source=source.name):
                first = source.read_text().splitlines()[:2]
                self.assertEqual(first[0], "// license:BSD-3-Clause")
                self.assertTrue(first[1].startswith("// copyright-holders:"))

    def test_headers_use_guards_without_emu_umbrella(self):
        for header in DRIVER.glob("*.h"):
            with self.subTest(header=header.name):
                text = header.read_text()
                guard = f"MAME_NOKIA_{header.stem.upper()}_H"
                self.assertIn(f"#ifndef {guard}", text)
                self.assertIn(f"#define {guard}", text)
                self.assertIn(f"#endif // {guard}", text)
                self.assertNotIn("#pragma once", text)
                self.assertNotIn('#include "emu.h"', text)

    def test_firmware_trace_quarantine_remains_small_and_observational(self):
        text = (DRIVER / "nokia_3310_trace.inc").read_text()
        self.assertLess(len(text.splitlines()), 200)
        self.assertNotIn("COMBINE_DATA", text)
        self.assertNotIn("set_state_int", text)
        self.assertNotIn("enqueue_rx_packet", text)


if __name__ == "__main__":
    unittest.main()
