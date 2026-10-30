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
	if (m_phase == 783)
	{
		program.write_word(0x020680, 0xf074); // CALL 0700, same extended page.
		program.write_word(0x020681, 0x0700);
		program.write_word(0x020682, 0xf4e1);
		program.write_word(0x020700, 0xf4e1); // Pause before inspecting RTN via RETF.
		m_cpu->set_state_int(STATE_GENPC, 0x020680);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(62050);
	}
	if (m_phase == 62050)
	{
		expect(m_cpu->state_int(STATE_GENPC) == 0x020701 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x03ff &&
			data.read_word(0x03ff) == 0x0682,
			"near CALL reaches its callee and stacks the continuation before fast return");
		data.write_word(0x03ff, 0xdead);
		program.write_word(0x020701, 0xf49b); // RETF must use RTN, not the sentinel.
		expect(capture_snapshot(machine(), m_near_return_saved) == STATERR_NONE,
			"save CALL-published RTN before fast return");
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(62051);
	}
	if (m_phase == 62051)
	{
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(STATE_GENPC) == 0x020683 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			data.read_word(0x03ff) == 0xdead,
			"SPRU131G CALL publishes RTN independently of its return-stack word");
		program.write_word(0x030780, 0xf074);
		program.write_word(0x030781, 0x0700);
		program.write_word(0x030700, 0xf4e1);
		m_cpu->set_state_int(STATE_GENPC, 0x030780);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(62052);
	}
	if (m_phase == 62052)
	{
		expect(m_cpu->state_int(STATE_GENPC) == 0x030701 &&
			data.read_word(0x03ff) == 0x0782,
			"second real CALL replaces the RTN checkpoint before restoration");
		expect(restore_snapshot(machine(), m_near_return_saved) == STATERR_NONE,
			"restore CALL-published RTN checkpoint");
		expect(m_cpu->state_int(STATE_GENPC) == 0x020701 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x03ff &&
			data.read_word(0x03ff) == 0xdead,
			"restore paused callee, page and independent stack sentinel");
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(62053);
	}
	if (m_phase == 62053)
	{
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(STATE_GENPC) == 0x020683 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			data.read_word(0x03ff) == 0xdead,
			"restored CALL RTN fast-returns identically after another call overwrites it");
		start_far_save_case(false);
		return true;
	}
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
