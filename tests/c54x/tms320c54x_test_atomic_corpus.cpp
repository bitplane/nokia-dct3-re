// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_absolute_examples.h"
#include "tms320c54x_test_memory_examples.h"
#include <algorithm>

namespace c54x_test {

// Independently asserted instruction words reached by the ROM4 corpus; no acquired ROM is loaded.
bool tms320c54x_test_state::check_atomic_corpus_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 78)
	{
		expect_opcode(0x74d6, m_port_reads == 2 && data.read_word(0x0a03) == 0xabcd &&
				data.read_word(0x0a00) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0a01 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e5 &&
				m_last_port_cycle - m_first_port_cycle == 2,
				"PORTR port reads take two cycles and circularly update AR6");
		program.write_word(0x05e0, 0x74d4); // PORTR port,*AR4+%.
		program.write_word(0x05e2, 0xf5e1);
		data.write_word(0x0a13, 0);
		data.write_word(0x0a10, 0xbeef);
		m_port_reads = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0a13);
		return resume_atomic_at(0x05e0, 6524);
	}
	if (m_phase == 6524)
	{
		expect_opcode(0x74d4,
			m_port_reads == 1 && data.read_word(0x0a13) == 0xabcd &&
			data.read_word(0x0a10) == 0xbeef &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0a10 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0a01 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e3,
			"ROM4 PORTR consumes its port word and stores through AR4 before circular wrap without selecting AR6");
	}
	if (m_phase == 80)
	{
		program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
		program.write_word(0x05e1, 0x0124);
		program.write_word(0x05e2, 0x75d6);
		program.write_word(0x05e3, 0x0124);
		program.write_word(0x05e4, 0xf5e1); // IDLE
		data.write_word(0x0a03, 0x1234);
		data.write_word(0x0a00, 0x5678);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(81);
	}
	if (m_phase == 81)
	{
		expect(m_port_writes == 2 && m_first_port_value == 0x1234 &&
				m_last_port_value == 0x5678 &&
				m_last_port_cycle - m_first_port_cycle == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0a01,
				"PORTW memory source takes two cycles and circularly updates AR6");
		program.write_word(0x05e0, 0x74d6); // PORTR port, *AR6+%
		program.write_word(0x05e1, 0x0123);
		program.write_word(0x05e2, 0xf844); // BC 05e4, ANEQ
		program.write_word(0x05e3, 0x05e4);
		program.write_word(0x05e4, 0x74d6);
		program.write_word(0x05e5, 0x0123);
		program.write_word(0x05e6, 0xf5e1); // IDLE
		m_port_reads = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_A, 1},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(82);
	}
	if (m_phase == 82 || m_phase == 83)
	{
		expect_opcode(0xf844, m_port_reads == 2 && m_last_port_cycle - m_first_port_cycle ==
				(m_phase == 82 ? 7 : 5),
				"BC ANEQ costs five cycles taken and three cycles not taken");
		if (m_phase == 82)
		{
			m_port_reads = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(83);
		}
		program.write_word(0x05e2, 0x10f8); // LD *(lk), A
		program.write_word(0x05e3, 0x0d00);
		data.write_word(0x0d00, 0xfffe);
		m_port_reads = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		return resume_atomic_at(0x05e0, 84);
	}
	if (m_phase == 84)
	{
		expect_opcode(0x10f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffffeULL &&
				m_port_reads == 2 &&
				m_last_port_cycle - m_first_port_cycle == 4,
				"LD absolute Smem sign-extends and costs an extra cycle");
	}
	if (m_phase == 6504)
	{
		write_words(program, {
			{0x05e0, 0x75d6}, // PORTW *AR6+%, port
			{0x05e1, 0x0124},
			{0x05e2, 0x6c8a}, // BANZ 05e4, *AR2-
			{0x05e3, 0x05e4}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0a03, 0x1234);
		data.write_word(0x0a00, 0x5678);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 1}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(98);
	}
	if (m_phase == 98 || m_phase == 99)
	{
		expect_opcode(0x6c8a, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle ==
				(m_phase == 98 ? 6 : 4) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) ==
				(m_phase == 98 ? 0 : 0xffff),
				"ROM4 6c8a BANZ costs four cycles taken, two not taken, and decrements AR2");
		if (m_phase == 98)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR2, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(99);
		}
	}
	if (m_phase == 9207)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		write_words(program, {
			{0x05e0, 0x75d6}, // PORTW *AR6+%, port
			{0x05e1, 0x0124},
			{0x05e2, 0xf830}, // BC 05e4, TC
			{0x05e3, 0x05e4}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		m_cpu->set_state_int(tms320c54x_device::STATE_BK, 4);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000); // TC
		return resume_atomic_at(0x05e0, 101);
	}
	if (m_phase == 101 || m_phase == 102)
	{
		expect_opcode(0xf830, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle ==
				(m_phase == 101 ? 7 : 5),
				"ROM4 f830 BC TC costs five cycles taken and three not taken");
		if (m_phase == 101)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(102);
		}
	}
	if (m_phase == 103)
	{
		verify_atomic_case(register_bit_examples()[8]);
		program.write_word(0x05e2, 0xf073); // B 05e4
		program.write_word(0x05e3, 0x05e4);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		return resume_atomic_at(0x05e0, 104);
	}
	if (m_phase == 104)
	{
		expect_opcode(0xf073, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 f073 B consumes its target word and costs four cycles");
		program.write_word(0x05e2, 0x75f8); // PORTW *(0d00), 0124
		program.write_word(0x05e3, 0x0d00);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0x75d6); // PORTW *AR6+%, port
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		data.write_word(0x0d00, 0x9abc);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		return resume_atomic_at(0x05e0, 105);
	}
	if (m_phase == 105)
	{
		expect_opcode(0x75f8, m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5 &&
				m_first_port_value == 0x1234 && m_middle_port_value == 0x9abc,
				"ROM4 75f8 consumes absolute source and port words in three cycles");
		program.write_word(0x05e2, 0x74f8); // PORTR 0123, *(0d00)
		program.write_word(0x05e3, 0x0d00);
		program.write_word(0x05e4, 0x0123);
		data.write_word(0x0d00, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		return resume_atomic_at(0x05e0, 106);
	}
	if (m_phase == 106)
	{
		expect_opcode(0x74f8, data.read_word(0x0d00) == 0xabcd && m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 74f8 consumes absolute destination and port words in three cycles");
		program.write_word(0x05e0, 0x2883); // MAC *AR3,A
		program.write_word(0x05e1, 0xf5e1);
		data.write_word(0x0d00, 1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x0d00}, {cpu_regs::STATE_T, 1}, {cpu_regs::STATE_A, 0x7fffffff},
			{cpu_regs::STATE_ST0, 0x0800}, // Preserve C.
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(107);
	}
	if (m_phase == 107 || m_phase == 108)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 107 ? 0x0080000000ULL : 0x007fffffffULL) &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00,
				"MAC sets sticky OVA, preserves C, and saturates only with OVM");
		if (m_phase == 107)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fffffff);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200); // OVM
			return resume_atomic_at(0x05e0, 108);
		}
		data.write_word(0x0d00, 0x8000);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 0x8000}, {cpu_regs::STATE_A, 0xffffffffffULL}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0240}, // OVM, FRCT
			{cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(109);
	}
	if (m_phase == 109 || m_phase == 110)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 109 ? 0x7fffffff : 0x7ffffffe) &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) == 0,
				"PMST.SMUL saturates fractional product before accumulation");
		if (m_phase == 109)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x0002); // SMUL
			return resume_atomic_at(0x05e0, 110);
		}
		program.write_word(0x05e0, 0xb03a); // MAC *AR5+,*AR4-,A,A
		data.write_word(0x0b00, 1);
		data.write_word(0x0c00, 1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR5, 0x0b00}, {cpu_regs::STATE_AR4, 0x0c00}, {cpu_regs::STATE_A, 0x7fffffff},
			{cpu_regs::STATE_ST0, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200); // OVM
		return resume_atomic_at(0x05e0, 111);
	}
	if (m_phase == 111)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400),
				"dual-memory MAC sets OVA and clamps A with OVM");
		program.write_word(0x05e0, 0xd6e1); // ST B,*AR3 || MACR *AR4+0%,A
		data.write_word(0x0c00, 1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0c00}, {cpu_regs::STATE_AR3, 0x0d00}, {cpu_regs::STATE_A, 0x7fffffff},
			{cpu_regs::STATE_B, 0x12340000}, {cpu_regs::STATE_T, 1}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(112);
	}
	if (m_phase == 112)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
				data.read_word(0x0d00) == 0x1234,
				"parallel ST/MACR stores old B and sets OVA on saturated A");
	}
	if (m_phase == 113)
	{
		verify_atomic_case(register_bit_examples()[9]);
		program.write_word(0x05e2, 0x6082); // CMPM *AR2,#lk
		program.write_word(0x05e3, 0x1234);
		data.write_word(0x0e00, 0x1234);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0e00}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(114);
	}
	if (m_phase == 114 || m_phase == 115)
	{
		expect_opcode(0x6082, bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) ==
				(m_phase == 114) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0e00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 6082 CMPM updates TC, preserves AR2, and costs two cycles");
		if (m_phase == 114)
		{
			data.write_word(0x0e00, 0x5678);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			return resume_atomic_at(0x05e0, 115);
		}
		program.write_word(0x05e2, 0x8093); // STL A,*AR3+
		program.write_word(0x05e3, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_AR3, 0x0d00}, {cpu_regs::STATE_PC, 0x05e2},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(116);
	}
	if (m_phase == 116)
	{
		expect_opcode(0x8093, data.read_word(0x0d00) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d01,
				"ROM4 8093 stores the low accumulator word and post-increments AR3");
	}
	if (m_phase == 128)
	{
		verify_atomic_case(register_bit_examples()[10]);
	}
	if (m_phase == 191)
	{
	}
	if (m_phase == 192)
	{
		verify_atomic_case(register_bit_examples()[1]);
		program.write_word(0x05e2, 0xf845); // BC 05f0, AEQ
		program.write_word(0x05e3, 0x05f0);
		program.write_word(0x05e4, 0xffff);
		program.write_word(0x05f0, 0x75d6); // PORTW *AR6+%, port
		program.write_word(0x05f1, 0x0124);
		program.write_word(0x05f2, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(193);
	}
	if (m_phase == 193 || m_phase == 194)
	{
		expect_opcode(0xf845, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == (m_phase == 193 ? 7 : 5) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 193 ? 0x05f3 : 0x05e7),
				"ROM4 BC AEQ uses the full 40-bit A and costs five cycles taken, three not taken");
		if (m_phase == 193)
		{
			program.write_word(0x05e4, 0x75d6);
			program.write_word(0x05e5, 0x0124);
			program.write_word(0x05e6, 0xf5e1);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x0100000000ULL}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(194);
		}
	}
	if (m_phase == 195)
	{
		verify_atomic_case(register_bit_examples()[2]);
		return begin_atomic_case(register_bit_examples()[3], 196);
	}
	if (m_phase == 196 || m_phase == 197)
	{
	}
	if (m_phase == 198 || m_phase == 199)
	{
		verify_atomic_case(register_bit_examples()[m_phase == 198 ? 5 : 6]);
		if (m_phase == 198)
		{
			return begin_atomic_case(register_bit_examples()[6], 199);
		}
		program.write_word(0x05e2, 0xf010); // SUB #lk, A
		program.write_word(0x05e3, 0xff80);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		return resume_atomic_at(0x05e0, 200);
	}
	if (m_phase == 200 || m_phase == 201)
	{
		expect_opcode(0xf010, m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 200 ? 0x80 : 0x10080) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 SUB #lk respects SXM and costs two cycles");
		if (m_phase == 200)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM set
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			return resume_atomic_at(0x05e0, 201);
		}
		program.write_word(0x05e2, 0xf000); // ADD #lk, A
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		return resume_atomic_at(0x05e0, 202);
	}
	if (m_phase == 202 || m_phase == 203)
	{
		expect_opcode(0xf000, m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 202 ? 0x1ff80 : 0xff80) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 ADD #lk respects SXM and costs two cycles");
		if (m_phase == 202)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM set
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			return resume_atomic_at(0x05e0, 203);
		}
		program.write_word(0x05e2, 0xf820); // BC 05f0, NTC
		program.write_word(0x05e3, 0x05f0);
		program.write_word(0x05e4, 0x75d6); // fall-through marker
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		program.write_word(0x05f0, 0x75d6); // branch marker
		program.write_word(0x05f1, 0x0124);
		program.write_word(0x05f2, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // NTC true
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		return resume_atomic_at(0x05e0, 204);
	}
	if (m_phase >= 204 && m_phase <= 207)
	{
		const bool taken = (m_phase & 1) == 0;
		expect_opcode(m_phase <= 205 ? 0xf820 : 0xf84c,
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 5) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x05f3 : 0x05e7),
				"ROM4 BC NTC/BNEQ takes five cycles true and three cycles false");
		if (m_phase < 207)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			if (m_phase == 204)
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000); // NTC false
			else if (m_phase == 205)
			{
				program.write_word(0x05e2, 0xf84c); // BC 05f0, BNEQ
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0100000000ULL);
			}
			else
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xf495); // NOP
		program.write_word(0x05e3, 0x75d6); // PORTW *AR6+%, port
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12}, {cpu_regs::STATE_B, 0x34}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(208);
	}
	if (m_phase == 208)
	{
		expect_opcode(0xf495, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x34 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e6,
				"ROM4 NOP preserves state and costs one cycle");
		program.write_word(0x05e2, 0xf074); // CALL 05f0
		program.write_word(0x05e3, 0x05f0);
		program.write_word(0x05e4, 0xf5e1);
		program.write_word(0x05f0, 0x75d6); // subroutine port marker
		program.write_word(0x05f1, 0x0124);
		program.write_word(0x05f2, 0xfc00); // RET
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(209);
	}
	if (m_phase == 209)
	{
		expect_opcode(0xf074, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6 &&
				data.read_word(0x02ff) == 0x05e4 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e5,
				"ROM4 CALL pushes continuation, branches, and costs four cycles");
		program.write_word(0x05e2, 0xfc00); // RET
		program.write_word(0x05f0, 0x75d6); // return port marker
		program.write_word(0x05f1, 0x0124);
		program.write_word(0x05f2, 0xf5e1);
		data.write_word(0x0300, 0x05f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(210);
	}
	if (m_phase == 210)
	{
		expect_opcode(0xfc00, m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f3,
				"ROM4 RET pops continuation and costs five cycles");
		return begin_atomic_case(register_bit_examples()[11], 211);
	}
	if (m_phase == 211)
	{
	}
	if (m_phase == 212)
	{
		verify_atomic_case(register_bit_examples()[12]);
		program.write_word(0x05e0, 0x6d90); // MAR *AR0+ (ARP-selected in compatibility mode)
		program.write_word(0x05e1, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0x0010}, {cpu_regs::STATE_AR4, 0x0e00}, {cpu_regs::STATE_ST0, 4 << 13},
			{cpu_regs::STATE_ST1, 0x0020}, // CMPT
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(213);
	}
	if (m_phase == 213)
	{
		expect_opcode(0x6d90, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0e01 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0010 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) >> 13) == 4,
				"ROM4 MAR AR0 aliases ARP in compatibility mode");
		program.write_word(0x05e0, 0x1090); // LD *AR0+, A
		program.write_word(0x05e1, 0xf5e1);
		data.write_word(0x0e01, 0x1234);
		return resume_atomic_at(0x05e0, 214);
	}
	if (m_phase == 214)
	{
		expect_opcode(0x1090, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0e02 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0010,
				"indirect AR0 read and postincrement use ARP in compatibility mode");
		program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
		program.write_word(0x05e1, 0x0124);
		program.write_word(0x05e2, 0x1c8b); // XOR *AR3-, A
		program.write_word(0x05e3, 0x1d93); // XOR *AR3+, B
		program.write_word(0x05e4, 0x1c82); // XOR *AR2, A
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		data.write_word(0x0f00, 0x00f0);
		data.write_word(0x0eff, 0xff80);
		data.write_word(0x0f10, 0x0f00);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678}, {cpu_regs::STATE_AR2, 0x0f10},
			{cpu_regs::STATE_AR3, 0x0f00}, {cpu_regs::STATE_AR6, 0x0a03},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // Preserve C.
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM must not sign-extend XOR.
		return resume_atomic_at(0x05e0, 215);
	}
	if (m_phase == 215)
	{
		const bool xor_results = m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1dc4 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xa9f8 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f10 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f00 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5;
		expect_opcode(0x1c8b, xor_results, "ROM4 XOR *AR3- selects A and decrements AR3");
		expect_opcode(0x1d93, xor_results, "ROM4 XOR *AR3+ selects B without SXM extension");
		expect_opcode(0x1c82, xor_results, "ROM4 XOR *AR2 selects A; three XORs cost three cycles");
		return begin_atomic_case(paired_memory_examples()[0], 216);
	}
	if (m_phase >= 216 && m_phase <= 218)
	{
		auto const &cases = paired_memory_examples();
		unsigned const index = 0 + m_phase - 216;
		verify_atomic_case(cases[index]);
		if (m_phase < 218) return begin_atomic_case(cases[index + 1], m_phase + 1);
		return begin_atomic_case(register_bit_examples()[7], 219);
	}
	if (m_phase == 219)
	{
		write_words(program, {
			{0x05e0, 0x75d6}, // PORTW *AR6+%, port
			{0x05e1, 0x0124},
			{0x05e2, 0x6b8a}, // ADDM #lk, *AR2-
			{0x05e3, 0xfff8}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0d20, 0x8007);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0d20}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0300}, // OVM and SXM.
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(220);
	}
	if (m_phase == 220)
	{
		expect_opcode(0x6b8a, data.read_word(0x0d20) == 0x8000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d1f &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 ADDM saturates signed underflow, sets C/OVA, and costs two cycles");
		program.write_word(0x05e0, 0x6b80); // ADDM #lk, *AR0 (ARP-selected in CMPT)
		program.write_word(0x05e1, 0x123b);
		program.write_word(0x05e2, 0xf5e1);
		data.write_word(0x0d30, 4);
		data.write_word(0x0d40, 0x7777);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0x0d40}, {cpu_regs::STATE_AR4, 0x0d30}, {cpu_regs::STATE_ST0, 4 << 13},
			{cpu_regs::STATE_ST1, 0x0020}, // CMPT
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(221);
	}
	if (m_phase == 221)
	{
		expect_opcode(0x6b80, data.read_word(0x0d30) == 0x123f &&
				data.read_word(0x0d40) == 0x7777 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0d40 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d30 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xec00) == 0x8000,
				"ADDM with compatibility AR0 selects ARP and clears carry without overflow");
		program.write_word(0x05e0, 0x6b8a); // ADDM #1, *AR2-
		program.write_word(0x05e1, 1);
		program.write_word(0x05e2, 0xf5e1);
		data.write_word(0x0d60, 0x7fff);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d60);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM; OVM clear.
		return resume_atomic_at(0x05e0, 222);
	}
	if (m_phase == 222)
	{
		expect(data.read_word(0x0d60) == 0x8000 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400,
				"ADDM wraps positive overflow with OVM clear and sets OVA without carry");
		data.write_word(0x0d60, 0xffff);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0d60}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(223);
	}
	if (m_phase == 223)
	{
		expect(data.read_word(0x0d60) == 0 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0800,
				"ADDM propagates unsigned carry without signed overflow");
		program.write_word(0x05e0, 0x6880); // ANDM #lk, *AR0
		program.write_word(0x05e1, 0x0f0f);
		program.write_word(0x05e2, 0x6980); // ORM #lk, *AR0
		program.write_word(0x05e3, 0x0033);
		program.write_word(0x05e4, 0xf5e1);
		data.write_word(0x0d30, 0x00f0);
		data.write_word(0x0d40, 0x7777);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0x0d40}, {cpu_regs::STATE_AR4, 0x0d30}, {cpu_regs::STATE_ST0, 4 << 13},
			{cpu_regs::STATE_ST1, 0x0020}, // CMPT
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(224);
	}
	if (m_phase == 224)
	{
		expect_opcode(0x6880, data.read_word(0x0d30) == 0x0033 &&
				data.read_word(0x0d40) == 0x7777,
				"ANDM compatibility AR0 uses ARP before ORM");
		expect_opcode(0x6980, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0d40 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d30 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe000) == 0x8000,
				"ORM compatibility AR0 uses ARP without modifying either pointer");
		program.write_word(0x05e0, 0x6b8a); // ADDM #8000h, *AR2-
		program.write_word(0x05e1, 0x8000);
		program.write_word(0x05e2, 0xf5e1);
		data.write_word(0x0d60, 0x8000);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d60);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM and OVM clear.
		return resume_atomic_at(0x05e0, 225);
	}
	if (m_phase == 225)
	{
		expect(data.read_word(0x0d60) == 0 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400,
				"ADDM SXM-clear 16-bit wrap does not carry out of the 32-bit ALU");
		return begin_atomic_case(paired_memory_examples()[3], 226);
	}
	if (m_phase >= 226 && m_phase <= 235)
	{
		auto const &cases = paired_memory_examples();
		unsigned const index = 3 + m_phase - 226;
		verify_atomic_case(cases[index]);
		if (m_phase < 235) return begin_atomic_case(cases[index + 1], m_phase + 1);
		return begin_atomic_case(memory_address_examples()[0], 236);
	}
	if (m_phase == 236 || m_phase == 6584)
	{
		static constexpr unsigned phases[] = {236, 6584};
		unsigned const position = std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases);
		auto const &cases = memory_address_examples();
		verify_atomic_case(cases[0 + position]);
		if (position + 1 < std::size(phases)) return begin_atomic_case(cases[0 + position + 1], phases[position + 1]);
	}
	if (m_phase == 237)
	{
		verify_atomic_case(register_bit_examples()[13]);
		return begin_atomic_case(memory_address_examples()[2], 238);
	}
	if (m_phase == 238)
	{
		verify_atomic_case(memory_address_examples()[2]);
	}
	if (m_phase == 239)
	{
		verify_atomic_case(register_bit_examples()[14]);
		return begin_atomic_case(memory_address_examples()[3], 240);
	}
	if (m_phase == 240)
	{
		verify_atomic_case(memory_address_examples()[3]);
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xfc30}, // RC TC, false
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(241);
	}
	if (m_phase == 241)
	{
		expect_opcode(0xfc30, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 RC TC false falls through without popping and costs three cycles");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xfc30}, // RC TC, true
			{0x05e3, 0xf5e1}, // Must be bypassed by the return.
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0300, 0x05e6);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST0, 0x1000}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(242);
	}
	if (m_phase == 242)
	{
		expect_opcode(0xfc30, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 RC TC true pops its target and costs five cycles");
		return begin_atomic_case(memory_address_examples()[4], 243);
	}
	if (m_phase == 243 || m_phase == 244 || m_phase == 245)
	{
		static constexpr unsigned phases[] = {243, 244, 245};
		unsigned const position = std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases);
		auto const &cases = memory_address_examples();
		verify_atomic_case(cases[4 + position]);
		if (position + 1 < std::size(phases)) return begin_atomic_case(cases[4 + position + 1], phases[position + 1]);
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xfe00}, // RETD
			{0x05e3, 0xe801}, // LD #1,A, first delay word
			{0x05e4, 0xe802}, // LD #2,A, second delay word
			{0x05e5, 0xf5e1}, // Must be bypassed by the return.
			{0x05e8, 0x75d6}, {0x05e9, 0x0124}, {0x05ea, 0xf5e1},
		});
		data.write_word(0x0300, 0x05e8);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(246);
	}
	if (m_phase == 246)
	{
		expect_opcode(0xfe00, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 RETD executes two delay words before return and costs three cycles");
		expect_opcode(0xe802, m_cpu->state_int(tms320c54x_device::STATE_A) == 2,
				"ROM4 LD #2,A executes as the second RETD delay word");
		return begin_atomic_case(memory_address_examples()[7], 247);
	}
	if (m_phase == 247)
	{
		verify_atomic_case(memory_address_examples()[7]);
	}
	if (m_phase == 6545 || m_phase == 6546)
	{
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xf020}, // LD #lk, A
			{0x05e3, 0xff80}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(256);
	}
	if (m_phase == 256 || m_phase == 257)
	{
		const bool sign_extend = m_phase == 257;
		expect_opcode(0xf020,
				m_cpu->state_int(tms320c54x_device::STATE_A) ==
						(sign_extend ? 0xffffffff80ULL : 0xff80ULL) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD #lk,A obeys SXM and costs two cycles");
		if (!sign_extend)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR6, 0x0a03},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(257);
		}
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x8094}, // STL A,*AR4+
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		data.write_word(0x0f20, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_AR4, 0x0f20}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(258);
	}
	if (m_phase == 258)
	{
		expect_opcode(0x8094, data.read_word(0x0f20) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f21 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 STL A,*AR4+ stores AL, postincrements, and costs one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x00f8}, // ADD *(lk),A
			{0x05e3, 0x0f20}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0f20, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffffffffffULL}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(259);
	}
	if (m_phase == 259 || m_phase == 260)
	{
		const bool sign_extend = m_phase == 260;
		expect_opcode(0x00f8,
				m_cpu->state_int(tms320c54x_device::STATE_A) ==
						(sign_extend ? 0xffffffffffULL : 0) &&
				(sign_extend || (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800)) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 absolute ADD obeys SXM, publishes carry, and costs two cycles");
		if (!sign_extend)
		{
			data.write_word(0x0f20, 0xfffe);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0x0100},
				{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(260);
		}
		program.write_word(0x05e2, 0x01f8); // ADD *(lk),B.
		data.write_word(0x0f20, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_B, 1}, {cpu_regs::STATE_ST0, 0x1800},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6589);
	}
	if (m_phase == 6589 || m_phase == 6590)
	{
		const bool signed_operand = m_phase == 6590;
		expect_opcode(0x01f8,
			m_cpu->state_int(tms320c54x_device::STATE_B) == (signed_operand ? 0xffffff8002ULL : 0x8002) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1000 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (signed_operand ? 0x0100 : 0) &&
			data.read_word(0x0f20) == 0x8001 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 absolute ADD into B obeys SXM, preserves A/TC/source, clears carry and consumes its address in two cycles");
		if (!signed_operand)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_B, 1}, {cpu_regs::STATE_ST0, 0x1800}, {cpu_regs::STATE_ST1, 0x0100},
				{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(6590);
		}
		program.write_word(0x05e2, 0x09f8); // SUB *(lk),B
		data.write_word(0x0f20, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(261);
	}
	if (m_phase == 261 || m_phase == 262)
	{
		const bool sign_extend = m_phase == 262;
		expect_opcode(0x09f8,
				m_cpu->state_int(tms320c54x_device::STATE_B) ==
						(sign_extend ? 1 : 0xffffffffffULL) &&
				bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == sign_extend &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 absolute SUB B obeys SXM, publishes no-borrow carry, and costs two cycles");
		if (!sign_extend)
		{
			data.write_word(0x0f20, 0xfffe);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_B, 0xffffffffffULL}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0x0100},
				{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(262);
		}
	}
	if (m_phase == 263)
	{
		verify_atomic_case(register_bit_examples()[15]);
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xec05}, // RPT #5
			{0x05e3, 0x0082}, // ADD *AR2,A
			{0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0f20, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 0x0f20}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(264);
	}
	if (m_phase == 264)
	{
		expect_opcode(0xec05, m_cpu->state_int(tms320c54x_device::STATE_A) == 6 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"ROM4 RPT #5 runs its one-cycle ADD body six times after one-cycle setup");
		return begin_atomic_case(memory_address_examples()[8], 265);
	}
	if (m_phase == 265)
	{
		verify_atomic_case(memory_address_examples()[8]);
		write_words(program, {
			{0x05e2, 0x7593}, // PORTW *AR3+,PA
			{0x05e3, 0x0124}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0f20, 0xabcd);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x0f20}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(266);
	}
	if (m_phase == 266)
	{
		expect_opcode(0x7593, m_middle_port_value == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f21 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 2 &&
				m_last_port_cycle - m_middle_port_cycle == 2,
				"ROM4 PORTW *AR3+,PA transfers the old source and costs two cycles");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xfc20}, // RC NTC, false
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST0, 0x1000}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(267);
	}
	if (m_phase == 267)
	{
		expect_opcode(0xfc20, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 RC NTC false retains SP and costs three cycles");
		program.write_word(0x05e3, 0xf5e1); // Must be bypassed by the return.
		program.write_word(0x05e8, 0x75d6);
		program.write_word(0x05e9, 0x0124);
		program.write_word(0x05ea, 0xf5e1);
		data.write_word(0x0300, 0x05e8);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(268);
	}
	if (m_phase == 268)
	{
		expect_opcode(0xfc20, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 RC NTC true pops its target and costs five cycles");
		return begin_atomic_case(memory_address_examples()[9], 269);
	}
	if (m_phase == 269 || m_phase == 270)
	{
		static constexpr unsigned phases[] = {269, 270};
		unsigned const position = std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases);
		auto const &cases = memory_address_examples();
		verify_atomic_case(cases[9 + position]);
		if (position + 1 < std::size(phases)) return begin_atomic_case(cases[9 + position + 1], phases[position + 1]);
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xfc45}, // RC AEQ, false with nonzero guard byte
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0100000000ULL}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(271);
	}
	if (m_phase == 271)
	{
		expect_opcode(0xfc45, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 RC AEQ false checks the 40-bit guard and costs three cycles");
		program.write_word(0x05e3, 0xf5e1); // Must be bypassed by the return.
		program.write_word(0x05e8, 0x75d6);
		program.write_word(0x05e9, 0x0124);
		program.write_word(0x05ea, 0xf5e1);
		data.write_word(0x0300, 0x05e8);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(272);
	}
	if (m_phase == 272)
	{
		expect_opcode(0xfc45, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 RC AEQ true pops its target and costs five cycles");
		return begin_atomic_case(memory_address_examples()[11], 273);
	}
	if (m_phase == 273 || m_phase == 274)
	{
		static constexpr unsigned phases[] = {273, 274};
		unsigned const position = std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases);
		auto const &cases = memory_address_examples();
		verify_atomic_case(cases[11 + position]);
		if (position + 1 < std::size(phases)) return begin_atomic_case(cases[11 + position + 1], phases[position + 1]);
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xf846}, // BC 05f0,AGT
			{0x05e3, 0x05f0},
			{0x05e4, 0xf5e1}, // Taken branch bypasses fallthrough.
			{0x05f0, 0x75d6}, {0x05f1, 0x0124}, {0x05f2, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0100000000ULL);
		return resume_atomic_at(0x05e0, 275);
	}
	if (m_phase == 275)
	{
		expect_opcode(0xf846, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 BC AGT uses the 40-bit sign and costs five cycles taken");
		program.write_word(0x05e2, 0xf84d); // BC 05f0,BEQ
		program.write_word(0x05e4, 0x75d6); // Fallthrough marker.
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0100000000ULL);
		return resume_atomic_at(0x05e0, 276);
	}
	if (m_phase == 276)
	{
		expect_opcode(0xf84d, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 BC BEQ checks B's guard byte and costs three cycles false");
		return begin_atomic_case(memory_address_examples()[13], 277);
	}
	if (m_phase == 277 || m_phase == 278)
	{
		static constexpr unsigned phases[] = {277, 278};
		unsigned const position = std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases);
		auto const &cases = memory_address_examples();
		verify_atomic_case(cases[13 + position]);
		if (position + 1 < std::size(phases)) return begin_atomic_case(cases[13 + position + 1], phases[position + 1]);
		write_words(program, {
			{0x05e3, 0x7482}, // PORTR PA,*AR2
			{0x05e4, 0x0123}, {0x05e5, 0x7482}, {0x05e6, 0x0123}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0f21, 0);
		m_port_reads = 0;
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f21);
		return resume_atomic_at(0x05e0, 279);
	}
	if (m_phase == 279)
	{
		expect_opcode(0x7482, data.read_word(0x0f21) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f21 &&
				m_port_reads == 2 && m_port_writes == 1 &&
				m_last_port_cycle - m_first_port_cycle == 2,
				"ROM4 PORTR PA,*AR2 writes the port word in two cycles");
		program.write_word(0x05e3, 0x7483); // PORTR PA,*AR3.
		program.write_word(0x05e5, 0x7483);
		data.write_word(0x0f22, 0xbeef);
		data.write_word(0x0f23, 0);
		data.write_word(0x0f24, 0xcafe);
		m_port_reads = 0;
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x0f23}, {cpu_regs::STATE_ST0, 0x0aa5}, {cpu_regs::STATE_ST1, 0x0100},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6570);
	}
	if (m_phase == 6570)
	{
		expect_opcode(0x7483,
			data.read_word(0x0f23) == 0xabcd && data.read_word(0x0f21) == 0xabcd &&
			data.read_word(0x0f22) == 0xbeef && data.read_word(0x0f24) == 0xcafe &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f23 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f21 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 5 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 133 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0100 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
			m_port_reads == 2 && m_port_writes == 1 && m_last_port_cycle - m_first_port_cycle == 2,
			"ROM4 PORTR selects stationary AR3, preserves neighbors/pointers/accumulators/status and consumes the port extension in two cycles");
		return begin_atomic_case(memory_address_examples()[15], 280);
	}
	if (m_phase == 280)
	{
		verify_atomic_case(memory_address_examples()[15]);
		m_phase = 9215;
	}
	if (m_phase >= 9215 && m_phase <= 9219)
	{
		if (m_phase != 9215)
		{
			const unsigned index = m_phase - 9216;
			const bool circular = index >= 2;
			expect_opcode(circular ? 0x56d3 : 0x5685,
				m_cpu->state_int(tms320c54x_device::STATE_A) == ((index & 1) ? 0xff801234ULL : 0xffff801234ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x76543210 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f20 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == (circular ? 0x0f21 : 0x0f24) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 DLD stationary/circular sources respect SXM, long-pointer wrap, B/status preservation and one-cycle timing");
		}
		if (m_phase < 9219)
		{
			const unsigned index = m_phase - 9215;
			write_words(program, {
				{0x05e3, index >= 2 ? 0x56d3 : 0x5685}, {0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124},
				{0x05e7, 0xf5e1},
			});
			write_words(data, {
				{0x0f20, 0xff80}, {0x0f21, 0x1234}, {0x0f24, 0xff80}, {0x0f25, 0x1234},
			});
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0x76543210}, {cpu_regs::STATE_AR5, 0x0f20},
				{cpu_regs::STATE_AR3, 0x0f24}, {cpu_regs::STATE_BK, 5}, {cpu_regs::STATE_ST0, 0x0aa5},
				{cpu_regs::STATE_ST1, (index & 1) ? 0 : 0x0100}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		write_words(program, {
			{0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		program.write_word(0x05e3, 0x7213); // MVDM dmad,AR3
		program.write_word(0x05e4, 0x0f20);
		data.write_word(0x0f20, 0x1234);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0);
		return resume_atomic_at(0x05e0, 281);
	}
	if (m_phase == 281)
	{
		expect_opcode(0x7213, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 MVDM copies dmad to AR3 in two cycles");
		program.write_word(0x05e3, 0x720b); // MVDM dmad,BL.
		data.write_word(0x0f20, 0xcafe);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x11223344}, {cpu_regs::STATE_B, 0xff1234abcdULL}, {cpu_regs::STATE_ST0, 0x1800},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6564);
	}
	if (m_phase == 6564)
	{
		expect_opcode(0x720b,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff1234cafeULL &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x11223344 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0100 &&
			data.read_word(0x0f20) == 0xcafe &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e9 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 MVDM to BL replaces only B low word, preserves guard/high/A/status/source and consumes dmad in two cycles");
		program.write_word(0x05e3, 0x708a); // MVKD dmad,*AR2-
		program.write_word(0x05e4, 0x0f20);
		data.write_word(0x0f20, 0x5678);
		data.write_word(0x0f21, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f21);
		return resume_atomic_at(0x05e0, 282);
	}
	if (m_phase == 282)
	{
		expect_opcode(0x708a, data.read_word(0x0f21) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 MVKD copies source to *AR2- in two cycles");
		program.write_word(0x05e3, 0x7083); // MVKD dmad,*AR3.
		data.write_word(0x0f23, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f23);
		return resume_atomic_at(0x05e0, 6520);
	}
	if (m_phase == 6520 || m_phase == 6531 || m_phase == 6532 || m_phase == 6563)
	{
		u16 const opcode = m_phase == 6520 ? 0x7083 : m_phase == 6531 ? 0x7092 : m_phase == 6532 ? 0x7090 : 0x7088;
		expect_opcode(opcode,
			data.read_word(0x0f23) == 0x5678 && data.read_word(0x0f20) == 0x5678 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f23 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (m_phase == 6520 ? 0x0f20 : m_phase == 6531 ? 0x0f24 : 0x0f23) &&
			(m_phase != 6532 || m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f24) &&
			(m_phase != 6563 || m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f22) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 MVKD consumes dmad, preserves source, selects its stationary/postincrement/postdecrement destination and costs two cycles");
		if (m_phase != 6563)
		{
			program.write_word(0x05e3, m_phase == 6520 ? 0x7092 : m_phase == 6531 ? 0x7090 : 0x7088);
			data.write_word(0x0f23, 0);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR2, 0x0f23}, {cpu_regs::STATE_AR0, 0x0f23}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(m_phase == 6520 ? 6531 : m_phase == 6531 ? 6532 : 6563);
		}
		return begin_atomic_case(memory_address_examples()[16], 283);
	}
	if (m_phase == 283 || m_phase == 284 || m_phase == 6521)
	{
		static constexpr unsigned phases[] = {283, 284, 6521};
		unsigned const position = std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases);
		auto const &cases = memory_address_examples();
		verify_atomic_case(cases[16 + position]);
		if (position + 1 < std::size(phases)) return begin_atomic_case(cases[16 + position + 1], phases[position + 1]);
		write_words(program, {
			{0x05e2, 0x7194}, // MVDK *AR4+,dmad
			{0x05e3, 0x0f22}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0f21, 0xabcd);
		data.write_word(0x0f22, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0f21}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(285);
	}
	if (m_phase == 285 || m_phase == 9226)
	{
		const bool stationary = m_phase == 9226;
		expect_opcode(stationary ? 0x7184 : 0x7194, data.read_word(0x0f22) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == (stationary ? 0x0f21 : 0x0f22) &&
				(!stationary || (data.read_word(0x0f21) == 0xabcd && data.read_word(0x0f20) == 0xbeef && data.read_word(0x0f23) == 0xcafe &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x76543210 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0100 &&
					m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7)) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 MVDK copies stationary/incrementing AR4 to dmad in two cycles");
		if (!stationary)
		{
			program.write_word(0x05e2, 0x7184);
			data.write_word(0x0f20, 0xbeef);
			data.write_word(0x0f22, 0);
			data.write_word(0x0f23, 0xcafe);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR4, 0x0f21}, {cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_B, 0x76543210},
				{cpu_regs::STATE_ST0, 0x0aa5}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(9226);
		}
		write_words(program, {
			{0x05e0, 0x75f8}, {0x05e1, 0x0d00}, {0x05e2, 0x0124},
			{0x05e3, 0x6c88}, // BANZ 05f0,*AR0-
			{0x05e4, 0x05f0},
			{0x05e5, 0x75f8}, // Fallthrough marker.
			{0x05e6, 0x0d01}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
			{0x05f0, 0x75f8}, // Taken marker.
			{0x05f1, 0x0d02}, {0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		data.write_word(0x0d00, 0x1111);
		data.write_word(0x0d01, 0x2222);
		data.write_word(0x0d02, 0x3333);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8000); // ARP=4
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0020); // CMPT
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0}, {cpu_regs::STATE_AR4, 1}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(286);
	}
	if (m_phase == 286 || m_phase == 287)
	{
		const bool taken = m_phase == 286;
		expect_opcode(0x6c88, m_port_writes == 2 &&
				m_last_port_value == (taken ? 0x3333 : 0x2222) &&
				m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 5) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == (taken ? 0 : 1) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == (taken ? 0 : 0xffff),
				"ROM4 BANZ *AR0- tests and updates ARP-selected AR4 in compatibility mode");
		if (taken)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR4, 0}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(287);
		}
		program.write_word(0x05e3, 0x6e88); // BANZD 05f0,*AR0-
		program.write_word(0x05e5, 0xf495); // First delay word.
		program.write_word(0x05e6, 0xf495); // Second delay word.
		program.write_word(0x05e7, 0x75f8); // Must be bypassed.
		program.write_word(0x05e8, 0x0d01);
		program.write_word(0x05e9, 0x0124);
		program.write_word(0x05ea, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0}, {cpu_regs::STATE_AR4, 1}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(288);
	}
	if (m_phase == 288)
	{
		expect_opcode(0x6e88, m_port_writes == 2 && m_last_port_value == 0x3333 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0 &&
				m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 BANZD *AR0- tests ARP-selected AR4 before delayed branch");
		return begin_atomic_case(memory_address_examples()[19], 289);
	}
	if (m_phase == 289 || m_phase == 290 || m_phase == 6573 || m_phase == 6574 || m_phase == 291 || m_phase == 292 || m_phase == 6537 || m_phase == 293 || m_phase == 294)
	{
		static constexpr unsigned phases[] = {289, 290, 6573, 6574, 291, 292, 6537, 293, 294};
		unsigned const position = std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases);
		auto const &cases = memory_address_examples();
		verify_atomic_case(cases[19 + position]);
		if (position + 1 < std::size(phases)) return begin_atomic_case(cases[19 + position + 1], phases[position + 1]);
		program.write_word(0x05e3, 0x6c8b); // BANZ 05eb,*AR3-
		write_words(program, {
			{0x05e4, 0x05eb}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1}, {0x05eb, 0x75f8},
			{0x05ec, 0x0d00}, {0x05ed, 0x0124}, {0x05ee, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 1);
		return resume_atomic_at(0x05e0, 295);
	}
	if (m_phase == 295 || m_phase == 296)
	{
		expect_opcode(0x6c8b, m_cpu->state_int(tms320c54x_device::STATE_AR3) ==
				(m_phase == 295 ? 0 : 0xffff) && m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == (m_phase == 295 ? 7 : 5),
				"ROM4 BANZ *AR3- tests before decrement and has taken/false cycle costs");
		if (m_phase == 295)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0);
			return resume_atomic_at(0x05e0, 296);
		}
		program.write_word(0x05e3, 0xf847); // BC 05eb,ALEQ
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
		return resume_atomic_at(0x05e0, 297);
	}
	if (m_phase == 297 || m_phase == 298)
	{
		expect_opcode(0xf847, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == (m_phase == 297 ? 8 : 6),
				"ROM4 BC ALEQ uses the 40-bit sign and has taken/false cycle costs");
		if (m_phase == 297)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 1);
			return resume_atomic_at(0x05e0, 298);
		}
		write_words(program, {
			{0x05e3, 0x880e}, // STLM A,T
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff12345678ULL}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(299);
	}
	if (m_phase == 299)
	{
		expect_opcode(0x880e, m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12345678ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 STLM A,T copies AL into T in one cycle");
		return begin_atomic_case(absolute_arithmetic_examples().front(), 300);
	}
	if ((m_phase >= 300 && m_phase <= 309) || m_phase == 6534 || m_phase == 6535)
	{
		static constexpr unsigned phases[] = {300, 301, 302, 303, 6534, 6535, 304, 305, 306, 307, 308, 309};
		auto const &cases = absolute_arithmetic_examples();
		expect(cases.size() == std::size(phases), "Absolute arithmetic case/continuation map is complete");
		unsigned const index = unsigned(std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases));
		verify_atomic_case(cases[index]);
		if (index + 1 < cases.size()) return begin_atomic_case(cases[index + 1], phases[index + 1]);
		write_words(program, {
			{0x05e3, 0x0282}, // ADDS *AR2,A
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0f66, 0xffff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_AR2, 0x0f66}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0100}, // SXM
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(310);
	}
	if (m_phase == 310 || m_phase == 311 || m_phase == 90010 || m_phase == 90011)
	{
		const bool destination_b = m_phase == 90010 || m_phase == 90011;
		const bool carry_case = m_phase == 311 || m_phase == 90011;
		if (destination_b)
			osd_printf_info("ADDS B observation: phase=%d a=%010llx b=%010llx st0=%04x ar7=%04x writes=%d cycles=%d\n",
				m_phase, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR7)), m_port_writes,
				int(m_last_port_cycle - m_first_port_cycle));
		expect_opcode(destination_b ? 0x0387 : 0x0282,
				m_cpu->state_int(destination_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) ==
				(!carry_case ? 0x10000 : 0x100000000ULL) &&
				bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
				carry_case &&
				(!destination_b || m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678) &&
				m_cpu->state_int(destination_b ? tms320c54x_device::STATE_AR7 : tms320c54x_device::STATE_AR2) == 0x0f66 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ADDS Smem,A/B ignores SXM, preserves the other accumulator, and carries at bit 32 in one cycle");
		if (m_phase == 310 || m_phase == 90010)
		{
			data.write_word(0x0f66, 1);
			m_port_writes = 0;
			m_cpu->set_state_int(destination_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, 0xffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(destination_b ? 90011 : 311);
		}
		if (m_phase == 311)
		{
			program.write_word(0x05e3, 0x0387); // ADDS *AR7,B
			data.write_word(0x0f66, 0xffff);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_B, 1}, {cpu_regs::STATE_AR7, 0x0f66},
				{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(90010);
		}
		return begin_atomic_case(long_offset_examples()[0], 312);
	}
	if (m_phase >= 312 && m_phase <= 325)
	{
		auto const &cases = long_offset_examples();
		unsigned const index = m_phase - 312;
		verify_atomic_case(cases[index]);
		if (m_phase < 325) return begin_atomic_case(cases[index + 1], m_phase + 1);
		program.write_word(0x05e3, 0xfa20); // BCD 05f0h, NTC
		program.write_word(0x05e4, 0x05f0);
		program.write_word(0x05e5, 0xe801); // Delay slot 1
		program.write_word(0x05e6, 0xe902); // Delay slot 2
		program.write_word(0x05e7, 0xe803); // Fall-through only
		write_words(program, {
			{0x05e8, 0x75f8}, {0x05e9, 0x0d00}, {0x05ea, 0x0124}, {0x05eb, 0xf5e1}, {0x05f0, 0x75f8}, {0x05f1, 0x0d00},
			{0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // NTC true
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(326);
	}
	if (m_phase == 326)
	{
		expect_opcode(0xfa20, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"BCD NTC executes two delay words then branches in three cycles");
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000); // NTC false
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(327);
	}
	if (m_phase == 327)
	{
		expect_opcode(0xfa20, m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ec &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"BCD NTC false executes delay words and falls through");
		write_words(program, {
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_phase = 328;
	}
	if (m_phase >= 328 && m_phase <= 331)
	{
		static constexpr u16 opcodes[] = { 0x8814, 0x8813, 0x8811, 0x8912 };
		static constexpr unsigned registers[] = { 4, 3, 1, 2 };
		const unsigned index = m_phase - 328;
		if (index)
		{
			expect_opcode(opcodes[index - 1],
					m_cpu->state_int(tms320c54x_device::STATE_AR0 + registers[index - 1]) ==
						0xbeef &&
					m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
					"STLM writes the selected address register in one cycle");
		}
		program.write_word(0x05e3, opcodes[index]);
		m_port_writes = 0;
		for (unsigned ar = 0; ar != 8; ++ar)
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234beef}, {cpu_regs::STATE_B, 0x5678cafe}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		++m_phase;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase == 332)
	{
		expect_opcode(0x8912,
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0xcafe &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"STLM B writes AR2 in one cycle");
		write_words(program, {
			{0x05e3, 0xec0c}, // RPT #12
			{0x05e4, 0x6d10}, // MAR *AR0+
			{0x05e5, 0xe80f}, // LD #15,A
			{0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(333);
	}
	if (m_phase == 333)
	{
		expect_opcode(0xec0c, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 13 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 18,
				"RPT #12 executes MAR thirteen times after one-cycle setup");
		expect_opcode(0xe80f, m_cpu->state_int(tms320c54x_device::STATE_A) == 15,
				"LD #15,A writes the accumulator after the repeated body");
		program.write_word(0x05e3, 0x13f8); // LDU *(0f20h),B
		write_words(program, {
			{0x05e4, 0x0f20}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f20, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		return resume_atomic_at(0x05e0, 334);
	}
	if (m_phase == 334)
	{
		expect_opcode(0x13f8, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8001 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"absolute LDU B zero-extends even with SXM set in two cycles");
		write_words(program, {
			{0x05e3, 0xe800}, // LD #k8,A/B.
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x11111111}, {cpu_regs::STATE_B, 0x22222222}, {cpu_regs::STATE_ST0, 0x1800},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(8000);
	}
	if (m_phase >= 8000 && m_phase <= 9023)
	{
		const unsigned index = m_phase - 8000;
		const u16 opcode = 0xe800 | (index & 0x1ff);
		const bool b = BIT(index, 8);
		const u16 st1 = index >= 512 ? 0x0100 : 0;
		expect_opcode(opcode,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (b ? 0x11111111 : index & 0xff) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (b ? index & 0xff : 0x22222222) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == st1 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"LD #k8 selects A/B, zero-extends all byte immediates with either SXM setting and preserves other accumulator/status in one cycle");
		if (index < 1023)
		{
			program.write_word(0x05e3, 0xe800 | ((index + 1) & 0x1ff));
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x11111111}, {cpu_regs::STATE_B, 0x22222222}, {cpu_regs::STATE_ST0, 0x1800},
				{cpu_regs::STATE_ST1, index + 1 >= 512 ? 0x0100 : 0}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x short immediate load conformance: PASS variants=512 sxm_settings=2\n");
		program.write_word(0x05e3, 0x81f8); // STL B,*(0f21h)
		write_words(program, {
			{0x05e4, 0x0f21}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f21, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234beef);
		return resume_atomic_at(0x05e0, 335);
	}
	if (m_phase == 335)
	{
		expect_opcode(0x81f8, data.read_word(0x0f21) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234beef &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"absolute STL B stores the low word in two cycles");
		write_words(program, {
			{0x05e3, 0x8192}, // STL B,*AR2+.
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0f24, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f24);
		return resume_atomic_at(0x05e0, 6538);
	}
	if (m_phase == 6538)
	{
		expect_opcode(0x8192,
			data.read_word(0x0f24) == 0xbeef &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234beef &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f25 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 STL B stores the low word before one AR2 increment and preserves B in one cycle");
		program.write_word(0x05e3, 0x8190); // STL B,*AR0+.
		data.write_word(0x0f26, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_AR0, 0x0f26}, {cpu_regs::STATE_AR1, 0x0f28},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6540);
	}
	if ((m_phase >= 6540 && m_phase <= 6542) || m_phase == native_store_phase)
	{
		const unsigned variant = m_phase == native_store_phase ? 3 : m_phase - 6540;
		const u16 opcode = variant == 0 ? 0x8190 : variant == 1 ? 0x8191 : variant == 2 ? 0x8090 : 0x8194;
		const u16 destination = variant == 1 ? 0x0f28 : variant == 3 ? 0x0f2a : 0x0f26;
		expect_opcode(opcode,
			data.read_word(destination) == (variant == 2 ? 0x5678 : 0xbeef) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234beef &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == (variant == 1 || variant == 3 ? 0x0f26 : 0x0f27) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == (variant == 1 ? 0x0f29 : 0x0f28) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f25 &&
			(variant != 3 || (m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f2b &&
				data.read_word(0x0f2b) == 0xcafe && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800)) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 STL selects A/B and AR0/AR1/AR4 independently, postincrements only its destination and preserves accumulators/status in one cycle");
		if (variant < 3)
		{
			program.write_word(0x05e3, variant == 0 ? 0x8191 : variant == 1 ? 0x8090 : 0x8194);
			data.write_word(0x0f26, 0);
			data.write_word(0x0f28, 0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0f26);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0f28);
			if (variant == 2)
			{
				m_native_store_context = {u16(m_cpu->state_int(tms320c54x_device::STATE_AR4)),
					u16(m_cpu->state_int(tms320c54x_device::STATE_ST0)), u16(m_cpu->state_int(tms320c54x_device::STATE_ST1))};
				m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f2a);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
				m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
				data.write_word(0x0f2a, 0);
				data.write_word(0x0f2b, 0xcafe);
			}
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(variant == 2 ? native_store_phase : m_phase + 1);
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_AR4, m_native_store_context[0]);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, m_native_store_context[1]);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, m_native_store_context[2]);
		program.write_word(0x05e3, 0x7582); // PORTW *AR2,0124h
		write_words(program, {
			{0x05e4, 0x0124}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f20, 0xface);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f20);
		return resume_atomic_at(0x05e0, 336);
	}
	if (m_phase == 336)
	{
		expect_opcode(0x7582, m_middle_port_value == 0xface &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5,
				"PORTW *AR2 emits memory word without modifying AR2 in two cycles");
		program.write_word(0x05e3, 0x7583); // PORTW *AR3,0124h.
		data.write_word(0x0f22, 0xbeef);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f22);
		return resume_atomic_at(0x05e0, 6536);
	}
	if (m_phase == 6536)
	{
		expect_opcode(0x7583, m_middle_port_value == 0xbeef &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f22 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
			data.read_word(0x0f22) == 0xbeef &&
			m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 PORTW selects AR3 rather than AR2, preserves source/pointers and consumes the port extension in two cycles");
		program.write_word(0x05e3, 0x7595); // PORTW *AR5+,0124h.
		data.write_word(0x0f26, 0xcafe);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f26);
		return resume_atomic_at(0x05e0, 6551);
	}
	if (m_phase == 6551 || m_phase == 6552)
	{
		const bool ar2 = m_phase == 6552;
		expect_opcode(ar2 ? 0x7592 : 0x7595,
			m_middle_port_value == (ar2 ? 0xface : 0xcafe) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (ar2 ? 0x0f29 : 0x0f20) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f22 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f27 &&
			data.read_word(ar2 ? 0x0f28 : 0x0f26) == (ar2 ? 0xface : 0xcafe) &&
			m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 PORTW selects AR2/AR5 independently, emits the pre-increment word, preserves source memory and increments once in two cycles");
		if (!ar2)
		{
			program.write_word(0x05e3, 0x7592); // PORTW *AR2+,0124h.
			data.write_word(0x0f28, 0xface);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f28);
			return resume_atomic_at(0x05e0, 6552);
		}
		program.write_word(0x05e3, 0x758a); // PORTW *AR2-,0124h.
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f28);
		return resume_atomic_at(0x05e0, 6571);
	}
	if (m_phase == 6571)
	{
		expect_opcode(0x758a,
			m_middle_port_value == 0xface && data.read_word(0x0f28) == 0xface &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f27 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f22 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f27 &&
			m_port_writes == 3 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 PORTW emits old AR2 memory before one decrement, preserves source/unrelated pointers and costs two cycles");
		return begin_atomic_case(memory_route_examples()[0], 337);
	}
	if (m_phase >= 337 && m_phase <= 340)
	{
		auto const &cases = memory_route_examples();
		unsigned const index = 0 + m_phase - 337;
		verify_atomic_case(cases[index]);
		if (m_phase < 340) return begin_atomic_case(cases[index + 1], m_phase + 1);
		program.write_word(0x05e3, 0xfa4d); // BCD 05f0h, BEQ
		program.write_word(0x05e4, 0x05f0);
		program.write_word(0x05e5, 0xe801); // Delay slot 1
		program.write_word(0x05e6, 0xe902); // Delay slot 2 changes B
		program.write_word(0x05e7, 0xe803); // Fall-through only
		write_words(program, {
			{0x05e8, 0x75f8}, {0x05e9, 0x0d00}, {0x05ea, 0x0124}, {0x05eb, 0xf5e1}, {0x05f0, 0x75f8}, {0x05f1, 0x0d00},
			{0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(341);
	}
	if (m_phase == 341)
	{
		expect_opcode(0xfa4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"BCD BEQ uses pre-slot B and executes both delay words");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 1}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(342);
	}
	if (m_phase == 342)
	{
		expect_opcode(0xfa4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ec &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"BCD BEQ false executes slots and falls through");
		write_words(program, {
			{0x05e3, 0xfc4d}, // RC BEQ
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x02ff, 0x05f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_SP, 0x02ff}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(343);
	}
	if (m_phase == 343)
	{
		expect_opcode(0xfc4d, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"RC BEQ pops return address in five cycles when B is zero");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_SP, 0x02ff}, {cpu_regs::STATE_B, 1}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(344);
	}
	if (m_phase == 344)
	{
		expect_opcode(0xfc4d, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"RC BEQ leaves stack untouched in three cycles when B is nonzero");
		write_words(program, {
			{0x05e3, 0x47e2}, // RPT *AR2(5)
			{0x05e4, 5},
			{0x05e5, 0x6d10}, // MAR *AR0+
			{0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		data.write_word(0x0f05, 2);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(345);
	}
	if (m_phase == 345)
	{
		expect_opcode(0x47e2, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
				"long-offset RPT Smem runs MAR three times after four-cycle setup");
		return begin_atomic_case(memory_route_examples()[4], 346);
	}
	if (m_phase >= 346 && m_phase <= 354)
	{
		auto const &cases = memory_route_examples();
		unsigned const index = 4 + m_phase - 346;
		verify_atomic_case(cases[index]);
		if (m_phase < 354) return begin_atomic_case(cases[index + 1], m_phase + 1);
		write_words(program, {
			{0x05e3, 0xf84e}, // BC 05f0,BGT
			{0x05e4, 0x05f0},
			{0x05e5, 0x75f8}, // Fallthrough marker.
			{0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
			{0x05f0, 0x75f8}, // Taken marker.
			{0x05f1, 0x0d00}, {0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 1);
		return resume_atomic_at(0x05e0, 355);
	}
	if (m_phase == 355 || m_phase == 356)
	{
		expect_opcode(0xf84e, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == (m_phase == 355 ? 8 : 6) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 355 ? 0x05f4 : 0x05e9),
				"ROM4 BC BGT tests signed 40-bit B and costs five/three cycles");
		if (m_phase == 355)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff00000000ULL);
			return resume_atomic_at(0x05e0, 356);
		}
		write_words(program, {
			{0x05e3, 0xec01}, // RPT #1
			{0x05e4, 0x6d10}, // MAR *AR0+
			{0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0f00);
		return resume_atomic_at(0x05e0, 357);
	}
	if (m_phase == 357)
	{
		expect_opcode(0xec01, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f02 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 RPT #1 executes MAR twice after one-cycle setup");
		write_words(program, {
			{0x05e3, 0xfa30}, // BCD 05f0,TC
			{0x05e4, 0x05f0},
			{0x05e5, 0xe801}, // Delay slot 1
			{0x05e6, 0xe902}, // Delay slot 2
			{0x05e7, 0xe803}, // Fallthrough only
			{0x05e8, 0x75f8}, {0x05e9, 0x0d00}, {0x05ea, 0x0124}, {0x05eb, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000); // TC true
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(358);
	}
	if (m_phase == 358 || m_phase == 359)
	{
		expect_opcode(0xfa30,
				m_cpu->state_int(tms320c54x_device::STATE_A) == (m_phase == 358 ? 1 : 3) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 358 ? 0x05f4 : 0x05ec) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle ==
					(m_phase == 358 ? 8 : 9),
				"ROM4 BCD TC executes both delay words and selects the correct target");
		if (m_phase == 358)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); // TC false
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(359);
		}
		return begin_atomic_case(memory_route_examples()[13], 360);
	}
	if (m_phase >= 360 && m_phase <= 361)
	{
		auto const &cases = memory_route_examples();
		unsigned const index = 13 + m_phase - 360;
		verify_atomic_case(cases[index]);
		if (m_phase < 361) return begin_atomic_case(cases[index + 1], m_phase + 1);
		program.write_word(0x05e3, 0x75ea); // PORTW *+AR2(5),0124
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 5);
		data.write_word(0x0f05, 0xabcd);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 362);
	}
	if (m_phase == 362)
	{
		expect_opcode(0x75ea, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 3 && m_middle_port_value == 0xabcd &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"long-offset PORTW consumes port before offset and takes three cycles");
		return begin_atomic_case(memory_route_examples()[15], 363);
	}
	if (m_phase == 363)
	{
		auto const &cases = memory_route_examples();
		unsigned const index = 15 + m_phase - 363;
		verify_atomic_case(cases[index]);
		program.write_word(0x05e3, 0x7dea); // MVDP *+AR2(5),0600
		program.write_word(0x05e4, 0x0600);
		program.write_word(0x05e5, 5);
		program.write_word(0x0600, 0);
		data.write_word(0x0f05, 0xbeef);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 364);
	}
	if (m_phase == 364)
	{
		expect_opcode(0x7dea, program.read_word(0x0600) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"long-offset MVDP consumes pmad before offset and takes five cycles");
		program.write_word(0x05e3, 0x7fea); // WRITA *+AR2(5)
		write_words(program, {
			{0x05e4, 5}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1}, {0x0600, 0},
		});
		data.write_word(0x0f05, 0x1234);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0600}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(365);
	}
	if (m_phase == 365)
	{
		expect_opcode(0x7fea, program.read_word(0x0600) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"long-offset WRITA stores the data word at A in six cycles");
		return begin_atomic_case(memory_route_examples()[16], 366);
	}
	if (m_phase >= 366 && m_phase <= 367)
	{
		auto const &cases = memory_route_examples();
		unsigned const index = 16 + m_phase - 366;
		verify_atomic_case(cases[index]);
		if (m_phase < 367) return begin_atomic_case(cases[index + 1], m_phase + 1);
		program.write_word(0x05e3, 0x74ea); // PORTR 0123,*+AR2(5)
		program.write_word(0x05e4, 0x0123);
		program.write_word(0x05e5, 5);
		data.write_word(0x0f05, 0);
		m_port_reads = 0;
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 368);
	}
	if (m_phase == 368)
	{
		expect_opcode(0x74ea, data.read_word(0x0f05) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_reads == 1 && m_port_writes == 2 &&
				m_middle_port_cycle - m_first_port_cycle == 3,
				"long-offset PORTR consumes port before offset and takes three cycles");
		return begin_atomic_case(memory_route_examples()[18], 369);
	}
	if (m_phase >= 369 && m_phase <= 373)
	{
		auto const &cases = memory_route_examples();
		unsigned const index = 18 + m_phase - 369;
		verify_atomic_case(cases[index]);
		if (m_phase < 373) return begin_atomic_case(cases[index + 1], m_phase + 1);
		program.write_word(0x05e3, 0x1080); // LD *AR0,A.
		data.write_word(0x0f22, 0x8002);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0x0f22}, {cpu_regs::STATE_B, 0x87654321}, {cpu_regs::STATE_T, 0x1234},
			{cpu_regs::STATE_ST0, 0x0aa5}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6591);
	}
	if (m_phase == 6591 || m_phase == 6592)
	{
		const bool signed_operand = m_phase == 6591;
		expect_opcode(0x1080,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (signed_operand ? 0xffffff8002ULL : 0x8002) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f22 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f20 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x87654321 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (signed_operand ? 0x0100 : 0) &&
			data.read_word(0x0f22) == 0x8002 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 stationary AR0 load selects its source, obeys SXM and preserves pointers/B/T/status in one cycle");
		if (signed_operand)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			return resume_atomic_at(0x05e0, 6592);
		}
		return begin_atomic_case(memory_route_examples()[23], 374);
	}
	if (m_phase >= 374 && m_phase <= 376)
	{
		auto const &cases = memory_route_examples();
		unsigned const index = 23 + m_phase - 374;
		verify_atomic_case(cases[index]);
		if (m_phase < 376) return begin_atomic_case(cases[index + 1], m_phase + 1);
		write_words(program, {
			{0x05e3, 0xf517}, // ADD A >> 9,B
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 3);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM off
		return resume_atomic_at(0x05e0, 377);
	}
	if (m_phase == 377 || m_phase == 378)
	{
		expect_opcode(0xf517,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80000000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(m_phase == 377 ? 0x7fc00003ULL : 0xffffc00003ULL) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 ADD A >> 9,B uses SXM-controlled fill in one cycle");
		if (m_phase == 377)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM on
			return resume_atomic_at(0x05e0, 378);
		}
		write_words(program, {
			{0x05e3, 0xf4e2}, // BACC A
			{0x05f0, 0x75f8}, {0x05f1, 0x0d00}, {0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x05f0}, {cpu_regs::STATE_B, 0x05e4}, {cpu_regs::STATE_SP, 0x0300},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(379);
	}
	if (m_phase == 379)
	{
		expect_opcode(0xf4e2, m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05f4 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"ROM4 BACC A branches to A's low word without a stack push in six cycles");
		write_words(program, {
			{0x05e3, 0xf474}, // SFTA A,-12
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000800ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM off
		return resume_atomic_at(0x05e0, 380);
	}
	if (m_phase == 380 || m_phase == 381)
	{
		expect_opcode(0xf474,
				m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 380 ? 0x00ff80000ULL : 0xfffff80000ULL) &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 SFTA A,-12 uses SXM fill and exports shifted-out bit 11 to carry");
		if (m_phase == 380)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000800ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM on
			return resume_atomic_at(0x05e0, 381);
		}
		program.write_word(0x05e3, 0x7722); // STM #lk, MMR 22h
		write_words(program, {
			{0x05e4, 0xa55a}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0022, 0);
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 382);
	}
	if (m_phase == 382)
	{
		expect_opcode(0x7722, data.read_word(0x0022) == 0xa55a &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 STM #lk,MMR22 includes the peripheral access cycle");
		write_words(program, {
			{0x05e3, 0xf537}, // SUB A >> 9,B
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM off
		return resume_atomic_at(0x05e0, 383);
	}
	if (m_phase == 383 || m_phase == 384)
	{
		expect_opcode(0xf537,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80000000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(m_phase == 383 ? 0xff80400000ULL : 0x00400000ULL) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"shifted SUB uses SXM-controlled fill without changing A in one cycle");
		if (m_phase == 383)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM on
			return resume_atomic_at(0x05e0, 384);
		}
		program.write_word(0x05e3, 0x6184); // BITF *AR4,#00f0
		write_words(program, {
			{0x05e4, 0x00f0}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f20, 0x00a0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0f20}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(385);
	}
	if (m_phase == 385 || m_phase == 386)
	{
		expect_opcode(0x6184,
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1800) ==
				(m_phase == 385 ? 0x1800 : 0x0800) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 BITF *AR4 sets and clears TC while preserving carry in two cycles");
		if (m_phase == 385)
		{
			data.write_word(0x0f20, 0x0a00);
			m_port_writes = 0;
			return resume_atomic_at(0x05e0, 386);
		}
		return begin_atomic_case(absolute_operand_examples()[0], 387);
	}
	if (m_phase >= 387 && m_phase <= 401)
	{
		auto const &cases = absolute_operand_examples();
		unsigned const index = m_phase - 387;
		verify_atomic_case(cases[index]);
		if (m_phase < 401) return begin_atomic_case(cases[index + 1], m_phase + 1);
		write_words(program, {
			{0x05e3, 0xf842}, // BC 05f0,AGEQ
			{0x05e4, 0x05f0},
			{0x05e5, 0x75f8}, // Fallthrough marker.
			{0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
			{0x05f0, 0x75f8}, // Taken marker.
			{0x05f1, 0x0d00}, {0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		return resume_atomic_at(0x05e0, 402);
	}
	if (m_phase == 402 || m_phase == 403)
	{
		expect_opcode(0xf842, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == (m_phase == 402 ? 8 : 6) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 402 ? 0x05f4 : 0x05e9),
				"ROM4 BC AGEQ takes zero A and rejects negative A at five/three cycles");
		if (m_phase == 402)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
			return resume_atomic_at(0x05e0, 403);
		}
		return begin_atomic_case(absolute_operand_examples()[15], 404);
	}
	if (m_phase >= 404 && m_phase <= 406)
	{
		auto const &cases = absolute_operand_examples();
		unsigned const index = m_phase - 404 + 15;
		verify_atomic_case(cases[index]);
		if (m_phase < 406) return begin_atomic_case(cases[index + 1], m_phase + 1);
		write_words(program, {
			{0x05e3, 0xf945}, // CC 05f0,AEQ
			{0x05e4, 0x05f0},
			{0x05e5, 0x75f8}, // Fallthrough marker.
			{0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
			{0x05f0, 0x75f8}, // Taken marker.
			{0x05f1, 0x0d00}, {0x05f2, 0x0124}, {0x05f3, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(407);
	}
	if (m_phase == 407 || m_phase == 408)
	{
		expect_opcode(0xf945, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == (m_phase == 407 ? 8 : 6) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 407 ? 0x05f4 : 0x05e9) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == (m_phase == 407 ? 0x02ff : 0x0300) &&
				data.read_word(0x02ff) == (m_phase == 407 ? 0x05e5 : 0xa55a),
				"ROM4 CC AEQ pushes return only on the five-cycle taken path");
		if (m_phase == 407)
		{
			data.write_word(0x02ff, 0xa55a);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(408);
		}
		program.write_word(0x05e3, 0xfc44); // RC ANEQ
		program.write_word(0x05e4, 0x75f8); // Fallthrough marker.
		program.write_word(0x05e5, 0x0d00);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		data.write_word(0x02ff, 0x05f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_SP, 0x02ff}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(409);
	}
	if (m_phase == 409 || m_phase == 410)
	{
		expect_opcode(0xfc44, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == (m_phase == 409 ? 8 : 6) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == (m_phase == 409 ? 0x05f4 : 0x05e8) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == (m_phase == 409 ? 0x0300 : 0x02ff),
				"ROM4 RC ANEQ pops only on the five-cycle taken path");
		if (m_phase == 409)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_SP, 0x02ff}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(410);
		}
		return begin_atomic_case(absolute_operand_examples()[18], 411);
	}
	if (m_phase >= 411 && m_phase <= 422)
	{
		auto const &cases = absolute_operand_examples();
		unsigned const index = m_phase - 411 + 18;
		verify_atomic_case(cases[index]);
		if (m_phase < 422) return begin_atomic_case(cases[index + 1], m_phase + 1);
		write_words(program, {
			{0x05e3, 0xff4d}, // XC 2,BEQ
			{0x05e4, 0xe801}, // LD #1,A
			{0x05e5, 0xe902}, // LD #2,B
			{0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(424);
	}
	if (m_phase == 424)
	{
		expect_opcode(0xff4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 XC 2,BEQ executes two words after a one-cycle condition test");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 1}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(425);
	}
	if (m_phase == 425)
	{
		expect_opcode(0xff4d, m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 1 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"rejected XC 2,BEQ executes two NOP slots at their cycle cost");
		program.write_word(0x0048, 0x0083); // ISR samples A through *AR3.
		program.write_word(0x0049, 0xf4eb); // RETE
		program.write_word(0x05e3, 0xff4d); // XC 2,BEQ
		program.write_word(0x05e4, 0x00f8); // ADD *(absolute),A
		program.write_word(0x05e5, 0x0060); // Read raises INT2.
		m_port_writes = 0;
		m_port_writes_at_irq = 0;
		m_repeat_reads = 0;
		m_irq_trigger_read = 1;
		m_irq_accumulator = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR3, 0x0061}, {cpu_regs::STATE_SP, 0x02ff},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_IMR, 4}, {cpu_regs::STATE_IFR, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(426);
	}
	if (m_phase == 426)
	{
		expect_opcode(0xff4d, m_repeat_reads == 1 && m_irq_accumulator == 1 &&
				m_port_writes_at_irq == 1 && m_port_writes == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 1,
				"XC 2 releases a pending interrupt after one two-word instruction");
		m_cpu->set_input_line(2, CLEAR_LINE);
		write_words(program, {
			{0x05e3, 0x1183}, // LD *AR3,B
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0fa0, 0xfffe);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0fa0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		set_registers(*m_cpu, {
			{cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(427);
	}
	if (m_phase == 427)
	{
		expect_opcode(0x1183, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffffffeULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0fa0 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD *AR3,B sign-extends under SXM without modifying AR3");
		program.write_word(0x05e3, 0x0093); // ADD *AR3+,A
		data.write_word(0x0fa1, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x7fffffff}, {cpu_regs::STATE_AR3, 0x0fa1}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0200}, // OVM
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(428);
	}
	if (m_phase == 428)
	{
		expect_opcode(0x0093, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0fa2 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 ADD *AR3+,A saturates under OVM and sets sticky overflow in one cycle");
		program.write_word(0x05e3, 0xf1a0); // OR A,B
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff00000000ULL}, {cpu_regs::STATE_B, 0xff}, {cpu_regs::STATE_ST0, 0x0c00},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(429);
	}
	if (m_phase == 429)
	{
		expect_opcode(0xf1a0, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff00000000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000ffULL &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0c00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 OR A,B combines all 40 bits without changing source or status");
		program.write_word(0x05e3, 0xf2c0); // XOR B,A
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff000000f0ULL}, {cpu_regs::STATE_B, 0xff0000000fULL}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(430);
	}
	if (m_phase == 430)
	{
		expect_opcode(0xf2c0, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff0000000fULL &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0c00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 XOR B,A combines all 40 bits without changing source or status");
		program.write_word(0x05e3, 0xf620); // SUB B,A
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x7fffffff}, {cpu_regs::STATE_B, 0xffffffffffULL}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0200}, // OVM
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(431);
	}
	if (m_phase == 431)
	{
		expect_opcode(0xf620, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fffffff &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffffffULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 SUB B,A saturates signed-32 overflow without changing B");
		program.write_word(0x05e3, 0x818a); // STL B,*AR2-
		data.write_word(0x0f10, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_AR2, 0x0f10}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(432);
	}
	if (m_phase == 432)
	{
		expect_opcode(0x818a, data.read_word(0x0f10) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f0f &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 STL B,*AR2- stores before decrement without changing carry");
		program.write_word(0x05e3, 0x81d3); // STL B,*AR3+%
		data.write_word(0x0ffb, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0xabcd}, {cpu_regs::STATE_AR3, 0x0ffb}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(433);
	}
	if (m_phase == 433)
	{
		expect_opcode(0x81d3, data.read_word(0x0ffb) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0ff8 &&
				m_cpu->state_int(tms320c54x_device::STATE_BK) == 4 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 STL B,*AR3+% wraps a four-word circular buffer after store");
		program.write_word(0x05e3, 0x8282); // STH A,*AR2
		data.write_word(0x0f90, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7ffff0000ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0x0001); // SST
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 434);
	}
	if (m_phase == 434)
	{
		expect_opcode(0x8282, data.read_word(0x0f90) == 0x7fff &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7ffff0000ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"PMST.SST clamps signed STH without changing accumulator or cycle cost");
		program.write_word(0x05e3, 0x82d3); // STH A,*AR3+%.
		data.write_word(0x0f93, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_B, 0xff87654321ULL}, {cpu_regs::STATE_AR3, 0x0f93},
			{cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ST0, 0x0aa5}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6515);
	}
	if (m_phase == 6515 || m_phase == 6516 || m_phase == 9227 || m_phase == 9228)
	{
		bool const ar2 = m_phase >= 9227;
		bool const store_b = m_phase == 6516 || m_phase == 9228;
		expect_opcode(ar2 ? (store_b ? 0x83d2 : 0x82d2) : (store_b ? 0x83d3 : 0x82d3),
			data.read_word(0x0f93) == (store_b ? 0x8765 : 0x1234) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			(!ar2 || (m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
				data.read_word(0x0f92) == 0xbeef && data.read_word(0x0f94) == 0xcafe)) &&
			m_cpu->state_int(tms320c54x_device::STATE_BK) == 4 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff87654321ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 STH A/B stores the high word before circular wrap without changing accumulators/status");
		if (m_phase != 9228)
		{
			const unsigned next = m_phase == 6515 ? 6516 : m_phase == 6516 ? 9227 : 9228;
			program.write_word(0x05e3, next == 6516 ? 0x83d3 : next == 9227 ? 0x82d2 : 0x83d2);
			data.write_word(0x0f93, 0);
			if (next >= 9227)
			{
				data.write_word(0x0f92, 0xbeef);
				data.write_word(0x0f94, 0xcafe);
				m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f93);
			}
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR3, next >= 9227 ? 0x0f90 : 0x0f93);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(next);
		}
		program.write_word(0x05e3, 0x8292); // STH A,*AR2+.
		data.write_word(0x0f95, 0);
		data.write_word(0x0f96, 0xcafe);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f95);
		return resume_atomic_at(0x05e0, 6575);
	}
	if (m_phase == 6575)
	{
		expect_opcode(0x8292,
			data.read_word(0x0f95) == 0x1234 && data.read_word(0x0f96) == 0xcafe &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f96 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f90 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff87654321ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 STH A through AR2 stores the high word before increment, preserves accumulators/status/neighbor and costs one cycle");
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 1);
		program.write_word(0x05e3, 0x4f82); // DST B,*AR2
		data.write_word(0x0f90, 0);
		data.write_word(0x0f91, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x8ffff0000ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear
		return resume_atomic_at(0x05e0, 435);
	}
	if (m_phase == 435)
	{
		expect_opcode(0x4f82, data.read_word(0x0f90) == 0xffff &&
				data.read_word(0x0f91) == 0xffff &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8ffff0000ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"PMST.SST clamps unsigned DST without changing accumulator or two-cycle cost");
		program.write_word(0x05e3, 0xd6e1); // ST B,*AR3 || MACR *AR4+0%,A
		data.write_word(0x0f90, 0);
		data.write_word(0x0fa0, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0x7ffff0000ULL}, {cpu_regs::STATE_T, 0},
			{cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR3, 0x0fa0}, {cpu_regs::STATE_AR4, 0x0f90},
			{cpu_regs::STATE_BK, 4},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM, ASM=0
		return resume_atomic_at(0x05e0, 436);
	}
	if (m_phase == 436)
	{
		expect_opcode(0xd6e1, data.read_word(0x0fa0) == 0x7fff &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7ffff0000ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"PMST.SST clamps parallel ST old B while MACR keeps one-cycle cost");
		data.write_word(0x0fa0, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0x3ffff0000ULL}, {cpu_regs::STATE_AR3, 0x0fa0},
			{cpu_regs::STATE_AR4, 0x0f90},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0101); // SXM, ASM=1
		return resume_atomic_at(0x05e0, 437);
	}
	if (m_phase == 437)
	{
		expect_opcode(0xd6e1, data.read_word(0x0fa0) == 0x7fff &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x3ffff0000ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"parallel ST shifts by ASM before SST clamps the stored value");
		program.write_word(0x05e3, 0x1093); // LD *AR3+,A
		data.write_word(0x0fa0, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_AR3, 0x0fa0},
			{cpu_regs::STATE_ST0, 0x0800},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 438);
	}
	if (m_phase == 438)
	{
		expect_opcode(0x1093, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0fa1 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD *AR3+,A sign-extends and postincrements in one cycle");
		program.write_word(0x05e3, 0x1084); // LD *AR4,A
		data.write_word(0x0f90, 0x8002);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f90);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // SXM clear
		return resume_atomic_at(0x05e0, 439);
	}
	if (m_phase == 439)
	{
		expect_opcode(0x1084, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8002 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f90 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD *AR4,A zero-extends without SXM and leaves pointer unchanged");
		program.write_word(0x05e3, 0x1094); // LD *AR4+,A
		data.write_word(0x0f90, 0xfffd);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 440);
	}
	if (m_phase == 440)
	{
		expect_opcode(0x1094, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffffdULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f91 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD *AR4+,A sign-extends and postincrements in one cycle");
		program.write_word(0x05e3, 0x4912); // LDM AR2,B
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x9abc}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(441);
	}
	if (m_phase == 441)
	{
		expect_opcode(0x4912, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x9abc &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x9abc &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LDM AR2,B zero-extends despite SXM in one cycle");
		program.write_word(0x05e3, 0x4911); // LDM AR1,B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR1, 0xabcd}, {cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0xffffffffffULL},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6560);
	}
	if (m_phase == 6560)
	{
		expect_opcode(0x4911,
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x9abc &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0100 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 LDM AR1,B selects AR1 rather than AR2, zero-extends despite SXM and preserves A/status/source in one cycle");
		write_words(program, {
			{0x05e3, 0x730b}, // MVMD BL,dmad
			{0x05e4, 0x0fa0}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0fa0, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
		return resume_atomic_at(0x05e0, 442);
	}
	if (m_phase == 442)
	{
		expect_opcode(0x730b, data.read_word(0x0fa0) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 MVMD BL,dmad copies without modifying BL in two cycles");
		program.write_word(0x05e3, 0x6882); // ANDM #0f0f,*AR2
		program.write_word(0x05e4, 0x0f0f);
		data.write_word(0x0f90, 0xff00);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f90);
		return resume_atomic_at(0x05e0, 443);
	}
	if (m_phase == 443)
	{
		expect_opcode(0x6882, data.read_word(0x0f90) == 0x0f00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f90 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 ANDM #lk,*AR2 preserves carry and costs two cycles");
		program.write_word(0x05e3, 0x6884); // ANDM #ff0f,*AR4
		program.write_word(0x05e4, 0xff0f);
		data.write_word(0x0f92, 0x1234);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f92);
		return resume_atomic_at(0x05e0, 444);
	}
	if (m_phase == 444)
	{
		expect_opcode(0x6884, data.read_word(0x0f92) == 0x1204 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f92 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 ANDM #lk,*AR4 selects its own pointer in two cycles");
		program.write_word(0x05e3, 0x6db1); // MAR *AR1+0
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 3}, {cpu_regs::STATE_AR1, 0x0f10}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_ST1, 0}, // CMPT clear
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(445);
	}
	if (m_phase == 445)
	{
		expect_opcode(0x6db1, m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f13 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == 3 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe800) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 MAR *AR1+0 adds AR0 without changing ARP in one cycle");
		program.write_word(0x05e3, 0x6dc2); // MAR *AR2-%
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0f08}, {cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(446);
	}
	if (m_phase == 446)
	{
		expect_opcode(0x6dc2, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f0b &&
				m_cpu->state_int(tms320c54x_device::STATE_BK) == 4 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe800) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 MAR *AR2-% wraps at BK without changing status in one cycle");
		program.write_word(0x05e3, 0xe900); // LD #0,B
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffffULL);
		return resume_atomic_at(0x05e0, 447);
	}
	if (m_phase == 447)
	{
		expect_opcode(0xe900, m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe800) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD #0,B clears guard and payload in one cycle");
		program.write_word(0x05e3, 0xe901); // LD #1,B
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xffffffffffULL);
		return resume_atomic_at(0x05e0, 448);
	}
	if (m_phase == 448)
	{
		expect_opcode(0xe901, m_cpu->state_int(tms320c54x_device::STATE_B) == 1 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe800) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD #1,B clears prior guard bits in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xec1f}, // RPT #31
			{0x05e3, 0x7693}, // ST #lk,*AR3+
			{0x05e4, 0x5a3c}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		for (unsigned i = 0; i != 32; ++i)
			data.write_word(0x0d00 + i, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0d00);
		return resume_atomic_at(0x05e0, 449);
	}
	if (m_phase == 449)
	{
		bool all_stored = true;
		for (unsigned i = 0; i != 32; ++i)
			all_stored &= data.read_word(0x0d00 + i) == 0x5a3c;
		expect_opcode(0xec1f, all_stored &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 67,
				"ROM4 RPT #31 repeats a two-cycle ST body 32 times");
		expect_opcode(0x7693, all_stored &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d20,
				"ROM4 ST #lk,*AR3+ writes consecutive ordinary-memory words");
		program.write_word(0x05e2, 0xf495);
		program.write_word(0x05e3, 0x7690); // ST #lk,*AR0+.
		program.write_word(0x05e4, 0xbeef);
		data.write_word(0x0f5f, 0x1234);
		data.write_word(0x0f60, 0);
		data.write_word(0x0f61, 0xabcd);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x11111111}, {cpu_regs::STATE_B, 0x22222222}, {cpu_regs::STATE_T, 0x3333},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_AR0, 0x0f60},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6553);
	}
	if (m_phase == 6553)
	{
		expect_opcode(0x7690,
			data.read_word(0x0f60) == 0xbeef &&
			data.read_word(0x0f5f) == 0x1234 && data.read_word(0x0f61) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f61 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d20 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x11111111 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x22222222 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x3333 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0100 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 ST #lk,*AR0+ consumes the literal, preserves neighbors/arithmetic state and postincrements after a two-cycle store");
		return begin_atomic_case(circular_marker_examples()[0], 450);
	}
	if (m_phase >= 450 && m_phase <= 453)
	{
		auto const &cases = circular_marker_examples();
		unsigned const index = m_phase - 450;
		verify_atomic_case(cases[index]);
		if (m_phase < 453) return begin_atomic_case(cases[index + 1], m_phase + 1);
		program.write_word(0x05e2, 0xff20); // XC 2,NTC
		program.write_word(0x05e3, 0x6d91); // MAR *AR1+
		program.write_word(0x05e4, 0x6d92); // MAR *AR2+
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(454);
	}
	if (m_phase == 454)
	{
		expect_opcode(0x6d92, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0131,
				"ROM4 MAR *AR2+ advances the second auxiliary register");
		expect_opcode(0xff20, m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0121 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0131 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 XC 2,NTC executes two one-word MAR slots when TC is clear");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130}, {cpu_regs::STATE_ST0, 0x1000},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(455);
	}
	if (m_phase == 455)
	{
		expect_opcode(0xff20, m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0120 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0130 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"rejected ROM4 XC 2,NTC charges two NOP slots without modifying ARs");
		return begin_atomic_case(circular_marker_examples()[4], 456);
	}
	if (m_phase >= 456 && m_phase <= 467)
	{
		auto const &cases = circular_marker_examples();
		unsigned const index = m_phase - 456 + 4;
		verify_atomic_case(cases[index]);
		if (m_phase < 467) return begin_atomic_case(cases[index + 1], m_phase + 1);
		m_phase = 9220;
	}
	if (m_phase >= 9220 && m_phase <= 9222)
	{
		if (m_phase != 9220)
			expect_opcode(m_phase == 9221 ? 0x6885 : 0x6985,
				data.read_word(0x0f40) == (m_phase == 9221 ? 0x0f00 : 0x0ff0) &&
				data.read_word(0x0f3f) == 0xbeef && data.read_word(0x0f41) == 0xcafe &&
				data.read_word(0x0f33) == 0x0ff0 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f40 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f33 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x76543210 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0100 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 AR5 immediate logical operation selects its word, preserves neighbors/accumulators/status/pointers and costs two cycles");
		if (m_phase < 9222)
		{
			program.write_word(0x05e2, m_phase == 9220 ? 0x6885 : 0x6985);
			program.write_word(0x05e3, m_phase == 9220 ? 0x0ff0 : 0x00f0);
			if (m_phase == 9220)
			{
				data.write_word(0x0f3f, 0xbeef);
				data.write_word(0x0f40, 0xff0f);
				data.write_word(0x0f41, 0xcafe);
				set_registers(*m_cpu, {
					{cpu_regs::STATE_AR5, 0x0f40}, {cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_B, 0x76543210},
					{cpu_regs::STATE_ST1, 0x0100},
				});
			}
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0x6fd2); // LD *AR2+%,5,A
		program.write_word(0x05e3, 0x0c45);
		data.write_word(0x0f23, 0xff80);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 0x0f23}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_ST1, 0x0100}, // SXM
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(468);
	}
	if (m_phase == 468)
	{
		expect_opcode(0x6fd2, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffff000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 shifted LD sign-extends before shift and wraps AR2 in two cycles");
		write_words(program, {
			{0x05e2, 0x7e8b}, // READA *AR3-
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1}, {0x0d40, 0xabcd},
		});
		data.write_word(0x0f33, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0d40}, {cpu_regs::STATE_AR3, 0x0f33}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(469);
	}
	if (m_phase == 469)
	{
		expect_opcode(0x7e8b, data.read_word(0x0f33) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0d40 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f32 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 READA uses A as program source and decrements AR3 after five cycles");
		program.write_word(0x05e2, 0x7e92); // READA *AR2+.
		data.write_word(0x0d40, 0xface); // Distinguish program source from data source.
		data.write_word(0x0f34, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f34);
		return resume_atomic_at(0x05e0, 6539);
	}
	if (m_phase == 6539)
	{
		expect_opcode(0x7e92,
			data.read_word(0x0f34) == 0xabcd && data.read_word(0x0d40) == 0xface &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0d40 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f35 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f32 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
			"ROM4 READA reads program rather than data memory at A and postincrements AR2 in five cycles");
	}
	if (m_phase == 470)
	{
		verify_atomic_case(register_bit_examples()[16]);
		program.write_word(0x05e2, 0xf508); // ADD A<<8,B
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x10000}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(471);
	}
	if (m_phase == 471)
	{
		expect_opcode(0xf508, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x133400 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 ADD A<<8,B updates B only in one cycle");
		program.write_word(0x05e2, 0xf500); // ADD A,B.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_B, 0x7fffffff}, {cpu_regs::STATE_T, 0x8888},
			{cpu_regs::STATE_ST0, 0x1800}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6554);
	}
	if (m_phase >= 6554 && m_phase <= 6557)
	{
		const unsigned index = m_phase - 6554;
		const bool destination_b = !(index & 1);
		const u16 st1 = index >= 2 ? 0x0200 : 0;
		const u64 result = index >= 2 ? 0x7fffffff : 0x80000000;
		expect_opcode(destination_b ? 0xf500 : 0xf600,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (destination_b ? 1 : result) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (destination_b ? result : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8888 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (destination_b ? 0x1200 : 0x1400) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == st1 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 ADD A/B into the opposite accumulator preserves its source/T/TC, selects destination overflow, clears carry and obeys OVM in one cycle");
		if (index < 3)
		{
			const bool next_b = !((index + 1) & 1);
			program.write_word(0x05e2, next_b ? 0xf500 : 0xf600);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, next_b ? 1 : 0x7fffffff}, {cpu_regs::STATE_B, next_b ? 0x7fffffff : 1},
				{cpu_regs::STATE_ST0, 0x1800}, {cpu_regs::STATE_ST1, index + 1 >= 2 ? 0x0200 : 0},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		program.write_word(0x05e2, 0xf400); // ADD A,A
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x7fffffff}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(472);
	}
	if (m_phase == 472)
	{
		expect_opcode(0xf400, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffffeULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0400) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 ADD A,A sets overflow across the 32-bit boundary in one cycle");
		write_words(program, {
			{0x05e2, 0xf028}, // LD #00aa,8,A
			{0x05e3, 0x00aa}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		return resume_atomic_at(0x05e0, 473);
	}
	if (m_phase == 473)
	{
		expect_opcode(0xf028, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xaa00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD #00aa,8,A consumes its immediate in two cycles");
		return begin_atomic_case(register_bit_examples()[17], 474);
	}
	if (m_phase == 474)
	{
	}
	if (m_phase == 475)
	{
		verify_atomic_case(register_bit_examples()[18]);
		program.write_word(0x05e2, 0x3182); // MPYA *AR2
		data.write_word(0x0f20, 7);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xfffffd0000ULL}, // A(32-16)=-3.
			{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR2, 0x0f20},
			{cpu_regs::STATE_ST1, 0}, // FRCT clear.
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(476);
	}
	if (m_phase == 476)
	{
		expect_opcode(0x3182, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffffebULL &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffd0000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 7 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 MPYA *AR2 writes signed product to B and operand to T in one cycle");
		program.write_word(0x05e2, 0x0885); // SUB *AR5,A
		data.write_word(0x0f25, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f25);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 477);
	}
	if (m_phase == 477)
	{
		expect_opcode(0x0885, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x17fff &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f25 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 SUB *AR5,A sign-extends the operand without pointer motion in one cycle");
		write_words(program, {
			{0x05e2, 0x1df8}, // XOR *(lk),B
			{0x05e3, 0x0f21}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0f21, 0x00f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0xff0000000fULL}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(478);
	}
	if (m_phase == 478)
	{
		expect_opcode(0x1df8, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000ffULL &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 absolute XOR into B preserves guard and status in two cycles");
		program.write_word(0x05e2, 0x80d3); // STL A,*AR3+%
		program.write_word(0x05e3, 0x818d); // STL B,*AR5-
		program.write_word(0x05e4, 0x81d2); // STL B,*AR2+%
		program.write_word(0x05e5, 0x8395); // STH B,*AR5+
		program.write_word(0x05e6, 0x75d6);
		program.write_word(0x05e7, 0x0124);
		program.write_word(0x05e8, 0xf5e1);
		write_words(data, {
			{0x0f22, 0}, {0x0f23, 0}, {0x0f24, 0}, {0x0f25, 0},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x89ababcd}, {cpu_regs::STATE_AR2, 0x0f22},
			{cpu_regs::STATE_AR3, 0x0f23}, {cpu_regs::STATE_AR5, 0x0f25}, {cpu_regs::STATE_BK, 4},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); // SST clear.
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
		return resume_atomic_at(0x05e0, 479);
	}
	if (m_phase == 479)
	{
		expect_opcode(0x80d3, data.read_word(0x0f23) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f20,
				"ROM4 STL A,*AR3+% stores before circular wrap");
		expect_opcode(0x818d, data.read_word(0x0f25) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f25,
				"ROM4 STL B,*AR5- stores at the old address");
		expect_opcode(0x81d2, data.read_word(0x0f22) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f23,
				"ROM4 STL B,*AR2+% stores before circular advance");
		expect_opcode(0x8395, data.read_word(0x0f24) == 0x89ab &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x89ababcd &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"four ROM4 accumulator stores preserve status and cost one cycle each");
		m_phase = 9229;
	}
	if (m_phase >= 9229 && m_phase <= 9231)
	{
		if (m_phase != 9229)
			expect_opcode(m_phase == 9230 ? 0x808c : 0x80d2,
				data.read_word(0x0f23) == 0x1234 && data.read_word(0x0f22) == 0xbeef && data.read_word(0x0f24) == 0xcafe &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == (m_phase == 9230 ? 0x0f22 : 0x0f23) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == (m_phase == 9231 ? 0x0f20 : 0x0f23) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f20 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x89ababcd &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e6 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 low A store precedes decrement/circular wrap and preserves neighbors/accumulators/status in one cycle");
		if (m_phase < 9231)
		{
			program.write_word(0x05e2, m_phase == 9229 ? 0x808c : 0x80d2);
			program.write_word(0x05e3, 0x75d6);
			program.write_word(0x05e4, 0x0124);
			program.write_word(0x05e5, 0xf5e1);
			data.write_word(0x0f22, 0xbeef);
			data.write_word(0x0f23, 0);
			data.write_word(0x0f24, 0xcafe);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR4, 0x0f23}, {cpu_regs::STATE_AR2, 0x0f23}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0x8815); // STLM A,AR5
		program.write_word(0x05e3, 0xe552); // MVDD *AR3-,*AR4
		program.write_word(0x05e4, 0xe754); // MVMM AR5,AR4
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		data.write_word(0x0f23, 0xcafe);
		data.write_word(0x0f26, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12340f25}, {cpu_regs::STATE_AR3, 0x0f23}, {cpu_regs::STATE_AR4, 0x0f26},
			{cpu_regs::STATE_AR5, 0}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(480);
	}
	if (m_phase == 480)
	{
		expect_opcode(0x8815,
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f25 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12340f25,
				"ROM4 STLM A,AR5 writes AL without changing A");
		expect_opcode(0xe552, data.read_word(0x0f26) == 0xcafe &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f22,
				"ROM4 MVDD copies the old AR3 word then decrements AR3");
		expect_opcode(0xe754,
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f25 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 MVMM copies AR5 to AR4; three moves cost one cycle each");
		program.write_word(0x05e2, 0xf843); // BC 05f0,ALT
		program.write_word(0x05e3, 0x05f0);
		program.write_word(0x05e4, 0x75d6); // Rejected-branch marker.
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		program.write_word(0x05f0, 0x75d6); // Taken-branch marker.
		program.write_word(0x05f1, 0x0124);
		program.write_word(0x05f2, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL);
		return resume_atomic_at(0x05e0, 481);
	}
	if (m_phase == 481 || m_phase == 482)
	{
		expect_opcode(0xf843,
				m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == (m_phase == 481 ? 7 : 5) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) ==
					(m_phase == 481 ? 0x05f3 : 0x05e7),
				"ROM4 BC ALT tests A's guard sign and costs five/three cycles");
		if (m_phase == 481)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0100000000ULL);
			return resume_atomic_at(0x05e0, 482);
		}
		program.write_word(0x05e2, 0xff30); // XC 2,TC
		program.write_word(0x05e3, 0x6d91); // MAR *AR1+
		program.write_word(0x05e4, 0x6d92); // MAR *AR2+
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130}, {cpu_regs::STATE_ST0, 0x1000},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(483);
	}
	if (m_phase == 483 || m_phase == 484)
	{
		expect_opcode(0xff30,
				m_cpu->state_int(tms320c54x_device::STATE_AR1) ==
					(m_phase == 483 ? 0x0121 : 0x0120) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) ==
					(m_phase == 483 ? 0x0131 : 0x0130) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 XC 2,TC executes or rejects two slots at five-cycle marker span");
		if (m_phase == 483)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130}, {cpu_regs::STATE_ST0, 0},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(484);
		}
		program.write_word(0x05e2, 0x1192); // LD *AR2+,B
		program.write_word(0x05e3, 0x6dd2); // MAR *AR2+%
		program.write_word(0x05e4, 0x7683); // ST #lk,*AR3
		program.write_word(0x05e5, 0x5aa5);
		program.write_word(0x05e6, 0x8085); // STL A,*AR5
		program.write_word(0x05e7, 0x75d6);
		program.write_word(0x05e8, 0x0124);
		program.write_word(0x05e9, 0xf5e1);
		data.write_word(0x0f22, 0x8000);
		data.write_word(0x0f30, 0);
		data.write_word(0x0f31, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234abcd}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR2, 0x0f22},
			{cpu_regs::STATE_AR3, 0x0f30}, {cpu_regs::STATE_AR5, 0x0f31}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_ST0, 0x0800},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
		return resume_atomic_at(0x05e0, 485);
	}
	if (m_phase == 485)
	{
		expect_opcode(0x1192,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffff8000ULL,
				"ROM4 LD *AR2+,B sign-extends under SXM");
		expect_opcode(0x6dd2,
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20,
				"ROM4 MAR *AR2+% wraps after the preceding linear load");
		expect_opcode(0x7683, data.read_word(0x0f30) == 0x5aa5 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f30,
				"ROM4 ST immediate writes the direct AR3 destination");
		expect_opcode(0x8085, data.read_word(0x0f31) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f31 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234abcd &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 load/MAR/immediate-store/accumulator-store cost 1+1+2+1 cycles");
		write_words(program, {
			{0x05e2, 0x771d}, // STM #lk,PMST
			{0x05e3, 0x4567}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0}, {cpu_regs::STATE_AR5, 0x0f13}, {cpu_regs::STATE_PMST, 0},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(486);
	}
	if (m_phase == 486)
	{
		expect_opcode(0x771d,
				m_cpu->state_int(tms320c54x_device::STATE_PMST) == 0x4567 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f13 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 STM writes direct PMST without pointer motion in two cycles");
		program.write_word(0x05e2, 0xff45); // XC 2,AEQ
		program.write_word(0x05e3, 0x6d91); // MAR *AR1+
		program.write_word(0x05e4, 0x6d92); // MAR *AR2+
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(487);
	}
	if (m_phase >= 487 && m_phase <= 490)
	{
		const bool accepted = m_phase == 487 || m_phase == 489;
		expect_opcode(m_phase < 489 ? 0xff45 : 0xff4e,
				m_cpu->state_int(tms320c54x_device::STATE_AR1) ==
					(accepted ? 0x0121 : 0x0120) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) ==
					(accepted ? 0x0131 : 0x0130) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 XC AEQ/BGT executes or rejects two guarded MAR slots");
		if (m_phase == 490)
		{
			program.write_word(0x05e2, 0x4812); // LDM AR2,A
			program.write_word(0x05e3, 0x8095); // STL A,*AR5+
			program.write_word(0x05e4, 0x4813); // LDM AR3,A
			program.write_word(0x05e5, 0x8095);
			program.write_word(0x05e6, 0x4814); // LDM AR4,A
			program.write_word(0x05e7, 0x8095);
			program.write_word(0x05e8, 0x4817); // LDM AR7,A
			program.write_word(0x05e9, 0x8095);
			program.write_word(0x05ea, 0x4910); // LDM AR0,B
			program.write_word(0x05eb, 0x75d6);
			program.write_word(0x05ec, 0x0124);
			program.write_word(0x05ed, 0xf5e1);
			for (u16 address = 0x0f40; address != 0x0f44; ++address)
				data.write_word(address, 0);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR0, 0x8000},
				{cpu_regs::STATE_AR2, 0x8002}, {cpu_regs::STATE_AR3, 0x8003}, {cpu_regs::STATE_AR4, 0x8004},
				{cpu_regs::STATE_AR5, 0x0f40}, {cpu_regs::STATE_AR7, 0x8007}, {cpu_regs::STATE_ST0, 0x0800},
			});
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
			return resume_atomic_at(0x05e0, 491);
		}
		if (m_phase == 488)
			program.write_word(0x05e2, 0xff4e); // XC 2,BGT
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A,
				m_phase == 487 ? 1 : 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_B,
				m_phase == 488 ? 0x0100000000ULL : 0xff00000000ULL);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		++m_phase;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase == 491)
	{
		expect_opcode(0x4812, data.read_word(0x0f40) == 0x8002,
				"ROM4 LDM AR2,A preserves the zero-extended load via STL");
		expect_opcode(0x4813, data.read_word(0x0f41) == 0x8003,
				"ROM4 LDM AR3,A preserves the zero-extended load via STL");
		expect_opcode(0x4814, data.read_word(0x0f42) == 0x8004,
				"ROM4 LDM AR4,A preserves the zero-extended load via STL");
		expect_opcode(0x4817, data.read_word(0x0f43) == 0x8007 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8007,
				"ROM4 LDM AR7,A zero-extends despite SXM");
		expect_opcode(0x4910,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f44 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 11,
				"five ROM4 LDM words and four STL stores cost one cycle each");
		write_words(program, {
			{0x05e2, 0xf200}, // ADD #lk,B,A
			{0x05e3, 0xfffe}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x0100}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0x0100}, // SXM.
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(492);
	}
	if (m_phase == 492)
	{
		expect_opcode(0xf200,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00fe &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0100 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 ADD #lk,B,A sign-extends under SXM and costs two cycles");
	}
	if (m_phase == 493)
	{
		verify_atomic_case(register_bit_examples()[19]);
		program.write_word(0x05e2, 0xf540); // LD A,0,B
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff000000f0ULL}, {cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(494);
	}
	if (m_phase == 494)
	{
		expect_opcode(0xf540,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff000000f0ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000f0ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0a00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 LD A,0,B preserves all 40 bits and sets OVB in one cycle");
		write_words(program, {
			{0x05e2, 0x10da}, // LD *AR2+0%,A
			{0x05e3, 0x6d93}, // MAR *AR3+
			{0x05e4, 0x7308}, // MVMD AL,dmad
			{0x05e5, 0x0f50},
			{0x05e6, 0x7313}, // MVMD AR3,dmad
			{0x05e7, 0x0f51},
			{0x05e8, 0x7681}, // ST #lk,*AR1
			{0x05e9, 0x5a5a}, {0x05ea, 0x75d6}, {0x05eb, 0x0124}, {0x05ec, 0xf5e1},
		});
		write_words(data, {
			{0x0f22, 0x8001}, {0x0f50, 0}, {0x0f51, 0}, {0x0f52, 0},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_AR0, 2},
			{cpu_regs::STATE_AR1, 0x0f52}, {cpu_regs::STATE_AR2, 0x0f22}, {cpu_regs::STATE_AR3, 0x0f30},
			{cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_ST0, 0x0800},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM.
		return resume_atomic_at(0x05e0, 495);
	}
	if (m_phase == 495)
	{
		expect_opcode(0x10da,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20,
				"ROM4 LD *AR2+0%,A sign-extends before circular advance");
		expect_opcode(0x6d93,
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f31,
				"ROM4 MAR *AR3+ updates the register in one cycle");
		expect_opcode(0x7308, data.read_word(0x0f50) == 0x8001,
				"ROM4 MVMD AL,dmad captures A's low word");
		expect_opcode(0x7313, data.read_word(0x0f51) == 0x0f31,
				"ROM4 MVMD AR3,dmad captures the modified register");
		expect_opcode(0x7681, data.read_word(0x0f52) == 0x5a5a &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f52 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
				"ROM4 load/MAR/two MVMDs/ST cost 1+1+2+2+2 cycles");
		program.write_word(0x05e2, 0x7718); // STM #lk,SP
		program.write_word(0x05e3, 0x0320);
		program.write_word(0x05e4, 0x7728); // STM #lk,028h
		program.write_word(0x05e5, 0x5aa5);
		program.write_word(0x05e6, 0x7758); // STM #lk,CLKMD
		program.write_word(0x05e7, 0x0002);
		program.write_word(0x05e8, 0x4858); // LDM CLKMD,A
		program.write_word(0x05e9, 0x75d6);
		program.write_word(0x05ea, 0x0124);
		program.write_word(0x05eb, 0xf5e1);
		data.write_word(0x0028, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(496);
	}
	if (m_phase == 496)
	{
		expect_opcode(0x7718,
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0320,
				"ROM4 STM direct SP writes the stack pointer");
		expect_opcode(0x7728, data.read_word(0x0028) == 0x5aa5,
				"ROM4 STM direct page-zero address writes backing data");
		expect_opcode(0x7758,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0003 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 12,
				"ROM4 CLKMD sequence includes peripheral waits for STM28, STM58 and LDM58");
		program.write_word(0x05e2, 0x8812); // STLM A,AR2
		program.write_word(0x05e3, 0x881a); // STLM A,BRC
		program.write_word(0x05e4, 0x731a); // MVMD BRC,dmad
		program.write_word(0x05e5, 0x0f50);
		program.write_word(0x05e6, 0x8915); // STLM B,AR5
		program.write_word(0x05e7, 0x891a); // STLM B,BRC
		program.write_word(0x05e8, 0x75d6);
		program.write_word(0x05e9, 0x0124);
		program.write_word(0x05ea, 0xf5e1);
		data.write_word(0x0f50, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678}, {cpu_regs::STATE_AR2, 0},
			{cpu_regs::STATE_AR5, 0}, {cpu_regs::STATE_BRC, 0}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(497);
	}
	if (m_phase == 497)
	{
		expect_opcode(0x8812,
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1234,
				"ROM4 STLM A,AR2 writes AL");
		expect_opcode(0x881a, data.read_word(0x0f50) == 0x1234,
				"ROM4 STLM A,BRC is captured before B overwrites BRC");
		expect_opcode(0x8915,
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x5678,
				"ROM4 STLM B,AR5 writes BL");
		expect_opcode(0x891a,
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"four ROM4 STLM stores and one MVMD cost 1+1+2+1+1 cycles");
		program.write_word(0x05e2, 0xe589); // MVDD *AR2+,*AR3+
		program.write_word(0x05e3, 0xe734); // MVMM AR3,AR4
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0f60, 0xbeef);
		data.write_word(0x0f70, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0f60}, {cpu_regs::STATE_AR3, 0x0f70}, {cpu_regs::STATE_AR4, 0},
			{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(498);
	}
	if (m_phase == 498)
	{
		expect_opcode(0xe589,
				data.read_word(0x0f70) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f61 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f71,
				"ROM4 MVDD reads/writes old addresses before both ARs advance");
		expect_opcode(0xe734,
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f71 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 MVMM copies the advanced AR3 in one cycle");
		program.write_word(0x05e2, 0xec06); // RPT #6
		program.write_word(0x05e3, 0x6d91); // MAR *AR1+
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0120);
		return resume_atomic_at(0x05e0, 499);
	}
	if (m_phase == 499)
	{
		expect_opcode(0xec06,
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0127 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
				"ROM4 RPT #6 executes MAR seven times and costs one setup cycle");
		program.write_word(0x05e2, 0xf84a); // BC 05f0,BGEQ
		program.write_word(0x05e3, 0x05f0);
		program.write_word(0x05e4, 0x75d6); // Rejected-branch marker.
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		program.write_word(0x05f0, 0x75d6); // Taken-branch marker.
		program.write_word(0x05f1, 0x0124);
		program.write_word(0x05f2, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		return resume_atomic_at(0x05e0, 500);
	}
	if (m_phase >= 500 && m_phase <= 503)
	{
		const bool taken = m_bleq_case ? m_bleq_case < 3 : m_phase == 500 || m_phase == 502;
		const u16 opcode = m_bleq_case ? 0xf84f : m_phase <= 501 ? 0xf84a : 0xf84b;
		expect_opcode(opcode,
				m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 5) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x05f3 : 0x05e7),
				"ROM4 BC tests B's 40-bit sign and costs five/three cycles");
		if (m_phase < 503)
		{
			if (m_phase == 501)
				program.write_word(0x05e2, 0xf84b); // BC 05f0,BLT
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_B,
					m_phase == 500 || m_phase == 501 ? 0xff00000000ULL : 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		if (m_bleq_case < 3)
		{
			static constexpr u64 values[] = {0, 0xff00000000ULL, 1};
			program.write_word(0x05e2, 0xf84f); // BC 05f0,BLEQ
			m_cpu->set_state_int(tms320c54x_device::STATE_B, values[m_bleq_case++]);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_port_writes = 0;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xfc47); // RC ALEQ
		program.write_word(0x05e3, 0x75d6); // Rejected-return marker.
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		program.write_word(0x05f0, 0x75d6); // Accepted-return marker.
		program.write_word(0x05f1, 0x0124);
		program.write_word(0x05f2, 0xf5e1);
		data.write_word(0x0300, 0x05f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(504);
	}
	if (m_phase == 504 || m_phase == 505)
	{
		const bool taken = m_phase == 505;
		expect_opcode(0xfc47,
				m_cpu->state_int(tms320c54x_device::STATE_SP) == (taken ? 0x0301 : 0x0300) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x05f3 : 0x05e6) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 5),
				"ROM4 RC ALEQ checks 40-bit guard, stack pop, and five/three cycles");
		if (!taken)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0xff00000000ULL}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(505);
		}
		program.write_word(0x05e2, 0xff47); // XC 2,ALEQ
		program.write_word(0x05e3, 0x6d91); // MAR *AR1+
		program.write_word(0x05e4, 0x6d92); // MAR *AR2+
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(506);
	}
	if (m_phase >= 506 && m_phase <= 509)
	{
		const bool accepted = m_phase == 506 || m_phase == 508;
		const u16 opcode = m_phase <= 507 ? 0xff47 : 0xff4c;
		expect_opcode(opcode,
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == (accepted ? 0x0121 : 0x0120) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == (accepted ? 0x0131 : 0x0130) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 XC ALEQ/BNEQ executes or rejects two guarded MAR slots");
		if (m_phase < 509)
		{
			if (m_phase == 507)
				program.write_word(0x05e2, 0xff4c); // XC 2,BNEQ
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A,
					m_phase == 506 ? 1 : 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B,
					m_phase == 507 ? 0xff00000000ULL : 0);
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xff4f); // XC 2,BLEQ.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_B, 0xff00000000ULL}, {cpu_regs::STATE_ST0, 0x1800},
			{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6565);
	}
	if ((m_phase >= 6565 && m_phase <= 6567) || m_phase == 6585 || m_phase == 6586)
	{
		static constexpr u64 values[] = { 0xff00000000ULL, 0, 0x0100000000ULL };
		const unsigned index = m_phase >= 6585 ? m_phase - 6585 + 3 : m_phase - 6565;
		const bool accepted = index < 2 || index == 3;
		expect_opcode(index < 3 ? 0xff4f : 0xff44,
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == (accepted ? 0x0121 : 0x0120) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (accepted ? 0x0131 : 0x0130) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (index < 3 ? 1 : index == 3 ? 0x0100000000ULL : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (index < 3 ? values[index] : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1800 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 XC BLEQ/ANEQ tests full 40-bit values, preserves A/B/status and handles both slots at equal cycle cost");
		if (index < 4)
		{
			program.write_word(0x05e2, index < 2 ? 0xff4f : 0xff44);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, index < 2 ? 1 : index == 2 ? 0x0100000000ULL : 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, index < 2 ? values[index + 1] : 0);
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR2, 0x0130}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(index == 2 ? 6585 : m_phase + 1);
		}
		program.write_word(0x05e2, 0x0882); // SUB *AR2,A
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f22, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f22);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 510);
	}
	if (m_phase == 510)
	{
		expect_opcode(0x0882,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x17fff &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f22 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 SUB *AR2,A sign-extends without pointer motion in one cycle");
		program.write_word(0x05e2, 0x0881); // SUB *AR1,A.
		data.write_word(0x0f21, 0x8001);
		data.write_word(0x0f22, 0x1234); // Distinguish AR1 from the prior AR2 case.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x10000}, {cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_AR1, 0x0f21},
			{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6501);
	}
	if (m_phase == 6501 || m_phase == 6502)
	{
		bool const signed_memory = m_phase == 6501;
		expect_opcode(0x0881,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (signed_memory ? 0x17fff : 0x7fff) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f21 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f22 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == (signed_memory ? 0 : 0x0800) &&
			data.read_word(0x0f21) == 0x8001 && data.read_word(0x0f22) == 0x1234 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 SUB *AR1,A respects SXM, no-borrow carry, source selection and one-cycle cost");
		if (signed_memory)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x10000}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(6502);
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		program.write_word(0x05e2, 0x0092); // ADD *AR2+,A.
		data.write_word(0x0f22, 0xffff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_AR2, 0x0f22},
			{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6525);
	}
	if (m_phase >= 6525 && m_phase <= 6528)
	{
		unsigned const index = m_phase - 6525;
		bool const destination_b = index >= 2;
		bool const signed_memory = !(index & 1);
		u64 const result = signed_memory ? 0 : 0x10000;
		expect_opcode(destination_b ? 0x0192 : 0x0092,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (destination_b ? 0x12345678 : result) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (destination_b ? result : 0x12345678) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f23 &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0e00) == (signed_memory ? 0x0800 : 0) &&
			data.read_word(0x0f22) == 0xffff &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 ADD through AR2 postincrements once, respects SXM and sets 32-bit carry for A/B");
		if (index < 3)
		{
			unsigned const next = index + 1;
			program.write_word(0x05e2, next >= 2 ? 0x0192 : 0x0092);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, next >= 2 ? 0x12345678 : 1}, {cpu_regs::STATE_B, next >= 2 ? 1 : 0x12345678},
				{cpu_regs::STATE_AR2, 0x0f22}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, next & 1 ? 0 : 0x0100},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7fff);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		program.write_word(0x05e2, 0x1b84); // OR *AR4,B
		data.write_word(0x0f24, 0x00f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0xff0000000fULL}, {cpu_regs::STATE_AR4, 0x0f24}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(511);
	}
	if (m_phase == 511)
	{
		expect_opcode(0x1b84,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000ffULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f24 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 OR *AR4,B preserves guard, status, and pointer in one cycle");
		write_words(program, {
			{0x05e2, 0x1bf8}, // OR *(lk),B
			{0x05e3, 0x0f24}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff0000000fULL);
		return resume_atomic_at(0x05e0, 512);
	}
	if (m_phase == 512)
	{
		expect_opcode(0x1bf8,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff000000ffULL &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 absolute OR into B consumes address and costs two cycles");
		program.write_word(0x05e2, 0x45f8); // LD *(lk),16,B
		data.write_word(0x0f24, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		return resume_atomic_at(0x05e0, 513);
	}
	if (m_phase == 513)
	{
		expect_opcode(0x45f8,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80010000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 absolute LD *,16,B sign-extends before shift in two cycles");
		program.write_word(0x05e0, 0x7586); // PORTW *AR6,port: marker must not mutate AR6.
		program.write_word(0x05e2, 0x4816); // LDM AR6,A
		program.write_word(0x05e3, 0x4913); // LDM AR3,B
		program.write_word(0x05e4, 0x7586);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR6, 0x8006},
			{cpu_regs::STATE_AR3, 0x8003},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 514);
	}
	if (m_phase == 514)
	{
		expect_opcode(0x4816,
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8006 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x8006,
				"ROM4 LDM AR6,A zero-extends without changing AR6");
		expect_opcode(0x4913,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8003 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x8003 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LDM AR3,B zero-extends despite SXM; both loads cost one cycle");
		write_words(program, {
			{0x05e0, 0x75d6},
			{0x05e2, 0x7215}, // MVDM dmad,AR5
			{0x05e3, 0x0f60},
			{0x05e4, 0x7314}, // MVMD AR4,dmad
			{0x05e5, 0x0f62},
			{0x05e6, 0x7315}, // MVMD AR5,dmad
			{0x05e7, 0x0f63}, {0x05e8, 0x75d6}, {0x05e9, 0x0124}, {0x05ea, 0xf5e1},
		});
		data.write_word(0x0f60, 0x4321);
		data.write_word(0x0f62, 0);
		data.write_word(0x0f63, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0xabcd}, {cpu_regs::STATE_AR5, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(515);
	}
	if (m_phase == 515)
	{
		expect_opcode(0x7215,
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x4321 &&
				data.read_word(0x0f60) == 0x4321,
				"ROM4 MVDM copies data into AR5 without altering the source");
		expect_opcode(0x7314,
				data.read_word(0x0f62) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xabcd,
				"ROM4 MVMD copies AR4 into data memory");
		expect_opcode(0x7315,
				data.read_word(0x0f63) == 0x4321 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"ROM4 MVMD copies the new AR5; three MMR moves cost two cycles each");
		program.write_word(0x05e0, 0x7586); // Preserve AR6 across both timing markers.
		program.write_word(0x05e2, 0x8816); // STLM A,AR6
		program.write_word(0x05e3, 0x8817); // STLM A,AR7
		program.write_word(0x05e4, 0x8821); // STLM A,MMR 0x21
		program.write_word(0x05e5, 0x8910); // STLM B,AR0
		program.write_word(0x05e6, 0x8911); // STLM B,AR1
		program.write_word(0x05e7, 0x4920); // LDM MMR 0x20,B
		program.write_word(0x05e8, 0x7586);
		program.write_word(0x05e9, 0x0124);
		program.write_word(0x05ea, 0xf5e1);
		data.write_word(0x0020, 0x9abc);
		data.write_word(0x0021, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x5678}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_AR7, 0}, {cpu_regs::STATE_AR0, 0}, {cpu_regs::STATE_AR1, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 516);
	}
	if (m_phase == 516)
	{
		expect_opcode(0x8816,
				m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1234,
				"ROM4 STLM A,AR6 publishes A's low word");
		expect_opcode(0x8817,
				m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1234,
				"ROM4 STLM A,AR7 publishes A's low word");
		expect_opcode(0x8821, data.read_word(0x0021) == 0x1234,
				"ROM4 STLM A,MMR 0x21 writes the peripheral-facing data space");
		expect_opcode(0x8910,
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x5678,
				"ROM4 STLM B,AR0 publishes B's low word");
		expect_opcode(0x8911,
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x5678,
				"ROM4 STLM B,AR1 publishes B's low word");
		expect_opcode(0x4920,
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x9abc &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
				"ROM4 mixed MMR transfers preserve CPU-register costs and include STLM21/LDM20 peripheral waits");
		program.write_word(0x05e0, 0x75d6);
		program.write_word(0x05e2, 0x71da); // MVDK *AR2+0%,dmad
		program.write_word(0x05e3, 0x0f40);
		program.write_word(0x05e4, 0x7692); // ST #lk,*AR2+
		program.write_word(0x05e5, 0xabcd);
		program.write_word(0x05e6, 0x7c8b); // MVPD pmad,*AR3-
		program.write_word(0x05e7, 0x0610);
		program.write_word(0x05e8, 0x7d82); // MVDP *AR2,pmad
		program.write_word(0x05e9, 0x0620);
		program.write_word(0x05ea, 0x8182); // STL B,*AR2
		write_words(program, {
			{0x05eb, 0x75d6}, {0x05ec, 0x0124}, {0x05ed, 0xf5e1}, {0x0610, 0xbeef}, {0x0620, 0},
		});
		write_words(data, {
			{0x0f21, 0}, {0x0f22, 0x1234}, {0x0f23, 0x5a3c}, {0x0f40, 0}, {0x0f50, 0},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 2}, {cpu_regs::STATE_AR2, 0x0f23}, {cpu_regs::STATE_AR3, 0x0f50},
			{cpu_regs::STATE_B, 0xcafe}, {cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(517);
	}
	if (m_phase == 517)
	{
		expect_opcode(0x71da,
				data.read_word(0x0f40) == 0x5a3c &&
				data.read_word(0x0f23) == 0x5a3c,
				"ROM4 MVDK copies old AR2 word before AR0-step circular wrap");
		expect_opcode(0x7692,
				data.read_word(0x0f21) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f22,
				"ROM4 ST #lk,*AR2+ writes at the wrapped address before advancing");
		expect_opcode(0x7c8b,
				data.read_word(0x0f50) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f4f,
				"ROM4 MVPD copies program memory to old AR3 address then decrements");
		expect_opcode(0x7d82,
				program.read_word(0x0620) == 0x1234,
				"ROM4 MVDP copies old AR2 data word into program memory");
		expect_opcode(0x8182,
				data.read_word(0x0f22) == 0xcafe &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f22 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 14,
				"ROM4 STL B,*AR2 overwrites data only; transfers cost 2+2+3+4+1 cycles");
		program.write_word(0x05e0, 0x7586); // Preserve source AR6 across markers.
		program.write_word(0x05e2, 0xe762); // MVMM AR6,AR2
		program.write_word(0x05e3, 0x7586);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR6, 0xabcd}, {cpu_regs::STATE_AR2, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(518);
	}
	if (m_phase == 518)
	{
		expect_opcode(0xe762,
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0xabcd &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 MVMM AR6,AR2 preserves the source and costs one cycle");
		program.write_word(0x05e0, 0x75d6);
		program.write_word(0x05e2, 0xe80d); // LD #13,A
		program.write_word(0x05e3, 0x75d6);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_ST0, 0x0800},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 519);
	}
	if (m_phase >= 519 && m_phase <= 525)
	{
		static constexpr u16 immediate_words[] = {
			0xe80d, 0xe820, 0xe905, 0xe906, 0xe90c, 0xe90f, 0xe97c
		};
		const unsigned index = m_phase - 519;
		const u16 opcode = immediate_words[index];
		const bool destination_b = (opcode & 0xff00) == 0xe900;
		expect_opcode(opcode,
				m_cpu->state_int(destination_b ? tms320c54x_device::STATE_B :
					tms320c54x_device::STATE_A) == (opcode & 0xff) &&
				m_cpu->state_int(destination_b ? tms320c54x_device::STATE_A :
					tms320c54x_device::STATE_B) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 LD #K writes only its destination without flags in one cycle");
		if (m_phase < 525)
		{
			program.write_word(0x05e2, immediate_words[index + 1]);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR6, 0x0a03},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xec00); // RPT #0.
		program.write_word(0x05e3, 0x6d91); // MAR *AR1+
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6600);
	}
	if (m_phase >= 6600 && m_phase <= 6855)
	{
		const unsigned index = m_phase - 6600;
		const u16 opcode = 0xec00 | index;
		const unsigned count = (opcode & 0xff) + 1;
		expect_opcode(opcode,
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0120 + count &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == count + 3,
				"ROM4 RPT #K executes MAR K+1 times after one setup cycle");
		if (index < 255)
		{
			program.write_word(0x05e2, opcode + 1);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x immediate repeat conformance: PASS variants=256\n");
		program.write_word(0x05e2, 0xf6bd); // RSBX ST1 bit 13
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x2000);
		m_xf_publications = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		return resume_atomic_at(0x05e0, 533);
	}
	if ((m_phase >= 533 && m_phase <= 536) || (m_phase >= 6547 && m_phase <= 6550))
	{
		struct status_case { u16 opcode, st0_before, st1_before, st0_after, st1_after; };
		static constexpr status_case cases[] = {
			{ 0xf6bd, 0x0800, 0x2000, 0x0800, 0 },
			{ 0xf6bf, 0x0800, 0x8000, 0x0800, 0 },
			{ 0xf7bd, 0x0800, 0, 0x0800, 0x2000 },
			{ 0xf7be, 0x0800, 0, 0x0800, 0x4000 },
			{ 0xf5bc, 0x0800, 0x0100, 0x1800, 0x0100 },
			{ 0xf4bc, 0x1800, 0x0100, 0x0800, 0x0100 },
			{ 0xf6b8, 0x1800, 0x0300, 0x1800, 0x0200 },
			{ 0xf7b8, 0x1800, 0x0200, 0x1800, 0x0300 }
		};
		const unsigned index = m_phase >= 6547 ? m_phase - 6547 + 4 : m_phase - 533;
		expect(m_xf_pin == bool(BIT(cases[index].st1_after, 13)) &&
				m_xf_publications == unsigned(BIT(cases[index].st1_before ^ cases[index].st1_after, 13)),
				"XF pin follows status mutations exactly once and ignores unrelated bits");
		expect_opcode(cases[index].opcode,
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == cases[index].st1_after &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == cases[index].st0_after &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 RSBX/SSBX changes only the selected ST0/ST1 bit in one cycle");
		if (index < 7)
		{
			program.write_word(0x05e2, cases[index + 1].opcode);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, cases[index + 1].st0_before);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, cases[index + 1].st1_before);
			m_xf_publications = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(index < 3 ? m_phase + 1 : index == 3 ? 6547 : m_phase + 1);
		}
		expect(capture_snapshot(machine(), m_xf_saved) == STATERR_NONE,
			"save architectural XF low");
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, cases[index].st1_after | 0x2000);
		expect(m_xf_pin, "debugger ST1 import drives XF high immediately");
		expect(restore_snapshot(machine(), m_xf_saved) == STATERR_NONE && !m_xf_pin,
			"post-load re-drives restored XF low instead of retaining external high");
		osd_printf_info("TMS320C54x XF output: PASS status_variants=8 debugger=1 restore=1\n");
		program.write_word(0x05e2, 0xfd30); // XC 1,TC
		program.write_word(0x05e3, 0x6d91); // MAR *AR1+
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1000);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(537);
	}
	if ((m_phase >= 537 && m_phase <= 542) || m_phase == 6543 || m_phase == 6544)
	{
		static constexpr u16 xc_words[] = {
			0xfd30, 0xfd30, 0xfd4b, 0xfd4b, 0xfd4d, 0xfd4d, 0xfd20, 0xfd20
		};
		static constexpr u16 st0_values[] = { 0x1000, 0, 0, 0, 0, 0, 0x0800, 0x1800 };
		static constexpr u64 b_values[] = {
			0, 0, 0xff00000000ULL, 0, 0, 1, 0x12345678, 0x12345678
		};
		const unsigned index = m_phase >= 6543 ? m_phase - 6543 + 6 : m_phase - 537;
		const bool accepted = (index & 1) == 0;
		expect_opcode(xc_words[index],
				m_cpu->state_int(tms320c54x_device::STATE_AR1) ==
					(accepted ? 0x0121 : 0x0120) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0_values[index] &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == b_values[index] &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 XC 1 tests TC/NTC/BLT/BEQ, preserves status/B and executes or rejects one MAR slot");
		if (index < 7)
		{
			program.write_word(0x05e2, xc_words[index + 1]);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, st0_values[index + 1]);
			m_cpu->set_state_int(tms320c54x_device::STATE_B, b_values[index + 1]);
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR1, 0x0120}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(index < 5 ? m_phase + 1 : index == 5 ? 6543 : 6544);
		}
	}
	return false;
}

} // namespace c54x_test
