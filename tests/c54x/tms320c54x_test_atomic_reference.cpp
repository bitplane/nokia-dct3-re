// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_square.h"

namespace c54x_test {

// TI instruction examples and arithmetic boundary sequences.
bool tms320c54x_test_state::check_atomic_reference_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 548 && m_phase <= 553)
	{
		unsigned const index = m_phase - 548;
		auto const test = square_case(index);
		check_case(*m_cpu, test, m_port_writes, m_last_port_cycle - m_first_port_cycle);
		// The verifier throws on mismatch; retain opcode accounting after success.
		expect_opcode(test.opcode, true, "TI square reference case");
		if (index + 1 < square_vectors.size())
		{
			m_port_writes = 0;
			prepare_case(*m_cpu, square_case(index + 1));
			return next_atomic_phase(m_phase + 1);
		}
		program.write_word(0x05e2, 0x5193); // DADD *AR3+,A,B
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f90, 0x1534);
		data.write_word(0x0f91, 0x3456);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x56788933}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR3, 0x0f90},
			{cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(554);
	}
	if ((m_phase >= 554 && m_phase <= 562) || m_phase == 9211 || m_phase == 9212)
	{
		struct long_alu_case { u16 opcode; u64 a_before; u16 st1; u16 high; u16 low; u64 a_after; u64 b_after; u16 ar3_after; u16 st0_after; unsigned cycles; };
		static constexpr long_alu_case cases[] = {
			{ 0x5193, 0x56788933, 0x0100, 0x1534, 0x3456, 0x56788933, 0x6bacbd89, 0x0f92, 0, 3 },
			{ 0x518b, 0x56783933, 0x0180, 0x1534, 0x3456, 0x56783933, 0x6bac6d89, 0x0f8e, 0, 3 },
			{ 0x5493, 0x56788933, 0x0100, 0x1534, 0x3456, 0x414454dd, 0, 0x0f92, 0x0800, 3 },
			{ 0x548b, 0x56783933, 0x0180, 0x1534, 0x3456, 0x414404dd, 0, 0x0f8e, 0x0800, 3 },
			{ 0x50f8, 0x7fffffff, 0x0300, 0, 1, 0x7fffffff, 0, 0x0f90, 0x0400, 4 },
			{ 0x5083, 0x7fffffff, 0x0380, 1, 1, 0x80000000, 0, 0x0f90, 0, 3 },
			{ 0x5483, 0x00010000, 0x0380, 0, 1, 0x0001ffff, 0, 0x0f90, 0x0800, 3 },
			{ 0x5893, 0x56788933, 0x0100, 0x1534, 0x3456, 0xffbebbab23ULL, 0, 0x0f92, 0, 3 },
			{ 0x588b, 0x56783933, 0x0180, 0x1534, 0x3456, 0xffbebcfb23ULL, 0, 0x0f8e, 0, 3 },
			{ 0x5082, 0x00010000, 0x0100, 1, 0xffff, 0x0002ffff, 0, 0x0f90, 0, 3 },
			{ 0x5085, 0x00010000, 0x0100, 1, 0xffff, 0x0002ffff, 0, 0x0f90, 0, 3 }
		};
		const unsigned index = m_phase >= 9211 ? m_phase - 9211 + 9 : m_phase - 554;
		const long_alu_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == row.ar3_after &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == row.st0_after &&
			(index < 9 || (m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f92 &&
				data.read_word(0x0f92) == row.high && data.read_word(0x0f93) == row.low &&
				data.read_word(0x0f90) == 2 && data.read_word(0x0f91) == 3)) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == row.cycles,
			"DADD/DSUB/DRSUB long-word arithmetic matches TI C16 examples and cycle costs");
		if (index + 1 < std::size(cases))
		{
			const long_alu_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			program.write_word(0x05e3, next.opcode == 0x50f8 ? 0x0f90 : 0x75d6);
			program.write_word(0x05e4, next.opcode == 0x50f8 ? 0x75d6 : 0x0124);
			program.write_word(0x05e5, next.opcode == 0x50f8 ? 0x0124 : 0xf5e1);
			program.write_word(0x05e6, 0xf5e1);
			data.write_word(0x0f90, index >= 8 ? 2 : next.high);
			data.write_word(0x0f91, index >= 8 ? 3 : next.low);
			data.write_word(0x0f92, next.high);
			data.write_word(0x0f93, next.low);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, next.a_before}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR3, 0x0f90},
				{cpu_regs::STATE_AR2, 0x0f92}, {cpu_regs::STATE_AR5, 0x0f92},
			});
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0,
					next.opcode == 0x588b ? 0x0800 : 0);
			set_registers(*m_cpu, {
				{cpu_regs::STATE_ST1, next.st1}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(index < 8 ? m_phase + 1 : 9211 + index - 8);
		}
		program.write_word(0x05e0, 0x75f8);
		program.write_word(0x05e1, 0x0d00);
		program.write_word(0x05e2, 0x0124);
		program.write_word(0x05e3, 0x5086); // DADD *AR6,A.
		program.write_word(0x05e4, 0x75f8);
		program.write_word(0x05e5, 0x0d00);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		data.write_word(0x0f92, 1);
		data.write_word(0x0f93, 0xffff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x10000}, {cpu_regs::STATE_B, 0x76543210}, {cpu_regs::STATE_AR6, 0x0f92},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(9252);
	}
	if (m_phase == 9252)
	{
		expect_opcode(0x5086,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x2ffff &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x76543210 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0f92 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0100 &&
			data.read_word(0x0f92) == 1 && data.read_word(0x0f93) == 0xffff &&
			data.read_word(0x0f90) == 2 && data.read_word(0x0f91) == 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 DADD through stationary AR6 preserves B/status/source/pointers in one cycle with independent absolute port markers");
		program.write_word(0x05e0, 0x75d6);
		program.write_word(0x05e1, 0x0124);
		program.write_word(0x05e2, 0x5a8b); // DADST *AR3-,A
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f90, 0x1534);
		data.write_word(0x0f91, 0x3456);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x2345}, {cpu_regs::STATE_AR3, 0x0f90},
			{cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0180); // SXM, C16
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(563);
	}
	if (m_phase >= 563 && m_phase <= 569)
	{
		struct t_long_case { u16 opcode; u16 st1; u64 a_after; u64 b_after; u16 ar3_after; int carry; unsigned cycles; };
		static constexpr t_long_case cases[] = {
			{ 0x5a8b, 0x0180, 0x38791111, 0, 0x0f8e, 0, 3 },
			{ 0x5a93, 0x0100, 0x3879579b, 0, 0x0f92, 0, 3 },
			{ 0x5e93, 0x0100, 0xfff1ef1111ULL, 0, 0x0f92, 0, 3 },
			{ 0x5e8b, 0x0180, 0xfff1ef579bULL, 0, 0x0f8e, -1, 3 },
			{ 0x5c93, 0x0100, 0xfff1ef1111ULL, 0, 0x0f92, 0, 3 },
			{ 0x5c8b, 0x0180, 0xfff1ef1111ULL, 0, 0x0f8e, 0, 3 },
			{ 0x5bf8, 0x0180, 0, 0x38791111, 0x0f90, 0, 4 }
		};
		const unsigned index = m_phase - 563;
		const t_long_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x2345 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == row.ar3_after &&
			(row.carry < 0 || bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == bool(row.carry)) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == row.cycles,
			"T-based long-word arithmetic matches TI result and cycle examples");
		if (m_phase < 569)
		{
			const t_long_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			program.write_word(0x05e3, next.opcode == 0x5bf8 ? 0x0f90 : 0x75d6);
			program.write_word(0x05e4, next.opcode == 0x5bf8 ? 0x75d6 : 0x0124);
			program.write_word(0x05e5, next.opcode == 0x5bf8 ? 0x0124 : 0xf5e1);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x2345},
				{cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, next.st1},
				{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0x1f83); // SUBC *AR3,B
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f90, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x10000}, {cpu_regs::STATE_AR3, 0x0f90},
			{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(570);
	}
	if (m_phase == 570)
	{
		expect_opcode(0x1f83,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10001 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUBC B uses the 1f encoding and leaves A unchanged");
		program.write_word(0x05e2, 0x5aea); // DADST *+AR2(6),A
		program.write_word(0x05e3, 6);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0f06, 0x1534);
		data.write_word(0x0f07, 0x3456);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x2345);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, C16 clear.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(571);
	}
	if (m_phase == 571)
	{
		expect_opcode(0x5aea,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x3879579b &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x2345 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f06 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"long-offset DADST preupdates AR and costs two cycles");
		program.write_word(0x05e2, 0x0e83); // SUBB *AR3,A
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f90, 6);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 6);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // Borrow input.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(572);
	}
	if (m_phase == 572)
	{
		expect_opcode(0x0e83,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffffffULL &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUBB A consumes inverted carry as borrow in one cycle");
		program.write_word(0x05e2, 0x0f83); // SUBB *AR3,B
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff80000006ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // No borrow input.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(573);
	}
	if (m_phase == 573)
	{
		expect_opcode(0x0f83,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80000000ULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUBB B preserves A and no-borrow carry");
		program.write_word(0x05e2, 0x0183); // ADD *AR3,B
		m_port_writes = 0;
		data.write_word(0x0f90, 1);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(574);
	}
	if (m_phase == 574)
	{
		expect_opcode(0x0183,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x100000000ULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ADD Smem,B carries across bit 32 in one cycle");
		program.write_word(0x05e2, 0x0b83); // SUBS *AR3,B
		data.write_word(0x0f90, 0xffff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x10000}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(575);
	}
	if (m_phase == 575)
	{
		expect_opcode(0x0b83,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 1 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUBS Smem,B suppresses SXM sign extension in one cycle");
		program.write_word(0x05e2, 0x1983); // AND *AR3,B
		data.write_word(0x0f90, 0x00ff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x123456}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(576);
	}
	if (m_phase == 576)
	{
		expect_opcode(0x1983,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x56 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"AND Smem,B zeroes upper bits without changing carry");
		program.write_word(0x05e2, 0x2183); // MPY *AR3,B
		data.write_word(0x0f90, 3);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xfffe);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, FRCT clear.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(577);
	}
	if (m_phase == 577)
	{
		expect_opcode(0x2183,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffffffaULL &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MPY Smem,B signs both factors and leaves carry unchanged");
		program.write_word(0x05e2, 0x2383); // MPYR *AR3,B
		data.write_word(0x0f90, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x4000},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0f90},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(578);
	}
	if ((m_phase >= 578 && m_phase <= 586) || (m_phase >= 9200 && m_phase <= 9204))
	{
		struct multiply_case { u16 opcode, t, memory, t_after; u64 a_before, b_before, a_after, b_after; u16 ar2_after = 0x0f90, st1 = 0x0100; };
		static constexpr multiply_case cases[] = {
			{ 0x2383, 0x4000, 3,      0x4000, 0x1234, 0,       0x1234, 0x10000 },
			{ 0x2583, 0xfffe, 3,      0xfffe, 0x1234, 0,       0x1234, 0x2fffa },
			{ 0x2783, 0x4000, 0xfffe, 0xfffe, 0x1234, 0,       0x1234, 4 },
			{ 0x2983, 0xfffe, 3,      0xfffe, 0x1234, 10,      0x1234, 4 },
			{ 0x2b83, 0x4000, 3,      0x4000, 0x1234, 0x10000, 0x1234, 0x20000 },
			{ 0x2c83, 2,      3,      2,      10,     0x5678,  4,      0x5678 },
			{ 0x2d83, 2,      3,      2,      0x1234, 10,      0x1234, 4 },
			{ 0x2e83, 0x4000, 3,      0x4000, 0x30000, 0x5678, 0x20000, 0x5678 },
			{ 0x2f83, 0x4000, 3,      0x4000, 0x1234, 0x30000, 0x1234, 0x20000 },
			{ 0x2082, 0xfffe, 3,      0xfffe, 0,      0x5678, 0xfffffffffaULL, 0x5678 },
			{ 0x218a, 0xfffe, 3,      0xfffe, 0x1234, 0,      0x1234, 0xfffffffffaULL, 0x0f8f },
			{ 0x2882, 0xfffe, 3,      0xfffe, 5,      0x5678, 0xffffffffffULL, 0x5678 },
			{ 0x2882, 0xfffe, 3,      0xfffe, 5,      0x5678, 0xfffffffff9ULL, 0x5678, 0x0f90, 0x0140 },
			{ 0x2d92, 2,      3,      2,      0x1234, 10,     0x1234, 4, 0x0f91 }
		};
		const unsigned index = m_phase >= 9200 ? m_phase - 9200 + 9 : m_phase - 578;
		const multiply_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == row.t_after &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			(index < 9 || m_cpu->state_int(tms320c54x_device::STATE_AR2) == row.ar2_after) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == row.st1 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"multiply/MAC family result, side effects, and one-cycle DARAM timing");
		if (index + 1 < std::size(cases))
		{
			const multiply_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			data.write_word(0x0f90, next.memory);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, next.a_before}, {cpu_regs::STATE_B, next.b_before}, {cpu_regs::STATE_T, next.t},
				{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, next.st1}, {cpu_regs::STATE_AR3, 0x0f90},
				{cpu_regs::STATE_AR2, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(index < 8 ? m_phase + 1 : 9200 + index - 8);
		}
		program.write_word(0x05e2, 0x3383); // MASA *AR3
		data.write_word(0x0f90, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x20000}, {cpu_regs::STATE_B, 10}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(587);
	}
	if (m_phase >= 587 && m_phase <= 590)
	{
		struct acca_case { u16 opcode, memory, t_before, t_after, ar3_after; u64 a_before, b_before, a_after, b_after; };
		static constexpr acca_case cases[] = {
			{ 0x3383, 3,      0,      3,      0x0f90, 0x20000,    10,      0x20000, 4 },
			{ 0x3583, 3,      0,      3,      0x0f90, 0x20000,    10,      0x20000, 16 },
			{ 0x3783, 3,      0,      3,      0x0f90, 0x40000000, 0x10000, 0x40000000, 0x20000 },
			{ 0x3693, 0x2000, 0x5678, 0x5678, 0x0f91, 0x12340000, 0x10000, 0x06270000, 0x20000000 }
		};
		const unsigned index = m_phase - 587;
		const acca_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == row.t_after &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == row.ar3_after &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ACCA multiply and POLY result, T/AR update, and one-cycle timing");
		if (m_phase < 590)
		{
			const acca_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			data.write_word(0x0f90, next.memory);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, next.a_before}, {cpu_regs::STATE_B, next.b_before},
				{cpu_regs::STATE_T, next.t_before}, {cpu_regs::STATE_ST0, 0x0800},
			});
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, FRCT clear.
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0x3083); // LD *AR3,T
		data.write_word(0x0f90, 0xabcd);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678}, {cpu_regs::STATE_T, 0},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0f90},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(591);
	}
	if ((m_phase >= 591 && m_phase <= 597) || (m_phase >= 6508 && m_phase <= 6510) || m_phase == 6572 || m_phase == 9205)
	{
		struct shifted_case { u16 opcode, memory, t_before, t_after; u64 a_before, b_before, a_after, b_after; };
		static constexpr shifted_case cases[] = {
			{ 0x3083, 0xabcd, 0, 0xabcd, 0x1234, 0x5678, 0x1234, 0x5678 },
			{ 0x3983, 0xfffe, 0, 0xfffe, 0x1234, 10, 0x1234, 14 },
			{ 0x3a83, 3, 0, 3, 10, 0x5678, 1, 0x5678 },
			{ 0x3c83, 0xfffe, 0, 0, 0x30000, 0x5678, 0x10000, 0x5678 },
			{ 0x3d83, 2, 0, 0, 1, 0x5678, 1, 0x20001 },
			{ 0x3e83, 0xfffe, 0, 0, 0x1234, 0x30000, 0x10000, 0x30000 },
			{ 0x3f83, 2, 0, 0, 0x1234, 1, 0x1234, 0x20001 },
			{ 0x3c92, 0xfffe, 0, 0, 0x30000, 0x5678, 0x10000, 0x5678 },
			{ 0x3f92, 2, 0, 0, 0x1234, 1, 0x1234, 0x20001 },
			{ 0x3f82, 2, 0, 0, 0x1234, 1, 0x1234, 0x20001 },
			{ 0x3c82, 0xfffe, 0, 0, 0x30000, 0x5678, 0x10000, 0x5678 },
			{ 0x308a, 0xabcd, 0x1234, 0xabcd, 0x1234, 0x5678, 0x1234, 0x5678 }
		};
		const unsigned index = m_phase == 9205 ? 11 : m_phase == 6572 ? 10 : m_phase >= 6508 ? 7 + m_phase - 6508 : m_phase - 591;
		const shifted_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == row.t_after &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			(index < 7 || m_cpu->state_int(tms320c54x_device::STATE_AR2) == (index == 11 ? 0x0f91 : index < 9 ? 0x0f93 : 0x0f92)) &&
			(index != 10 || (data.read_word(0x0f92) == 0xfffe && data.read_word(0x0f90) == 2)) &&
			(index != 11 || (data.read_word(0x0f92) == 0xabcd && data.read_word(0x0f90) == 2)) &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"T load, square, and shifted-add routing with one-cycle DARAM timing");
		if (index + 1 < std::size(cases))
		{
			const shifted_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			data.write_word(0x0f90, index >= 9 ? 2 : next.memory);
			data.write_word(0x0f92, next.memory);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, next.a_before}, {cpu_regs::STATE_B, next.b_before},
				{cpu_regs::STATE_T, next.t_before}, {cpu_regs::STATE_ST0, 0x0800},
			});
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, FRCT clear.
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_AR2, 0x0f92}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(index == 10 ? 9205 : index == 9 ? 6572 : index < 6 ? m_phase + 1 : 6508 + index - 6);
		}
		program.write_word(0x05e2, 0x4083); // SUB *AR3,16,A,A
		data.write_word(0x0f90, 2);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x30000);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x9abc);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // No borrow must retain cleared C.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(598);
	}
	if ((m_phase >= 598 && m_phase <= 601) || m_phase == 6561)
	{
		struct shifted_sub_case { u16 opcode, st0_before, st0_after; u64 a_before, b_before, a_after, b_after; };
		static constexpr shifted_sub_case cases[] = {
			{ 0x4083, 0,      0,      0x30000, 0x5678, 0x10000,       0x5678 },
			{ 0x4183, 0x0800, 0x0800, 0x30000, 0x5678, 0x30000,       0x10000 },
			{ 0x4283, 0x0800, 0,      0x1234,  0x10000, 0xffffff0000, 0x10000 },
			{ 0x4383, 0,      0,      0x1234,  0x30000, 0x1234,        0x10000 },
			{ 0x4082, 0x0800, 0x0800, 0x40000, 0x5678, 0x10000,       0x5678 }
		};
		const unsigned index = m_phase == 6561 ? 4 : m_phase - 598;
		const shifted_sub_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x9abc &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			(index != 4 || (m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f92 &&
				data.read_word(0x0f92) == 3)) &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == row.st0_after &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"shifted SUB routing, borrow-only carry change, and one-cycle DARAM timing");
		if (index < 4)
		{
			const shifted_sub_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			data.write_word(0x0f90, 2);
			data.write_word(0x0f92, 3);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, next.a_before}, {cpu_regs::STATE_B, next.b_before}, {cpu_regs::STATE_T, 0x9abc},
				{cpu_regs::STATE_ST0, next.st0_before}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0f90},
				{cpu_regs::STATE_AR2, 0x0f92}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(index < 3 ? m_phase + 1 : 6561);
		}
		program.write_word(0x05e2, 0x4392); // SUB *AR2+,16,B,B.
		data.write_word(0x0f92, 2);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x30000}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_AR2, 0x0f92}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6505);
	}
	if (m_phase == 6505)
	{
		expect_opcode(0x4392,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10000 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x9abc &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f93 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) &&
			data.read_word(0x0f92) == 2 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 shifted SUB into B retains cleared carry on no borrow and postincrements AR2 in one cycle");
		program.write_word(0x05e2, 0x438a); // SUB *AR2-,16,B,B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x30000}, {cpu_regs::STATE_AR2, 0x0f92}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6529);
	}
	if (m_phase == 6529)
	{
		expect_opcode(0x438a,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10000 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x9abc &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f91 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) &&
			data.read_word(0x0f92) == 2 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 shifted SUB through AR2 reads before decrement and preserves A/T/cleared carry in one cycle");
		program.write_word(0x05e2, 0x5283); // DADD *AR3,B,A
		data.write_word(0x0f90, 1);
		data.write_word(0x0f91, 0xffff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x30002}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(602);
	}
	if ((m_phase >= 602 && m_phase <= 605) || m_phase == 9213 || m_phase == 9214)
	{
		struct dadd_case { u16 opcode, st1; u64 a_after, b_after; };
		static constexpr dadd_case cases[] = {
			{ 0x5283, 0x0100, 0x50001, 0x30002 },
			{ 0x5383, 0x0100, 0x1234,  0x50001 },
			{ 0x5283, 0x0180, 0x40001, 0x30002 },
			{ 0x5383, 0x0180, 0x1234,  0x40001 },
			{ 0x5385, 0x0100, 0x1234,  0x50001 },
			{ 0x5385, 0x0180, 0x1234,  0x40001 }
		};
		const unsigned index = m_phase >= 9213 ? m_phase - 9213 + 4 : m_phase - 602;
		const dadd_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			(index < 4 || (m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f92 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == row.st1 &&
				data.read_word(0x0f92) == 1 && data.read_word(0x0f93) == 0xffff &&
				data.read_word(0x0f90) == 7 && data.read_word(0x0f91) == 8)) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"DADD B-source routing and C16 carry isolation with one-cycle DARAM timing");
		if (index + 1 < std::size(cases))
		{
			const dadd_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			if (index >= 3)
			{
				data.write_word(0x0f90, 7);
				data.write_word(0x0f91, 8);
				data.write_word(0x0f92, 1);
				data.write_word(0x0f93, 0xffff);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f92);
			}
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x30002}, {cpu_regs::STATE_ST0, 0},
				{cpu_regs::STATE_ST1, next.st1}, {cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(index < 3 ? m_phase + 1 : 9213 + index - 3);
		}
		program.write_word(0x05e2, 0x5583); // DSUB *AR3,B
		data.write_word(0x0f90, 1);
		data.write_word(0x0f91, 0xffff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x20001}, {cpu_regs::STATE_T, 2}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(606);
	}
	if (m_phase >= 606 && m_phase <= 613)
	{
		struct long_b_case { u16 opcode, high, low, st1; u64 b_before, b_after; };
		static constexpr long_b_case cases[] = {
			{ 0x5583, 1, 0xffff, 0x0100, 0x20001, 2 },
			{ 0x5583, 1, 0xffff, 0x0180, 0x20001, 0x10002 },
			{ 0x5983, 2, 1,      0x0100, 0x1ffff, 2 },
			{ 0x5983, 2, 1,      0x0180, 0x1ffff, 0x10002 },
			{ 0x5d83, 1, 1,      0x0100, 0x5678,  0xfffffeffff },
			{ 0x5d83, 1, 1,      0x0180, 0x5678,  0xffffffffff },
			{ 0x5f83, 1, 1,      0x0100, 0x5678,  0xfffffeffff },
			{ 0x5f83, 1, 1,      0x0180, 0x5678,  0xffffff0003 }
		};
		const unsigned index = m_phase - 606;
		const long_b_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"B long-arithmetic C16 mode and one-cycle DARAM timing");
		if (m_phase < 613)
		{
			const long_b_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			data.write_word(0x0f90, next.high);
			data.write_word(0x0f91, next.low);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, next.b_before}, {cpu_regs::STATE_T, 2},
				{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, next.st1}, {cpu_regs::STATE_AR3, 0x0f90},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xb03a); // MAC *AR5,*AR4+,A,A
		data.write_word(0x0f90, 2);
		data.write_word(0x0f91, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x18000}, {cpu_regs::STATE_B, 0x28000}, {cpu_regs::STATE_T, 0},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR4, 0x0f91},
			{cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(614);
	}
	if (m_phase >= 614 && m_phase <= 629)
	{
		struct dual_mac_case { u16 opcode; u64 a_after, b_after; };
		static constexpr dual_mac_case cases[] = {
			{ 0xb03a, 0x18006, 0x28000 }, { 0xb13a, 0x18000, 0x18006 },
			{ 0xb23a, 0x28006, 0x28000 }, { 0xb33a, 0x18000, 0x28006 },
			{ 0xb43a, 0x20000, 0x28000 }, { 0xb53a, 0x18000, 0x20000 },
			{ 0xb63a, 0x30000, 0x28000 }, { 0xb73a, 0x18000, 0x30000 },
			{ 0xb83a, 0x17ffa, 0x28000 }, { 0xb93a, 0x18000, 0x17ffa },
			{ 0xba3a, 0x27ffa, 0x28000 }, { 0xbb3a, 0x18000, 0x27ffa },
			{ 0xbc3a, 0x10000, 0x28000 }, { 0xbd3a, 0x18000, 0x10000 },
			{ 0xbe3a, 0x20000, 0x28000 }, { 0xbf3a, 0x18000, 0x20000 }
		};
		const unsigned index = m_phase - 614;
		const dual_mac_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"dual MAC/MAS source, destination, rounding, pointer, and one-cycle timing");
		if (m_phase < 629)
		{
			const dual_mac_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x18000}, {cpu_regs::STATE_B, 0x28000}, {cpu_regs::STATE_T, 0},
				{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR4, 0x0f91},
				{cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xd03a); // ST A,*AR4+ || MAC *AR5,A
		data.write_word(0x0f90, 3);
		data.write_word(0x0f91, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x18000}, {cpu_regs::STATE_B, 0x28000}, {cpu_regs::STATE_T, 2},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR4, 0x0f91},
			{cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(630);
	}
	if (m_phase >= 630 && m_phase <= 645)
	{
		struct parallel_mac_case { u16 opcode, store; u64 a_after, b_after; };
		static constexpr parallel_mac_case cases[] = {
			{ 0xd03a, 1, 0x18006, 0x28000 }, { 0xd13a, 1, 0x18000, 0x28006 },
			{ 0xd23a, 2, 0x18006, 0x28000 }, { 0xd33a, 2, 0x18000, 0x28006 },
			{ 0xd43a, 1, 0x20000, 0x28000 }, { 0xd53a, 1, 0x18000, 0x30000 },
			{ 0xd63a, 2, 0x20000, 0x28000 }, { 0xd73a, 2, 0x18000, 0x30000 },
			{ 0xd83a, 1, 0x17ffa, 0x28000 }, { 0xd93a, 1, 0x18000, 0x27ffa },
			{ 0xda3a, 2, 0x17ffa, 0x28000 }, { 0xdb3a, 2, 0x18000, 0x27ffa },
			{ 0xdc3a, 1, 0x10000, 0x28000 }, { 0xdd3a, 1, 0x18000, 0x20000 },
			{ 0xde3a, 2, 0x10000, 0x28000 }, { 0xdf3a, 2, 0x18000, 0x20000 }
		};
		const unsigned index = m_phase - 630;
		const parallel_mac_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
			data.read_word(0x0f91) == row.store &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"parallel store MAC/MAS routing, rounding, and one-cycle timing");
		if (m_phase < 645)
		{
			const parallel_mac_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			data.write_word(0x0f91, 0);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x18000}, {cpu_regs::STATE_B, 0x28000}, {cpu_regs::STATE_T, 2},
				{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR4, 0x0f91},
				{cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xd93a); // TI SPRU172C: ST A,*AR4+ || MAS *AR5,B.
		data.write_word(0x0f90, 0x4321);
		data.write_word(0x0f91, 0x1234);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x111111}, {cpu_regs::STATE_B, 0x1111}, {cpu_regs::STATE_T, 0x0400},
			{cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0105); // ASM = 5.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(646);
	}
	if (m_phase == 646 || m_phase == 647)
	{
		const bool rounded = m_phase == 647;
		expect_opcode(rounded ? 0xddba : 0xd93a,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x111111 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(rounded ? 0xfffef40000ULL : 0xfffef38d11ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0400 &&
			data.read_word(0x0f91) == (rounded ? 0x0022 : 0x0222) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == (rounded ? 0x0f91 : 0x0f90) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI parallel MAS/MASR examples match result, ASM store, pointer, and cycle cost");
		if (!rounded)
		{
			program.write_word(0x05e2, 0xddba); // ST A,*AR4+ || MASR *AR5+,B.
			data.write_word(0x0f91, 0x1234);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x111111}, {cpu_regs::STATE_B, 0x1111}, {cpu_regs::STATE_T, 0x0400},
				{cpu_regs::STATE_ST0, 0},
			});
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0101); // ASM = 1.
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(647);
		}
		program.write_word(0x05e2, 0xd9b7); // ST A,*AR5- || MAS *AR5+,B.
		data.write_word(0x0f90, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x18000}, {cpu_regs::STATE_B, 0x28000}, {cpu_regs::STATE_T, 2},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR5, 0x0f90},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(648);
	}
	if (m_phase == 648)
	{
		expect_opcode(0xd9b7,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x18000 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x27ffa &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
			data.read_word(0x0f90) == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f91 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"parallel MAS reads X before aliased Y store and applies Xmod once");
		program.write_word(0x05e2, 0xb83a); // MAS *AR5,*AR4+,A,A with FRCT.
		data.write_word(0x0f90, 2);
		data.write_word(0x0f91, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x18000}, {cpu_regs::STATE_B, 0x28000}, {cpu_regs::STATE_T, 0},
			{cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0140); // SXM, FRCT.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(649);
	}
	if (m_phase >= 649 && m_phase <= 652)
	{
		struct multiply_mode_case {
			u16 opcode, x, y, t_before, t_after, st1, st0_after, store;
			u64 a_before, b_before, a_after, b_after;
		};
		static constexpr multiply_mode_case cases[] = {
			{ 0xb83a, 2,      3, 0, 2,      0x0140, 0,      3,
			  0x18000,    0x28000, 0x17ff4,    0x28000 },
			{ 0xd93a, 3,      0, 2, 2,      0x0140, 0,      1,
			  0x18000,    0x28000, 0x18000,    0x27ff4 },
			{ 0xb83a, 0xffff, 2, 0, 0xffff, 0x0300, 0x0400, 2,
			  0x7fffffff, 0,       0x7fffffff, 0 },
			{ 0xd93a, 0xffff, 0, 2, 2,      0x0300, 0x0200, 1,
			  0x18000,    0x7fffffff, 0x18000, 0x7fffffff }
		};
		const unsigned index = m_phase - 649;
		const multiply_mode_case &row = cases[index];
		expect_opcode(row.opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == row.a_after &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == row.b_after &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == row.t_after &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0600) == row.st0_after &&
			data.read_word(0x0f91) == row.store &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"dual and parallel MAS FRCT doubling, OVM saturation, and one-cycle timing");
		if (m_phase < 652)
		{
			const multiply_mode_case &next = cases[index + 1];
			program.write_word(0x05e2, next.opcode);
			data.write_word(0x0f90, next.x);
			data.write_word(0x0f91, next.y);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, next.a_before}, {cpu_regs::STATE_B, next.b_before},
				{cpu_regs::STATE_T, next.t_before}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, next.st1},
				{cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xe13a); // LMS *AR5,*AR4+.
		data.write_word(0x0f90, 0x0055);
		data.write_word(0x0f91, 0x00aa);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x77778888}, {cpu_regs::STATE_B, 0x100}, {cpu_regs::STATE_T, 0x4444},
			{cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(653);
	}
	if (m_phase == 653)
	{
		expect_opcode(0xe13a,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x77cd0888 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x3972 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI LMS example updates both accumulators without overwriting T in one cycle");
		program.write_word(0x05e2, 0xa6ab); // MACSU *AR4+,*AR5+,A.
		data.write_word(0x0f90, 0x8765);
		data.write_word(0x0f91, 0x1234);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1000}, {cpu_regs::STATE_B, 0x2222}, {cpu_regs::STATE_T, 8}, {cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0f90}, {cpu_regs::STATE_AR5, 0x0f91}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(654);
	}
	if (m_phase == 654)
	{
		expect_opcode(0xa6ab,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x09a0aa84 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2222 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8765 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f91 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f92 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI MACSU example uses unsigned X, signed Y, and one-cycle dual addressing");
		program.write_word(0x05e2, 0xa7ab); // MACSU *AR4+,*AR5+,B.
		data.write_word(0x0f90, 0x8765);
		data.write_word(0x0f91, 0xfffe);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0140); // SXM, FRCT.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0f90}, {cpu_regs::STATE_AR5, 0x0f91}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(655);
	}
	if (m_phase == 655)
	{
		expect_opcode(0xa7ab,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffde26cULL &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8765 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f91 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f92 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MACSU B uses signed Y, unsigned X, FRCT doubling, and one cycle");
		program.write_word(0x05e2, 0x788b); // MACP *AR3-,0700,A.
		write_words(program, {
			{0x05e3, 0x0700}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1}, {0x0700, 0x1234},
		});
		data.write_word(0x0f90, 0x0055);
		data.write_word(0x0f91, 0x0066);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x770000}, {cpu_regs::STATE_T, 8}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(656);
	}
	if (m_phase == 656)
	{
		expect_opcode(0x788b,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7d0b44 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f8f &&
			data.read_word(0x0f91) == 0x0066 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"TI MACP example reads program coefficient without delay write in three cycles");
		program.write_word(0x05e2, 0x7a8b); // MACD *AR3-,0700,A.
		data.write_word(0x0f91, 0x0066);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x770000}, {cpu_regs::STATE_T, 8}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(657);
	}
	if (m_phase == 657)
	{
		expect_opcode(0x7a8b,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7d0b44 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f8f &&
			data.read_word(0x0f91) == 0x0055 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"TI MACD example copies Smem to successor and costs three cycles");
		program.write_word(0x05e2, 0xec02); // RPT #2.
		program.write_word(0x05e3, 0x7893); // MACP *AR3+,0700,A.
		write_words(program, {
			{0x05e4, 0x0700}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1}, {0x0700, 4}, {0x0701, 5},
			{0x0702, 6},
		});
		data.write_word(0x0f90, 1);
		data.write_word(0x0f91, 2);
		data.write_word(0x0f92, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0x0100},
			{cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(658);
	}
	if (m_phase == 658)
	{
		expect_opcode(0x7893,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 32 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f93 &&
			data.read_word(0x0f91) == 2 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
			"repeated MACP advances program coefficient and pipelines after first multiply");
		program.write_word(0x05e2, 0xe03a); // FIRS *AR5,*AR4+,0700.
		write_words(program, {
			{0x05e3, 0x0700}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1}, {0x0700, 0x1234},
		});
		data.write_word(0x0f90, 0x0055);
		data.write_word(0x0f91, 0x00aa);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x770000}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x4444},
			{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR4, 0x0f91},
			{cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(659);
	}
	if (m_phase == 659)
	{
		expect_opcode(0xe03a,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff0000 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8762c &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"TI FIRS example accumulates old A high into B and sums X/Y into A");
		program.write_word(0x05e2, 0xec02); // RPT #2.
		program.write_word(0x05e3, 0xe03a); // FIRS *AR5,*AR4+,0700.
		write_words(program, {
			{0x05e4, 0x0700}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1}, {0x0700, 1}, {0x0701, 2},
			{0x0702, 3},
		});
		data.write_word(0x0f90, 1);
		data.write_word(0x0f91, 2);
		data.write_word(0x0f92, 2);
		data.write_word(0x0f93, 2);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x770000}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x4444},
			{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR4, 0x0f91},
			{cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(660);
	}
	if (m_phase == 660)
	{
		expect_opcode(0xe03a,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x30000 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x86 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f94 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
			"repeated FIRS uses old A, advances coefficients, and pipelines after first pass");
		program.write_word(0x05e2, 0xa03a); // ADD *AR5,*AR4+,A.
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f90, 0x0055);
		data.write_word(0x0f91, 0x00aa);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678}, {cpu_regs::STATE_T, 0x4444},
			{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR4, 0x0f91},
			{cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(661);
	}
	if (m_phase >= 661 && m_phase <= 664)
	{
		const u16 opcode = 0xa03a + ((m_phase - 661) << 8);
		const bool subtract = m_phase >= 663;
		const bool destination_b = BIT(m_phase - 661, 0);
		const u64 result = subtract ? 0xffffab0000ULL : 0xff0000;
		expect_opcode(opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (destination_b ? 0x1234 : result) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (destination_b ? result : 0x5678) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"dual-memory ADD/SUB selects A/B, shifts both operands, and costs one cycle");
		if (m_phase < 664)
		{
			program.write_word(0x05e2, opcode + 0x100);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678}, {cpu_regs::STATE_ST0, 0},
				{cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xa249); // SUB *AR2-,*AR3+,A.
		data.write_word(0x0f94, 0xfffb);
		data.write_word(0x0f95, 7);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_AR2, 0x0f94}, {cpu_regs::STATE_AR3, 0x0f95}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6522);
	}
	if (m_phase == 6522 || m_phase == 6523)
	{
		bool const modify = m_phase == 6522;
		expect_opcode(modify ? 0xa249 : 0xa201,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffff40000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (modify ? 0x0f93 : 0x0f94) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == (modify ? 0x0f96 : 0x0f95) &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0800 &&
			data.read_word(0x0f94) == 0xfffb && data.read_word(0x0f95) == 7 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 dual SUB shifts signed X/Y, updates carry at bit 32 and independently modifies its pointers");
		if (modify)
		{
			program.write_word(0x05e2, 0xa201); // SUB *AR2,*AR3,A.
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR2, 0x0f94}, {cpu_regs::STATE_AR3, 0x0f95}, {cpu_regs::STATE_ST0, 0},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(6523);
		}
		program.write_word(0x05e2, 0xe33a); // ABDST *AR5,*AR4+.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffabcd0000ULL}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(665);
	}
	if (m_phase == 665)
	{
		expect_opcode(0xe33a,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffab0000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5433 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI ABDST example adds absolute old A high to B and subtracts dual operands");
		data.write_word(0x0f90, 0xfffe);
		data.write_word(0x0f91, 3);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffabcd0000ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0140); // SXM, FRCT.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(666);
	}
	if (m_phase == 666)
	{
		expect_opcode(0xe33a,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffb0000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xa866 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ABDST sign-extends X/Y under SXM and doubles distance under FRCT");
		program.write_word(0x05e2, 0x4c83); // LTD *AR3.
		data.write_word(0x0f90, 0x6cac);
		data.write_word(0x0f91, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR3, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(667);
	}
	if (m_phase == 667)
	{
		expect_opcode(0x4c83,
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x6cac &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			data.read_word(0x0f90) == 0x6cac &&
			data.read_word(0x0f91) == 0x6cac &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI LTD example copies Smem into T and the next data address in one cycle");
		program.write_word(0x05e2, 0x4c93); // LTD *AR3+.
		data.write_word(0x0f90, 0x1234);
		data.write_word(0x0f91, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f90);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(668);
	}
	if (m_phase == 668)
	{
		expect_opcode(0x4c93,
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f91 &&
			data.read_word(0x0f91) == 0x1234 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"LTD postincrements only after writing the original address successor");
		program.write_word(0x05e2, 0xf43f); // SUB A>>1,A.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffffffcULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(669);
	}
	if (m_phase == 669)
	{
		expect_opcode(0xf43f,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffffeULL &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUB A>>1,A sign-fills the 40-bit source when SXM is set in one cycle");
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffffffcULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(670);
	}
	if (m_phase == 670)
	{
		expect_opcode(0xf43f,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7ffffffffeULL &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUB A>>1,A zero-fills the 40-bit source when SXM is clear in one cycle");
		program.write_word(0x05e2, 0xf165); // XOR #lk,16,A,B.
		program.write_word(0x05e3, 0x00ff);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff12340000ULL}, {cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(671);
	}
	if (m_phase == 671)
	{
		expect_opcode(0xf165,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12340000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff12cb0000ULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"XOR #lk,16,A,B preserves A and carry, updates B, and takes two cycles");
		program.write_word(0x05e2, 0xb43a); // MACR *AR5,*AR4+,A,A.
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f90, 0);
		data.write_word(0x0f91, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffffff8000ULL}, {cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_AR5, 0x0f90},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(672);
	}
	if (m_phase == 672)
	{
		expect_opcode(0xb43a,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MACR rounds negative half-word exactly to zero in one cycle");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffffff7fffULL}, {cpu_regs::STATE_AR4, 0x0f91}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(673);
	}
	if (m_phase == 673)
	{
		expect_opcode(0xb43a,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff0000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MACR rounds just below negative half-word to minus one word in one cycle");
		program.write_word(0x05e2, 0x798b); // MACP *AR3-,0700,B.
		write_words(program, {
			{0x05e3, 0x0700}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1}, {0x0700, 0x1234},
		});
		data.write_word(0x0f90, 0x0055);
		data.write_word(0x0f91, 0x0066);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x770000}, {cpu_regs::STATE_T, 8},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0f90},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(674);
	}
	if (m_phase == 674)
	{
		expect_opcode(0x798b,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f8f &&
			data.read_word(0x0f91) == 0x0066 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"MACP B preserves A and the successor while multiplying in three cycles");
		program.write_word(0x05e2, 0x7b8b); // MACD *AR3-,0700,B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x770000}, {cpu_regs::STATE_T, 8}, {cpu_regs::STATE_AR3, 0x0f90},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(675);
	}
	if (m_phase == 675)
	{
		expect_opcode(0x7b8b,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f8f &&
			data.read_word(0x0f91) == 0x0055 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"MACD B preserves A, copies the successor, and costs three cycles");
		program.write_word(0x05e2, 0x79f8); // MACP *(absolute),0700,B.
		write_words(program, {
			{0x05e3, 0x0f90}, {0x05e4, 0x0700}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0f90, 0x0055);
		data.write_word(0x0f91, 0x0066);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x770000);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(676);
	}
	if (m_phase == 676)
	{
		expect_opcode(0x79f8,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
			data.read_word(0x0f91) == 0x0066 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
			"absolute MACP consumes data address before coefficient in four cycles");
		program.write_word(0x05e2, 0x7bf8); // MACD *(absolute),0700,B.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x770000);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(677);
	}
	if (m_phase == 677)
	{
		expect_opcode(0x7bf8,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
			data.read_word(0x0f91) == 0x0055 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
			"absolute MACD copies the successor and costs four cycles");
		program.write_word(0x05e2, 0x79ea); // MACP *+AR2(5),0700,B.
		program.write_word(0x05e3, 0x0700);
		program.write_word(0x05e4, 5);
		data.write_word(0x0f05, 0x0055);
		data.write_word(0x0f06, 0x0066);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x770000}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(678);
	}
	if (m_phase == 678)
	{
		expect_opcode(0x79ea,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
			data.read_word(0x0f06) == 0x0066 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
			"long-offset MACP consumes coefficient before offset in four cycles");
		program.write_word(0x05e2, 0x7bea); // MACD *+AR2(5),0700,B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x770000}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(679);
	}
	if (m_phase == 679)
	{
		expect_opcode(0x7bea,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7d0b44 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0055 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
			data.read_word(0x0f06) == 0x0055 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
			"long-offset MACD copies successor after AR2 preupdate in four cycles");
		program.write_word(0x05e2, 0x4cf8); // LTD *(absolute).
		program.write_word(0x05e3, 0x0f90);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0f90, 0x6cac);
		data.write_word(0x0f91, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(680);
	}
	if (m_phase == 680)
	{
		expect_opcode(0x4cf8,
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x6cac &&
			data.read_word(0x0f90) == 0x6cac &&
			data.read_word(0x0f91) == 0x6cac &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"absolute LTD copies the selected word to its successor in two cycles");
		program.write_word(0x05e2, 0x4cea); // LTD *+AR2(5).
		program.write_word(0x05e3, 5);
		data.write_word(0x0f05, 0x1234);
		data.write_word(0x0f06, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(681);
	}
	if (m_phase == 681)
	{
		expect_opcode(0x4cea,
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
			data.read_word(0x0f05) == 0x1234 &&
			data.read_word(0x0f06) == 0x1234 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"long-offset LTD preupdates AR2 and copies to its successor in two cycles");
		program.write_word(0x05e2, 0x1045); // LD 45h,A, direct Smem.
		program.write_word(0x05e3, 0x8046); // STL A,46h, direct Smem.
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0945, 0x1234);
		data.write_word(0x0946, 0);
		data.write_word(0x0f90, 0xabcd);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0012); // DP 12h.
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // CPL clear.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_SP, 0x0f00}, {cpu_regs::STATE_AR5, 0x0f90}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(682);
	}
	if (m_phase == 682)
	{
		expect_opcode(0x1045,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			data.read_word(0x0946) == 0x1234 &&
			data.read_word(0x0f90) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0f00 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"direct Smem load/store use DP:offset with CPL clear in one cycle each");
		data.write_word(0x0f45, 0x5678);
		data.write_word(0x0f46, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4100); // CPL set.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(683);
	}
	if (m_phase == 683)
	{
		expect_opcode(0x1045,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x5678 &&
			data.read_word(0x0f46) == 0x5678 &&
			data.read_word(0x0946) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0f00 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"direct Smem load/store use SP+offset with CPL set in one cycle each");
		program.write_word(0x05e2, 0x6b45); // ADDM #1, 45h.
		program.write_word(0x05e3, 1);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0945, 0x1234);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(684);
	}
	if (m_phase == 684)
	{
		expect_opcode(0x6b45,
			data.read_word(0x0945) == 0x1235 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"direct ADDM uses DP:offset without modifying an AR");
		program.write_word(0x05e2, 0x4c45); // LTD 45h.
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f45, 0x5678);
		data.write_word(0x0f46, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST1, 0x4100}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(685);
	}
	if (m_phase == 685)
	{
		expect_opcode(0x4c45,
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5678 &&
			data.read_word(0x0f46) == 0x5678 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0f00 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"direct LTD uses SP+offset without modifying an AR");
		program.write_word(0x05e2, 0x2045); // MPY 45h, A.
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0945, 4);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_T, 3}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(686);
	}
	if (m_phase == 686)
	{
		expect_opcode(0x2045,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 12 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"direct MPY uses DP:offset and costs one cycle");
		program.write_word(0x05e2, 0x2945); // MAC 45h, B.
		data.write_word(0x0f45, 0xfffc);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST1, 0x4100}, {cpu_regs::STATE_B, 16}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(687);
	}
	if (m_phase == 687)
	{
		expect_opcode(0x2945,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 4 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 12 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f90 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0f00 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"direct MAC uses SP+offset and costs one cycle");
		program.write_word(0x05e2, 0x26ea); // SQUR *+AR2(5), A.
		program.write_word(0x05e3, 5);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0f05, 0xfffd);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_T, 0},
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(688);
	}
	if (m_phase == 688)
	{
		expect_opcode(0x26ea,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 9 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffd &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"long-offset SQUR publishes T, preupdates AR, and costs two cycles");
		program.write_word(0x05e2, 0x271e); // SQUR 30, B (TI SPRU172C example).
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x031e, 0x000f);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST0, 0x0006}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_T, 3},
			{cpu_regs::STATE_B, 0x01f4}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(689);
	}
	if (m_phase == 689)
	{
		expect_opcode(0x271e,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00e1 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x000f &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI direct SQUR example squares 15 into B in one cycle");
		program.write_word(0x05e2, 0x391e); // SQURA 30, B (TI SPRU172C example).
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x03200000}, {cpu_regs::STATE_T, 3}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(690);
	}
	if (m_phase == 690)
	{
		expect_opcode(0x391e,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x032000e1 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x000f &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI direct SQURA example adds 15 squared in one cycle");
		program.write_word(0x05e2, 0x3a09); // SQURS 9, A (TI SPRU172C example).
		data.write_word(0x0309, 0x1234);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x014b5db0}, {cpu_regs::STATE_T, 0x8765}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(691);
	}
	if (m_phase == 691)
	{
		expect_opcode(0x3a09,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0320 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI direct SQURS example subtracts 1234h squared in one cycle");
		program.write_word(0x05e2, 0x0605); // ADDC 5, A.
		data.write_word(0x0405, 0xffff);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0808); // C=1, DP=8.
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM must be ignored.
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(692);
	}
	if (m_phase == 692)
	{
		expect_opcode(0x0605,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"direct ADDC zero-extends FFFFh and consumes carry despite SXM");
		program.write_word(0x05e2, 0x0e05); // SUBB 5, A (TI SPRU172C example).
		data.write_word(0x0405, 6);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0008); // C=0, DP=8.
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 6);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(693);
	}
	if (m_phase == 693)
	{
		expect_opcode(0x0e05,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffffffULL &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI direct SUBB example subtracts inverted carry as borrow");
		program.write_word(0x05e2, 0x6b94); // ADDM #FFF8h, *AR4+ (TI example).
		program.write_word(0x05e3, 0xfff8);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0100, 0x8007);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0100);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0300); // SXM, OVM.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(694);
	}
	if (m_phase == 694)
	{
		expect_opcode(0x6b94,
			data.read_word(0x0100) == 0x8000 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0101 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"TI ADDM example saturates negative overflow and costs two cycles");
		m_phase = 9223;
	}
	if (m_phase >= 9223 && m_phase <= 9225)
	{
		if (m_phase != 9223)
			expect_opcode(m_phase == 9224 ? 0x6b84 : 0x6b8c,
				data.read_word(0x0100) == 0x8000 && data.read_word(0x00ff) == 0xbeef && data.read_word(0x0101) == 0xcafe &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == (m_phase == 9224 ? 0x0100 : 0x00ff) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0c00 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0300 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 AR4 ADDM saturates before stationary/decrement pointer update, preserves neighbors/accumulators and costs two cycles");
		if (m_phase < 9225)
		{
			program.write_word(0x05e2, m_phase == 9223 ? 0x6b84 : 0x6b8c);
			data.write_word(0x00ff, 0xbeef);
			data.write_word(0x0100, 0x8007);
			data.write_word(0x0101, 0xcafe);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR4, 0x0100}, {cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678},
				{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xcdb9); // ST A,*AR3+ || MPY *AR5+,B.
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0200, 0x1111);
		data.write_word(0x0300, 0x4000);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff84211234ULL}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x4000},
			{cpu_regs::STATE_AR3, 0x0200}, {cpu_regs::STATE_AR5, 0x0300},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0140); // SXM, FRCT.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(695);
	}
	if (m_phase == 695)
	{
		expect_opcode(0xcdb9,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff84211234ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x20000000 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4000 &&
			data.read_word(0x0200) == 0x8421 &&
			data.read_word(0x0300) == 0x4000 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0201 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0301 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI ST||MPY stores old A and squares fractional 4000h into B in one cycle");
		program.write_word(0x05e2, 0xcfb9); // ST B,*AR3+ || MPY *AR5+,B.
		data.write_word(0x0200, 0);
		data.write_word(0x0300, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xabcdef}, {cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_T, 2},
			{cpu_regs::STATE_AR3, 0x0200}, {cpu_regs::STATE_AR5, 0x0300}, {cpu_regs::STATE_ST1, 0x0100},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(696);
	}
	if (m_phase == 696)
	{
		expect_opcode(0xcfb9,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xabcdef &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 6 &&
			data.read_word(0x0200) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0201 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0301 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ST||MPY stores the old destination before replacing it with the product");
		program.write_word(0x05e2, 0xc131); // ST A,*AR3 || ADD *AR5,B.
		data.write_word(0x0200, 0);
		data.write_word(0x0300, 2);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_B, 0xdead}, {cpu_regs::STATE_AR3, 0x0200},
			{cpu_regs::STATE_AR5, 0x0300}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(697);
	}
	if (m_phase == 697)
	{
		expect_opcode(0xc131,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12365678 &&
			data.read_word(0x0200) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0300 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ST||ADD uses opposite accumulator and stores old source in one cycle");
		program.write_word(0x05e2, 0xc5f5); // ST A,*AR3- || SUB *AR5+0%,B.
		data.write_word(0x01ff, 0x1111);
		data.write_word(0x0300, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff84210000ULL}, {cpu_regs::STATE_B, 0x0010000001ULL}, {cpu_regs::STATE_AR0, 2},
			{cpu_regs::STATE_AR3, 0x01ff}, {cpu_regs::STATE_AR5, 0x0300}, {cpu_regs::STATE_BK, 4},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); // SST clear.
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0101); // SXM, ASM=1.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(698);
	}
	if (m_phase == 698)
	{
		expect_opcode(0xc5f5,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff84210000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffbe00000ULL &&
			data.read_word(0x01ff) == 0x0842 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x01fe &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0302 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI ST||SUB example subtracts opposite accumulator in one cycle");
		program.write_word(0x05e2, 0xca31); // ST B,*AR3 || LD *AR5,A.
		data.write_word(0x0200, 0);
		data.write_word(0x0300, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_AR3, 0x0200},
			{cpu_regs::STATE_AR5, 0x0300}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ST1, 0x0100},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(699);
	}
	if (m_phase == 699)
	{
		expect_opcode(0xca31,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80010000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345678 &&
			data.read_word(0x0200) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0300 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ST||LD sign-extends Xmem into A and stores B in one cycle");
		program.write_word(0x05e2, 0xe411); // ST A,*AR3 || LD *AR3,T.
		data.write_word(0x0200, 0x80ff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR3, 0x0200},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(700);
	}
	if (m_phase == 700)
	{
		expect_opcode(0xe411,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x80ff &&
			data.read_word(0x0200) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ST||LD T reads Xmem before writing the same Ymem cell");
		program.write_word(0x05e2, 0xe4e9); // ST A,*AR3+ || LD *AR4+0%,T.
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x87654321);
		data.write_word(0x0203, 0x80ff);
		data.write_word(0x0300, 0xbeef);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR3, 0x0300},
			{cpu_regs::STATE_AR4, 0x0203}, {cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_ST0, 0x0aa5},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6503);
	}
	if (m_phase == 6503 || m_phase == 6533)
	{
		bool const store_b = m_phase == 6533;
		expect_opcode(store_b ? 0xe6e9 : 0xe4e9,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x87654321 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x80ff &&
			data.read_word(0x0300) == (store_b ? 0x8765 : 0x1234) && data.read_word(0x0203) == 0x80ff &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0301 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0200 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 parallel load T selects old A/B store, independently advances Y and wraps circular X in one cycle");
		if (!store_b)
		{
			program.write_word(0x05e2, 0xe6e9);
			data.write_word(0x0300, 0xbeef);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR3, 0x0300}, {cpu_regs::STATE_AR4, 0x0203},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(6533);
		}
		program.write_word(0x05e2, 0xc131); // ST A,*AR3 || ADD *AR5,B.
		data.write_word(0x0300, 1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // Clear carry on ADD.
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0300); // SXM, OVM.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x0200}, {cpu_regs::STATE_AR5, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(701);
	}
	if (m_phase == 701)
	{
		expect_opcode(0xc131,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7fffffff &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0e00) == 0x0200 &&
			data.read_word(0x0200) == 0x7fff &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ST||ADD saturates B, sets OVB and clears carry without altering the store");
		program.write_word(0x05e2, 0xc531); // ST A,*AR3 || SUB *AR5,B.
		data.write_word(0x0300, 0x8000);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x00010000);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0300); // SXM, OVM.
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x0200}, {cpu_regs::STATE_AR5, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(702);
	}
	if (m_phase == 702)
	{
		expect_opcode(0xc531,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80000000ULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0e00) == 0x0a00 &&
			data.read_word(0x0200) == 1 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ST||SUB saturates negative B, sets OVB and carry, and stores old A");
		program.write_word(0x05e2, 0x0483); // ADD *AR3,TS,A.
		data.write_word(0x0200, 0x0100);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x10}, {cpu_regs::STATE_T, 4}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR3, 0x0200}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(703);
	}
	if (m_phase == 703)
	{
		expect_opcode(0x0483,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1010 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ADD Smem,TS uses T low six bits and costs one cycle");
		program.write_word(0x05e2, 0x0583); // ADD *AR3,TS,B.
		data.write_word(0x0200, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x10000);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xabff); // Low six bits encode -1.
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(7031);
	}
	if (m_phase == 7031)
	{
		expect_opcode(0x0583,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xc000 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0xabff &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ADD Smem,TS,B sign-fills negative shifts, ignores upper T bits and preserves A");
		program.write_word(0x05e2, 0x0d83); // SUB *AR3,TS,B.
		data.write_word(0x0200, 0x0100);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 4}, {cpu_regs::STATE_B, 0x1000}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(704);
	}
	if (m_phase == 704)
	{
		expect_opcode(0x0d83,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0800 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUB Smem,TS sets no-borrow carry and costs one cycle");
		program.write_word(0x05e2, 0x0c83); // SUB *AR3,TS,A.
		data.write_word(0x0200, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x10000}, {cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_T, 0xabff},
			{cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear: zero fill.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(7041);
	}
	if (m_phase == 7041)
	{
		expect_opcode(0x0c83,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xc000 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345678 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0800 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0xabff &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUB Smem,TS,A zero-fills with SXM clear, sets no-borrow carry and preserves B");
		program.write_word(0x05e2, 0x1583); // LD *AR3,TS,B.
		data.write_word(0x0200, 0xfedc);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 8}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(705);
	}
	if (m_phase == 705)
	{
		expect_opcode(0x1583,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffedc00ULL &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI LD Smem,TS example sign-extends fedc and shifts by eight");
		program.write_word(0x05e2, 0x1483); // LD *AR3,TS,A.
		data.write_word(0x0200, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x003f); // TS=-1.
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(706);
	}
	if (m_phase == 706)
	{
		expect_opcode(0x1483,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffc000ULL &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"LD Smem,TS sign-fills a negative T shift in one cycle");
		program.write_word(0x05e2, 0x9083); // ADD *AR2+,3,A.
		data.write_word(0x0300, 2);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_AR2, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(707);
	}
	if (m_phase == 707)
	{
		expect_opcode(0x9083,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 17 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0301 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ADD Xmem,SHFT shifts Xmem and postincrements its pointer in one cycle");
		program.write_word(0x05e2, 0x9304); // SUB *AR2,4,B.
		data.write_word(0x0300, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR2, 0x0300},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(708);
	}
	if (m_phase == 708)
	{
		expect_opcode(0x9304,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7fff0 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0300 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUB Xmem,SHFT sign-extends Xmem and records borrow in one cycle");
		program.write_word(0x05e2, 0xf486); // MAX A.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xfffffffff6ULL}, {cpu_regs::STATE_B, 0xffffffffcbULL}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(709);
	}
	if (m_phase == 709)
	{
		expect_opcode(0xf486,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffff6ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffffcbULL &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI MAX A chooses the larger signed accumulator and clears carry");
		program.write_word(0x05e2, 0xf586); // MAX B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x55}, {cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(710);
	}
	if (m_phase == 710)
	{
		expect_opcode(0xf586,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x55 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MAX B preserves B and sets carry when B is larger");
		program.write_word(0x05e2, 0xf487); // MIN A.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(711);
	}
	if (m_phase == 711)
	{
		expect_opcode(0xf487,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI MIN tie chooses B and sets carry");
		program.write_word(0x05e2, 0xf587); // MIN B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff00000000ULL}, {cpu_regs::STATE_B, 0x7fffffff}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(712);
	}
	if (m_phase == 712)
	{
		expect_opcode(0xf587,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff00000000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff00000000ULL &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MIN B compares the signed guard byte and selects A in one cycle");
		program.write_word(0x05e2, 0xf48e); // EXP A.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffffffffcbULL}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(713);
	}
	if (m_phase == 713)
	{
		expect_opcode(0xf48e,
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x19 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffffcbULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI EXP A example computes 25 redundant sign shifts in one cycle");
		program.write_word(0x05e2, 0xf58e); // EXP B.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0785432105ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(714);
	}
	if (m_phase == 714)
	{
		expect_opcode(0xf58e,
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffc &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0785432105ULL &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI EXP B example reports negative exponent for guard-byte content");
		program.write_word(0x05e2, 0xf48f); // NORM A.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffff001ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x13);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(715);
	}
	if (m_phase == 715)
	{
		expect_opcode(0xf48f,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80080000ULL &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI NORM A example shifts negative A left by 19 in one cycle");
		program.write_word(0x05e2, 0xf68f); // NORM B,A.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffff001ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x210a0a0a0aULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xfff9); // TS=-7.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(716);
	}
	if (m_phase == 716)
	{
		expect_opcode(0xf68f,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0042141414ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x210a0a0a0aULL &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI NORM B,A example shifts guarded B right by seven");
		m_phase = 9247;
	}
	if (m_phase >= 9247 && m_phase <= 9249)
	{
		if (m_phase != 9247)
			expect_opcode(0xf78f,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (m_phase == 9248 ? 0xff80080000ULL : 0x0042141414ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == (m_phase == 9248 ? 0x13 : 0xfff9) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1800 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0100 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 NORM B left/right uses signed six-bit T, preserves A/T/carry/status and costs one cycle");
		if (m_phase < 9249)
		{
			program.write_word(0x05e2, 0xf78f);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, m_phase == 9247 ? 0xfffffff001ULL : 0x210a0a0a0aULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, m_phase == 9247 ? 0x13 : 0xfff9);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1800);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xf48e); // EXP A with zero source.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_T, 0x1234}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(717);
	}
	if (m_phase == 717)
	{
		expect_opcode(0xf48e,
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"EXP zero source publishes zero exponent");
		program.write_word(0x05e2, 0xf48f); // NORM A with OVM.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x40000000);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 1);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0300); // SXM, OVM.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(718);
	}
	if (m_phase == 718)
	{
		expect_opcode(0xf48f,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"NORM left shift saturates on OVM and preserves carry in one cycle");
		program.write_word(0x05e2, 0xf59f); // RND A,B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffffffffffULL}, {cpu_regs::STATE_B, 1}, {cpu_regs::STATE_ST0, 0x0a00},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(719);
	}
	if (m_phase == 719)
	{
		expect_opcode(0xf59f,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffffffULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7fff &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0a00 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI RND A,B example adds 8000h without changing status flags");
		program.write_word(0x05e2, 0xf49f); // RND A.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200); // OVM.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(720);
	}
	if (m_phase == 720)
	{
		expect_opcode(0xf49f,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0a00 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI RND A example saturates under OVM without changing flags");
		program.write_word(0x05e2, 0xf583); // SAT B.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x7123456789ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // OVM clear.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(721);
	}
	if (m_phase == 721)
	{
		expect_opcode(0xf583,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7fffffff &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0a00 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI SAT B example clamps positive guard overflow with OVM clear");
		program.write_word(0x05e2, 0xf483); // SAT A.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xf812345678ULL}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(722);
	}
	if (m_phase == 722)
	{
		expect_opcode(0xf483,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80000000ULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI SAT A example clamps negative guard overflow and sets OVA");
		program.write_word(0x05e2, 0xf583); // SAT B without overflow.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x00123456}, {cpu_regs::STATE_ST0, 0x0a00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(723);
	}
	if (m_phase == 723)
	{
		expect_opcode(0xf583,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00123456 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0800 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"TI SAT B in-range example clears OVB and preserves carry");
		program.write_word(0x05e2, 0x6283); // MPY *AR3,#4,A.
		program.write_word(0x05e3, 4);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0200, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_T, 0x1234}, {cpu_regs::STATE_AR3, 0x0200},
			{cpu_regs::STATE_AR6, 0x0a03},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0040); // FRCT.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(724);
	}
	if (m_phase == 724)
	{
		expect_opcode(0x6283,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 24 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"MPY Smem,#lk publishes T, doubles under FRCT and costs two cycles");
		program.write_word(0x05e2, 0x6383); // MPY *AR3,#fffe,B.
		program.write_word(0x05e3, 0xfffe);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x123456}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(9400);
	}
	if (m_phase == 9400 || m_phase == 9401)
	{
		const bool fractional = m_phase == 9401;
		expect_opcode(0x6383,
			m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(fractional ? 0xfffffffff4ULL : 0xfffffffffaULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 24 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (fractional ? 0x0040 : 0) &&
			data.read_word(0x0200) == 3 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"immediate MPY to B preserves A/source/pointer/carry, sign extends and doubles only under FRCT in two cycles");
		if (!fractional)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_ST1, 0x0040}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(9401);
		}
		program.write_word(0x05e2, 0x6683); // MAC *AR3,#fffe,B,A.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 100}, {cpu_regs::STATE_B, 10}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(9402);
	}
	if (m_phase == 9402 || m_phase == 9403)
	{
		const bool same_destination = m_phase == 9403;
		expect_opcode(same_destination ? 0x6783 : 0x6683,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 4 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(same_destination ? 0xfffffffffeULL : 10) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (same_destination ? 0x0040 : 0) &&
			data.read_word(0x0200) == 3 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"immediate MAC selects B addend independently of A/B destination, applies FRCT and preserves source/pointer/carry in two cycles");
		if (!same_destination)
		{
			program.write_word(0x05e2, 0x6783); // MAC *AR3,#fffe,B,B.
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_ST1, 0x0040}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(9403);
		}
		program.write_word(0x05e2, 0x6683); // MAC *AR3,#2,B,A, positive overflow.
		program.write_word(0x05e3, 2);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x7ffffffe}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(9410);
	}
	if (m_phase >= 9410 && m_phase <= 9413)
	{
		const unsigned index = m_phase - 9410;
		const bool destination_b = index >= 2;
		const bool saturate = index & 1;
		const u64 result = saturate ? 0x7fffffffULL : 0x80000004ULL;
		expect_opcode(destination_b ? 0x6783 : 0x6683,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (destination_b ? 0x1234 : result) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (destination_b ? result : 0x7ffffffeULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (destination_b ? 0x0a00 : 0x0c00) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (saturate ? 0x0200 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			data.read_word(0x0200) == 3 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"immediate MAC overflow belongs only to its destination, preserves carry/source and saturates only with OVM in two cycles");
		if (index < 3)
		{
			const unsigned next = index + 1;
			program.write_word(0x05e2, next >= 2 ? 0x6783 : 0x6683);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x7ffffffe}, {cpu_regs::STATE_ST0, 0x0800},
				{cpu_regs::STATE_ST1, (next & 1) ? 0x0200 : 0}, {cpu_regs::STATE_AR6, 0x0a03},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(9410 + next);
		}
		program.write_word(0x05e2, 0xf166); // MPY #fffe,B.
		program.write_word(0x05e3, 0xfffe);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x2000}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(725);
	}
	if (m_phase == 725)
	{
		expect_opcode(0xf166,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffc000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x2000 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"TI MPY #fffe,B example uses signed immediate and keeps T");
		program.write_word(0x05e2, 0x62f8); // MPY *(0200),#5,A.
		write_words(program, {
			{0x05e3, 0x0200}, {0x05e4, 5}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(726);
	}
	if (m_phase == 726)
	{
		expect_opcode(0x62f8,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 15 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"MPY absolute Smem consumes address before immediate and costs three cycles");
		program.write_word(0x05e2, 0x629b); // Invalid read: short MOD=3 is write-only.
		program.write_word(0x05e3, 4);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x01ff, 7);
		data.write_word(0x0200, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR3, 0x01ff},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(727);
	}
	if (m_phase == 727)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x01ff && m_port_writes == 1,
			"MPY rejects write-only short preincrement before operand effects and the following PORTW");
		m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
		program.write_word(0x05e2, 0xf480); // ADD A,ASM,A.
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffff00ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x011f); // SXM, ASM=-1.
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(728);
	}
	if (m_phase == 728)
	{
		expect_opcode(0xf480,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffe80ULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ADD A,ASM sign-fills a right shift and records carry in one cycle");
		program.write_word(0x05e2, 0xf681); // SUB B,ASM,A.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x40);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0101); // SXM, ASM=1.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(729);
	}
	if (m_phase == 729)
	{
		expect_opcode(0xf681,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffff90ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x40 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUB B,ASM,A uses B as source and records borrow in one cycle");
		program.write_word(0x05e2, 0xf480); // ADD A,ASM,A.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x123456789aULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0103); // SXM, ASM=3.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(40000);
	}
	if (m_phase == 40000)
	{
		expect_opcode(0xf480,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 9 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x123456789aULL &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ADD A,ASM left-shifts its source without modifying B in one cycle");
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, ASM=0, OVM clear.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(40001);
	}
	if (m_phase >= 40001 && m_phase <= 40004)
	{
		const unsigned index = m_phase - 40001;
		const bool negative = index >= 2;
		const bool saturate = BIT(index, 0);
		const u64 expected = negative ?
			(saturate ? 0xff80000000ULL : 0xff00000000ULL) :
			(saturate ? 0x007fffffffULL : 0x00fffffffeULL);
		expect_opcode(0xf480,
			m_cpu->state_int(tms320c54x_device::STATE_A) == expected &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x123456789aULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0e00) == (negative ? 0x0c00 : 0x0400) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ADD A,ASM distinguishes signed overflow, carry and OVM saturation in one cycle");
		if (index < 3)
		{
			const unsigned next = index + 1;
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, next >= 2 ? 0xff80000000ULL : 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100 | (BIT(next, 0) ? 0x0200 : 0));
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x ASM accumulator add conformance: PASS left_shift=1 overflow_variants=4\n");
		program.write_word(0x05e2, 0xf581); // SUB A,ASM,B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffffffffffULL}, {cpu_regs::STATE_B, 0x7fffffff}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(40005);
	}
	if (m_phase >= 40005 && m_phase <= 40008)
	{
		const unsigned index = m_phase - 40005;
		const bool negative = index >= 2;
		const bool saturate = BIT(index, 0);
		const u64 expected = negative ?
			(saturate ? 0xff80000000ULL : 0xff7fffffffULL) :
			(saturate ? 0x007fffffffULL : 0x0080000000ULL);
		expect_opcode(0xf581,
			m_cpu->state_int(tms320c54x_device::STATE_B) == expected &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (negative ? 1 : 0xffffffffffULL) &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0e00) == (negative ? 0x0a00 : 0x0200) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SUB A,ASM,B owns B overflow, preserves A and distinguishes OVM saturation in one cycle");
		if (index < 3)
		{
			const unsigned next = index + 1;
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, next >= 2 ? 1 : 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, next >= 2 ? 0xff80000000ULL : 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100 | (BIT(next, 0) ? 0x0200 : 0));
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x ASM accumulator subtract conformance: PASS overflow_variants=4 source_preserved=1\n");
		program.write_word(0x05e2, 0xf580); // ADD A,ASM,B; first vector ASM=-16, SXM clear.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xfffffeffffULL}, {cpu_regs::STATE_B, 17}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0010}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(41000);
	}
	if (m_phase >= 41000 && m_phase < 41512)
	{
		const unsigned index = m_phase - 41000;
		const bool subtract = BIT(index, 0);
		const bool sign_fill = BIT(index, 1);
		const int shift = int((index >> 2) & 31) - 16;
		const bool saturate = BIT(index, 7);
		const bool sticky = BIT(index, 8);
		// Signed right shifts round toward minus infinity; avoid depending
		// on the host's negative right-shift behavior in the reference result.
		const s64 operand = shift >= 0 ? -65537LL * (1LL << shift) :
			(sign_fill ? -((65537LL + (1LL << -shift) - 1) / (1LL << -shift)) :
			s64(0xfffffeffffULL / (1ULL << -shift)));
		const s64 result = subtract ? 17 - operand : 17 + operand;
		const bool overflow = result > 0x7fffffffLL || result < -0x80000000LL;
		const s64 stored = saturate && overflow ?
			(result < 0 ? -0x80000000LL : 0x7fffffffLL) : result;
		const bool carry = subtract ? 17 >= u32(operand) : u64(17) + u32(operand) > 0xffffffffULL;
		const u16 status = (carry ? 0x0800 : 0) |
			(overflow ? 0x0200 : 0) | (sticky ? 0x0600 : 0);
		expect_opcode(subtract ? 0xf581 : 0xf580,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffeffffULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (u64(stored) & 0xffffffffffULL) &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0e00) == status &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) ==
				((shift & 0x1f) | (sign_fill ? 0x0100 : 0) | (saturate ? 0x0200 : 0)) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ASM ADD/SUB covers signed shifts, SXM fill, saturation and sticky overflow in one cycle");
		if (index < 511)
		{
			const unsigned next = index + 1;
			program.write_word(0x05e2, BIT(next, 0) ? 0xf581 : 0xf580);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xfffffeffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 17);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, BIT(next, 8) ? 0x0600 : 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
				((int((next >> 2) & 31) - 16) & 0x1f) | (BIT(next, 1) ? 0x0100 : 0) |
				(BIT(next, 7) ? 0x0200 : 0));
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x ASM arithmetic shifts: PASS variants=512 shifts=32 sxm_settings=2 ovm_settings=2 sticky_settings=2\n");
		program.write_word(0x05e2, 0xf58c); // MPYA T,B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0080000000ULL}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 2},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(730);
	}
	if (m_phase == 730)
	{
		expect_opcode(0xf58c,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0080000000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10000 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MPYA T uses signed 17-bit A high word in one cycle");
		program.write_word(0x05e2, 0x3183); // MPYA *AR3.
		data.write_word(0x0200, 2);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x1234}, {cpu_regs::STATE_AR3, 0x0200},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(731);
	}
	if (m_phase == 731)
	{
		expect_opcode(0x3183,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10000 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 2 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MPYA Smem publishes T and uses the same signed 17-bit A high word");
		program.write_word(0x05e2, 0xf488); // MACA T,A,A.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0080000000ULL}, {cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_T, 1},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(732);
	}
	if (m_phase == 732)
	{
		expect_opcode(0xf488,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0080008000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MACA T preserves old A as multiplicand and accumulator source");
		program.write_word(0x05e2, 0xf689); // MACAR T,B,A.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xfffffe0000ULL}, {cpu_regs::STATE_B, 0x800c}, {cpu_regs::STATE_T, 4},
			{cpu_regs::STATE_ST0, 0x1800}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6558);
	}
	if (m_phase == 6558 || m_phase == 6559)
	{
		const bool fractional = m_phase == 6559;
		expect_opcode(0xf689,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (fractional ? 0 : 0x10000) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x800c &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 4 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (fractional ? 0x0040 : 0) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 MACAR uses signed old A high word and old B source, doubles under FRCT before rounding and preserves B/T/status in one cycle");
		if (!fractional)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0xfffffe0000ULL}, {cpu_regs::STATE_ST1, 0x0040}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(6559);
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
		program.write_word(0x05e2, 0xf48b); // MASAR T,A,A.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x10000}, {cpu_regs::STATE_T, 0x8000}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(733);
	}
	if (m_phase == 733)
	{
		expect_opcode(0xf48b,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x20000 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"MASAR T subtracts a negative 17-bit product and rounds in one cycle");
		program.write_word(0x05e2, 0xe211); // SQDST *AR3,*AR3.
		data.write_word(0x0200, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0080010000ULL}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR3, 0x0200},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(734);
	}
	if (m_phase == 734)
	{
		expect_opcode(0xe211,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x40010001 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SQDST squares signed 17-bit A high word before replacing A");
		program.write_word(0x05e2, 0xf48d); // SQUR A,A.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0080010000ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(735);
	}
	if (m_phase == 735)
	{
		expect_opcode(0xf48d,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x40010001 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"SQUR A uses the full signed 17-bit high word in one cycle");
		program.write_word(0x05e2, 0xf167); // MAC #0345h,A,B.
		program.write_word(0x05e3, 0x0345);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1000);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x0400);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0040); // FRCT.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(736);
	}
	if (m_phase == 736)
	{
		expect_opcode(0xf167,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1000 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x001a3800 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x0400 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"MAC #lk uses T, FRCT and a separate source accumulator in two cycles");
		program.write_word(0x05e2, 0x6495); // MAC *AR5+,#1234h,A.
		program.write_word(0x05e3, 0x1234);
		data.write_word(0x0100, 0x5678);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1000}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_AR5, 0x0100}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(737);
	}
	if (m_phase == 737)
	{
		expect_opcode(0x6495,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x06261060 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5678 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0101 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"MAC Smem,#lk writes T and postincrements AR5 in two cycles");
		program.write_word(0x05e2, 0x65f8); // MAC *(0200h),#5,A,B.
		write_words(program, {
			{0x05e3, 0x0200}, {0x05e4, 5}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0200, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x100}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(738);
	}
	if (m_phase == 738)
	{
		expect_opcode(0x65f8,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x100 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10f &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"MAC absolute Smem fetches address before immediate with cycle surcharge");
		program.write_word(0x05e2, 0x649b); // Invalid read: short MOD=3 is write-only.
		program.write_word(0x05e3, 4);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x01ff, 7);
		data.write_word(0x0200, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x10}, {cpu_regs::STATE_AR3, 0x01ff}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(739);
	}
	if (m_phase == 739)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x01ff && m_port_writes == 1,
			"MAC rejects write-only short preincrement before operand effects and the following PORTW");
		m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
		program.write_word(0x05e2, 0xf160); // ADD #ffffh,16,A,B.
		program.write_word(0x05e3, 0xffff);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x00010000);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(740);
	}
	if (m_phase == 740)
	{
		expect_opcode(0xf160,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00010000 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ADD #lk,16 sign-extends with SXM, preserves source and sets carry");
		program.write_word(0x05e2, 0xf361); // SUB #1,16,B,B.
		program.write_word(0x05e3, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x00020000}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(741);
	}
	if (m_phase == 741)
	{
		expect_opcode(0xf361,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00010000 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"SUB #lk,16 subtracts from B and records no borrow in two cycles");
		program.write_word(0x05e2, 0xf060); // ADD #ffffh,16,A,A with SXM clear.
		program.write_word(0x05e3, 0xffff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(742);
	}
	if (m_phase == 742)
	{
		expect_opcode(0xf060,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00ffff0000ULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ADD #lk,16 zero-extends with SXM clear and records overflow");
		program.write_word(0x05e2, 0x65e3); // MAC *AR3(2),#4,A,B.
		write_words(program, {
			{0x05e3, 4}, {0x05e4, 2}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0202, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x10}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR3, 0x0200},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(743);
	}
	if (m_phase == 743)
	{
		expect_opcode(0x65e3,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1c &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"MAC long-offset fetches immediate then offset with a cycle surcharge");
		program.write_word(0x05e2, 0xf167); // MAC #8000h,A,B with FRCT and OVM.
		program.write_word(0x05e3, 0x8000);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x8000}, {cpu_regs::STATE_PMST, 0},
			{cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0240); // OVM, FRCT.
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(744);
	}
	if (m_phase == 744)
	{
		expect_opcode(0xf167,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x007fffffffULL &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0200) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"MAC immediate saturates a fractional product and records OVB");
		program.write_word(0x05e2, 0x4d83); // DELAY *AR3.
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0200, 0x6cac);
		data.write_word(0x0201, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 0x1234}, {cpu_regs::STATE_AR3, 0x0200}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(745);
	}
	if (m_phase == 745)
	{
		expect_opcode(0x4d83,
			data.read_word(0x0200) == 0x6cac &&
			data.read_word(0x0201) == 0x6cac &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"DELAY copies Smem to its successor without changing T or AR in one cycle");
		program.write_word(0x05e2, 0x4deb); // DELAY *+AR3(1), legal long MOD=13.
		program.write_word(0x05e3, 0x0001);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x01ff, 0x1111);
		data.write_word(0x0200, 0x2222);
		data.write_word(0x0201, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x01ff);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(746);
	}
	if (m_phase == 746)
	{
		expect_opcode(0x4deb,
			data.read_word(0x01ff) == 0x1111 &&
			data.read_word(0x0201) == 0x2222 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"Long-offset DELAY preincrements AR3 before its source read in two cycles");
		program.write_word(0x05e2, 0x4df8); // DELAY *(0200h).
		program.write_word(0x05e3, 0x0200);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0200, 0x55aa);
		data.write_word(0x0201, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(747);
	}
	if (m_phase == 747)
	{
		expect_opcode(0x4df8,
			data.read_word(0x0200) == 0x55aa &&
			data.read_word(0x0201) == 0x55aa &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"DELAY absolute consumes its address extension and surcharge cycle");
		program.write_word(0x05e2, 0x4de3); // DELAY *AR3(2).
		program.write_word(0x05e3, 2);
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0202, 0xabcd);
		data.write_word(0x0203, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(748);
	}
	if (m_phase == 748)
	{
		expect_opcode(0x4de3,
			data.read_word(0x0202) == 0xabcd &&
			data.read_word(0x0203) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"DELAY long-offset preserves AR3 and charges the extension cycle");
		program.write_word(0x05e2, 0x2483); // MPYU *AR3,A with FRCT and OVM.
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0200, 0xffff);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xffff);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0240); // OVM, FRCT.
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0200);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(749);
	}
	if (m_phase == 749)
	{
		expect_opcode(0x2483,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x007fffffffULL &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0xffff &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0200 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"MPYU doubles unsigned operands under FRCT and saturates with OVA");
		program.write_word(0x05e0, 0x75d6);
		program.write_word(0x05e1, 0x0124);
		program.write_word(0x05e2, 0xfe44); // RCD ANEQ
		program.write_word(0x05e3, 0xe800); // Delay word changes tested A.
		program.write_word(0x05e4, 0xe802);
		program.write_word(0x05e5, 0xe803); // Only the false path reaches this.
		write_words(program, {
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1}, {0x05ec, 0x75d6}, {0x05ed, 0x0124}, {0x05ee, 0xf5e1},
		});
		data.write_word(0x0300, 0x05ec);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(750);
	}
	return false;
}

} // namespace c54x_test
