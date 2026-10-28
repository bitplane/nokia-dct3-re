// license:BSD-3-Clause
// copyright-holders:Gaz

/* Development-only execution tests for the clean-room TMS320C54x core. */

#include "emu.h"
#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "nokia_dspif.h"
#include "nokia_cobba.h"
#include "nokia_dsp_c54x.h"
#include "gsm_tch_f_l1.h"
#include "util/gsmfr.h"
#include "tms320c54x_mcbsp.h"
#include "tms320c54x_dma.h"
#include "tms320c54x_bsp_state.h"
#include "tms320c54x_test_state.h"
#include <sstream>
#include <bitset>

namespace {

using c54x_test::tms320c54x_test_state;

class c54x_backend_bsp_test_state : public driver_device
{
public:
	c54x_backend_bsp_test_state(const machine_config &config, device_type type,
			const char *tag) : driver_device(config, type, tag),
		m_backend(*this, "backend"), m_cpu(*this, "backend:cpu"), m_cobba(*this, "test_codec") { }
	void test(machine_config &config)
	{
		NOKIA_DSPIF(config, "test_transport", 0);
		NOKIA_COBBA(config, m_cobba, 0);
		m_cobba->rf_receive_cb().set(FUNC(c54x_backend_bsp_test_state::rf_sample));
		NOKIA_CCONT(config, "test_power", 0);
		NOKIA_DSP_C54X(config, m_backend, 13'000'000);
		m_backend->set_transport_tag("^test_transport");
		m_backend->set_cobba_tag("^test_codec");
		m_backend->set_ccont_tag("^test_power");
		m_backend->set_program_rom_tag("^test_program");
		m_backend->set_data_rom_tag("^test_data");
		m_backend->set_external_codec_clock(true);
	}
protected:
	virtual void machine_start() override
	{
		m_timer = timer_alloc(FUNC(c54x_backend_bsp_test_state::check), this);
		save_item(NAME(m_rf_source_reads));
	}
	virtual void machine_reset() override
	{
		m_stage = !strcmp(machine().options().bios(), "requests") ? 90 : 0;
		m_case = 0;
		m_rf_source_reads = 0;
		m_timer->adjust(attotime::from_usec(100));
	}
private:
	u16 rf_sample() { return 0x1200 + (++m_rf_source_reads) * 0x111; }
	auto transmit_pin_edge(bool sync)
	{
		const auto opposite = m_backend->codec_transmit_clock_w(false, sync);
		expect(opposite.supported && !opposite.transmit_ready_edge, "falling CLKX does not shift default-polarity TX");
		const auto result = m_backend->codec_transmit_clock_w(true, sync);
		expect(result.supported, "external burst transmit pins supported");
		return result;
	}
	void transmit_pin_word(u16 word, bool ready)
	{
		for (unsigned i = 0; i < 16; ++i)
		{
			const auto pin = transmit_pin_edge(i == 0);
			expect(pin.driven && pin.data == bool((word >> (15 - i)) & 1) &&
				pin.transmit_ready_edge == (!i && ready), "native TX pin word and load-time XRDY edge");
		}
		const auto pin = transmit_pin_edge(false);
		expect(!pin.driven && !pin.transmit_ready_edge, "TX releases DX after the final bit");
	}
	void transmit_pin_suffix()
	{
		unsigned word = 0;
		for (unsigned i = 0; i < 8; ++i)
		{
			// Checkpoint ends with CLKX low; no intervening low write on replay.
			const auto pin = m_backend->codec_transmit_clock_w(true, false);
			expect(pin.supported && pin.driven && !pin.transmit_ready_edge, "restored TX suffix shifts without another load interrupt");
			word = (word << 1) | unsigned(pin.data);
			if (i != 7) m_backend->codec_transmit_clock_w(false, false);
		}
		expect(word == 0x78, "queued BDXR cannot rewrite the current output shifter");
		const auto pin = transmit_pin_edge(false);
		expect(!pin.driven && !pin.transmit_ready_edge, "replayed output word releases DX");
	}
	void prepare_transmit_irq()
	{
		m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
		m_cpu->set_state_int(STATE_GENPC, 0x0801);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 1);
	}
	void check_transmit_vector()
	{
		expect(m_cpu->state_int(STATE_GENPC) == 0x00d5 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
			m_cpu->space(AS_DATA).read_word(0x0fff) == 0x0801 &&
			!m_cpu->state_int(tms320c54x_device::STATE_IFR), "TX source dispatches native vector 21 once");
	}
	void check_clock_inputs()
	{
		auto &data = m_cpu->space(AS_DATA);
		data.write_word(0x22, 0);
		for (unsigned pins = 0; pins < 4; ++pins)
		{
			m_backend->codec_receive_clock_w(pins & 1, false, false);
			m_backend->codec_transmit_clock_w(pins & 2, false);
			expect((data.read_word(0x22) & 0x300) == (pins << 8), "BSPC IN0/IN1 reflect external clocks while engines reset");
			m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "save external serial clock levels");
			m_backend->codec_receive_clock_w(!(pins & 1), false, false);
			m_backend->codec_transmit_clock_w(!(pins & 2), false);
			expect((data.read_word(0x22) & 0x300) == ((pins ^ 3) << 8), "clock changes are visible without enabling serial engines");
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE &&
				(data.read_word(0x22) & 0x300) == (pins << 8), "restored external clocks are visible in BSPC");
			data.write_word(0x22, 0x300);
			expect((data.read_word(0x22) & 0x300) == (pins << 8), "BSPC cannot write input clock levels");
		}
		osd_printf_info("C54x native backend BSP clock inputs: PASS levels=4 save_replay=4 reset_inputs=1 handset_clock_claim=0\n");
	}
	void check_transmit_pins()
	{
		auto &data = m_cpu->space(AS_DATA);
		if (m_stage == 18)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_backend->reset_line_w(0);
		}
		else if (m_stage == 19) m_backend->reset_line_w(1);
		else if (m_stage == 20)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x00a0);
			data.write_word(0x0800, 0xf4e1); data.write_word(0x00d4, 0xf4e1);
			data.write_word(0x22, 0x48); data.write_word(0x23, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0x20);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
			prepare_transmit_irq();
			m_cpu->set_state_int(STATE_GENPC, 0x0800); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_cpu->resume(SUSPEND_REASON_DISABLE);
		}
		else if (m_stage == 21)
		{
			expect(m_cpu->state_int(STATE_GENPC) == 0x0801 && m_cpu->state_int(tms320c54x_device::STATE_IDLE), "TX fixture starts from IDLE");
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			data.write_word(0x21, 0x1234); data.write_word(0x21, 0x5678);
			expect(!(data.read_word(0x22) & 0x800), "BDXR overwrite does not manufacture a frame");
			expect(!(data.read_word(0x22) & 0x1000), "BDXR write alone cannot clear BSP underrun");
			unsigned word = 0;
			for (unsigned i = 0; i < 8; ++i)
			{
				const auto pin = transmit_pin_edge(i == 0);
				expect(pin.driven && pin.transmit_ready_edge == !i, "XRDY rises at output-shifter load, not word completion");
				word = (word << 1) | unsigned(pin.data);
				if (i == 3) data.write_word(0x21, 0x9abc);
			}
			expect(word == 0x56 && !(data.read_word(0x22) & 0x800), "active XSR is independent of queued BDXR");
			expect(data.read_word(0x22) & 0x1000, "fresh FSX clears underrun while output is active");
			m_backend->codec_transmit_clock_w(false, false);
		}
		else if (m_stage == 22)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20, "load-time BXINT0 arrives before the output word finishes");
			m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "save partial XSR, queued BDXR and pending TX interrupt");
			transmit_pin_suffix(); m_cpu->resume(SUSPEND_REASON_DISABLE);
			expect(data.read_word(0x22) & 0x1000, "queued BDXR prevents output-completion underrun");
		}
		else if (m_stage == 23)
		{
			check_transmit_vector();
			prepare_transmit_irq(); transmit_pin_word(0x9abc, true);
			expect(!(data.read_word(0x22) & 0x1000), "advance the live output to underrun before restoring non-underrun state");
			m_stage = 32; m_timer->adjust(attotime::from_usec(100)); return;
		}
		else if (m_stage == 32)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20, "allow the displaced transfer's asynchronous source to settle before restore");
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && !(data.read_word(0x22) & 0x800),
				"restore partial TX and its independently pending interrupt");
			expect(data.read_word(0x22) & 0x1000, "partial TX restores XSREMPTY independently of ready state");
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true); transmit_pin_suffix(); m_cpu->resume(SUSPEND_REASON_DISABLE);
			m_stage = 24; m_timer->adjust(attotime::from_usec(100)); return;
		}
		else if (m_stage == 24)
		{
			check_transmit_vector(); prepare_transmit_irq(); transmit_pin_word(0x9abc, true);
		}
		else if (m_stage == 25 || m_stage == 28)
		{
			expect(!(data.read_word(0x22) & 0x1000), "unqueued completion or reset asserts underrun");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20 && (data.read_word(0x22) & 0x800), "queued-word or XRST XRDY transition reaches BXINT0");
			m_cpu->resume(SUSPEND_REASON_DISABLE);
		}
		else if (m_stage == 26)
		{
			check_transmit_vector(); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); transmit_pin_word(0x9abc, false);
		}
		else if (m_stage == 27)
		{
			expect(!(data.read_word(0x22) & 0x1000), "retained-word replay does not clear prior underrun");
			expect(!m_cpu->state_int(tms320c54x_device::STATE_IFR), "retained-word underrun cannot retrigger BXINT0");
			prepare_transmit_irq();
			for (unsigned i = 0; i < 4; ++i)
			{
				const auto pin = transmit_pin_edge(i == 0);
				expect(pin.driven && !pin.transmit_ready_edge, "underrun starts the reset-abort word without an interrupt");
			}
			data.write_word(0x21, 0x5678); data.write_word(0x22, 8);
			expect(data.read_word(0x22) & 0x800, "XRST raises XRDY for an unwritten pending transfer");
			data.write_word(0x22, 0x48);
			for (unsigned i = 0; i < 16; ++i) expect(!transmit_pin_edge(false).driven, "XRST aborts output and cancels the queued word");
		}
		else if (m_stage == 29)
		{
			check_transmit_vector(); m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			for (unsigned i = 0; i < 17; ++i)
			{
				const auto pin = transmit_pin_edge(i == 0);
				expect(!pin.driven && !pin.transmit_ready_edge, "no reset-time preload is invented");
			}
		}
		else if (m_stage == 30)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_IFR), "empty frames do not create a late TX interrupt");
			check_clock_inputs();
			osd_printf_info("C54x native backend BSP transmit pins: PASS words=5 partial_restore=1 queued_word=1 underrun_no_irq=1 shift_status=1 reset_abort=1 reset_irq=1 vectors=4 handset_audio_claim=0\n");
			osd_printf_info("C54x native backend BSP: PASS cases=3 save_replay=3 reset_cases=3 handset_audio_claim=0\n");
			osd_printf_info("C54x native backend BSP observation: PASS receive_cases=2 debugger_reads=8 overrun_restore=2 normal_acks=4 handset_audio_claim=0\n");
			m_stage = 40; m_timer->adjust(attotime::from_usec(100)); return;
		}
		++m_stage; m_timer->adjust(attotime::from_usec(100));
	}
	void receive_pin_cycle(bool sync, bool bit)
	{
		expect(m_backend->codec_receive_clock_w(true, sync, bit) &&
			m_backend->codec_receive_clock_w(false, sync, bit), "external burst receive pins supported");
	}
	void continuous_receive_bits(u16 word, unsigned begin = 0, unsigned end = 16)
	{
		for (unsigned i = begin; i < end; ++i)
			receive_pin_cycle(false, (word >> (15 - i)) & 1);
	}
	void check_continuous_receive()
	{
		auto &data = m_cpu->space(AS_DATA);
		if (m_stage == 40)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			data.write_word(0x22, 0); data.write_word(0x23, 0); data.write_word(0x22, 0x80);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			receive_pin_cycle(true, false);
			continuous_receive_bits(0x1234);
		}
		else if (m_stage == 41)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x10 && data.read_word(0x20) == 0x1234,
				"continuous receiver publishes its initial word through source 4");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			continuous_receive_bits(0x5678, 0, 8);
			m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "save continuous receive mode and partial successor");
			continuous_receive_bits(0x5678, 8, 16);
		}
		else if (m_stage == 42)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x10 && data.read_word(0x20) == 0x5678,
				"continuous successor needs no second frame sync");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			data.write_word(0x22, 0); // Displace live continuous arming before load.
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE, "restore continuous receive after the live edge settled");
			m_stage = 42;
			continuous_receive_bits(0x5678, 8, 16);
		}
		else if (m_stage == 43)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x10 && data.read_word(0x20) == 0x5678,
				"restored continuous receiver completes the same successor");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			continuous_receive_bits(0x9abc); continuous_receive_bits(0xdef0);
		}
		else if (m_stage == 44)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x10 && (data.read_word(0x22) & 0x2400) == 0x2400 &&
				data.read_word(0x20) == 0x9abc, "continuous overrun preserves unread BDRR and adds no completion");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			continuous_receive_bits(0xffff);
			expect(!(data.read_word(0x22) & 0x2400), "BDRR read alone does not resume an overrun BSP stream");
			receive_pin_cycle(true, false); continuous_receive_bits(0x1357);
		}
		else if (m_stage == 45)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x10 && data.read_word(0x20) == 0x1357,
				"fresh FSR restarts the acknowledged continuous receiver");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			continuous_receive_bits(0xffff, 0, 8);
			data.write_word(0x22, 0); data.write_word(0x22, 0x80);
			continuous_receive_bits(0x1234);
		}
		else if (m_stage == 46)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_IFR) && !(data.read_word(0x22) & 0x2400),
				"RRST cancels partial and continuous state without manufacturing a new FSR");
			osd_printf_info("C54x native backend BSP continuous RX: PASS words=5 partial_restore=1 single_sync_continuation=1 overrun_restart=1 reset_abort=1 source=4 handset_clock_claim=0\n");
			m_stage = 50; m_timer->adjust(attotime::from_usec(100)); return;
		}
		++m_stage; m_timer->adjust(attotime::from_usec(100));
	}
	void continuous_transmit_suffix()
	{
		unsigned tail = 0;
		for (unsigned i = 0; i < 8; ++i)
		{
			const auto pin = transmit_pin_edge(false);
			expect(pin.driven && !pin.transmit_ready_edge, "continuous first-word suffix retains its own shifter");
			tail = (tail << 1) | unsigned(pin.data);
		}
		expect(tail == 0x78, "queued successor cannot overwrite the active continuous word");
		for (unsigned i = 0; i < 16; ++i)
		{
			const auto pin = transmit_pin_edge(false);
			expect(pin.driven && pin.data == bool((0x9abc >> (15 - i)) & 1) && pin.transmit_ready_edge == !i,
				"queued word starts on the next bit edge without a second FSX and raises load-time XRDY");
		}
		const auto halted = transmit_pin_edge(false);
		expect(!halted.driven && !halted.transmit_ready_edge && !(m_cpu->space(AS_DATA).read_word(0x22) & 0x1000),
			"continuous underrun releases DX and asserts XSREMPTY without a ready edge");
	}
	void check_continuous_transmit()
	{
		auto &data = m_cpu->space(AS_DATA);
		if (m_stage == 50)
		{
			data.write_word(0x22, 0); data.write_word(0x23, 0); data.write_word(0x22, 0x40);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			data.write_word(0x21, 0x5678);
			unsigned prefix = 0;
			for (unsigned i = 0; i < 8; ++i)
			{
				const auto pin = transmit_pin_edge(i == 0);
				expect(pin.driven && pin.transmit_ready_edge == !i, "continuous TX starts with one external frame sync");
				prefix = (prefix << 1) | unsigned(pin.data);
				if (i == 3) data.write_word(0x21, 0x9abc);
			}
			expect(prefix == 0x56, "continuous prefix preserves queued-word separation");
		}
		else if (m_stage == 51)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20, "initial continuous load reaches source 5");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "save continuous TX partial word and queued successor");
			continuous_transmit_suffix();
		}
		else if (m_stage == 52)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20, "queued continuous load independently reaches source 5");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			data.write_word(0x22, 0); // Displace continuous mode before restoration.
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE, "restore continuous TX after the live source settled");
			m_stage = 52; continuous_transmit_suffix();
		}
		else if (m_stage == 53)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20, "restored queued load repeats source 5");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			data.write_word(0x21, 0x1357);
			for (unsigned i = 0; i < 32; ++i)
			{
				const auto pin = transmit_pin_edge(false);
				expect(!pin.driven && !pin.transmit_ready_edge, "a late DXR write cannot manufacture an external restart sync");
			}
			transmit_pin_word(0x1357, true);
		}
		else if (m_stage == 54)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20, "fresh external restart load reaches source 5");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			transmit_pin_word(0x1357, false); // Retained word on FSX, no new DXR or XINT.
			data.write_word(0x21, 0x2468);
			for (unsigned i = 0; i < 4; ++i)
			{
				const auto pin = transmit_pin_edge(i == 0);
				expect(pin.driven && pin.transmit_ready_edge == !i, "continuous reset-abort word starts normally");
			}
			data.write_word(0x22, 0); data.write_word(0x22, 0x40);
			for (unsigned i = 0; i < 32; ++i) expect(!transmit_pin_edge(false).driven, "XRST cancels continuous output");
		}
		else if (m_stage == 55)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20, "only the reset-abort word's initial load remains pending");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
		}
		else if (m_stage == 56)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_IFR), "halted/reset continuous output produces no late source");
			osd_printf_info("C54x native backend BSP continuous TX: PASS completed_words=6 load_irqs=5 partial_restore=1 queued_boundary=1 underrun_halt=1 external_restart=1 reset_abort=1 source=5 handset_clock_claim=0\n");
			m_stage = 60; m_timer->adjust(attotime::from_usec(100)); return;
		}
		++m_stage; m_timer->adjust(attotime::from_usec(100));
	}
	void frame_ignore_suffix()
	{
		auto &data = m_cpu->space(AS_DATA);
		for (unsigned i = 8; i < 32; ++i)
		{
			const unsigned word = i < 16 ? 0x5678 : 0x9abc;
			const bool bit = (word >> (15 - i % 16)) & 1;
			const bool sync = i % 4 >= 2;
			receive_pin_cycle(sync, bit);
			expect(bool(data.read_word(0x22) & 0x400) == (i == 15 || i == 31), "FIG ignores later FSR pulses without dropping samples");
			if (i == 15 || i == 31) expect(data.read_word(0x20) == word, "FIG preserves complete receive words");
			const auto pin = transmit_pin_edge(sync);
			expect(pin.driven && pin.data == bit && pin.transmit_ready_edge == (i == 16), "FIG ignores later FSX pulses across queued word boundaries");
		}
		expect(!transmit_pin_edge(false).driven, "FIG does not suppress continuous underrun");
	}
	void check_frame_ignore()
	{
		auto &data = m_cpu->space(AS_DATA);
		if (m_stage == 60)
		{
			data.write_word(0x22, 0); data.write_word(0x23, 0x100); data.write_word(0x22, 0xc0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			data.write_word(0x21, 0x5678); receive_pin_cycle(true, false);
			for (unsigned i = 0; i < 8; ++i)
			{
				receive_pin_cycle(false, (0x5678 >> (15 - i)) & 1);
				const auto pin = transmit_pin_edge(i == 0);
				expect(pin.driven && pin.data == bool((0x5678 >> (15 - i)) & 1) && pin.transmit_ready_edge == !i, "FIG still requires the initial frame sync");
				if (i == 3) data.write_word(0x21, 0x9abc);
			}
		}
		else if (m_stage == 61)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20, "FIG initial TX load raises source 5");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "save FIG streams after initial sync release");
			frame_ignore_suffix();
		}
		else if (m_stage == 62)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x30, "FIG receive completion and queued transmit load raise sources 4/5");
			data.write_word(0x22, 0);
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE, "restore FIG streams after live reset displaced their sync state");
			m_stage = 62; frame_ignore_suffix();
		}
		else if (m_stage == 63)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x30, "restored FIG streams reproduce both interrupt sources");
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			data.write_word(0x22, 0); data.write_word(0x22, 0xc0);
			for (unsigned i = 0; i < 32; ++i)
			{
				receive_pin_cycle(false, true);
				expect(!transmit_pin_edge(false).driven && !(data.read_word(0x22) & 0x400), "reset FIG streams need fresh external sync");
			}
		}
		else if (m_stage == 64)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_IFR), "reset FIG streams produce no late completion");
			osd_printf_info("C54x native backend BSP frame ignore: PASS directions=2 words=4 partial_restore=1 ignored_sync=1 reset_abort=1 sources=4,5 handset_clock_claim=0\n");
			m_case = 0; m_stage = 70; m_timer->adjust(attotime::from_usec(100)); return;
		}
		++m_stage; m_timer->adjust(attotime::from_usec(100));
	}
	auto joined_edge(bool clock, bool sync)
	{
		const auto pin = m_backend->codec_transmit_clock_w(clock, sync);
		expect(pin.supported && m_backend->codec_receive_clock_w(clock, sync, pin.driven && pin.data),
			"Joined external pins use supported TX and RX modes");
		return pin;
	}
	void joined_bits(unsigned begin, unsigned end)
	{
		unsigned const width = std::array<unsigned, 4>{16, 8, 10, 12}[(m_case / 16) % 4];
		u16 const word = std::array<u16, 4>{0, 0x8001, 0xa597, 0x7ffe}[m_case % 4];
		bool const shift = !((m_case / 4) & 1), active = !((m_case / 4) & 2);
		for (unsigned bit = begin; bit < end; ++bit)
		{
			if (bit) joined_edge(shift, !active);
			const auto pin = joined_edge(!shift, !active);
			expect(pin.driven && pin.data == bool((word >> (width - 1 - bit)) & 1) && !pin.transmit_ready_edge,
				"Joined RX samples the actual held TX bit, MSB first");
			expect(bool(m_cpu->space(AS_DATA).read_word(0x22) & 0x400) == (bit + 1 == width),
				"Joined receiver becomes ready only after its last bit");
		}
	}
	void check_joined_pins()
	{
		auto &data = m_cpu->space(AS_DATA);
		unsigned const format = (m_case / 16) % 4, polarity = (m_case / 4) % 4;
		unsigned const width = std::array<unsigned, 4>{16, 8, 10, 12}[format], mask = (1U << width) - 1;
		u16 const word = std::array<u16, 4>{0, 0x8001, 0xa597, 0x7ffe}[m_case % 4], payload = word & mask;
		u16 const expected = payload & (1U << (width - 1)) ? payload | ~mask : payload;
		bool const shift = !(polarity & 1), active = !(polarity & 2);
		if (m_stage == 70)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			data.write_word(0x22, 0);
			data.write_word(0x23, (format & 2 ? 0x80 : 0) | (polarity & 1 ? 0x40 : 0) | (polarity & 2 ? 0x20 : 0));
			joined_edge(!shift, !active);
			data.write_word(0x22, 0xc8 | (format & 1 ? 4 : 0));
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			data.write_word(0x21, word);
			if (m_case >= 64)
			{
				// Short pulse: RX qualifies sync before TX loads the MSB.
				joined_edge(shift, !active);
				joined_edge(!shift, active);
			}
			expect(joined_edge(shift, active).transmit_ready_edge, "Joined frame transfers the pending DXR once");
			if (m_case < 64)
			{
				joined_edge(!shift, active);
				joined_edge(shift, active);
			}
			// Long external sync releases strictly between the TX and RX
			// sampling edges. No assumption about COBBA's fitted phase.
			joined_edge(shift, !active);
			joined_bits(0, width / 2);
		}
		else if (m_stage == 71)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20, "Only TX load is pending halfway through a joined word");
			m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "Save both joined shifters and clock levels mid-word");
			joined_bits(width / 2, width);
		}
		else if (m_stage == 72)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x30 && data.read_word(0x20) == expected,
				"Joined word reaches BDRR and independent RX/TX CPU sources");
			data.write_word(0x22, 0); joined_edge(shift, active);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x20 && !(data.read_word(0x22) & 0x400),
				"Restore partial joined word before receive completion, displacing live reset state");
			joined_bits(width / 2, width);
		}
		else
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x30 && data.read_word(0x20) == expected,
				"Restored joined word reproduces payload and both pending sources");
			expect(!joined_edge(shift, !active).driven, "Joined burst releases TX after the last sample");
			if (++m_case == 128)
			{
				osd_printf_info("C54x native backend BSP joined pins: PASS cases=128 formats=4 polarities=4 patterns=4 phases=2 partial_restore=128 sources=4,5 handset_clock_claim=0\n");
				m_stage = 80; m_timer->adjust(attotime::from_usec(100)); return;
			}
			m_stage = 70; m_timer->adjust(attotime::from_usec(100)); return;
		}
		++m_stage; m_timer->adjust(attotime::from_usec(100));
	}
	void check_clocked_store()
	{
		auto &data = m_cpu->space(AS_DATA);
		if (m_stage == 80)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			data.write_word(0x22, 0); data.write_word(0x23, 0);
			m_clocked_previous_word = data.read_word(0x20);
			joined_edge(false, false);
			m_cobba->control_data_w(0x0610); m_cobba->control_select_w(8);
			expect(m_cobba->codec_serial_loopback(), "Clocked store fixture enables the documented codec test mode");
			data.write_word(0x0800, 0x7721); // STM #0aaa, BDXR
			data.write_word(0x0801, 0x0aaa);
			data.write_word(0x0802, 0x4820); // LDM BDRR,A: same adjacent pair as the boot retry.
			data.write_word(0x0803, 0xf4e1);
			data.write_word(0x22, 0xc8);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
			m_cpu->set_state_int(STATE_GENPC, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_cpu->resume(SUSPEND_REASON_DISABLE);
		}
		else if (m_stage == 81)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(STATE_GENPC) == 0x0804 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				data.read_word(0x21) == 0x0aaa && !(data.read_word(0x22) & 0xc00) &&
				!m_cpu->state_int(tms320c54x_device::STATE_IFR) && !m_cobba->codec_serial_loopbacks(),
				"Native STM loads BDXR but cannot create ready edges or an untimed codec echo");
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			expect(joined_edge(true, true).transmit_ready_edge, "An external FSX transfers the native STM's pending word");
			joined_edge(false, true); joined_edge(true, true); joined_edge(true, false);
			for (unsigned bit = 0; bit < 16; ++bit)
			{
				if (bit) joined_edge(true, false);
				const auto pin = joined_edge(false, false);
				expect(pin.driven && pin.data == bool((0x0aaa >> (15 - bit)) & 1), "Clocked STM emits its original immediate literal");
			}
		}
		else
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x30 && data.read_word(0x20) == 0x0aaa &&
				!m_cobba->codec_serial_loopbacks() && u16(m_cpu->state_int(tms320c54x_device::STATE_A)) == m_clocked_previous_word &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
				"Clocked STM/LDM retains the previous RX word, preserves B/status and completes only through independent TX/RX sources");
			logerror("[opassert] op=7721\n");
			if (m_case == 128)
			{
				// Repeat the same pending-word/pin contract with staged execution;
				// this does not assert the silicon's peripheral-write phase.
				m_cpu->set_pipeline_frontend(true);
				++m_case; m_stage = 80;
				m_timer->adjust(attotime::from_usec(100)); return;
			}
			osd_printf_info("C54x native backend BSP clocked store: PASS opcode=7721 pending_without_clocks=1 echo=0aaa untimed=0 sources=4,5 handset_clock_claim=0\n");
			osd_printf_info("C54x staged backend BSP clocked store: PASS opcode=7721 pending_without_clocks=1 echo=0aaa untimed=0 sources=4,5 physical_phase_claim=0\n");
			throw emu_fatalerror(0, "C54x backend BSP tests complete");
		}
		++m_stage; m_timer->adjust(attotime::from_usec(100));
	}
	void receive_pin_bits(unsigned begin, unsigned end, bool ready)
	{
		for (unsigned i = begin; i < end; ++i)
		{
			receive_pin_cycle(false, (0x5678 >> (15 - i)) & 1);
			expect(bool(m_cpu->space(AS_DATA).read_word(0x22) & 0x400) == (ready && i == 15),
				"pin receiver publishes only at the final bit");
		}
	}
	void receive_pin_suffix()
	{
		// The checkpoint holds CLK high before bit 9's falling sample edge.
		expect(m_backend->codec_receive_clock_w(false, false, bool(0x5678 & 0x80)) &&
			!(m_cpu->space(AS_DATA).read_word(0x22) & 0x400), "restored high clock admits the pending falling edge");
		receive_pin_bits(9, 16, true);
	}
	void check_receive_pins()
	{
		auto &data = m_cpu->space(AS_DATA);
		if (m_stage == 8)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			m_backend->reset_line_w(1);
		}
		else if (m_stage == 9)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x00a0);
			data.write_word(0x0800, 0xf4e1); data.write_word(0x00d0, 0xf4e1);
			data.write_word(0x0022, 0x0088); data.write_word(0x0023, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0x0010);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x0010);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
			m_cpu->set_state_int(STATE_GENPC, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_cpu->resume(SUSPEND_REASON_DISABLE);
		}
		else if (m_stage == 10)
		{
			expect(m_cpu->state_int(STATE_GENPC) == 0x0801 && m_cpu->state_int(tms320c54x_device::STATE_IDLE), "pin fixture starts from IDLE");
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			receive_pin_cycle(true, false);
			receive_pin_bits(0, 8, false);
			expect(m_backend->codec_receive_clock_w(true, false, bool(0x5678 & 0x80)), "hold receive clock before the next sample");
			m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "save backend with half a serial word received");
			receive_pin_suffix();
		}
		else if (m_stage == 11 || m_stage == 13 || m_stage == 16)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == 0x10 && (data.read_word(0x22) & 0x400), "last receive bit reaches native CPU BRINT0");
			if (m_stage == 11) expect(data.read_word(0x20) == 0x5678, "initial pin word");
			m_cpu->resume(SUSPEND_REASON_DISABLE);
		}
		else if (m_stage == 12 || m_stage == 14 || m_stage == 17)
		{
			expect(m_cpu->state_int(STATE_GENPC) == 0x00d1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff && data.read_word(0x0fff) == 0x0801 &&
				!m_cpu->state_int(tms320c54x_device::STATE_IFR), "serial pin completion dispatches vector 20 exactly once");
			if (m_stage == 12)
			{
				m_saved.clear(); m_saved.seekg(0);
				expect(machine().save().read_stream(m_saved) == STATERR_NONE, "restore partial serial shift and CPU");
				expect(m_cpu->state_int(STATE_GENPC) == 0x0801 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
					!(data.read_word(0x22) & 0x400), "partial restore precedes readiness and interrupt entry");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				receive_pin_suffix();
			}
			else
			{
				expect(data.read_word(0x20) == 0x5678, "replayed/fresh pin word preserves all bits");
				if (m_stage == 17)
				{
					osd_printf_info("C54x native backend BSP receive pins: PASS words=3 partial_restore=1 reset_abort=1 vectors=3 handset_audio_claim=0\n");
					m_stage = 18; m_timer->adjust(attotime::from_usec(100)); return;
				}
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_cpu->set_state_int(STATE_GENPC, 0x0801);
				m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 1);
				m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
				receive_pin_cycle(true, false); receive_pin_bits(0, 8, false);
				data.write_word(0x22, 8); data.write_word(0x22, 0x88);
				receive_pin_bits(8, 16, false);
			}
		}
		else if (m_stage == 15)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_IFR) && !(data.read_word(0x22) & 0x400), "RRST abort leaves no receive edge");
			receive_pin_cycle(true, false); receive_pin_bits(0, 16, true);
		}
		++m_stage;
		m_timer->adjust(attotime::from_usec(100));
	}
	void check_rf_observation()
	{
		auto &io = m_cpu->space(AS_IO);
		auto peek = [this, &io](u16 expected) {
			u64 const reads = m_cobba->rf_receive_samples();
			u32 const source = m_rf_source_reads;
			{
				auto const disable = machine().disable_side_effects();
				expect(io.read_word(0x27) == expected && io.read_word(0x27) == expected, "RF debugger reads retain the last acquired word");
			}
			expect(m_cobba->rf_receive_samples() == reads && m_rf_source_reads == source, "RF debugger reads never advance the external source");
		};
		peek(0);
		expect(io.read_word(0x27) == 0x1311 && m_rf_source_reads == 1 && m_cobba->rf_receive_samples() == 1,
			"ordinary RF read consumes exactly one source word");
		peek(0x1311);
		m_saved.str({}); m_saved.clear();
		expect(machine().save().write_stream(m_saved) == STATERR_NONE, "save RF observation latch and independent source cursor");
		expect(io.read_word(0x27) == 0x1422 && m_rf_source_reads == 2, "RF next source word");
		m_saved.clear(); m_saved.seekg(0);
		expect(machine().save().read_stream(m_saved) == STATERR_NONE && m_rf_source_reads == 1 && m_cobba->rf_receive_samples() == 1,
			"restore RF sample count and external fixture cursor");
		peek(0x1311);
		expect(io.read_word(0x27) == 0x1422 && m_rf_source_reads == 2 && m_cobba->rf_receive_samples() == 2,
			"RF continuation after load consumes the same next source word");
		m_cobba->reset();
		peek(0);
		expect(!m_cobba->rf_receive_samples() && m_rf_source_reads == 2, "RF reset clears observation state without clocking the source");
		osd_printf_info("C54x native backend RF observation: PASS debugger_reads=8 ordinary_reads=3 source_restore=1 reset=1 physical_rf_claim=0\n");
	}
	void expect(bool condition, const char *message)
	{
		if (!condition) throw emu_fatalerror("C54x backend BSP: %s", message);
	}
	void check_slot_request()
	{
		auto &data = m_cpu->space(AS_DATA);
		auto &io = m_cpu->space(AS_IO);
		// Include a packed parameter whose bit 3 overlaps the slot request.
		u16 const payload = std::array<u16, 4>{0, 0x8000, 0x9201, 0x9209}[(m_case % 8) / 2];
		u16 const request = m_case < 8 ? 8 : 0x1000;
		unsigned const compare_port = m_case < 8 ? 0x0f : 4;
		unsigned const accept_port = 1 + m_case % 2;
		if (m_stage == 90)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			m_backend->reset_line_w(0);
			io.write_word(4, 15000); io.write_word(0x0f, 15000);
			m_cpu->space(AS_PROGRAM).write_word(0xff80, 0xf4e1); // Bench reset entry: IDLE.
			data.write_word(0x0852, 0); data.write_word(0x0853, 0);
			data.write_word(0x0854, payload); data.write_word(0x0855, 0);
			m_backend->reset_line_w(1);
			io.write_word(2, request); // Mask this source before its future expiry.
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			expect(!io.read_word(1), "Shared parameter data alone cannot assert a peripheral request");
			io.write_word(compare_port, (io.read_word(0x0d) + 100) % 5000);
		}
		else if (m_stage == 91)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			expect(data.read_word(0x0854) == payload, "Slot expiry cannot rewrite the shared parameter payload");
			expect(io.read_word(1) == request, "Compare expiry exposes only its pending request, not parameter bits");
			expect(!(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4), "In-service masking retains the pending slot without asserting INT2");
			io.write_word(1, 1);
			expect(io.read_word(1) == request && data.read_word(0x0854) == payload, "An unrelated completion cannot retire the compare or change parameter bits");
			io.write_word(2, 0);
		}
		else if (m_stage == 92)
		{
			// CPU input changes settle between callbacks, before the snapshot.
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4, "Unmasking the retained slot request asserts native source 2");
			m_timer->adjust(attotime::never);
			m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "Save the pending slot and its independent payload");
			io.write_word(accept_port, request);
			expect(!io.read_word(1) && data.read_word(0x0854) == payload, "Either accept port retires the slot without changing these payloads");
		}
		else if (m_stage == 93)
		{
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE && io.read_word(1) == request && data.read_word(0x0854) == payload,
				"Restoring pending hardware state does not reconstruct it from shared data");
		}
		else if (m_stage == 94)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4, "The pending snapshot restores the native source-2 latch");
			io.write_word(accept_port, request);
			m_backend->reset_line_w(0);
			expect(!io.read_word(1), "DSP reset does not retain a stale slot request");
		}
		else if (m_stage == 95) m_backend->reset_line_w(1);
		else if (m_stage == 96)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			io.write_word(2, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			io.write_word(compare_port, (io.read_word(0x0d) + 100) % 5000);
			m_timer->adjust(attotime::never);
			m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "Save the armed timer before expiry");
			m_backend->reset_line_w(0);
		}
		else if (m_stage == 97)
		{
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE && !io.read_word(1),
				"Restore an armed, not-yet-pending timer after live reset cancellation");
		}
		else
		{
			expect(io.read_word(1) == request && data.read_word(0x0854) == payload,
				"The restored compare expires through its saved timer without changing the payload");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4, "Restored armed timer delivers the same native interrupt source");
			m_backend->reset_line_w(0);
			expect(!io.read_word(1), "Reset clears a pending slot but preserves DARAM");
			if (++m_case == 16)
			{
				m_stage = 100; m_timer->adjust(attotime::from_usec(100)); return;
			}
			m_stage = 90; m_timer->adjust(attotime::from_usec(100)); return;
		}
		++m_stage; m_timer->adjust(attotime::from_usec(100));
	}
	void check_compare_overlap()
	{
		auto &data = m_cpu->space(AS_DATA);
		auto &io = m_cpu->space(AS_IO);
		if (m_stage == 100)
		{
			m_backend->reset_line_w(0);
			io.write_word(4, 15000); io.write_word(0x0f, 15000);
			data.write_word(0x0854, 0x9209); data.write_word(0x0855, 0);
			m_backend->reset_line_w(1); io.write_word(2, 0x1008);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			u16 const target = (io.read_word(0x0d) + 100) % 5000;
			io.write_word(4, target); io.write_word(0x0f, target);
		}
		else if (m_stage == 101)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			expect(io.read_word(1) == 0x1008 && !(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4),
				"Masked coincident compares retain both independent requests");
			m_timer->adjust(attotime::never); m_saved.str({}); m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "Save both pending comparators and their mask");
			io.write_word(2, 8);
			expect(io.read_word(1) == 0x1000, "Accepting the slot leaves the transfer pending");
		}
		else if (m_stage == 102)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4, "The unmasked transfer asserts the shared CPU source");
			m_saved.clear(); m_saved.seekg(0);
			expect(machine().save().read_stream(m_saved) == STATERR_NONE && io.read_word(1) == 0x1008,
				"Restore recovers both pending sources after one was accepted");
		}
		else if (m_stage == 103)
		{
			expect(!(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4), "Restored in-service mask suppresses the CPU line");
			io.write_word(1, 0x1000);
			expect(io.read_word(1) == 8, "Transfer completion leaves the slot pending");
			io.write_word(2, 0);
		}
		else
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4, "The remaining unmasked slot asserts the same CPU source");
			io.write_word(1, 8);
			expect(!io.read_word(1) && data.read_word(0x0854) == 0x9209, "Both acknowledgements preserve packed parameter data");
			osd_printf_info("C54x native backend compare requests: PASS cases=16 channels=2 pending_restore=16 armed_restore=16 overlap_restore=1 payload_preserved=1 masked_pending=1 ack_ports=1,2 reset=16 mapping_claim=0\n");
			throw emu_fatalerror(0, "C54x backend compare request tests complete");
		}
		++m_stage; m_timer->adjust(attotime::from_usec(100));
	}
	TIMER_CALLBACK_MEMBER(check)
	{
		if (m_stage >= 100) { check_compare_overlap(); return; }
		if (m_stage >= 90) { check_slot_request(); return; }
		if (m_stage >= 80) { check_clocked_store(); return; }
		if (m_stage >= 70) { check_joined_pins(); return; }
		if (m_stage >= 60) { check_frame_ignore(); return; }
		if (m_stage >= 50) { check_continuous_transmit(); return; }
		if (m_stage >= 40) { check_continuous_receive(); return; }
		if (m_stage >= 18) { check_transmit_pins(); return; }
		if (m_stage >= 8) { check_receive_pins(); return; }
		if (m_stage == 0)
		{
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			m_backend->reset_line_w(1);
		}
		else if (m_stage == 1)
		{
			// Synthetic overlay code, not a recovered handset ISR or clock.
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x00a0);
			auto &data = m_cpu->space(AS_DATA);
			data.write_word(0x0800, 0xf4e1);
			data.write_word(0x00d0, 0xf4e1);
			data.write_word(0x00d4, 0xf4e1);
			data.write_word(0x0022, m_case == 0 ? 0x0080 : m_case == 1 ? 0x0040 : 0x00c0);
			if (m_case != 0) data.write_word(0x0021, 0x1234);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0x0030);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x0010);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
			m_cpu->set_state_int(STATE_GENPC, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_cpu->resume(SUSPEND_REASON_DISABLE);
		}
		else if (m_stage == 2)
		{
			expect(m_cpu->state_int(STATE_GENPC) == 0x0801 &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE), "initial IDLE");
			m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
			u16 output = 0;
			expect(m_backend->codec_duplex_frame(0x5678, output) == (m_case != 0),
				"backend transmit completion");
			if (m_case != 0) expect(output == 0x1234, "backend transmitted payload");
		}
		else if (m_stage == 3)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) ==
				(m_case == 0 ? 0x10 : m_case == 1 ? 0x20 : 0x30), "backend ready edges reach IFR");
			if (m_case != 1)
			{
				auto &data = m_cpu->space(AS_DATA);
				for (unsigned overrun = 0; overrun < 2; ++overrun)
				{
					if (overrun) expect(!m_backend->codec_receive_frame(0x9abc), "unread BDRR rejects the overflowing sample");
					u16 const control = data.read_word(0x0022);
					expect((control & 0x2400) == (overrun ? 0x2400 : 0x0400), "receive-ready and overrun before observation");
					{
						auto const disable = machine().disable_side_effects();
						expect(data.read_word(0x0020) == 0x5678 && data.read_word(0x0020) == 0x5678,
							"debugger sees the retained receive word");
					}
					expect(data.read_word(0x0022) == control, "debugger BDRR reads preserve receive-ready and overrun");
				}
			}
			m_saved.str(std::string());
			m_saved.clear();
			expect(machine().save().write_stream(m_saved) == STATERR_NONE, "save backend and pending CPU");
			m_cpu->resume(SUSPEND_REASON_DISABLE);
		}
		else if (m_stage == 4 || m_stage == 5)
		{
			expect(m_cpu->state_int(STATE_GENPC) == (m_case == 1 ? 0x00d5 : 0x00d1) &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (m_case == 2 ? 0x20 : 0),
				"backend vector and simultaneous receive priority");
			if (m_case != 1)
			{
				auto &data = m_cpu->space(AS_DATA);
				expect((data.read_word(0x0022) & 0x2400) == 0x2400, "backend overrun and receive word survive dispatch and save/load");
				expect(data.read_word(0x0020) == 0x5678, "backend retained RX payload");
				expect(!(data.read_word(0x0022) & 0x2400), "ordinary BDRR read acknowledges receive-ready and overrun");
			}
			if (m_stage == 4)
			{
				m_saved.clear();
				m_saved.seekg(0);
				expect(machine().save().read_stream(m_saved) == STATERR_NONE, "restore backend and CPU");
				m_cpu->resume(SUSPEND_REASON_DISABLE);
			}
			else
			{
				m_backend->reset_line_w(0);
			}
		}
		else if (m_stage == 6)
		{
			m_reset_ifr = m_cpu->state_int(tms320c54x_device::STATE_IFR);
			u16 output = 0xaaaa;
			expect(m_cpu->space(AS_DATA).read_word(0x0022) == 0x0800,
				"reset clears backend BSP ready and loaded-word state");
			expect(!m_backend->codec_duplex_frame(0x5678, output) && output == 0xaaaa &&
				!m_backend->codec_transmit_frame(output) &&
				!m_backend->codec_receive_frame(0x5678), "reset BSP rejects completed frames");
			expect(m_cpu->space(AS_DATA).read_word(0x0022) == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == m_reset_ifr,
				"reset frame calls cannot publish readiness or source edges");
		}
		else
		{
			// Allow asynchronous CPU input delivery before rejecting extra edges.
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == m_reset_ifr,
				"reset frame calls leave no asynchronously delivered source edges");
			if (++m_case == 3)
			{
				check_rf_observation();
				m_stage = 8;
				m_timer->adjust(attotime::from_usec(100));
				return;
			}
			m_stage = 0;
			m_timer->adjust(attotime::from_usec(100));
			return;
		}
		++m_stage;
		m_timer->adjust(attotime::from_usec(100));
	}
	required_device<nokia_dsp_c54x_device> m_backend;
	required_device<tms320c54x_device> m_cpu;
	required_device<nokia_cobba_device> m_cobba;
	u32 m_rf_source_reads = 0;
	emu_timer *m_timer = nullptr;
	unsigned m_stage = 0, m_case = 0;
	u16 m_reset_ifr = 0;
	u16 m_clocked_previous_word = 0;
	std::stringstream m_saved;
};

