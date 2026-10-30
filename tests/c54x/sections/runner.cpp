// license:BSD-3-Clause
// copyright-holders:Gaz
#include "emu.h"
#include "tms320c54x_test_register_sequence.h"
#include "c54x_section_selected.h"
#if defined(C54X_SECTION_BSP_WORD_SOURCE) || defined(C54X_SECTION_BSP_INTERRUPT)
#include "tms320c54x_bsp_state.h"
#endif
#if defined(C54X_SECTION_MCBSP_CONTROL) || defined(C54X_SECTION_DEEP_IDLE_CLOCKS)
#include "tms320c54x_mcbsp.h"
#endif

namespace {
class section_state : public driver_device
{
public:
	section_state(machine_config const &c, device_type t, char const *tag)
		: driver_device(c, t, tag), m_cpu(*this, "cpu")
#if defined(C54X_SECTION_MCBSP_CONTROL) || defined(C54X_SECTION_DEEP_IDLE_CLOCKS)
		, m_mcbsp(*this, "mcbsp")
#endif
	{ }
	void test(machine_config &c)
	{
		TMS320C54X(c, m_cpu, 13'000'000);
		m_cpu->set_extended_program(true);
#ifdef C54X_SECTION_COUNT_RETIREMENTS
		m_cpu->instruction_observer_cb().set([this](offs_t pc,u16 opcode) {
			auto const &expected=*m_cases.at(m_fixture->count()).retired_instruction;
			if (pc==expected.address && expected.first_cycle && m_cpu->pipeline_cycles()!=expected.first_cycle+m_retired_count)
				throw emu_fatalerror("Repeated instruction does not retire at its expected one-clock cadence");
			if (pc==expected.address && (opcode!=expected.opcode || ++m_retired_count>expected.count))
				throw emu_fatalerror("Repeated instruction retirement identity/count mismatch");
		});
#endif
#ifdef C54X_SECTION_OBSERVE_MEMORY_MULTIPLY
		m_cpu->instruction_observer_cb().set([this](offs_t pc,u16 opcode) {
			auto const &input=m_cases.at(m_fixture->count());
			auto const &words=input.states.front().observation.program;
			auto const &operand=input.data_events->front();
			u16 source=0xdead,word=0xbeef,literal=0xcafe;
			bool const retained=m_cpu->observe_execute_memory_multiply(pc,opcode,source,word,literal);
			if (pc==words.front().address) {
				if (!retained || source!=operand.address || word!=operand.value || literal!=input.retained_program_reads.back().value)
					throw emu_fatalerror("Section multiply observer loses captured operand/literal");
			} else if (retained || source!=0xdead || word!=0xbeef || literal!=0xcafe)
				throw emu_fatalerror("Section non-multiply observer changes outputs");
			if (m_cpu->observe_execute_memory_multiply(pc^1,opcode,source,word,literal))
				throw emu_fatalerror("Section multiply observer accepts wrong PC");
		});
#endif
#ifdef C54X_SECTION_C5416_BUS
		m_cpu->set_c5416_bus_profile(true);
#endif
#ifdef C54X_SECTION_STANDARD_SWWSR
		m_cpu->set_standard_swwsr(true);
#endif
		m_cpu->set_addrmap(AS_PROGRAM, &section_state::program);
		m_cpu->set_addrmap(AS_DATA, &section_state::data);
		m_cpu->set_addrmap(AS_IO, &section_state::data);
#ifdef C54X_SECTION_MCBSP_CONTROL
		TMS320C54X_MCBSP(c, m_mcbsp, 13'000'000);
		m_mcbsp->tx_word_cb().set([](u16) { throw emu_fatalerror("McBSP control fixture must reset before transmitting a word"); });
#endif
#ifdef C54X_SECTION_DEEP_IDLE_CLOCKS
		TMS320C54X_MCBSP(c, m_mcbsp, 13'000'000);
		m_cpu->peripheral_clock_stop_cb().set(m_mcbsp,FUNC(tms320c54x_mcbsp_device::internal_clock_stop_w));
		m_mcbsp->tx_word_cb().set([this](u16 value) {
			if (value!=0xa55a || ++m_serial_words!=1) throw emu_fatalerror("Deep-idle serial word/count mismatch");
		});
		m_mcbsp->tx_irq_cb().set([this](int state) {
			if (m_serial_external) m_cpu->set_input_line(5,state);
		});
#endif
		m_cpu->bio_in_cb().set([this]() { return m_fixture ? m_fixture->sample_bio() : 1; });
		m_cpu->xf_out_cb().set([this](int value) {
			m_xf = bool(value);
			if (m_fixture) m_fixture->observe_xf(m_xf);
		});
	}
protected:
	void machine_start() override
	{
		m_timer = timer_alloc(FUNC(section_state::tick), this);
#ifdef C54X_SECTION_DEEP_IDLE_CLOCKS
		save_item(NAME(m_serial_words)); save_item(NAME(m_frozen_tim)); save_item(NAME(m_frozen_tcr));
		save_item(NAME(m_serial_external));
#endif
#ifdef C54X_SECTION_COUNT_RETIREMENTS
		save_item(NAME(m_retired_count));
#endif
#ifdef C54X_SECTION_BSP_WORD_SOURCE
		// Completed-word RX only; no serial-pin state changes in this attachment.
		save_item(NAME(m_bsp.control)); save_item(NAME(m_bsp.receive));
		save_item(NAME(m_bsp.receive_ready)); save_item(NAME(m_bsp.receive_overrun));
#endif
		save_item(NAME(m_irq_acks));
		save_item(NAME(m_last_irq_case));
		save_item(NAME(m_case_irq_cursor));
		m_cpu->set_irq_acknowledge_callback([this](device_t &, int line) {
			unsigned const index = m_fixture->count();
#ifdef C54X_SECTION_COUNT_RETIREMENTS
			check_retirements(index);
#endif
			if (m_last_irq_case != index) { m_last_irq_case = index; m_case_irq_cursor = 0; }
			auto const &sequence = m_cases.at(index).irqs;
			if (m_case_irq_cursor >= sequence.size()) throw emu_fatalerror("Unexpected section IRQ case=%u ordinal=%u",index,m_case_irq_cursor);
			auto const &expected = sequence[m_case_irq_cursor];
			u64 const cycle = m_cpu->pipeline_cycles();
			if (line != expected.line || cycle < expected.earliest || cycle > expected.latest ||
				m_fixture->data_events_seen() != expected.data_events_before ||
				(expected.io_events_before && m_fixture->io_events_seen() != *expected.io_events_before) ||
				(expected.return_pc && m_cpu->state_int(tms320c54x_device::STATE_RTN) != *expected.return_pc))
				throw emu_fatalerror("Section IRQ case=%u line=%d clock=%llu data_events=%u previous_case=%u return_pc=%04x",
					index,line,(unsigned long long)cycle,m_fixture->data_events_seen(),m_last_irq_case,
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_RTN)));
			for (auto const &reg : expected.registers)
				if ((m_cpu->state_int(reg.index) & reg.mask) != (reg.value & reg.mask))
					throw emu_fatalerror("Section IRQ case=%u register=%d value=%llx expected=%llx",index,reg.index,
						(unsigned long long)m_cpu->state_int(reg.index),(unsigned long long)reg.value);
			++m_case_irq_cursor; ++m_irq_acks;
			if (expected.clear_input_on_ack) m_cpu->set_input_line(line, CLEAR_LINE);
			return 0;
		}, "section_irq");
		m_cases = c54x_test::section_cases();
		m_fixture = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
			m_cases.size(), [this](unsigned i) {
#ifdef C54X_SECTION_COUNT_RETIREMENTS
				if (i) check_retirements(i-1);
				m_retired_count=0;
				auto const &states=m_cases.at(i).retired_accumulator_states;
				if (!states.empty() && states.size()!=m_cases.at(i).retired_instruction->count+1)
					throw emu_fatalerror("Retirement state sequence must include initial state and every body result");
#endif
#ifdef C54X_SECTION_MCBSP_CONTROL
				m_mcbsp->reset();
				m_mcbsp->control_w(0,1); m_mcbsp->control_w(1,1); m_mcbsp->control_w(0,5);
#endif
#ifdef C54X_SECTION_DEEP_IDLE_CLOCKS
				m_serial_external=false; m_mcbsp->reset(); m_serial_words=0;
				auto write=[this](u16 reg,u16 value) { m_mcbsp->control_w(0,reg); m_mcbsp->control_w(1,value); };
				write(4,0x40);
				if (i>=4) { write(5,1); write(1,1); }
				else { write(6,12); write(7,0x2000); write(14,0x200); write(1,0x41); }
				m_mcbsp->data_w(3,0xa55a);
				m_serial_external=i>=4;
#endif
#ifdef C54X_SECTION_BSP_WORD_SOURCE
				if (i) check_bsp_frame(i-1);
				m_bsp.reset(); m_bsp.control_w(0x80);
				u16 const word=m_cases.at(i).data_events->front().value;
				if (!m_bsp.frame_receive(word) || m_bsp.frame_receive(word^0xffff) || !m_bsp.receive_overrun)
					throw emu_fatalerror("BSP fixture must start with an unread frame and overrun");
#endif
				return m_cases.at(i);
			});
