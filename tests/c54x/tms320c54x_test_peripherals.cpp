// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

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
