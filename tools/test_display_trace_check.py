#!/usr/bin/env python3
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import display_trace_check


class DisplayTraceCheckTests(unittest.TestCase):
    def test_lcd_transport_accepts_selected_command_and_full_frame(self):
        lines = [
            "display_io: off=2d data=21",
            "display_io: off=6e data=24",
            "display_io: off=6e data=40",
            "display_io: off=6e data=80",
        ] + ["display_io: off=2e data=00"] * 504
        errors, counts = display_trace_check.check_lcd_io("\n".join(lines))
        self.assertEqual(errors, [])
        self.assertEqual(counts["commands"], 3)
        self.assertEqual(counts["data_bytes"], 504)

    def test_lcd_transport_rejects_wrong_selection(self):
        errors, _ = display_trace_check.check_lcd_io(
            "display_io: off=2d data=25\ndisplay_io: off=6e data=24"
        )
        self.assertTrue(any("without select" in error for error in errors))

    def test_v600_profile_uses_rom_authored_defaults(self):
        record = "000901340104010100ffffff"
        text = "\n".join((
            "display_profile: pc=0029a996 source0=" + "00" * 12 + " active=" + "00" * 10,
            "display_profile: pc=0029a768 source0=" + record + " active=" + "00" * 10,
            "display_profile: pc=002b1e80 source0=" + record +
                " active=" + "ff" * 7 + "0400ff",
        ))
        errors, counts = display_trace_check.check_v600_profile(text)
        self.assertEqual(errors, [])
        self.assertEqual(counts["update_seen"], 0)

    def test_descriptor_requires_all_rom_authored_fields(self):
        rom = bytearray(0x100000)
        descriptor = 0x2DAD78 - display_trace_check.FLASH_BASE
        for offset, value in ((descriptor, 0x0749),
                              (descriptor + 4, (0x18A8 << 16) | 12)):
            encoded = ((value & 0xffff) << 16) | (value >> 16)
            rom[offset:offset + 4] = encoded.to_bytes(4, "little")
        eeprom = bytearray([0xff]) * 0x4000
        eeprom[0x18A8:0x18A8 + 36] = display_trace_check.PROFILE_DEFAULTS
        errors, result = display_trace_check.check_descriptor(
            bytes(rom), bytes(eeprom), "v600")
        self.assertEqual(errors, [])
        self.assertTrue(result["defaults"])

        eeprom[0x18A8 + 12] ^= 1
        errors, _ = display_trace_check.check_descriptor(
            bytes(rom), bytes(eeprom), "v600")
        self.assertTrue(any("ROM-authored" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
