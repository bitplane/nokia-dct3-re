import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
DRIVER = ROOT / "driver"


class MameSourceComplianceTest(unittest.TestCase):
    def test_sources_have_mame_attribution_headers(self):
        sources = list(DRIVER.glob("*.cpp")) + list(DRIVER.glob("*.h"))
        sources.append(DRIVER / "nokia_dct3_trace.inc")
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
        text = (DRIVER / "nokia_dct3_trace.inc").read_text()
        # Keep the quarantine reviewable even as independently gated
        # cross-product observations are moved out of the production driver.
        # Cross-product radio gates extend the original quarantine; retain a
        # hard ceiling so closed investigations must still be removed.
        self.assertLess(len(text.splitlines()), 400)
        self.assertNotIn("COMBINE_DATA", text)
        self.assertNotIn("set_state_int", text)
        self.assertNotIn("enqueue_rx_packet", text)

    def test_firmware_address_observations_stay_in_trace_quarantine(self):
        driver = (DRIVER / "nokia_dct3.cpp").read_text()
        for token in ("fw_byte(0x", "fw_word(0x", "fw_dword(0x", "FW_SCHED_RUNNING_TASK_ID"):
            with self.subTest(token=token):
                self.assertNotIn(token, driver)

    def test_retired_broad_trace_families_do_not_return(self):
        sources = "\n".join(path.read_text() for path in DRIVER.glob("nokia_dct3*"))
        for token in (
            "radio_type0f_commit", "sim_fifo_read", "sim_txd:",
            "sim_control_w", "pup_output:", "LOG_PUP_OUTPUTS",
        ):
            with self.subTest(token=token):
                self.assertNotIn(token, sources)

    def test_production_sources_do_not_read_process_environment(self):
        for source in list(DRIVER.glob("*.cpp")) + list(DRIVER.glob("*.h")):
            with self.subTest(source=source.name):
                text = source.read_text()
                self.assertNotIn("getenv", text)
                self.assertNotIn("NOKIA_DCT3_", text)

    def test_generic_gsm_layers_do_not_branch_on_handset_families(self):
        for name in (
            "nokia_gsm_session.cpp",
            "nokia_lapdm_link.cpp",
            "gsm_tch_f_l1.cpp",
        ):
            with self.subTest(source=name):
                text = (DRIVER / name).read_text().lower()
                self.assertNotIn("nse8", text)
                self.assertNotIn("nhm5", text)

    def test_driver_has_no_runtime_handset_name_dispatch(self):
        text = (DRIVER / "nokia_dct3.cpp").read_text()
        for token in (
            "machine().system().name",
            "machine().system().parent",
            "driver_name",
        ):
            with self.subTest(token=token):
                self.assertNotIn(token, text)


if __name__ == "__main__":
    unittest.main()