// Execute the stock flash-staged verifier without supplying a DSP mask ROM.
// This is a protocol investigation fixture, not an NSM-3 compatibility backend.
class nsm3_verifier_state : public driver_device
{
public:
	nsm3_verifier_state(const machine_config &config, device_type type, const char *tag) :
		driver_device(config, type, tag), m_cpu(*this, "maincpu"), m_cobba(*this, "cobba") { }
	void verifier(machine_config &config)
	{
		TMS320C54X(config, m_cpu, 13'000'000);
		NOKIA_COBBA(config, m_cobba, 0);
		m_cpu->set_addrmap(AS_PROGRAM, &nsm3_verifier_state::program_map);
		m_cpu->set_addrmap(AS_DATA, &nsm3_verifier_state::data_map);
		m_cpu->set_addrmap(AS_IO, &nsm3_verifier_state::io_map);
	}
	void nse5_verifier(machine_config &config)
	{
		verifier(config);
		m_cpu->set_addrmap(AS_PROGRAM, &nsm3_verifier_state::nse5_program_map);
	}

private:
	bool npe3() const { return !strcmp(machine().system().name, "npe3verify"); }
	bool nhm3() const { return !strcmp(machine().system().name, "nhm3verify"); }
	bool nse5() const { return !strcmp(machine().system().name, "nse5verify"); }
	unsigned block_count() const { return nse5() ? 228 : (npe3() || nhm3()) ? 232 : 116; }
	void program_map(address_map &map)
	{
		map(0, 0xffff).ram();
		// ROM4's acquired PROM has the immutable version word here. This
		// fixture supplies an explicit version input, not missing ROM code.
		map(0xff87, 0xff87).rw(FUNC(nsm3_verifier_state::version_r), FUNC(nsm3_verifier_state::version_w));
	}
	void nse5_program_map(address_map &map)
	{
		map(0, 0x7fff).ram();
		map(0x8000, 0xffff).rom().region("mask", 0x10000);
	}
	u16 version_r() { return !strcmp(machine().options().bios(), "rom4") ? 4 : 6; }
	void version_w(u16 value)
	{
		logerror("nsm3_verifier: immutable_version_write=%04x\n", value);
	}
	void data_map(address_map &map) { map(0, 0xffff).ram(); }
	void io_map(address_map &map)
	{
		map(0, 0xffff).rw(FUNC(nsm3_verifier_state::io_r), FUNC(nsm3_verifier_state::io_w));
	}
	bool using_cobba() const
	{
		return !strcmp(machine().options().bios(), "cobba") ||
				!strcmp(machine().options().bios(), "cobba_alt") ||
				!strcmp(machine().options().bios(), "rom4");
	}
	u16 io_r(offs_t offset)
	{
		if (using_cobba() && offset == 0x2d)
		{
			u16 const value = m_cobba->control_data_r();
			logerror("nsm3_verifier: port_read=%04x data=%04x blocks=%u\n", u16(offset), value, m_block);
			return value;
		}
		throw emu_fatalerror(1, "NSM3 verifier requires peripheral read: port=%04x pc=%04x blocks=%u",
			u16(offset), u16(m_cpu->state_int(tms320c54x_device::STATE_PC)), m_block);
	}
	void io_w(offs_t offset, u16 data)
	{
		logerror("nsm3_verifier: port_write=%04x data=%04x blocks=%u\n", u16(offset), data, m_block);
		if (using_cobba())
		{
			if (offset == 0x2c)
				m_cobba->control_select_w(data);
			else if (offset == 0x2d)
				m_cobba->control_data_w(data);
		}
	}
	void machine_start() override
	{
		m_timer = timer_alloc(FUNC(nsm3_verifier_state::step), this);
	}
	void machine_reset() override
	{
		auto &program = m_cpu->space(AS_PROGRAM);
		auto &data = m_cpu->space(AS_DATA);
		for (unsigned i = 0; i != (nse5() ? 0x8000 : 0x10000); ++i)
			if (i != 0xff87)
				program.write_word(i, 0xffff);
		u16 const *const staged = &memregion("verifier")->as_u16();
		for (unsigned i = 0; i != (nse5() ? 210 : 223); ++i)
			program.write_word(0x0f00 + i, staged[i]);
		// Values written by the MCU at 0x2cac80..0x2cacee, not a mask-ROM snapshot.
		data.write_word(0x0800, 0);
		data.write_word(0x0801, 0xffff);
		data.write_word(0x0802, 0xffff);
		data.write_word(0x0803, 0xffff);
		data.write_word(0x087b, 0x0100);
		data.write_word(0x087c, 0x0300);
		data.write_word(0x087d, (npe3() || nhm3() || nse5()) ? 1 : 0);
		data.write_word(0x087e, nse5() ? 0xc800 : (npe3() || nhm3()) ? 0xd000 : 0xe800);
		data.write_word(0x087f, 1);
		data.write_word(0x0880, 1);
		data.write_word(0x0881, 0x0200);
		m_block = 0;
		m_ticks = 0;
		// Metamorphic peripheral input, not a proposed handset identity.
		if (!strcmp(machine().options().bios(), "cobba_alt"))
		{
			m_cobba->control_data_w(0x0016);
			m_cobba->control_select_w(0x000f);
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0f00);
		m_timer->adjust(attotime::from_usec(1), 0, attotime::from_usec(1));
	}
	TIMER_CALLBACK_MEMBER(step)
	{
		auto &data = m_cpu->space(AS_DATA);
		unsigned const pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
		if ((pc < 0x0f00 || pc >= (nse5() ? 0x0fd2 : 0x0fdf)) &&
				!(nse5() && pc >= 0x8000))
			throw emu_fatalerror(1, "NSM3 verifier escaped staged image: pc=%04x block=%u", pc, m_block);
		if (data.read_word(0x0801) != 0xffff)
			throw emu_fatalerror(0, "NSM3 verifier publication: blocks=%u word0=%04x word1=%04x word2=%04x word3=%04x pc=%04x fingerprint=%04x%04x pmst=%04x",
				m_block, data.read_word(0x0800), data.read_word(0x0801), data.read_word(0x0802), data.read_word(0x0803), pc,
				data.read_word(nse5() ? 0x1f0e : 0x04f7), data.read_word(nse5() ? 0x1f0f : 0x04f8), u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)));
		if (++m_ticks == 2000000)
			throw emu_fatalerror(1, "NSM3 verifier timeout: pc=%04x blocks=%u flags=%04x/%04x",
				pc, m_block, data.read_word(0x087f), data.read_word(0x0880));
		bool const second = BIT(m_block, 0);
		unsigned const flag = second ? 0x0880 : 0x087f;
		bool const polling = second ? (pc == 0x0f33 || pc == 0x0f35) : (pc == 0x0f17 || pc == 0x0f19);
		if (m_block == block_count() || !polling || data.read_word(flag) != 1)
			return;
		u8 const *const flash = memregion("flash")->base();
		unsigned const base = second ? 0x0b00 : 0x0900;
		for (unsigned i = 0; i != 512; ++i)
		{
			unsigned const offset = 0x40 + (m_block * 512 + i) * 0x20;
			u16 const value = (m_block == block_count() - 1 && i >= 510) ? 0xffff :
				(u16(flash[offset]) << 8) | flash[offset + 1];
			data.write_word(base + i, value);
		}
		data.write_word(flag, 0);
		logerror("nsm3_verifier: block=%u flag=%04x pc=%04x\n", m_block, flag, pc);
		++m_block;
	}
	required_device<tms320c54x_device> m_cpu;
	required_device<nokia_cobba_device> m_cobba;
	emu_timer *m_timer = nullptr;
	unsigned m_block = 0;
	unsigned m_ticks = 0;
};

