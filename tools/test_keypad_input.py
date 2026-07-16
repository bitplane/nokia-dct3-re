import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class KeypadInputTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.driver = (ROOT / "driver/nokia_3310.cpp").read_text()
        cls.harness = (ROOT / "mame_noki3210_input_exerciser.lua").read_text()

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

    def test_input_remains_physical_matrix_driven(self):
        self.assertIn("field:set_value(1)", self.harness)
        self.assertIn("PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(noki3310_state::key_irq), 0)", self.driver)
        self.assertNotIn("debug_ram", self.harness)

    def test_harness_audits_the_physical_3210_sram_boundary(self):
        self.assertIn("install_read_tap(0x120000, 0x17ffff", self.harness)
        self.assertIn("install_write_tap(0x120000, 0x17ffff", self.harness)
        self.assertIn("upper_ram_reads", self.harness)
        self.assertIn("upper_ram_writes", self.harness)

    def test_buzzer_fixture_uses_only_mapped_mad2_registers(self):
        self.assertIn("NOKI3210_BUZZER_FIXTURE_AT", self.harness)
        for address in ("0x20015", "0x2001c", "0x2001d", "0x2001e"):
            self.assertIn(f"space:write_u8({address}", self.harness)

    def test_interactive_target_uses_standard_mame_input(self):
        makefile = (ROOT / "Makefile").read_text()
        target = makefile.split("run-interactive:", 1)[1].split("\n\n", 1)[0]
        self.assertIn("INTERACTIVE_MAME_ARGS", target)
        self.assertIn("PRESERVE_NVRAM=1", target)
        self.assertNotIn("autoboot_script", target)
        self.assertNotIn("POST_READY_KEYS", target)


if __name__ == "__main__":
    unittest.main()
