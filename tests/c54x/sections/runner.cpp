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
		m_cases = c54x_test::section_cases();
		m_fixture = std::make_unique<c54x_test::register_sequence_fixture>(machine(), *m_cpu, *m_timer,
			m_cases.size(), [this](unsigned i) { return m_cases.at(i); });
	}
	void machine_reset() override { m_fixture->start(); }
private:
	void program(address_map &map) { map(0, 0x3ffff).ram(); }
	void data(address_map &map) { map(0, 0xffff).ram(); }
	TIMER_CALLBACK_MEMBER(tick)
	{
		if (!m_fixture->step()) return;
		osd_printf_info("C54x section: PASS cases=%u restores=%u\n", m_fixture->count(), m_fixture->restores());
		throw emu_fatalerror(0, "C54x section complete");
	}
	required_device<tms320c54x_device> m_cpu;
	emu_timer *m_timer = nullptr;
	std::vector<c54x_test::clocked_register_case> m_cases;
	std::unique_ptr<c54x_test::register_sequence_fixture> m_fixture;
};
ROM_START(tms54isa)
ROM_END
}
SYST(2026, tms54isa, 0, 0, test, 0, section_state, empty_init, "Test", "C54x selected section", MACHINE_NO_SOUND_HW)