ROM_START(nsm3verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "Compare existing COBBA register model (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture (not handset identity)")
	ROM_SYSTEM_BIOS(3, "rom4", "Immutable PROM version sensitivity fixture (not NSM-3 hardware)")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x1d0000, "flash", 0)
	ROM_LOAD("8210_5.31ppm_c.fls", 0, 0x1d0000,
		CRC(927022b1) SHA1(c1a0fe95cedb89a92b19654208cc4855e1a4988e))
ROM_END

// Same staged program and observed geometry, distinct stock 8250 flash input.
// Peripheral variants remain sensitivity fixtures, not fitted NSM-3D identity.
ROM_START(nsm3dverify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x1d0000, "flash", 0)
	ROM_LOAD("8250-502mcuppmk.fls", 0, 0x1d0000,
		CRC(2c58e48b) SHA1(f26c98ffcfffbbd5714889e10cfa41c5f6dd2529))
ROM_END

// Own 3310 v6.39 input and MCU-derived 116-block geometry; no donor mask ROM.
ROM_START(nhm5verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x200000, "flash", 0)
	ROM_LOAD("3310f639e.fls", 0, 0x200000,
		CRC(13430c77) SHA1(d5da65f417595200314eb0115bf46ca1fbf53128))
ROM_END

