// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {















u16 tms320c54x_test_state::repeat_irq_r(offs_t offset)
{
	if (m_phase == 6517 || m_phase == 6518 || m_phase == 251210 || m_phase == 62162 || m_phase == 62163)
		return m_stack_mmr[offset];
	if (offset) return 0;
	m_last_operand_cycle = m_cpu->total_cycles();
	if (++m_repeat_reads == 1)
	{
		m_first_operand_cycle = m_last_operand_cycle;
	}
	if (m_repeat_reads == m_irq_trigger_read)
		m_cpu->set_input_line(2, ASSERT_LINE);
	return 1;
}







} // namespace c54x_test
