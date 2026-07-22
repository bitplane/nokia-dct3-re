import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class KeypadInputTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.driver = (ROOT / "driver/nokia_dct3.cpp").read_text()
        cls.harness = (ROOT / "mame_nokia_dct3_input_exerciser.lua").read_text()

    def test_handset_controls_have_mame_names(self):
        expected = {
            "Navi / Left Softkey": "KEYCODE_ENTER",
            "C / Right Softkey": "KEYCODE_BACKSPACE",
            "Scroll Up": "KEYCODE_UP",
            "Scroll Down": "KEYCODE_DOWN",
            "Keypad *": "KEYCODE_ASTERISK",
            "Keypad #": "KEYCODE_MINUS",
            "Power": "KEYCODE_SPACE",
        }
        for name, keycode in expected.items():
            self.assertIn(f'PORT_NAME("{name}")', self.driver)
            self.assertIn(keycode, self.driver)
        for digit in range(10):
            self.assertIn(f'PORT_NAME("Keypad {digit}")', self.driver)

    def test_harness_uses_semantic_aliases(self):
        for alias in ("navi", "select", "left", "soft1"):
            self.assertIn(f"key_fields.{alias} = key_fields.enter", self.harness)
        for alias in ("clear", "back", "right", "soft2"):
            self.assertIn(f"key_fields.{alias} = key_fields.c", self.harness)
        self.assertIn("key_fields.hash = key_fields.minus", self.harness)

    def test_scripted_sequences_support_emulation_time_waits(self):
        self.assertIn('string.match(name, "^wait(%d+)$")', self.harness)
        self.assertIn("emu.wait(tonumber(wait_ms) / 1000)", self.harness)

    def test_shutdown_publishes_the_terminal_lcd_mirror(self):
        stop = self.harness.split("emu.add_machine_stop_notifier", 1)[1]
        self.assertIn("queue_lcd_dump()", stop)
        self.assertLess(stop.index("queue_lcd_dump()"), stop.index("write_lcd_dump()"))

    def test_periodic_oracle_captures_when_video_frames_stop(self):
        periodic = self.harness.split("emu.register_periodic", 1)[1]
        self.assertIn("if lcd_dirty then", periodic)
        self.assertLess(periodic.index("queue_lcd_dump()"), periodic.index("write_lcd_dump()"))

    def test_input_remains_physical_matrix_driven(self):
        self.assertIn("field:set_value(1)", self.harness)
        self.assertIn("PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(nokia_dct3_state::key_irq), 0)", self.driver)
        self.assertNotIn("debug_ram", self.harness)

    def test_matrix_drive_contract_uses_direction_and_active_low_signal(self):
        self.assertIn("m_mad2_regs[0xa8] & ~m_mad2_regs[0x28]", self.driver)

    def test_3310_fixture_uses_boot_relative_timing(self):
        self.assertIn('machine.system.name == "noki3310"', self.harness)
        self.assertIn('machine.system.name == "noki3330"', self.harness)
        self.assertIn("or is_3410", self.harness)
        self.assertIn("if is_five_row_product then", self.harness)
        self.assertIn("startup_ready_time = emulation_seconds()", self.harness)

    def test_3410_uses_its_rom_derived_matrix(self):
        ports = self.driver.split("static INPUT_PORTS_START( noki3410 )", 1)[1]
        ports = ports.split("INPUT_PORTS_END", 1)[0]
        for name, keycode in (
            ("Left Softkey / Menu", "KEYCODE_ENTER"),
            ("Right Softkey", "KEYCODE_BACKSPACE"),
            ("Scroll Up", "KEYCODE_UP"),
            ("Scroll Down", "KEYCODE_DOWN"),
            ("Send", "KEYCODE_S"),
            ("End", "KEYCODE_E"),
        ):
            self.assertIn(f'PORT_NAME("{name}")', ports)
            self.assertIn(keycode, ports)
        self.assertIn(
            "SYST( 2002, noki3410, 0,      0,      noki3410, noki3410,",
            self.driver,
        )

        fixture = self.harness.split("if is_3410 then", 1)[1]
        fixture = fixture.split("elseif is_five_row_product then", 1)[0]
        self.assertIn('enter = field_by_mask("COL.4", 0x01)', fixture)
        self.assertIn('c = field_by_mask("COL.1", 0x01)', fixture)
        self.assertIn('up = field_by_mask("COL.3", 0x10)', fixture)
        self.assertIn('down = field_by_mask("COL.3", 0x01)', fixture)
        self.assertIn('send = field_by_mask("COL.4", 0x02)', fixture)
        self.assertIn('["end"] = field_by_mask("COL.1", 0x02)', fixture)

    def test_3310_navigation_gate_uses_only_physical_key_sequences(self):
        makefile = (ROOT / "Makefile").read_text()
        target = makefile.split("verify-3310-navigation:", 1)[1].split("\n\n", 1)[0]
        self.assertIn("enter,wait1000,enter,wait700,enter,wait700,down", target)
        self.assertIn("down,wait700,c,wait700,c", target)
        self.assertIn("ORACLE_3310_PHONEBOOK_NAV_SHA", target)
        self.assertIn("ORACLE_3310_IDLE_SHA", target)
        self.assertNotIn("debug_ram", target)
        self.assertNotIn("TRACE_", target)

    def test_3330_gates_use_only_physical_first_boot_and_navigation_inputs(self):
        makefile = (ROOT / "Makefile").read_text()
        self.assertIn("NOKI3330_FIRST_BOOT_KEYS := 1,2,3,4,5,enter", makefile)
        self.assertIn("0,1,0,1,2,0,0,2,wait600,enter", makefile)
        target = makefile.split("verify-3330-navigation:", 1)[1].split("\n\n", 1)[0]
        self.assertIn("wait4000,enter,wait900,down", target)
        self.assertIn("down,wait900,c", target)
        self.assertIn("ORACLE_3330_MESSAGES_SHA", target)
        self.assertIn("ORACLE_3330_IDLE_SHA", target)
        self.assertNotIn("debug_ram", target)
        self.assertNotIn("TRACE_", target)

    def test_3410_gates_reject_blank_frames_and_use_only_physical_keys(self):
        makefile = (ROOT / "Makefile").read_text()
        self.assertIn("ORACLE_3410_IDLE_SHA ?= 14c1f25e", makefile)
        self.assertIn("ORACLE_3410_MESSAGES_SHA ?= a44445d8", makefile)
        self.assertIn("! -name '*_z918_*' ! -name '*_ff918_*'", makefile)

        frontier = makefile.split("verify-3410-frontier:", 1)[1].split("\n\n", 1)[0]
        self.assertIn("POST_READY_KEYS=end", frontier)
        self.assertIn("ORACLE_3410_IDLE_SHA", frontier)

        target = makefile.split("verify-3410-navigation:", 1)[1].split("\n\n", 1)[0]
        self.assertIn("POST_READY_KEYS=enter", target)
        self.assertIn("enter,wait1000,end", target)
        self.assertIn("ORACLE_3410_MESSAGES_SHA", target)
        self.assertIn("ORACLE_3410_IDLE_SHA", target)
        self.assertNotIn("debug_ram", target)
        self.assertNotIn("TRACE_", target)

    def test_harness_audits_the_physical_3210_sram_boundary(self):
        self.assertIn("install_read_tap(0x120000, 0x17ffff", self.harness)
        self.assertIn("install_write_tap(0x120000, 0x17ffff", self.harness)
        self.assertIn("upper_ram_reads", self.harness)
        self.assertIn("upper_ram_writes", self.harness)

    def test_buzzer_fixture_uses_only_mapped_mad2_registers(self):
        self.assertIn("NOKIA_DCT3_BUZZER_FIXTURE_AT", self.harness)
        for address in ("0x20015", "0x2001c", "0x2001d", "0x2001e"):
            self.assertIn(f"space:write_u8({address}", self.harness)

    def test_rtc_fixture_uses_ccont_gensio_transactions(self):
        fixture = self.harness.split("if rtc_fixture_at >= 0 then", 1)[1]
        fixture = fixture.split("if state_roundtrip_at >= 0 then", 1)[0]
        self.assertIn("space:write_u8(0x2002d", fixture)
        self.assertIn("space:write_u8(0x2002c", fixture)
        self.assertIn("ccont_write(0x0b, 1)", fixture)
        self.assertIn("ccont_write(0x0c, 12)", fixture)
        self.assertIn("ccont_write(0x0f, 0x50)", fixture)

    def test_mad2_reset_fixture_uses_only_the_mapped_controller_register(self):
        fixture = self.harness.split("if mad2_reset_fixture_at >= 0 then", 1)[1]
        fixture = fixture.split("if state_roundtrip_at >= 0 then", 1)[0]
        self.assertIn("space:write_u8(0x20001", fixture)
        self.assertNotIn("debug_ram", fixture)

    def test_mad2_watchdog_fixture_uses_only_the_mapped_controller_register(self):
        fixture = self.harness.split("if mad2_watchdog_fixture_at >= 0 then", 1)[1]
        fixture = fixture.split("if state_roundtrip_at >= 0 then", 1)[0]
        self.assertIn("space:write_u8(0x20003, 0x01)", fixture)
        self.assertNotIn("debug_ram", fixture)

    def test_interactive_target_uses_standard_mame_input(self):
        makefile = (ROOT / "Makefile").read_text()
        target = makefile.split("run-interactive:", 1)[1].split("\n\n", 1)[0]
        self.assertIn("INTERACTIVE_MAME_ARGS", target)
        self.assertIn("PRESERVE_NVRAM=1", target)
        self.assertNotIn("autoboot_script", target)
        self.assertNotIn("POST_READY_KEYS", target)


if __name__ == "__main__":
    unittest.main()
