// license:BSD-3-Clause
// copyright-holders:Gaz
#include "emu.h"
#include "tms320c54x_test_register_sequence.h"
#include "c54x_section_selected.h"

namespace {
class section_state : public driver_device
{
public:
	section_state(machine_config const &c, device_type t, char const *tag)
		: driver_device(c, t, tag), m_cpu(*this, "cpu") { }
	void test(machine_config &c)
	{
		TMS320C54X(c, m_cpu, 13'000'000);
		m_cpu->set_extended_program(true);
		m_cpu->set_addrmap(AS_PROGRAM, &section_state::program);
		m_cpu->set_addrmap(AS_DATA, &section_state::data);
		m_cpu->set_addrmap(AS_IO, &section_state::data);
		m_cpu->xf_out_cb().set([this](int value) {
			m_xf = bool(value);
			if (m_fixture) m_fixture->observe_xf(m_xf);
		});
	}
protected:
	void machine_start() override
	{
		m_timer = timer_alloc(FUNC(section_state::tick), this);
		save_item(NAME(m_irq_acks));
		save_item(NAME(m_last_irq_case));
		save_item(NAME(m_case_irq_cursor));
		m_cpu->set_irq_acknowledge_callback([this](device_t &, int line) {
			unsigned const index = m_fixture->count();
			if (m_last_irq_case != index) { m_last_irq_case = index; m_case_irq_cursor = 0; }
			auto const &sequence = m_cases.at(index).irqs;
			if (m_case_irq_cursor >= sequence.size()) throw emu_fatalerror("Unexpected section IRQ case=%u ordinal=%u",index,m_case_irq_cursor);
			auto const &expected = sequence[m_case_irq_cursor];
			u64 const cycle = m_cpu->pipeline_cycles();
			if (line != expected.line || cycle < expected.earliest || cycle > expected.latest ||
				m_fixture->data_events_seen() != expected.data_events_before ||
				(expected.return_pc && m_cpu->state_int(tms320c54x_device::STATE_RTN) != *expected.return_pc))
				throw emu_fatalerror("Section IRQ case=%u line=%d clock=%llu data_events=%u previous_case=%u return_pc=%04x",
					index,line,(unsigned long long)cycle,m_fixture->data_events_seen(),m_last_irq_case,
					unsigned(m_cpu->state_int(tms320c54x_device::STATE_RTN)));
			for (auto const &reg : expected.registers)
				if ((m_cpu->state_int(reg.index) & reg.mask) != (reg.value & reg.mask))
					throw emu_fatalerror("Section IRQ case=%u register=%d value=%llx expected=%llx",index,reg.index,
						(unsigned long long)m_cpu->state_int(reg.index),(unsigned long long)reg.value);
			++m_case_irq_cursor; ++m_irq_acks; m_cpu->set_input_line(line, CLEAR_LINE); return 0;
		}, "section_irq");
		m_cases = c54x_test::section_cases();
		m_fixture = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
			m_cases.size(), [this](unsigned i) { return m_cases.at(i); });
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
	void program(address_map &map) {
		map(0, 0x3ffff).ram();
#ifdef C54X_SECTION_HIGH_PROGRAM
		map(0x7f0000, 0x7fffff).ram();
#endif
	}
	void data(address_map &map) { map(0, 0xffff).ram(); }
	TIMER_CALLBACK_MEMBER(tick)
	{
		bool const complete = m_fixture->step();
#ifdef C54X_SECTION_XF
		if (m_xf != bool(BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1),13)))
			throw emu_fatalerror("Section XF output mismatch");
#endif
		if (!complete) return;
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
	emu_timer *m_timer = nullptr;
	std::vector<c54x_test::clocked_register_case> m_cases;
	std::unique_ptr<c54x_test::register_sequence_fixture> m_fixture;
	u32 m_irq_acks = 0;
	u32 m_last_irq_case = ~u32(0);
	u32 m_case_irq_cursor = 0;
	bool m_xf = false;
};
ROM_START(tms54isa)
ROM_END
}
SYST(2026, tms54isa, 0, 0, test, 0, section_state, empty_init, "Test", "C54x selected section", MACHINE_NO_SOUND_HW)
