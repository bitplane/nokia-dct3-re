// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_near_flow.h"

namespace c54x_test {

// Extended program flow, RTN and pending-transfer restoration sequence.
bool tms320c54x_test_state::check_atomic_control_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if ((m_phase >= 750 && m_phase <= 757) || (m_phase >= 60040 && m_phase <= 60041))
	{
		unsigned const index = m_phase >= 60040 ? m_phase - 60038 : m_phase < 752 ? m_phase - 750 : m_phase - 748;
		auto const &test = near_flow_cases()[index];
		check_case(*m_cpu, test, m_port_writes, m_last_port_cycle - m_first_port_cycle);
		expect_opcode(test.opcode, true, "conditional delay/stack reference");
		if (index == 4) expect_opcode(0x8a63, true, "POPM high address bits");
		if (index + 1 < near_flow_cases().size())
		{
			unsigned const next = index + 1;
			m_port_writes = 0;
			prepare_case(*m_cpu, near_flow_cases()[next]);
			return next_atomic_phase(next < 2 ? 750 + next : next < 4 ? 60038 + next : 748 + next);
		}
		// Far-control fixtures use distinct pages, not mirrored low RAM.
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
		program.write_word(0x020600, 0xf983); // FCALL 03:0700
		program.write_word(0x020601, 0x0700);
		program.write_word(0x020602, 0xf4e1);
		program.write_word(0x030700, 0xe807);
		program.write_word(0x030701, 0xf4e4); // FRET
		set_registers(*m_cpu, {
			{STATE_GENPC, 0x020600}, {cpu_regs::STATE_SP, 0x0400}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0800}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(758);
	}
	if (m_phase == 758)
	{
		expect_opcode(0xf983,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 7 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			m_cpu->state_int(STATE_GENPC) == 0x020603 &&
			data.read_word(0x03fe) == 2 && data.read_word(0x03ff) == 0x0602,
			"FCALL/FRET preserve extended page and PC-then-XPC stack order");
		program.write_word(0x020600, 0xfb83); // FCALLD 03:0700
		program.write_word(0x020602, 0xe901);
		program.write_word(0x020603, 0xe902);
		program.write_word(0x020604, 0xf4e1);
		m_cpu->set_state_int(STATE_GENPC, 0x020600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(759);
	}
	if (m_phase == 759)
	{
		expect_opcode(0xfb83,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
			m_cpu->state_int(STATE_GENPC) == 0x020605 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			data.read_word(0x03ff) == 0x0604,
			"FCALLD executes two caller-page delay words and returns after them");
		program.write_word(0x020600, 0xf7e6); // FBACCD B
		program.write_word(0x020601, 0xf020); // One two-word delay instruction.
		program.write_word(0x020602, 0x1234);
		program.write_word(0x7f0800, 0xf4e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x12347f0800ULL);
		m_cpu->set_state_int(STATE_GENPC, 0x020600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(760);
	}
	if (m_phase == 760)
	{
		expect_opcode(0xf7e6,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(STATE_GENPC) == 0x7f0801 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400,
			"FBACCD masks to 23 bits and accepts a two-word caller-page delay instruction");
		program.write_word(0x7f0600, 0xf5e7); // FCALA B
		program.write_word(0x7f0601, 0xf4e1);
		program.write_word(0x020700, 0xe805);
		program.write_word(0x020701, 0xf4e4);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x020700);
		m_cpu->set_state_int(STATE_GENPC, 0x7f0600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(761);
	}
	if (m_phase == 761)
	{
		expect_opcode(0xf5e7,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 5 &&
			m_cpu->state_int(STATE_GENPC) == 0x7f0602 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			data.read_word(0x03fe) == 0x7f && data.read_word(0x03ff) == 0x0601,
			"FCALA B returns to the caller's highest supported program page");
		program.write_word(0x030700, 0xf6e5); // FRETED
		program.write_word(0x030701, 0xe901);
		program.write_word(0x030702, 0xe902);
		program.write_word(0x020800, 0xf4e1);
		data.write_word(0x03fe, 0x0082);
		data.write_word(0x03ff, 0x0800);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_SP, 0x03fe}, {STATE_GENPC, 0x030700}, {cpu_regs::STATE_ST1, 0x0800},
			{cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(762);
	}
	if (m_phase == 762)
	{
		expect_opcode(0xf6e5,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
			m_cpu->state_int(STATE_GENPC) == 0x020801 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
			"FRETED masks the stacked page, restores PC, executes callee-page delay words and enables interrupts");
		program.write_word(0x02ffff, 0xe803);
		program.write_word(0x020000, 0xf4e1);
		program.write_word(0x030000, 0xe809);
		m_cpu->set_state_int(STATE_GENPC, 0x02ffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(763);
	}
	if (m_phase == 763)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
			m_cpu->state_int(STATE_GENPC) == 0x020001,
			"sequential PC wrap does not increment XPC");
		program.write_word(0x020600, 0x771e); // STM #ffff, XPC
		program.write_word(0x020601, 0xffff);
		program.write_word(0x7f0602, 0x481e); // LDM XPC, A
		program.write_word(0x7f0603, 0xf4e1);
		m_cpu->set_state_int(STATE_GENPC, 0x020600);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(764);
	}
	if (m_phase == 764)
	{
		expect_opcode(0x771e,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7f &&
			m_cpu->state_int(STATE_GENPC) == 0x7f0604,
			"XPC memory-mapped writes mask to seven bits and affect the next fetch");
		start_far_case(0);
		return true;
	}
	if (m_phase >= 765 && m_phase < 765 + std::size(far_opcodes))
	{
		const unsigned index = m_phase - 765;
		const u16 op = far_opcodes[index];
		const bool immediate = (op & 0xfc80) == 0xf880;
		const bool returning = (op & 0xfdfe) == 0xf4e4;
		const bool call = !returning && (immediate ? BIT(op, 8) : BIT(op, 0));
		const unsigned continuation = 0x0600 + (immediate ? 2 : 1) + (BIT(op, 9) ? 2 : 0);
		expect_opcode(op,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 7 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (BIT(op, 9) ? 6 : 0x020700) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			m_cpu->state_int(STATE_GENPC) == (call ? 0x010000 + continuation + 1 : 0x020702) &&
			(!returning || bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800) == !BIT(op, 0)),
			"all far branch/call/return variants execute on distinct pages with correct delay and stack semantics");
		if (call)
			expect(data.read_word(0x03fe) == 1 && data.read_word(0x03ff) == continuation,
				"far call variant stores PC then XPC");
		if (index + 1 < std::size(far_opcodes))
		{
			start_far_case(index + 1);
			return true;
		}
		if (profile_is("far-atomic"))
		{
			osd_printf_info("TMS320C54x atomic far transfers: PASS encodings=16 caller_page=1 target_page=2 call_return_frame=1 delayed_target_retained=1 staged_acceptance=0 save_replay=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated atomic far transfers complete");
		}
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
		program.write_word(0x020600, 0xf074); // Near CALL on page 2.
		program.write_word(0x020601, 0x0700);
		program.write_word(0x020602, 0xf4e1);
		program.write_word(0x020700, 0xe803);
		program.write_word(0x020701, 0xfc00);
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
		m_near_return_saved.str(std::string());
		m_near_return_saved.clear();
		expect(machine().save().write_stream(m_near_return_saved) == STATERR_NONE,
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
		m_near_return_saved.clear();
		m_near_return_saved.seekg(0);
		expect(machine().save().read_stream(m_near_return_saved) == STATERR_NONE,
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
		m_far_transfer.saved.str(std::string());
		m_far_transfer.saved.clear();
		expect(machine().save().write_stream(m_far_transfer.saved) == STATERR_NONE,
			"write pending far-transfer save state");
		return next_atomic_phase(786);
	}
	return false;
}

} // namespace c54x_test