// NPE-3 additionally supplies a different block count and source extent.
ROM_START(npe3verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x3a0000, "flash", 0)
	ROM_LOAD("6210_556c.fls", 0, 0x3a0000,
		CRC(203fb962) SHA1(3d9ea319503e78ec69b60d72cda23e461e118ea9))
ROM_END

// 6250's own staged bytes and sparse flash stream; no handset verdict supplied.
ROM_START(nhm3verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_SYSTEM_BIOS(3, "rom4", "PROM version sensitivity fixture")
	ROM_REGION16_LE(446, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nsm3_verifier.bin", 0, 446,
		CRC(53e2de79) SHA1(6646da3c5be9c70deda7e0b5b9f257d5d2ace815))
	ROM_REGION(0x3a0000, "flash", 0)
	ROM_LOAD("6250-503mcuppmc.fls", 0, 0x3a0000,
		CRC(8dffb91b) SHA1(95607ce39c383bda75f1e6aeae67a214b787b0a1))
ROM_END

// Stock NSE-5 upload plus independently recovered ROM4 CRC routines. COBBA
// variants are explicit peripheral fixtures, not a measured 7110 publication.
ROM_START(nse5verify)
	ROM_SYSTEM_BIOS(0, "boundary", "Fail closed at unsupported peripheral")
	ROM_SYSTEM_BIOS(1, "cobba", "COBBA model comparison (not handset validation)")
	ROM_SYSTEM_BIOS(2, "cobba_alt", "COBBA register-F sensitivity fixture")
	ROM_REGION16_LE(420, "verifier", 0)
	ROM_LOAD16_WORD_SWAP("nse5_verifier.bin", 0, 420,
		CRC(e6c77fdc) SHA1(caca7599d9ca1a7dddf2df37f32be4aacd420deb))
	ROM_REGION(0x390000, "flash", 0)
	ROM_LOAD("7110f501_ppmc.fls", 0, 0x390000,
		CRC(919ac753) SHA1(53af8324919f455ba8199d2c05f7a921cfb811d5))
	ROM_REGION16_LE(0x20000, "mask", ROMREGION_ERASEFF)
	ROM_LOAD16_WORD_SWAP("dsp_full.bin", 0, 0x1fffe,
		CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739))
ROM_END

