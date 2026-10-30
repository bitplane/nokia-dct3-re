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
	if (m_phase == 780)
	{
		// READA/WRITA use A's extended address, not the executing page.
		program.write_word(0x010600, 0x7ff8); // WRITA *(0500)
		program.write_word(0x010601, 0x0500);
		program.write_word(0x010602, 0x7ef8); // READA *(0501)
		program.write_word(0x010603, 0x0501);
		program.write_word(0x010604, 0xf4e1);
		data.write_word(0x0500, 0xbeef);
		data.write_word(0x0501, 0);
		program.write_word(0x020900, 0);
		program.write_word(0x010900, 0x1111);
		program.write_word(0x000900, 0x2222);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x020900);
		m_cpu->set_state_int(STATE_GENPC, 0x010600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(781);
	}
	if (m_phase == 781)
	{
		expect_opcode(0x7ff8,
			data.read_word(0x0501) == 0xbeef &&
			program.read_word(0x020900) == 0xbeef &&
			program.read_word(0x010900) == 0x1111 && program.read_word(0x000900) == 0x2222,
			"READA/WRITA address extended program memory independently of XPC");
		program.write_word(0x010148, 0x4a1e); // ISR saves XPC explicitly.
		program.write_word(0x010149, 0xf883); // FB 03:0700
		program.write_word(0x01014a, 0x0700);
		program.write_word(0x030700, 0xe809);
		program.write_word(0x030701, 0xf4e5); // FRETE restores saved XPC/PC.
		program.write_word(0x010605, 0xf4e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x0100);
		m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 4);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
		m_cpu->set_input_line(2, ASSERT_LINE);
		return next_atomic_phase(782);
	}
	if (m_phase == 782)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 9 &&
			m_cpu->state_int(STATE_GENPC) == 0x010606 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			data.read_word(0x03fe) == 1 && data.read_word(0x03ff) == 0x0605 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
			"interrupt retains XPC and pushes only PC; explicit ISR XPC save allows a far return");
		m_cpu->set_input_line(2, CLEAR_LINE);
		m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
		write_words(program, {
			{0x020600, 0xf074}, // Near CALL on page 2.
			{0x020601, 0x0700}, {0x020602, 0xf4e1}, {0x020700, 0xe803}, {0x020701, 0xfc00},
		});
		m_cpu->set_state_int(STATE_GENPC, 0x020600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(783);
	}
	if (m_phase == 783)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
			m_cpu->state_int(STATE_GENPC) == 0x020603 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			data.read_word(0x03ff) == 0x0602,
			"near CALL/RET retain the current page and use one return-stack word");
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
