// license:BSD-3-Clause
// copyright-holders:Gaz

// Isolated original MU4 routine execution, not a DA150 or handset boot profile.
#include "emu.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "machine/nandflash.h"
#include "tms320c54x_dma.h"
#include "tms320c54x_mcbsp.h"
#include "tlv320aic23.h"
#include <array>
#include <vector>
#include <sstream>
#include <fstream>

namespace {
class mu4_storage_test_state : public driver_device
{
public:
	mu4_storage_test_state(machine_config const &config, device_type type, char const *tag)
				: driver_device(config, type, tag), m_cpu(*this, "cpu"), m_dma(*this, "dma"), m_mcbsp(*this, "mcbsp1"), m_mcbsp0(*this, "mcbsp0"), m_mcbsp2(*this, "mcbsp2"), m_program_ram(*this, "program_ram"), m_dma_sink(*this, "dma_sink"), m_nand(*this, "nand"), m_cinit(*this, "cinit"),
		  m_initdisk(*this, "initdisk"), m_disk_cinit(*this, "disk_cinit"), m_disk_helpers(*this, "disk_helpers"),
		  m_disk_vectors(*this, "disk_vectors"), m_segment(*this, "segment") { }
	void test(machine_config &config)
	{
		// Test clock only; DA150 PLL/physical memory mapping is not claimed here.
		TMS320C54X(config, m_cpu, 13'000'000);
		m_cpu->set_extended_program(true);
		m_cpu->set_addrmap(AS_PROGRAM, &mu4_storage_test_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &mu4_storage_test_state::data_map);
		m_cpu->set_addrmap(AS_IO, &mu4_storage_test_state::io_map);
		m_cpu->bio_in_cb().set(FUNC(mu4_storage_test_state::bio_r));
		TMS320C54X_DMA(config, m_dma, 13'000'000);
		m_dma->set_cpu(m_cpu);
		m_dma->set_per_channel_reload(true);
		m_dma->completion_cb().set(FUNC(mu4_storage_test_state::dma_complete));
		TMS320C54X_MCBSP(config, m_mcbsp, 13'000'000);
		m_mcbsp->tx_word_cb().set(FUNC(mu4_storage_test_state::serial_tx));
		m_mcbsp->tx_bit_cb().set(FUNC(mu4_storage_test_state::serial_tx_bit));
		m_mcbsp->tx_irq_cb().set(FUNC(mu4_storage_test_state::serial_tx_irq));
		m_mcbsp->tx_event_cb().set([this](int state) { m_dma->sync_w(6, state); });
		TMS320C54X_MCBSP(config, m_mcbsp0, 13'000'000);
		TMS320C54X_MCBSP(config, m_mcbsp2, 13'000'000);
		m_mcbsp2->tx_irq_cb().set([this](int state) {
			if (m_phase == 30 && system_bios() >= 9) { if (state) ++m_command_tx_irqs; m_cpu->set_input_line(7, state); }
		});
		m_mcbsp2->tx_bit_cb().set([this](int state) {
			m_command_wire_byte = (m_command_wire_byte << 1) | (state ? 1 : 0);
			if (++m_command_wire_bit_count == 8) { m_command_wire_decoded.push_back(m_command_wire_byte); m_command_wire_bit_count = m_command_wire_byte = 0; }
		});
		m_mcbsp2->tx_word_cb().set(FUNC(mu4_storage_test_state::command_output));
		m_mcbsp2->rx_event_cb().set([this](int state) { m_command_rx_ready = bool(state); });
		m_mcbsp2->rx_irq_cb().set([this](int state) {
			if (m_phase == 30 && system_bios() >= 11) { if (state) ++m_command_rx_irqs; m_cpu->set_input_line(6, state); }
		});
		m_mcbsp0->tx_word_cb().set(FUNC(mu4_storage_test_state::stream_tx));
		m_mcbsp0->tx_bit_cb().set([this](int state) {
			if ((m_phase >= 51 && m_phase <= 53) || m_phase == 66 || m_phase == 67 || (m_phase >= 70 && m_phase <= 73)) m_external_bits.push_back(state);
			subdevice<tlv320aic23_device>("codec")->din_w(state);
		});
		m_mcbsp0->tx_event_cb().set([this](int state) { m_dma->sync_w(2, state); });
		m_mcbsp0->tx_irq_cb().set([this](int state) { if (state) ++m_external_tx_irqs; m_cpu->set_input_line(5, state); });
		m_mcbsp0->rx_event_cb().set([this](int state) { m_dma->sync_w(1, state); });
		m_mcbsp0->rx_irq_cb().set([this](int state) { if (state) ++m_rx_irqs; m_cpu->set_input_line(4, state); });
		TLV320AIC23(config, "codec", 11'995'200);
		subdevice<tlv320aic23_device>("codec")->bclk_cb().set([this](int state) {
			if (m_phase >= 54 && m_phase <= 57) { m_codec_edges.push_back(state); if (state) ++m_codec_clocks; }
			m_mcbsp0->tx_clock_w(state); m_mcbsp0->rx_clock_w(state);
		});
		subdevice<tlv320aic23_device>("codec")->frame_cb().set([this](int state) {
			if (m_phase >= 54 && m_phase <= 57) { m_codec_edges.push_back(2 + state); if (state) m_codec_frames.push_back(m_codec_clocks); }
			m_mcbsp0->tx_frame_w(state); m_mcbsp0->rx_frame_w(state);
		});
		// Explicit converted-sample bench input, not a modeled analog ADC.
		subdevice<tlv320aic23_device>("codec")->converted_adc_cb().set([](offs_t channel) -> u16 { return channel ? 0x5678 : 0x1234; });
		subdevice<tlv320aic23_device>("codec")->dout_cb().set([this](int state) { m_mcbsp0->rx_data_w(state); });
		subdevice<tlv320aic23_device>("codec")->din_word_cb().set([this](offs_t channel, u16 value) {
			if (m_phase == 30 && system_bios() == 20 && m_stream_words)
			{
				if (!m_continuous_tx_count) fatalerror("MU4 codec DIN word has no observed transmitter word");
				if (channel != (m_continuous_din_words & 1) || value != m_continuous_tx[m_continuous_tx_head])
					fatalerror("MU4 continuous codec DIN mismatch word=%u channel=%u actual=%04x expected=%04x",
						m_continuous_din_words, unsigned(channel), value, m_continuous_tx[m_continuous_tx_head]);
				m_continuous_tx_head = (m_continuous_tx_head + 1) % m_continuous_tx.size();
				--m_continuous_tx_count;
				++m_continuous_din_words;
				if (value) ++m_continuous_nonzero_words;
			}
			if (m_phase >= 54 && m_phase <= 57)
			{
				if (channel != (m_codec_din_words.size() & 1)) fatalerror("MU4 codec DIN channel ordering mismatch");
				m_codec_din_words.push_back(value);
			}
			if (m_phase == 30 && m_native_din_words < m_native_tx_words.size())
			{
				if (channel != (m_native_din_words & 1) || value != m_native_tx_words[m_native_din_words])
					fatalerror("MU4 native codec DIN does not match transmitted word %u", m_native_din_words);
				++m_native_din_words;
				finish_stream_if_ready();
			}
		});
		SAMSUNG_K9K1208U0A(config, m_nand);
		m_nand->rnb_wr_callback().set(FUNC(mu4_storage_test_state::ready_w));
	}
private:
	required_device<tms320c54x_device> m_cpu;
	required_device<tms320c54x_dma_device> m_dma;
	required_device<tms320c54x_mcbsp_device> m_mcbsp;
	required_device<tms320c54x_mcbsp_device> m_mcbsp0;
	required_device<tms320c54x_mcbsp_device> m_mcbsp2;
	std::vector<u16> m_serial_tx_words;
	std::vector<int> m_serial_tx_bits;
	unsigned m_serial_saved_bits = 0;
	unsigned m_serial_tx_irqs = 0;
	std::vector<u16> m_dma_output;
	unsigned m_dma_completions = 0;
	unsigned m_rx_dma_completions = 0, m_rx_irqs = 0;
	void dma_complete(u8 channel)
	{
		if (channel != 2 && channel != 3) fatalerror("MU4 unexpected DMA completion channel");
		if (channel == 3) ++m_dma_completions;
		else ++m_rx_dma_completions;
		unsigned const selection = (m_dma->read(0) >> 6) & 3;
		if (selection == 1 || selection == 2)
		{
			unsigned const line = channel == 2 ? 10 : 11;
			m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE);
		}
		finish_stream_if_ready();
	}
	unsigned m_stream_words = 0, m_stream_config_writes = 0;
	unsigned m_native_din_words = 0;
	std::array<u16, 4> m_continuous_tx{};
	unsigned m_continuous_tx_head = 0, m_continuous_tx_count = 0;
	unsigned m_continuous_din_words = 0, m_continuous_nonzero_words = 0;
	std::array<u16, 4> m_source_words{};
	unsigned m_source_head = 0, m_source_count = 0, m_source_reads = 0;
	std::array<u16, 256> m_verified_tone{};
	std::array<bool, 2> m_verified_tone_valid{};
	std::array<unsigned, 2> m_verified_tone_reads{};
	unsigned m_tone_dma_words = 0;
	std::array<bool, 256> m_tone_cleared{};
	unsigned m_tone_discarded_words = 0;
	unsigned m_native_dma_rx_vectors = 0, m_native_dma_tx_vectors = 0;
	unsigned m_native_dma_tx_handler = 0;
	unsigned m_native_stream_pending_sets = 0, m_native_stream_pending_clears = 0, m_native_stream_pending_reads = 0;
	unsigned m_native_dispatch_traces = 0, m_native_descriptor_traces = 0;
	unsigned m_native_stream_consumer_entries = 0;
	unsigned m_native_selection_traces[17] = {};
	unsigned m_native_selection_writes = 0;
	unsigned m_native_startup_fetches[4] = {};
	unsigned m_native_main_call_traces = 0;
	unsigned m_native_config_call_traces = 0;
	unsigned m_native_tail_data_reads = 0;
	unsigned m_native_lookup_operand_traces = 0;
	unsigned m_native_cache_traces[6] = {};
	unsigned m_native_startup_subcall_traces = 0;
	unsigned m_native_startup_return_traces = 0;
	unsigned m_native_consumer_state_traces = 0;
	unsigned m_native_consumer_paths[6] = {};
	unsigned m_native_consumer_mode_writes = 0;
	unsigned m_native_command_paths[6] = {};
	unsigned m_measurement_blocks = 0;
	std::array<u32, 2> m_measurement_expected{};
	std::array<unsigned, 2> m_measurement_samples{};
	unsigned m_measurement_factor = 1;
	std::array<u16, 128> m_tone_expected{};
	std::array<u16, 2> m_tone_phase{};
	u16 m_tone_buffer = 0;
	unsigned m_tone_blocks = 0, m_tone_phase_wraps = 0;
	unsigned m_native_command_cursor = 0, m_native_command_waits = 0;
	unsigned m_command_tx_irqs = 0, m_command_wire_bits = 0;
	unsigned m_command_ack_cursor = 0, m_command_wire_bit_count = 0;
	bool m_command_wire_clock = false;
	bool m_command_ack_pending = false;
	u8 m_command_ack_token = 0, m_command_wire_byte = 0;
	u16 m_command_tx_register = 0;
	std::vector<u16> m_command_tx_words, m_command_wire_decoded;
	std::array<u16, 64> m_saved_command_tx{}, m_saved_command_decoded{};
	unsigned m_saved_command_tx_count = 0, m_saved_command_decoded_count = 0;
	bool m_command_rx_ready = false, m_command_rx_busy = false, m_command_rx_clock = true;
	u8 m_command_rx_byte = 0;
	unsigned m_command_rx_phase = 0, m_command_rx_irqs = 0;
	// The replay controller/reference deliberately survive restoration of the DUT.
	unsigned m_native_replay_leg = 0;
	// Test supervisor, not DUT state: survives the requested machine reset.
	unsigned m_native_reset_leg = 0;
	unsigned m_native_reset_settings_mask = 0;
	unsigned m_native_bootstrap_entries = 0, m_native_bootstrap_resident_entries = 0;
	unsigned m_native_bootstrap_paths[10] = {};
	bool m_native_runtime_observers_installed = false;
	unsigned m_native_runtime_reads[9] = {}, m_native_runtime_writes[9] = {};
	std::stringstream m_native_checkpoint, m_native_reference;
	unsigned m_native_settings_call_traces = 0;
	unsigned m_native_metadata_call_traces = 0;
	unsigned m_native_allocation_call_traces = 0;
	unsigned m_native_cache_update_traces = 0;
	unsigned m_native_cache_mode_traces[6] = {};
	bool m_native_settings_buffer_active = false;
	unsigned m_native_buffer_calls[20] = {}, m_native_buffer_returns[20] = {};
	static constexpr offs_t buffer_calls[] = {
		0x3b940, 0x3b95b, 0x3b973, 0x3b99b, 0x3b9c4, 0x3b9d3,
		0x3b9f2, 0x3b9fb, 0x3ba17, 0x3ba6a, 0x3ba78, 0x3ba7a,
		0x3ba84, 0x3ba88, 0x3ba8a, 0x3ba95, 0x3baa7, 0x3bab5,
		0x3babb, 0x3bacb
	};
	bool m_native_worker_window_started = false;
	attotime m_native_worker_window_start;
	std::vector<u16> m_native_tx_words;
	std::vector<u16> m_codec_din_words, m_codec_saved_din;
	unsigned m_codec_rx_snapshot_count = 0;
	unsigned native_stream_target() const { return system_bios() >= 3 ? 1024 : 128; }
	unsigned native_worker_tail_ms() const { return system_bios() >= 7 ? 20000 : system_bios() == 6 ? 5000 : system_bios() == 5 ? 1000 : 100; }
	void trace_native_iterator(char const *point)
	{
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		logerror("mu4_native_iterator: point=%s source142c=%04x,%04x position145c=%04x,%04x advance145e=%04x reads=%u strobes=%u\n",
			point, data.read_word(0x142c), data.read_word(0x142d), data.read_word(0x145c),
			data.read_word(0x145d), data.read_word(0x145e), m_data_reads, unsigned(m_writes.size()));
		if (m_writes.size() >= 8)
			logerror("mu4_native_iterator_bus: point=%s suffix=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x\n", point,
				m_writes[m_writes.size()-8], m_writes[m_writes.size()-7], m_writes[m_writes.size()-6], m_writes[m_writes.size()-5],
				m_writes[m_writes.size()-4], m_writes[m_writes.size()-3], m_writes[m_writes.size()-2], m_writes[m_writes.size()-1]);
	}
	void finish_stream_if_ready()
	{
		if (original_bootstrap_profile()) return; // Fixed-window bootstrap observation, not a routine gate.
		if (m_phase == 30 && system_bios() >= 2 && m_stream_words >= native_stream_target() &&
			m_native_din_words >= native_stream_target() && m_dma_completions >= native_stream_target() / 128 &&
			m_rx_dma_completions >= native_stream_target() / 128)
		{
			if (system_bios() >= 4)
			{
				if (!m_native_worker_window_started)
				{
					m_native_worker_window_started = true;
					m_native_worker_window_start = machine().time();
					m_native_tail_data_reads = m_data_reads;
					trace_native_iterator("start");
					m_check->adjust(attotime::from_msec(native_worker_tail_ms()));
				}
			}
			else m_check->adjust(attotime::zero);
		}
	}
	unsigned m_codec_clocks = 0;
	std::vector<unsigned> m_codec_frames;
	std::vector<int> m_codec_edges, m_codec_saved_edges;
	attotime m_codec_snapshot_time;
	std::vector<u16> m_external_words;
	std::vector<int> m_external_bits;
	unsigned m_external_tx_irqs = 0;
	bool m_external_checkpoint_pending = false;
	void external_reg_w(u16 index, u16 value) { m_mcbsp0->control_w(0, index); m_mcbsp0->control_w(1, value); }
	u16 receive_status() { m_mcbsp0->control_w(0, 0); return m_mcbsp0->control_r(1); }
	void receive_setup(u16 rcr1, u16 rcr2, u16 pcr = 0, u16 spcr1 = 1)
	{
		external_reg_w(0, 0); external_reg_w(2, rcr1); external_reg_w(3, rcr2); external_reg_w(14, pcr);
		m_mcbsp0->rx_clock_w(!BIT(pcr, 0)); m_mcbsp0->rx_frame_w(BIT(pcr, 2));
		external_reg_w(0, spcr1);
	}
	void receive_bit(int bit, bool rising = false)
	{
		m_mcbsp0->rx_data_w(bit); m_mcbsp0->rx_clock_w(rising); m_mcbsp0->rx_clock_w(!rising);
	}
	void receive_word(u16 value, unsigned bits, unsigned delay = 0, bool inverted = false)
	{
		m_mcbsp0->rx_frame_w(!inverted);
		for (unsigned i = 0; i < delay; ++i) receive_bit(0, inverted);
		if (delay) m_mcbsp0->rx_frame_w(inverted);
		for (unsigned i = bits; i-- > 0; ) { receive_bit(BIT(value, i), inverted); m_mcbsp0->rx_frame_w(inverted); }
		receive_bit(0, inverted); // RBR -> DRR at the next sampling edge.
	}
	void external_clocks(unsigned count, bool inverted = true)
	{
		while (count--) { m_mcbsp0->tx_clock_w(!inverted); m_mcbsp0->tx_clock_w(inverted); }
	}
	void check_external_underrun(unsigned i)
	{
		static constexpr unsigned widths[] = {8, 12, 16};
		if (i < std::size(widths))
		{
			external_reg_w(1, 0); external_reg_w(4, i << 5); external_reg_w(5, 1); external_reg_w(14, 0);
			m_mcbsp0->tx_clock_w(0); m_mcbsp0->tx_frame_w(0);
			m_external_words.clear(); m_external_bits.clear(); external_reg_w(1, 1);
			unsigned const empty_irq = m_external_tx_irqs;
			m_mcbsp0->tx_frame_w(1); external_clocks(widths[i], false);
			m_mcbsp0->control_w(0, 1);
			if (m_external_words != std::vector<u16>({0}) || (m_mcbsp0->control_r(1) & 7) != 3 || m_external_tx_irqs != empty_irq)
				fatalerror("MU4 McBSP empty frame must transmit zeros without fabricating ready edges");
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->data_w(3, 0xa55a); m_mcbsp0->tx_frame_w(1);
			if ((m_mcbsp0->control_r(1) & 7) != 5) fatalerror("MU4 McBSP fresh transfer must deactivate XEMPTY");
			external_clocks(widths[i], false);
			unsigned const repeat_irq = m_external_tx_irqs;
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1);
			if ((m_mcbsp0->control_r(1) & 7) != 3) fatalerror("MU4 McBSP repeated DXR must retain empty status while shifting");
			external_clocks(widths[i], false);
			u16 const mask = (1U << widths[i]) - 1;
			if (m_external_words != std::vector<u16>({0, u16(0xa55a & mask), u16(0xa55a & mask)}) || m_external_tx_irqs != repeat_irq)
				fatalerror("MU4 McBSP underrun did not repeat old DXR without new ready events");
			for (unsigned bit = 0; bit < widths[i]; ++bit)
				if (m_external_bits[widths[i] * 2 + bit] != BIT(0xa55a, widths[i] - 1 - bit))
					fatalerror("MU4 McBSP underrun repeat bit order mismatch");
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->data_w(3, 0x1234);
			if ((m_mcbsp0->control_r(1) & 7) != 1) fatalerror("MU4 McBSP refill must not clear underflow before XSR transfer");
			m_mcbsp0->tx_frame_w(1); external_clocks(widths[i], false);
			if (m_external_words.back() != (0x1234 & mask) || m_external_tx_irqs != repeat_irq + 1)
				fatalerror("MU4 McBSP underrun recovery mismatch");
			return;
		}
		external_reg_w(1, 0); external_reg_w(4, 0x140);
		m_mcbsp0->tx_frame_w(0); external_reg_w(1, 1);
		m_external_words.clear(); m_external_bits.clear();
		m_mcbsp0->data_w(3, 0xa55a); m_mcbsp0->tx_frame_w(1); external_clocks(32, false);
		if (m_external_words != std::vector<u16>({0xa55a}) || m_external_bits.size() != 16 || (m_mcbsp0->control_r(1) & 7) != 3)
			fatalerror("MU4 McBSP mid-frame underflow must stop shifting until the next frame");
		m_mcbsp0->data_w(3, 0x1234); external_clocks(16, false);
		if (m_external_words.size() != 1) fatalerror("MU4 McBSP underrun refill resumed without frame sync");
		m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1); external_clocks(1, false);
		m_mcbsp0->data_w(3, 0xabcd); external_clocks(31, false);
		if (m_external_words != std::vector<u16>({0xa55a, 0x1234, 0xabcd})) fatalerror("MU4 McBSP mid-frame underflow recovery mismatch");
		// The following save fixture repeats one complete 16-bit word.
		external_reg_w(4, 0x40);
		m_mcbsp0->data_w(3, 0x1234); m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1); external_clocks(16, false);
		logerror("mu4_mcbsp_underrun: PASS widths=8,12,16 empty_zero=1 repeat_dxr=1 xempty=1 ready_edges=1 recovery=1 midframe_wait=1\n");
	}
	void check_receive_overrun()
	{
		receive_setup(0, 0);
		unsigned const irqs = m_rx_irqs;
		receive_word(0x11, 8); receive_word(0x22, 8);
		if ((receive_status() & 7) != 3) fatalerror("MU4 McBSP RFULL asserted before three unread words");
		receive_word(0x33, 8); receive_word(0x44, 8);
		if ((receive_status() & 7) != 7 || m_rx_irqs != irqs + 1) fatalerror("MU4 McBSP overrun flag/ready edge mismatch");
		{ auto const disable = machine().disable_side_effects(); if (m_mcbsp0->data_r(1) != 0x11) fatalerror("MU4 McBSP overrun overwrote unread DRR"); }
		if ((receive_status() & 7) != 7 || m_mcbsp0->data_r(1) != 0x11 || (receive_status() & 7) != 1)
			fatalerror("MU4 McBSP overrun peek/clear mismatch");
		receive_bit(0);
		if (m_mcbsp0->data_r(1) != 0x22) fatalerror("MU4 McBSP overrun discarded buffered RBR");
		for (unsigned bit = 0; bit < 16; ++bit) receive_bit(0);
		if (receive_status() & 6) fatalerror("MU4 McBSP overrun recovered without a new frame");
		receive_word(0x55, 8);
		if (m_mcbsp0->data_r(1) != 0x55 || m_rx_irqs != irqs + 3) fatalerror("MU4 McBSP overrun frame recovery mismatch");
		receive_word(0x66, 8); receive_word(0x77, 8); receive_word(0x88, 8); external_reg_w(0, 0);
		if (receive_status() & 6) fatalerror("MU4 McBSP receiver reset did not clear RFULL/RRDY");
		logerror("mu4_mcbsp_overrun: PASS three_word_threshold=1 drr_retained=1 rbr_retained=1 rsr_loss=1 peek=1 read_clear=1 frame_recovery=1 reset=1\n");
	}
	void stream_tx(u16 value)
	{
		++m_stream_words;
		// TX reports the last launched bit before DIN samples it on the next rising edge.
		// Idle-line DIN words before this observable transmission are not stream evidence.
		if (m_phase == 30 && m_native_tx_words.size() < native_stream_target()) m_native_tx_words.push_back(value);
		if (m_phase == 30 && system_bios() == 20)
		{
			if (!m_source_count || value != m_source_words[m_source_head])
				fatalerror("MU4 transmitter/source mismatch word=%u source_reads=%u pending=%u actual=%04x expected=%04x pc=%06x",
					m_stream_words, m_source_reads, m_source_count, value, m_source_words[m_source_head], unsigned(m_cpu->pc()));
			m_source_head = (m_source_head + 1) % m_source_words.size();
			--m_source_count;
			if (m_continuous_tx_count == m_continuous_tx.size()) fatalerror("MU4 codec comparison queue overflow");
			m_continuous_tx[(m_continuous_tx_head + m_continuous_tx_count) % m_continuous_tx.size()] = value;
			++m_continuous_tx_count;
		}
		if ((m_phase >= 51 && m_phase <= 53) || m_phase == 66 || m_phase == 67 || (m_phase >= 70 && m_phase <= 73)) m_external_words.push_back(value);
		if (m_phase == 30 && m_stream_words <= 8) logerror("mu4_native_stream: word=%04x\n", value);
		finish_stream_if_ready();
	}
	void dma_output_w(u16 value) { m_dma_sink[0] = value; if (m_phase >= 38 && m_phase <= 50) m_dma_output.push_back(value); }
	void dma_reg_w(u16 index, u16 value) { m_dma->write(1, index); m_dma->write(3, value); }
	u16 dma_reg_r(u16 index) { m_dma->write(1, index); return m_dma->read(3); }
	// VC5410A IFR bit 11 is XINT1; its priority rank 14 is not a bit index.
	void serial_tx_irq(int value)
	{
		if (value) ++m_serial_tx_irqs;
		if (!(m_dma->read(0) & 0xc0)) m_cpu->set_input_line(11, value);
	}
	void serial_tx_bit(int value) { if (m_phase >= 31 && m_phase <= 37) m_serial_tx_bits.push_back(value); }
	void mcbsp_reg_w(u16 index, u16 value) { m_mcbsp->control_w(0, index); m_mcbsp->control_w(1, value); }
	u16 mcbsp_status() { m_mcbsp->control_w(0, 1); return m_mcbsp->control_r(1); }
	void serial_tx(u16 value)
	{
		if (m_phase == 30 && system_bios() >= 2) subdevice<tlv320aic23_device>("codec")->control_word_w(value);
		m_serial_tx_words.push_back(value);
		if (m_phase == 30 && m_serial_tx_words.size() <= 16) logerror("mu4_native_tx: word=%04x\n", value);
		// End the serial-setup fixture at its observed complete control stream,
		// before the separate streaming-profile acceptance.
		if (m_phase == 30 && system_bios() < 2 && m_serial_tx_words.size() == 6) m_check->adjust(attotime::zero);
	}
	required_shared_ptr<u16> m_program_ram;
	required_shared_ptr<u16> m_dma_sink;
	required_device<samsung_k9k1208u0a_device> m_nand;
	required_region_ptr<u16> m_cinit;
	required_region_ptr<u16> m_initdisk, m_disk_cinit, m_disk_helpers, m_disk_vectors;
	required_region_ptr<u8> m_segment;
	unsigned m_segment_cursor = 0, m_segment_waits = 0;
	unsigned m_file_source = 0, m_file_size = 0, m_file_cursor = 0, m_files_checked = 0;
	u16 m_file_marker = 0;
	std::vector<u16> m_verified_files;
	unsigned m_read_words = 0;
	std::stringstream m_saved_dma;
	u16 m_serial_index[2] = {}, m_serial_regs[2][32] = {}, m_serial_word = 0;
	bool m_serial_ready = false;
	unsigned m_serial_reads = 0;
	u8 m_direction = 0, m_gpio = 0;
	bool m_ready = true;
	unsigned m_bio_reads = 0, m_busy_reads = 0, m_data_reads = 0;
	attotime m_read_started, m_first_data;
	std::vector<u16> m_writes;
	emu_timer *m_check = nullptr;
	emu_timer *m_command = nullptr;
	emu_timer *m_command_clock = nullptr;
	emu_timer *m_command_receive_clock = nullptr;
	emu_timer *m_native_replay = nullptr;
	unsigned m_phase = 54;
	unsigned m_native_entry_reads = 0, m_native_far_reads = 0;
	unsigned m_native_mcbsp_trace = 0, m_native_mcbsp_polls = 0;
	unsigned m_native_adjacent_reads = 0;
	unsigned m_native_dma_writes = 0;
	u16 m_native_mcbsp_index = 0, m_native_mcbsp_status = 0;
	unsigned m_page = 0;
	static u16 pattern(unsigned index) { return u16(0x1234 + index * 37); }
	void program_map(address_map &map)
	{
		// This is a routine-test address space, not the unknown DA150 silicon map.
		map(0, 0x7fffff).ram().share("program_ram");
		map(0x0200, 0x0f32).rom().region("entry", 0);
		map(0x2080, 0x20ad).rom().region("trampolines", 0);
		map(0x20ae, 0x3679).rom().region("library", 0);
	}
	void data_map(address_map &map)
	{
		map(0, 0xffff).ram();
		map(0x0020, 0x0023).rw(m_mcbsp0, FUNC(tms320c54x_mcbsp_device::data_r), FUNC(tms320c54x_mcbsp_device::data_w));
		map(0x0031, 0x0031).r(FUNC(mu4_storage_test_state::serial_r));
		map(0x0033, 0x0033).rw(FUNC(mu4_storage_test_state::command_tx_r), FUNC(mu4_storage_test_state::command_tx_w));
		map(0x0034, 0x0035).rw(FUNC(mu4_storage_test_state::serial_config_r), FUNC(mu4_storage_test_state::serial_config_w));
		map(0x0038, 0x0039).rw(FUNC(mu4_storage_test_state::serial_config0_r), FUNC(mu4_storage_test_state::serial_config0_w));
		map(0x003c, 0x003d).rw(FUNC(mu4_storage_test_state::gpio_r), FUNC(mu4_storage_test_state::gpio_w));
		map(0x0040, 0x0043).rw(m_mcbsp, FUNC(tms320c54x_mcbsp_device::data_r), FUNC(tms320c54x_mcbsp_device::data_w));
		map(0x0048, 0x0049).rw(m_mcbsp, FUNC(tms320c54x_mcbsp_device::control_r), FUNC(tms320c54x_mcbsp_device::control_w));
		map(0x0054, 0x0057).r(m_dma, FUNC(tms320c54x_dma_device::read)).w(FUNC(mu4_storage_test_state::dma_w));
		map(0x6200, 0x6200).ram().w(FUNC(mu4_storage_test_state::dma_output_w)).share("dma_sink");
	}
	void dma_w(offs_t offset, u16 value)
	{
		if (m_phase == 30 && m_native_far_reads && m_native_dma_writes++ < 128)
			logerror("mu4_native_dma: offset=%u index=%04x value=%04x pc=%06x\n",
				unsigned(offset), m_dma->read(1), value, unsigned(m_cpu->pc()));
		m_dma->write(offset, value);
	}
	// Loader and legacy profiles use word ingress; the resident pins profile uses McBSP2.
	u16 serial_r()
	{
		if (m_phase == 30 && system_bios() >= 11)
		{
			if (!machine().side_effects_disabled())
			{
				if (!m_command_rx_ready) fatalerror("MU4 native DRR read before RRDY");
				++m_serial_reads;
			}
			return m_mcbsp2->data_r(1);
		}
		if (!m_serial_ready) fatalerror("MU4 serial read without delivered word");
		m_serial_ready = false;
		m_serial_regs[0][0] &= ~u16(2);
		++m_serial_reads;
		return m_serial_word;
	}
	u16 serial_config_r(offs_t offset)
	{
		if (m_phase == 30 && system_bios() >= 11) return m_mcbsp2->control_r(offset);
		if (offset && m_phase == 30 && system_bios() >= 9 && m_serial_index[0] == 1) return m_mcbsp2->control_r(1);
		return offset ? m_serial_regs[0][m_serial_index[0]] : m_serial_index[0];
	}
	void serial_config_w(offs_t offset, u16 value)
	{
		if (offset) m_serial_regs[0][m_serial_index[0]] = value;
		else m_serial_index[0] = value & 31;
		if (m_phase == 30 && system_bios() >= 9) m_mcbsp2->control_w(offset, value);
	}
	u16 command_tx_r() { return m_command_tx_register; } // Bench register history; not a recovered DXR read contract.
	void command_tx_w(u16 value)
	{
		m_command_tx_register = value;
		if (m_phase != 30 || system_bios() < 9) return;
		m_mcbsp2->data_w(3, value);
		if (!m_command_clock->enabled() || m_command_clock->remaining() == attotime::never)
			m_command_clock->adjust(attotime::from_usec(5));
	}
	void command_output(u16 value)
	{
		if (m_command_wire_decoded.size() != m_command_tx_words.size() + 1 || m_command_wire_decoded.back() != value)
			fatalerror("MU4 serial pin decoder disagrees with McBSP transmit word");
		m_command_tx_words.push_back(value);
		if (measurement_profile())
		{
			if (m_command_tx_words.size() != 12) return;
			u8 checksum = 0;
			for (unsigned i = 3; i < 10; ++i) checksum ^= m_command_tx_words[i];
			if (m_command_tx_words[3] != 0x1e || m_command_tx_words[4] != 3 || m_command_tx_words[5] != 0xaa ||
				m_command_tx_words[6] != 1 || m_command_tx_words[7] != 0x68 || m_command_tx_words[10] != checksum || m_command_tx_words[11] != 0x55)
				fatalerror("MU4 measurement response framing differs from original contract");
			m_command_ack_token = m_command_tx_words[9];
			m_command_ack_pending = true;
			m_command->adjust(attotime::from_msec(1));
			return;
		}
		if (system_bios() < 10 || m_command_tx_words.size() != 14) return;
		static const std::vector<u16> expected = {0x7f, 1, 0x55, 0x1e, 5, 0xaa, 1, 0x71, 1, 0, 0, 0x80, 0x40, 0x55};
		if (m_command_tx_words != expected) fatalerror("MU4 original status response differs from decoded contract");
		m_command_ack_token = m_command_tx_words[11];
		m_command_ack_pending = true;
		m_command->adjust(attotime::from_msec(1));
	}
	TIMER_CALLBACK_MEMBER(command_clock)
	{
		m_command_wire_clock = !m_command_wire_clock;
		if (m_command_wire_clock && !m_command_wire_bits && BIT(m_gpio, 4))
		{
			m_mcbsp2->tx_frame_w(1);
			m_command_wire_bits = 8;
		}
		m_mcbsp2->tx_clock_w(m_command_wire_clock);
		if (m_command_wire_clock && m_command_wire_bits)
		{
			m_mcbsp2->tx_frame_w(0);
			--m_command_wire_bits;
		}
		// Explicit 100 kbit/s bench source, not an asserted MU4 board clock.
		if (m_command_wire_bits || BIT(m_gpio, 4) || m_command_wire_clock)
			m_command_clock->adjust(attotime::from_usec(5));
	}
	u16 serial_config0_r(offs_t offset) { return m_mcbsp0->control_r(offset); }
	void serial_config0_w(offs_t offset, u16 value)
	{
		if (m_phase == 30 && m_native_far_reads && m_stream_config_writes++ < 64)
			logerror("mu4_native_stream_config: offset=%u index=%04x value=%04x pc=%06x\n",
				unsigned(offset), m_mcbsp0->control_r(0), value, unsigned(m_cpu->pc()));
		m_mcbsp0->control_w(offset, value);
	}
	void deliver_serial(u8 value)
	{
		if (m_serial_ready || !(m_serial_regs[0][0] & 1)) fatalerror("MU4 serial fixture not receive-ready");
		m_serial_word = value;
		m_serial_ready = true;
		m_serial_regs[0][0] |= 2;
		m_cpu->set_input_line(6, ASSERT_LINE);
		m_cpu->set_input_line(6, CLEAR_LINE);
	}
	void command_receive(u8 value)
	{
		if (system_bios() < 11) { deliver_serial(value); return; }
		m_command_rx_byte = value;
		m_command_rx_busy = true;
		m_command_rx_phase = 0;
		m_command_rx_clock = true;
		m_mcbsp2->rx_data_w(0); m_mcbsp2->rx_frame_w(1); m_mcbsp2->rx_clock_w(1);
		m_command_receive_clock->adjust(attotime::from_usec(5));
	}
	TIMER_CALLBACK_MEMBER(command_receive_clock)
	{
		m_command_rx_clock = !m_command_rx_clock;
		if (!m_command_rx_clock && m_command_rx_phase >= 1 && m_command_rx_phase <= 8)
			m_mcbsp2->rx_data_w(BIT(m_command_rx_byte, 8 - m_command_rx_phase));
		if (m_command_rx_phase) m_mcbsp2->rx_frame_w(0);
		m_mcbsp2->rx_clock_w(m_command_rx_clock);
		if (!m_command_rx_clock) ++m_command_rx_phase;
		if ((system_bios() == 12 || system_bios() == 19) && !m_native_replay_leg && m_native_command_cursor == 1 && m_command_rx_phase == 4 && !m_command_rx_clock)
			// Let synchronized input events settle without advancing to the next bit.
			m_native_replay->adjust(attotime::from_nsec(1));
		if ((system_bios() == 14 || system_bios() == 18) && !m_native_reset_leg && m_native_command_cursor == 1 && m_command_rx_phase == 4 && !m_command_rx_clock)
			m_native_replay->adjust(attotime::from_nsec(1));
		// One delay cycle, eight data cycles, then RBR-to-DRR publication.
		if (m_command_rx_phase == 10 && m_command_rx_clock) m_command_rx_busy = false;
		else m_command_receive_clock->adjust(attotime::from_usec(5));
	}
	TIMER_CALLBACK_MEMBER(native_replay_checkpoint)
	{
		if (system_bios() == 14 || system_bios() == 18)
		{
			if (m_phase != 30 || m_native_reset_leg || !m_command_rx_busy || m_command_rx_phase != 4 || m_command_rx_clock ||
				m_command_rx_irqs || !m_command_tx_words.empty() || !machine().scheduler().can_save())
				fatalerror("MU4 reset fixture is not at the settled first-byte boundary");
			m_native_replay->adjust(attotime::never);
			m_native_reset_leg = 1;
			// Retain NAND. The full-startup profile replays the original upload
			// and reset-vector software; the legacy diagnostic uses routine ABIs.
			m_files_checked = 0;
			m_verified_files.clear();
			m_phase = system_bios() == 18 ? 74 : 19;
			logerror("mu4_native_reset_request: mid_rx_byte=1 data_bits=3 rx_irqs=0 tx_words=0\n");
			machine().schedule_soft_reset();
			return;
		}
		if (m_native_replay_leg == 2)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 native restore has pending synchronized inputs");
			// Mark this callback modified before restoration. The restored timer
			// state then survives callback cleanup without rearming the DUT timer.
			m_native_replay->adjust(attotime::never);
			m_native_checkpoint.clear(); m_native_checkpoint.seekg(0);
			if (machine().save().read_stream(m_native_checkpoint) != STATERR_NONE)
				fatalerror("MU4 native checkpoint restore failed");
			m_native_replay_leg = 3;
			logerror("mu4_native_replay_restore: mid_byte=1 original_end_timer=1\n");
			return;
		}
		if (m_phase != 30 || m_native_replay_leg || !m_command_rx_busy || m_command_rx_phase != 4 || m_command_rx_clock || !machine().scheduler().can_save())
			fatalerror("MU4 native replay checkpoint is not a settled mid-byte boundary");
		m_native_replay->adjust(attotime::never);
		m_native_checkpoint.str(""); m_native_checkpoint.clear();
		if (machine().save().write_stream(m_native_checkpoint) != STATERR_NONE)
			fatalerror("MU4 native mid-byte checkpoint failed");
		m_native_replay_leg = 1;
		logerror("mu4_native_replay_checkpoint: rx_phase=4 data_bits=3 rx_irqs=%u request_cursor=%u\n", m_command_rx_irqs, m_native_command_cursor);
	}
	void observe_native_tone(offs_t address)
	{
		if (m_phase != 30 || machine().side_effects_disabled() || m_cpu->pc() != address + 1) return;
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		if (address == 0x2876a)
		{
			m_tone_buffer = data.read_word(0xbb86) ? 0xbaf9 : 0xba79;
			for (unsigned channel = 0; channel < 2; ++channel)
			{
				u16 phase = data.read_word(0xbb79 + channel);
				s32 const step = s16(data.read_word(0xbb7b + channel));
				s32 const amplitude = s16(data.read_word(0xbb7d + channel));
				for (unsigned i = 0; i < 64; ++i)
				{
					// Independent phase folding and signed fixed-point scaling, not CPU results.
					s32 const sum = s16(phase) + step;
					if (sum > 32767 || sum < -32768) ++m_tone_phase_wraps;
					phase = u16(sum);
					s32 const distance = sum - 0x4000;
					s32 const folded = s16(u16(distance < 0 ? -distance : distance));
					s32 const index = folded >= 0 ? folded / 128 : -((-folded + 127) / 128);
					u16 const table = data.read_word(0x17fd + (index < 0 ? -index : index));
					s64 const product = s64(s16(table)) * amplitude * 2;
					if (product < -0x80000000LL || product > 0x7fffffffLL)
						fatalerror("MU4 tone fixture exceeds independently modeled nonsaturating scale range");
					s64 const scaled = product >= 0 ? product / 65536 : -((-product + 65535) / 65536);
					m_tone_expected[channel * 64 + i] = u16(scaled);
				}
				m_tone_phase[channel] = phase;
			}
			return;
		}
		for (unsigned i = 0; i < m_tone_expected.size(); ++i)
			if (data.read_word(m_tone_buffer + i) != m_tone_expected[i])
				fatalerror("MU4 native tone mismatch block=%u sample=%u actual=%04x expected=%04x", m_tone_blocks,
					i, data.read_word(m_tone_buffer + i), m_tone_expected[i]);
		for (unsigned channel = 0; channel < 2; ++channel)
			if (data.read_word(0xbb79 + channel) != m_tone_phase[channel])
				fatalerror("MU4 native tone phase disagrees with independent block model");
		if (system_bios() == 20)
		{
			unsigned const bank = m_tone_buffer == 0xbaf9;
			if (m_verified_tone_valid[bank] && m_verified_tone_reads[bank] != 128)
				fatalerror("MU4 tone bank overwritten before complete verified DMA consumption");
			std::copy(m_tone_expected.begin(), m_tone_expected.end(), m_verified_tone.begin() + bank * 128);
			m_verified_tone_valid[bank] = true;
			m_verified_tone_reads[bank] = 0;
			std::fill(m_tone_cleared.begin() + bank * 128, m_tone_cleared.begin() + (bank + 1) * 128, false);
		}
		++m_tone_blocks;
	}
	bool finish_native_replay()
	{
		if (system_bios() != 12 && system_bios() != 19) return false;
		if (!machine().scheduler().can_save() || !m_native_replay_leg)
			fatalerror("MU4 native replay has no restorable checkpoint");
		if (m_native_replay_leg == 1)
		{
			m_native_reference.str(""); m_native_reference.clear();
			if (machine().save().write_stream(m_native_reference) != STATERR_NONE)
				fatalerror("MU4 native first-leg snapshot failed");
			m_native_replay_leg = 2;
			m_native_replay->adjust(attotime::from_nsec(1));
			return true;
		}
		std::stringstream replay;
		if (m_native_replay_leg != 3) fatalerror("MU4 native replay comparison precedes restoration");
		if (machine().save().write_stream(replay) != STATERR_NONE)
			fatalerror("MU4 native second-leg snapshot failed");
		auto const expected = m_native_reference.str(), actual = replay.str();
		if (expected.size() != actual.size()) fatalerror("MU4 native replay snapshot size changed");
		// MAME's uncompressed stream has a 32-byte version/system/signature header.
		if (actual.size() < 32 || !std::equal(expected.begin(), expected.begin() + 32, actual.begin()))
			fatalerror("MU4 native replay snapshot header changed");
		size_t position = 32;
		unsigned differences = 0, compared = 0, frontend = 0;
		for (int index = 0; index < machine().save().registration_count(); ++index)
		{
			void *base; u32 size, count, blocks, stride;
			char const *name = machine().save().indexed_item(index, base, size, count, blocks, stride);
			size_t const length = size_t(size) * count * blocks;
			if (position + length > actual.size()) fatalerror("MU4 native replay registry extent exceeds snapshot");
			// lua_engine::on_machine_postload resets its frontend resume timer.
			// No Lua script runs here; that timer is not emulated hardware state.
			if (std::string_view(name).starts_with("timer/lua_engine::resume/")) ++frontend;
			else
			{
				++compared;
				auto const mismatch = std::mismatch(expected.begin() + position, expected.begin() + position + length, actual.begin() + position);
				if (mismatch.first != expected.begin() + position + length)
				{
					++differences;
					logerror("mu4_native_replay_mismatch: item=%s item_byte=%u expected=%02x actual=%02x\n", name,
						unsigned(mismatch.first - expected.begin() - position), u8(*mismatch.first), u8(*mismatch.second));
				}
			}
			position += length;
		}
		if (position != actual.size() || differences) fatalerror("MU4 native replay differs in %u registered emulation state items", differences);
		logerror("mu4_native_replay: PASS mid_rx_byte=1 complete_legs=2 emulation_state_equal=1 compared_items=%u frontend_timer_items=%u rx_words=11 tx_words=14 processing=0\n", compared, frontend);
		return false;
	}
	TIMER_CALLBACK_MEMBER(command_input)
	{
		if (m_phase != 30 || system_bios() < 8) return;
		// Original parser framing and selector 0x49: read-only software status query.
		static constexpr u8 status[] = {0x1e, 2, 0xaa, 1, 0x49, 1, 0xff, 0x55};
		static constexpr u8 measurement[] = {0x1e, 2, 0xaa, 1, 0x40, 1, 0xf6, 0x55};
		auto const &packet = measurement_profile() ? measurement : status;
		bool const ack = m_native_command_cursor == std::size(packet);
		if (ack && (!m_command_ack_pending || m_command_ack_cursor == 3)) return;
		// Replay starts after the separately checked streaming prefix, so both
		// legs share the same scheduled worker-window endpoint.
		if (system_bios() >= 12 && !original_bootstrap_profile() && !m_native_worker_window_started) { m_command->adjust(attotime::from_msec(1)); return; }
		if (m_native_command_paths[5] && !m_serial_ready && !m_command_rx_busy && !m_command_rx_ready && (m_serial_regs[0][0] & 1))
		{
			if (ack)
			{
				u8 const reply[] = {0x7f, m_command_ack_token, 0x55};
				command_receive(reply[m_command_ack_cursor++]);
				logerror("mu4_native_command_ack_input: cursor=%u total=3\n", m_command_ack_cursor);
			}
			else
			{
				command_receive(packet[m_native_command_cursor++]);
				logerror("mu4_native_command_input: cursor=%u total=%u\n", m_native_command_cursor, unsigned(std::size(packet)));
			}
		}
		else if (++m_native_command_waits > 20000) fatalerror("MU4 native serial command ingress timeout");
		m_command->adjust(attotime::from_msec(1));
	}
	void io_map(address_map &map)
	{
		map(0x4000, 0x7fff).rw(FUNC(mu4_storage_test_state::nand_r), FUNC(mu4_storage_test_state::nand_w));
	}
	u16 gpio_r(offs_t offset) { return offset ? m_gpio : m_direction; }
	void gpio_w(offs_t offset, u16 value)
	{
		if (offset) m_gpio = value; else m_direction = value;
		if (BIT(m_direction, 2)) m_nand->ce_w(BIT(m_gpio, 2));
	}
	void ready_w(int state)
	{
		m_ready = bool(state);
		if (!state) m_read_started = machine().time();
	}
	int bio_r()
	{
		++m_bio_reads;
		if (!m_ready) ++m_busy_reads;
		return m_ready;
	}
	void nand_w(offs_t offset, u16 value)
	{
		// ADD_H is held enabled by this fixture; U202's lower 14 address bits alias.
		if ((m_direction & 7) != 7) fatalerror("MU4 storage: strobe before GPIO outputs enabled");
		m_writes.push_back((u16(m_gpio & 7) << 8) | (value & 0xff));
		if (BIT(m_gpio, 2)) return;
		if (BIT(m_gpio, 0)) m_nand->command_w(value);
		else if (BIT(m_gpio, 1)) m_nand->address_w(value);
		else m_nand->data_w(value);
	}
	u16 nand_r()
	{
		if (!m_data_reads) m_first_data = machine().time();
		++m_data_reads;
		return m_nand->data_r();
	}
	bool original_bootstrap_profile() const { return system_bios() >= 16 && system_bios() <= 20; }
	bool measurement_profile() const { return system_bios() == 13 || system_bios() == 20; }
	void verify_native_status()
	{
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		if (m_command_ack_cursor != 3 || m_command_tx_words.size() != 14 || m_command_wire_bit_count ||
			data.read_word(0x1657) != 11 || data.read_word(0x1658) != 11 || data.read_word(0x1659) != 14 ||
			data.read_word(0x165a) != 14 || data.read_word(0x165e) || data.read_word(0x165d) || data.read_word(0xbb80))
			fatalerror("MU4 framed status transaction did not settle after the physical peer acknowledgement");
		logerror("mu4_native_status_transaction: PASS rx_words=11 tx_words=14 pin_decode=1 ack=1 retries=0 queue_empty=1 mode=0 full_boot=0\n");
		if (system_bios() >= 11)
		{
			if (m_command_rx_irqs != 11 || m_command_rx_busy || m_command_rx_ready)
				fatalerror("MU4 pin receive did not drain eleven device-owned RRDY interrupts");
			logerror("mu4_native_receive_pins: PASS bytes=11 rx_irqs=11 frame=1 data=1 clock=1 drr_drained=1\n");
		}
	}
	void verify_native_measurement()
	{
		auto const disable = machine().disable_side_effects();
		auto &data = m_cpu->space(AS_DATA);
		logerror("mu4_native_measurement_observe: mode=%04x complete=%04x blocks=%04x,%04x left=%04x,%04x right=%04x,%04x tx_words=%u full_boot=0\n",
			data.read_word(0xbb80), data.read_word(0xb64e), data.read_word(0xb650), data.read_word(0xb651),
			data.read_word(0xb652), data.read_word(0xb653), data.read_word(0xb65e), data.read_word(0xb65f), unsigned(m_command_tx_words.size()));
		if (m_measurement_blocks != 6 || m_command_ack_cursor != 3 || m_command_tx_words.size() != 12 ||
			m_command_tx_words[8] != 0x20 || m_command_wire_bit_count || m_command_rx_irqs != 11 ||
			m_command_rx_busy || m_command_rx_ready || data.read_word(0x1657) != 11 || data.read_word(0x1658) != 11 ||
			data.read_word(0x1659) != 12 || data.read_word(0x165a) != 12 ||
			data.read_word(0xbb80) || data.read_word(0x165d) || data.read_word(0x165e))
			fatalerror("MU4 original measurement command did not complete six blocks and settle its acknowledged response");
		logerror("mu4_native_measurement: PASS blocks=6 stereo=1 independent_arithmetic=1 pin_request=1 ack=1 tx_words=12 mode=0 music_decode=0 full_boot=0\n");
		if (m_tone_blocks < 6 || !m_tone_phase_wraps)
			fatalerror("MU4 native tone observation did not cover repeated blocks and signed phase wraparound");
		logerror("mu4_native_tone: PASS blocks=%u samples=%u stereo=1 independent_table_scale=1 phase_wraps=%u music_decode=0 analog_audio=0\n",
			m_tone_blocks, m_tone_blocks * 128, m_tone_phase_wraps);
	}
	void install_measurement_observers()
	{
		for (offs_t address : {0x2876a, 0x287ad})
			m_cpu->space(AS_PROGRAM).install_read_tap(address, address, "mu4_native_tone",
				[this](offs_t address, u16 &, u16) { observe_native_tone(address); });
		m_cpu->space(AS_DATA).install_read_tap(0x1900, 0x19ff, "mu4_measurement_input",
			[this](offs_t, u16 &value, u16)
			{
				if (m_phase != 30 || machine().side_effects_disabled()) return;
				unsigned const pc = m_cpu->pc();
				if (pc != 0x287fa && pc != 0x2880b) return;
				unsigned const channel = pc == 0x2880b;
				s32 const sample = s16(value);
				u32 const scaled = unsigned(sample < 0 ? -sample : sample) / 16;
				u64 const sum = m_measurement_expected[channel] + u64(scaled) * scaled * m_measurement_factor;
				if (sum > 0x7fffffff || ++m_measurement_samples[channel] > 64)
					fatalerror("MU4 live measurement inputs exceed independently modeled block range");
				m_measurement_expected[channel] = sum;
			});
		m_cpu->space(AS_PROGRAM).install_read_tap(0x287ea, 0x28816, "mu4_measurement_result",
			[this](offs_t address, u16 &, u16)
			{
				if (m_phase != 30 || machine().side_effects_disabled() || m_cpu->pc() != address + 1) return;
				if (address != 0x287ea && address != 0x28816) return;
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				if (address == 0x287ea)
				{
					// Observe live input reads: DMA can update the buffer during execution.
					m_measurement_factor = BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1), 6) ? 2 : 1;
					m_measurement_samples.fill(0);
					for (unsigned channel = 0; channel < 2; ++channel)
					{
						u16 const aggregate = 0xb64a + 2 * channel;
						m_measurement_expected[channel] = (u32(data.read_word(aggregate)) << 16) | data.read_word(aggregate + 1);
					}
					return;
				}
				u16 const left = m_cpu->state_int(tms320c54x_device::STATE_AR4), right = m_cpu->state_int(tms320c54x_device::STATE_AR5);
				if (m_measurement_samples[0] != 64 || m_measurement_samples[1] != 64 ||
					((u32(data.read_word(left)) << 16) | data.read_word(left + 1)) != m_measurement_expected[0] ||
					((u32(data.read_word(right)) << 16) | data.read_word(right + 1)) != m_measurement_expected[1])
					fatalerror("MU4 native sample-energy mismatch block=%u samples=%u,%u actual=%08x,%08x expected=%08x,%08x factor=%u", m_measurement_blocks,
						m_measurement_samples[0], m_measurement_samples[1], (u32(data.read_word(left)) << 16) | data.read_word(left + 1),
						(u32(data.read_word(right)) << 16) | data.read_word(right + 1), m_measurement_expected[0], m_measurement_expected[1], m_measurement_factor);
				logerror("mu4_native_measurement_block: index=%u left=%04x,%04x right=%04x,%04x st1=%04x\n",
					m_measurement_blocks++, data.read_word(left), data.read_word(left + 1), data.read_word(right), data.read_word(right + 1),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
			});
	}
	virtual void machine_start() override
	{
		if (original_bootstrap_profile())
		{
			m_phase = 74;
			m_cpu->space(AS_PROGRAM).install_ram(0, 0xffff, &m_program_ram[0]);
			if (measurement_profile()) install_measurement_observers();
			m_cpu->space(AS_PROGRAM).install_read_tap(0x0e41, 0x0e41, "mu4_original_bootstrap_entry",
				[this](offs_t address, u16 &, u16) { if (!machine().side_effects_disabled() && m_cpu->pc() == address + 1) ++m_native_bootstrap_entries; });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x6d62, 0x6d62, "mu4_original_bootstrap_resident",
				[this](offs_t address, u16 &, u16) { if (!machine().side_effects_disabled() && (m_cpu->pc() & 0xffff) == address + 1) ++m_native_bootstrap_resident_entries; });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x090f, 0x3538, "mu4_original_bootstrap_calls",
				[this](offs_t address, u16 &, u16)
				{
					if (machine().side_effects_disabled() || m_cpu->pc() != address + 1) return;
					static constexpr offs_t points[] = {0x090f, 0x0945, 0x094e, 0x0951, 0x095e, 0x096c, 0x0997, 0x09b3, 0x09b5, 0x3538};
					auto const found = std::find(std::begin(points), std::end(points), address);
					if (found == std::end(points)) return;
					unsigned &count = m_native_bootstrap_paths[found - std::begin(points)];
					if (count++ < 4)
					{
						logerror("mu4_original_bootstrap_path: address=%04x a=%010llx nand_reads=%u\n", unsigned(address),
							static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL, m_data_reads);
						if (address == 0x3538)
						{
							auto const disable = machine().disable_side_effects();
							auto &data = m_cpu->space(AS_DATA);
							logerror("mu4_original_bootstrap_loader: name=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x ext=%04x,%04x,%04x length=%04x,%04x vector=%04x,%04x\n",
								data.read_word(0x36b0), data.read_word(0x36b1), data.read_word(0x36b2), data.read_word(0x36b3),
								data.read_word(0x36b4), data.read_word(0x36b5), data.read_word(0x36b6), data.read_word(0x36b7),
								data.read_word(0x36b9), data.read_word(0x36ba), data.read_word(0x36bb), data.read_word(0x36c2), data.read_word(0x36c3),
								m_cpu->space(AS_PROGRAM).read_word(0x2000), m_cpu->space(AS_PROGRAM).read_word(0x2001));
						}
					}
				});
			m_cpu->space(AS_DATA).install_write_tap(0x374d, 0x374d, "mu4_original_bootstrap_gate_init",
				[this](offs_t, u16 &value, u16) { if (!machine().side_effects_disabled()) logerror("mu4_original_bootstrap_gate_init: value=%04x pc=%06x\n", value, unsigned(m_cpu->pc())); });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3acf2, 0x3acf2, "mu4_original_bootstrap_dispatch",
				[this](offs_t address, u16 &, u16) { if (!machine().side_effects_disabled() && m_cpu->pc() == address + 1) ++m_native_command_paths[5]; });
		}
		if (system_bios() == 15)
		{
			// Fresh-process routine bench: mount externally retained NAND, never
			// run the erased-media formatter or inject a resident state snapshot.
			m_phase = 19;
			logerror("mu4_native_retained_start: fresh_process=1 original_mount=1 erase=0 runtime_snapshot=0\n");
		}
		m_check = timer_alloc(FUNC(mu4_storage_test_state::check), this);
		m_command = timer_alloc(FUNC(mu4_storage_test_state::command_input), this);
		m_command_clock = timer_alloc(FUNC(mu4_storage_test_state::command_clock), this);
		m_command_receive_clock = timer_alloc(FUNC(mu4_storage_test_state::command_receive_clock), this);
		m_native_replay = timer_alloc(FUNC(mu4_storage_test_state::native_replay_checkpoint), this);
		if (system_bios() >= 11)
		{
			// Save the external peer alongside McBSP2. Growing observation vectors
			// need bounded backing storage; save_item(vector) fixes its initial size.
			save_item(NAME(m_direction)); save_item(NAME(m_gpio)); save_item(NAME(m_ready));
			save_item(NAME(m_serial_index)); save_item(NAME(m_serial_regs));
			save_item(NAME(m_serial_word)); save_item(NAME(m_serial_ready)); save_item(NAME(m_serial_reads));
			save_item(NAME(m_native_command_cursor)); save_item(NAME(m_native_command_waits));
			save_item(NAME(m_command_tx_irqs)); save_item(NAME(m_command_wire_bits));
			save_item(NAME(m_command_ack_cursor)); save_item(NAME(m_command_wire_bit_count));
			save_item(NAME(m_command_wire_clock)); save_item(NAME(m_command_ack_pending));
			save_item(NAME(m_command_ack_token)); save_item(NAME(m_command_wire_byte));
			save_item(NAME(m_command_tx_register));
			save_item(NAME(m_command_rx_ready)); save_item(NAME(m_command_rx_busy));
			save_item(NAME(m_command_rx_clock)); save_item(NAME(m_command_rx_byte));
			save_item(NAME(m_command_rx_phase)); save_item(NAME(m_command_rx_irqs));
			save_item(NAME(m_saved_command_tx)); save_item(NAME(m_saved_command_decoded));
			save_item(NAME(m_saved_command_tx_count)); save_item(NAME(m_saved_command_decoded_count));
			machine().save().register_presave(save_prepost_delegate(FUNC(mu4_storage_test_state::save_command_peer), this));
			machine().save().register_postload(save_prepost_delegate(FUNC(mu4_storage_test_state::restore_command_peer), this));
		}
		if (system_bios() == 20)
		{
			save_item(NAME(m_stream_words));
			save_item(NAME(m_continuous_tx)); save_item(NAME(m_continuous_tx_head));
			save_item(NAME(m_continuous_tx_count)); save_item(NAME(m_continuous_din_words));
			save_item(NAME(m_continuous_nonzero_words));
			save_item(NAME(m_source_words)); save_item(NAME(m_source_head)); save_item(NAME(m_source_count)); save_item(NAME(m_source_reads));
			save_item(NAME(m_verified_tone)); save_item(NAME(m_verified_tone_valid));
			save_item(NAME(m_verified_tone_reads)); save_item(NAME(m_tone_dma_words));
			save_item(NAME(m_tone_cleared)); save_item(NAME(m_tone_discarded_words));
			m_cpu->space(AS_DATA).install_write_tap(0xba79, 0xbb78, "mu4_native_tone_cleanup",
				[this](offs_t address, u16 &value, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled()) return;
					unsigned const pc = m_cpu->pc(), bank = address >= 0xbaf9;
					if (pc != (bank ? 0x3b05a : 0x3b058)) return;
					unsigned const offset = address - (bank ? 0xbaf9 : 0xba79), index = bank * 128 + offset;
					if (value || !m_verified_tone_valid[bank] || m_tone_cleared[index])
						fatalerror("MU4 original tone cleanup store contract changed");
					unsigned const position = 2 * (offset % 64) + (offset >= 64);
					if (position >= m_verified_tone_reads[bank]) ++m_tone_discarded_words;
					m_tone_cleared[index] = true;
					m_verified_tone[index] = 0;
				});
			m_cpu->space(AS_DATA).install_read_tap(0x80, 0xffff, "mu4_native_dma_source",
				[this](offs_t address, u16 &value, u16)
				{
					// Timer-driven DMA reads, not CPU operands or supervisor inspection.
					if (m_phase != 30 || machine().side_effects_disabled() || machine().scheduler().currently_executing() ||
						!m_dma->source_matches(3, AS_DATA, address)) return;
					bool const tone = address >= 0xba79 && address <= 0xbb78;
					if (m_source_count == m_source_words.size()) fatalerror("MU4 DMA source comparison queue overflow");
					m_source_words[(m_source_head + m_source_count) % m_source_words.size()] = value;
					++m_source_count; ++m_source_reads;
					if (!tone) return;
					unsigned const bank = address >= 0xbaf9;
					unsigned const offset = address - (bank ? 0xbaf9 : 0xba79);
					unsigned const count = m_verified_tone_reads[bank], position = count % 128;
					unsigned const expected_offset = position / 2 + ((position & 1) ? 64 : 0);
					if (!m_verified_tone_valid[bank] || (count >= 128 && !m_tone_cleared[bank * 128 + offset]) || offset != expected_offset ||
						value != m_verified_tone[bank * 128 + offset])
						fatalerror("MU4 tone DMA source mismatch bank=%u count=%u offset=%u actual=%04x expected=%04x mode=%04x blocks=%u pc=%06x",
							bank, count, offset, value, m_verified_tone[bank * 128 + offset], m_cpu->space(AS_DATA).read_word(0xbb80), m_tone_blocks, unsigned(m_cpu->pc()));
					++m_verified_tone_reads[bank];
					if (!m_tone_cleared[bank * 128 + offset]) ++m_tone_dma_words;
				});
		}
	}
	void save_command_peer()
	{
		if (m_command_tx_words.size() > m_saved_command_tx.size() || m_command_wire_decoded.size() > m_saved_command_decoded.size())
			fatalerror("MU4 command peer snapshot capture capacity exceeded");
		m_saved_command_tx_count = m_command_tx_words.size();
		m_saved_command_decoded_count = m_command_wire_decoded.size();
		std::copy(m_command_tx_words.begin(), m_command_tx_words.end(), m_saved_command_tx.begin());
		std::copy(m_command_wire_decoded.begin(), m_command_wire_decoded.end(), m_saved_command_decoded.begin());
	}
	void restore_command_peer()
	{
		if (m_saved_command_tx_count > m_saved_command_tx.size() || m_saved_command_decoded_count > m_saved_command_decoded.size())
			fatalerror("MU4 command peer snapshot has invalid capture count");
		m_command_tx_words.assign(m_saved_command_tx.begin(), m_saved_command_tx.begin() + m_saved_command_tx_count);
		m_command_wire_decoded.assign(m_saved_command_decoded.begin(), m_saved_command_decoded.begin() + m_saved_command_decoded_count);
	}
	void capture_native_ram(char const *stage)
	{
		if (system_bios() != 14 && system_bios() != 15) return;
		// Observation only: omit CPU/MMIO registers and encode words explicitly.
		auto const disable = machine().disable_side_effects();
		std::string const name = util::string_format("mu4_ram_%s_leg%u.bin", stage, m_native_reset_leg);
		std::ofstream output(name, std::ios::binary | std::ios::trunc);
		auto &data = m_cpu->space(AS_DATA);
		for (unsigned address = 0x80; address < 0x10000; ++address)
		{
			u16 const value = data.read_word(address);
			output.put(value & 0xff);
			output.put(value >> 8);
		}
		output.close();
		if (!output) fatalerror("MU4 RAM observation could not be written");
		logerror("mu4_native_ram_observation: stage=%s leg=%u first=0080 words=65408 encoding=le16 file=%s\n", stage, m_native_reset_leg, name.c_str());
	}
	virtual void machine_reset() override
	{
		if (((system_bios() == 14 && m_phase == 19) || (system_bios() == 18 && m_phase == 74)) && m_native_reset_leg == 1)
		{
			if (m_mcbsp2->control_r(1) || m_mcbsp2->control_r(0))
				fatalerror("MU4 serial controller retained control state across reset");
			m_native_reset_leg = 2;
			logerror("mu4_native_reset_controller: control_cleared=1 loader_restart=1\n");
		}
		m_direction = m_gpio = 0;
		m_ready = true;
		m_bio_reads = m_busy_reads = m_data_reads = 0;
		m_writes.clear();
		m_serial_tx_words.clear(); m_serial_tx_bits.clear();
		m_serial_tx_irqs = 0;
		m_dma_output.clear(); m_dma_completions = 0;
		m_rx_dma_completions = m_rx_irqs = 0;
		m_stream_words = m_stream_config_writes = 0;
		m_native_din_words = 0;
		m_continuous_tx.fill(0);
		m_continuous_tx_head = m_continuous_tx_count = m_continuous_din_words = m_continuous_nonzero_words = 0;
		m_source_words.fill(0); m_source_head = m_source_count = m_source_reads = 0;
		m_verified_tone.fill(0); m_verified_tone_valid.fill(false); m_verified_tone_reads.fill(0); m_tone_dma_words = 0;
		m_tone_cleared.fill(false); m_tone_discarded_words = 0;
		m_native_dma_rx_vectors = m_native_dma_tx_vectors = 0;
		m_native_dma_tx_handler = 0;
		m_native_stream_pending_sets = m_native_stream_pending_clears = m_native_stream_pending_reads = 0;
		m_native_dispatch_traces = m_native_descriptor_traces = 0;
		m_native_stream_consumer_entries = 0;
		std::fill(std::begin(m_native_selection_traces), std::end(m_native_selection_traces), 0);
		m_native_selection_writes = 0;
		std::fill(std::begin(m_native_startup_fetches), std::end(m_native_startup_fetches), 0);
		m_native_main_call_traces = 0;
		m_native_config_call_traces = 0;
		m_native_tail_data_reads = 0;
		m_native_lookup_operand_traces = 0;
		std::fill(std::begin(m_native_cache_traces), std::end(m_native_cache_traces), 0);
		m_native_startup_subcall_traces = 0;
		m_native_startup_return_traces = 0;
		m_native_consumer_state_traces = m_native_consumer_mode_writes = 0;
		std::fill(std::begin(m_native_consumer_paths), std::end(m_native_consumer_paths), 0);
		std::fill(std::begin(m_native_command_paths), std::end(m_native_command_paths), 0);
		m_native_command_cursor = m_native_command_waits = 0;
		m_measurement_blocks = 0;
		m_measurement_expected.fill(0);
		m_measurement_samples.fill(0);
		m_measurement_factor = 1;
		m_tone_expected.fill(0); m_tone_phase.fill(0);
		m_tone_buffer = 0;
		m_tone_blocks = m_tone_phase_wraps = 0;
		m_command->adjust(attotime::never);
		m_command_clock->adjust(attotime::never);
		m_command_receive_clock->adjust(attotime::never);
		m_native_replay->adjust(attotime::never);
		m_native_replay_leg = 0;
		m_command_rx_ready = m_command_rx_busy = false; m_command_rx_clock = true;
		m_command_rx_byte = 0; m_command_rx_phase = m_command_rx_irqs = 0;
		m_command_tx_register = m_command_tx_irqs = m_command_wire_bits = 0;
		m_command_wire_clock = false; m_command_tx_words.clear();
		m_command_ack_cursor = m_command_wire_bit_count = 0;
		m_command_ack_pending = false; m_command_ack_token = m_command_wire_byte = 0; m_command_wire_decoded.clear();
		m_native_settings_call_traces = 0;
		m_native_metadata_call_traces = 0;
		m_native_allocation_call_traces = 0;
		m_native_cache_update_traces = 0;
		std::fill(std::begin(m_native_cache_mode_traces), std::end(m_native_cache_mode_traces), 0);
		m_native_settings_buffer_active = false;
		std::fill(std::begin(m_native_runtime_reads), std::end(m_native_runtime_reads), 0);
		std::fill(std::begin(m_native_runtime_writes), std::end(m_native_runtime_writes), 0);
		std::fill(std::begin(m_native_buffer_calls), std::end(m_native_buffer_calls), 0);
		std::fill(std::begin(m_native_buffer_returns), std::end(m_native_buffer_returns), 0);
		m_native_worker_window_started = false;
		m_native_tx_words.clear();
		m_external_words.clear(); m_external_bits.clear();
		auto &program = m_cpu->space(AS_PROGRAM);
		// Synthetic wrapper initializes the routine ABI, then calls untouched code.
		u16 const wrapper[] = {
			0x7718, 0x1200, // STM #1200,SP.
			0xf7be,         // SSBX CPL: stack-relative direct operands.
			0x76f8, 0x003c, 7, 0x76f8, 0x003d, 7,
			0xf6b8, 0xf020, u16(m_phase ? 0xffff : 0), // Unsigned logical row; 3035 adds one.
			0xf980, 0x3035, 0xf980, 0x306c, 0xf4e1
		};
		for (unsigned i = 0; i < std::size(wrapper); ++i) program.write_word(0x1800 + i, wrapper[i]);
		if (m_phase == 54)
		{
			program.write_word(0x1800, 0xf4e1);
			auto &codec = *subdevice<tlv320aic23_device>("codec");
			for (u16 word : { 0x0c10, 0x0818, 0x0a01, 0x0e53, 0x1023 }) codec.control_word_w(word);
			m_codec_edges.clear(); m_codec_frames.clear(); m_codec_clocks = 0;
			m_codec_din_words.clear();
			// Codec reset owns idle-low BCLK/FS. Do not override those pins with
			// the inactive levels used by the independent receive waveform fixture.
			external_reg_w(0, 0); external_reg_w(2, 0x140); external_reg_w(3, 0x44);
			external_reg_w(14, 0x0e); external_reg_w(0, 1);
			external_reg_w(4, 0x140); external_reg_w(5, 0x44); external_reg_w(1, 1);
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x6000, 0x1357); data.write_word(0x6001, 0x9bdf);
			data.write_word(0x6400, 0xffff); data.write_word(0x6401, 0xffff);
			dma_reg_w(0xf, 0x6000); dma_reg_w(0x10, 0x23); dma_reg_w(0x11, 1); dma_reg_w(0x12, 0x2000); dma_reg_w(0x13, 0xc541);
			dma_reg_w(0x32, 0x6000); dma_reg_w(0x33, 0x23); dma_reg_w(0x34, 1); dma_reg_w(0x35, 0x2000);
			dma_reg_w(0xa, 0x21); dma_reg_w(0xb, 0x6400); dma_reg_w(0xc, 1); dma_reg_w(0xd, 0x1000); dma_reg_w(0xe, 0xc055);
			dma_reg_w(0x2e, 0x21); dma_reg_w(0x2f, 0x6400); dma_reg_w(0x30, 1); dma_reg_w(0x31, 0x1000);
			dma_reg_w(0x20, 1); dma_reg_w(0x22, 0xffff);
			m_dma->write(0, 0x4c);
		}
		if (m_phase == 58 || m_phase == 59)
		{
			program.write_word(0x1800, 0xf4e1);
			receive_setup(0x0140, 4);
			if (m_phase == 59)
			{
				m_mcbsp0->rx_frame_w(1);
				for (int bit = 15; bit >= 9; --bit) { receive_bit(BIT(0xa55a, bit)); m_mcbsp0->rx_frame_w(0); }
			}
		}
		if (m_phase == 31)
		{
			program.write_word(0x1800, 0xf4e1);
			mcbsp_reg_w(4, 0x0040); mcbsp_reg_w(5, 0);
			mcbsp_reg_w(6, 12); mcbsp_reg_w(7, 0x2000);
			mcbsp_reg_w(14, 0); // External clock absent: a queued word must remain busy.
			mcbsp_reg_w(1, 0x0041);
			if ((mcbsp_status() & 7) != 3) fatalerror("MU4 McBSP reset-release readiness mismatch");
			m_mcbsp->data_w(3, 0xa55a);
			if (mcbsp_status() & 2) fatalerror("MU4 McBSP DXR write did not clear readiness");
		}
		if (m_phase == 38 || m_phase == 44)
		{
			program.write_word(0x1800, 0xf4e1);
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x6000, 0x1234); data.write_word(0x6040, 0x5678);
			data.write_word(0x6001, 0xabcd); data.write_word(0x6041, 0x9abc);
			dma_reg_w(0x0f, 0x6000); dma_reg_w(0x10, 0x6200); dma_reg_w(0x11, 1);
			dma_reg_w(0x12, 0x2001); dma_reg_w(0x13, m_phase == 44 ? 0x6541 : 0xc541);
			dma_reg_w(0x20, 0x40); dma_reg_w(0x22, 0xffc1);
			dma_reg_w(0x32, 0x6000); dma_reg_w(0x33, 0x6200);
			dma_reg_w(0x34, 1); dma_reg_w(0x35, 0x2001);
			m_dma->write(0, m_phase == 44 ? 0x48 : 8);
			if (m_phase == 44)
			{
				mcbsp_reg_w(1, 1); // XINT1 must be suppressed while INTOSEL selects DMA3.
			}
		}
		if (m_phase == 49)
		{
			program.write_word(0x1800, 0xf4e1);
			m_cpu->space(AS_DATA).write_word(0x6000, 0x1234);
			m_cpu->space(AS_DATA).write_word(0x6001, 0xabcd);
			dma_reg_w(0xf, 0x6000); dma_reg_w(0x10, 0x6200);
			dma_reg_w(0x11, 0); dma_reg_w(0x12, 0x2000); dma_reg_w(0x13, 0x4141);
			m_dma->sync_w(2, ASSERT_LINE);
			m_dma->write(0, 8);
			m_dma->sync_w(2, ASSERT_LINE); // Held level is not a second event.
		}
		if (m_phase == 51)
		{
			program.write_word(0x1800, 0xf4e1);
			external_reg_w(4, 0x0140); external_reg_w(5, 0x0044);
			external_reg_w(14, 0x000e); external_reg_w(1, 1);
			m_mcbsp0->tx_frame_w(1); m_mcbsp0->tx_clock_w(1);
			m_mcbsp0->data_w(3, 0xa55a);
		}
		if (m_phase >= 25 && m_phase <= 29)
		{
			program.write_word(0x1809, 0xf4e1);
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x6000, 0x1234); data.write_word(0x6001, 0xabcd);
			program.write_word(0x2ffff, 0xffff); program.write_word(0x20000, 0xffff);
			data.write_word(0x6100, 0xffff);
			data.write_word(0x55, 0);
			data.write_word(0x56, 0x6000);
			data.write_word(0x56, m_phase == 25 ? 0xffff : 0x6100);
			data.write_word(0x56, m_phase == 25 ? 1 : 0);
			data.write_word(0x56, 0);
			data.write_word(0x56, m_phase == 25 ? 0x0144 : 0x0145);
			if (data.read_word(0x55) != 5) fatalerror("MU4 DMA autoincrement mismatch");
			data.write_word(0x55, 0x1e); data.write_word(0x56, 0); data.write_word(0x56, 0xff82);
			data.write_word(0x55, 0x1f);
			if (data.read_word(0x57) != 2 || data.read_word(0x55) != 0x1f) fatalerror("MU4 DMA page/nonincrement mismatch");
			if (m_dma->source_matches(0, AS_DATA, 0x6000)) fatalerror("MU4 disabled DMA source inspection matched");
			data.write_word(0x54, 1);
			if (!m_dma->source_matches(0, AS_DATA, 0x6000) || m_dma->source_matches(0, AS_DATA, 0x6001) ||
				m_dma->source_matches(0, AS_PROGRAM, 0x6000) || m_dma->source_matches(6, AS_DATA, 0x6000) ||
				data.read_word(0x55) != 0x1f)
				fatalerror("MU4 DMA source inspection changed index or matched wrong channel/address/space");
			if (!(data.read_word(0x54) & 1) || data.read_word(0x6100) != 0xffff || program.read_word(0x2ffff) != 0xffff)
				fatalerror("MU4 DMA completed synchronously");
			if (m_phase == 27) data.write_word(0x54, 0);
		}
		if (m_phase == 4)
		{
			// Flush is called with NAND already selected by the storage reader.
			program.write_word(0x1808, 3);
			// Routine ABI fixture: dirty cache contents and its physical row base.
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x3b12, 0); data.write_word(0x3b13, 64);
			data.write_word(0x3b14, 1);
			for (unsigned i = 0; i < 8192; ++i) data.write_word(0x4000 + i, pattern(i));
			program.write_word(0x1809, 0xf980); program.write_word(0x180a, 0x0725);
			program.write_word(0x180b, 0xf4e1);
		}
		if (m_phase == 6 || m_phase == 19 || m_phase == 21 || m_phase == 24)
		{
			if (m_phase == 19 || m_phase == 21 || m_phase == 24)
			{
				program.install_rom(0x20ae, 0x3679, reinterpret_cast<u16 *>(memregion("library")->base()));
				// Remove InitDisk-only ROM mappings before the consumer DMA writes these addresses.
				program.install_ram(0x367a, 0x4abe, &m_program_ram[0x367a]);
			}
			// Original startup's mount ABI: filesystem context and partition-aware flag.
			u16 const mount[] = {0x7600, 1, 0xf020, 0x3aea, 0xf980, 0x308e, 0xf4e1};
			for (unsigned i = 0; i < std::size(mount); ++i) program.write_word(0x1809 + i, mount[i]);
			if (m_phase == 21 || m_phase == 24)
			{
				// Same directory ABI as startup 0960..096b; no descriptor/file-state injection.
				u16 const directory[] = {0x7600, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x09c6,
					0x7600, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x09fa, 0xf4e1};
				for (unsigned i = 0; i < std::size(directory); ++i) program.write_word(0x180f + i, directory[i]);
				if (m_phase == 24)
				{
					// Loader-safe stack matches original 3538; isolate 2ed4 before its program transfer.
					u16 const load[] = {0x7718, 0x3aea, 0xe800, 0xf980, 0x2ed4, 0xf4e1};
					for (unsigned i = 0; i < std::size(load); ++i) program.write_word(0x181b + i, load[i]);
				}
			}
		}
		if (m_phase == 9)
		{
			u16 const format[] = {0xf020, 0x3aea, 0xf980, 0x051d, 0xf4e1};
			for (unsigned i = 0; i < std::size(format); ++i) program.write_word(0x1809 + i, format[i]);
		}
		if (m_phase == 22)
		{
			unsigned cursor = 0x1809;
			if (!m_file_cursor)
			{
				u16 const seek[] = {0x7600, 0, 0x7601, 0, 0x7602, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x33ea};
				for (u16 word : seek) program.write_word(cursor++, word);
			}
			m_read_words = std::min(256U, (m_file_size - m_file_cursor) / 2);
			u16 const read[] = {0x7600, 0x6000, 0x7601, u16(m_read_words), 0x7602, 0x3aea,
				0xf020, 0x36b0, 0xf980, 0x32d3, 0xf4e1};
			for (u16 word : read) program.write_word(cursor++, word);
		}
		if (m_phase == 23)
		{
			u16 const next[] = {0x7600, 0x3aea, 0xf020, 0x36b0, 0xf980, 0x0a0b, 0xf4e1};
			for (unsigned i = 0; i < std::size(next); ++i) program.write_word(0x1809 + i, next[i]);
		}
		if (m_phase == 7 || m_phase == 19 || m_phase == 21 || m_phase == 24)
		{
			// Original C globals are routine inputs, not an invented disk image.
			auto &data = m_cpu->space(AS_DATA);
			unsigned cursor = 0;
			while (cursor < m_cinit.length())
			{
				unsigned const count = m_cinit[cursor++];
				if (!count) break;
				if (cursor + count + 1 > m_cinit.length()) fatalerror("MU4 fixture truncated cinit");
				unsigned const destination = m_cinit[cursor++];
				if (destination + count > 0x10000) fatalerror("MU4 fixture wrapping cinit");
				for (unsigned i = 0; i < count; ++i) data.write_word(destination + i, m_cinit[cursor++]);
			}
			if (cursor != m_cinit.length()) fatalerror("MU4 fixture incomplete cinit");
			if (m_phase == 7)
			{
				u16 const recovery[] = {0xf020, 0x3aea, 0xf980, 0x0880, 0xf4e1};
				for (unsigned i = 0; i < std::size(recovery); ++i) program.write_word(0x1809 + i, recovery[i]);
			}
		}
		if (m_phase == 10 || m_phase == 12 || m_phase == 13 || m_phase == 14 || m_phase == 18)
		{
			// Replace the routine library with unchanged InitDisk code, not a flat overlay image.
			program.install_rom(0x256d, 0x4aa8, &m_initdisk[0]);
			program.install_rom(0x4aa9, 0x4abe, &m_disk_helpers[0]);
			program.install_rom(0x0080, 0x00f7, &m_disk_vectors[0]);
			auto &data = m_cpu->space(AS_DATA);
			if (m_phase == 18)
			{
				// Independent factory-erased input, not media left by earlier routine tests.
				m_nand->nvram_reset();
				for (unsigned address = 0x80; address < 0x10000; ++address) data.write_word(address, 0);
			}
			unsigned cursor = 0;
			while (cursor < m_disk_cinit.length())
			{
				unsigned const count = m_disk_cinit[cursor++];
				if (!count) break;
				if (cursor + count + 1 > m_disk_cinit.length()) fatalerror("MU4 InitDisk truncated cinit");
				unsigned const destination = m_disk_cinit[cursor++];
				if (destination + count > 0x10000) fatalerror("MU4 InitDisk wrapping cinit");
				for (unsigned i = 0; i < count; ++i) data.write_word(destination + i, m_disk_cinit[cursor++]);
			}
			if (cursor != m_disk_cinit.length()) fatalerror("MU4 InitDisk incomplete cinit");
			// Original ABI: block zero in A, original NAND geometry context on the stack.
			u16 const marker[] = {0xf074, 0x377a, 0x7600, 0x0585, 0xf020, 0, 0xf074, 0x2889, 0xf4e1};
			for (unsigned i = 0; i < std::size(marker); ++i) program.write_word(0x1809 + i, marker[i]);
			if (m_phase == 13)
			{
				u16 const scan[] = {0xf074, 0x377a, 0x7600, 0x2000, 0x7601, 0x2100,
					0xf020, 0x068c, 0xf074, 0x36b9, 0xf4e1};
				for (unsigned i = 0; i < std::size(scan); ++i) program.write_word(0x1809 + i, scan[i]);
			}
			if (m_phase == 14)
			{
				u16 const receive[] = {0xf074, 0x346a, 0x771d, 0x00e8, 0x7700, 0x0040,
					0xf6bb, 0xf4e1, 0xf073, 0x1810};
				for (unsigned i = 0; i < std::size(receive); ++i) program.write_word(0x1809 + i, receive[i]);
			}
			if (m_phase == 18)
			{
				u16 const initialize[] = {0xf074, 0x346a, 0x771d, 0x00e8,
					0xf020, 0x057e, 0xf074, 0x30de, 0xf4e1};
				for (unsigned i = 0; i < std::size(initialize); ++i) program.write_word(0x1809 + i, initialize[i]);
			}
		}
		if (m_phase == 17)
		{
			u16 const consume[] = {0xf074, 0x34c0, 0xf4e1};
			for (unsigned i = 0; i < std::size(consume); ++i) program.write_word(0x1809 + i, consume[i]);
		}
		// Disable the unrelated core timer so it cannot wake the completion IDLE.
		program.write_word(0x17fe, 0x7726); program.write_word(0x17ff, 0x0010);
		program.write_word(0xff80, 0xf073); program.write_word(0xff81, 0x17fe);
		m_check->adjust(m_phase >= 25 ? attotime::from_usec(1) : attotime::from_msec(m_phase == 24 ? 8000 : m_phase == 18 ? 60000 : m_phase == 13 ? 8000 : (m_phase == 7 || m_phase == 14) ? 500 : m_phase == 4 ? 250 : m_phase >= 6 ? 50 : 1));
		if (m_phase == 74)
		{
			// Original serial-boot entry handoff, not mask-ROM or DA150 reset wiring.
			// Let the original startup execute its C table and choose its NAND file.
			auto word = [this](unsigned offset) -> u16
			{
				if (offset + 1 >= m_segment.length()) fatalerror("MU4 bootstrap upload bounds");
				return (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
			};
			if (word(0) != 0xaa55 || word(6) != 0x08aa || word(16) || word(18) != 0x0e41)
				fatalerror("MU4 original bootstrap header changed");
			unsigned const end = 6 + (unsigned(word(2)) << 16) + word(4);
			unsigned cursor = 20, records = 0, uploaded = 0;
			while (word(cursor))
			{
				unsigned const count = word(cursor);
				u32 const destination = (u32(word(cursor + 2)) << 16) | word(cursor + 4);
				cursor += 6;
				if (destination + count > 0x10000 || cursor + count * 2 > end)
					fatalerror("MU4 original bootstrap upload range changed");
				for (unsigned i = 0; i < count; ++i) program.write_word(destination + i, word(cursor + i * 2));
				cursor += count * 2;
				uploaded += count;
				++records;
			}
			if (cursor + 2 != end || records != 7 || uploaded != 10760)
				fatalerror("MU4 original bootstrap upload coverage changed records=%u words=%u", records, uploaded);
			logerror("mu4_original_bootstrap_upload: records=%u words=%u program_only=1 data_alias=unvalidated\n", records, uploaded);
			m_phase = 30;
			if (system_bios() >= 17) m_command->adjust(attotime::from_msec(1));
			m_check->adjust(attotime::from_seconds(20));
			logerror("mu4_original_bootstrap_start: reset_vector=00ff80 serial_entry=000e41 retained_nand=1 routine_wrapper=0 mask_rom=0\n");
		}
	}
	void start_media_read()
	{
		m_nand->command_w(0);
		m_nand->address_w(0); m_nand->address_w(64 + m_page);
		m_nand->address_w(0); m_nand->address_w(0);
		m_check->adjust(attotime::from_usec(11));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		if (original_bootstrap_profile())
		{
			if (!m_native_bootstrap_entries || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL))
				fatalerror("MU4 original bootstrap entry did not execute cleanly");
			auto const disable = machine().disable_side_effects();
			logerror("mu4_original_bootstrap_frontier: entry_count=%u resident_count=%u pc=%06x st1=%04x imr=%04x ifr=%04x flag374d=%04x nand_reads=%u stream_words=%u tail_ms=20000 board_boot=0 music_decode=0\n",
				m_native_bootstrap_entries, m_native_bootstrap_resident_entries, unsigned(m_cpu->state_int(STATE_GENPC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), m_cpu->space(AS_DATA).read_word(0x374d), m_data_reads, m_stream_words);
			if (measurement_profile()) verify_native_measurement();
			else if (system_bios() >= 17) verify_native_status();
			if (system_bios() == 20)
			{
				logerror("mu4_native_codec_continuous_observe: tx_words=%u din_words=%u pending=%u nonzero=%u\n",
					m_stream_words, m_continuous_din_words, m_continuous_tx_count, m_continuous_nonzero_words);
				if (m_continuous_din_words <= 1024 || m_continuous_tx_count > 1 ||
					m_continuous_din_words + m_continuous_tx_count != m_stream_words || !m_continuous_nonzero_words)
					fatalerror("MU4 processing did not produce continuously verified nonzero codec output");
				logerror("mu4_native_codec_continuous: PASS complete_window=1 digital_din=1 nonzero=1 music_decode=0 analog_audio=0\n");
				logerror("mu4_native_tone_dma_observe: source_reads=%u pending=%u tone_words=%u generated_blocks=%u discarded=%u\n",
					m_source_reads, m_source_count, m_tone_dma_words, m_tone_blocks, m_tone_discarded_words);
				if (m_source_reads != m_stream_words + m_source_count || m_source_count > 2 ||
					!m_tone_dma_words || m_tone_dma_words + m_tone_discarded_words != m_tone_blocks * 128)
					fatalerror("MU4 generated tone samples are neither verified through DMA nor cancelled by original cleanup");
				logerror("mu4_native_tone_dma: PASS independent_samples=1 stereo_order=1 dma_to_tx=1 tx_to_din=1 music_decode=0 analog_audio=0\n");
			}
			if (finish_native_replay()) return;
			if (system_bios() == 18)
			{
				if (m_native_reset_leg != 2 || m_native_bootstrap_entries != 2 || m_native_bootstrap_resident_entries != 2)
					fatalerror("MU4 full startup did not repeat after the mid-byte reset");
				logerror("mu4_original_bootstrap_reset: PASS mid_rx_byte=1 controller_cleared=1 original_startup_legs=2 retained_nand=1 rx_words=11 tx_words=14 firmware_state_forcing=0 board_reset=0\n");
			}
			machine().schedule_exit();
			return;
		}
		if (m_phase >= 58 && m_phase <= 65)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (m_phase == 58)
			{
				for (unsigned i = 0; i < 4; ++i) receive_bit(1);
				if (receive_status() & 2) fatalerror("MU4 receive shifted without a frame");
				m_mcbsp0->rx_frame_w(1);
				for (int bit = 15; bit >= 0; --bit)
				{
					if (bit == 7) m_mcbsp0->rx_frame_w(1); // RFIG must ignore this early frame.
					receive_bit(BIT(0xa55a, bit)); m_mcbsp0->rx_frame_w(0);
				}
				if (receive_status() & 2) fatalerror("MU4 receive ready before RBR handoff edge");
				for (int bit = 15; bit >= 0; --bit) receive_bit(BIT(0x5aa5, bit));
				if (!(receive_status() & 2)) fatalerror("MU4 receive first word never ready");
				{ auto const disable = machine().disable_side_effects(); if (m_mcbsp0->data_r(1) != 0xa55a) fatalerror("MU4 receive peek mismatch"); }
				if (!(receive_status() & 2) || m_mcbsp0->data_r(1) != 0xa55a || (receive_status() & 2)) fatalerror("MU4 DRR read readiness mismatch");
				receive_bit(0);
				if (!(receive_status() & 2) || m_mcbsp0->data_r(1) != 0x5aa5 || m_rx_irqs != 2) fatalerror("MU4 receive buffered second word mismatch");
				m_phase = 59; machine().schedule_soft_reset(); return;
			}
			if (m_phase == 59)
			{
				if (!machine().scheduler().can_save() || (receive_status() & 2)) fatalerror("MU4 receive partial save is not quiescent");
				m_saved_dma.str({}); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 receive save failed");
				++m_phase; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 61)
			{
				if (!machine().scheduler().can_save()) fatalerror("MU4 receive restore has synchronized inputs pending");
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE || (receive_status() & 2)) fatalerror("MU4 receive restore mismatch");
			}
			if (m_phase == 60 || m_phase == 61)
			{
				for (int bit = 8; bit >= 0; --bit) receive_bit(BIT(0xa55a, bit));
				receive_bit(0);
				if (!(receive_status() & 2) || m_mcbsp0->data_r(1) != 0xa55a) fatalerror("MU4 receive pending bit replay mismatch");
				++m_phase; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 62)
			{
				if (m_rx_irqs != 2 || !(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x10)) fatalerror("MU4 receive interrupt replay mismatch");
				receive_setup(0, 5); receive_word(0xfa, 8, 1);
				if (m_mcbsp0->data_r(1) != 0xfa) fatalerror("MU4 8-bit receive delay mismatch");
				receive_setup(0x20, 6, 5, 0x2001); receive_word(0xabc, 12, 2, true);
				if (m_mcbsp0->data_r(0) != 0xffff || m_mcbsp0->data_r(1) != 0xfabc) fatalerror("MU4 signed receive/polarity mismatch");
				receive_setup(0x20, 4, 0, 0x4001); receive_word(0xabc, 12);
				if (m_mcbsp0->data_r(0) || m_mcbsp0->data_r(1) != 0xabc0) fatalerror("MU4 left-justified receive mismatch");
				m_mcbsp0->rx_frame_w(1); receive_bit(1); external_reg_w(0, 0);
				for (unsigned i = 0; i < 32; ++i) receive_bit(1);
				if (receive_status() & 2) fatalerror("MU4 receive reset did not cancel pending word");
				logerror("mu4_mcbsp_receive: PASS edge_handoff=1 buffering=1 widths=8,12,16 delays=0,1,2 polarity=1 justification=1 peek=1 pending_restore=1 irq=1 reset=1\n");
				check_receive_overrun();
				receive_setup(0x140, 0x44, 0x000e);
				data.write_word(0x6300, 0xffff); data.write_word(0x6340, 0xffff);
				dma_reg_w(0xa, 0x21); dma_reg_w(0xb, 0x6300); dma_reg_w(0xc, 1);
				dma_reg_w(0xd, 0x1000); dma_reg_w(0xe, 0xc055);
				dma_reg_w(0x20, 0x40); dma_reg_w(0x22, 0xffc1);
				dma_reg_w(0x2e, 0x21); dma_reg_w(0x2f, 0x6300); dma_reg_w(0x30, 1); dma_reg_w(0x31, 0x1000);
				m_dma->write(0, 0x44);
				m_phase = 63; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 63)
			{
				if (data.read_word(0x6300) != 0xffff || m_rx_dma_completions) fatalerror("MU4 receive DMA advanced without REVT0");
				m_mcbsp0->rx_frame_w(0);
				for (int bit = 15; bit >= 0; --bit) { receive_bit(BIT(0x1234, bit)); m_mcbsp0->rx_frame_w(1); }
				receive_bit(BIT(0x5678, 15));
				m_phase = 64; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 64)
			{
				if (data.read_word(0x6300) != 0x1234 || data.read_word(0x6340) != 0xffff || (receive_status() & 2) || m_rx_dma_completions)
					fatalerror("MU4 receive DMA first-word/readiness mismatch");
				for (int bit = 14; bit >= 0; --bit) receive_bit(BIT(0x5678, bit));
				receive_bit(0);
				m_phase = 65; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (data.read_word(0x6300) != 0x1234 || data.read_word(0x6340) != 0x5678 || m_rx_dma_completions != 1 ||
				!(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x400) || (receive_status() & 2) ||
				dma_reg_r(0xb) != 0x6300 || dma_reg_r(0xc) != 1 || !(m_dma->read(0) & 4))
				fatalerror("MU4 receive DMA completion/sort/reload/IRQ mismatch");
			m_dma->write(0, 0);
			logerror("mu4_mcbsp_receive_dma: PASS event=REVT0 channel=2 deferred=1 sorted=1 auto_reload=1 irq=10\n");
			m_phase = 31; machine().schedule_soft_reset(); return;
		}
		if (m_phase >= 54 && m_phase <= 57)
		{
			auto &codec = *subdevice<tlv320aic23_device>("codec");
			if (m_phase == 54)
			{
				if (!m_codec_edges.empty() || codec.reg(7) != 0x53 || codec.reg(8) != 0x23)
					fatalerror("MU4 codec inactive/control mismatch");
				codec.control_word_w(0x1201);
				m_phase = 55; m_check->adjust(attotime::from_usec(92)); return;
			}
			if (m_phase == 55)
			{
				if (m_codec_clocks != 1104 || m_codec_frames.size() != 5)
					fatalerror("MU4 codec clock rate mismatch clocks=%u frames=%u", m_codec_clocks, unsigned(m_codec_frames.size()));
				for (unsigned i = 1; i < m_codec_frames.size(); ++i)
					if (m_codec_frames[i] - m_codec_frames[i - 1] != 272) fatalerror("MU4 codec frame divider mismatch");
				if (m_codec_din_words != std::vector<u16>({0x1357,0x9bdf,0x1357,0x9bdf,0x1357,0x9bdf,0x1357,0x9bdf}) ||
					m_rx_dma_completions != 4 || !machine().scheduler().can_save()) fatalerror("MU4 codec duplex/checkpoint mismatch");
				m_codec_rx_snapshot_count = m_rx_dma_completions;
				m_saved_dma.str({}); m_saved_dma.clear();
				m_codec_snapshot_time = machine().time();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 codec save failed");
				m_codec_edges.clear(); m_phase = 56; m_check->adjust(attotime::from_usec(10)); return;
			}
			if (m_phase == 56)
			{
				m_codec_saved_edges = m_codec_edges;
				m_codec_saved_din.assign(m_codec_din_words.begin() + 8, m_codec_din_words.end());
				if (m_codec_saved_din != std::vector<u16>({0x1357, 0x9bdf}) || m_rx_dma_completions != m_codec_rx_snapshot_count + 1 ||
					m_cpu->space(AS_DATA).read_word(0x6400) != 0x1234 || m_cpu->space(AS_DATA).read_word(0x6401) != 0x5678 || !machine().scheduler().can_save())
					fatalerror("MU4 codec stereo/converted-sample delivery mismatch");
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 codec restore failed");
				// Loading inside a timer callback restores scheduler base time, but
				// adjust() still uses this callback's old timestamp. Target the saved
				// observation deadline, not ten microseconds after the old callback.
				m_codec_edges.clear(); m_codec_din_words.clear(); m_codec_rx_snapshot_count = m_rx_dma_completions; m_phase = 57;
				m_check->adjust(m_codec_snapshot_time + attotime::from_usec(10) - machine().time()); return;
			}
			if (m_codec_edges != m_codec_saved_edges)
				fatalerror("MU4 codec pending clock replay mismatch expected=%u actual=%u first=%d/%d", unsigned(m_codec_saved_edges.size()), unsigned(m_codec_edges.size()), m_codec_saved_edges.empty() ? -1 : m_codec_saved_edges.front(), m_codec_edges.empty() ? -1 : m_codec_edges.front());
			if (m_codec_din_words != m_codec_saved_din || m_rx_dma_completions != m_codec_rx_snapshot_count + 1 ||
				m_cpu->space(AS_DATA).read_word(0x6400) != 0x1234 || m_cpu->space(AS_DATA).read_word(0x6401) != 0x5678)
				fatalerror("MU4 codec mid-word duplex replay mismatch");
			codec.control_word_w(0x1200);
			codec.control_word_w(0x1e00);
			if (codec.reg(7) != 1 || codec.reg(9)) fatalerror("MU4 codec reset defaults mismatch");
			logerror("mu4_codec_clock: PASS inactive=1 controls=1 bclk_mclk=1 frame_divider=272 pending_restore=1 reset=1\n");
			logerror("mu4_codec_duplex: PASS post_clock_pins=1 din_stereo=1 dout_converted_fixture=1 dma=2,3 mid_word_restore=1\n");
			m_phase = 58; machine().schedule_soft_reset(); return;
		}
		if (m_phase >= 31 && m_phase <= 37)
		{
			if (m_phase == 31)
			{
				if (!m_serial_tx_words.empty() || (mcbsp_status() & 6)) fatalerror("MU4 McBSP advanced without clock");
				mcbsp_reg_w(14, 0x0200);
				m_phase = 32; m_check->adjust(attotime::from_usec(3)); return;
			}
			if (m_phase == 32)
			{
				if ((mcbsp_status() & 7) != 7 || !m_serial_tx_words.empty() || m_serial_tx_bits.size() != 1)
					fatalerror("MU4 McBSP buffer-to-shift timing mismatch status=%04x words=%u bits=%u", mcbsp_status(), unsigned(m_serial_tx_words.size()), unsigned(m_serial_tx_bits.size()));
				m_mcbsp->data_w(3, 0x5aa5);
				if (mcbsp_status() & 2) fatalerror("MU4 McBSP second buffer not busy");
				m_serial_saved_bits = m_serial_tx_bits.size();
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 McBSP pending save failed");
				m_phase = 33; m_check->adjust(attotime::from_usec(70)); return;
			}
			if (m_phase == 33 || m_phase == 34)
			{
				if (m_serial_tx_words != std::vector<u16>({0xa55a, 0x5aa5}) || (mcbsp_status() & 7) != 3)
					fatalerror("MU4 McBSP serialized word/completion mismatch");
				unsigned const first = m_phase == 33 ? 0 : m_serial_saved_bits;
				if (m_serial_tx_bits.size() != 32 - first) fatalerror("MU4 McBSP serialized bit count mismatch");
				for (unsigned bit = first; bit < 32; ++bit)
					if (m_serial_tx_bits[bit - first] != BIT(bit < 16 ? 0xa55a : 0x5aa5, 15 - (bit & 15)))
						fatalerror("MU4 McBSP serialized bit order mismatch");
				if (m_phase == 33)
				{
					m_saved_dma.clear(); m_saved_dma.seekg(0);
					if (machine().save().read_stream(m_saved_dma) != STATERR_NONE || (mcbsp_status() & 7) != 5)
						fatalerror("MU4 McBSP pending restore mismatch");
					m_serial_tx_words.clear(); m_serial_tx_bits.clear();
					m_phase = 34; m_check->adjust(attotime::from_usec(70)); return;
				}
				m_mcbsp->data_w(3, 0x1234); mcbsp_reg_w(1, 0);
				m_phase = 35; m_check->adjust(attotime::from_usec(40)); return;
			}
			if (m_phase == 35)
			{
				if (mcbsp_status() & 7 || m_serial_tx_words.size() != 2 || m_serial_tx_irqs != 4) fatalerror("MU4 McBSP reset/interrupt count mismatch");
				m_serial_tx_words.clear(); m_serial_tx_bits.clear();
				mcbsp_reg_w(4, 0); mcbsp_reg_w(1, 0x0041); m_mcbsp->data_w(3, 0xa55a);
				m_phase = 36; m_check->adjust(attotime::from_usec(40)); return;
			}
			unsigned const width = m_phase == 36 ? 8 : 12;
			u16 const expected = m_phase == 36 ? 0x005a : 0x055a;
			if (m_serial_tx_words != std::vector<u16>({expected}) || m_serial_tx_bits.size() != width || (mcbsp_status() & 7) != 3)
				fatalerror("MU4 McBSP short-word completion mismatch");
			for (unsigned bit = 0; bit < width; ++bit)
				if (m_serial_tx_bits[bit] != BIT(expected, width - bit - 1)) fatalerror("MU4 McBSP short-word bit order mismatch");
			if (m_phase == 36)
			{
				m_serial_tx_words.clear(); m_serial_tx_bits.clear();
				mcbsp_reg_w(1, 0); mcbsp_reg_w(4, 0x0020); mcbsp_reg_w(1, 0x0041); m_mcbsp->data_w(3, 0xa55a);
				m_phase = 37; m_check->adjust(attotime::from_usec(40)); return;
			}
			if (m_serial_tx_irqs != 8) fatalerror("MU4 McBSP short-word interrupt mismatch");
			m_saved_dma.str(std::string());
			logerror("mu4_mcbsp_conformance: PASS reset_ready=1 busy=1 external_stall=1 bit_order=1 pending_restore=1 cancel=1 widths=8,12,16 tx_irqs=8\n");
			m_phase = 38; machine().schedule_soft_reset(); return;
		}
		if (m_phase >= 38 && m_phase <= 43)
		{
			if (m_phase == 38)
			{
				if (!m_dma_output.empty() || m_dma->read(0) != 8) fatalerror("MU4 synchronized DMA advanced without event");
				m_dma->sync_event(3);
				if (!m_dma_output.empty()) fatalerror("MU4 mismatched DMA event advanced transfer");
				m_dma->sync_event(2);
				if (!m_dma_output.empty()) fatalerror("MU4 synchronized DMA completed synchronously");
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 synchronized DMA save failed");
			}
			else if (m_phase <= 42)
			{
				unsigned const words = m_phase - 38;
				std::vector<u16> const expected = {0x1234, 0x5678, 0xabcd, 0x9abc};
				if (m_dma_output != std::vector<u16>(expected.begin(), expected.begin() + words))
					fatalerror("MU4 synchronized DMA sorting mismatch step=%u", words);
				if (m_dma_completions != (words == 4 ? 1U : 0U)) fatalerror("MU4 DMA block interrupt ordering mismatch");
				if (words == 4)
				{
					if (m_dma->read(0) != 8 || dma_reg_r(0xf) != 0x6000 || dma_reg_r(0x10) != 0x6200 ||
						dma_reg_r(0x11) != 1 || dma_reg_r(0x12) != 0x2001) fatalerror("MU4 DMA auto-reload mismatch");
					m_saved_dma.clear(); m_saved_dma.seekg(0);
					if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 synchronized DMA restore failed");
					m_dma_output.clear(); m_dma_completions = 0;
				}
				else m_dma->sync_event(2);
			}
			else
			{
				if (m_dma_output != std::vector<u16>({0x1234}) || m_dma_completions || dma_reg_r(0xf) != 0x6040)
					fatalerror("MU4 synchronized DMA pending replay mismatch");
				m_dma->write(0, 0); m_saved_dma.str(std::string());
				m_phase = 44; machine().schedule_soft_reset(); return;
			}
			++m_phase; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase >= 44 && m_phase <= 48)
		{
			unsigned const words = m_phase - 44;
			std::vector<u16> const expected = {0x1234, 0x5678, 0xabcd, 0x9abc};
			if (m_dma_output != std::vector<u16>(expected.begin(), expected.begin() + words) || m_dma_completions != words / 2)
				fatalerror("MU4 DMA frame interrupt/transfer ordering mismatch step=%u", words);
			if (bool(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x0800) != (words >= 2))
				fatalerror("MU4 DMA/McBSP interrupt mux mismatch step=%u", words);
			if (words == 4)
			{
				if (m_dma->read(0) & 8) fatalerror("MU4 non-reloading DMA remains enabled");
				logerror("mu4_dma_sync_conformance: PASS event_gate=1 sorting=1 multiframe=1 auto_reload=1 block_interrupt=1 frame_interrupt=1 no_reload=1 pending_restore=1\n");
				m_phase = 49; machine().schedule_soft_reset(); return;
			}
			m_dma->sync_event(2); ++m_phase; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 49 || m_phase == 50)
		{
			std::vector<u16> const expected = m_phase == 49 ? std::vector<u16>({0x1234}) : std::vector<u16>({0x1234, 0xabcd});
			if (m_dma_output != expected || m_dma_completions != m_phase - 48 || (m_dma->read(0) & 8))
				fatalerror("MU4 DMA held synchronization level mismatch");
			if (m_phase == 49)
			{
				m_dma->write(0, 8); ++m_phase; m_check->adjust(attotime::from_usec(1)); return;
			}
			m_dma->sync_w(2, CLEAR_LINE);
			logerror("mu4_dma_sync_level: PASS ready_before_enable=1 held_level=1 reenable=1\n");
			m_phase = 51; machine().schedule_soft_reset(); return;
		}
		if (m_phase >= 51 && m_phase <= 53)
		{
			// Pin transitions queue synchronized CPU inputs. Save/load must happen
			// after those anonymous timers drain: postload discards them, while the
			// input queue itself is not serialized by MAME.
			if (m_external_checkpoint_pending)
			{
				if (!machine().scheduler().can_save()) fatalerror("MU4 external checkpoint has pending synchronized inputs");
				if (!(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x20))
					fatalerror("MU4 external checkpoint has not latched XINT0");
				m_external_checkpoint_pending = false;
				if (m_phase == 51)
				{
					m_saved_dma.str(std::string()); m_saved_dma.clear();
					if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 external McBSP save failed");
				}
				else
				{
					m_saved_dma.clear(); m_saved_dma.seekg(0);
					if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 external McBSP restore failed");
					if (!(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 0x20))
						fatalerror("MU4 external checkpoint did not restore XINT0");
					m_external_words.clear(); m_external_bits.clear();
				}
				++m_phase; m_check->adjust(attotime::from_usec(1)); return;
			}
			if (m_phase == 51)
			{
				external_clocks(4);
				if (!m_external_bits.empty()) fatalerror("MU4 external McBSP shifted without a frame");
				m_mcbsp0->tx_frame_w(0);
				if (m_external_bits != std::vector<int>({1})) fatalerror("MU4 zero-delay frame first bit not asynchronous");
				m_mcbsp0->control_w(0, 1);
				if ((m_mcbsp0->control_r(1) & 7) != 5) fatalerror("MU4 external XRDY asserted before opposite edge");
				external_clocks(1); // Same edge as the asynchronous first bit cannot shift twice.
				if (m_external_bits.size() != 1) fatalerror("MU4 external McBSP double-shifted first bit");
				if ((m_mcbsp0->control_r(1) & 7) != 7) fatalerror("MU4 external XRDY missing after opposite edge");
				m_mcbsp0->data_w(3, 0x5aa5);
				m_mcbsp0->tx_frame_w(1); m_mcbsp0->tx_frame_w(0); // XFIG ignores this early frame.
				external_clocks(6);
				if (m_external_bits.size() != 7 || !m_external_words.empty()) fatalerror("MU4 external frame progress mismatch");
				m_external_checkpoint_pending = true;
				m_check->adjust(attotime::from_usec(1)); return;
			}
			else
			{
				external_clocks(9 + 16);
				unsigned const first = m_phase == 52 ? 0 : 7;
				if (m_external_words != std::vector<u16>({0xa55a, 0x5aa5}) || m_external_bits.size() != 32 - first)
					fatalerror("MU4 external framed word/save replay mismatch");
				for (unsigned bit = first; bit < 32; ++bit)
					if (m_external_bits[bit - first] != BIT(bit < 16 ? 0xa55a : 0x5aa5, 15 - (bit & 15)))
						fatalerror("MU4 external framed bit order mismatch");
				external_clocks(8);
				if (m_external_words.size() != 2) fatalerror("MU4 external McBSP advanced beyond frame length");
				if (m_phase == 52)
				{
					m_external_checkpoint_pending = true;
					m_check->adjust(attotime::from_usec(1)); return;
				}
				else
				{
					m_mcbsp0->data_w(3, 0x1234); m_mcbsp0->tx_frame_w(1); m_mcbsp0->tx_frame_w(0);
					external_reg_w(1, 0); external_clocks(32);
					if (m_external_words.size() != 2) fatalerror("MU4 external reset failed to cancel frame");
					m_external_words.clear(); m_external_bits.clear();
					external_reg_w(14, 0); external_reg_w(4, 0); external_reg_w(5, 1);
					m_mcbsp0->tx_clock_w(0); m_mcbsp0->tx_frame_w(0); external_reg_w(1, 1);
					m_mcbsp0->data_w(3, 0xa55a); m_mcbsp0->tx_frame_w(1);
					if (!m_external_bits.empty()) fatalerror("MU4 delayed frame shifted before clock");
					external_clocks(8, false);
					if (m_external_words != std::vector<u16>({0x005a}) || m_external_bits.size() != 8)
						fatalerror("MU4 external polarity/data delay mismatch");
					m_external_words.clear(); m_external_bits.clear();
					external_reg_w(1, 0); external_reg_w(4, 0x20); external_reg_w(5, 2);
					m_mcbsp0->tx_frame_w(0); external_reg_w(1, 1);
					m_mcbsp0->data_w(3, 0xa55a); m_mcbsp0->tx_frame_w(1);
					external_clocks(1, false);
					if (!m_external_bits.empty()) fatalerror("MU4 two-bit delay emitted on first clock");
					external_clocks(12, false);
					if (m_external_words != std::vector<u16>({0x055a}) || m_external_bits.size() != 12)
						fatalerror("MU4 external two-bit delay mismatch");
					logerror("mu4_mcbsp_external: PASS frame_gate=1 stereo=1 bit_order=1 ignore=1 asynchronous=1 delay=1 polarity=1 cancel=1 pending_restore=1 synchronized_inputs=1\n");
					m_phase = 70; m_check->adjust(attotime::from_usec(1)); return;
				}
			}
			++m_phase; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase >= 70 && m_phase <= 73)
		{
			// Each physical-input variant yields so synchronized IRQ events can drain.
			check_external_underrun(m_phase - 70);
			if (m_phase < 73) { ++m_phase; m_check->adjust(attotime::from_usec(1)); return; }
			m_mcbsp0->tx_frame_w(0); m_mcbsp0->tx_frame_w(1); external_clocks(7, false);
			m_phase = 66; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 66 || m_phase == 67)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 underrun checkpoint has pending synchronized inputs");
			if (m_phase == 66)
			{
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 underrun save failed");
			}
			else
			{
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 underrun restore failed");
			}
			m_mcbsp0->control_w(0, 1);
			if ((m_mcbsp0->control_r(1) & 7) != 3) fatalerror("MU4 underrun save/restore lost XEMPTY or XRDY");
			m_external_words.clear(); m_external_bits.clear(); external_clocks(9, false);
			if (m_external_words != std::vector<u16>({0x1234}) || m_external_bits.size() != 9)
				fatalerror("MU4 underrun save/restore shifted word mismatch");
			for (unsigned bit = 0; bit < 9; ++bit)
				if (m_external_bits[bit] != BIT(0x1234, 8 - bit)) fatalerror("MU4 underrun save/restore bit mismatch");
			if (m_phase == 66) { m_phase = 67; m_check->adjust(attotime::from_usec(1)); return; }
			logerror("mu4_mcbsp_underrun_restore: PASS partial_word=1 xempty=1 ready=1 bits=9\n");
			receive_setup(0, 0); receive_word(0x11, 8); receive_word(0x22, 8); receive_word(0x33, 8);
			m_phase = 68; m_check->adjust(attotime::from_usec(1)); return;
		}
		if (m_phase == 68 || m_phase == 69)
		{
			if (!machine().scheduler().can_save()) fatalerror("MU4 overrun checkpoint has pending synchronized inputs");
			if (m_phase == 68)
			{
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 overrun save failed");
			}
			else
			{
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 overrun restore failed");
			}
			if ((receive_status() & 7) != 7 || m_mcbsp0->data_r(1) != 0x11 || (receive_status() & 7) != 1)
				fatalerror("MU4 overrun save/restore lost unread DRR or RFULL");
			receive_bit(0);
			if (m_mcbsp0->data_r(1) != 0x22 || (receive_status() & 6)) fatalerror("MU4 overrun save/restore lost buffered RBR");
			if (m_phase == 68) { m_phase = 69; m_check->adjust(attotime::from_usec(1)); return; }
			logerror("mu4_mcbsp_overrun_restore: PASS rfull=1 drr=1 rbr=1 read_clear=1\n");
			m_saved_dma.str(std::string()); m_phase = 25; machine().schedule_soft_reset(); return;
		}
		if (m_phase >= 25 && m_phase <= 29)
		{
			auto &data = m_cpu->space(AS_DATA);
			auto &program = m_cpu->space(AS_PROGRAM);
			if (m_phase == 28)
			{
				if ((data.read_word(0x54) & 1) || data.read_word(0x6100) != 0x1234) fatalerror("MU4 DMA pre-restore transfer failed");
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE || !(data.read_word(0x54) & 1) || data.read_word(0x6100) != 0xffff)
					fatalerror("MU4 DMA pending save restore mismatch");
				data.write_word(0x55, 0x32);
				if (data.read_word(0x57) != 0x1032) fatalerror("MU4 DMA reload-bank save restore mismatch");
				m_phase = 29;
				m_check->adjust(attotime::from_usec(1));
				return;
			}
			if (data.read_word(0x54) & 1) fatalerror("MU4 DMA completion bit remains set");
			if (m_dma->source_matches(0, AS_DATA, 0x6000)) fatalerror("MU4 completed DMA source inspection remained active");
			if (m_phase == 25 && (program.read_word(0x2ffff) != 0x1234 || program.read_word(0x20000) != 0xabcd))
				fatalerror("MU4 DMA program page wrap mismatch");
			if (m_phase == 26 && data.read_word(0x6100) != 0x1234) fatalerror("MU4 DMA zero-count single-word mismatch");
			if (m_phase == 27 && data.read_word(0x6100) != 0xffff) fatalerror("MU4 DMA canceled transfer wrote data");
			if (m_phase == 27)
			{
				for (unsigned bank : {0x24U, 0x2aU, 0x2eU, 0x32U, 0x36U, 0x3aU})
				{
					data.write_word(0x55, bank);
					for (unsigned field = 0; field < 4; ++field) data.write_word(0x56, 0x1000 + bank + field);
					data.write_word(0x55, bank);
					for (unsigned field = 0; field < 4; ++field)
						if (data.read_word(0x56) != (field == 3 ? bank + field : 0x1000 + bank + field))
							fatalerror("MU4 DMA reload bank masking mismatch bank=%02x field=%u", bank, field);
				}
				for (unsigned reserved : {0x28U, 0x29U})
				{
					data.write_word(0x55, reserved); data.write_word(0x57, 0xffff);
					if (data.read_word(0x57)) fatalerror("MU4 DMA reserved bank not zero");
				}
				data.write_word(0x54, 1);
				m_saved_dma.str(std::string()); m_saved_dma.clear();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 DMA pending save failed");
				data.write_word(0x55, 0x32); data.write_word(0x57, 0xffff);
				m_phase = 28;
				m_check->adjust(attotime::from_usec(1));
				return;
			}
			if (m_phase == 29)
			{
				if (data.read_word(0x6100) != 0x1234) fatalerror("MU4 DMA restored transfer failed");
				m_saved_dma.str(std::string());
				logerror("mu4_dma_conformance: PASS deferred=1 page_wrap=1 zero_count=1 cancel=1 pending_restore=1 reload_banks=6 frame_mask=1 reserved=1\n");
				logerror("mu4_dma_source_inspection: PASS disabled=1 active=1 space=1 address=1 channel=1 index_preserved=1\n");
				m_phase = 0;
			}
			else ++m_phase;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 24)
		{
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x3aea)
				fatalerror("MU4 original loader failed pc=%04x sp=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
			if (m_cpu->space(AS_DATA).read_word(0x54) & 1) fatalerror("MU4 loader returned before DMA completion");
			auto word = [this](unsigned offset) -> u16
			{
				if (offset + 1 >= m_segment.length()) fatalerror("MU4 loader source bounds");
				return (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
			};
			unsigned segment = 0;
			while (word(segment) != 0xaa22)
				segment += 10 + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
			unsigned cursor = segment + 6, records = 0, checked = 0;
			unsigned const end = cursor + (unsigned(word(segment + 2)) << 16) + word(segment + 4);
			while (word(cursor))
			{
				unsigned const count = word(cursor);
				u32 const destination = (u32(word(cursor + 2)) << 16) | word(cursor + 4);
				cursor += 6;
				if (cursor + count * 2 > end) fatalerror("MU4 loader section bounds");
				bool const program = destination > 0xffff;
				// Independent expected rule from original 2f00..2f0e and 2f60..2f70.
				u32 const address = (destination & 0x8000) ? destination : destination & 0xffff;
				for (unsigned i = 0; i < count; ++i)
				{
					u32 const target = (program ? address & 0x7f0000 : 0) | u16(address + i);
					u16 const actual = m_cpu->space(program ? AS_PROGRAM : AS_DATA).read_word(target);
					if (actual != word(cursor + i * 2))
						fatalerror("MU4 DMA destination mismatch space=%s address=%06x actual=%04x expected=%04x", program ? "program" : "data", target, actual, word(cursor + i * 2));
				}
				cursor += count * 2;
				checked += count;
				++records;
			}
			if (cursor + 2 != end || checked != 51921) fatalerror("MU4 loader coverage mismatch words=%u", checked);
			logerror("mu4_loader: PASS original_loader=1 dma_complete=1 records=%u words=%u data_and_program=1\n", records, checked);
			capture_native_ram("loaded");
			if ((system_bios() == 14 || system_bios() == 15) && !m_native_runtime_observers_installed)
			{
				// Install once across the reset legs; counters are per-leg observations.
				m_native_runtime_observers_installed = true;
				auto const observe = [this](offs_t address, u16 value, bool write)
				{
					if (m_phase != 30 || machine().side_effects_disabled()) return;
					static constexpr offs_t candidates[] = {0x374a, 0x374d, 0x374e, 0x3753, 0x3754, 0x3756, 0x3757, 0x3b12, 0x3b13};
					auto const found = std::find(std::begin(candidates), std::end(candidates), address);
					if (found == std::end(candidates)) return;
					unsigned &count = (write ? m_native_runtime_writes : m_native_runtime_reads)[found - std::begin(candidates)];
					if (count++ < 16)
						logerror("mu4_native_runtime_word: leg=%u access=%s address=%04x value=%04x pc=%06x\n",
							m_native_reset_leg, write ? "write" : "read", unsigned(address), value, unsigned(m_cpu->state_int(STATE_GENPC)));
				};
				m_cpu->space(AS_DATA).install_read_tap(0x374a, 0x3b13, "mu4_native_runtime_read",
					[observe](offs_t address, u16 &value, u16) { observe(address, value, false); });
				m_cpu->space(AS_DATA).install_write_tap(0x374a, 0x3b13, "mu4_native_runtime_write",
					[observe](offs_t address, u16 &value, u16) { observe(address, value, true); });
			}
			// Observe unchanged 3538 -> loader -> 2000 -> page-2 common-window entry.
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2000, 0x2000, "mu4_native_entry",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_entry_reads; });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x6d62, 0x6d62, "mu4_native_far_entry",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_far_reads; });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x028afc, 0x028afc, "mu4_native_dma_tx_handler",
				[this](offs_t, u16 &, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() &&
						m_cpu->state_int(STATE_GENPC) == 0x028afd) ++m_native_dma_tx_handler;
				});
			m_cpu->space(AS_PROGRAM).install_read_tap(0x029545, 0x029545, "mu4_native_stream_consumer",
				[this](offs_t, u16 &, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() &&
						m_cpu->state_int(STATE_GENPC) == 0x029546)
					{
						++m_native_stream_consumer_entries;
						if (m_native_consumer_state_traces++ < 8)
						{
							auto const disable = machine().disable_side_effects();
							auto &data = m_cpu->space(AS_DATA);
							logerror("mu4_native_consumer_state: mode_bb80=%04x flag94de=%04x flag0062=%04x phase_bb86=%04x phase_bb91=%04x b66d=%04x b64e=%04x count_b650=%04x,%04x\n",
								data.read_word(0xbb80), data.read_word(0x94de), data.read_word(0x62), data.read_word(0xbb86),
								data.read_word(0xbb91), data.read_word(0xb66d), data.read_word(0xb64e), data.read_word(0xb650), data.read_word(0xb651));
						}
					}
				});
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2954f, 0x2963f, "mu4_native_consumer_paths",
				[this](offs_t address, u16 &, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() || m_cpu->state_int(STATE_GENPC) != address + 1) return;
					static constexpr offs_t points[] = {0x2954f, 0x29553, 0x29557, 0x29559, 0x295b2, 0x2963f};
					for (unsigned i = 0; i < std::size(points); ++i)
						if (address == points[i]) ++m_native_consumer_paths[i];
				});
			m_cpu->space(AS_DATA).install_write_tap(0xbb80, 0xbb80, "mu4_native_consumer_mode",
				[this](offs_t, u16 &value, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() && m_native_consumer_mode_writes++ < 16)
						logerror("mu4_native_consumer_mode: value=%04x pc=%06x\n", value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_PROGRAM).install_read_tap(0x39b6d, 0x3acf2, "mu4_native_command_paths",
				[this](offs_t address, u16 &, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() || m_cpu->state_int(STATE_GENPC) != address + 1) return;
					static constexpr offs_t points[] = {0x39b6d, 0x39bc2, 0x39dc7, 0x39f08, 0x39f5e, 0x3acf2};
					for (unsigned i = 0; i < std::size(points); ++i)
						if (address == points[i]) ++m_native_command_paths[i];
				});
			m_cpu->space(AS_DATA).install_write_tap(0xb633, 0xb633, "mu4_native_stream_pending_write",
				[this](offs_t, u16 &value, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled()) return;
					unsigned &count = value ? m_native_stream_pending_sets : m_native_stream_pending_clears;
					if (count++ < 8) logerror("mu4_native_stream_pending: write=%04x pc=%06x\n", value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_DATA).install_read_tap(0xb633, 0xb633, "mu4_native_stream_pending_read",
				[this](offs_t, u16 &value, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled()) return;
					if (m_native_stream_pending_reads++ < 8) logerror("mu4_native_stream_pending: read=%04x pc=%06x\n", value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_DATA).install_write_tap(0x007e, 0x007e, "mu4_native_dispatch_word",
				[this](offs_t, u16 &value, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() && m_native_dispatch_traces++ < 16)
						logerror("mu4_native_dispatch_word: write=%04x pc=%06x\n", value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_DATA).install_write_tap(0x806e, 0x807f, "mu4_native_dispatch_descriptor",
				[this](offs_t address, u16 &value, u16)
				{
					if (m_phase == 30 && !machine().side_effects_disabled() && m_native_descriptor_traces++ < 32)
						logerror("mu4_native_dispatch_descriptor: address=%04x write=%04x pc=%06x\n", unsigned(address), value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2000, 0x207f, "mu4_native_dma_vectors",
				[this](offs_t address, u16 &value, u16)
				{
					// fetch() increments PC before reading. Exclude DMA/table/debug reads.
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1) return;
					u16 const base = m_cpu->state_int(tms320c54x_device::STATE_PMST) & 0xff80;
					if (address != base + 0x68 && address != base + 0x6c) return;
					unsigned &count = address == base + 0x68 ? m_native_dma_rx_vectors : m_native_dma_tx_vectors;
					if (!count++) logerror("mu4_native_dma_vector: address=%04x words=%04x,%04x,%04x,%04x source=%u\n",
						unsigned(address), value, m_cpu->space(AS_PROGRAM).read_word(address + 1),
						m_cpu->space(AS_PROGRAM).read_word(address + 2), m_cpu->space(AS_PROGRAM).read_word(address + 3),
						address == base + 0x68 ? 10 : 11);
				});
			m_cpu->space(AS_DATA).install_write_tap(0xbdb8, 0xbde7, "mu4_native_selection_state",
				[this](offs_t address, u16 &value, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() ||
						(address != 0xbdb8 && address != 0xbdb9 && address != 0xbdbc &&
						 address != 0xbde3 && address != 0xbde4 && address != 0xbde7)) return;
					if (m_native_selection_writes++ < 32)
						logerror("mu4_native_selection_write: address=%04x value=%04x pc=%06x\n",
							unsigned(address), value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			auto const selection_observer = [this](offs_t address, u16 &opcode, u16)
				{
					address &= 0xffff;
					if (m_phase != 30 || !m_native_dma_tx_handler || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1) return;
					static constexpr u16 addresses[] = {
						0x3f37, 0x3f4b, 0x3edc, 0x3ef3, 0x3f3f, 0x3f40, 0x3f5b, 0x3f5f,
						0x3f6b, 0x3da6, 0x3f6d, 0x8b56, 0x8b5a, 0x8b63, 0x8b6c, 0x8b74, 0x4045
					};
					auto const found = std::find(std::begin(addresses), std::end(addresses), address);
					if (found == std::end(addresses)) return;
					unsigned const slot = found - std::begin(addresses);
					if (m_native_selection_traces[slot]++ >= 4) return;
					u16 const st0 = m_cpu->state_int(tms320c54x_device::STATE_ST0), st1 = m_cpu->state_int(tms320c54x_device::STATE_ST1);
					u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
					u16 const base = BIT(st1, 14) ? sp : (st0 & 0x1ff) << 7;
					auto const disable = machine().disable_side_effects();
					auto &data = m_cpu->space(AS_DATA);
					logerror("mu4_native_selection: pc=%06x opcode=%04x st0=%04x st1=%04x sp=%04x base=%04x word007f=%04x ar2=%04x\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, st0, st1, sp, base, data.read_word(0x7f), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)));
					logerror("mu4_native_selection_acc: a=%010llx\n", static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
					if (base >= 0x80) logerror("mu4_native_selection_ram: base=%04x slot38=%04x slot39=%04x slot63=%04x slot64=%04x slot67=%04x\n",
						base, data.read_word(u16(base + 0x38)), data.read_word(u16(base + 0x39)), data.read_word(u16(base + 0x63)),
						data.read_word(u16(base + 0x64)), data.read_word(u16(base + 0x67)));
					if (base >= 0x80) logerror("mu4_native_selection_control: slot3c=%04x\n", data.read_word(u16(base + 0x3c)));
				};
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3da6, 0x4045, "mu4_native_selection", selection_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x28afc, 0x28b74, "mu4_native_isr_selection", selection_observer);
			auto const startup_observer = [this](offs_t address, u16 &opcode, u16)
				{
					address &= 0xffff;
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1) return;
					static constexpr u16 call_sites[] = {
						0x9063, 0x906b, 0x9074, 0x907a, 0x90a5, 0x90a7, 0x90b0,
						0x90b5, 0x90bc, 0x90c4, 0x90c7, 0x90cb, 0x90cd, 0x90d0,
						0x90d2, 0x90d4, 0x90d6, 0x90de, 0x90e0, 0x90e6, 0x90f2,
						0x90f4, 0x90fb, 0x9104, 0x9106, 0x9115
					};
					for (u16 site : call_sites)
						if ((address == site || address == site + 2) && m_native_main_call_traces++ < 64)
							logerror("mu4_native_main_call: site=%04x point=%s pc=%06x opcode=%04x sp=%04x\n",
								site, address == site ? "call" : "return", unsigned(m_cpu->state_int(STATE_GENPC)),
								opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
					unsigned const slot = address == 0x904b ? 0 : address == 0x6dbf ? 1 : address == 0x6d44 ? 2 : address == 0x3f1d ? 3 : 4;
					if (slot == 4 || m_native_startup_fetches[slot]++ >= 4) return;
					logerror("mu4_native_startup_fetch: pc=%06x opcode=%04x sp=%04x\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
				};
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3f1d, 0x6dbf, "mu4_native_startup_common", startup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2904b, 0x29117, "mu4_native_startup_main", startup_observer);
			auto const startup_return_observer = [this](offs_t address, u16 &opcode, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1 ||
						(address > 0x6d61 && address < 0x6dbf) || m_native_startup_return_traces++ >= 120) return;
					auto const disable = machine().disable_side_effects();
					u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
					logerror("mu4_native_startup_return_step: physical=%04x logical=%06x opcode=%04x sp=%04x stack=%04x,%04x,%04x st0=%04x st1=%04x imr=%04x ifr=%04x\n",
						unsigned(address), unsigned(m_cpu->state_int(STATE_GENPC)), opcode, sp,
						m_cpu->space(AS_DATA).read_word(sp), m_cpu->space(AS_DATA).read_word(u16(sp + 1)),
						m_cpu->space(AS_DATA).read_word(u16(sp + 2)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)));
				};
			m_cpu->space(AS_PROGRAM).install_read_tap(0x6d44, 0x6dc5, "mu4_native_startup_return", startup_return_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3d73, 0x3da5, "mu4_native_startup_context", startup_return_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x38c3e, 0x3c6cd, "mu4_native_startup_read",
				[this](offs_t address, u16 &opcode, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != u16(address + 1)) return;
					// Restrict the lower-call census to the settings operation, not earlier boot writes.
					if (address == 0x3c6ba && m_cpu->state_int(tms320c54x_device::STATE_SP) >= 0x1200)
						m_native_settings_buffer_active = true;
					else if (address == 0x3c6bc)
						m_native_settings_buffer_active = false;
					if (m_native_settings_buffer_active)
						for (unsigned i = 0; i < std::size(buffer_calls); ++i)
							if (address == buffer_calls[i] || address == buffer_calls[i] + 2)
							{
								bool const returning = address != buffer_calls[i];
								unsigned &count = returning ? m_native_buffer_returns[i] : m_native_buffer_calls[i];
								if (++count > 4) continue;
								auto const disable = machine().disable_side_effects();
								auto &data = m_cpu->space(AS_DATA);
								logerror("mu4_native_buffer_call: site=%06x point=%s count=%u opcode=%04x sp=%04x dirty=%04x erase=%04x retry=%04x a=%010llx\n",
									unsigned(buffer_calls[i]), returning ? "return" : "call", count, opcode,
									unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
									data.read_word(0x3b14), data.read_word(0x3b10), data.read_word(0xbdac),
									static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
							}
					static constexpr offs_t settings_calls[] = {
						0x38c55, 0x38c5a, 0x38c64, 0x38c75, 0x38c80, 0x38c89,
						0x38c93, 0x38ca1, 0x38caf, 0x38cb9, 0x38cc0, 0x38cc6
					};
					for (offs_t site : settings_calls)
						if ((address == site || address == site + 2) && m_native_settings_call_traces++ < 64)
						{
							auto const disable = machine().disable_side_effects();
							logerror("mu4_native_settings_call: site=%06x point=%s opcode=%04x ar2=%04x a=%010llx\n",
								unsigned(site), address == site ? "call" : "return", opcode,
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					static constexpr offs_t metadata_calls[] = {0x3c414, 0x3c41f, 0x3c43e, 0x3c441, 0x3c44f};
					for (offs_t site : metadata_calls)
						if ((address == site || address == site + 2) && m_native_metadata_call_traces++ < 40)
						{
							auto const disable = machine().disable_side_effects();
							logerror("mu4_native_metadata_call: site=%06x point=%s opcode=%04x sp=%04x ar2=%04x a=%010llx\n",
								unsigned(site), address == site ? "call" : "return", opcode,
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					static constexpr offs_t allocation_calls[] = {0x3c4e9, 0x3c4f0, 0x3c4f7, 0x3c518, 0x3c552};
					for (offs_t site : allocation_calls)
						if ((address == site || address == site + 2) && m_native_allocation_call_traces++ < 40)
						{
							auto const disable = machine().disable_side_effects();
							logerror("mu4_native_allocation_call: site=%06x point=%s opcode=%04x sp=%04x ar1=%04x ar2=%04x ar6=%04x a=%010llx\n",
								unsigned(site), address == site ? "call" : "return", opcode,
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					static constexpr offs_t cache_update_calls[] = {
						0x3c5dd, 0x3c5df, 0x3c5ea, 0x3c63c, 0x3c63e, 0x3c64c,
						0x3c67a, 0x3c67c, 0x3c68a, 0x3c6ba, 0x3c6bd
					};
					for (offs_t site : cache_update_calls)
						if ((address == site || address == site + 2) && m_native_cache_update_traces++ < 48)
						{
							auto const disable = machine().disable_side_effects();
							auto &data = m_cpu->space(AS_DATA);
							u16 const ar6 = m_cpu->state_int(tms320c54x_device::STATE_AR6);
							logerror("mu4_native_cache_update: site=%06x point=%s opcode=%04x sp=%04x ar6=%04x mode=%04x cache=%04x,%04x a=%010llx\n",
								unsigned(site), address == site ? "call" : "return", opcode,
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), ar6,
								data.read_word(u16(ar6 + 2)), data.read_word(0xb4f8), data.read_word(0xb4f9),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					static constexpr offs_t cache_mode_points[] = {0x3c5f3, 0x3c5f8, 0x3c5fb, 0x3c5ff, 0x3c65c, 0x3c6ad};
					for (unsigned i = 0; i < std::size(cache_mode_points); ++i)
						if (address == cache_mode_points[i] && m_native_cache_mode_traces[i]++ < 4)
						{
							auto const disable = machine().disable_side_effects();
							u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
							logerror("mu4_native_cache_mode: pc=%06x opcode=%04x ar0=%04x ar2=%04x sp=%04x input=%04x st0=%04x a=%010llx\n",
								unsigned(address), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)),
								unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)), sp,
								m_cpu->space(AS_DATA).read_word(u16(sp + 7)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)),
								static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
						}
					if ((address <= 0x392c2 || (address >= 0x3941d && address <= 0x3957f)) &&
						(opcode & 0xfff8) == 0xf980 && m_native_startup_subcall_traces++ < 64)
					{
						auto const disable = machine().disable_side_effects();
						logerror("mu4_native_startup_subcall: pc=%06x target=%06x\n", unsigned(address),
							unsigned((u32(opcode & 7) << 16) | m_cpu->space(AS_PROGRAM).read_word(address + 1)));
					}
					static constexpr offs_t points[] = {0x39115, 0x39117, 0x3911a, 0x3911c, 0x3bb3d, 0x3bb5f, 0x3bbd6};
					if (std::find(std::begin(points), std::end(points), address) == std::end(points) ||
						m_native_config_call_traces++ >= 24) return;
					auto const disable = machine().disable_side_effects();
					auto &data = m_cpu->space(AS_DATA);
					logerror("mu4_native_startup_read: pc=%06x opcode=%04x sp=%04x ar1=%04x ar2=%04x buffer1444=%04x,%04x,%04x,%04x,%04x,%04x\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
						data.read_word(0x1444), data.read_word(0x1445), data.read_word(0x1446),
						data.read_word(0x1447), data.read_word(0x1448), data.read_word(0x1449));
				});
			auto const lookup_observer = [this](offs_t address, u16 &opcode, u16)
				{
					// These snapshots are pre-execution; only known opcode boundaries are selected.
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != u16(address + 1)) return;
					if (address >= 0x3035 && address <= 0x305b && address != 0x3035 && address != 0x3050 &&
						address != 0x3052 && address != 0x3053 && address != 0x3057 && address != 0x3058 && address != 0x305a) return;
					auto const disable = machine().disable_side_effects();
					auto &data = m_cpu->space(AS_DATA);
					if (data.read_word(0x145e) != 6 || m_native_lookup_operand_traces++ >= 40) return;
					u16 const sp = m_cpu->state_int(tms320c54x_device::STATE_SP);
					logerror("mu4_native_lookup_operand: pc=%06x opcode=%04x ar0=%04x ar2=%04x ar6=%04x a=%010llx b=%010llx stack=%04x,%04x,%04x,%04x st1=%04x ifr=%04x\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)),
						static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL,
						static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_B)) & 0xffffffffffULL,
						data.read_word(sp), data.read_word(u16(sp+1)), data.read_word(u16(sp+2)), data.read_word(u16(sp+3)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)));
				};
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3bb7c, 0x3bb7c, "mu4_native_lookup_limit", lookup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3bd13, 0x3bd13, "mu4_native_lookup_address", lookup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x329d, 0x329d, "mu4_native_lookup_byte", lookup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3035, 0x305b, "mu4_native_lookup_nand_address", lookup_observer);
			m_cpu->space(AS_PROGRAM).install_read_tap(0x3234, 0x327e, "mu4_native_lookup_cache",
				[this](offs_t address, u16 &opcode, u16)
				{
					if (m_phase != 30 || machine().side_effects_disabled() ||
						u16(m_cpu->state_int(tms320c54x_device::STATE_PC)) != address + 1) return;
					static constexpr u16 points[] = {0x3234, 0x324c, 0x325d, 0x326e, 0x327b, 0x327e};
					auto const found = std::find(std::begin(points), std::end(points), address);
					if (found == std::end(points)) return;
					auto const disable = machine().disable_side_effects();
					auto &data = m_cpu->space(AS_DATA);
					if (data.read_word(0x145e) != 6 || m_native_cache_traces[found - std::begin(points)]++ >= 4) return;
					u16 const source = m_cpu->state_int(tms320c54x_device::STATE_AR6);
					logerror("mu4_native_cache: pc=%06x opcode=%04x ar0=%04x ar1=%04x ar2=%04x source=%04x count=%04x row=%04x,%04x a=%010llx\n",
						unsigned(m_cpu->state_int(STATE_GENPC)), opcode, unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
						source, data.read_word(u16(source+0x11)), data.read_word(u16(source+0x12)), data.read_word(u16(source+0x13)),
						static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)) & 0xffffffffffULL);
				});
			if (measurement_profile()) install_measurement_observers();
			m_cpu->space(AS_DATA).install_write_tap(0x48, 0x49, "mu4_native_mcbsp_config",
				[this](offs_t offset, u16 &value, u16)
				{
					if (machine().side_effects_disabled()) return;
					if (offset == 0x48) m_native_mcbsp_index = value;
					if (m_native_mcbsp_trace++ < 64)
						logerror("mu4_native_mcbsp: write address=%04x index=%04x value=%04x pc=%06x\n", unsigned(offset), m_native_mcbsp_index, value, unsigned(m_cpu->state_int(STATE_GENPC)));
				});
			m_cpu->space(AS_DATA).install_read_tap(0x49, 0x49, "mu4_native_mcbsp_status",
				[this](offs_t, u16 &value, u16)
				{
					if (machine().side_effects_disabled()) return;
					if (m_native_mcbsp_index == 1)
					{
						++m_native_mcbsp_polls;
						m_native_mcbsp_status = value;
					}
				});
			m_cpu->space(AS_DATA).install_read_tap(0x4a, 0x4a, "mu4_native_adjacent_status",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_adjacent_reads; });
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x3538);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_phase = 30;
			if (system_bios() >= 8) m_command->adjust(attotime::from_msec(1));
			m_check->adjust(attotime::from_seconds(8));
			return;
		}
		if (m_phase == 30)
		{
			if (!m_native_entry_reads || !m_native_far_reads)
				fatalerror("MU4 original program transfer missing entry=%u far=%u pc=%06x", m_native_entry_reads, m_native_far_reads, unsigned(m_cpu->state_int(STATE_GENPC)));
			if (system_bios() != 13 && m_serial_tx_words != std::vector<u16>({0x0c10, 0x0818, 0x0a01, 0x0e53, 0x1023, 0x1201}))
				fatalerror("MU4 original serial-setup sequence mismatch");
			if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) fatalerror("MU4 original native stream encountered an illegal instruction");
			if (system_bios() >= 2 && (m_stream_words < native_stream_target() || m_dma_completions < native_stream_target() / 128 ||
				m_native_din_words < native_stream_target() || m_rx_dma_completions < native_stream_target() / 128))
				fatalerror("MU4 original streaming block incomplete tx=%u din=%u tx_dma=%u rx_dma=%u pc=%06x illegal=%u",
					m_stream_words, m_native_din_words, m_dma_completions, m_rx_dma_completions,
					unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
			if (system_bios() >= 2) logerror("mu4_native_stream: PASS words=%u dma_completions=%u\n", m_stream_words, m_dma_completions);
			if (system_bios() >= 2 && system_bios() != 13)
			{
				for (unsigned i = 0; i < 64; ++i)
					if (m_cpu->space(AS_DATA).read_word(0x1980 + i) != 0x1234 || m_cpu->space(AS_DATA).read_word(0x19c0 + i) != 0x5678)
						fatalerror("MU4 native converted-ADC buffer mismatch at frame %u", i);
				logerror("mu4_native_receive: PASS converted_fixture=1 frames=64 din_words=%u rx_dma_completions=%u sorted_buffer=1980,19c0\n", m_native_din_words, m_rx_dma_completions);
				if (system_bios() >= 3 && system_bios() != 14)
				{
					if (m_native_dma_tx_vectors < 7 || m_native_dma_tx_handler < 7)
						fatalerror("MU4 original TX interrupt delivery incomplete vectors=%u handler=%u",
							m_native_dma_tx_vectors, m_native_dma_tx_handler);
					for (unsigned i = 0; i < 64; ++i)
						if (m_cpu->space(AS_DATA).read_word(0x1900 + i) != 0x1234 || m_cpu->space(AS_DATA).read_word(0x1940 + i) != 0x5678)
							fatalerror("MU4 native reloaded RX buffer mismatch at frame %u", i);
					logerror("mu4_native_sustained: PASS words=%u din_words=%u tx_blocks=%u rx_blocks=%u reload_buffer=1900,1940\n",
						m_stream_words, m_native_din_words, m_dma_completions, m_rx_dma_completions);
					logerror("mu4_native_interrupts: PASS tx_vectors=%u tx_handler=%u rx_vectors=%u\n",
						m_native_dma_tx_vectors, m_native_dma_tx_handler, m_native_dma_rx_vectors);
				}
			}
			logerror("mu4_native_entry: PASS original_transfer=1 entry_reads=%u far_reads=%u pc=%06x illegal=%u idle=%u pmst=%04x\n",
				m_native_entry_reads, m_native_far_reads, unsigned(m_cpu->state_int(STATE_GENPC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
			logerror("mu4_native_dma_vector_counts: rx=%u tx=%u imr=%04x ifr=%04x st1=%04x\n",
				m_native_dma_rx_vectors, m_native_dma_tx_vectors,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_IMR)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
			u16 pending;
			{ auto const disable = machine().disable_side_effects(); pending = m_cpu->space(AS_DATA).read_word(0xb633); }
			{
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				logerror("mu4_native_dispatch_snapshot: word007e=%04x descriptor806e=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x writes=%u,%u\n",
					data.read_word(0x7e), data.read_word(0x806e), data.read_word(0x806f), data.read_word(0x8070), data.read_word(0x8071),
					data.read_word(0x8072), data.read_word(0x8073), data.read_word(0x8074), data.read_word(0x8075), m_native_dispatch_traces, m_native_descriptor_traces);
			}
			logerror("mu4_native_stream_pending_counts: sets=%u clears=%u reads=%u final=%04x\n",
				m_native_stream_pending_sets, m_native_stream_pending_clears, m_native_stream_pending_reads, pending);
			logerror("mu4_native_consumer_path_counts: mode_gate=%u flag94de_gate=%u flag0062_gate=%u transfer_entry=%u mode1_test=%u transfer_exit=%u mode_writes=%u\n",
				m_native_consumer_paths[0], m_native_consumer_paths[1], m_native_consumer_paths[2], m_native_consumer_paths[3],
				m_native_consumer_paths[4], m_native_consumer_paths[5], m_native_consumer_mode_writes);
			{
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				logerror("mu4_native_command_paths: enqueue=%u dequeue=%u parser=%u tx_queue=%u command_read=%u dispatch=%u rx_indices=%04x,%04x tx_indices=%04x,%04x queued_words=%04x parser_state=%04x\n",
					m_native_command_paths[0], m_native_command_paths[1], m_native_command_paths[2], m_native_command_paths[3],
					m_native_command_paths[4], m_native_command_paths[5], data.read_word(0x1657), data.read_word(0x1658),
					data.read_word(0x1659), data.read_word(0x165a), data.read_word(0x165e), data.read_word(0x1661));
			}
			if (system_bios() >= 4)
			{
				{
					auto const disable = machine().disable_side_effects();
					if (m_cpu->space(AS_DATA).read_word(0x8074) != 2 || m_cpu->space(AS_DATA).read_word(0x8075) != 0x9545)
						fatalerror("MU4 original stream descriptor has no expected consumer binding");
				}
				if (!m_native_worker_window_started || machine().time() - m_native_worker_window_start < attotime::from_msec(native_worker_tail_ms()))
					fatalerror("MU4 worker observation ended before its %u ms tail", native_worker_tail_ms());
				logerror("mu4_native_worker_window: PASS tail_ms=%u checked_din_prefix=1024 pending=%04x reads=%u\n", native_worker_tail_ms(), pending, m_native_stream_pending_reads);
				logerror("mu4_native_worker_storage: tail_data_reads=%u\n", m_data_reads - m_native_tail_data_reads);
				trace_native_iterator("end");
				logerror("mu4_native_stream_binding: PASS descriptor=806e entry=029545 consumer_entries=%u\n", m_native_stream_consumer_entries);
				logerror("mu4_native_startup_counts: main=%u continuation=%u helper=%u selection_start=%u\n",
					m_native_startup_fetches[0], m_native_startup_fetches[1], m_native_startup_fetches[2], m_native_startup_fetches[3]);
				if (system_bios() >= 7)
				{
					if (!m_native_startup_fetches[1] || !m_native_startup_fetches[2] ||
						!m_native_stream_consumer_entries || !m_native_stream_pending_reads || m_native_stream_pending_clears < 2)
						fatalerror("MU4 longer startup did not activate its original streaming consumer");
					logerror("mu4_native_worker_activation: PASS consumer_entries=%u reads=%u clears=%u full_boot=0\n",
						m_native_stream_consumer_entries, m_native_stream_pending_reads, m_native_stream_pending_clears);
				}
				for (unsigned i = 0; i < std::size(buffer_calls); ++i)
					logerror("mu4_native_buffer_counts: site=%06x calls=%u returns=%u\n",
						unsigned(buffer_calls[i]), m_native_buffer_calls[i], m_native_buffer_returns[i]);
			}
			logerror("mu4_native_mcbsp_boundary: index=%04x status=%04x polls=%u adjacent_reads=%u config_writes=%u tx_words=%u tx_irqs=%u controller_modeled=partial\n",
				m_native_mcbsp_index, m_native_mcbsp_status, m_native_mcbsp_polls, m_native_adjacent_reads, m_native_mcbsp_trace,
				unsigned(m_serial_tx_words.size()), m_serial_tx_irqs);
			if (system_bios() == 8)
			{
				auto const disable = machine().disable_side_effects();
				auto &data = m_cpu->space(AS_DATA);
				if (m_native_command_cursor != 8 || m_native_command_paths[0] != 1 || m_native_command_paths[3] != 3 ||
					data.read_word(0x1657) != 8 || data.read_word(0x1658) != 8 || data.read_word(0x1659) != 3 ||
					data.read_word(0x165a) != 1 || data.read_word(0x165e) != 4 || data.read_word(0x33) != 0x7f || data.read_word(0xbb80))
					fatalerror("MU4 native framed request did not reach its original queued-command/acknowledgement boundary");
				logerror("mu4_native_framed_request: PASS rx_words=8 queued_command=1 ack_words=3 first_tx_register=007f tx_wire=0 processing=0\n");
			}
			if (system_bios() >= 9)
			{
				logerror("mu4_native_command_wire: words=%u tx_irqs=%u\n", unsigned(m_command_tx_words.size()), m_command_tx_irqs);
				for (unsigned i = 0; i < std::min<unsigned>(m_command_tx_words.size(), 64); ++i)
					logerror("mu4_native_command_wire_word: index=%u value=%04x\n", i, m_command_tx_words[i]);
				if (system_bios() == 9)
				{
					std::vector<u16> expected = {0x7f, 1, 0x55, 0x1e, 5, 0xaa, 1, 0x71, 1, 0, 0, 0x80, 0x40, 0x55};
					std::vector<u16> const response(expected.begin() + 3, expected.end());
					expected.insert(expected.end(), response.begin(), response.end());
					expected.insert(expected.end(), response.begin(), response.end());
					if (m_command_tx_words != expected || m_command_wire_bit_count || m_command_ack_cursor)
						fatalerror("MU4 unacknowledged status response did not reproduce three complete wire copies");
					logerror("mu4_native_status_noack: PASS tx_words=36 pin_decode=1 response_copies=3 peer_ack=0 processing=0\n");
				}
				if (system_bios() >= 10 && system_bios() != 13 && system_bios() != 14 &&
					(system_bios() != 15 || m_native_command_paths[5]))
				{
					verify_native_status();
				}
			}
			if (measurement_profile()) verify_native_measurement();
			else if (finish_native_replay()) return;
			if (system_bios() == 14)
			{
				if (m_native_reset_leg != 2 || m_native_reset_settings_mask != 15)
					fatalerror("MU4 native reset fixture missed its reset or firmware-created settings file");
				logerror("mu4_native_reset_storage: PASS mid_rx_byte=1 controller_cleared=1 retained_payloads=6 settings_mask=f original_reload=1\n");
				if (m_native_command_cursor || m_command_rx_irqs || !m_command_tx_words.empty() || m_native_command_paths[5])
					fatalerror("MU4 reset frontier changed; re-evaluate the native control-loop restart contract");
				logerror("mu4_native_reset_frontier: control_restart=0 dispatcher=0 rx_words=0 tx_words=0 tail_ms=%u stream_equivalence=0 board_reset=0 music_decode=0\n", native_worker_tail_ms());
			}
			capture_native_ram("settled");
			if (system_bios() == 15)
			{
				if (m_files_checked != 6 || m_native_reset_settings_mask != 15)
					fatalerror("MU4 fresh-process retained medium lacks original uploads or settings");
				if (!m_native_command_paths[5] && (m_native_command_cursor || m_command_rx_irqs || !m_command_tx_words.empty()))
					fatalerror("MU4 retained control frontier has partial unexplained traffic");
				logerror("mu4_native_retained_storage: PASS fresh_process=1 original_payloads=6 settings_mask=f erase=0 runtime_snapshot=0\n");
				logerror("mu4_native_retained_control: active=%u rx_words=%u tx_words=%u tail_ms=%u board_boot=0 music_decode=0\n",
					m_native_command_paths[5] ? 1 : 0, m_command_rx_irqs, unsigned(m_command_tx_words.size()), native_worker_tail_ms());
			}
			machine().schedule_exit();
			return;
		}
		if (m_phase == 20)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL))
				fatalerror("MU4 segment illegal opcode pc=%04x cursor=%u", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), m_segment_cursor);
			// XF is the original firmware's external busy indication, not a RAM-state override.
			if (m_serial_ready || BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1), 13))
			{
				if (++m_segment_waits > 10000) fatalerror("MU4 segment serial/busy timeout cursor=%u pc=%04x", m_segment_cursor, unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			}
			else if (m_segment_cursor < m_segment.length())
			{
				deliver_serial(m_segment[m_segment_cursor++]);
				m_segment_waits = 0;
			}
			else if (!data.read_word(0x1f6) && data.read_word(0x4bbf) == data.read_word(0x4bc0))
			{
				if (m_serial_reads != m_segment.length() + 2 || data.read_word(0x1a2) != 0x40 || data.read_word(0x1f9) != 0x40 || data.read_word(0x584) != 7)
					fatalerror("MU4 original segment checksum/receive mismatch reads=%u checksum=%04x calculated=%04x", m_serial_reads, data.read_word(0x1a2), data.read_word(0x1f9));
				logerror("mu4_initdisk_segment: PASS segments=7 original_bytes=%u final_checksum=0040 ring_drained=1\n", m_segment_cursor);
				m_phase = 19;
				machine().schedule_soft_reset();
				return;
			}
			else if (++m_segment_waits > 10000)
				fatalerror("MU4 segment completion timeout pc=%04x state=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), data.read_word(0x1f6));
			// Bounded word-ingress cadence, not recovered physical McBSP timing.
			m_check->adjust(attotime::from_msec(1));
			return;
		}
		if (m_phase == 18)
		{
			auto &data = m_cpu->space(AS_DATA);
			unsigned const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
			if (!((pc >= 0x31f0 && pc <= 0x3203) || (pc >= 0x3499 && pc <= 0x34a4)) ||
				m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) || data.read_word(0x1f6) || data.read_word(0x1f5) ||
				data.read_word(0x4bbf) != 0xff || data.read_word(0x4bc0) != 0xff ||
				data.read_word(0x10d) != 0xaa55 || data.read_word(0x19d) != 0xbbcc)
				fatalerror("MU4 original initialization did not reach receive boundary pc=%04x", pc);
			logerror("mu4_initdisk_initialize: PASS original_init=1 erased_input=1 receiver_wait=1 no_received_words=1 writes=%u reads=%u\n",
				unsigned(m_writes.size()), m_data_reads);
			m_phase = 20;
			m_check->adjust(attotime::from_msec(1));
			return;
		}
		if (m_phase == 14 || m_phase == 15 || m_phase == 16)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 serial ISR did not return to idle phase=%u pc=%04x sp=%04x illegal=%u", m_phase,
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
			if (m_phase == 14)
			{
				if (data.read_word(0x4bbf) != 0xff || data.read_word(0x4bc0) != 0xff)
					fatalerror("MU4 original serial ring initialization mismatch");
				deliver_serial(0x12);
			}
			else
			{
				unsigned const index = m_phase - 15;
				if (m_serial_ready || m_serial_reads != index + 1 || data.read_word(0x4bbf) != index ||
					data.read_word(0x4abf + index) != (index ? 0x34 : 0x12))
					fatalerror("MU4 original serial ISR ring delivery mismatch");
				if (m_phase == 16)
				{
					m_phase = 17;
					machine().schedule_soft_reset();
					return;
				}
				deliver_serial(0x34);
			}
			++m_phase;
			m_check->adjust(attotime::from_msec(1));
			return;
		}
		if (m_phase == 8)
		{
			auto &data = m_cpu->space(AS_DATA);
			for (unsigned i = 0; i < 512; ++i)
			{
				u16 const word = data.read_word(0x1746 + i / 2);
				u8 const expected = (i & 1) ? u8(word) : u8(word >> 8);
				if (m_nand->data_r() != expected) fatalerror("MU4 recovery template mismatch byte=%u", i);
			}
			logerror("mu4_storage_template: PASS original_template=512 physical_row=32\n");
			m_phase = 9;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 1 || m_phase == 3 || m_phase == 11)
		{
			if (m_nand->is_busy()) fatalerror("MU4 storage test pattern program did not complete");
			++m_phase;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 5)
		{
			if (m_nand->is_busy()) fatalerror("MU4 flush media read still busy");
			for (unsigned i = 0; i < 256; ++i)
			{
				u16 const low = m_nand->data_r();
				u16 const value = low | (u16(m_nand->data_r()) << 8);
				if (value != pattern(m_page * 256 + i))
					fatalerror("MU4 flush media mismatch page=%u word=%u actual=%04x", m_page, i, value);
			}
			for (unsigned i = 0; i < 16; ++i)
				if (m_nand->data_r() != 0xff) fatalerror("MU4 flush spare fill mismatch");
			if (++m_page < 32) { start_media_read(); return; }
			logerror("mu4_storage_flush: PASS pages=32 words=8192 spare=512 erase=1 program=32 readback=32\n");
			m_phase = 6;
			machine().schedule_soft_reset();
			return;
		}
		u16 const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
		if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) ||
				!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
			fatalerror("MU4 original storage routine did not finish: pc=%04x illegal=%u writes=%u reads=%u",
				pc, unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), unsigned(m_writes.size()), m_data_reads);
		if (m_phase == 9)
		{
			unsigned mutations = 0;
			for (u16 value : m_writes) mutations += value == 0x0160 || value == 0x0180;
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 1 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_data_reads || mutations)
				fatalerror("MU4 format prerequisite contract mismatch");
			logerror("mu4_storage_format: PASS missing_boot_record rejected=1 media_mutations=0\n");
			m_phase = 10;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 10 || m_phase == 12)
		{
			unsigned spare_reads = 0, status_reads = 0;
			for (u16 value : m_writes) { spare_reads += value == 0x0150; status_reads += value == 0x0170; }
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != (m_phase == 10 ? 0xff : 0) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_busy_reads ||
				spare_reads != (m_phase == 10 ? 2 : 1) || status_reads != spare_reads)
				fatalerror("MU4 InitDisk marker contract mismatch a=%llx sp=%04x spare=%u status=%u reads=%u busy=%u",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), spare_reads, status_reads, m_data_reads, m_busy_reads);
			if (m_phase == 12)
			{
				logerror("mu4_initdisk_marker: PASS erased=ff bad_marker=00 original_spare_reader=1\n");
				m_phase = 13;
				machine().schedule_soft_reset();
				return;
			}
			// External bad-block fixture: program only spare byte 5 of physical page zero.
			m_nand->command_w(0x50); m_nand->command_w(0x80);
			m_nand->address_w(5); m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(0);
			m_nand->data_w(0); m_nand->command_w(0x10);
			m_phase = 11;
			m_check->adjust(attotime::from_usec(201));
			return;
		}
		if (m_phase == 13)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || data.read_word(0x2000))
				fatalerror("MU4 InitDisk scan return/reserved bitmap mismatch");
			for (unsigned i = 0; i < 256; ++i)
				// 256d byte-swaps its source bitmap in place before NAND writeback.
				if (data.read_word(0x2100 + i) != (i == 0 ? 0x0100 : 0))
					fatalerror("MU4 InitDisk bitmap mismatch group=%u value=%04x", i, data.read_word(0x2100 + i));
			logerror("mu4_initdisk_scan: PASS blocks=4096 bad_block=0 reserved_bad=0 reads=%u busy_reads=%u\n", m_data_reads, m_busy_reads);
			m_phase = 14;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 17)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0x1234 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				m_cpu->space(AS_DATA).read_word(0x4bc0) != 1)
				fatalerror("MU4 original serial word consumer mismatch");
			logerror("mu4_initdisk_serial: PASS vector=00d8 register=0031 words=2 value=1234 firmware_ring=1\n");
			m_phase = 18;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 6)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 1 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_busy_reads || !m_data_reads)
				fatalerror("MU4 erased mount contract mismatch a=%llx sp=%04x reads=%u busy=%u",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), m_data_reads, m_busy_reads);
			logerror("mu4_storage_mount: PASS erased_boot_sector rejected=1 reads=%u busy_reads=%u\n", m_data_reads, m_busy_reads);
			m_phase = 7;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 19)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_data_reads || !m_busy_reads)
				fatalerror("MU4 InitData mount of original InitDisk medium failed a=%llx sp=%04x",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
			logerror("mu4_initdisk_mount: PASS original_initdata_mount=1 original_initdisk_media=1\n");
			m_phase = 21;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 21 || m_phase == 23)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 directory reader did not return with balanced stack");
			logerror("mu4_directory: a=%llx sp=%04x name=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x ext=%04x,%04x,%04x length=%04x,%04x\n",
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
				data.read_word(0x36b0), data.read_word(0x36b1), data.read_word(0x36b2), data.read_word(0x36b3),
				data.read_word(0x36b4), data.read_word(0x36b5), data.read_word(0x36b6), data.read_word(0x36b7),
				data.read_word(0x36b9), data.read_word(0x36ba), data.read_word(0x36bb), data.read_word(0x36c2), data.read_word(0x36c3));
			if (m_phase == 23 && m_cpu->state_int(tms320c54x_device::STATE_A) == 1)
			{
				if (m_files_checked != 6) fatalerror("MU4 file count mismatch files=%u", m_files_checked);
				logerror("mu4_file_readback: PASS files=6 original_readers=1 complete_payloads=1\n");
				m_phase = 24;
				machine().schedule_soft_reset();
				return;
			}
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_A) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 original directory reader failed pc=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			m_file_size = (unsigned(data.read_word(0x36c2)) << 16) | data.read_word(0x36c3);
			if (((system_bios() == 14 && m_native_reset_leg == 2) || system_bios() == 15) && (m_file_size == 1800 || m_file_size == 56))
			{
				// Original startup creates these settings files. Preserve them and enumerate
				// onward through the unchanged next-entry ABI, not a fabricated list.
				static constexpr char const *settings[] = { "TRACKLST", "SETTING1", "SETTING2", "SETTING3" };
				unsigned match = std::size(settings);
				for (unsigned entry = 0; entry < std::size(settings); ++entry)
				{
					bool equal = true;
					for (unsigned i = 0; i < 9; ++i) equal &= data.read_word(0x36b0 + i) == u8(settings[entry][i]);
					if (equal) match = entry;
				}
				if (match == std::size(settings) || m_file_size != (match ? 56 : 1800))
					fatalerror("MU4 reset encountered an unknown non-upload file");
				if (data.read_word(0x36b9) != 'B' || data.read_word(0x36ba) != 'I' || data.read_word(0x36bb) != 'N' ||
					BIT(m_native_reset_settings_mask, match)) fatalerror("MU4 reset settings entry is invalid or duplicated");
				m_native_reset_settings_mask |= 1U << match;
				logerror("mu4_native_reset_settings: name=%s.BIN bytes=%u retained=1\n", settings[match], m_file_size);
				m_phase = 23;
				machine().schedule_soft_reset();
				return;
			}
			m_file_source = 0;
			for (unsigned offset = 0; offset < m_segment.length(); )
			{
				unsigned const size = (unsigned(m_segment[offset + 2]) << 24) | (unsigned(m_segment[offset + 3]) << 16) |
					(unsigned(m_segment[offset + 4]) << 8) | m_segment[offset + 5];
				if (size == m_file_size)
				{
					if (m_file_source) fatalerror("MU4 ambiguous source size");
					m_file_source = offset + 6;
					m_file_marker = (u16(m_segment[offset]) << 8) | m_segment[offset + 1];
				}
				offset += size + 10;
			}
			if (!m_file_source || (m_file_size & 1)) fatalerror("MU4 file size has no original segment size=%u", m_file_size);
			char const *name = nullptr;
			switch (m_file_marker)
			{
			case 0xaa22: name = "MCUSI16 "; break;
			case 0xaa44: name = "MP3SI16 "; break;
			case 0xaa88: name = "RERSI16 "; break;
			case 0xaabb: name = "AACSI16 "; break;
			case 0xaadd: name = "USBSI16 "; break;
			case 0xaa99: name = "RELSI16 "; break;
			default: fatalerror("MU4 unexpected file marker %04x", m_file_marker);
			}
			for (unsigned i = 0; i < 9; ++i)
				if (data.read_word(0x36b0 + i) != u8(name[i])) fatalerror("MU4 directory name mismatch marker=%04x", m_file_marker);
			if (data.read_word(0x36b9) != 'B' || data.read_word(0x36ba) != 'I' || data.read_word(0x36bb) != 'N')
				fatalerror("MU4 directory extension mismatch");
			m_file_cursor = 0;
			m_phase = 22;
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 22)
		{
			auto &data = m_cpu->space(AS_DATA);
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_A) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200)
				fatalerror("MU4 original file reader failed pc=%04x", unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
			for (unsigned i = 0; i < m_read_words; ++i)
			{
				unsigned const source = m_file_source + m_file_cursor + i * 2;
				u16 const expected = (u16(m_segment[source]) << 8) | m_segment[source + 1];
				if (data.read_word(0x6000 + i) != expected)
					fatalerror("MU4 file mismatch marker=%04x byte=%u actual=%04x expected=%04x", m_file_marker, m_file_cursor + i * 2, data.read_word(0x6000 + i), expected);
			}
			m_file_cursor += m_read_words * 2;
			if (m_file_cursor == m_file_size)
			{
				if (std::find(m_verified_files.begin(), m_verified_files.end(), m_file_marker) != m_verified_files.end())
					fatalerror("MU4 directory repeats segment %04x", m_file_marker);
				m_verified_files.push_back(m_file_marker);
				++m_files_checked;
				logerror("mu4_file_payload: PASS marker=%04x bytes=%u\n", m_file_marker, m_file_size);
				m_phase = 23;
			}
			machine().schedule_soft_reset();
			return;
		}
		if (m_phase == 7)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 || !m_busy_reads)
				fatalerror("MU4 original template writer failed");
			m_nand->command_w(0); m_nand->address_w(0); m_nand->address_w(32);
			m_nand->address_w(0); m_nand->address_w(0);
			m_phase = 8;
			m_check->adjust(attotime::from_usec(11));
			return;
		}
		if (m_phase == 4)
		{
			unsigned erase = 0, program = 0;
			for (u16 value : m_writes) { erase += value == 0x0160; program += value == 0x0180; }
			if (erase != 1 || program != 32 || !m_busy_reads ||
				m_cpu->state_int(tms320c54x_device::STATE_A) != 0 ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x4000) ||
				m_cpu->space(AS_DATA).read_word(0x3b14) != 0)
				fatalerror("MU4 flush failed a=%llx sp=%04x erase=%u program=%u busy_reads=%u",
					(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), erase, program, m_busy_reads);
			m_phase = 5;
			start_media_read();
			return;
		}
		std::vector<u16> const expected = m_phase ?
			std::vector<u16>{0x0100, 0x0200, 0x0200, 0x0200, 0x0201} :
			std::vector<u16>{0x0100, 0x0200, 0x0201, 0x0200, 0x0200};
		if (m_writes != expected || m_data_reads != 2 || !m_busy_reads ||
				m_cpu->state_int(tms320c54x_device::STATE_A) != (m_phase ? 0x3412 : 0xffff) ||
				m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1200 ||
				m_first_data - m_read_started < attotime::from_usec(10))
		{
			for (u16 word : m_writes) logerror("mu4_storage_write=%04x\n", word);
			fatalerror("MU4 storage contract mismatch: pc=%04x a=%010llx sp=%04x busy_reads=%u data_reads=%u",
				pc, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), m_busy_reads, m_data_reads);
		}
		if (!m_phase)
		{
			logerror("mu4_storage_erased: PASS row=1 value=ffff bio_reads=%u busy_reads=%u\n", m_bio_reads, m_busy_reads);
			// External device-conformance pattern, not a provisioned filesystem or firmware state.
			m_nand->command_w(0); m_nand->command_w(0x80);
			m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(0); m_nand->address_w(1);
			m_nand->data_w(0x12); m_nand->data_w(0x34); m_nand->command_w(0x10);
			m_phase = 1;
			m_check->adjust(attotime::from_usec(201));
			return;
		}
		logerror("mu4_storage_original: PASS rows=1,65536 values=ffff,3412 writes=5 reads=2 bio_reads=%u busy_reads=%u\n",
			m_bio_reads, m_busy_reads);
		// Non-erased marker makes the original block erase necessary before programming.
		m_nand->command_w(0); m_nand->command_w(0x80);
		m_nand->address_w(0); m_nand->address_w(64); m_nand->address_w(0); m_nand->address_w(0);
		m_nand->data_w(0); m_nand->command_w(0x10);
		m_phase = 3;
		m_check->adjust(attotime::from_usec(201));
	}
};
static INPUT_PORTS_START(mu4_storage_test) INPUT_PORTS_END
ROM_START(mu4nand)
	ROM_SYSTEM_BIOS(0, "setup", "Original storage, loader and serial setup")
	ROM_SYSTEM_BIOS(1, "stream", "Original streaming frontier (incomplete)")
	ROM_SYSTEM_BIOS(2, "sustain", "Original eight-block digital streaming fixture")
	ROM_SYSTEM_BIOS(3, "worker", "Original streaming worker observation (100 ms tail)")
	ROM_SYSTEM_BIOS(4, "settle", "Original startup observation (1 second tail)")
	ROM_SYSTEM_BIOS(5, "scan", "Original startup observation (5 second tail)")
	ROM_SYSTEM_BIOS(6, "startup", "Original bounded storage startup observation (20 second tail)")
	ROM_SYSTEM_BIOS(7, "command", "Original framed serial status request (20 second tail)")
	ROM_SYSTEM_BIOS(8, "wire", "Original status request with externally clocked serial TX")
	ROM_SYSTEM_BIOS(9, "wireack", "Original serial status transaction with peer acknowledgement")
	ROM_SYSTEM_BIOS(10, "pins", "Original status transaction with McBSP2 RX and TX pins")
	ROM_SYSTEM_BIOS(11, "replay", "Original pin-level status transaction with mid-byte replay")
	ROM_SYSTEM_BIOS(12, "measure", "Original pin-level sample measurement observation")
	ROM_SYSTEM_BIOS(13, "reset", "Original mid-byte bench reset observation (incomplete)")
	ROM_SYSTEM_BIOS(14, "retained", "Original fresh-process retained-media observation")
	ROM_SYSTEM_BIOS(15, "bootstrap", "Original uploaded startup observation (isolated bench)")
	ROM_SYSTEM_BIOS(16, "bootstatus", "Original uploaded startup with pin-level status transaction")
	ROM_SYSTEM_BIOS(17, "bootreset", "Original uploaded startup with mid-byte reset and status restart")
	ROM_SYSTEM_BIOS(18, "bootreplay", "Original uploaded startup with mid-byte save-state replay")
	ROM_SYSTEM_BIOS(19, "bootmeasure", "Original uploaded startup with pin-level sample measurement")
	ROM_REGION(741916, "segment", 0)
	ROM_LOAD("mu4_initdata_container.bin", 0, 741916, CRC(e0c05bf2) SHA1(5ff0b99c8d93b6ef2cda0bcd002810a4ab7a0e8f))
	ROM_REGION16_LE(240, "disk_vectors", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_vectors.bin", 0, 240, CRC(2f1b5de4) SHA1(145048a918768dbee2d5fce5213a6e26ece50edf))
	ROM_REGION16_LE(44, "disk_helpers", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_helpers.bin", 0, 44, CRC(6e84f71a) SHA1(fa38ceaa537490d6d57c84a6406990aad5b869a6))
	ROM_REGION16_LE(19064, "initdisk", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_library.bin", 0, 19064, CRC(e9ecd12c) SHA1(748672ed6cac0e864fe0e70a9026b8cafaf8a7bc))
	ROM_REGION16_LE(2522, "disk_cinit", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdisk_cinit.bin", 0, 2522, CRC(535a37c1) SHA1(78d90095d93a4cbed326dff901671ece0004462e))
	ROM_REGION16_LE(546, "cinit", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_cinit.bin", 0, 546, CRC(9d1fb99f) SHA1(d08dd94e471519ed69869b3685a8a6c9cd393f58))
	ROM_REGION16_LE(92, "trampolines", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_trampolines.bin", 0, 92, CRC(0de68a60) SHA1(3b2b1ac46f7c0cef5c5c1288a7cb0bcf1bb817de))
	ROM_REGION16_LE(6758, "entry", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_entry.bin", 0, 6758, CRC(aa68038b) SHA1(c4d0c3b8211569ee4ec23d2945a9855b5423ec89))
	ROM_REGION16_LE(11160, "library", 0)
	ROM_LOAD16_WORD_SWAP("mu4_initdata_library.bin", 0, 11160, CRC(f3b1ddd5) SHA1(e6d082993f823ebbff46f33bc36402a8e8c0c5d8))
ROM_END
} // anonymous namespace
CONS(2026, mu4nand, 0, 0, test, mu4_storage_test, mu4_storage_test_state, empty_init,
		"Test", "MU4 original NAND routine boundary", MACHINE_NO_SOUND_HW)