ROM_START(tms54test)
	ROM_SYSTEM_BIOS(0, "core", "Core conformance tests")
	ROM_SYSTEM_BIOS(1, "timer", "Timer interrupt boundary observation (not fidelity acceptance)")
	ROM_SYSTEM_BIOS(2, "swap-body", "Original MU4 byte-swap body (not block-repeat acceptance)")
	ROM_SYSTEM_BIOS(3, "swap-join", "Consecutive MU4 byte-swap bodies (not block-repeat acceptance)")
	ROM_SYSTEM_BIOS(4, "rptb", "Staged straight-line RPTB conformance")
	ROM_SYSTEM_BIOS(5, "rmw-forward", "Staged same-address RMW forwarding")
	ROM_SYSTEM_BIOS(6, "negate", "Staged accumulator NEG conformance")
	ROM_SYSTEM_BIOS(7, "mapped-word", "Staged accumulator/T/TRN memory arithmetic")
	ROM_SYSTEM_BIOS(8, "mapped-bitf", "Staged accumulator/T/TRN bitfield tests")
	ROM_SYSTEM_BIOS(9, "mpy-mapped", "Staged two-word MPY mapped accumulator consumers")
	ROM_SYSTEM_BIOS(10, "long-mvkd", "Staged long stores, moves and RMW conformance")
	ROM_SYSTEM_BIOS(11, "imr-store", "Staged immediate IMR write and pending interrupts")
	ROM_SYSTEM_BIOS(12, "bus-registers", "C5416-compatible external-bus register conformance")
	ROM_SYSTEM_BIOS(13, "bus-io", "Staged external I/O waits and READY conformance")
	ROM_SYSTEM_BIOS(14, "bus-io-atomic", "Atomic external I/O waits and READY conformance")
	ROM_SYSTEM_BIOS(15, "bus-io-compose", "Atomic waited I/O control-flow composition")
	ROM_SYSTEM_BIOS(16, "bus-stm", "Staged C5416 bus-control immediate writes")
	ROM_SYSTEM_BIOS(17, "read-a-mapped", "Staged READA mapped accumulator writes")
	ROM_SYSTEM_BIOS(18, "dp-absolute", "Staged absolute memory-to-DP load")
	ROM_SYSTEM_BIOS(19, "mvdm-ar", "Staged absolute data-to-AR MVDM")
	ROM_SYSTEM_BIOS(20, "mvdm-delay", "Staged MVDM conditional branch delay composition")
	ROM_SYSTEM_BIOS(21, "far-call-delay", "Staged delayed far-call page and return ownership")
	ROM_SYSTEM_BIOS(22, "mac-smem", "Staged T-based MAC/MAS arithmetic and retained operands")
	ROM_SYSTEM_BIOS(23, "mvkd-delay", "Staged two-word MVKD branch delay composition")
	ROM_SYSTEM_BIOS(24, "mvmd-ar", "Staged encoded AR-to-data MVMD ownership")
	ROM_SYSTEM_BIOS(25, "ifr-pair", "Staged IFR read-back acknowledgement")
	ROM_SYSTEM_BIOS(26, "cmpm-absolute", "Staged absolute memory comparison and TC branch")
	ROM_SYSTEM_BIOS(27, "mvkd-peripheral", "Staged retained peripheral MVKD transaction")
	ROM_SYSTEM_BIOS(28, "t-peripheral", "Staged T/TRN producer and peripheral store")
	ROM_SYSTEM_BIOS(29, "bitf-peripheral", "Staged peripheral status BITF and branch")
	ROM_SYSTEM_BIOS(30, "far-atomic", "Atomic far-transfer reference contracts")
	ROM_SYSTEM_BIOS(31, "fcala", "Staged accumulator far-call ownership")
	ROM_SYSTEM_BIOS(32, "fcala-producer", "Staged DLD producer and accumulator far call")
	ROM_SYSTEM_BIOS(33, "conditional-call", "Staged CC predicate and conditional stack ownership")
	ROM_SYSTEM_BIOS(34, "conditional-ret", "Staged RC predicate and conditional return ownership")
	ROM_SYSTEM_BIOS(35, "subs", "Staged unsigned memory subtraction")
	ROM_SYSTEM_BIOS(36, "mvmd-acc", "Staged mapped accumulator producer and move")
	ROM_SYSTEM_BIOS(37, "mvmd-st1", "Staged mapped accumulator to full status register")
	ROM_SYSTEM_BIOS(38, "mvmd-cpl", "Staged full status transfer CPL consumer latency")
	ROM_SYSTEM_BIOS(39, "mvmd-status-alu", "Staged full status SXM and ASM consumer latency")
	ROM_SYSTEM_BIOS(40, "mvmd-block", "Staged full status and adjacent block repeat")
	ROM_SYSTEM_BIOS(41, "mvmd-cmpt", "Staged full status compatibility-mode consumer latency")
	ROM_SYSTEM_BIOS(42, "mvmd-status-irq", "Staged full status mask and interrupt composition")
	ROM_SYSTEM_BIOS(43, "memory-shift16", "Staged memory ADD and SUB shift sixteen")
	ROM_SYSTEM_BIOS(44, "ldu-imr", "Staged unsigned IMR load and pointer producer")
	ROM_SYSTEM_BIOS(45, "preinc-store", "Staged short preincrement accumulator stores")
	ROM_SYSTEM_BIOS(46, "stm-brc", "Staged immediate block counter and adjacent repeat")
	ROM_SYSTEM_BIOS(47, "preinc-st", "Staged short preincrement immediate stores")
	ROM_SYSTEM_BIOS(48, "st-long-periph", "Staged long peripheral immediate store timing")
	ROM_SYSTEM_BIOS(49, "rmw-long-periph", "Staged long peripheral read-modify-write timing")
	ROM_SYSTEM_BIOS(50, "mcbsp-control", "Staged actual McBSP selector and reset sequence")
	ROM_SYSTEM_BIOS(51, "stlm-imr", "Staged accumulator interrupt-mask publication")
	ROM_SYSTEM_BIOS(52, "pmst-map", "Staged PMST publication and program mapping")
	ROM_SYSTEM_BIOS(53, "pmst-pop", "Staged PMST stack restoration and program mapping")
	ROM_SYSTEM_BIOS(54, "pmst-prefetch", "Staged PMST prefetch ownership conformance")
	ROM_SYSTEM_BIOS(55, "pmst-iptr", "Staged PMST pop and interrupt vector composition")
	ROM_SYSTEM_BIOS(56, "pop-repeat", "Staged repeat-control stack restoration")
	ROM_SYSTEM_BIOS(57, "pop-brc", "Staged block counter pop and repeat setup")
	ROM_SYSTEM_BIOS(58, "stack-irq", "Staged stack updates and interrupt entry ordering")
	ROM_SYSTEM_BIOS(59, "stack-irq-return", "Staged stack interrupt return round trip")
	ROM_SYSTEM_BIOS(60, "ldu-st1", "Staged unsigned mapped-status register read")
	ROM_SYSTEM_BIOS(61, "cmpm-direct", "Staged direct DP/SP comparison and branch")
	ROM_SYSTEM_BIOS(62, "timer-stm", "Staged timer immediate writes and retained peripheral wait")
	ROM_SYSTEM_BIOS(63, "pshd-mapped", "Staged mapped-register PSHD source ownership")
	ROM_SYSTEM_BIOS(64, "pshd-repeat", "Staged repeated PSHD with retained per-iteration operands")
	ROM_SYSTEM_BIOS(65, "exp-t", "Staged exponent publication and T consumer latency")
	ROM_SYSTEM_BIOS(66, "adds", "Staged unsigned memory add and retained operands")
	ROM_SYSTEM_BIOS(67, "xmem-load", "Staged compact-address shifted accumulator load")
	ROM_SYSTEM_BIOS(68, "cala", "Staged near accumulator call and exact return frame")
	ROM_SYSTEM_BIOS(69, "cala-producer", "Staged DLD target publication before near accumulator call")
	ROM_SYSTEM_BIOS(70, "mvdd", "Staged dual-data transfer address and operand ownership")
	ROM_SYSTEM_BIOS(71, "mvdd-repeat", "Repeated dual-data transfers with DARAM overlap ordering")
	ROM_SYSTEM_BIOS(72, "logic-absolute", "Retained absolute-addressed memory logic")
	ROM_SYSTEM_BIOS(73, "popd-mapped", "Mapped POPD destination and stack ownership")
	ROM_SYSTEM_BIOS(74, "popd-repeat", "Retained repeated POPD and mapped context restoration")
	ROM_SYSTEM_BIOS(75, "popd-ram", "Existing RAM POPD and compiler latency controls")
	ROM_SYSTEM_BIOS(76, "popd-edges", "POPD exclusions and DAGEN conflict replay")
	ROM_SYSTEM_BIOS(77, "bacc", "Retained near accumulator branches without call frames")
	ROM_SYSTEM_BIOS(78, "fbacc", "Retained far accumulator branches without call frames")
	ROM_SYSTEM_BIOS(79, "bacc-producer", "Near accumulator branches after DLD and mapped READA")
	ROM_SYSTEM_BIOS(80, "fbacc-producer", "Far accumulator branches after DLD and mapped READA")
	ROM_SYSTEM_BIOS(81, "logic-offset", "Retained long-offset logical memory operands")
	ROM_SYSTEM_BIOS(82, "logic-short", "Existing short logical memory controls")
	ROM_SYSTEM_BIOS(83, "logic-peripheral", "Existing peripheral logical memory controls")
	ROM_SYSTEM_BIOS(84, "load-offset", "Existing offset and absolute memory load controls")
	ROM_SYSTEM_BIOS(85, "logic-edges", "Long logical memory exclusion replay")
	ROM_SYSTEM_BIOS(86, "mvmd-t", "Staged T and TRN mapped moves")
	ROM_SYSTEM_BIOS(87, "mvdd-periph", "Retained RAM to peripheral dual transfers")
	ROM_SYSTEM_BIOS(88, "absolute", "Staged accumulator ABS conformance")
	ROM_SYSTEM_BIOS(89, "mpy-mem-imm", "Retained memory and immediate multiply")
	ROM_SYSTEM_BIOS(90, "mpy-t-imm", "Existing T and immediate multiply control")
	ROM_SYSTEM_BIOS(91, "rptb-branch", "Forward near branches inside block repeats")
	ROM_SYSTEM_BIOS(92, "rptb-br-edges", "In-block branch exclusions")
	ROM_SYSTEM_BIOS(93, "signed-branch", "Existing accumulator branch controls")
	ROM_SYSTEM_BIOS(94, "tc-branch", "Existing TC branch controls")
	ROM_SYSTEM_BIOS(95, "bio-branch", "Existing BIO branch controls")
	ROM_SYSTEM_BIOS(96, "plain-branch", "Existing held-word conditional branch controls")
	ROM_SYSTEM_BIOS(97, "software-intr", "Retained software interrupt entry and return")
	ROM_SYSTEM_BIOS(98, "ldu-periph", "Retained unsigned peripheral bus loads")
	ROM_SYSTEM_BIOS(99, "store-bypass", "Same-location store and load bus bypass")
	ROM_SYSTEM_BIOS(100, "stlm-bk", "STLM BK publication and circular-address visibility")
	ROM_SYSTEM_BIOS(101, "add-bk", "Retained absolute ADD from BK")
	ROM_SYSTEM_BIOS(102, "add-absolute", "Existing absolute ADD register and RAM controls")
	ROM_SYSTEM_BIOS(103, "mvmd-bk", "Retained BK transfers and STLM producer composition")
	ROM_SYSTEM_BIOS(104, "store-delay", "Complete two-word stores in control delays")
	ROM_SYSTEM_BIOS(105, "st-delay-edge", "Rejected stores straddling a control delay")
	ROM_SYSTEM_BIOS(106, "move-t", "Retained T and TRN move destinations with consumers")
	ROM_SYSTEM_BIOS(107, "mpy-delay", "Retained memory multiply in control delay words")
	ROM_SYSTEM_BIOS(108, "mpy-delay-edge", "Rejected multiply straddling a control delay")
	ROM_SYSTEM_BIOS(109, "cmpm-smem", "Retained Smem equality and indirect address ownership")
	ROM_SYSTEM_BIOS(110, "cmpm-branch", "Short indirect comparison and adjacent TC branch")
	ROM_SYSTEM_BIOS(111, "st-extended-ar", "Extended accumulator stores to retained AR destinations")
	ROM_SYSTEM_BIOS(112, "st-ar-consumer", "Extended AR stores with legal-gap indirect consumers")
	ROM_SYSTEM_BIOS(113, "ld-ts", "Retained T-controlled Smem loads")
	ROM_SYSTEM_BIOS(114, "ld-ts-producer", "Legal T producers followed by T-controlled loads")
	ROM_SYSTEM_BIOS(115, "mvdd-source", "Retained mapped-source dual transfers")
	ROM_SYSTEM_BIOS(116, "mvdd-source-rpt", "Repeated mapped-source copies and read-clear endpoints")
	ROM_SYSTEM_BIOS(117, "mvdd-source-edge", "Rejected mapped dual-transfer endpoint combinations")
	ROM_SYSTEM_BIOS(118, "ret-load", "Adjacent load and conditional return replay")
	ROM_SYSTEM_BIOS(119, "carry-branch", "Carry branch retained decision and delay replay")
	ROM_SYSTEM_BIOS(120, "banzd-long", "Long-offset BANZ/BANZD ownership and replay")
	ROM_SYSTEM_BIOS(121, "write-a", "Retained accumulator-addressed program writes")
	ROM_SYSTEM_BIOS(122, "write-a-edge", "Excluded WRITA sources and control compositions")
	ROM_SYSTEM_BIOS(123, "xmem-alu", "Retained compact Xmem ADD and SUB replay")
	ROM_SYSTEM_BIOS(124, "xmem-store", "Retained compact shifted Xmem stores")
	ROM_SYSTEM_BIOS(125, "xmem-store-bp", "Compact Xmem store and load bus bypass")
	ROM_SYSTEM_BIOS(126, "drsub", "Retained reversed long-word subtraction")
	ROM_SYSTEM_BIOS(127, "long-pair", "DADD and DSUB retained long-pair controls")
	ROM_SYSTEM_BIOS(128, "norm", "Retained normalization shift sampling and replay")
	ROM_SYSTEM_BIOS(129, "norm-edge", "Recovered negative normalization-count aliases")
	ROM_SYSTEM_BIOS(130, "exp-norm", "EXP producer and normalization T latency")
	ROM_SYSTEM_BIOS(131, "xc-signed", "Signed XC predicate and protected-slot replay")
	ROM_SYSTEM_BIOS(132, "xc-controls", "Existing XC sampling and interrupt controls")
	ROM_SYSTEM_BIOS(133, "mpy-dual", "Dual-memory multiply addresses and arithmetic replay")
	ROM_SYSTEM_BIOS(134, "mpy-dual-edge", "Excluded dual-memory multiply low-page operands")
	ROM_SYSTEM_BIOS(135, "alu-dual", "Dual-memory ADD SUB retained arithmetic replay")
	ROM_SYSTEM_BIOS(136, "alu-dual-edge", "Excluded dual-memory ADD SUB low-page operands")
	ROM_SYSTEM_BIOS(137, "mac-t-imm", "Immediate MAC retained T literal and accumulator selection")
	ROM_SYSTEM_BIOS(138, "mar-index", "Indexed MAR AR0 arithmetic and restoration")
	ROM_SYSTEM_BIOS(139, "minmax", "Signed accumulator MAX MIN Execute ownership and replay")
	ROM_SYSTEM_BIOS(140, "store-index", "AR0-indexed accumulator stores with retained Read addresses")
	ROM_SYSTEM_BIOS(141, "compare-delay", "CMPM BITF delayed branch condition and interrupt ownership")
	ROM_SYSTEM_BIOS(142, "cmp-delay-edge", "Incomplete CMPM BITF delay span exclusions")
	ROM_SYSTEM_BIOS(143, "asm-load", "Retained memory load to ASM without T publication")
	ROM_SYSTEM_BIOS(144, "asm-consumer", "Zero-latency ASM load and adjacent register consumers")
	ROM_SYSTEM_BIOS(145, "asm-edge", "ASM mapped peripheral source exclusions")
	ROM_SYSTEM_BIOS(146, "t-load-control", "Existing RAM mapped T loads and multiply pair control")
	ROM_SYSTEM_BIOS(147, "ts-alu", "Retained memory arithmetic with T-selected shifts")
	ROM_SYSTEM_BIOS(148, "ts-alu-producer", "Instruction-driven T shift arithmetic latency")
	ROM_SYSTEM_BIOS(149, "ts-alu-edge", "TS arithmetic unsupported shift and source exclusions")
	ROM_SYSTEM_BIOS(150, "st-t", "Extended accumulator stores to mapped T")
	ROM_SYSTEM_BIOS(151, "st-t-consumer", "Mapped T store shift latency and younger Read merge")
	ROM_SYSTEM_BIOS(152, "st-t-edge", "Mapped T store neighboring destination exclusions")
	ROM_SYSTEM_BIOS(153, "asm-mapped", "Mapped accumulator T TRN and AR sources for LD ASM")
	ROM_SYSTEM_BIOS(154, "rptb-single", "Single repeat within an outer block repeat")
	ROM_SYSTEM_BIOS(155, "rptb-rpt-edge", "Unsupported inner repeat setup forms")
	ROM_SYSTEM_BIOS(156, "macsu-dual", "Dual signed unsigned multiply accumulate ownership")
	ROM_SYSTEM_BIOS(157, "macsu-dual-edge", "Dual MACSU low mapped source exclusions")
	ROM_SYSTEM_BIOS(158, "rptb-fcall", "Far calls nested callees and interrupts within a block repeat")
	ROM_SYSTEM_BIOS(159, "rptb-fcall-edge", "Unsupported in-block call setup forms")
	ROM_SYSTEM_BIOS(160, "rptb-back", "Finite backward inner loops within block repeat")
	ROM_SYSTEM_BIOS(161, "load-delay", "Complete offset LD in control delay words")
	ROM_SYSTEM_BIOS(162, "ld-delay-edge", "Delay LD spill and unsupported load family exclusions")
	ROM_SYSTEM_BIOS(163, "ld-ar0-index", "AR0 indexed plain LD address read and replay ownership")
	ROM_SYSTEM_BIOS(164, "rptbd", "Delayed block repeat with retained delay words and replay")
	ROM_SYSTEM_BIOS(165, "rptbd-edge", "Delayed block repeat count control and spill exclusions")
	ROM_SYSTEM_BIOS(166, "rptbd-irq", "Delayed block repeat IRQ NMI and saved continuation")
	ROM_SYSTEM_BIOS(167, "mvdk-mapped", "Retained MVDK mapped register source and alias publication")
	ROM_SYSTEM_BIOS(168, "mvdk-producer", "Mapped MVDK accumulator producer latency and publication")
	ROM_SYSTEM_BIOS(169, "mvdk-map-edge", "Unadmitted MVDK control and peripheral source exclusions")
	ROM_SYSTEM_BIOS(170, "conditional-cd", "Delayed signed accumulator conditional calls and replay")
	ROM_SYSTEM_BIOS(171, "cond-cd-edge", "Unadmitted delayed conditional call predicate exclusions")
	ROM_SYSTEM_BIOS(172, "rptb-ccd", "Delayed conditional calls inside repeat blocks and at endpoints")
	ROM_SYSTEM_BIOS(173, "repeat-context", "Outside-body restoration of same or replaced active repeat contexts")
	ROM_SYSTEM_BIOS(174, "push-pop-same", "Adjacent PSHM POPM same-location source and destination ownership")
	ROM_SYSTEM_BIOS(175, "pmst-popd", "Indirect PMST stack restoration and retained program mapping")
	ROM_SYSTEM_BIOS(176, "repeat-pmst", "Indirect unchanged PMST restoration outside an active repeat body")
	ROM_SYSTEM_BIOS(177, "repeat-popd", "Indirect active repeat-context restoration outside the loop body")
	ROM_SYSTEM_BIOS(178, "mac-dual", "Signed dual-memory MAC with retained reads and accumulator feedback")
	ROM_SYSTEM_BIOS(179, "mac-dual-edge", "Signed dual-memory MAC low-page operand exclusions")
	ROM_SYSTEM_BIOS(180, "repeat-rearm", "Post-loop immediate BRC publication and adjacent repeat ownership")
	ROM_SYSTEM_BIOS(181, "rearm-edge", "Access-owned in-body BRC publication exclusion")
	ROM_SYSTEM_BIOS(182, "mar-reverse", "AR0 reverse-carry MAR updates with retained Access ownership")
	ROM_SYSTEM_BIOS(183, "rptb-bcd", "Delayed signed conditional branches within a repeat block")
	ROM_SYSTEM_BIOS(184, "asm-immediate", "Immediate ASM publication and zero-latency accumulator consumers")
	ROM_SYSTEM_BIOS(185, "asm-store", "Retained short ASM accumulator stores and Execute-owned source")
	ROM_SYSTEM_BIOS(186, "asm-store-ld", "Zero-latency immediate ASM load to short accumulator store")
	ROM_SYSTEM_BIOS(187, "shift-delay", "Complete shifted memory loads in retained control-delay words")
	ROM_SYSTEM_BIOS(188, "shift-delay-edge", "Shifted delay spill and unadmitted extension/source exclusions")
	ROM_SYSTEM_BIOS(189, "asm-store-bp", "ASM-shifted same-location store and load bypass")
	ROM_SYSTEM_BIOS(190, "move-brc", "Memory move block count and zero-latency repeat setup")
	ROM_SYSTEM_BIOS(191, "move-brc-edge", "Memory move block count body and delay exclusions")
	ROM_SYSTEM_BIOS(192, "ld-circular", "Signed circular loads with retained block size and replay")
	ROM_SYSTEM_BIOS(193, "block-xc-irq", "Short repeat XC interrupt protection and instance overlap")
	ROM_SYSTEM_BIOS(194, "block-xc", "Conditional block slots and loop endpoint ownership")
	ROM_SYSTEM_BIOS(195, "block-xc-edge", "Conditional execution crossing repeat endpoints is excluded")
	ROM_SYSTEM_BIOS(196, "imm-forward", "Immediate RAM store forwarding and producer replay")
	ROM_SYSTEM_BIOS(197, "repeat-isr", "Software-saved callback repeat and interrupted CCD resumption")
	ROM_SYSTEM_BIOS(198, "squr", "Accumulator square with signed 17-bit operand")
	ROM_SYSTEM_BIOS(199, "rptb-bd", "Unconditional delayed branches within a repeat block")
	ROM_SYSTEM_BIOS(200, "rptb-banz", "Long-offset BANZ within a repeat block")
	ROM_SYSTEM_BIOS(201, "add-delay", "Absolute mapped and RAM ADD within a control delay")
	ROM_SYSTEM_BIOS(202, "arith-bk", "Mapped BK operand capture for ordinary ADD and SUB")
	ROM_SYSTEM_BIOS(203, "arith-ar", "Existing mapped auxiliary-register arithmetic controls")
	ROM_SYSTEM_BIOS(204, "imm-index", "AR0-indexed immediate stores with retained literals")
	ROM_SYSTEM_BIOS(205, "fcala-delay", "Delayed accumulator far-call frame and interrupt composition")
	ROM_SYSTEM_BIOS(206, "mac-bk", "Mapped BK signed multiply-accumulate operand ownership")
	ROM_SYSTEM_BIOS(207, "mac-delay", "Retained signed multiply-accumulate in complete control delays")
	ROM_SYSTEM_BIOS(208, "mac-delay-edge", "Rejected multiply-accumulate straddling a control delay")
	ROM_SYSTEM_BIOS(209, "dld-delay", "Retained double-word load in complete control delays")
	ROM_SYSTEM_BIOS(210, "dld-delay-edge", "Rejected double-word load straddling a control delay")
	ROM_SYSTEM_BIOS(211, "st-dagen", "Retained constant DAGEN initialization and bank arbitration")
	ROM_SYSTEM_BIOS(212, "asm-delay", "Retained ASM load and adjacent shift in branch delays")
	ROM_SYSTEM_BIOS(213, "asm-delay-edge", "Rejected ASM load straddling a control delay")
	ROM_SYSTEM_BIOS(214, "repeat-body-isr", "Software-saved ISR repeat returning directly into an outer body")
	ROM_SYSTEM_BIOS(215, "add-sp", "Retained absolute ADD from SP and stack-update contexts")
	ROM_SYSTEM_BIOS(216, "instr-observer", "Execute-owned instruction observation without program-read aliases")
	ROM_SYSTEM_BIOS(217, "mvkd-sp", "Retained MVKD and MVDM from mapped SP with stack-update contexts")
	ROM_SYSTEM_BIOS(218, "rptb-page", "Low-PC repeat endpoint and foreign-page hazard")
	ROM_SYSTEM_BIOS(219, "rptb-fcalld", "Delayed far calls and complete delay spans inside block repeats")
	ROM_SYSTEM_BIOS(220, "st-shift-ar", "Shifted accumulator stores to retained AR destinations")
	ROM_SYSTEM_BIOS(221, "st-shift-ar-use", "Shifted AR stores with legal-gap indirect consumers")
	ROM_SYSTEM_BIOS(222, "st-shift-ar-bank", "Shifted stores and shared-bank early DAGEN write arbitration")
	ROM_SYSTEM_BIOS(223, "st-shift-ar-edge", "Shifted-store control and peripheral destination exclusions")
	ROM_SYSTEM_BIOS(224, "mvdk-acc", "Retained MVDK to accumulator fields and guard-width aliases")
	ROM_SYSTEM_BIOS(225, "mvdk-acc-prod", "Mapped and direct accumulator producers before MVDK field destinations")
	ROM_SYSTEM_BIOS(226, "mvkd-forward", "Retained MVKD source forwarded to same-address RAM followers")
	ROM_SYSTEM_BIOS(227, "rptbd-shift-st", "Shifted stores in complete RPTBD delays with interrupt and replay")
	ROM_SYSTEM_BIOS(228, "rptbd-shift-edge", "Shifted RPTBD span, destination and control-owner exclusions")
	ROM_SYSTEM_BIOS(229, "rpt-smem", "Basic memory repeat counts with address, interrupt and replay coverage")
	ROM_SYSTEM_BIOS(230, "rpt-smem-edge", "Memory repeat control, address and lifecycle exclusions")
	ROM_SYSTEM_BIOS(231, "rpt-ld", "Repeated accumulator loads with per-iteration flags, interrupts and replay")
	ROM_SYSTEM_BIOS(232, "rpt-ld-edge", "Unadmitted repeated register-family exclusions")
	ROM_SYSTEM_BIOS(233, "rpt-ld-block", "Focused repeated accumulator loads inside an outer block repeat")
	ROM_SYSTEM_BIOS(234, "stlm-pmst", "Execute-owned accumulator PMST stores with mapping and interrupt replay")
	ROM_SYSTEM_BIOS(235, "stlm-pmst-edge", "PMST store owner, addressing and neighboring-destination exclusions")
	ROM_SYSTEM_BIOS(236, "pshd-long", "Long-offset and absolute pushes with interrupt and replay coverage")
	ROM_SYSTEM_BIOS(237, "pshd-long-map", "Focused long-address push mapped-register source coverage")
	ROM_SYSTEM_BIOS(238, "pshd-long-edge", "Long-address push source, addressing and compound-owner exclusions")
	ROM_SYSTEM_BIOS(239, "mpy-mod3-edge", "Write-only short-preincrement multiply exclusions")
	ROM_SYSTEM_BIOS(240, "subc-mod3-edge", "Write-only short-preincrement SUBC exclusions")
	ROM_SYSTEM_BIOS(241, "rpt-subc-st", "Repeated legal SUBC and immediate store replay")
	ROM_SYSTEM_BIOS(242, "load-mod3-edge", "Write-only short-preincrement load exclusions")
	ROM_SYSTEM_BIOS(243, "indirect-load", "Legal short-indirect load address and read replay")
	ROM_SYSTEM_BIOS(244, "ldu-shift16", "Legal unsigned and shift16 load domains")
	ROM_SYSTEM_BIOS(245, "indirect-dld", "Legal indirect double-word load replay")
	ROM_SYSTEM_BIOS(246, "load-variable", "Legal variable-shift memory load replay")
	ROM_SYSTEM_BIOS(247, "bitf-mod3-edge", "Write-only short-preincrement compare and bitfield exclusions")
	ROM_SYSTEM_BIOS(248, "bitf-smem", "Read-capable Smem bitfield and immediate replay")
	ROM_SYSTEM_BIOS(249, "arith-mod3-edge", "Write-only short-preincrement arithmetic source exclusions")
	ROM_SYSTEM_BIOS(250, "memory-alu", "Read-capable scalar memory ADD and SUB replay")
	ROM_SYSTEM_BIOS(251, "shifted-alu", "Read-capable variable-shift memory arithmetic replay")
	ROM_SYSTEM_BIOS(252, "mul-mod3-edge", "Write-only short-preincrement multiply and load exclusions")
	ROM_SYSTEM_BIOS(253, "mul-t-sources", "Read-capable memory multiply and T-load replay")
	// BIOS selection is eight-bit (index + 1); further fixtures need another namespace.
	ROM_SYSTEM_BIOS(254, "stack-mod3-edge", "Atomic prefix and write-only MMR/push source exclusions")