#ifdef C54X_SECTION_MCBSP_CONTROL
		m_fixture->set_data_observer([this](bool write,u32 at,u16) {
			if (!write && at==0x39) {
				m_mcbsp->data_w(3,0xa55a);
				if (m_mcbsp->control_r(1)!=1) throw emu_fatalerror("Real DXR write must make captured XRDY stale");
			}
		});
#endif
#ifdef C54X_SECTION_OBSERVE_MVDD
		m_fixture->set_data_observer([this](bool write,u32 at,u16) {
			auto const &input=m_cases.at(m_fixture->count());
			auto const &instruction=input.states.front().observation.program.front();
			u32 pc=0; u16 opcode=0,source=0;
			bool valid=write ? m_cpu->observe_staged_mvdd_write(at,pc,opcode,source) : m_cpu->observe_staged_data_read(at,pc,opcode);
			if (!valid || pc!=instruction.address || opcode!=instruction.value || (write && source!=input.data_events->front().address))
				throw emu_fatalerror("Section MVDD observer loses logical instruction or source identity");
			if (write && m_cpu->observe_staged_data_read(input.data_events->front().address,pc,opcode))
				throw emu_fatalerror("Section MVDD read identity outlives its Read");
			if (write ? m_cpu->observe_staged_mvdd_write(at^1,pc,opcode,source) : m_cpu->observe_staged_data_read(at^1,pc,opcode))
				throw emu_fatalerror("Section MVDD observer accepts wrong bus address");
			auto const disable=machine().disable_side_effects();
			if (write ? m_cpu->observe_staged_mvdd_write(at,pc,opcode,source) : m_cpu->observe_staged_data_read(at,pc,opcode))
				throw emu_fatalerror("Section MVDD observer accepts debugger peek");
		});
