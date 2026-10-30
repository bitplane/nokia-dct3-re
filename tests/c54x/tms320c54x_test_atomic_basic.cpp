// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

// Synthetic basic-operation sequence.
bool tms320c54x_test_state::check_atomic_basic_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 77)
	{
		switch (m_phase)
		{
		case 77:
			program.write_word(0x05e0, 0x74d6); // PORTR port, *AR6+%
			program.write_word(0x05e1, 0x0123);
			program.write_word(0x05e2, 0x74d6);
			program.write_word(0x05e3, 0x0123);
			program.write_word(0x05e4, 0xf5e1); // IDLE
			data.write_word(0x0a03, 0);
			data.write_word(0x0a00, 0);
			m_port_reads = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(78);
		}
	}
	return false;
}

} // namespace c54x_test