ROM_END

ROM_START(tms54xtra)
	ROM_SYSTEM_BIOS(0, "stlm-t", "Accumulator T/TRN and BRC publication with retained consumers")
	ROM_SYSTEM_BIOS(1, "ov-branch", "Atomic overflow branches and consumed sticky flags")
	ROM_SYSTEM_BIOS(2, "cond-reg-store", "Atomic conditional T/BRC stores with read/write-back")
	ROM_SYSTEM_BIOS(3, "saccd", "Existing conditional accumulator-store regression banks")
	ROM_SYSTEM_BIOS(4, "atomic-ret", "Atomic signed conditional returns and stack ownership")
	ROM_SYSTEM_BIOS(5, "xorm", "Atomic memory XOR addressing, flags and transaction replay")
	ROM_SYSTEM_BIOS(6, "xc-status", "Access-owned conditional execution after status producers")
	ROM_SYSTEM_BIOS(7, "xc-latency", "Access-owned TC/carry/accumulator conditions across variable producer lengths")
	ROM_SYSTEM_BIOS(8, "xc-barrier", "Conditional-execute status after sequential-target return barriers")
	ROM_SYSTEM_BIOS(9, "parallel-sub", "Atomic parallel subtraction preserves fractional accumulator bits")
	ROM_SYSTEM_BIOS(10, "mmr-long-edge", "Forbidden long-offset and reserved MMR operand forms")
	ROM_SYSTEM_BIOS(11, "rpt-xfer-edge", "Forbidden single-repeat absolute and offset transfer operands")
	ROM_SYSTEM_BIOS(12, "mvdp", "Staged standalone data-to-program transfers")
	ROM_SYSTEM_BIOS(13, "mvdp-pair", "Adjacent retained data-to-program transfers")
	ROM_SYSTEM_BIOS(14, "rpt-mvdp", "Repeated retained data-to-program transfers")
	ROM_SYSTEM_BIOS(15, "rpt-mvdp-irq", "Repeated program copies with deferred IRQ and NMI")
	ROM_SYSTEM_BIOS(16, "std-swwsr", "Original-family SWWSR without C5416 bus controls")
	ROM_SYSTEM_BIOS(17, "std-stm", "Retained original-family SWWSR publication")
	ROM_SYSTEM_BIOS(18, "std-io", "Staged I/O with original SWWSR waits and READY")
	ROM_SYSTEM_BIOS(19, "std-io-atomic", "Atomic I/O with original SWWSR waits and READY")
	ROM_SYSTEM_BIOS(20, "clock-stm", "Retained CLKMD writes without an asserted physical PLL clock")
	ROM_SYSTEM_BIOS(21, "mvpd-extended", "Retained absolute and long-offset program-to-data copies")
	ROM_SYSTEM_BIOS(22, "mvpd-ext-edge", "Rejected mapped MVPD destinations without address effects")
	ROM_SYSTEM_BIOS(23, "mvpd-controls", "Existing short and repeated MVPD regression controls")
	ROM_SYSTEM_BIOS(24, "literal-guard", "Opcode-shaped literal words in legal block stores")
	ROM_SYSTEM_BIOS(25, "addm-ar", "Retained absolute mapped-AR addition and flags")
	ROM_SYSTEM_BIOS(26, "addm-ar-load", "Legal-gap indirect consumers after mapped-AR addition")
	ROM_SYSTEM_BIOS(27, "rotate", "Staged accumulator rotates and carry forwarding")
	ROM_SYSTEM_BIOS(28, "rptz", "Retained staged repeat and accumulator clearing")
	ROM_SYSTEM_BIOS(29, "rpt-store", "Repeated short accumulator stores through real pipeline slots")
	ROM_SYSTEM_BIOS(30, "rpt-store-edge", "Repeated mapped-store exclusions before DAGEN or bus effects")
	ROM_SYSTEM_BIOS(31, "bitt", "Retained staged BITT memory and T selection")
	ROM_SYSTEM_BIOS(32, "bitt-retd", "RETD with an absolute or long-offset BITT delay instruction")
	ROM_SYSTEM_BIOS(33, "tc-return", "TC/NTC returns with live BITT producers and retained decisions")
	ROM_SYSTEM_BIOS(34, "mar-circular", "Circular MAR with legal buffer, stride and compatibility cases")
	ROM_SYSTEM_BIOS(35, "bcd-arithmetic", "Retained arithmetic in AEQ/BEQ branch delay words")
	ROM_SYSTEM_BIOS(36, "bcd-arith-edge", "Excluded branch predicates and incomplete arithmetic delay spans")
	ROM_SYSTEM_BIOS(37, "signed-call", "All signed A/B conditional calls with retained target and frame")
	ROM_SYSTEM_BIOS(38, "rpt-writa", "Retained repeated WRITA with immediate and memory counts")
	ROM_SYSTEM_BIOS(39, "rpt-writa16", "Repeated WRITA on the standard 16-bit program-address profile")
	ROM_SYSTEM_BIOS(40, "serial-stm", "Retained immediate BSP control publication")
	ROM_SYSTEM_BIOS(41, "status-bitf", "Retained ST0 and ST1 bitfield sampling")
	ROM_SYSTEM_BIOS(42, "stlm-bsp", "Accumulator publication to external BSP transmit register")
	ROM_SYSTEM_BIOS(43, "stlm-bsp-edge", "Excluded BSP transmit control and repeat compositions")
	ROM_SYSTEM_BIOS(44, "rptb-exit", "Forward loop exits with explicit outside BRAF clear")
	ROM_SYSTEM_BIOS(45, "reada-brc", "Program-memory counter publication and spaced RPTB")
	ROM_SYSTEM_BIOS(46, "reada-brc16", "Standard-width program-memory counter publication")
	ROM_SYSTEM_BIOS(47, "reada-brc-edge", "Excluded repeated and active-loop count transfers")
	ROM_SYSTEM_BIOS(48, "st1-rules", "Primary-source ST1 reserved-bit and memory-write constraints")
	ROM_SYSTEM_BIOS(49, "st1-bits", "Status-bit instructions retain direct INTM control")
	ROM_SYSTEM_BIOS(50, "stlm-st1", "Accumulator status publication and SXM/ASM consumer latency")
	ROM_SYSTEM_BIOS(51, "stlm-cpl", "Full status accumulator store CPL consumer latency")
	ROM_SYSTEM_BIOS(52, "stlm-cmpt", "Full status accumulator store compatibility-mode latency")
	ROM_SYSTEM_BIOS(53, "stlm-st1-edge", "Excluded status store repeat/control compositions")
	ROM_SYSTEM_BIOS(54, "stlm-st1-irq", "Accumulator status publication IRQ/NMI composition")
	ROM_SYSTEM_BIOS(55, "shift-periph", "Shifted peripheral operand retention and access clock")
	ROM_SYSTEM_BIOS(56, "shift-bsp", "Shifted BSP receive acknowledgement and pending next frame")
	ROM_SYSTEM_BIOS(57, "shift-per-edge", "Excluded shifted peripheral control and arithmetic forms")
	ROM_SYSTEM_BIOS(58, "sat", "Staged signed saturation and overflow publication")
	ROM_SYSTEM_BIOS(59, "store-circular", "Circular accumulator stores with retained addresses")
	ROM_SYSTEM_BIOS(60, "tc-call", "TC and NTC conditional calls with retained predicate")
	ROM_SYSTEM_BIOS(61, "cmpm-acc", "Retained CMPM reads of all accumulator register lanes")
	ROM_SYSTEM_BIOS(62, "rpt-mvkd", "Repeated data-memory moves with retained source progression")
	ROM_SYSTEM_BIOS(63, "rpt-mvkd-edge", "Excluded mapped and extended repeated data moves")
	ROM_SYSTEM_BIOS(64, "stm-acc", "Immediate accumulator-field publication and mapped read ordering")
	ROM_SYSTEM_BIOS(65, "st-t-long", "Long-address T/TRN stores with retained destinations")
	ROM_SYSTEM_BIOS(66, "st-t-long-edge", "Rejected long-address T/TRN mapped destinations")
	ROM_SYSTEM_BIOS(67, "st-t-branch", "Ordinary T/TRN stores after nondelayed conditional branches")
	ROM_SYSTEM_BIOS(68, "shift-unowned", "Shifted bus reads of unowned low-address gaps")
	ROM_SYSTEM_BIOS(69, "portr-circular", "Circular I/O reads with retained pointers and samples")
	ROM_SYSTEM_BIOS(70, "portr-block", "Circular I/O sampling within a retained RPTBD block")
	ROM_SYSTEM_BIOS(71, "mac-family", "Dual MAC MACR MAS MASR with independent source and destination")
	ROM_SYSTEM_BIOS(72, "mac-family-edge", "Dual MAC family mapped operand exclusions")
	ROM_SYSTEM_BIOS(73, "stmac", "Parallel MAC stores with retained T X and ASM")
	ROM_SYSTEM_BIOS(74, "stmac-edge", "Parallel MAC mapped operand exclusions")
	ROM_SYSTEM_BIOS(75, "sqdst", "Square distance with retained operands and old accumulator feedback")
	ROM_SYSTEM_BIOS(76, "sqdst-edge", "Square distance mapped operand exclusions")
	ROM_SYSTEM_BIOS(77, "portr-bypass", "PORTR consumers and the native request-mask sequence")
	ROM_SYSTEM_BIOS(78, "mvdk-circular", "Circular source moves with retained data and mapped publications")
	ROM_SYSTEM_BIOS(79, "mvdk-circ-edge", "Circular move control and peripheral destination exclusions")
	ROM_SYSTEM_BIOS(80, "mvdk-repeat", "Repeated destination-addressed copies with consecutive EAR publication")
	ROM_SYSTEM_BIOS(81, "mvdk-rpt-edge", "Repeated MVDK mapped-memory composition exclusions")
	ROM_SYSTEM_BIOS(82, "rptb-call", "Near calls and returns at block endpoints and whole-block calls")
	ROM_SYSTEM_BIOS(83, "rptb-call-edge", "Internal and incomplete near calls in block repeats")
	ROM_SYSTEM_BIOS(84, "rpt-mem-copy", "Captured memory counts driving pipelined data transfers")
	ROM_SYSTEM_BIOS(85, "rpt-mem-wrap", "Captured repeat counts across the 16-bit program counter wrap")
	ROM_SYSTEM_BIOS(86, "rpt-mem-chain", "Independent ownership across copy and NOP memory-count repeats")
	ROM_SYSTEM_BIOS(87, "addc", "Unsigned Smem addition with Execute-live carry and accumulators")
	ROM_SYSTEM_BIOS(88, "addc-edge", "ADDC control and unowned mapped-source exclusions")
	ROM_SYSTEM_BIOS(89, "addc-chain", "Adjacent ADDC accumulator and carry dependencies")
	ROM_SYSTEM_BIOS(90, "repeat-stlm", "STLM suspension and restoration of an interrupted block repeat")
	ROM_SYSTEM_BIOS(91, "mvdm-acc", "Absolute data-to-accumulator-slice MVDM")
	ROM_SYSTEM_BIOS(92, "mvdm-acc-edge", "MVDM accumulator control-source exclusions")
	ROM_SYSTEM_BIOS(93, "stack-page0", "Direct page-zero RAM PSHM and POPM")
	ROM_SYSTEM_BIOS(94, "stack-page0-edge", "Direct page-zero stack peripheral and indirect exclusions")
	ROM_SYSTEM_BIOS(95, "stld", "Parallel store with retained Xmem load into accumulator or T")
	ROM_SYSTEM_BIOS(96, "stld-edge", "Parallel store/load mapped operand exclusions")
	ROM_SYSTEM_BIOS(97, "unsigned-map", "ADDS and SUBS retained mapped sources and Execute-live arithmetic")
	ROM_SYSTEM_BIOS(98, "unsigned-edge", "ADDS and SUBS mapped control-source exclusions")
	ROM_SYSTEM_BIOS(99, "cmpm-map", "CMPM retained T TRN and auxiliary sources with TC consumers")
	ROM_SYSTEM_BIOS(100, "cmpm-map-edge", "CMPM unowned mapped-source exclusions")
	ROM_SYSTEM_BIOS(101, "poll-interrupt", "Interrupt delivery through a delayed conditional polling loop")
	ROM_SYSTEM_BIOS(102, "rptb-tc-delay", "TC NTC delayed branch ownership inside a block repeat")
	ROM_SYSTEM_BIOS(103, "rcd", "Conditional delayed return predicate stack and retained delay words")
	ROM_SYSTEM_BIOS(104, "addm-brc", "Retained absolute ADDM arithmetic on BRC")
	ROM_SYSTEM_BIOS(105, "addm-brc-loop", "ADDM BRC publication before separated RPTB and RPTBD consumers")
	ROM_SYSTEM_BIOS(106, "st-mvmm", "Immediate-store postincrement visibility to MVMM")
	ROM_SYSTEM_BIOS(107, "ldm-ar-age", "Older LDM source before younger DAGEN updates")
	ROM_SYSTEM_BIOS(108, "mapped-load-age", "Mapped LD and LDU before younger DAGEN updates")
	ROM_SYSTEM_BIOS(109, "mvmd-repeat", "Fixed AL BL source with incrementing repeated MVMD destination")
	ROM_SYSTEM_BIOS(110, "rptbd-t-load", "Delay-owned T load consumed by the first RPTBD multiply")
	ROM_SYSTEM_BIOS(111, "rptb-exit-delay", "Delayed forward block exits retain decision and delay ownership")
	ROM_SYSTEM_BIOS(112, "mvmd-rptz", "RPTZ clear precedes the first fixed-accumulator MVMD sample")
	ROM_SYSTEM_BIOS(113, "squr-smem", "Memory SQUR retains its signed operand and publishes T at Read")
	ROM_SYSTEM_BIOS(114, "rpt-sfta", "Repeated arithmetic accumulator shift carry overflow and replay")
	ROM_SYSTEM_BIOS(115, "addm-t", "Mapped T ADDM retained arithmetic and replay")
	ROM_SYSTEM_BIOS(116, "rptb-memory", "Memory-counted single repeat inside an outer block")
	ROM_SYSTEM_BIOS(117, "banzd-t-load", "Delay-owned T load across taken and untaken BANZD")
	ROM_SYSTEM_BIOS(118, "sqacc-repeat", "Repeated RAM square accumulate per-iteration ownership and replay")
	ROM_SYSTEM_BIOS(119, "sqacc-rpt-edge", "Repeated square mapped and peripheral source exclusions")
	ROM_SYSTEM_BIOS(120, "mac-rpt", "Repeated dual MAC paired reads live feedback and replay")
	ROM_SYSTEM_BIOS(121, "mac-rpt-edge", "Repeated dual MAC low operand exclusions")
	ROM_SYSTEM_BIOS(122, "bd-rpt", "BD delay repeat completes before a fresh target repeat")
	ROM_SYSTEM_BIOS(123, "bd-rpt-edge", "Delay repeat owner and word-span exclusions")
	ROM_SYSTEM_BIOS(124, "stm-ind", "Plain indirect STM page-zero RAM publication")
	ROM_SYSTEM_BIOS(125, "stm-ind-edge", "Plain indirect STM register and peripheral exclusions")
	ROM_SYSTEM_BIOS(126, "stld-repeat", "Repeated parallel store and accumulator or T load")
	ROM_SYSTEM_BIOS(127, "stld-rpt-edge", "Repeated parallel store/load mapped operand exclusions")
	ROM_SYSTEM_BIOS(128, "block-xc-end-irq", "XC1 repeat-end protection across wrap and final fallthrough")
	ROM_SYSTEM_BIOS(129, "asm-circular", "ASM-shifted circular accumulator stores")
	ROM_SYSTEM_BIOS(130, "st-par-alu", "Parallel store and add or subtract")
	ROM_SYSTEM_BIOS(131, "st-par-edge", "Parallel store arithmetic mapped operand exclusions")
	ROM_SYSTEM_BIOS(132, "mvdk-index", "AR0-indexed source moves with mapped publications")
	ROM_SYSTEM_BIOS(133, "mvdk-index-edge", "Indexed move control and peripheral destination exclusions")
	ROM_SYSTEM_BIOS(134, "mvdk-index-rpt", "Repeated indexed source moves and overlapping RAM copies")
	ROM_SYSTEM_BIOS(135, "mvdk-irpt-edge", "Repeated indexed move mapped source and destination exclusions")
	ROM_SYSTEM_BIOS(136, "cond-store", "Conditional register and accumulator RAM stores")
	ROM_SYSTEM_BIOS(137, "cond-store-edge", "Conditional store mapped destination exclusions")
	ROM_SYSTEM_BIOS(138, "shift16-map", "Shift16 ADD SUB with retained owned mapped sources")
	ROM_SYSTEM_BIOS(139, "shift16-map-edge", "Shift16 arithmetic control and peripheral source exclusions")
	ROM_SYSTEM_BIOS(140, "logic-map", "Memory logic with retained owned mapped sources")
	ROM_SYSTEM_BIOS(141, "logic-map-edge", "Memory logic unowned low-register exclusions")
	ROM_SYSTEM_BIOS(142, "rptbd-rpt", "Short repeat in the first RPTBD delay word")
	ROM_SYSTEM_BIOS(143, "rptbd-rpt-edge", "RPTBD delay repeat span and count exclusions")
	ROM_SYSTEM_BIOS(144, "retd-rpt", "Delay repeat drains before a retained stack return")
	ROM_SYSTEM_BIOS(145, "stm-st1", "Immediate ST1 publication and arithmetic consumers")
	ROM_SYSTEM_BIOS(146, "stm-st1-edge", "Immediate ST1 control composition exclusions")
	ROM_SYSTEM_BIOS(147, "stm-cpl", "Immediate ST1 CPL consumer latency")
	ROM_SYSTEM_BIOS(148, "stm-cmpt", "Immediate ST1 CMPT consumer latency")
	ROM_SYSTEM_BIOS(149, "alu-map", "Dual memory ADD SUB with owned mapped sources")
	ROM_SYSTEM_BIOS(150, "alu-map-edge", "Dual memory ADD SUB unowned source exclusions")
	ROM_SYSTEM_BIOS(151, "ov-branch-pipe", "Overflow branch decision and consumed sticky flags")
	ROM_SYSTEM_BIOS(152, "shift-alu-delay", "Shifted memory ADD SUB in delayed transfer words")
	ROM_SYSTEM_BIOS(153, "maca-delay", "Accumulator memory multiply in delayed transfer words")
	ROM_SYSTEM_BIOS(154, "delay-copy", "Read retained successor memory copy")
	ROM_SYSTEM_BIOS(155, "delay-copy-ctrl", "Successor memory copy in control delay words")
	ROM_SYSTEM_BIOS(156, "xc-ret", "Return preserves bounded XC word protection")
	ROM_SYSTEM_BIOS(157, "move-brc-delay", "BRC move in ordinary delayed control words")
	ROM_SYSTEM_BIOS(158, "rptbd-square", "Square arithmetic in delayed block setup words")
	ROM_SYSTEM_BIOS(159, "alu-map-irq", "Dual ADD SUB samples IMR IFR without acknowledgement")
	ROM_SYSTEM_BIOS(160, "alu-low-bus", "Dual ADD SUB samples unowned low words through board map")
	ROM_SYSTEM_BIOS(161, "mpyr-delay", "Rounded memory multiplication in delayed controls")
	ROM_SYSTEM_BIOS(162, "mpyr-delay-edge", "Reject rounded memory multiply straddling delay end")
	ROM_SYSTEM_BIOS(163, "stld-map", "Parallel load from owned registers and unowned board words")
	ROM_SYSTEM_BIOS(164, "stld-bsp", "Parallel BSP receive and RAM store")
	ROM_SYSTEM_BIOS(165, "stld-periph", "Parallel volatile peripheral read and RAM store")
	ROM_SYSTEM_BIOS(166, "square-delay", "Square accumulation in ordinary delayed controls")
	ROM_SYSTEM_BIOS(167, "st-par-mpy", "Parallel store and retained-T memory multiply")
	ROM_SYSTEM_BIOS(168, "st-mpy-edge", "Parallel multiply mapped operand exclusions")
	ROM_SYSTEM_BIOS(169, "cond-store-ar", "Conditional stores to mapped auxiliary registers")
	ROM_SYSTEM_BIOS(170, "calld-rpt", "Short delay repeat completes before a near call and return")
	ROM_SYSTEM_BIOS(171, "ctrl-t-load", "Short T loads in ordinary delayed controls")
	ROM_SYSTEM_BIOS(172, "stm-low-ram", "Absolute immediate low RAM stores and adjacent consumers")
	ROM_SYSTEM_BIOS(173, "st-alu-rpt", "Repeated parallel stores and ADD/SUB feedback")
	ROM_SYSTEM_BIOS(174, "st-alu-rpt-edge", "Repeated parallel ADD/SUB mapped operand exclusions")
	ROM_SYSTEM_BIOS(175, "squr-delay", "Memory SQUR in ordinary control delay words")
	ROM_SYSTEM_BIOS(176, "rptbd-squr", "Memory SQUR and MPYA in RPTBD delay words")
	ROM_SYSTEM_BIOS(177, "bcd-rpt", "Conditional branch outcomes after delay-owned repeats")
	ROM_SYSTEM_BIOS(178, "bcd-rpt-edge", "Conditional branch delay repeat exclusions")
	ROM_SYSTEM_BIOS(179, "bitt-ctrl", "Short BITT and T-producer ordering in delayed controls")
	ROM_SYSTEM_BIOS(180, "mvdd-follow", "Retained MVDD forwarding across adjacency and delayed calls")
	ROM_SYSTEM_BIOS(181, "st-acc-mvmm", "Pending accumulator-store DAGEN visibility to MVMM")
	ROM_SYSTEM_BIOS(182, "shift16-load-age", "Shifted mapped AR loads retain the older Read sample")
	ROM_SYSTEM_BIOS(183, "addsub-ar-age", "Mapped ADD SUB retains the older AR Read sample")
	ROM_SYSTEM_BIOS(184, "bd-read-a", "Retained READA transfers in unconditional BD delay spans")
	ROM_SYSTEM_BIOS(185, "mac-map-imm", "Read-retained mapped source with Execute-live immediate MAC")
	ROM_SYSTEM_BIOS(186, "tc-delay-call", "TC NTC delayed conditional calls retain their Read decision")
	ROM_SYSTEM_BIOS(187, "tmac-repeat", "Repeated RAM T-based MAC retains operands and accumulator feedback")
	ROM_SYSTEM_BIOS(188, "tmac-rpt-edge", "Repeated T-based MAC mapped and peripheral exclusions")
	ROM_SYSTEM_BIOS(189, "lms", "Paired LMS samples and dual accumulator Execute ownership")
	ROM_SYSTEM_BIOS(190, "lms-edge", "LMS mapped and peripheral operand exclusions")
	ROM_SYSTEM_BIOS(191, "addm-ar-short", "Short DP and plain indirect mapped AR ADDM ownership")
	ROM_SYSTEM_BIOS(192, "bd-maca-long", "Long offset and absolute accumulator multiply in BD delay")
	ROM_SYSTEM_BIOS(193, "bd-maca-edge", "Long accumulator multiply delay spill exclusions")
	ROM_SYSTEM_BIOS(194, "push-mvmm-sp", "MVMM SP view includes preceding pending stack allocations")
	ROM_SYSTEM_BIOS(195, "xorm-absolute", "Staged absolute immediate XORM retained read and publication")
	ROM_SYSTEM_BIOS(196, "mvdm-t", "MVDM T TRN source capture and adjacent mapped consumer")
	ROM_SYSTEM_BIOS(197, "move-rpt-owner", "Mapped move precedes an independent repeat header")
	ROM_SYSTEM_BIOS(198, "shifted-bk", "Read-owned extended ADD/SUB from mapped BK")
	ROM_SYSTEM_BIOS(199, "shift-bk-edge", "Excluded shifted BK and adjacent register compositions")
	ROM_SYSTEM_BIOS(200, "pmac-mod3-edge", "Write-only short-preincrement program multiply source exclusions")
	ROM_SYSTEM_BIOS(201, "store-observe", "Retained accumulator-store inspection across pending write restore")
	ROM_SYSTEM_BIOS(202, "push-observe", "Retained direct-PSHM stack and source inspection across restore")
	ROM_SYSTEM_BIOS(203, "dual-sub-native", "Exact native dual-memory subtraction and pointer updates")
	ROM_SYSTEM_BIOS(204, "delay-mod3-edge", "Write-only short-preincrement LTD DELAY source exclusions")
	ROM_SYSTEM_BIOS(205, "idle-modes", "Staged IDLE1/2/3 barriers, timer clocks, interrupt wake and replay")
	ROM_SYSTEM_BIOS(206, "clock-mvdk", "Retained RAM-to-CLKMD moves and peripheral access across restore")
	ROM_SYSTEM_BIOS(207, "clock-mvdk-edge", "Excluded repeated/delayed CLKMD moves have no operand or clock effects")
	ROM_SYSTEM_BIOS(208, "pop-irq", "Retained IMR/IFR stack writes and W1C acknowledgement across restore")
	ROM_SYSTEM_BIOS(209, "pop-irq-edge", "Excluded repeated/delayed interrupt pops have no stack or acknowledgement effects")
	ROM_SYSTEM_BIOS(210, "mpy-abs-edge", "Unadmitted memory-immediate MPY absolute-alias exclusions")
	ROM_SYSTEM_BIOS(211, "mac-abs-edge", "Unadmitted memory-immediate MAC absolute-alias exclusions")
	ROM_SYSTEM_BIOS(212, "delay-abs-edge", "Unadmitted LTD DELAY absolute-alias exclusions")
	ROM_SYSTEM_BIOS(213, "pmac-abs-edge", "Unadmitted MACP MACD absolute-alias exclusions")
	ROM_SYSTEM_BIOS(214, "mem-t-abs-edge", "Unadmitted memory T multiply absolute-alias exclusions")
	ROM_SYSTEM_BIOS(215, "load-abs-edge", "Unadmitted scalar load absolute-alias exclusions")
	ROM_SYSTEM_BIOS(216, "rmw-abs-edge", "Unadmitted read-modify-write absolute-alias exclusions")
	ROM_SYSTEM_BIOS(217, "logic-abs-edge", "Unadmitted memory logic absolute-alias exclusions")
	ROM_SYSTEM_BIOS(218, "arith-abs-edge", "Unadmitted scalar arithmetic absolute-alias exclusions")
	ROM_SYSTEM_BIOS(219, "long-abs-edge", "Unadmitted long-memory arithmetic absolute-alias exclusions")
	ROM_SYSTEM_BIOS(220, "source-domain", "Complete implemented single-memory read/RMW exclusion census")
