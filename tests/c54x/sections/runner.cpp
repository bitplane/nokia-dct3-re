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
		m_cpu->xf_out_cb().set([this](int value) { m_xf = bool(value); });
	}
protected:
	void machine_start() override
	{
		m_timer = timer_alloc(FUNC(section_state::tick), this);
		save_item(NAME(m_irq_acks));
		save_item(NAME(m_xf));
		save_item(NAME(m_last_irq_case));
		m_cpu->set_irq_acknowledge_callback([this](device_t &, int line) {
			unsigned const index = m_fixture->count();
			auto const &expected = m_cases.at(index).irq;
			u64 const cycle = m_cpu->pipeline_cycles();
			if (!expected || line != expected->line || cycle < expected->earliest || cycle > expected->latest ||
				m_last_irq_case == index || m_fixture->data_events_seen() != expected->data_events_before ||
				(expected->return_pc && m_cpu->state_int(tms320c54x_device::STATE_RTN) != *expected->return_pc))
				throw emu_fatalerror("Section IRQ case=%u line=%d clock=%llu data_events=%u previous_case=%u",
					index,line,(unsigned long long)cycle,m_fixture->data_events_seen(),m_last_irq_case);
			m_last_irq_case = index; ++m_irq_acks; m_cpu->set_input_line(line, CLEAR_LINE); return 0;
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
	void program(address_map &map) { map(0, 0x3ffff).ram(); }
	void data(address_map &map) { map(0, 0xffff).ram(); }
	TIMER_CALLBACK_MEMBER(tick)
	{
		bool const complete = m_fixture->step();
#ifdef C54X_SECTION_XF
		if (m_xf != bool(BIT(m_cpu->state_int(tms320c54x_device::STATE_ST1),13)))
			throw emu_fatalerror("Section XF output mismatch");
#endif
		if (!complete) return;
		unsigned expected_irqs = 0;
		for (auto const &input : m_cases) expected_irqs += bool(input.irq);
		if (m_irq_acks != expected_irqs) throw emu_fatalerror("Section IRQ count mismatch");
		osd_printf_info("C54x section: PASS cases=%u restores=%u\n", m_fixture->count(), m_fixture->restores());
		throw emu_fatalerror(0, "C54x section complete");
	}
	required_device<tms320c54x_device> m_cpu;
	emu_timer *m_timer = nullptr;
	std::vector<c54x_test::clocked_register_case> m_cases;
	std::unique_ptr<c54x_test::register_sequence_fixture> m_fixture;
	u32 m_irq_acks = 0;
	u32 m_last_irq_case = ~u32(0);
	bool m_xf = false;
};
ROM_START(tms54isa)
ROM_END
}
SYST(2026, tms54isa, 0, 0, test, 0, section_state, empty_init, "Test", "C54x selected section", MACHINE_NO_SOUND_HW)
