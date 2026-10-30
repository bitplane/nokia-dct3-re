// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include <algorithm>

namespace c54x_test {

// TI instruction examples and arithmetic boundary sequences.
bool tms320c54x_test_state::check_atomic_reference_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 726)
	{
		program.write_word(0x05e2, 0xf480); // ADD A,ASM,A.
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffffffff00ULL}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x011f}, // SXM, ASM=-1.
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
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
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x10}, {cpu_regs::STATE_B, 0x40}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_ST1, 0x0101}, // SXM, ASM=1.
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
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
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_B, 0x123456789aULL}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0103}, // SXM, ASM=3.
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
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
		return resume_atomic_at(0x05e0, 40001);
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
	}
	return false;
}

} // namespace c54x_test