ROM_END

ROM_START(tms54rom4)
	ROM_SYSTEM_BIOS(0, "entry", "Transform-entry snapshot")
	ROM_SYSTEM_BIOS(1, "cold", "Cold reset from mask ROM and DROM")
	ROM_SYSTEM_BIOS(2, "crc-helper", "Isolated acquired ROM carry/XOR helper")
	ROM_SYSTEM_BIOS(3, "crc-packed", "Isolated acquired ROM packed CRC")
	ROM_SYSTEM_BIOS(4, "fr-packing", "Isolated acquired ROM FR parameter round trip")
	ROM_SYSTEM_BIOS(5, "fr-decoder", "Isolated acquired ROM FR decoder")
	ROM_SYSTEM_BIOS(6, "fr-encoder", "Isolated acquired ROM FR encoder")
	ROM_SYSTEM_BIOS(7, "efr-crc", "Isolated acquired ROM EFR CRC")
	ROM_SYSTEM_BIOS(8, "efr-packing", "Isolated acquired ROM EFR packing and error detection")
	ROM_SYSTEM_BIOS(9, "efr-decoder", "Isolated acquired ROM EFR speech decoder")
	ROM_SYSTEM_BIOS(10, "efr-encoder", "Isolated acquired ROM EFR speech encoder")
	ROM_SYSTEM_BIOS(11, "efr-dtx-decoder", "Isolated acquired ROM EFR silence and speech decoder")
	ROM_SYSTEM_BIOS(12, "efr-dtx-encoder", "Isolated acquired ROM EFR silence and speech encoder")
	ROM_SYSTEM_BIOS(13, "efr-erasure", "Isolated acquired ROM EFR active-speech erasure decoder")
	ROM_SYSTEM_BIOS(14, "efr-homing", "Isolated acquired ROM EFR decoder homing and speech restart")
	ROM_SYSTEM_BIOS(15, "efr-enc-homing", "Isolated acquired ROM EFR encoder homing and speech restart")
	ROM_SYSTEM_BIOS(16, "efr-loss-observe", "Characterize native EFR loss differences without conformance promotion")
	ROM_SYSTEM_BIOS(17, "efr-invalid-sid", "Isolated acquired ROM EFR invalid SID and bad SID decoder")
	ROM_SYSTEM_BIOS(18, "efr-home-errors", "Characterize original EFR homing/error reset and PCM differences")
	ROM_SYSTEM_BIOS(19, "cold-pipeline", "Cold mask ROM execution with the staged CPU frontend")
	ROM_SYSTEM_BIOS(20, "fr-decoder-pipe", "Isolated acquired ROM FR decoder on staged CPU")
	ROM_SYSTEM_BIOS(21, "fr-encoder-pipe", "Isolated acquired ROM FR encoder on staged CPU")
	ROM_SYSTEM_BIOS(22, "efr-encoder-pipe", "Isolated acquired ROM EFR encoder on staged CPU")
	ROM_SYSTEM_BIOS(23, "efr-decoder-pipe", "Isolated acquired ROM EFR decoder on staged CPU")
	ROM_SYSTEM_BIOS(24, "efr-dtx-enc-pipe", "Isolated acquired ROM EFR DTX encoder on staged CPU")
	ROM_SYSTEM_BIOS(25, "efr-dtx-dec-pipe", "Isolated acquired ROM EFR DTX decoder on staged CPU")
	ROM_SYSTEM_BIOS(26, "efr-ehome-pipe", "Isolated acquired ROM EFR encoder homing on staged CPU")
	ROM_SYSTEM_BIOS(27, "efr-homing-pipe", "Isolated acquired ROM EFR decoder homing on staged CPU")
	ROM_SYSTEM_BIOS(28, "efr-isid-pipe", "Isolated acquired ROM EFR invalid SID decoder on staged CPU")
	ROM_SYSTEM_BIOS(29, "efr-erasure-pipe", "Strict acquired ROM EFR erasure comparison on staged CPU")
	ROM_SYSTEM_BIOS(30, "efr-loss-pipe", "Characterize staged EFR loss differences without conformance promotion")
	ROM_SYSTEM_BIOS(31, "efr-herr-pipe", "Characterize staged EFR homing error reset and PCM differences")
	ROM_SYSTEM_BIOS(32, "rf-bitmap", "Original RF search bitmap decoder basis and replay")
	ROM_SYSTEM_BIOS(33, "rf-bitmap-pipe", "Original RF search bitmap decoder on staged CPU")
	ROM_SYSTEM_BIOS(34, "rf-pairs", "Original adjacent RF pair products and TC history policy")
	ROM_SYSTEM_BIOS(35, "rf-pairs-pipe", "Original adjacent RF pair products on staged CPU")
	ROM_SYSTEM_BIOS(36, "efr-lost-sid", "Original EFR lost-SID muting and speech resumption")
	ROM_SYSTEM_BIOS(37, "efr-lsid-pipe", "Original EFR lost-SID muting on staged CPU")
	ROM_SYSTEM_BIOS(38, "efr-lsid-observe", "Characterize complete EFR lost-SID recovery without conformance promotion")
	ROM_SYSTEM_BIOS(39, "efr-lso-pipe", "Characterize complete EFR lost-SID recovery on staged CPU")
	ROM_REGION16_LE(0x80000, "dspprg", 0)
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe, CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(38))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe, CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(39))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe, CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(36))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe, CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(37))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(34))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(35))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(32))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(33))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(31))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(29))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(30))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(28))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(24))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(25))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(26))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(27))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(23))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(22))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(21))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(20))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(19))
	ROMX_LOAD("transform_entry_prog.bin", 0, 0x80000,
			CRC(99757118) SHA1(0a1da67d21f4c333acd331271c3d9f08e896008f),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(0))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(1))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(2))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(3))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(4))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(5))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(6))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(7))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(8))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(9))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(10))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(11))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(12))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(13))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(14))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(15))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(16))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(17))
	ROMX_LOAD("dsp_full.bin", 0, 0x1fffe,
			CRC(886f35e4) SHA1(a05a1e96a8c36ec5a47e1ea059d15afa54ca5739),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(18))
	ROM_REGION16_LE(0x20000, "dspdata", 0)
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000, CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(38))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000, CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(39))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000, CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(36))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000, CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(37))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(34))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(35))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(32))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(33))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(31))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(29))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(30))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(28))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(24))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(25))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(26))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(27))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(23))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(22))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(21))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(20))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(19))
	ROMX_LOAD("transform_entry_data.bin", 0, 0x20000,
			CRC(bef92101) SHA1(1c547eb7fd457d95cdf7956462e80a40dbadb46e),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(0))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(1))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(2))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(3))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(4))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(5))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(6))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(7))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(8))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(9))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(10))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(11))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(12))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(13))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(14))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(15))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(16))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(17))
	ROMX_LOAD("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d),
			ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(18))
	ROM_REGION(24206, "efr_homing_error_input", 0)
	ROMX_LOAD("homing_error.dec", 0, 24206, CRC(f04d25bf) SHA1(e232a2cbc161fbacb185758001677c1e462bf0ca), ROM_BIOS(18))
	ROMX_LOAD("homing_error.dec", 0, 24206, CRC(f04d25bf) SHA1(e232a2cbc161fbacb185758001677c1e462bf0ca), ROM_BIOS(31))
	ROM_REGION(15680, "efr_homing_error_pcm", 0)
	ROMX_LOAD("homing_error.out", 0, 15680, CRC(f1f822a8) SHA1(fbdf3d13761497e798baadc21b5dd8ac327ca593), ROM_BIOS(18))
	ROMX_LOAD("homing_error.out", 0, 15680, CRC(f1f822a8) SHA1(fbdf3d13761497e798baadc21b5dd8ac327ca593), ROM_BIOS(31))
	ROM_REGION(255892, "efr_lost_sid_input", 0)
	ROMX_LOAD("lost_sid.dec", 0, 255892, CRC(943bba49) SHA1(f26d632264a5b684024e45130b5bd337e095bacb), ROM_BIOS(38))
	ROMX_LOAD("lost_sid.dec", 0, 255892, CRC(943bba49) SHA1(f26d632264a5b684024e45130b5bd337e095bacb), ROM_BIOS(39))
	ROMX_LOAD("lost_sid.dec", 0, 255892, CRC(943bba49) SHA1(f26d632264a5b684024e45130b5bd337e095bacb), ROM_BIOS(36))
	ROMX_LOAD("lost_sid.dec", 0, 255892, CRC(943bba49) SHA1(f26d632264a5b684024e45130b5bd337e095bacb), ROM_BIOS(37))
	ROM_REGION(165760, "efr_lost_sid_pcm", 0)
	ROMX_LOAD("lost_sid.out", 0, 165760, CRC(409e4ecb) SHA1(fdf1df0aec709a45a9a4819bb3fb49f76c93bdbe), ROM_BIOS(38))
	ROMX_LOAD("lost_sid.out", 0, 165760, CRC(409e4ecb) SHA1(fdf1df0aec709a45a9a4819bb3fb49f76c93bdbe), ROM_BIOS(39))
	ROMX_LOAD("lost_sid.out", 0, 165760, CRC(409e4ecb) SHA1(fdf1df0aec709a45a9a4819bb3fb49f76c93bdbe), ROM_BIOS(36))
	ROMX_LOAD("lost_sid.out", 0, 165760, CRC(409e4ecb) SHA1(fdf1df0aec709a45a9a4819bb3fb49f76c93bdbe), ROM_BIOS(37))
	ROM_REGION(255892, "efr_invalid_sid_input", 0)
	ROMX_LOAD("invalid_sid.dec", 0, 255892, CRC(1f747e55) SHA1(a26dfa2a8406406114ffb47092d98c5fdd78e758), ROM_BIOS(17))
	ROMX_LOAD("invalid_sid.dec", 0, 255892, CRC(1f747e55) SHA1(a26dfa2a8406406114ffb47092d98c5fdd78e758), ROM_BIOS(28))
	ROM_REGION(165760, "efr_invalid_sid_pcm", 0)
	ROMX_LOAD("invalid_sid.out", 0, 165760, CRC(e9d2e42a) SHA1(ccd15ebb216d99c0ff7bd9a54f250f816d52b779), ROM_BIOS(17))
	ROMX_LOAD("invalid_sid.out", 0, 165760, CRC(e9d2e42a) SHA1(ccd15ebb216d99c0ff7bd9a54f250f816d52b779), ROM_BIOS(28))
	ROM_REGION(14720, "efr_homing_encoder_pcm", 0)
	ROMX_LOAD("homing_enc.inp", 0, 14720, CRC(b1c8352b) SHA1(2aef5f790d2fad7ee3acd529bfca4656b44d9165), ROM_BIOS(15))
	ROMX_LOAD("homing_enc.inp", 0, 14720, CRC(b1c8352b) SHA1(2aef5f790d2fad7ee3acd529bfca4656b44d9165), ROM_BIOS(26))
	ROM_REGION(22632, "efr_homing_encoder_serial", 0)
	ROMX_LOAD("homing_enc.cod", 0, 22632, CRC(7457992e) SHA1(10aee87b5da40ac216c96306fa7ad7616e2cceda), ROM_BIOS(15))
	ROMX_LOAD("homing_enc.cod", 0, 22632, CRC(7457992e) SHA1(10aee87b5da40ac216c96306fa7ad7616e2cceda), ROM_BIOS(26))
	ROM_REGION(22724, "efr_homing_input", 0)
	ROMX_LOAD("homing.dec", 0, 22724, CRC(7935ce74) SHA1(8d28790270a6897f04f9ac4bf19d8c5c0aeaf170), ROM_BIOS(14))
	ROMX_LOAD("homing.dec", 0, 22724, CRC(7935ce74) SHA1(8d28790270a6897f04f9ac4bf19d8c5c0aeaf170), ROM_BIOS(27))
	ROM_REGION(14720, "efr_homing_pcm", 0)
	ROMX_LOAD("homing.out", 0, 14720, CRC(45ad424d) SHA1(d006cdaa2cbfb6b65c1c7954ed22442491ef990a), ROM_BIOS(14))
	ROMX_LOAD("homing.out", 0, 14720, CRC(45ad424d) SHA1(d006cdaa2cbfb6b65c1c7954ed22442491ef990a), ROM_BIOS(27))
	ROM_REGION(80522, "efr_erasure_input", 0)
	ROMX_LOAD("erasure.dec", 0, 80522, CRC(22c45c1f) SHA1(d43031c892249f8bb518bd22cea3b3211f9faeb4), ROM_BIOS(13))
	ROMX_LOAD("erasure.dec", 0, 80522, CRC(22c45c1f) SHA1(d43031c892249f8bb518bd22cea3b3211f9faeb4), ROM_BIOS(16))
	ROMX_LOAD("erasure.dec", 0, 80522, CRC(22c45c1f) SHA1(d43031c892249f8bb518bd22cea3b3211f9faeb4), ROM_BIOS(29))
	ROMX_LOAD("erasure.dec", 0, 80522, CRC(22c45c1f) SHA1(d43031c892249f8bb518bd22cea3b3211f9faeb4), ROM_BIOS(30))
	ROM_REGION(52160, "efr_erasure_pcm", 0)
	ROMX_LOAD("erasure.out", 0, 52160, CRC(804fa6e4) SHA1(2354fca3c58245950b00734f002acbcb354a323b), ROM_BIOS(13))
	ROMX_LOAD("erasure.out", 0, 52160, CRC(804fa6e4) SHA1(2354fca3c58245950b00734f002acbcb354a323b), ROM_BIOS(16))
	ROMX_LOAD("erasure.out", 0, 52160, CRC(804fa6e4) SHA1(2354fca3c58245950b00734f002acbcb354a323b), ROM_BIOS(29))
	ROMX_LOAD("erasure.out", 0, 52160, CRC(804fa6e4) SHA1(2354fca3c58245950b00734f002acbcb354a323b), ROM_BIOS(30))
	ROM_REGION(165760, "efr_dtx_encoder_pcm", 0)
	ROMX_LOAD("dtx.inp", 0, 165760, CRC(4e0886dc) SHA1(d9b80f83f09556f430f03deb17c207b4aefe782e), ROM_BIOS(12))
	ROMX_LOAD("dtx.inp", 0, 165760, CRC(4e0886dc) SHA1(d9b80f83f09556f430f03deb17c207b4aefe782e), ROM_BIOS(24))
	ROM_REGION(254856, "efr_dtx_encoder_serial", 0)
	ROMX_LOAD("dtx.cod", 0, 254856, CRC(6b2a679f) SHA1(00c8972a74dd4e06843058d7a1899e0d5d957bdc), ROM_BIOS(12))
	ROMX_LOAD("dtx.cod", 0, 254856, CRC(6b2a679f) SHA1(00c8972a74dd4e06843058d7a1899e0d5d957bdc), ROM_BIOS(24))
	ROM_REGION(255892, "efr_dtx_input", 0)
	ROMX_LOAD("dtx.dec", 0, 255892, CRC(8bb21b82) SHA1(464b9f354aa4cd2828c3f80e812b18af170f7624), ROM_BIOS(11))
	ROMX_LOAD("dtx.dec", 0, 255892, CRC(8bb21b82) SHA1(464b9f354aa4cd2828c3f80e812b18af170f7624), ROM_BIOS(25))
	ROM_REGION(165760, "efr_dtx_pcm", 0)
	ROMX_LOAD("dtx.out", 0, 165760, CRC(a2bc82da) SHA1(44ea0a4b98623d3282f8bdafb0a068287a879f47), ROM_BIOS(11))
	ROMX_LOAD("dtx.out", 0, 165760, CRC(a2bc82da) SHA1(44ea0a4b98623d3282f8bdafb0a068287a879f47), ROM_BIOS(25))
	ROM_REGION(52352, "efr_encoder_pcm", 0)
	ROMX_LOAD("spch_dos.inp", 0, 52352, CRC(36fb2ba9) SHA1(3d4abd66d2d033de929175f1d84e970f78007628), ROM_BIOS(10))
	ROMX_LOAD("spch_dos.inp", 0, 52352, CRC(36fb2ba9) SHA1(3d4abd66d2d033de929175f1d84e970f78007628), ROM_BIOS(22))
	ROM_REGION(80196, "efr_encoder_serial", 0)
	ROMX_LOAD("spch_dos.cod", 0, 80196, CRC(f4d761a3) SHA1(49bd709881e15e018632567bedabf6ed3b94c479), ROM_BIOS(10))
	ROMX_LOAD("spch_dos.cod", 0, 80196, CRC(f4d761a3) SHA1(49bd709881e15e018632567bedabf6ed3b94c479), ROM_BIOS(22))
	ROM_REGION(80522, "efr_reference_input", 0)
	ROMX_LOAD("spch_dos.dec", 0, 80522, CRC(cdaa4997) SHA1(e93d62943336e4327e7bab5905dee73a58b7ce4b), ROM_BIOS(9))
	ROMX_LOAD("spch_dos.dec", 0, 80522, CRC(cdaa4997) SHA1(e93d62943336e4327e7bab5905dee73a58b7ce4b), ROM_BIOS(23))
	ROM_REGION(52160, "efr_reference_pcm", 0)
	ROMX_LOAD("spch_dos.out", 0, 52160, CRC(bbfe268c) SHA1(d738d872bbb4bcf84bc68031166a8981003eea94), ROM_BIOS(9))
	ROMX_LOAD("spch_dos.out", 0, 52160, CRC(bbfe268c) SHA1(d738d872bbb4bcf84bc68031166a8981003eea94), ROM_BIOS(23))
	ROM_REGION16_LE(0x20000, "dspdrom", 0)
	ROM_LOAD16_WORD_SWAP("dsp_cold_data.bin", 0, 0x20000,
			CRC(c8111608) SHA1(024c7f970f4ef754d3e90471de48a167515f930d))
