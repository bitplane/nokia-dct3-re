import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class MachineProfileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.driver = (ROOT / "driver/nokia_3310.cpp").read_text()
        cls.ccont = (ROOT / "driver/nokia_ccont.cpp").read_text()
        cls.makefile = (ROOT / "Makefile").read_text()

    def test_3210_owns_validated_boot_defaults(self):
        self.assertIn(
            "{ 0x01, true, true, true, true, false, false, 64, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_3210 };",
            self.driver,
        )
        profile = self.driver.split("void noki3310_state::noki3210(machine_config &config)", 1)[1]
        profile = profile.split("void noki3310_state::noki5210", 1)[0]
        self.assertIn(
            "m_mad2->set_timer0_hz(33'055);",
            profile,
        )
        self.assertIn("m_mad2->set_timer0_catchup(false);", profile)
        self.assertNotIn('"NOKIA_DCT3_TIMER0_CATCHUP"', profile)
        apply = self.driver.split("void noki3310_state::apply_product_config", 1)[1]
        apply = apply.split("uint16_t noki3310_state::fw_word", 1)[0]
        self.assertIn("set_service_enabled(product.dsp_service)", apply)
        self.assertIn("set_external_service_enabled(product.external_service)", apply)
        self.assertIn("set_enabled(product.radio_peer)", apply)
        self.assertNotIn("nokia_env_u32", apply)
        self.assertNotIn("std::getenv", apply)

    def test_dsp_bootstrap_count_is_product_configuration(self):
        self.assertIn(
            "{ 0x04, true, true, true, false, true, false, 58, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_3310 };",
            self.driver,
        )

    def test_3330_owns_observed_peer_adc_keypad_and_bootstrap_defaults(self):
        self.assertIn(
            "{ 0x04, true, true, true, false, true, false, 64, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_3310 };",
            self.driver,
        )
        profile = self.driver.split("void noki3310_state::noki3330(machine_config &config)", 1)[1]
        profile = profile.split("void noki3310_state::noki3210", 1)[0]
        self.assertIn("apply_product_config(PRODUCT_3330);", profile)
        self.assertIn(
            "m_dsp_hle->set_bootstrap_exchange_limit(product.dsp_bootstrap_exchanges);",
            self.driver,
        )

    def test_other_products_keep_conservative_defaults(self):
        self.assertIn(
            "{ 0x04, false, false, false, false, false, false, 64, false, false, false, 0, 0, false, 0x00, 0x00, 84, 48, 84, 48, ADC_DEFAULT };",
            self.driver,
        )

    def test_3310_owns_evidenced_pack_and_peer_defaults(self):
        self.assertIn(
            "{ 0x000, 0x3ff, 0x220, 0x026, 0x200, 0x000, 0x200, 0x000 };",
            self.driver,
        )
        self.assertIn("m_simi->set_enabled(m_product.sim_device &&", self.driver)

    def test_3410_owns_dsp_reset_release_wiring(self):
        self.assertIn(
            "{ 0x02, true, true, true, false, true, false, 64, true, true, true, 0, 50, true, 0x53, 0x04, 102, 72, 96, 65, ADC_3310 };",
            self.driver,
        )
        profile = self.driver.split("void noki3310_state::noki3410(machine_config &config)", 1)[1]
        profile = profile.split("void noki3310_state::noki7110", 1)[0]
        self.assertIn("apply_product_config(PRODUCT_3410);", profile)

    def test_3410_owns_intel_b3_block_lock_protocol(self):
        self.assertIn("bool flash_b3_block_lock;", self.driver)
        self.assertIn("if (m_product.flash_b3_block_lock)", self.driver)
        for confirm in ("0x01", "0xd0", "0x2f"):
            self.assertIn(f"(data & 0xff) == {confirm}", self.driver)
        self.assertIn("m_flash_b3_erase_suspended", self.driver)
        self.assertIn("return 0x00c0 & mem_mask; // ready + erase suspended", self.driver)

    def test_contact_service_oracle_is_explicit_negative_profile(self):
        target = self.makefile.split("CONTACT_SERVICE_ENV :=", 1)[1].split("\n\n", 1)[0]
        for model in (
            "MODEL_DSP_SERVICE",
            "MODEL_EXTERNAL_SERVICE_PEER",
            "MODEL_SIM_DEVICE",
        ):
            self.assertIn(f"NOKIA_DCT3_{model}=0", target)
        self.assertIn("NOKIA_DCT3_CCONT_READY=0", target)
        self.assertIn("verify: RUN_ENV=$(CONTACT_SERVICE_ENV)", self.makefile)

    def test_3210_buzzer_uses_mad2_pup_and_divider(self):
        self.assertIn("BEEP(config, m_buzzer)", self.driver)
        self.assertIn("BIT(m_mad2->reg(0x15), 5)", self.driver)
        self.assertIn("(u16(m_mad2_regs[0x1c]) << 8) | m_mad2_regs[0x1d]", self.driver)
        self.assertIn("m_buzzer->set_clock(13'000'000 / divider)", self.driver)
        self.assertIn('NOKIA_DCT3_TRACE_BUZZER', self.driver)

    def test_charger_input_updates_vchar_and_latches_both_edges(self):
        body = self.driver.split("INPUT_CHANGED_MEMBER( noki3310_state::charger_irq )", 1)[1]
        body = body.split("static INPUT_PORTS_START", 1)[0]
        self.assertIn("set_charger_input(newval != 0", body)
        self.assertIn('NOKIA_DCT3_CHARGER_ADC', body)
        self.assertNotIn("latch_irq_sources", body)
        self.assertNotIn("if (newval)", body)

        ccont_body = self.ccont.split(
            "void nokia_ccont_device::set_charger_input", 1
        )[1].split("void nokia_ccont_device::select_w", 1)[0]
        self.assertIn("set_adc_source(5, connected ? vchar : 0)", ccont_body)
        self.assertIn("latch_irq_sources(0x08)", ccont_body)

    def test_charger_wake_resets_the_digital_baseband_domain(self):
        ccont_body = self.ccont.split(
            "void nokia_ccont_device::set_charger_input", 1
        )[1].split("void nokia_ccont_device::select_w", 1)[0]
        self.assertIn("connected && !m_charger_connected && !m_powered", ccont_body)
        self.assertIn("RESET_CHARGER", ccont_body)
        self.assertIn("m_power_cb(1)", ccont_body)

        power_body = self.driver.split("void noki3310_state::reset_digital_baseband", 1)[1]
        power_body = power_body.split("void noki3310_state::sim_irq_w", 1)[0]
        for device in (
            "m_maincpu", "m_mad2", "m_gensio", "m_mbus", "m_dspif",
            "m_dsp_hle", "m_external_service_peer", "m_simi", "m_sim_card",
            "m_lcd",
        ):
            self.assertIn(f"{device}->reset()", power_body)
        self.assertIn("machine_reset()", power_body)
        self.assertIn("m_power_on = 0", power_body)

        callback = self.driver.split("void noki3310_state::ccont_power_w", 1)[1]
        callback = callback.split("void noki3310_state::reset_digital_baseband", 1)[0]
        self.assertIn("reset_digital_baseband()", callback)


if __name__ == "__main__":
    unittest.main()
