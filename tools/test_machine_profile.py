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
        cls.external_service = (
            ROOT / "driver/nokia_external_service.cpp"
        ).read_text()
        cls.external_service_header = (
            ROOT / "driver/nokia_external_service.h"
        ).read_text()
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
                "keypad_wiring": "KEYPAD_NSE8",
                "simi_controller": "true",
                "synthetic_sim_card": "true",
                "dsp_service": "true",
                "external_service_transport": "true",
                "dsp_service_control":
                    "DSP_SERVICE_CONTROL_COMPACT",
                "external_service": "EXTERNAL_SERVICE_NSE8",
                "radio": "RADIO_NSE8",
                "dsp_speech_control":
                    "DSP_SPEECH_CONTROL_NSE8",
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
        self.assertIn(
            "set_external_service_enabled(product.external_service_transport)",
            apply,
        )
        self.assertIn("set_protocol_contract(product.radio)", apply)
        self.assertIn("set_enabled(product.radio.enabled())", apply)
        self.assertIn("set_service_delay_us(product.dsp_service_delay_us)", apply)
        self.assertIn("set_peer_poll_ms(product.dsp_peer_poll_ms)", apply)
        self.assertNotIn("nokia_env_u32", apply)
        self.assertNotIn("std::getenv", apply)

    def test_dsp_bootstrap_count_is_product_configuration(self):
        self.assert_profile_fields(
            "make_3310_config",
            {
                "dsp_bootstrap": "BOOTSTRAP_READY_58",
                "keypad_wiring": "KEYPAD_NHM5",
                "dsp_service_control":
                    "DSP_SERVICE_CONTROL_COMPACT",
                "external_service": "EXTERNAL_SERVICE_NHM5",
                "dsp_speech_control":
                    "DSP_SPEECH_CONTROL_NHM5",
            },
        )

    def test_6110_profile_contains_only_documented_hardware_contracts(self):
        self.assert_profile_fields(
            "make_6110_config",
            {
                "keypad_wiring": "KEYPAD_NSE3",
                "boot_rom_bypass": "false",
                "simi_controller": "true",
                "synthetic_sim_card": "true",
                "dsp_bootstrap":
                    "BOOTSTRAP_FLASH_VERIFICATION_PARTIAL",
                "cobba_pcm.data_clock": "1'000'000",
                "cobba_pcm.frame_clock": "8'000",
                "cobba_pcm.sample_bits": "13",
                "cobba_pcm.word_clocks": "16",
                "cobba_hle_voice.microphone": "nokia_cobba_device::mic2",
                "cobba_hle_voice.output": "nokia_cobba_device::ear",
                "pup_eeprom_scl_bit": "2",
                "dsp_speech_control":
                    "DSP_SPEECH_CONTROL_NSE3_COMMAND",
                "dsp_service_control":
                    "DSP_SERVICE_CONTROL_FRAMED",
            },
        )
        self.assertIn(
            "m_pup->set_eeprom_scl_bit(product.pup_eeprom_scl_bit);",
            self.driver,
        )
        body = self.function_body(
            "constexpr nokia_product_config make_6110_config",
            "constexpr nokia_product_config make_conservative_config",
        )
        for peer in ("dsp_service", "external_service"):
            field = (
                "external_service_transport"
                if peer == "external_service"
                else peer
            )
            self.assertNotIn(f"result.{field} = true;", body)
        self.assertNotIn("result.radio =", body)
        self.assertNotIn("DSP_SPEECH_CONTROL_NSE8", body)
        self.assertNotIn("DSP_SPEECH_CONTROL_NHM5", body)
        self.assertIn("2, BOOTSTRAP_FLASH_VERIFICATION_ROM3", body)
        self.assertNotIn("3, BOOTSTRAP_FLASH_VERIFICATION_ROM3", body)

        profile = self.driver.split(
            "void nokia_dct3_state::noki6110(machine_config &config)", 1
        )[1].split("void nokia_dct3_state::noki7110", 1)[0]
        for declaration in (
            "dct3_nse3_map",
            'INTEL_28F800B3T(config.replace(), "flash");',
            "I2C_24C64(config.replace(), m_eeprom);",
            "nokia_cobba_device::ear",
            "nokia_cobba_device::mic2",
            "apply_product_config(PRODUCT_6110);",
        ):
            self.assertIn(declaration, profile)

    def test_radio_protocol_is_one_typed_product_contract(self):
        self.assertIn(
            "nokia_radio_peer_device::protocol_contract radio",
            self.driver,
        )
        self.assertIn(
            "result.radio = RADIO_NSE8;",
            self.driver,
        )
        self.assertIn(
            "result.radio = RADIO_NHM5;",
            self.driver,
        )
        self.assertIn(
            "m_radio_peer->set_protocol_contract(product.radio);",
            self.driver,
        )
        for retired in (
            "wire_profile", "acquisition_profile", "radio_wire",
            "radio_acquisition", "bool radio_peer",
        ):
            self.assertNotIn(retired, self.driver)

    def test_6110_map_uses_nse3_physical_extents_without_later_rom2_alias(self):
        body = self.function_body(
            "void nokia_dct3_state::dct3_nse3_map",
            "INPUT_CHANGED_MEMBER",
        )
        self.assertIn("map(0x00100000, 0x0010ffff)", body)
        self.assertIn("map(0x00200000, 0x002fffff)", body)
        self.assertIn("map(0x00a00000, 0x00ffffff).unmaprw()", body)
        self.assertNotIn("rom2_mirror_r", body)
        self.assertNotIn("eeprom_r", body)

        reset = self.function_body(
            "void nokia_dct3_state::machine_reset",
            "TIMER_CALLBACK_MEMBER",
        )
        self.assertIn("if (m_product.boot_rom_bypass)", reset)
        self.assertIn("NOKIA_FLASH_ENTRY", reset)

        rom = self.driver.split("ROM_START( noki6110 )", 1)[1].split(
            "ROM_END", 1
        )[0]
        self.assertIn('ROM_REGION16_BE(0x100000, "flash"', rom)
        self.assertIn('ROM_REGION(0x02000, "eeprom"', rom)
        self.assertIn("nse3_rom3_f711604_boot.bin", rom)
        self.assertIn("6110_nse3_v406_rom3_candidate.fls", rom)
        self.assertIn("CRC(78f6dce9)", rom)
        self.assertIn("SHA1(5025a6ac3b4a13714211fde903f27f92cbb7c9b6)", rom)
        self.assertIn("6110_nse3_v548_rom3_ppmb.fls", rom)
        self.assertIn("CRC(451cde56)", rom)
        self.assertIn("SHA1(5768841c9eb39c744f4fa04f0485e4f9ad4553b3)", rom)
        self.assertIn("6110_nse3_v548_rom4_ppmb.fls", rom)
        self.assertIn("CRC(83f67ad4)", rom)
        self.assertIn("SHA1(3bcc5c93ec247c63490e134196aab98a4e60c184)", rom)
        self.assertIn("nse3_rom4_boot.bin", rom)
        self.assertEqual(rom.count("NO_DUMP"), 13)
        self.assertNotIn("MAD2_INTERNAL_ROMS", rom)

        start = self.function_body(
            "void nokia_dct3_state::machine_start",
            "void nokia_dct3_state::post_load",
        )
        self.assertIn("m_product.dsp_bootstrap_override", start)
        self.assertIn(
            "system_bios() == m_product.dsp_bootstrap_override->bios",
            start,
        )
        self.assertIn(
            "set_bootstrap_contract",
            start,
        )
        # The package-labelled ROM4 image is BIOS 3.  A third-party HLE can
        # advance both exact v5.48 images with a self-consistent 4/4 pair, so
        # progress through the equality gate does not identify the fitted DSP.
        self.assertNotIn("system_bios() == 3", start)
        self.assertNotIn("BOOTSTRAP_FLASH_VERIFICATION_ROM4", self.driver)

    def test_6110_has_ue4_keypad_instead_of_inherited_input_map(self):
        matrix = self.driver.split("static INPUT_PORTS_START( noki6110 )", 1)[1]
        matrix = matrix.split("INPUT_PORTS_END", 1)[0]
        for control in (
            "Flip", "Volume Up", "Volume Down", "Left Softkey",
            "Right Softkey", "Send", "End / Mode", "Up", "Down",
        ):
            self.assertIn(f'PORT_NAME("{control}")', matrix)
        for digit in "0123456789":
            self.assertIn(f'PORT_NAME("Keypad {digit}")', matrix)

    def test_3330_owns_observed_peer_adc_keypad_and_bootstrap_defaults(self):
        self.assert_profile_fields(
            "make_3330_config",
            {
                "simi_controller": "true",
                "synthetic_sim_card": "true",
                "dsp_service": "true",
                "external_service_transport": "true",
                "external_service": "EXTERNAL_SERVICE_NHM6",
                "dsp_service_control": "DSP_SERVICE_CONTROL_COMPACT",
                "radio": "RADIO_NHM6",
                "dsp_bootstrap": "BOOTSTRAP_READY_64",
                "keypad_wiring": "KEYPAD_NHM6",
                "ccont_board": "ADC_STANDARD",
            },
        )
        profile = self.driver.split("void nokia_dct3_state::noki3330(machine_config &config)", 1)[1]
        profile = profile.split("void nokia_dct3_state::noki3210", 1)[0]
        self.assertIn("apply_product_config(PRODUCT_3330);", profile)
        self.assertIn(
            "m_dsp_hle->set_bootstrap_contract(product.dsp_bootstrap);",
            self.driver,
        )

    def test_other_products_keep_conservative_defaults(self):
        body = self.function_body(
            "constexpr nokia_product_config make_conservative_config",
            "constexpr nokia_product_config PRODUCT_3210",
        )
        self.assertIn("nokia_product_config result;", body)
        self.assertIn("result.keypad_wiring = keypad_wiring;", body)
        self.assertIn("result.dsp_bootstrap = BOOTSTRAP_READY_64;", body)
        self.assertIn("bool simi_controller = false;", self.driver)
        self.assertIn("bool synthetic_sim_card = false;", self.driver)
        self.assertIn("bool dsp_service = false;", self.driver)
        self.assertIn("bool external_service_transport = false;", self.driver)
        self.assertIn(
            "nokia_radio_peer_device::protocol_contract radio;",
            self.driver,
        )

    def test_multi_model_frontiers_reset_the_correct_bios_nvram_namespace(self):
        self.assertIn("noki3310_3", self.makefile)
        self.assertIn("noki3330_1", self.makefile)
        prepare = self.makefile.split("prepare-run-nvram: build", 1)[1].split(
            "\nrun:", 1
        )[0]
        for phone in ("noki3310", "noki3330", "noki3410"):
            self.assertIn(f'[ "$(PHONE)" = "{phone}" ]', prepare)
        for store in ("flash", "sim_card", "eeprom"):
            self.assertIn(
                f'"$(RUN_NVRAM_DIR)/$(NVRAM_SYSTEM)/{store}"',
                prepare,
            )
        self.assertIn(
            "5871dd93badb1fa410dd22a6b7a12cf2d3b8f938e1514e989858dd45a2b35b74",
            self.makefile,
        )

    def test_external_service_application_is_one_typed_product_contract(self):
        self.assertIn(
            "struct application_contract", self.external_service_header
        )
        self.assertIn(
            "application_contract m_application;",
            self.external_service_header,
        )
        self.assertIn(
            "product.external_service",
            self.driver,
        )
        contracts = {
            "make_3210_config": "EXTERNAL_SERVICE_NSE8",
            "make_3310_config": "EXTERNAL_SERVICE_NHM5",
            "make_3330_config": "EXTERNAL_SERVICE_NHM6",
        }
        for builder, contract in contracts.items():
            body = self.function_body(
                f"constexpr nokia_product_config {builder}()",
                "constexpr nokia_product_config",
            )
            self.assertIn(f"result.external_service = {contract};", body)

        for builder in ("make_6110_config",):
            body = self.function_body(
                f"constexpr nokia_product_config {builder}()",
                "constexpr nokia_product_config",
            )
            self.assertNotIn("external_service =", body)

        self.assertIn("EXTERNAL_SERVICE_NSE8", self.driver)
        self.assertIn("EXTERNAL_SERVICE_NHM5", self.driver)
        self.assertIn("EXTERNAL_SERVICE_NHM6", self.driver)
        self.assertNotIn("application_profile", self.external_service)
        self.assertNotIn("application_profile", self.external_service_header)
        self.assertNotIn("nse8", self.external_service.lower())
        self.assertNotIn("nhm5", self.external_service.lower())

    def test_each_machine_applies_one_explicit_product_profile(self):
        expected = {
            "noki6110": ("dct3_base(config);", "PRODUCT_6110"),
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
        self.assertIn("m_simi->set_enabled(m_product.simi_controller &&", self.driver)
        self.assertIn(
            "m_simi->set_card_present(m_product.synthetic_sim_card &&",
            self.driver,
        )

    def test_3210_board_profile_routes_both_voltage_inputs(self):
        profile = self.driver.split(
            "constexpr nokia_ccont_board_profile ADC_3210", 1
        )[1].split("constexpr nokia_ccont_board_profile ADC_STANDARD", 1)[0]
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
                "simi_controller": "true",
                "synthetic_sim_card": "true",
                "dsp_service": "true",
                "external_service_transport": "true",
                "dsp_service_control": "DSP_SERVICE_CONTROL_COMPACT",
                "keypad_wiring": "KEYPAD_NHM2",
                "dsp_bootstrap": "BOOTSTRAP_PING_PONG",
                "dsp_service_delay_us": "50",
                "dsp_peer_poll_ms": "4",
                "flash_b3_block_lock": "true",
                "dsp_reset_wiring": "DSP_RESET_WIRING_3410",
                "display": "DISPLAY_3410",
                "ccont_board": "ADC_STANDARD",
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

    def test_missing_pcm_oracle_removes_only_the_physical_link(self):
        makefile = (ROOT / "Makefile").read_text()
        fixture = (ROOT / "fixtures/radio_pcm_missing/noki3210.cfg").read_text()
        self.assertIn("fixtures/radio_pcm_missing", makefile)
        self.assertIn('tag=":HWCFG"', fixture)
        self.assertIn('mask="32" defvalue="32" value="0"', fixture)
        for mask in ("1", "2", "4", "8", "16"):
            self.assertNotIn(f'tag=":HWCFG" type="CONFIG" mask="{mask}"', fixture)

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