#endif
#ifdef C54X_SECTION_OBSERVE_STORE
		m_fixture->set_data_observer([this](bool write, u32 at, u16 value) {
			if (!write) return;
			auto const &input = m_cases.at(m_fixture->count()).states.front().observation;
			u16 const opcode = input.opcode;
			u32 const pc = input.program.front().address;
			if ((opcode & 0xfe00) != 0x4a00 && at != 0x420) return;
			u16 destination = 0, observed = 0;
			if (!m_cpu->observe_execute_memory_store(pc,opcode,destination,observed) || destination != at || observed != value ||
				m_cpu->observe_execute_memory_store(pc+1,opcode,destination,observed) ||
				m_cpu->observe_execute_memory_store(pc,opcode ^ 0x100,destination,observed))
				throw emu_fatalerror("Section store observation identity mismatch");
		});
#endif
	}
	void machine_reset() override { m_fixture->start(); }
private:
#ifdef C54X_SECTION_COUNT_RETIREMENTS
	void check_retirements(unsigned index) {
		if (m_retired_count!=m_cases.at(index).retired_instruction->count)
			throw emu_fatalerror("Repeated instruction count case=%u actual=%u expected=%u",index,m_retired_count,
				m_cases.at(index).retired_instruction->count);
	}
	void check_retirement_state(unsigned index) {
		auto const &body=*m_cases.at(index).retired_instruction;
		if (body.first_cycle) {
			unsigned const remaining=m_cpu->pipeline_cycles()<body.first_cycle-1 ? 0 : m_retired_count ? body.count-m_retired_count : body.count-1;
			if (m_cpu->state_int(tms320c54x_device::STATE_RC)!=remaining)
				throw emu_fatalerror("Repeated instruction RC does not match its retired-body count");
		}
		auto const &states=m_cases.at(index).retired_accumulator_states;
		if (states.empty()) return;
		if (m_retired_count>=states.size()) throw emu_fatalerror("Retirement state outside expected sequence");
		auto const &expected=states[m_retired_count];
		static constexpr int registers[]{tms320c54x_device::STATE_A,tms320c54x_device::STATE_B,tms320c54x_device::STATE_ST0};
		for (unsigned i=0;i<3;++i) {
			u64 const actual=m_cpu->state_int(registers[i]);
			if (actual!=expected[i])
				throw emu_fatalerror("Retirement state case=%u body=%u field=%u actual=%llx expected=%llx",index,m_retired_count,i,
					(unsigned long long)actual,(unsigned long long)expected[i]);
		}
	}
