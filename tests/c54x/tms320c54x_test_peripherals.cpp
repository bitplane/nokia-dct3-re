// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::check_mmr_bus()
{
	auto &data = m_cpu->space(AS_DATA);
	u16 const ar = m_cpu->state_int(tms320c54x_device::STATE_AR3);
	u16 const flags = m_cpu->state_int(tms320c54x_device::STATE_IFR);
	u16 const gap = data.read_word(3);
	unsigned observed = 0;
	auto tap = data.install_write_tap(0x13, 0x13, "mmr_bus_check",
		[&observed](offs_t, u16 &, u16) { ++observed; });
	data.write_word(0x13, 0x1234);
	expect(m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1234 && data.read_word(0x13) == 0x1234,
		"Mapped AR3 and CPU state share one register");
	data.write_word(0x13, 0xab00, 0xff00); data.write_word(0x13, 0x00cd, 0x00ff);
	expect(data.read_word(0x13) == 0xabcd && observed == 3, "MMR partial writes preserve unselected bits and reach passive taps");
	tap.remove();
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0xbeef);
	data.write_word(1, 0x00ef, 0x00ff);
	expect(data.read_word(1) == 0xbe00, "IFR byte write clears only selected low flags");
	data.write_word(1, 0x0e00, 0xff00);
	expect(data.read_word(1) == 0xb000, "IFR byte write clears only selected high flags");
	data.write_word(3, 0x5a5a);
	expect(data.read_word(3) == 0x5a5a, "Reserved CPU register gap stays in the board map");
	data.write_word(3, gap);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, ar);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, flags);
	osd_printf_info("TMS320C54x MMR bus: PASS unified_state=1 partial_write=1 ifr_clear=1 passive_tap=1 reserved_gap=1\n");
}



u16 tms320c54x_test_state::test_port_r()
{
	const u64 cycle = m_cpu->total_cycles();
	if (m_port_reads++ == 0)
		m_first_port_cycle = cycle;
	else
		m_last_port_cycle = cycle;
	return 0xabcd;
}

void tms320c54x_test_state::test_port_w(u16 value)
{
	const u64 cycle = m_cpu->total_cycles();
	if (m_port_writes++ == 0)
	{
		m_first_port_cycle = cycle;
		m_first_port_value = value;
	}
	else
	{
		if (m_port_writes == 2)
		{
			m_middle_port_value = value;
			m_middle_port_cycle = cycle;
		}
		m_last_port_cycle = cycle;
		m_last_port_value = value;
	}
}

} // namespace c54x_test
