// license:BSD-3-Clause
// copyright-holders:Gaz

// Isolated original MU4 routine execution, not a DA150 or handset boot profile.
#include "emu.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "machine/nandflash.h"
#include "tms320c54x_dma.h"
#include "tms320c54x_mcbsp.h"
#include "tlv320aic23.h"
#include <vector>
#include <sstream>

namespace {
class mu4_storage_test_state : public driver_device
{
public:
	mu4_storage_test_state(machine_config const &config, device_type type, char const *tag)
				: driver_device(config, type, tag), m_cpu(*this, "cpu"), m_dma(*this, "dma"), m_mcbsp(*this, "mcbsp1"), m_mcbsp0(*this, "mcbsp0"), m_program_ram(*this, "program_ram"), m_dma_sink(*this, "dma_sink"), m_nand(*this, "nand"), m_cinit(*this, "cinit"),
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
		m_mcbsp0->tx_word_cb().set(FUNC(mu4_storage_test_state::stream_tx));
		m_mcbsp0->tx_bit_cb().set([this](int state) { if (m_phase >= 51 && m_phase <= 53) m_external_bits.push_back(state); });
		m_mcbsp0->tx_event_cb().set([this](int state) { m_dma->sync_w(2, state); });
		m_mcbsp0->tx_irq_cb().set([this](int state) { m_cpu->set_input_line(5, state); });
		m_mcbsp0->rx_event_cb().set([this](int state) { m_dma->sync_w(1, state); });
		m_mcbsp0->rx_irq_cb().set([this](int state) { if (state) ++m_rx_irqs; m_cpu->set_input_line(4, state); });
		TLV320AIC23(config, "codec", 11'995'200);
		subdevice<tlv320aic23_device>("codec")->bclk_cb().set([this](int state) {
			if (m_phase >= 54 && m_phase <= 57) { m_codec_edges.push_back(state); if (state) ++m_codec_clocks; }
			else m_mcbsp0->tx_clock_w(state);
		});
		subdevice<tlv320aic23_device>("codec")->frame_cb().set([this](int state) {
			if (m_phase >= 54 && m_phase <= 57) { m_codec_edges.push_back(2 + state); if (state) m_codec_frames.push_back(m_codec_clocks); }
			else m_mcbsp0->tx_frame_w(state);
		});
		SAMSUNG_K9K1208U0A(config, m_nand);
		m_nand->rnb_wr_callback().set(FUNC(mu4_storage_test_state::ready_w));
	}
private:
	required_device<tms320c54x_device> m_cpu;
	required_device<tms320c54x_dma_device> m_dma;
	required_device<tms320c54x_mcbsp_device> m_mcbsp;
	required_device<tms320c54x_mcbsp_device> m_mcbsp0;
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
	}
	unsigned m_stream_words = 0, m_stream_config_writes = 0;
	unsigned m_codec_clocks = 0;
	std::vector<unsigned> m_codec_frames;
	std::vector<int> m_codec_edges, m_codec_saved_edges;
	attotime m_codec_snapshot_time;
	std::vector<u16> m_external_words;
	std::vector<int> m_external_bits;
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
	void stream_tx(u16 value)
	{
		++m_stream_words;
		if (m_phase >= 51 && m_phase <= 53) m_external_words.push_back(value);
		if (m_phase == 30 && m_stream_words <= 8) logerror("mu4_native_stream: word=%04x\n", value);
		if (m_phase == 30 && system_bios() == 2 && m_stream_words == 128) m_check->adjust(attotime::zero);
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
		if (m_phase == 30 && system_bios() == 2) subdevice<tlv320aic23_device>("codec")->control_word_w(value);
		m_serial_tx_words.push_back(value);
		if (m_phase == 30 && m_serial_tx_words.size() <= 16) logerror("mu4_native_tx: word=%04x\n", value);
		// End the serial-setup fixture at its observed complete control stream,
		// before the separate streaming-profile acceptance.
		if (m_phase == 30 && system_bios() != 2 && m_serial_tx_words.size() == 6) m_check->adjust(attotime::zero);
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
	// Word-level receive fixture only; serial clocks/framing are not modeled here.
	u16 serial_r()
	{
		if (!m_serial_ready) fatalerror("MU4 serial read without delivered word");
		m_serial_ready = false;
		m_serial_regs[0][0] &= ~u16(2);
		++m_serial_reads;
		return m_serial_word;
	}
	u16 serial_config_r(offs_t offset) { return offset ? m_serial_regs[0][m_serial_index[0]] : m_serial_index[0]; }
	void serial_config_w(offs_t offset, u16 value)
	{
		if (offset) m_serial_regs[0][m_serial_index[0]] = value;
		else m_serial_index[0] = value & 31;
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
	virtual void machine_start() override
	{
		m_check = timer_alloc(FUNC(mu4_storage_test_state::check), this);
	}
	virtual void machine_reset() override
	{
		m_direction = m_gpio = 0;
		m_ready = true;
		m_bio_reads = m_busy_reads = m_data_reads = 0;
		m_writes.clear();
		m_serial_tx_words.clear(); m_serial_tx_bits.clear();
		m_serial_tx_irqs = 0;
		m_dma_output.clear(); m_dma_completions = 0;
		m_rx_dma_completions = m_rx_irqs = 0;
		m_stream_words = m_stream_config_writes = 0;
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
			data.write_word(0x54, 1);
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
				m_phase = 55; m_check->adjust(attotime::from_usec(100)); return;
			}
			if (m_phase == 55)
			{
				if (m_codec_clocks != 1200 || m_codec_frames.size() != 5)
					fatalerror("MU4 codec clock rate mismatch clocks=%u frames=%u", m_codec_clocks, unsigned(m_codec_frames.size()));
				for (unsigned i = 1; i < m_codec_frames.size(); ++i)
					if (m_codec_frames[i] - m_codec_frames[i - 1] != 272) fatalerror("MU4 codec frame divider mismatch");
				m_saved_dma.str({}); m_saved_dma.clear();
				m_codec_snapshot_time = machine().time();
				if (machine().save().write_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 codec save failed");
				m_codec_edges.clear(); m_phase = 56; m_check->adjust(attotime::from_usec(10)); return;
			}
			if (m_phase == 56)
			{
				m_codec_saved_edges = m_codec_edges;
				m_saved_dma.clear(); m_saved_dma.seekg(0);
				if (machine().save().read_stream(m_saved_dma) != STATERR_NONE) fatalerror("MU4 codec restore failed");
				// Loading inside a timer callback restores scheduler base time, but
				// adjust() still uses this callback's old timestamp. Target the saved
				// observation deadline, not ten microseconds after the old callback.
				m_codec_edges.clear(); m_phase = 57;
				m_check->adjust(m_codec_snapshot_time + attotime::from_usec(10) - machine().time()); return;
			}
			if (m_codec_edges != m_codec_saved_edges)
				fatalerror("MU4 codec pending clock replay mismatch expected=%u actual=%u first=%d/%d", unsigned(m_codec_saved_edges.size()), unsigned(m_codec_edges.size()), m_codec_saved_edges.empty() ? -1 : m_codec_saved_edges.front(), m_codec_edges.empty() ? -1 : m_codec_edges.front());
			codec.control_word_w(0x1200);
			codec.control_word_w(0x1e00);
			if (codec.reg(7) != 1 || codec.reg(9)) fatalerror("MU4 codec reset defaults mismatch");
			logerror("mu4_codec_clock: PASS inactive=1 controls=1 bclk_mclk=1 frame_divider=272 pending_restore=1 reset=1\n");
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
					m_saved_dma.str(std::string()); m_phase = 25; machine().schedule_soft_reset(); return;
				}
			}
			++m_phase; m_check->adjust(attotime::from_usec(1)); return;
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
			// Observe unchanged 3538 -> loader -> 2000 -> page-2 common-window entry.
			m_cpu->space(AS_PROGRAM).install_read_tap(0x2000, 0x2000, "mu4_native_entry",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_entry_reads; });
			m_cpu->space(AS_PROGRAM).install_read_tap(0x6d62, 0x6d62, "mu4_native_far_entry",
				[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_native_far_reads; });
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
			m_check->adjust(attotime::from_seconds(8));
			return;
		}
		if (m_phase == 30)
		{
			if (!m_native_entry_reads || !m_native_far_reads)
				fatalerror("MU4 original program transfer missing entry=%u far=%u pc=%06x", m_native_entry_reads, m_native_far_reads, unsigned(m_cpu->state_int(STATE_GENPC)));
			if (m_serial_tx_words != std::vector<u16>({0x0c10, 0x0818, 0x0a01, 0x0e53, 0x1023, 0x1201}))
				fatalerror("MU4 original serial-setup sequence mismatch");
			if (system_bios() == 2 && (m_stream_words < 128 || !m_dma_completions))
				fatalerror("MU4 original streaming block incomplete words=%u completions=%u pc=%06x illegal=%u",
					m_stream_words, m_dma_completions, unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
			if (system_bios() == 2) logerror("mu4_native_stream: PASS words=%u dma_completions=%u\n", m_stream_words, m_dma_completions);
			logerror("mu4_native_entry: PASS original_transfer=1 entry_reads=%u far_reads=%u pc=%06x illegal=%u idle=%u pmst=%04x\n",
				m_native_entry_reads, m_native_far_reads, unsigned(m_cpu->state_int(STATE_GENPC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
			logerror("mu4_native_mcbsp_boundary: index=%04x status=%04x polls=%u adjacent_reads=%u config_writes=%u tx_words=%u tx_irqs=%u controller_modeled=partial\n",
				m_native_mcbsp_index, m_native_mcbsp_status, m_native_mcbsp_polls, m_native_adjacent_reads, m_native_mcbsp_trace,
				unsigned(m_serial_tx_words.size()), m_serial_tx_irqs);
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