#endif
	void program(address_map &map) {
		map(0, 0x3ffff).ram();
#ifdef C54X_SECTION_HIGH_PROGRAM
		map(0x7f0000, 0x7fffff).ram();
#endif
	}
	void data(address_map &map) {
		map(0, 0xffff).ram();
#ifdef C54X_SECTION_MCBSP_CONTROL
		map(0x38,0x39).rw(m_mcbsp,FUNC(tms320c54x_mcbsp_device::control_r),FUNC(tms320c54x_mcbsp_device::control_w));
#endif
#ifdef C54X_SECTION_BSP_WORD_SOURCE
		map(0x20,0x20).r(FUNC(section_state::bsp_read));
#endif
	}
#ifdef C54X_SECTION_BSP_WORD_SOURCE
	u16 bsp_read()
	{
		if (machine().side_effects_disabled()) return m_bsp.receive;
		if (!m_bsp.receive_ready || !m_bsp.receive_overrun)
			throw emu_fatalerror("BSP read must consume the unread overrun frame");
		u16 const word=m_bsp.receive_r();
		if (m_bsp.receive_ready || m_bsp.receive_overrun || !m_bsp.frame_receive(word^0xffff))
			throw emu_fatalerror("BSP acknowledgement must admit the next frame");
		return word;
	}
	void check_bsp_frame(unsigned i)
	{
		u16 const expected=m_cases.at(i).data_events->front().value^0xffff;
		if (!m_bsp.receive_ready || m_bsp.receive_overrun || m_bsp.receive!=expected)
			throw emu_fatalerror("BSP next frame must remain unread through completion and replay");
	}
#endif
	TIMER_CALLBACK_MEMBER(tick)
	{
#ifdef C54X_SECTION_BSP_INTERRUPT
		if (m_fixture->tick_count()==16) {
			unsigned const index=m_fixture->count(), source=index<4 ? 4 : 5;
			tms320c54x_bsp_state port;
			port.control_w(index==8 ? 0xc0 : source==4 ? 0x80 : 0x40);
			if (source==5) port.transmit_w(0x1234);
			u16 output=0;
			auto const frame=port.duplex_frame(0x5678,output);
			if (frame.transmit_ready_edge!=(source==5) || frame.receive_ready_edge!=(source==4 || index==8) ||
				frame.transmitted!=(source==5) || (frame.transmitted && output!=0x1234))
				throw emu_fatalerror("BSP frame must produce only its enabled ready transitions");
			if (frame.transmit_ready_edge) m_cpu->set_input_line(5,HOLD_LINE);
			if (frame.receive_ready_edge) m_cpu->set_input_line(4,HOLD_LINE);
		}
#endif
#ifdef C54X_SECTION_DEEP_IDLE_CLOCKS
		unsigned const now=m_fixture->tick_count(), index=m_fixture->count();
		if (now==100) {
			auto const disable=machine().disable_side_effects();
			m_frozen_tim=m_cpu->state_int(tms320c54x_device::STATE_TIM);
			m_frozen_tcr=m_cpu->state_int(tms320c54x_device::STATE_TCR);
		}
		if (now==1300 || now==2599) {
			{
				auto const disable=machine().disable_side_effects();
				if (m_serial_words || (index<4 && (m_frozen_tim>=100 ||
					m_cpu->state_int(tms320c54x_device::STATE_TIM)!=m_frozen_tim ||
					m_cpu->state_int(tms320c54x_device::STATE_TCR)!=m_frozen_tcr || (m_frozen_tcr&0x10))))
					throw emu_fatalerror("Deep-idle clock case=%u tick=%u words=%u TIM=%llu/%u TCR=%llu/%u",index,now,m_serial_words,
						(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_TIM),m_frozen_tim,
						(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_TCR),m_frozen_tcr);
			}
		}
		if (now==2600 && index>=4) {
			m_mcbsp->tx_frame_w(1);
			for (unsigned bit=0;bit<16;++bit) { m_mcbsp->tx_clock_w(1); m_mcbsp->tx_clock_w(0); }
		}
		if (now==3899 && m_serial_words!=unsigned(index>=4 || (index&1)))
			throw emu_fatalerror("Deep-idle serial clock did not complete exactly the eligible word");
#endif
		bool const complete = m_fixture->step();
#ifdef C54X_SECTION_COUNT_RETIREMENTS
		check_retirement_state(complete ? m_cases.size()-1 : m_fixture->count());
#endif
#ifdef C54X_SECTION_XF
		if (m_xf != bool(BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1),13)))
			throw emu_fatalerror("Section XF output mismatch");
