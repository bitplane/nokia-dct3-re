import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class MachineProfileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.driver = (ROOT / "driver/nokia_3310.cpp").read_text()
        cls.makefile = (ROOT / "Makefile").read_text()

    def test_3210_owns_validated_boot_defaults(self):
        self.assertIn(
            "constexpr nokia_product_config PRODUCT_3210 = { 0x01, true, true, false };",
            self.driver,
        )
        profile = self.driver.split("void noki3310_state::noki3210(machine_config &config)", 1)[1]
        profile = profile.split("void noki3310_state::noki5210", 1)[0]
        self.assertIn(
            'm_mad2->set_timer0_hz(nokia_env_u32("NOKI3210_TIMER0_HZ", 33\'055));',
            profile,
        )
        self.assertIn("m_mad2->set_timer0_catchup(false);", profile)
        self.assertNotIn('"NOKI3210_TIMER0_CATCHUP"', profile)
        self.assertIn('"NOKI3210_MODEL_DSP_SERVICE", 1', profile)
        self.assertIn('"NOKI3210_MODEL_EXTERNAL_SERVICE_PEER", 1', profile)

    def test_other_products_keep_conservative_defaults(self):
        self.assertIn(
            "constexpr nokia_product_config PRODUCT_DEFAULT = { 0x04, false, false, false };",
            self.driver,
        )

    def test_contact_service_oracle_is_explicit_negative_profile(self):
        target = self.makefile.split("CONTACT_SERVICE_ENV :=", 1)[1].split("\n\n", 1)[0]
        for model in (
            "MODEL_DSP_SERVICE",
            "MODEL_EXTERNAL_SERVICE_PEER",
            "MODEL_SIM_DEVICE",
        ):
            self.assertIn(f"NOKI3210_{model}=0", target)
        self.assertIn("NOKI3210_CCONT_READY=0", target)
        self.assertIn("verify: RUN_ENV=$(CONTACT_SERVICE_ENV)", self.makefile)

    def test_3210_buzzer_uses_mad2_pup_and_divider(self):
        self.assertIn("BEEP(config, m_buzzer)", self.driver)
        self.assertIn("BIT(m_mad2->reg(0x15), 5)", self.driver)
        self.assertIn("(u16(m_mad2_regs[0x1c]) << 8) | m_mad2_regs[0x1d]", self.driver)
        self.assertIn("m_buzzer->set_clock(13'000'000 / divider)", self.driver)
        self.assertIn('NOKI3210_TRACE_BUZZER', self.driver)

    def test_charger_input_updates_vchar_and_latches_both_edges(self):
        body = self.driver.split("INPUT_CHANGED_MEMBER( noki3310_state::charger_irq )", 1)[1]
        body = body.split("static INPUT_PORTS_START", 1)[0]
        self.assertIn("set_adc_source(5", body)
        self.assertIn('NOKI3210_CHARGER_ADC', body)
        self.assertIn("latch_irq_sources(0x08)", body)
        self.assertNotIn("if (newval)", body)


if __name__ == "__main__":
    unittest.main()
