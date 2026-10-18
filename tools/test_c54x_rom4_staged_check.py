import json
from pathlib import Path
import re
import struct
import unittest
from unittest.mock import patch

from tools.c54x_rom4_staged_check import (
    SOURCE_OFFSET, TAIL_SOURCE_OFFSET, REPEAT_SOURCE_OFFSET, DEMAND_SOURCE_OFFSET,
    REFERENCE, TIME, check,
)


class StagedFrontierTest(unittest.TestCase):
    def setUp(self):
        self.rows = json.loads(REFERENCE.read_text())["records"]
        self.text = "\n".join(self.rows) + "\n"
        self.uploads = [
            ("rom4_staged_frontier:", SOURCE_OFFSET),
            ("rom4_staged_uploaded_tail:", TAIL_SOURCE_OFFSET),
            ("rom4_staged_repeat_tail:", REPEAT_SOURCE_OFFSET),
            ("rom4_staged_demand_tail:", DEMAND_SOURCE_OFFSET),
        ]
        self.image = bytearray(REPEAT_SOURCE_OFFSET + 48)
        self.payloads = {}
        for prefix, offset in self.uploads:
            row = next(row for row in self.rows if row.startswith(prefix))
            words = re.search(r"words=(\S+)", row).group(1).split(",")
            payload = struct.pack(f">{len(words)}H", *(int(word, 16) for word in words))
            self.payloads[offset] = payload
            self.image[offset:offset + len(payload)] = payload
        self.image = bytes(self.image)

    def test_binds_clean_observation_without_promoting_boot(self):
        result = check(self.text, self.image)
        self.assertEqual(result["status"], "live_staged_native_service_checkpoint")
        self.assertEqual(result["source_offset"], SOURCE_OFFSET)
        self.assertEqual(result["tail_source_offset"], TAIL_SOURCE_OFFSET)
        self.assertEqual(result["repeat_source_offset"], REPEAT_SOURCE_OFFSET)
        self.assertEqual(result["demand_source_offset"], DEMAND_SOURCE_OFFSET)
        self.assertEqual(result["checkpoint_pc"], 0x09ac)
        self.assertEqual(result["host_header_reads"], 9)
        self.assertEqual(result["host_consumed"], 22)
        self.assertEqual(result["rf_sample_reads"], 134560)
        self.assertEqual(result["rf_sample_blocks"], 4205)
        self.assertEqual(result["rf_sample_remainder"], 0)
        self.assertEqual(result["rf_control_writes"], 262)
        self.assertEqual(result["observed_rf_control_writes"], 32)
        self.assertEqual(result["transfer_expiries"], 2878)
        self.assertEqual(result["observed_transfer_expiries"], 256)
        self.assertEqual(result["bsp_transmit_words"], [0x0aaa, 0xffd5])
        self.assertEqual(result["bsp_receive_words"], [0x0aaa, 0x0aaa])
        self.assertEqual(result["completion_strobes"], 1642)
        self.assertEqual(result["low_gap_bus_reads"], 9)
        self.assertEqual(result["swwsr"], 0x2000)
        self.assertEqual(result["clean_validator_continuations"], 1)
        for name in ("boot_claim", "rf_acquisition_claim", "fitted_bus_dialect_claim",
                     "physical_low_gap_identity_claim", "parallel_store_carry_claim",
                     "upload_complete_claim", "preceding_instruction_retirement_claim",
                     "uninterrupted_session_claim", "terminal_illegal",
                     "checkpoint_illegal", "reset_request_observed"):
            self.assertFalse(result[name], name)

    def test_rejects_each_missing_or_duplicate_record(self):
        for index, row in enumerate(self.rows):
            with self.subTest(index=index, kind="missing"), self.assertRaises(ValueError):
                check("\n".join(self.rows[:index] + self.rows[index + 1:]), self.image)
            with self.subTest(index=index, kind="duplicate"), self.assertRaises(ValueError):
                check(self.text + row + "\n", self.image)

    def test_rejects_changed_event_fields(self):
        for row in self.rows:
            match = re.search(r"\w+=(\S+)", row)
            if not match:
                continue
            changed = row[:match.start(1)] + "invalid" + row[match.end(1):]
            with self.subTest(row=row), self.assertRaises(ValueError):
                check(self.text.replace(row, changed), self.image)

    def test_rejects_faults_even_with_all_positive_records(self):
        for fault in ("rom4_reset_request: reason=00000004",
                      "unsupported staged C54x form", "unimplemented C54x instruction",
                      "TMS320C54x illegal instruction"):
            with self.subTest(fault=fault), self.assertRaisesRegex(ValueError, "reset or DSP fault"):
                check(self.text + fault, self.image)

    def test_rebank_cannot_promote_a_fault_or_rejected_challenge(self):
        for original, bad in (("illegal=0", "illegal=1"),
                              ("r6=00000001", "r6=00000000"),
                              ("system=nse1pipe", "system=noki5110")):
            rows = [row.replace(original, bad) for row in self.rows]
            with self.subTest(bad=bad), patch("tools.c54x_rom4_staged_check.REFERENCE") as reference:
                reference.read_text.return_value = json.dumps({"records": rows})
                with self.assertRaises(ValueError):
                    check("\n".join(rows), self.image)

    def test_rejects_failed_or_duplicate_validator(self):
        row = next(row for row in self.rows if row.startswith("rom4_validator:"))
        for text in (self.text.replace("r6=00000001", "r6=00000000"),
                     self.text + row, self.text.replace(row, "")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_rejects_old_reset_lifecycle_counts(self):
        for original, old in (("host_consumed=22", "host_consumed=29"),
                              ("rf_port32_writes=262", "rf_port32_writes=242"),
                              ("transfer_expiries=2878", "transfer_expiries=2663"),
                              ("completion_strobes=1642", "completion_strobes=1603")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(self.text.replace(original, old), self.image)

    def test_rejects_shifted_timer_table_and_missing_tail_controls(self):
        for original, bad in (("target=0834 counter=0834", "target=0884 counter=0884"),
                              ("port31=0000 port32=0050", "port31=2813 port32=0030"),
                              ("rf_port32_writes=262", "rf_port32_writes=260")):
            self.assertIn(original, self.text)
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check(self.text.replace(original, bad), self.image)

    def test_rejects_changed_or_duplicate_mcu_uploads(self):
        for offset, payload in self.payloads.items():
            changed = bytearray(self.image)
            changed[offset] ^= 1
            with self.subTest(offset=offset, kind="changed"), self.assertRaises(ValueError):
                check(self.text, bytes(changed))
            with self.subTest(offset=offset, kind="duplicate"), self.assertRaises(ValueError):
                check(self.text, self.image + payload)
            with self.subTest(offset=offset, kind="truncated"), self.assertRaises(ValueError):
                check(self.text, self.image[:offset + len(payload) - 1])

    def test_rejects_out_of_order_or_out_of_bounds_time(self):
        for before, after in (("t=0.175897519", "t=0.100000000"),
                              ("t=10.016666667", "t=9.999999999"),
                              ("t=10.016666667", "t=10.100000000"),
                              ("t=0.500000000", "t=0.499999999"),
                              ("t=0.133888808", "t=0.133888750")):
            self.assertIn(before, self.text)
            with self.subTest(after=after), self.assertRaises(ValueError):
                check(self.text.replace(before, after), self.image)

    def test_does_not_claim_absolute_submicrosecond_timing(self):
        shifted = TIME.sub(lambda match: f"t={float(match.group(1)) + 1e-9:.9f}", self.text)
        check(shifted, self.image)

    def test_rejects_reordered_or_malformed_records(self):
        swapped = self.rows[:]
        swapped[1], swapped[2] = swapped[2], swapped[1]
        for text in ("", "\n".join(swapped), self.text + "rom4_staged_end: malformed\n",
                     self.text.replace("system=nse1pipe", "system=noki5110")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_accepts_mame_log_prefixes_and_unrelated_diagnostics(self):
        prefixed = "\n".join("[luaengine] " + row for row in self.rows)
        check("Soft reset\n" + prefixed + "\nunrelated diagnostic\n", self.image)


if __name__ == "__main__":
    unittest.main()
