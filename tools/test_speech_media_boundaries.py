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
        self.assertIn("attotime::from_msec(20)", dsp)
        tick = dsp[
            dsp.index("TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::speech_tick)"):
            dsp.index("void nokia_dsp_hle_device::drain_responses")
        ]
        self.assertIn("submit_uplink_speech", tick)
        self.assertIn("take_downlink_speech", tick)
        self.assertIn("transfer_frame_block", tick)
        self.assertIn("m_radio_peer->speech_channel_active()", tick)
        self.assertIn("m_speech_control_mask", tick)
        self.assertNotIn("0x060b", tick.lower())
        self.assertIn("read_microphone_pcm", pcm)
        self.assertIn("write_earpiece_pcm", pcm)
        self.assertIn("520 kHz", pcm)
        header = (ROOT / "driver/nokia_mad2_pcm.h").read_text()
        self.assertIn("u32 m_data_clock = 0", header)
        self.assertIn("u32 m_frame_clock = 0", header)
        self.assertIn("m_data_clock % m_frame_clock", pcm)
        self.assertIn("m_frames_transferred += dsp_to_cobba.size()", pcm)
        self.assertIn("m_sync_clocks != 1", pcm)
        self.assertIn("m_word_clocks != 16", pcm)
        self.assertIn("clock_edge::falling", pcm)
        self.assertIn("BIT(transmitted, 15 - bit)", pcm)
        self.assertIn("m_idle_clocks_transferred", pcm)
        self.assertIn("m_sample_bits < 2 || m_sample_bits > 16", pcm)
        self.assertIn("const unsigned shift = 16 - m_sample_bits", pcm)
        self.assertIn("wire_earpiece", pcm)
        self.assertIn("wire_microphone", pcm)

    def test_cobba_owns_analogue_microphone_conversion_boundary(self):
        cobba = (ROOT / "driver/nokia_cobba.cpp").read_text()
        phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        self.assertIn(
            "stream_alloc(microphone_inputs, audio_outputs, pcm_rate)", cobba
        )
        self.assertIn("stream.get(m_selected_microphone, index)", cobba)
        self.assertIn('MICROPHONE(config, "microphone", 1)', phone)
        self.assertIn("nokia_cobba_device::mic2", phone)
        self.assertIn("nokia_cobba_device::ear", phone)
        self.assertIn("set_hle_internal_voice_route(", phone)
        self.assertIn("must never write these pins", phone)
        self.assertNotIn("block.fill(0)", cobba)

    def test_nse8_analogue_gains_are_product_configuration(self):
        cobba = (ROOT / "driver/nokia_cobba.cpp").read_text()
        phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        self.assertIn("m_microphone_gain_db / 20.0F", cobba)
        self.assertIn("m_earpiece_gain_db / 20.0F", cobba)
        self.assertIn("cobba_internal_microphone_gain_db = 18.0F", phone)
        self.assertIn("cobba_internal_earpiece_gain_db = -10.0F", phone)
        self.assertIn("cobba_pcm_sample_bits = 13", phone)
        self.assertIn("set_internal_voice_gains(", phone)
        self.assertIn("set_pcm_sample_bits(product.cobba_pcm_sample_bits)", phone)

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
        self.assertIn("sine_1khz", voice)
        self.assertIn("m_voice_peer->exchange", radio)
        self.assertNotIn("sine_1khz", dsp + cobba)


if __name__ == "__main__":
    unittest.main()
