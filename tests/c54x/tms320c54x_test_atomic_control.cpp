// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

// Extended program flow, RTN and pending-transfer restoration sequence.
bool tms320c54x_test_state::check_atomic_control_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 784)
	{
		expect(false, "far save checkpoint operand was not executed");
	}
	if (m_phase == 785)
	{
		const unsigned checkpoint = m_far_transfer.returning ? 0x030602 : 0x010603;
		expect(m_cpu->state_int(STATE_GENPC) == checkpoint &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (m_far_transfer.returning ? 0x0400 : 0x03fe),
			"far save checkpoint retires exactly one delay word on the original page");
		expect(capture_snapshot(machine(), m_far_transfer.saved) == STATERR_NONE,
			"write pending far-transfer save state");
		return next_atomic_phase(786);
	}
	return false;
}

} // namespace c54x_test