ROM_END

ROM_START(tms54bsp)
	ROM_SYSTEM_BIOS(0, "bsp", "Native backend serial peripherals")
	ROM_SYSTEM_BIOS(1, "requests", "Native backend slot request storage")
	ROM_REGION16_LE(0x20000, "test_program", ROMREGION_ERASE00)
	ROM_REGION16_LE(0x20000, "test_data", ROMREGION_ERASE00)
ROM_END

} // anonymous namespace

SYST(2026, tms54test, 0, 0, test, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x core conformance tests",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, tms54xtra, 0, 0, test, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x extended conformance profiles",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, tms54bsp, 0, 0, test, 0, c54x_backend_bsp_test_state, empty_init,
		"MAME", "C54x native backend completed-frame fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nsm3verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NSM-3 stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nsm3dverify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NSM-3D stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, npe3verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NPE-3 stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nhm5verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "3310 own stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nhm3verify, 0, 0, verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "6250 stock staged DSP verifier fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, nse5verify, 0, 0, nse5_verifier, 0, nsm3_verifier_state, empty_init,
		"MAME", "NSE-5 stock staged DSP verifier with ROM4 CRC routines",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
SYST(2026, tms54rom4, 0, 0, rom4, 0, tms320c54x_test_state, empty_init,
		"MAME", "TMS320C54x ROM4 private execution fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
