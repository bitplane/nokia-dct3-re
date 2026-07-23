import pathlib
import json
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class MachineProfileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.driver = (ROOT / "driver/nokia_dct3.cpp").read_text()
        cls.b3_flash = (ROOT / "driver/nokia_b3_flash.cpp").read_text()
        cls.ccont = (ROOT / "driver/nokia_ccont.cpp").read_text()
        cls.pup = (ROOT / "driver/nokia_pup.cpp").read_text()
        cls.makefile = (ROOT / "Makefile").read_text()

    def function_body(self, signature, next_signature):
        return self.driver.split(signature, 1)[1].split(next_signature, 1)[0]

    def assert_profile_fields(self, builder, expected):
        body = self.function_body(
            f"constexpr nokia_product_config {builder}()",
            "constexpr nokia_product_config",
        )
        for field, value in expected.items():
            self.assertIn(f"result.{field} = {value};", body)

    def test_3210_owns_validated_boot_defaults(self):
        self.assert_profile_fields(
            "make_3210_config",
            {
                "power_on_column_mask": "0x01",
                "sim_device": "true",
                "dsp_service": "true",
                "external_service": "true",
                "radio_peer": "true",
                "dsp_service_delay_us": "4'000",
                "dsp_peer_poll_ms": "4",
                "ccont_board": "ADC_3210",
            },
        )
        profile = self.driver.split("void nokia_dct3_state::noki3210(machine_config &config)", 1)[1]
        profile = profile.split("void nokia_dct3_state::noki5210", 1)[0]
        self.assertIn(
            "m_mad2->set_timer0_hz(33'055);",
            profile,
        )
        self.assertIn("m_mad2->set_timer0_catchup(false);", profile)
        self.assertNotIn('"NOKIA_DCT3_TIMER0_CATCHUP"', profile)
        apply = self.function_body(
            "void nokia_dct3_state::apply_product_config",
            "void nokia_dct3_state::machine_reset",
        )
        self.assertIn("set_service_enabled(product.dsp_service)", apply)
        self.assertIn("set_external_service_enabled(product.external_service)", apply)
        self.assertIn("set_enabled(product.radio_peer)", apply)
        self.assertIn("set_service_delay_us(product.dsp_service_delay_us)", apply)
        self.assertIn("set_peer_poll_ms(product.dsp_peer_poll_ms)", apply)
        self.assertNotIn("nokia_env_u32", apply)
        self.assertNotIn("std::getenv", apply)

    def test_dsp_bootstrap_count_is_product_configuration(self):
        self.assert_profile_fields(
            "make_3310_config",
            {"dsp_bootstrap_exchanges": "58", "keypad_five_rows": "true"},
        )

    def test_3330_owns_observed_peer_adc_keypad_and_bootstrap_defaults(self):
        self.assert_profile_fields(
            "make_3330_config",
            {
                "sim_device": "true",
                "dsp_service": "true",
                "external_service": "true",
                "keypad_five_rows": "true",
                "ccont_board": "ADC_3310",
            },
        )
        profile = self.driver.split("void nokia_dct3_state::noki3330(machine_config &config)", 1)[1]
        profile = profile.split("void nokia_dct3_state::noki3210", 1)[0]
        self.assertIn("apply_product_config(PRODUCT_3330);", profile)
        self.assertIn(
            "m_dsp_hle->set_bootstrap_exchange_limit(product.dsp_bootstrap_exchanges);",
            self.driver,
        )

    def test_other_products_keep_conservative_defaults(self):
        body = self.function_body(
            "constexpr nokia_product_config make_conservative_config",
            "constexpr nokia_product_config PRODUCT_3210",
        )
        self.assertIn("nokia_product_config result;", body)
        self.assertIn("result.power_on_column_mask = power_on_column_mask;", body)
        self.assertIn("bool sim_device = false;", self.driver)
        self.assertIn("bool dsp_service = false;", self.driver)
        self.assertIn("bool external_service = false;", self.driver)
        self.assertIn("bool radio_peer = false;", self.driver)

    def test_each_machine_applies_one_explicit_product_profile(self):
        expected = {
            "noki3310": ("dct3_base(config);", "PRODUCT_3310"),
            "noki3330": ("dct3_32mbit_flash_base(config);", "PRODUCT_3330"),
            "noki3210": ("dct3_base(config);", "PRODUCT_3210"),
            "noki5210": ("dct3_32mbit_flash_base(config);", "PRODUCT_5X10"),
            "noki8xxx": ("dct3_base(config);", "PRODUCT_8XXX"),
            "noki7110": ("dct3_32mbit_flash_base(config);", "PRODUCT_DEFAULT"),
            "noki6210": ("dct3_32mbit_flash_base(config);", "PRODUCT_DEFAULT"),
        }
        for machine, (base, product) in expected.items():
            with self.subTest(machine=machine):
                body = self.driver.split(
                    f"void nokia_dct3_state::{machine}(machine_config &config)", 1
                )[1].split("\n}\n", 1)[0]
                self.assertIn(base, body)
                self.assertEqual(body.count("apply_product_config("), 1)
                self.assertIn(f"apply_product_config({product});", body)
                self.assertNotIn("noki3330(config)", body)

    def test_3310_owns_evidenced_pack_and_peer_defaults(self):
        self.assertIn(
            "{ 0x000, 0x3ff, 0x220, 0x026, 0x200, 0x000, 0x200, 0x000 }",
            self.driver,
        )
        self.assertIn("m_simi->set_enabled(m_product.sim_device &&", self.driver)

    def test_3210_board_profile_routes_both_voltage_inputs(self):
        profile = self.driver.split(
            "constexpr nokia_ccont_board_profile ADC_3210", 1
        )[1].split("constexpr nokia_ccont_board_profile ADC_3310", 1)[0]
        self.assertIn("{ 0x2c0, 0x2c0, 0x2d0, 0x280, 0x200, 0x000, 0x200, 0x000 }", profile)
        self.assertIn("{ 0, 1 }, 2, 3, 4, 5", profile)
        census = json.loads((ROOT / "docs/data/ccont_adc_channels.json").read_text())
        signals = {item["channel"]: item["signal"] for item in census["channels"]}
        self.assertEqual("vbatt", signals[0])
        self.assertEqual("vbatt", signals[1])
        self.assertEqual("bsi", signals[3])
        self.assertEqual("btemp", signals[4])
        self.assertEqual("vchar", signals[5])
        self.assertFalse(census["timing"]["conversion_irq"])

    def test_3410_owns_dsp_reset_release_wiring(self):
        self.assert_profile_fields(
            "make_3410_config",
            {
                "power_on_column_mask": "0x02",
                "dsp_bootstrap_ping_pong": "true",
                "dsp_code_block_request": "true",
                "dsp_parked_boot_status": "true",
                "dsp_service_delay_us": "50",
                "flash_b3_block_lock": "true",
                "dsp_reset_running_status": "0x53",
                "dsp_release_mask": "0x04",
                "lcd_controller_width": "102",
                "lcd_controller_height": "72",
            },
        )
        profile = self.driver.split("void nokia_dct3_state::noki3410(machine_config &config)", 1)[1]
        profile = profile.split("void nokia_dct3_state::noki7110", 1)[0]
        self.assertIn("apply_product_config(PRODUCT_3410);", profile)

    def test_3410_owns_intel_b3_block_lock_protocol(self):
        self.assertIn("bool flash_b3_block_lock = false;", self.driver)
        self.assertIn("m_b3_flash->set_enabled(product.flash_b3_block_lock);", self.driver)
        for confirm in ("0x01", "0xd0", "0x2f"):
            self.assertIn(f"command == {confirm}", self.b3_flash)
        self.assertIn("m_erase_suspended", self.b3_flash)
        self.assertIn("return 0x00c0 & mem_mask; // ready + erase suspended", self.b3_flash)
        self.assertNotIn("m_flash_b3_erase_suspended", self.driver)

    def test_contact_service_oracle_is_explicit_negative_profile(self):
        target = self.makefile.split("CONTACT_SERVICE_ARGS :=", 1)[1].split("\n\n", 1)[0]
        self.assertIn("fixtures/contact_service", target)
        self.assertIn("verify: RUN_EXTRA_ARGS=$(CONTACT_SERVICE_ARGS)", self.makefile)
        fixture = (ROOT / "fixtures/contact_service/noki3210.cfg").read_text()
        for mask in (1, 2, 4, 8):
            self.assertIn(f'mask="{mask}"', fixture)
            self.assertIn(f'defvalue="{mask}" value="0"', fixture)

    def test_3210_buzzer_uses_mad2_pup_and_divider(self):
        self.assertIn("BEEP(config, m_buzzer)", self.driver)
        self.assertIn("BIT(m_regs[CONTROL], 5)", self.pup)
        self.assertIn("m_regs[BUZZER_DIVIDER_MSB]", self.pup)
        self.assertIn("m_regs[BUZZER_DIVIDER_LSB]", self.pup)
        self.assertIn("13'000'000 / divider", self.pup)
        self.assertIn('logerror("buzzer:', self.pup)

    def test_charger_input_updates_vchar_and_latches_both_edges(self):
        body = self.driver.split("INPUT_CHANGED_MEMBER( nokia_dct3_state::charger_irq )", 1)[1]
        body = body.split("static INPUT_PORTS_START", 1)[0]
        self.assertIn("set_charger_input(m_product.ccont_board.vchar_channel", body)
        self.assertIn("m_product.ccont_board.vchar_connected_raw", body)
        self.assertNotIn("latch_irq_sources", body)
        self.assertNotIn("if (newval)", body)

        ccont_body = self.ccont.split(
            "void nokia_ccont_device::set_charger_input", 1
        )[1].split("void nokia_ccont_device::select_w", 1)[0]
        self.assertIn("set_adc_source(channel, connected ? vchar : 0)", ccont_body)
        self.assertIn("latch_irq_sources(0x08)", ccont_body)

    def test_charger_wake_resets_the_digital_baseband_domain(self):
        ccont_body = self.ccont.split(
            "void nokia_ccont_device::set_charger_input", 1
        )[1].split("void nokia_ccont_device::select_w", 1)[0]
        self.assertIn("connected && !m_charger_connected && !m_powered", ccont_body)
        self.assertIn("RESET_CHARGER", ccont_body)
        self.assertIn("m_power_cb(1)", ccont_body)

        power_body = self.driver.split("void nokia_dct3_state::reset_digital_baseband", 1)[1]
        power_body = power_body.split("void nokia_dct3_state::sim_irq_w", 1)[0]
        for device in (
            "m_maincpu", "m_mad2", "m_gensio", "m_mbus", "m_dspif",
            "m_dsp_hle", "m_external_service_peer", "m_simi", "m_sim_card",
            "m_lcd", "m_radio_peer",
        ):
            self.assertIn(f"{device}->reset()", power_body)
        self.assertNotIn("m_gsm_network->reset()", power_body)
        self.assertIn("immutable cell data", power_body)
        self.assertIn("machine_reset()", power_body)
        self.assertIn("m_kbgpio->clear_power_on_latch()", power_body)

        callback = self.driver.split("void nokia_dct3_state::ccont_power_w", 1)[1]
        callback = callback.split("void nokia_dct3_state::reset_digital_baseband", 1)[0]
        self.assertIn("reset_digital_baseband()", callback)


if __name__ == "__main__":
    unittest.main()
