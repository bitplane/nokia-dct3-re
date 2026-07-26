from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class SpeechMediaBoundaryTests(unittest.TestCase):
    def test_radio_frames_follow_protocol_state(self):
        header = (ROOT / "driver/nokia_radio_peer.h").read_text()
        source = (ROOT / "driver/nokia_radio_peer.cpp").read_text()
        session = (ROOT / "driver/nokia_gsm_session.h").read_text()

        self.assertIn("speech_frame_octets = 33", header)
        self.assertIn("call_connected() const", session)
        gate = source[
            source.index("bool nokia_radio_peer_device::speech_channel_active() const"):
            source.index("bool nokia_radio_peer_device::speech_queue_push")
        ]
        self.assertIn("m_traffic_channel_active", gate)
        self.assertNotIn("call_connected", gate)
        self.assertNotIn("mcu_control", gate)

    def test_codec_has_only_frame_and_pcm_contracts(self):
        header = (ROOT / "driver/nokia_gsm_fr_codec.h").read_text()
        self.assertIn("pcm_samples = 160", header)
        self.assertIn("frame_octets = 33", header)
        self.assertNotIn("radio_peer", header)
        self.assertNotIn('#include "nokia_cobba', header)

    def test_dsp_clocks_codec_and_cobba_at_twenty_ms(self):
        dsp = (ROOT / "driver/nokia_dsp_hle.cpp").read_text()
        cobba = (ROOT / "driver/nokia_cobba.h").read_text()
        pcm = (ROOT / "driver/nokia_mad2_pcm.cpp").read_text()

        self.assertIn("pcm_rate = 8'000", cobba)
        self.assertIn("pcm_block_samples = 160", cobba)
        self.assertIn(
            "const attotime speech_period = m_mad2_pcm->block_period()", dsp
        )
        self.assertIn(
            "m_speech_timer->adjust(speech_period, 0, speech_period)", dsp
        )
        tick = dsp[
            dsp.index("TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::speech_tick)"):
            dsp.index("void nokia_dsp_hle_device::drain_responses")
        ]
        self.assertIn("submit_uplink_speech", tick)
        self.assertIn("take_downlink_speech", tick)
        self.assertIn("speech_delivery::none", tick)
        self.assertIn("speech_delivery::good", tick)
        self.assertIn(
            "radio_delivery !=\n"
            "\t\t\tnokia_radio_peer_device::speech_delivery::none",
            tick,
        )
        self.assertIn("transfer_frame_block", tick)
        self.assertIn("m_radio_peer->speech_channel_active()", tick)
        self.assertIn("m_speech_control_mask", tick)
        self.assertIn("m_mad2_pcm->link_ready()", tick)
        self.assertIn("const bool active = requested && pcm_link_ready", tick)
        rejected = tick.index("if (!m_mad2_pcm->transfer_frame_block")
        submit = tick.index("submit_uplink_speech")
        self.assertLess(rejected, submit)
        self.assertIn("return;", tick[rejected:submit])
        self.assertNotIn("0x060b", tick.lower())
        self.assertIn("read_microphone_pcm", pcm)
        self.assertIn("write_earpiece_pcm", pcm)
        self.assertIn("520 kHz", pcm)
        header = (ROOT / "driver/nokia_mad2_pcm.h").read_text()
        self.assertIn("u32 m_data_clock = 0", header)
        self.assertIn("u32 m_frame_clock = 0", header)
        self.assertIn(
            "nokia_cobba_device::pcm_block_samples, m_frame_clock", header
        )
        self.assertIn("m_data_clock % m_frame_clock", pcm)
        self.assertIn("m_frames_transferred += dsp_to_cobba.size()", pcm)
        self.assertIn("m_sync_clocks == 1", pcm)
        self.assertIn("m_word_clocks == 16", pcm)
        self.assertIn("clock_edge::falling", pcm)
        self.assertIn("BIT(transmitted, 15 - bit)", pcm)
        self.assertIn("m_idle_clocks_transferred", pcm)
        self.assertIn("m_sample_bits >= 2 && m_sample_bits <= 16", pcm)
        self.assertIn("bool nokia_mad2_pcm_device::link_ready() const", pcm)
        self.assertIn("++m_transfer_failures", pcm)
        self.assertIn("const unsigned shift = 16 - m_sample_bits", pcm)
        self.assertIn("wire_earpiece", pcm)
        self.assertIn("wire_microphone", pcm)

    def test_cobba_owns_analogue_microphone_conversion_boundary(self):
        cobba = (ROOT / "driver/nokia_cobba.cpp").read_text()
        phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        self.assertIn(
            "stream_alloc(microphone_inputs, audio_outputs, pcm_rate)", cobba
        )
        self.assertIn("stream.get(m_hle_microphone, index)", cobba)
        base = phone[
            phone.index("void nokia_dct3_state::dct3_base"):
            phone.index("void nokia_dct3_state::noki3310")
        ]
        nse8 = phone[
            phone.index("void nokia_dct3_state::noki3210"):
            phone.index("void nokia_dct3_state::noki5210")
        ]
        self.assertNotIn('MICROPHONE(config, "microphone", 1)', base)
        self.assertNotIn("m_cobba->add_route", base)
        self.assertIn('MICROPHONE(config, "microphone", 1)', nse8)
        self.assertIn("nokia_cobba_device::mic2", nse8)
        self.assertIn("nokia_cobba_device::ear", nse8)
        self.assertIn("set_hle_voice_profile(", phone)
        self.assertIn("must never mutate this fallback", phone)
        self.assertNotIn("block.fill(0)", cobba)

    def test_nse8_analogue_gains_are_product_configuration(self):
        cobba = (ROOT / "driver/nokia_cobba.cpp").read_text()
        phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        self.assertIn("m_hle_microphone_gain_db / 20.0F", cobba)
        self.assertIn("m_hle_output_gain_db / 20.0F", cobba)
        self.assertIn("cobba_hle_voice.microphone_gain_db = 18.0F", phone)
        self.assertIn("cobba_hle_voice.output_gain_db = -10.0F", phone)
        self.assertIn("cobba_pcm.sample_bits = 13", phone)
        self.assertIn("set_hle_voice_profile(", phone)
        self.assertIn("set_pcm_sample_bits(product.cobba_pcm.sample_bits)", phone)

    def test_nhm5_topology_does_not_import_nse8_pcm_or_gains(self):
        phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        nhm5 = phone[
            phone.index("constexpr nokia_product_config make_3310_config()"):
            phone.index("constexpr nokia_product_config make_3330_config()")
        ]
        self.assertIn(
            "cobba_hle_voice.microphone = nokia_cobba_device::mic2", nhm5
        )
        self.assertIn(
            "cobba_hle_voice.output = nokia_cobba_device::ear", nhm5
        )
        self.assertNotIn("cobba_pcm.data_clock", nhm5)
        self.assertNotIn("cobba_pcm.frame_clock", nhm5)
        self.assertNotIn("cobba_pcm.sample_bits", nhm5)
        self.assertNotIn("microphone_gain_db", nhm5)
        self.assertNotIn("output_gain_db", nhm5)

    def test_audio_profiles_are_grouped_and_configuration_only(self):
        phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        pcm = (ROOT / "driver/nokia_mad2_pcm.h").read_text()
        cobba = (ROOT / "driver/nokia_cobba.h").read_text()
        runtime = "".join(
            (ROOT / path).read_text()
            for path in (
                "driver/nokia_dsp_hle.cpp",
                "driver/nokia_radio_peer.cpp",
                "driver/nokia_gsm_session.cpp",
            )
        )
        self.assertIn("struct bus_profile", pcm)
        self.assertIn("struct hle_voice_profile", cobba)
        self.assertIn("bus_profile cobba_pcm", phone)
        self.assertIn("hle_voice_profile cobba_hle_voice", phone)
        self.assertNotIn("cobba_pcm_data_clock", phone)
        self.assertNotIn("cobba_hle_voice_microphone", phone)
        self.assertEqual(phone.count("set_hle_voice_profile("), 1)
        self.assertNotIn("set_hle_voice_profile", runtime)

    def test_cobba_control_transport_is_separate_and_opaque(self):
        header = (ROOT / "driver/nokia_cobba.h").read_text()
        cobba = (ROOT / "driver/nokia_cobba.cpp").read_text()
        dsp = (ROOT / "driver/nokia_dsp_hle.cpp").read_text()
        for token in (
            "control_data_w(u16 data)",
            "control_select_w(u16 select)",
            "control_data_r() const",
            "std::array<u16, 16> m_control_registers",
        ):
            self.assertIn(token, header)
        self.assertIn("m_control_data_latch = data & 0x0fff", cobba)
        self.assertIn("m_control_address = select & 0x0f", cobba)
        self.assertIn("m_control_read = BIT(select, 4)", cobba)
        self.assertIn("m_control_registers[0x0d] = 0x000c", cobba)
        self.assertNotIn("control_select_w", dsp)
        control_write = cobba.split(
            "void nokia_cobba_device::control_select_w", 1
        )[1].split("u16 nokia_cobba_device::control_data_r", 1)[0]
        self.assertNotIn("m_hle_microphone", control_write)
        self.assertNotIn("m_hle_output", control_write)
        self.assertNotIn("gain", control_write)
        conformance = cobba.split(
            "u8 nokia_cobba_device::run_control_conformance_checks", 1
        )[1].split("bool nokia_cobba_device::write_earpiece_pcm", 1)[0]
        self.assertNotIn("m_hle_microphone", conformance)
        self.assertNotIn("m_hle_output", conformance)
        self.assertNotIn("gain", conformance)

    def test_remote_voice_source_stays_at_network_boundary(self):
        voice = (
            (ROOT / "driver/nokia_gsm_voice_peer.cpp").read_text()
            + (ROOT / "driver/nokia_gsm_voice_peer.h").read_text()
        )
        radio = (ROOT / "driver/nokia_radio_peer.cpp").read_text()
        dsp = (ROOT / "driver/nokia_dsp_hle.cpp").read_text()
        cobba = (ROOT / "driver/nokia_cobba.cpp").read_text()
        self.assertIn("nokia_gsm_fr_codec m_uplink_decoder", voice)
        self.assertIn("nokia_gsm_fr_codec m_downlink_encoder", voice)
        self.assertIn("nokia_gsm_fr_receiver m_uplink_receiver", voice)
        self.assertIn("m_uplink_decoder.snapshot()", voice)
        self.assertIn("m_downlink_encoder.snapshot()", voice)
        self.assertIn("m_uplink_receiver.snapshot()", voice)
        self.assertIn("const speech_frame *uplink", voice)
        self.assertIn("sine_1khz", voice)
        self.assertIn("m_voice_peer->exchange", radio)
        self.assertIn("m_voice_peer->start_call()", radio)
        self.assertNotIn("sine_1khz", dsp + cobba)

    def test_layer1_has_an_independent_tdma_burst_clock(self):
        l1 = (
            (ROOT / "driver/gsm_tch_f_l1.cpp").read_text()
            + (ROOT / "driver/gsm_tch_f_l1.h").read_text()
        )
        radio = (ROOT / "driver/nokia_radio_peer.cpp").read_text()
        dsp = (ROOT / "driver/nokia_dsp_hle.cpp").read_text()
        self.assertIn("diagonal_transmitter", l1)
        self.assertIn("diagonal_receiver", l1)
        self.assertIn("full_rate_slot", l1)
        self.assertIn("substitute_facch", l1)
        self.assertIn("interleave_sacch", l1)
        self.assertIn("attotime::from_ticks(60, 13'000)", radio)
        burst = radio[
            radio.index(
                "TIMER_CALLBACK_MEMBER(nokia_radio_peer_device::burst_tick)"
            ):
            radio.index("const char *nokia_radio_peer_device::phase_name")
        ]
        self.assertIn("pack_normal_burst", burst)
        self.assertIn("m_network_receiver.receive", burst)
        self.assertIn("m_handset_receiver.receive", burst)
        self.assertNotIn("from_msec(20)", burst)
        self.assertNotIn("from_msec(20)", dsp)
        self.assertIn("m_mad2_pcm->block_period()", dsp)
        self.assertIn("m_speech_codec.snapshot()", dsp)
        self.assertIn("m_speech_codec.restore(m_speech_codec_state)", dsp)
        self.assertIn("STRUCT_MEMBER(m_speech_codec_state.channels, dp0)", dsp)
        self.assertIn("prepare_l1_save", radio)
        self.assertIn("restore_l1_block_kinds", radio)
        self.assertIn("STRUCT_MEMBER(state.previous, data)", radio)
        self.assertIn("STRUCT_MEMBER(state.bursts, data)", radio)
        self.assertNotIn(
            "FUNC(nokia_radio_peer_device::reset_l1_pipeline), this", radio
        )


if __name__ == "__main__":
    unittest.main()