#endif
		if (!complete) return;
#ifdef C54X_SECTION_COUNT_RETIREMENTS
		check_retirements(m_cases.size()-1);
#endif
#ifdef C54X_SECTION_BSP_WORD_SOURCE
		check_bsp_frame(m_cases.size()-1);
#endif
#ifdef C54X_SECTION_XF_RESTORE
		// The external sink is deliberately unsaved: post-load must re-drive it.
		std::stringstream saved;
		if (c54x_test::capture_snapshot(machine(),saved) != STATERR_NONE) throw emu_fatalerror("Capture XF restore probe");
		u16 const st1 = m_cpu->state_int(tms320c54x_device::STATE_ST1);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1,st1 ^ 0x2000);
		if (m_xf != !bool(BIT(st1,13))) throw emu_fatalerror("Debugger import must drive XF immediately");
		if (c54x_test::restore_snapshot(machine(),saved) != STATERR_NONE || m_xf != bool(BIT(st1,13)))
			throw emu_fatalerror("Post-load must re-drive the unsaved XF sink");
#endif
		unsigned expected_irqs = 0;
		for (auto const &input : m_cases) expected_irqs += input.irqs.size();
		if (m_irq_acks != expected_irqs) throw emu_fatalerror("Section IRQ count mismatch: actual=%u expected=%u",m_irq_acks,expected_irqs);
		osd_printf_info("C54x section: PASS cases=%u restores=%u\n", m_fixture->count(), m_fixture->restores());
		throw emu_fatalerror(0, "C54x section complete");
	}
	required_device<tms320c54x_device> m_cpu;
#if defined(C54X_SECTION_MCBSP_CONTROL) || defined(C54X_SECTION_DEEP_IDLE_CLOCKS)
	required_device<tms320c54x_mcbsp_device> m_mcbsp;
#endif
#ifdef C54X_SECTION_DEEP_IDLE_CLOCKS
	u32 m_serial_words=0;
	u16 m_frozen_tim=0, m_frozen_tcr=0;
	bool m_serial_external=false;
#endif
	emu_timer *m_timer = nullptr;
	std::vector<c54x_test::clocked_register_case> m_cases;
	std::unique_ptr<c54x_test::register_sequence_fixture> m_fixture;
	u32 m_irq_acks = 0;
	u32 m_last_irq_case = ~u32(0);
	u32 m_case_irq_cursor = 0;
#ifdef C54X_SECTION_COUNT_RETIREMENTS
	u32 m_retired_count = 0;
#endif
	bool m_xf = false;
#ifdef C54X_SECTION_BSP_WORD_SOURCE
	tms320c54x_bsp_state m_bsp;
#endif
};
ROM_START(tms54isa)
ROM_END
}
SYST(2026, tms54isa, 0, 0, test, 0, section_state, empty_init, "Test", "C54x selected section", MACHINE_NO_SOUND_HW)
