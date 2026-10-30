// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_parallel.h"
#include "tms320c54x_test_product_examples.h"
#include "tms320c54x_test_memory_examples.h"
#include "tms320c54x_test_memory_continuations.h"
#include "tms320c54x_test_shift_examples.h"
#include "tms320c54x_test_dual_reference.h"
#include <algorithm>

namespace c54x_test {

// TI instruction examples and arithmetic boundary sequences.
bool tms320c54x_test_state::check_atomic_reference_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	static constexpr unsigned memory_phases[] = {
		570, 571, 572, 573, 574, 575, 576, 577, 578, 579, 580, 581, 582, 583, 584, 585, 586,
		9200, 9201, 9202, 9203, 9204, 587, 588, 589, 590, 591, 592, 593, 594, 595, 596, 597,
		6508, 6509, 6510, 6572, 9205, 598, 599, 600, 601, 6561, 6505, 6529
	};
	auto const memory_phase = std::find(std::begin(memory_phases), std::end(memory_phases), m_phase);
	if (memory_phase != std::end(memory_phases))
	{
		unsigned const index = unsigned(memory_phase - std::begin(memory_phases));
		auto const &test = memory_examples().at(index);
		check_case(*m_cpu, test, m_port_writes, m_last_port_cycle - m_first_port_cycle);
		expect_opcode(test.opcode, true, "Memory-operand literal reference");
		if (index + 1 < memory_examples().size())
		{
			return begin_atomic_case(memory_examples()[index + 1], memory_phases[index + 1]);
		}
	}
	if (m_phase >= 614 && m_phase <= 652)
	{
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
		write_words(data, {
			{0x0f90, 1}, {0x0f91, 2}, {0x0f92, 2}, {0x0f93, 2},
		});
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
		return begin_atomic_case(memory_continuations().front(), 665);
	}
	if (m_phase >= 665 && m_phase <= 694)
	{
		unsigned const index = m_phase - 665;
		auto const &cases = memory_continuations();
		auto const &test = cases[index];
		check_case(*m_cpu, test, m_port_writes, m_last_port_cycle - m_first_port_cycle);
		expect_opcode(test.opcode, true, "Literal memory/accumulator continuation");
		if (index + 1 < cases.size())
		{
			return begin_atomic_case(cases[index + 1], m_phase + 1);
		}
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
		return begin_atomic_case(parallel_case(0), 695);
	}
	if ((m_phase >= 695 && m_phase <= 702) || m_phase == 6503 || m_phase == 6533)
	{
		static constexpr unsigned phases[] = {695, 696, 697, 698, 699, 700, 6503, 6533, 701, 702};
		unsigned const index = std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases);
		auto const test = parallel_case(index);
		check_case(*m_cpu, test, m_port_writes, m_last_port_cycle - m_first_port_cycle);
		// Record opcode coverage only after the shared checks succeed.
		expect_opcode(test.opcode, true, "parallel store arithmetic reference case");
		if (index + 1 < std::size(phases))
		{
			return begin_atomic_case(parallel_case(index + 1), phases[index + 1]);
		}
		return begin_atomic_case(shift_examples().front(), 703);
	}
	static constexpr unsigned shift_phases[] = {703, 7031, 704, 7041, 705, 706, 707, 708, 709, 710, 711, 712, 713, 714, 715, 716, 9248, 9249, 717, 718, 719, 720, 721, 722, 723};
	auto const shift_phase = std::find(std::begin(shift_phases), std::end(shift_phases), m_phase);
	if (shift_phase != std::end(shift_phases))
	{
		unsigned const index = unsigned(shift_phase - std::begin(shift_phases));
		auto const &cases = shift_examples();
		auto const &test = cases[index];
		check_case(*m_cpu, test, m_port_writes, m_last_port_cycle - m_first_port_cycle);
		expect_opcode(test.opcode, true, "Literal shift/comparison/normalization reference");
		if (index + 1 < cases.size())
		{
			return begin_atomic_case(cases[index + 1], shift_phases[index + 1]);
		}
		write_words(program, {
			{0x05e2, 0x6283}, // MPY *AR3,#4,A.
			{0x05e3, 4}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0200, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_T, 0x1234}, {cpu_regs::STATE_AR3, 0x0200},
			{cpu_regs::STATE_AR6, 0x0a03},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0040); // FRCT.
		return resume_atomic_at(0x05e0, 724);
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
		return begin_atomic_case(product_examples()[0], 730);
	}
	if ((m_phase >= 730 && m_phase <= 738) || (m_phase >= 740 && m_phase <= 749) ||
		(m_phase >= 6558 && m_phase <= 6559))
	{
		unsigned const index = m_phase >= 6558 ? m_phase - 6555 :
			m_phase <= 732 ? m_phase - 730 : m_phase <= 738 ? m_phase - 728 : m_phase - 729;
		auto const &test = product_examples()[index];
		check_case(*m_cpu, test, m_port_writes, m_last_port_cycle - m_first_port_cycle);
		expect_opcode(test.opcode, true, "literal multiply/arithmetic/DELAY reference sequence");
		if (index + 1 < product_examples().size())
		{
			unsigned const next = index + 1;
			m_port_writes = 0;
			prepare_case(*m_cpu, product_examples()[next]);
			return next_atomic_phase(next < 3 ? 730 + next : next < 5 ? 6555 + next :
				next <= 10 ? 728 + next : 729 + next);
		}
	}
	return false;
}

} // namespace c54x_test
