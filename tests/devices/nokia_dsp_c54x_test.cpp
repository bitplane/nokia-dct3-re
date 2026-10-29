// license:BSD-3-Clause
// copyright-holders:Gaz

// Synthetic Nokia backend integration, not CPU-only ISA conformance.

#include "emu.h"
#include "emuopts.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include "nokia_ccont.h"
#include "nokia_cobba.h"
#include "nokia_dsp_c54x.h"
#include "nokia_dspif.h"
#include "tms320c54x_test_snapshot.h"
#include <sstream>

namespace {

using c54x_test::capture_snapshot;

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
			expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "save external serial clock levels");
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
			expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "save partial XSR, queued BDXR and pending TX interrupt");
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
			expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "save continuous receive mode and partial successor");
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
			expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "save continuous TX partial word and queued successor");
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
			expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "save FIG streams after initial sync release");
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
			expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "Save both joined shifters and clock levels mid-word");
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
			expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "save backend with half a serial word received");
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
		expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "save RF observation latch and independent source cursor");
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
			expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "Save the pending slot and its independent payload");
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
			expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "Save the armed timer before expiry");
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
			m_timer->adjust(attotime::never); expect(capture_snapshot(machine(), m_saved) == STATERR_NONE, "Save both pending comparators and their mask");
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

ROM_START(tms54bsp)
	ROM_SYSTEM_BIOS(0, "bsp", "Native backend serial peripherals")
	ROM_SYSTEM_BIOS(1, "requests", "Native backend slot request storage")
	ROM_REGION16_LE(0x20000, "test_program", ROMREGION_ERASE00)
	ROM_REGION16_LE(0x20000, "test_data", ROMREGION_ERASE00)
ROM_END

} // anonymous namespace

SYST(2026, tms54bsp, 0, 0, test, 0, c54x_backend_bsp_test_state, empty_init,
		"MAME", "C54x native backend completed-frame fixture",
		MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
