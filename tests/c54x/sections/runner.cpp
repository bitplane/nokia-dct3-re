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
	}
protected:
	void machine_start() override
	{
		m_timer = timer_alloc(FUNC(section_state::tick), this);
		save_item(NAME(m_irq_acks));
#ifdef C54X_SECTION_IRQ
		m_cpu->set_irq_acknowledge_callback([this](device_t &, int line) {
			if (line != 0 || m_cpu->pipeline_cycles() != 6)
				throw emu_fatalerror("Section IRQ ordering mismatch");
			++m_irq_acks; m_cpu->set_input_line(line, CLEAR_LINE); return 0;
		}, "section_irq");
#endif
		m_cases = c54x_test::section_cases();
		m_fixture = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
			m_cases.size(), [this](unsigned i) { return m_cases.at(i); });
#ifdef C54X_SECTION_OBSERVE_STORE
		m_fixture->set_data_observer([this](bool write, u32 at, u16 value) {
			if (!write) return;
			u16 const opcode = m_cpu->space(AS_PROGRAM).read_word(0x600);
			if ((opcode & 0xfe00) != 0x4a00 && at != 0x420) return;
			u16 destination = 0, observed = 0;
			if (!m_cpu->observe_execute_memory_store(0x600,opcode,destination,observed) || destination != at || observed != value ||
				m_cpu->observe_execute_memory_store(0x601,opcode,destination,observed) ||
				m_cpu->observe_execute_memory_store(0x600,opcode ^ 0x100,destination,observed))
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
		if (!m_fixture->step()) return;
#ifdef C54X_SECTION_IRQ
		if (m_irq_acks != m_cases.size()) throw emu_fatalerror("Section IRQ count mismatch");
#endif
		osd_printf_info("C54x section: PASS cases=%u restores=%u\n", m_fixture->count(), m_fixture->restores());
		throw emu_fatalerror(0, "C54x section complete");
	}
	required_device<tms320c54x_device> m_cpu;
	emu_timer *m_timer = nullptr;
	std::vector<c54x_test::clocked_register_case> m_cases;
	std::unique_ptr<c54x_test::register_sequence_fixture> m_fixture;
	u32 m_irq_acks = 0;
};
ROM_START(tms54isa)
ROM_END
}
SYST(2026, tms54isa, 0, 0, test, 0, section_state, empty_init, "Test", "C54x selected section", MACHINE_NO_SOUND_HW)
