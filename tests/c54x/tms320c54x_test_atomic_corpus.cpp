// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_square.h"
#include "tms320c54x_test_shift_examples.h"
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
		program.write_word(0x05e0, 0xb03a); // MAC *AR5, *AR4+, A, A
		program.write_word(0x05e1, 0xf5e1);
		data.write_word(0x0b00, 0xfffe);
		data.write_word(0x0c00, 3);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 10}, {cpu_regs::STATE_B, 20}, {cpu_regs::STATE_AR5, 0x0b00},
			{cpu_regs::STATE_AR4, 0x0c00}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(79);
	}
	if (m_phase == 79)
	{
		expect_opcode(0xb03a, m_cpu->state_int(tms320c54x_device::STATE_A) == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 20 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c01 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b00,
				"ROM4 b03a signed MAC updates T and one pointer");
		program.write_word(0x05e0, 0xb3be); // MAC *AR5+, *AR4+%, B, B
		data.write_word(0x0b00, 0xfffd);
		data.write_word(0x0c03, 4);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR4, 0x0c03}, {cpu_regs::STATE_AR5, 0x0b00},
			{cpu_regs::STATE_B, 20}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(80);
	}
	if (m_phase == 80)
	{
		expect_opcode(0xb3be, m_cpu->state_int(tms320c54x_device::STATE_B) == 8 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b01,
				"ROM4 b3be signed MAC updates both pointer modes");
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
		program.write_word(0x05e0, 0xb0be); // MAC *AR5+, *AR4+%, A, A
		program.write_word(0x05e1, 0xf5e1);
		data.write_word(0x0b00, 0xfffd);
		data.write_word(0x0c03, 4);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR4, 0x0c03}, {cpu_regs::STATE_AR5, 0x0b00},
			{cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_A, 20}, {cpu_regs::STATE_B, 9}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(85);
	}
	if (m_phase == 85)
	{
		expect_opcode(0xb0be, m_cpu->state_int(tms320c54x_device::STATE_A) == 8 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 9 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b01,
				"ROM4 b0be signed MAC preserves B and updates both pointers");
		program.write_word(0x05e0, 0xe2e4); // SQDST *AR4+%, *AR2-
		program.write_word(0x05e1, 0xf5e1);
		data.write_word(0x0c03, 5);
		data.write_word(0x0d00, 7);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0000030000ULL}, {cpu_regs::STATE_B, 10}, {cpu_regs::STATE_AR0, 1},
			{cpu_regs::STATE_AR4, 0x0c03}, {cpu_regs::STATE_AR2, 0x0d00}, {cpu_regs::STATE_BK, 4},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 86);
	}
	if (m_phase == 86)
	{
		expect_opcode(0xe2e4, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffffe0000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 19 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0cff,
				"ROM4 e2e4 SQDST squares old A and loads signed vector difference");
		program.write_word(0x05e0, 0xe245); // SQDST *AR2-,*AR3-.
		data.write_word(0x0c10, 0xfffb);
		data.write_word(0x0d10, 7);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x30000}, {cpu_regs::STATE_B, 10}, {cpu_regs::STATE_T, 0x2222},
			{cpu_regs::STATE_AR2, 0x0c10}, {cpu_regs::STATE_AR3, 0x0d10}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6506);
	}
	if (m_phase == 6506 || m_phase == 6507)
	{
		bool const fractional = m_phase == 6507;
		expect_opcode(fractional ? 0xe249 : 0xe245,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xfffff40000ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (fractional ? 28 : 19) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x2222 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0c0f &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == (fractional ? 0x0d11 : 0x0d0f) &&
			data.read_word(0x0c10) == 0xfffb && data.read_word(0x0d10) == 7,
			"ROM4 SQDST uses signed X-Y, squares old A with FRCT scaling and independently modifies X/Y");
		if (!fractional)
		{
			program.write_word(0x05e0, 0xe249); // SQDST *AR2-,*AR3+.
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x30000}, {cpu_regs::STATE_B, 10}, {cpu_regs::STATE_AR2, 0x0c10},
				{cpu_regs::STATE_AR3, 0x0d10}, {cpu_regs::STATE_ST1, 0x0140}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(6507);
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		program.write_word(0x05e0, 0x74d6); // PORTR port, *AR6+%
		program.write_word(0x05e1, 0x0123);
		program.write_word(0x05e2, 0x4f81); // DST B, *AR1
		program.write_word(0x05e3, 0x74d6);
		program.write_word(0x05e4, 0x0123);
		program.write_word(0x05e5, 0xf5e1); // IDLE
		m_port_reads = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR1, 0x0d00}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_B, 0xff12345678ULL}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(87);
	}
	if (m_phase == 87)
	{
		expect_opcode(0x4f81, data.read_word(0x0d00) == 0x1234 &&
				data.read_word(0x0d01) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0d00 &&
				m_port_reads == 2 &&
				m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 4f81 DST writes both halves and costs two cycles");
		program.write_word(0x05e0, 0x4f93); // DST B, *AR3+
		program.write_word(0x05e1, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_AR3, 0x0e00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(88);
	}
	if (m_phase == 88)
	{
		expect(data.read_word(0x0e00) == 0x1234 &&
				data.read_word(0x0e01) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e02,
				"DST long operand post-increments AR by two");
		program.write_word(0x05e0, 0x4fd3); // DST B, *AR3+%
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xabcdef01ULL}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_B, 0x76543210}, {cpu_regs::STATE_AR3, 0x0e02}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(90);
	}
	if (m_phase == 90)
	{
		expect_opcode(0x4fd3, data.read_word(0x0e02) == 0x7654 &&
				data.read_word(0x0e03) == 0x3210 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e00,
				"DST long circular operand advances by two and wraps");
		program.write_word(0x05e0, 0x4fd2); // DST B,*AR2+%.
		write_words(data, {
			{0x0e11, 0xbeef}, {0x0e12, 0}, {0x0e13, 0}, {0x0e14, 0xcafe},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0e12);
		return resume_atomic_at(0x05e0, 6519);
	}
	if (m_phase == 6519)
	{
		expect_opcode(0x4fd2,
			data.read_word(0x0e12) == 0x7654 && data.read_word(0x0e13) == 0x3210 &&
			data.read_word(0x0e11) == 0xbeef && data.read_word(0x0e14) == 0xcafe &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0e10 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e00 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xabcdef01ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x76543210,
			"ROM4 DST through AR2 stores high/low words then wraps by two without changing accumulators or adjacent cells");
		write_words(program, {
			{0x05e0, 0x74d6}, {0x05e1, 0x0123},
			{0x05e2, 0x4f90}, // DST B,*AR0+.
			{0x05e3, 0x74d6}, {0x05e4, 0x0123}, {0x05e5, 0xf5e1},
		});
		write_words(data, {
			{0x0e21, 0xbeef}, {0x0e22, 0}, {0x0e23, 0}, {0x0e24, 0xcafe},
		});
		m_port_reads = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0x0e22}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_ST0, 0x0aa5},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6562);
	}
	if (m_phase == 6562 || (m_phase >= 9208 && m_phase <= 9210))
	{
		const bool into_memory_from_a = m_phase == 9208 || m_phase == 9209;
		const bool decrement = m_phase == 9210;
		const u16 opcode = m_phase == 6562 ? 0x4f90 : m_phase == 9208 ? 0x4e82 : m_phase == 9209 ? 0x4e95 : 0x4f8d;
		expect_opcode(opcode,
			data.read_word(decrement ? 0x0e23 : 0x0e22) == (into_memory_from_a ? 0xabcd : 0x7654) &&
			data.read_word(decrement ? 0x0e22 : 0x0e23) == (into_memory_from_a ? 0xef01 : 0x3210) &&
			data.read_word(0x0e21) == 0xbeef && data.read_word(0x0e24) == 0xcafe &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0e24 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (m_phase == 6562 ? 0x0e10 : 0x0e22) &&
			(m_phase == 6562 || m_cpu->state_int(tms320c54x_device::STATE_AR5) == (m_phase == 9208 ? 0x0e22 : decrement ? 0x0e21 : 0x0e24)) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e00 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xabcdef01ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x76543210 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0100 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e6 &&
			m_port_reads == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 DST selects A/B, orders both words and updates long pointers while preserving neighbors/accumulators/status in two cycles");
		if (m_phase != 9210)
		{
			const unsigned next = m_phase == 6562 ? 9208 : m_phase + 1;
			program.write_word(0x05e2, next == 9208 ? 0x4e82 : next == 9209 ? 0x4e95 : 0x4f8d);
			data.write_word(0x0e22, 0);
			data.write_word(0x0e23, 0);
			m_port_reads = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_AR2, 0x0e22}, {cpu_regs::STATE_AR5, next == 9210 ? 0x0e23 : 0x0e22},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(next);
		}
		program.write_word(0x05e0, 0x74d6); // PORTR port, *AR6+%
		program.write_word(0x05e1, 0x0123);
		program.write_word(0x05e2, 0x6ded); // MAR *+AR5(-7)
		program.write_word(0x05e3, 0xfff9);
		program.write_word(0x05e4, 0x74d6);
		program.write_word(0x05e5, 0x0123);
		program.write_word(0x05e6, 0xf5e1); // IDLE
		m_port_reads = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR5, 0x1000}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(91);
	}
	if (m_phase == 91)
	{
		expect_opcode(0x6ded, m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0ff9 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
				m_port_reads == 2 &&
				m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 6ded consumes signed MAR offset and costs two cycles");
		program.write_word(0x05e0, 0x6ddc); // MAR *AR4+0%
		program.write_word(0x05e1, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR4, 0x0c03}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(92);
	}
	if (m_phase == 92)
	{
		expect_opcode(0x6ddc, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e2,
				"ROM4 6ddc circular MAR uses AR0 and consumes no extension");
		program.write_word(0x05e0, 0x4092); // SUB *AR2+, 16, A
		program.write_word(0x05e1, 0xf5e1);
		data.write_word(0x0d00, 3);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0d00}, {cpu_regs::STATE_A, 0x00050000}, {cpu_regs::STATE_B, 0x1234},
			{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(93);
	}
	if (m_phase == 93)
	{
		expect_opcode(0x4092, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00020000 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d01 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e2,
				"ROM4 4092 subtracts the shifted Smem and advances AR2");
		program.write_word(0x05e0, 0x5781); // DLD *AR1, B
		program.write_word(0x05e1, 0xf5e1);
		data.write_word(0x0e00, 0x8001);
		data.write_word(0x0e01, 0x2345);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0e00);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 94);
	}
	if (m_phase == 94)
	{
		expect_opcode(0x5781, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80012345ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0e00,
				"ROM4 5781 DLD sign-extends B without modifying AR1");
		program.write_word(0x05e0, 0xa5be); // MPY *AR5+, *AR4+0%, B
		program.write_word(0x05e1, 0xf5e1);
		data.write_word(0x0b00, 0xfffe);
		data.write_word(0x0c03, 3);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR4, 0x0c03}, {cpu_regs::STATE_AR5, 0x0b00},
			{cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(95);
	}
	if (m_phase == 95)
	{
		expect_opcode(0xa5be, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfffffffffaULL &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b01,
				"ROM4 a5be signed dual MPY loads T and updates both pointers");
		program.write_word(0x05e0, 0xb736); // MACR *AR5, *AR4-, B, B
		data.write_word(0x0b00, 3);
		data.write_word(0x0c03, 4);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR5, 0x0b00}, {cpu_regs::STATE_AR4, 0x0c03}, {cpu_regs::STATE_B, 0x8000},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(96);
	}
	if (m_phase == 96)
	{
		expect_opcode(0xb736, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x10000 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c02 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b00,
				"ROM4 b736 MACR rounds and updates Y pointer");
		program.write_word(0x05e0, 0xb43e); // MACR *AR5,*AR4+0%,A,A.
		data.write_word(0x0b00, 0xfffe);
		data.write_word(0x0c03, 4);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x8008}, {cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_AR0, 1},
			{cpu_regs::STATE_AR5, 0x0b00}, {cpu_regs::STATE_AR4, 0x0c03}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6530);
	}
	if (m_phase == 6530)
	{
		expect_opcode(0xb43e,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0b00 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
			data.read_word(0x0b00) == 0xfffe && data.read_word(0x0c03) == 4,
			"ROM4 MACR uses signed X/Y, rounds old A plus product, publishes X in T and circularly wraps Y");
		program.write_word(0x05e0, 0xd6e1); // ST B,*AR3 || MACR *AR4+0%,A
		data.write_word(0x0c03, 0xfffe);
		data.write_word(0x0d00, 0);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x8008}, {cpu_regs::STATE_B, 0x0012345678ULL}, {cpu_regs::STATE_T, 4},
			{cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR4, 0x0c03}, {cpu_regs::STATE_AR3, 0x0d00},
			{cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(97);
	}
	if (m_phase == 97)
	{
		expect_opcode(0xd6e1, data.read_word(0x0d00) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0012345678ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d00,
				"ROM4 d6e1 stores old B and rounds MAC into A");
		program.write_word(0x05e0, 0xd6e9); // ST B,*AR3+ || MACR *AR4+0%,A.
		data.write_word(0x0c03, 0xfffe);
		data.write_word(0x0d00, 0);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x8008}, {cpu_regs::STATE_AR4, 0x0c03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6504);
	}
	if (m_phase == 6504)
	{
		expect_opcode(0xd6e9,
			data.read_word(0x0d00) == 0x1234 && data.read_word(0x0c03) == 0xfffe &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0012345678ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 4 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0c00 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d01,
			"ROM4 d6e9 stores old B, rounds signed MAC into A and updates both pointers independently");
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
		program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
		program.write_word(0x05e1, 0x0124);
		program.write_word(0x05e2, 0x4593); // LD *AR3+,16,B
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0d00, 0xff80);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x0d00}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_B, 0},
			{cpu_regs::STATE_ST1, 0x0100}, // SXM
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(100);
	}
	if (m_phase == 100)
	{
		expect_opcode(0x4593, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff800000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d01 &&
				m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 4593 sign-extends into B, post-increments AR3, and costs one cycle");
		program.write_word(0x05e2, 0x4592); // LD *AR2+,16,B.
		data.write_word(0x0d10, 0xff80);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x11223344}, {cpu_regs::STATE_AR2, 0x0d10}, {cpu_regs::STATE_ST0, 0x0aa5},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6513);
	}
	if (m_phase == 6513 || m_phase == 6514 || m_phase == 6568 || m_phase == 6569 || m_phase == 9206 || m_phase == 9207)
	{
		bool const into_a = m_phase >= 9206;
		bool const signed_memory = m_phase == 6513 || m_phase == 6568 || m_phase == 9206;
		bool const stationary = m_phase >= 6568;
		expect_opcode(into_a ? 0x4488 : stationary ? 0x4582 : 0x4592,
			m_cpu->state_int(into_a ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (signed_memory ? 0xffff800000ULL : 0xff800000ULL) &&
			m_cpu->state_int(into_a ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == 0x11223344 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (stationary ? 0x0d10 : 0x0d11) &&
			(!into_a || m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0d0f) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d01 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
			data.read_word(0x0d10) == 0xff80 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 shifted LD respects SXM, accumulator selection and pointer updates, preserving status/source in one cycle");
		if (m_phase != 9207)
		{
			const unsigned next = m_phase == 6513 ? 6514 : m_phase == 6514 ? 6568 : m_phase == 6568 ? 6569 : m_phase == 6569 ? 9206 : 9207;
			program.write_word(0x05e2, next >= 9206 ? 0x4488 : next >= 6568 ? 0x4582 : 0x4592);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d10);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x0d10);
			if (next >= 9206)
			{
				m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
				m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x11223344);
			}
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, next == 6568 || next == 9206 ? 0x0100 : 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(next);
		}
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
		return begin_atomic_case(register_bit_examples()[8], 103);
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
		return begin_atomic_case(register_bit_examples()[9], 113);
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
		program.write_word(0x05e0, 0xf7bb); // SSBX INTM
		program.write_word(0x05e1, 0x4a08); // PSHM AL
		program.write_word(0x05e2, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(117);
	}
	if (m_phase == 117)
	{
		expect_opcode(0xf7bb, m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800,
				"ROM4 f7bb masks interrupts");
		expect_opcode(0x4a08, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
				data.read_word(0x02ff) == 0x5678,
				"ROM4 4a08 pushes AL to TOS");
		program.write_word(0x05e0, 0x8a08); // POPM AL
		program.write_word(0x05e1, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12340000);
		return resume_atomic_at(0x05e0, 118);
	}
	if (m_phase == 118)
	{
		expect_opcode(0x8a08, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"ROM4 8a08 restores AL from TOS and advances SP");
		program.write_word(0x05e0, 0x4a60); // PSHM data address 60.
		program.write_word(0x05e1, 0x4a61); // PSHM data address 61.
		program.write_word(0x05e2, 0xf5e1);
		data.write_word(0x0060, 0x1234);
		data.write_word(0x0061, 0xabcd);
		return resume_atomic_at(0x05e0, 6517);
	}
	if (m_phase == 6517)
	{
		expect_opcode(0x4a60, data.read_word(0x02ff) == 0x1234 &&
			data.read_word(0x0060) == 0x1234,
			"ROM4 PSHM 60 reads its seven-bit data address and pushes without changing the source");
		expect_opcode(0x4a61, data.read_word(0x02fe) == 0xabcd &&
			data.read_word(0x0061) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02fe &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678,
			"ROM4 PSHM 61 pushes after 60 in descending stack order without changing A");
		data.write_word(0x0060, 0);
		data.write_word(0x0061, 0);
		program.write_word(0x05e0, 0x8a61); // POPM data address 61.
		program.write_word(0x05e1, 0x8a60); // POPM data address 60.
		return resume_atomic_at(0x05e0, 6518);
	}
	if (m_phase == 6518)
	{
		expect_opcode(0x8a61, data.read_word(0x0061) == 0xabcd,
			"ROM4 POPM 61 restores the last-pushed value to its data address");
		expect_opcode(0x8a60, data.read_word(0x0060) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678,
			"ROM4 POPM 60 restores the first-pushed value and balances the stack without changing A");
		program.write_word(0x05e0, 0xf6bb); // RSBX INTM
		program.write_word(0x05e1, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
		return resume_atomic_at(0x05e0, 119);
	}
	if (m_phase == 119)
	{
		expect_opcode(0xf6bb, !(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
				"ROM4 f6bb clears INTM for interrupt return");
		write_words(program, {
			{0x05e0, 0x75d6}, // PORTW *AR6+%, port
			{0x05e1, 0x0124},
			{0x05e2, 0x7212}, // MVDM 0e00,AR2
			{0x05e3, 0x0e00}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0e00, 0x1234);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(120);
	}
	if (m_phase == 120)
	{
		expect_opcode(0x7212, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 7212 MVDM moves data to AR2 in two cycles");
		program.write_word(0x05e2, 0x7312); // MVMD AR2,0e01
		program.write_word(0x05e3, 0x0e01);
		data.write_word(0x0e01, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0a03);
		return resume_atomic_at(0x05e0, 121);
	}
	if (m_phase == 121)
	{
		expect_opcode(0x7312, data.read_word(0x0e01) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"MVMD moves AR2 to data memory in two cycles");
		write_words(program, {
			{0x05e2, 0xec02}, // RPT #2
			{0x05e3, 0x7212}, // MVDM 0e00,AR2
			{0x05e4, 0x0e00}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0e00, 0x1111);
		data.write_word(0x0e01, 0x2222);
		data.write_word(0x0e02, 0x3333);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(122);
	}
	if (m_phase == 122)
	{
		expect_opcode(0x7212, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x3333 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"repeated MVDM advances data source and pipelines after first move");
		program.write_word(0x05e3, 0x7312); // MVMD AR2,0e10
		program.write_word(0x05e4, 0x0e10);
		data.write_word(0x0e10, 0);
		data.write_word(0x0e11, 0);
		data.write_word(0x0e12, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x4455}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(123);
	}
	if (m_phase == 123)
	{
		expect_opcode(0x7312, data.read_word(0x0e10) == 0x4455 &&
				data.read_word(0x0e11) == 0x4455 &&
				data.read_word(0x0e12) == 0x4455 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"repeated MVMD advances data destination and pipelines after first move");
		program.write_word(0x05e0, 0x4a09); // PSHM AH
		program.write_word(0x05e1, 0x4a0a); // PSHM AG
		program.write_word(0x05e2, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(124);
	}
	if (m_phase == 124)
	{
		expect_opcode(0x4a09, data.read_word(0x02ff) == 0x3456,
				"ROM4 4a09 pushes AH before decrementing SP again");
		expect_opcode(0x4a0a, data.read_word(0x02fe) == 0x0012 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02fe,
				"ROM4 4a0a pushes the eight-bit guard and decrements SP");
		program.write_word(0x05e0, 0x8a0a); // POPM AG
		program.write_word(0x05e1, 0x8a09); // POPM AH
		program.write_word(0x05e2, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xabcd987654ULL);
		return resume_atomic_at(0x05e0, 125);
	}
	if (m_phase == 125)
	{
		expect_opcode(0x8a0a, (m_cpu->state_int(tms320c54x_device::STATE_A) >> 32) == 0x12,
				"ROM4 8a0a restores AG without sign extension");
		expect_opcode(0x8a09, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234567654ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"ROM4 8a09 restores AH, preserves AL, and advances SP");
		program.write_word(0x05e0, 0x4a0b); // PSHM BL
		program.write_word(0x05e1, 0x4a0c); // PSHM BH
		program.write_word(0x05e2, 0x4a0d); // PSHM BG
		program.write_word(0x05e3, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x7f1234abcdULL);
		return resume_atomic_at(0x05e0, 126);
	}
	if (m_phase == 126)
	{
		expect_opcode(0x4a0b, data.read_word(0x02ff) == 0xabcd,
				"ROM4 4a0b pushes BL");
		expect_opcode(0x4a0c, data.read_word(0x02fe) == 0x1234,
				"ROM4 4a0c pushes BH");
		expect_opcode(0x4a0d, data.read_word(0x02fd) == 0x007f &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02fd,
				"ROM4 4a0d pushes the eight-bit B guard");
		program.write_word(0x05e0, 0x8a0d); // POPM BG
		program.write_word(0x05e1, 0x8a0c); // POPM BH
		program.write_word(0x05e2, 0x8a0b); // POPM BL
		program.write_word(0x05e3, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5511223344ULL);
		return resume_atomic_at(0x05e0, 127);
	}
	if (m_phase == 127)
	{
		expect_opcode(0x8a0d, (m_cpu->state_int(tms320c54x_device::STATE_B) >> 32) == 0x7f,
				"ROM4 8a0d restores BG without sign extension");
		expect_opcode(0x8a0c, (m_cpu->state_int(tms320c54x_device::STATE_B) >> 16 & 0xffff) == 0x1234,
				"ROM4 8a0c restores BH");
		expect_opcode(0x8a0b, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x7f1234abcdULL &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234567654ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"ROM4 8a0b restores BL, preserves A, and advances SP");
		return begin_atomic_case(register_bit_examples()[10], 128);
	}
	if (m_phase == 128)
	{
		verify_atomic_case(register_bit_examples()[10]);
		program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
		program.write_word(0x05e1, 0x0124);
		program.write_word(0x05e2, 0xf071); // RPTZ A,#lk
		program.write_word(0x05e3, 0x0000);
		program.write_word(0x05e4, 0xf495); // NOP
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(129);
	}
	if (m_phase == 129)
	{
		expect_opcode(0xf071, m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 f071 clears A, repeats one NOP, and costs two cycles");
		program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
		program.write_word(0x05e1, 0x0124);
		program.write_word(0x05e2, 0xf272); // RPTBD 05e6h
		program.write_word(0x05e3, 0x05e6);
		program.write_word(0x05e4, 0xf495); // delay slot 1
		program.write_word(0x05e5, 0xf495); // delay slot 2
		program.write_word(0x05e6, 0xf495); // one-word repeat body
		program.write_word(0x05e7, 0x75d6);
		program.write_word(0x05e8, 0x0124);
		program.write_word(0x05e9, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_BRC, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(130);
	}
	if (m_phase == 130)
	{
		expect_opcode(0xf272, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == 7 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ea &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000),
				"ROM4 f272 costs two cycles and retires after its one-word block");
		program.write_word(0x05e0, 0x75d6); // PORTW *AR6+%, port
		program.write_word(0x05e1, 0x0124);
		program.write_word(0x05e2, 0xf072); // RPTB 05e4h
		program.write_word(0x05e3, 0x05e4);
		program.write_word(0x05e4, 0xf495); // one-word repeat body
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_BRC, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(131);
	}
	if (m_phase == 131)
	{
		expect_opcode(0xf072, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == 7 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000),
				"ROM4 f072 costs four cycles and retires after its one-word block");
		write_words(program, {
			{0x05e0, 0x75d6}, // PORTW *AR6+%, port
			{0x05e1, 0x0124},
			{0x05e2, 0x34f8}, // BITT *(absolute)
			{0x05e3, 0x0d00}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0d00, 0x8000);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 0}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(132);
	}
	if (m_phase == 132 || m_phase == 133)
	{
		const bool high_bit = m_phase == 132;
		expect_opcode(0x34f8, bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) == high_bit &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 34f8 tests bit 15 minus T and costs two cycles when absolute");
		if (high_bit)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 15);
			return resume_atomic_at(0x05e0, 133);
		}
		program.write_word(0x05e2, 0x70f8); // MVKD dmad, *(absolute)
		write_words(program, {
			{0x05e3, 0x0d00}, {0x05e4, 0x0e00}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0d00, 0);
		data.write_word(0x0e00, 0x4567);
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 134);
	}
	if (m_phase == 134)
	{
		expect_opcode(0x70f8, data.read_word(0x0d00) == 0x4567 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 70f8 copies absolute data and costs three cycles");
		program.write_word(0x05e2, 0x61f8); // BITF *(absolute),#lk
		program.write_word(0x05e3, 0x0d00);
		program.write_word(0x05e4, 0x0040);
		data.write_word(0x0d00, 0x0140);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		return resume_atomic_at(0x05e0, 135);
	}
	if (m_phase == 135)
	{
		expect_opcode(0x61f8, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 61f8 tests the masked absolute word in three cycles");
		write_words(program, {
			{0x05e2, 0x6182}, // BITF *AR2,#lk
			{0x05e3, 0x0040}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0d00, 0x0100);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d00);
		return resume_atomic_at(0x05e0, 136);
	}
	if (m_phase == 136)
	{
		expect_opcode(0x6182, !(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 6182 tests indirect Smem in two cycles without pointer movement");
		program.write_word(0x05e2, 0x6183); // BITF *AR3,#0040.
		data.write_word(0x0d10, 0x0140);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_B, 0x87654321}, {cpu_regs::STATE_AR3, 0x0d10},
			{cpu_regs::STATE_ST0, 0x0aa5}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6511);
	}
	if (m_phase == 6511 || m_phase == 6512)
	{
		bool const matched = m_phase == 6511;
		expect_opcode(0x6183,
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (matched ? 0x1aa5 : 0x0aa5) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x87654321 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d10 &&
			data.read_word(0x0d10) == (matched ? 0x0140 : 0x0100) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 BITF through AR3 sets and clears only TC, preserves operands and consumes its mask in two cycles");
		if (matched)
		{
			data.write_word(0x0d10, 0x0100);
			m_port_writes = 0;
			return resume_atomic_at(0x05e0, 6512);
		}
		program.write_word(0x05e2, 0x68f8); // ANDM #lk,*(absolute)
		write_words(program, {
			{0x05e3, 0x0d00}, {0x05e4, 0x00f0}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0d00, 0x0ff0);
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 137);
	}
	if (m_phase >= 137 && m_phase <= 139)
	{
		const u16 opcode = m_phase == 137 ? 0x68f8 : m_phase == 138 ? 0x69f8 : 0x6bf8;
		const u16 expected = m_phase == 137 ? 0x00f0 : m_phase == 138 ? 0x0ff0 : 0x00f3;
		expect_opcode(opcode, data.read_word(0x0d00) == expected &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 absolute immediate memory operator costs three cycles");
		if (m_phase != 139)
		{
			program.write_word(0x05e2, m_phase == 137 ? 0x69f8 : 0x6bf8);
			program.write_word(0x05e4, m_phase == 137 ? 0x0f00 : 0x0003);
			data.write_word(0x0d00, 0x00f0);
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05e2, 0xff0c); // XC 2,C
		program.write_word(0x05e3, 0xf495); // NOP
		program.write_word(0x05e4, 0xf793); // CMPL B,B
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(140);
	}
	if (m_phase == 140 || m_phase == 141)
	{
		const bool carry_set = m_phase == 141;
		expect_opcode(0xff0c, m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(carry_set ? 0xffffffedcbULL : 0x1234ULL) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle ==
				5,
				"ROM4 ff0c executes or replaces two words with NOPs according to carry");
		if (!carry_set)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(141);
		}
		expect_opcode(0xf793, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffedcbULL,
				"ROM4 f793 complements the full 40-bit B accumulator");
		program.write_word(0x05e2, 0x47f8); // RPT *(absolute)
		program.write_word(0x05e3, 0x0d00);
		program.write_word(0x05e4, 0xf495); // NOP, repeated three times
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		data.write_word(0x0d00, 2);
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 142);
	}
	if (m_phase == 142)
	{
		expect_opcode(0x47f8, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == 9,
				"ROM4 absolute RPT Smem costs four cycles and repeats the NOP three times");
		write_words(program, {
			{0x05e2, 0x4782}, // RPT *AR2
			{0x05e3, 0xf495}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d00);
		return resume_atomic_at(0x05e0, 143);
	}
	if (m_phase == 143)
	{
		expect_opcode(0x4782, m_port_writes == 2 &&
				m_last_port_cycle - m_first_port_cycle == 8 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d00,
				"indirect RPT Smem costs three cycles without moving AR2");
		write_words(program, {
			{0x05e2, 0x7d92}, // MVDP *AR2+,pmad
			{0x05e3, 0x0b00}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0d00, 0x2468);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d00);
		return resume_atomic_at(0x05e0, 144);
	}
	if (m_phase == 144)
	{
		expect_opcode(0x7d92, program.read_word(0x0b00) == 0x2468 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d01 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 MVDP moves *AR2+ to program memory in four cycles");
		write_words(program, {
			{0x05e2, 0x7c92}, // MVPD pmad,*AR2+
			{0x05e3, 0x0b00}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d10);
		return resume_atomic_at(0x05e0, 145);
	}
	if (m_phase == 145)
	{
		expect_opcode(0x7c92, data.read_word(0x0d10) == 0x2468 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d11 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 MVPD moves program memory to *AR2+ in three cycles");
		program.write_word(0x05e2, 0x7df8); // MVDP *(absolute),pmad
		write_words(program, {
			{0x05e3, 0x0d00}, {0x05e4, 0x0b01}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 146);
	}
	if (m_phase == 146)
	{
		expect_opcode(0x7df8, program.read_word(0x0b01) == 0x2468 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"absolute MVDP reads address before pmad and costs five cycles");
		program.write_word(0x05e2, 0x7cf8); // MVPD pmad,*(absolute)
		write_words(program, {
			{0x05e3, 0x0d11}, {0x05e4, 0x0b01}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 147);
	}
	if (m_phase == 147)
	{
		expect_opcode(0x7cf8, data.read_word(0x0d11) == 0x2468 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"absolute MVPD reads address before pmad and costs four cycles");
		program.write_word(0x05e2, 0xec02); // RPT #2
		program.write_word(0x05e3, 0x7c92); // MVPD pmad,*AR2+
		write_words(program, {
			{0x05e4, 0x0b10}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1}, {0x0b10, 0x1357}, {0x0b11, 0x2468},
			{0x0b12, 0x369a},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0d20);
		return resume_atomic_at(0x05e0, 148);
	}
	if (m_phase == 148)
	{
		expect_opcode(0x7c92, data.read_word(0x0d20) == 0x1357 &&
				data.read_word(0x0d21) == 0x2468 &&
				data.read_word(0x0d22) == 0x369a &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d23 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"repeated MVPD advances program and data addresses at one cycle after setup");
		program.write_word(0x05e2, 0x7f92); // WRITA *AR2+
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0d30, 0xabcd);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0b20}, {cpu_regs::STATE_AR2, 0x0d30}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(149);
	}
	if (m_phase == 149)
	{
		expect_opcode(0x7f92, program.read_word(0x0b20) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0b20 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d31 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 WRITA writes A-addressed program memory in five cycles");
		write_words(program, {
			{0x05e2, 0x7ef8}, // READA *(absolute)
			{0x05e3, 0x0d31}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 150);
	}
	if (m_phase == 150)
	{
		expect_opcode(0x7ef8, data.read_word(0x0d31) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0b20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"absolute READA reads A-addressed program memory in six cycles");
		program.write_word(0x05e2, 0xec02); // RPT #2
		program.write_word(0x05e3, 0x7f92); // WRITA *AR2+
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		data.write_word(0x0d40, 0x1357);
		data.write_word(0x0d41, 0x2468);
		data.write_word(0x0d42, 0x369a);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0b30}, {cpu_regs::STATE_AR2, 0x0d40}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(151);
	}
	if (m_phase == 151)
	{
		expect_opcode(0x7f92, program.read_word(0x0b30) == 0x1357 &&
				program.read_word(0x0b31) == 0x2468 &&
				program.read_word(0x0b32) == 0x369a &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0b30 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d43 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 10,
				"repeated WRITA advances program and data addresses at one cycle after setup");
		return begin_atomic_case(memory_transfer_examples().front(), 152);
	}
	if (m_phase >= 152 && m_phase <= 185)
	{
		auto const &cases = memory_transfer_examples();
		unsigned const index = m_phase - 152;
		verify_atomic_case(cases[index]);
		if (index + 1 < cases.size()) return begin_atomic_case(cases[index + 1], m_phase + 1);
		u16 pc = 0x0600;
		for (unsigned i = 0; i != std::size(rom4_saved_mmr); ++i)
		{
			program.write_word(pc++, 0x7700 | rom4_saved_mmr[i]); // STM #lk, MMR
			program.write_word(pc++, 0x4000 + i);
		}
		for (u8 mmr : rom4_saved_mmr)
			program.write_word(pc++, 0x4a00 | mmr); // PSHM MMR
		program.write_word(pc, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
		return resume_atomic_at(0x0600, 186);
	}
	if (m_phase == 186)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02f4,
				"ROM4 MMR burst pushes twelve values");
		for (unsigned i = 0; i != std::size(rom4_saved_mmr); ++i)
		{
			const bool correct_value = data.read_word(0x02ff - i) == 0x4000 + i;
			expect_opcode(0x7700 | rom4_saved_mmr[i], correct_value,
					"ROM4 STM initializes the selected register");
			expect_opcode(0x4a00 | rom4_saved_mmr[i], correct_value,
					"ROM4 MMR burst pushes the selected register");
		}
		u16 pc = 0x0640;
		for (unsigned i = 0; i != std::size(rom4_saved_mmr); ++i)
		{
			program.write_word(pc++, 0x7700 | rom4_saved_mmr[i]);
			program.write_word(pc++, 0x5000 + i);
		}
		for (unsigned i = std::size(rom4_saved_mmr); i != 0; --i)
			program.write_word(pc++, 0x8a00 | rom4_saved_mmr[i - 1]); // POPM MMR
		for (u8 mmr : rom4_saved_mmr)
			program.write_word(pc++, 0x4a00 | mmr);
		program.write_word(pc, 0xf5e1);
		return resume_atomic_at(0x0640, 187);
	}
	if (m_phase == 187)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02f4,
				"ROM4 MMR burst pops then pushes twelve values");
		for (unsigned i = 0; i != std::size(rom4_saved_mmr); ++i)
			expect_opcode(0x8a00 | rom4_saved_mmr[i],
					data.read_word(0x02ff - i) == 0x4000 + i,
					"ROM4 MMR burst restores the selected register");
		program.write_word(0x05e2, 0x0883); // SUB *AR3,A
		program.write_word(0x05e3, 0x1d83); // XOR *AR3,B
		program.write_word(0x05e4, 0x1c83); // XOR *AR3,A
		program.write_word(0x05e5, 0x7713); // STM #0f01,AR3
		program.write_word(0x05e6, 0x0f01);
		program.write_word(0x05e7, 0x1c93); // XOR *AR3+,A
		program.write_word(0x05e8, 0x75d6);
		program.write_word(0x05e9, 0x0124);
		program.write_word(0x05ea, 0xf5e1);
		data.write_word(0x0f00, 0xffff);
		data.write_word(0x0f01, 0x00f0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 5);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x123400);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x0f00}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(188);
	}
	if (m_phase == 188)
	{
		expect_opcode(0x0883, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff09,
				"ROM4 signed SUB and distinct XOR operands produce the expected A result");
		expect_opcode(0x1d83, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12cbff,
				"ROM4 XOR Smem,B uses the unextended word");
		expect_opcode(0x1c83, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff09,
				"ROM4 XOR Smem,A uses the unextended first word");
		expect_opcode(0x7713, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f02,
				"ROM4 STM switches the indirect source address");
		expect_opcode(0x1c93, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff09 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 8,
				"ROM4 XOR *AR3+ uses the second word and advances in one cycle");
		program.write_word(0x05e2, 0xf4e1); // IDLE 1
		program.write_word(0x05e3, 0xffff); // Must not execute before wake.
		return resume_atomic_at(0x05e2, 189);
	}
	if (m_phase == 189)
	{
		expect_opcode(0xf4e1, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e3 &&
				!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
				"ROM4 IDLE 1 retains the next PC and waits for an interrupt");
		program.write_word(0x05e2, 0xed00); // LD #0,ASM
		program.write_word(0x05e3, 0xf482); // LD A,ASM,A
		program.write_word(0x05e4, 0x8083); // STL A,*AR3
		program.write_word(0x05e5, 0x75d6);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		data.write_word(0x0f10, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x011f); // SXM, ASM=-1
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x0f10}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(190);
	}
	if (m_phase == 190)
	{
		expect_opcode(0xed00, (m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x1f) == 0,
				"ROM4 LD #0,ASM clears the prior negative shift");
		expect_opcode(0xf482, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234,
				"ASM-based load keeps A unchanged after ASM reset");
		expect_opcode(0x8083, data.read_word(0x0f10) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f10 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 STL A,*AR3 stores the low word without pointer change in one cycle");
		return begin_atomic_case(register_bit_examples()[0], 191);
	}
	if (m_phase == 191)
	{
		verify_atomic_case(register_bit_examples()[0]);
		return begin_atomic_case(register_bit_examples()[1], 192);
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
		return begin_atomic_case(register_bit_examples()[2], 195);
	}
	if (m_phase == 195)
	{
		verify_atomic_case(register_bit_examples()[2]);
		return begin_atomic_case(register_bit_examples()[3], 196);
	}
	if (m_phase == 196 || m_phase == 197)
	{
		verify_atomic_case(register_bit_examples()[m_phase == 196 ? 3 : 4]);
		return m_phase == 196 ? begin_atomic_case(register_bit_examples()[4], 197) :
			begin_atomic_case(register_bit_examples()[5], 198);
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
		verify_atomic_case(register_bit_examples()[11]);
		return begin_atomic_case(register_bit_examples()[12], 212);
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
		write_words(program, {
			{0x05e0, 0x75d6}, // PORTW *AR6+%, port
			{0x05e1, 0x0124},
			{0x05e2, 0x768a}, // STM #lk, *AR2-
			{0x05e3, 0xabcd}, {0x05e4, 0x75d6}, {0x05e5, 0x0124},
			{0x05e6, 0x8092}, // STL A, *AR2+
			{0x05e7, 0x75d6}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_AR2, 0x0d80}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(216);
	}
	if (m_phase == 216)
	{
		expect_opcode(0x768a, data.read_word(0x0d80) == 0xabcd &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 4,
				"ROM4 ST #lk,*AR2- writes before decrement in two cycles");
		expect_opcode(0x8092, data.read_word(0x0d7f) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d80 &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 STL A,*AR2+ writes the low word and restores AR2 in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, // PORTW *AR6+%, port
			{0x05e1, 0x0124},
			{0x05e2, 0xe5a8}, // MVDD *AR4+, *AR2+
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0xe5ca}, // MVDD *AR2+0%, *AR4+
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0e00, 0x1357);
		data.write_word(0x0d03, 0x2468);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR2, 0x0d02}, {cpu_regs::STATE_AR4, 0x0e00},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(217);
	}
	if (m_phase == 217)
	{
		expect_opcode(0xe5a8, data.read_word(0x0d02) == 0x1357 &&
				m_middle_port_cycle - m_first_port_cycle == 3,
				"ROM4 MVDD *AR4+,*AR2+ copies before both increments in one cycle");
		expect_opcode(0xe5ca, data.read_word(0x0e01) == 0x2468 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0e02 &&
				m_port_writes == 3 && m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 MVDD *AR2+0%,*AR4+ wraps the X pointer in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, // PORTW *AR6+%, port
			{0x05e1, 0x0124},
			{0x05e2, 0x12f8}, // LDU *(lk), A
			{0x05e3, 0x0d40}, {0x05e4, 0x75d6}, {0x05e5, 0x0124},
			{0x05e6, 0x138b}, // LDU *AR3-, B
			{0x05e7, 0x75d6}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		data.write_word(0x0d40, 0x8001);
		data.write_word(0x0d50, 0xff80);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR3, 0x0d50},
			{cpu_regs::STATE_AR6, 0x0a03},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM does not sign-extend LDU.
		return resume_atomic_at(0x05e0, 218);
	}
	if (m_phase == 218)
	{
		expect_opcode(0x12f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 4,
				"ROM4 absolute LDU zero-extends despite SXM and costs two cycles");
		expect_opcode(0x138b, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d4f &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 indirect LDU reads before decrement and costs one cycle");
		return begin_atomic_case(register_bit_examples()[7], 219);
	}
	if (m_phase == 219)
	{
		verify_atomic_case(register_bit_examples()[7]);
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
		write_words(program, {
			{0x05e0, 0x75d6}, // PORTW *AR6+%, port
			{0x05e1, 0x0124},
			{0x05e2, 0x7182}, // MVDK *AR2, dmad
			{0x05e3, 0x0d71}, {0x05e4, 0x75d6}, {0x05e5, 0x0124},
			{0x05e6, 0x8183}, // STL B, *AR3
			{0x05e7, 0x75d6}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		data.write_word(0x0d70, 0xaaa5);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x1234abcd}, {cpu_regs::STATE_AR2, 0x0d70}, {cpu_regs::STATE_AR3, 0x0d72},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(226);
	}
	if (m_phase == 226)
	{
		expect_opcode(0x7182, data.read_word(0x0d71) == 0xaaa5 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d70 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 4,
				"ROM4 MVDK *AR2,dmad copies through a fixed pointer in two cycles");
		expect_opcode(0x8183, data.read_word(0x0d72) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d72 &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 STL B,*AR3 stores BL without changing AR3 in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x4492}, // LD *AR2+,16,A
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0x458b}, // LD *AR3-,16,B
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0d00, 0x8001);
		data.write_word(0x0d10, 0xff80);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR2, 0x0d00},
			{cpu_regs::STATE_AR3, 0x0d10}, {cpu_regs::STATE_AR6, 0x0a03},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 227);
	}
	if (m_phase == 227)
	{
		expect_opcode(0x4492, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80010000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d01 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
				"ROM4 LD *AR2+,16,A sign-extends, increments, and costs one cycle");
		expect_opcode(0x458b, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff800000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d0f &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 LD *AR3-,16,B sign-extends, decrements, and costs one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x4493}, // LD *AR3+,16,A
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0x458a}, // LD *AR2-,16,B
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0d11, 0x7fff);
		data.write_word(0x0d01, 0x0080);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR2, 0x0d01},
			{cpu_regs::STATE_AR3, 0x0d11}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(228);
	}
	if (m_phase == 228)
	{
		expect_opcode(0x4493, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x7fff0000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d12 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
				"ROM4 LD *AR3+,16,A loads positive data and increments in one cycle");
		expect_opcode(0x458a, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00800000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d00 &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 LD *AR2-,16,B loads positive data and decrements in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x1a82}, // OR *AR2,A
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0x1b83}, // OR *AR3,B
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0d20, 0x00f0);
		data.write_word(0x0d30, 0x0f0f);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12340000}, {cpu_regs::STATE_B, 0xaaaa0000}, {cpu_regs::STATE_AR2, 0x0d20},
			{cpu_regs::STATE_AR3, 0x0d30}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(229);
	}
	if (m_phase == 229)
	{
		expect_opcode(0x1a82, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123400f0 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d20 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
				"ROM4 OR *AR2,A preserves AR2 and costs one cycle");
		expect_opcode(0x1b83, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xaaaa0f0f &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d30 &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 OR *AR3,B preserves AR3 and costs one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x1a83}, // OR *AR3,A
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0x1b82}, // OR *AR2,B
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0d20, 0x3333);
		data.write_word(0x0d30, 0x5555);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xaaaa0000}, {cpu_regs::STATE_B, 0xcccc0000}, {cpu_regs::STATE_AR2, 0x0d20},
			{cpu_regs::STATE_AR3, 0x0d30}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(230);
	}
	if (m_phase == 230)
	{
		expect_opcode(0x1a83, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xaaaa5555 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d30 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
				"ROM4 OR *AR3,A loads AR3 data in one cycle");
		expect_opcode(0x1b82, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xcccc3333 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d20 &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 OR *AR2,B loads AR2 data in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x6d8c}, // MAR *AR4-
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0x1c84}, // XOR *AR4,A
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0d40, 0x0f0f);
		data.write_word(0x0d41, 0xf0f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12340000}, {cpu_regs::STATE_AR4, 0x0d41}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(231);
	}
	if (m_phase == 231)
	{
		expect_opcode(0x6d8c, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d40 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
				"ROM4 MAR *AR4- decrements AR4 in one cycle");
		expect_opcode(0x1c84, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12340f0f &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 XOR *AR4,A reads the decremented address in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x108a}, // LD *AR2-,A
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0x8084}, // STL A,*AR4
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0d51, 0x8001);
		data.write_word(0x0d60, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 0x0d51}, {cpu_regs::STATE_AR4, 0x0d60},
			{cpu_regs::STATE_AR6, 0x0a03},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 232);
	}
	if (m_phase == 232)
	{
		expect_opcode(0x108a, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d50 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
				"ROM4 LD *AR2-,A sign-extends, decrements, and costs one cycle");
		expect_opcode(0x8084, data.read_word(0x0d60) == 0x8001 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d60 &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 STL A,*AR4 stores AL without changing AR4 in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x1a8b}, // OR *AR3-,A
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0xe598}, // MVDD *AR3+,*AR2+
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0d6f, 0x5555);
		data.write_word(0x0d70, 0x00f0);
		data.write_word(0x0d80, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x100000}, {cpu_regs::STATE_AR2, 0x0d80}, {cpu_regs::STATE_AR3, 0x0d70},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(233);
	}
	if (m_phase == 233)
	{
		expect_opcode(0x1a8b, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1000f0 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
				"ROM4 OR *AR3-,A uses the old address in one cycle");
		expect_opcode(0xe598, data.read_word(0x0d80) == 0x5555 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d70 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d81 &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 MVDD copies from decremented AR3 and advances both pointers in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xe742}, // MVMM AR4,AR2
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0xe743}, // MVMM AR4,AR3
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0}, {cpu_regs::STATE_AR3, 0}, {cpu_regs::STATE_AR4, 0x0d90},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(234);
	}
	if (m_phase == 234)
	{
		expect_opcode(0xe742, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d90 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
				"ROM4 MVMM AR4,AR2 copies an auxiliary register in one cycle");
		expect_opcode(0xe743, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0d90 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0d90 &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"ROM4 MVMM AR4,AR3 preserves its source in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xe748}, // MVMM AR4,SP
			{0x05e3, 0x75d6}, {0x05e4, 0x0124},
			{0x05e5, 0xe782}, // MVMM SP,AR2
			{0x05e6, 0x75d6}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0}, {cpu_regs::STATE_AR4, 0x0d90}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(235);
	}
	if (m_phase == 235)
	{
		expect_opcode(0xe748, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0d90 &&
				m_port_writes == 3 && m_middle_port_cycle - m_first_port_cycle == 3,
				"MVMM AR4,SP accepts SP as destination in one cycle");
		expect_opcode(0xe782, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d90 &&
				m_last_port_cycle - m_middle_port_cycle == 3,
				"MVMM SP,AR2 accepts SP as source in one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xe5c9}, // MVDD *AR2+0%,*AR3+
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		data.write_word(0x0e04, 0x2468);
		data.write_word(0x0e10, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 1}, {cpu_regs::STATE_AR2, 0x0e04}, {cpu_regs::STATE_AR3, 0x0e10},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_BK, 5}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(236);
	}
	if (m_phase == 236)
	{
		expect_opcode(0xe5c9, data.read_word(0x0e10) == 0x2468 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0e00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e11 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 MVDD wraps source AR2 and increments destination AR3 in one cycle");
		program.write_word(0x05e2, 0xe5e9); // MVDD *AR4+0%,*AR3+.
		data.write_word(0x0e04, 0x1357);
		data.write_word(0x0e12, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR4, 0x0e04}, {cpu_regs::STATE_AR3, 0x0e12}, {cpu_regs::STATE_A, 0x12345678},
			{cpu_regs::STATE_B, 0x87654321}, {cpu_regs::STATE_ST0, 0x1800}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6584);
	}
	if (m_phase == 6584)
	{
		expect_opcode(0xe5e9,
			data.read_word(0x0e12) == 0x1357 && data.read_word(0x0e04) == 0x1357 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0e00 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0e13 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0e00 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x87654321 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1800 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 MVDD reads old AR4 before circular wrap and increments AR3 independently while preserving source/accumulators/status in one cycle");
		return begin_atomic_case(register_bit_examples()[13], 237);
	}
	if (m_phase == 237)
	{
		verify_atomic_case(register_bit_examples()[13]);
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x1293}, // LDU *AR3+,A
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		data.write_word(0x0ef0, 0xff80);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffabcdef00ULL}, {cpu_regs::STATE_AR3, 0x0ef0}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_ST1, 0x0100}, // SXM must not affect LDU.
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(238);
	}
	if (m_phase == 238)
	{
		expect_opcode(0x1293, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff80 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0ef1 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 LDU *AR3+,A ignores SXM, clears upper bits, and costs one cycle");
		return begin_atomic_case(register_bit_examples()[14], 239);
	}
	if (m_phase == 239)
	{
		verify_atomic_case(register_bit_examples()[14]);
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xe800}, // LD #0,A
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff12345678ULL}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(240);
	}
	if (m_phase == 240)
	{
		expect_opcode(0xe800, m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 LD #0,A clears the 40-bit accumulator in one cycle");
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
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x4ef8}, // DST A, absolute Lmem
			{0x05e3, 0x0f00}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0f00, 0);
		data.write_word(0x0f01, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffabcd1234ULL}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(243);
	}
	if (m_phase == 243)
	{
		expect_opcode(0x4ef8, data.read_word(0x0f00) == 0xabcd &&
				data.read_word(0x0f01) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 absolute DST stores high then low word in three cycles");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x4e8b}, // DST A,*AR3-
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		data.write_word(0x0f10, 0);
		data.write_word(0x0f11, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x00abcd1234ULL}, {cpu_regs::STATE_AR3, 0x0f11}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(244);
	}
	if (m_phase == 244)
	{
		expect_opcode(0x4e8b, data.read_word(0x0f11) == 0xabcd &&
				data.read_word(0x0f10) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f0f &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"odd-address DST stores its high word at AR3 and low word before it");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x5783}, // DLD *AR3,B
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		data.write_word(0x0f20, 0x5678);
		data.write_word(0x0f21, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR3, 0x0f21}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_ST1, 0x0100}, // SXM
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(245);
	}
	if (m_phase == 245)
	{
		expect_opcode(0x5783, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80015678ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f21 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"odd-address DLD loads high word from AR3 and low word before it");
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
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0x1092}, // LD *AR2+, A
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		data.write_word(0x0f20, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 0x0f20}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_ST1, 0x0100}, // SXM
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(247);
	}
	if (m_phase == 247)
	{
		expect_opcode(0x1092, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f21 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 LD *AR2+,A sign-extends, postincrements, and costs one cycle");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xfc4b}, // RC BLT, false
			{0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 1}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(248);
	}
	if (m_phase == 248)
	{
		expect_opcode(0xfc4b, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 RC BLT false retains SP and costs three cycles");
		program.write_word(0x05e3, 0xf5e1); // Must be bypassed by the return.
		program.write_word(0x05e8, 0x75d6);
		program.write_word(0x05e9, 0x0124);
		program.write_word(0x05ea, 0xf5e1);
		data.write_word(0x0300, 0x05e8);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0xff00000000ULL}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(249);
	}
	if (m_phase == 249)
	{
		expect_opcode(0xfc4b, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 RC BLT true tests the 40-bit sign and costs five cycles");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xfa45}, // BCD pmad, AEQ
			{0x05e3, 0x05ec},
			{0x05e4, 0xe801}, // First delay word
			{0x05e5, 0xe802}, // Second delay word
			{0x05e6, 0x75d6}, // Fallthrough marker
			{0x05e7, 0x0124}, {0x05e8, 0xf5e1},
			{0x05ec, 0x75d6}, // Taken marker
			{0x05ed, 0x0124}, {0x05ee, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(250);
	}
	if (m_phase == 250)
	{
		expect_opcode(0xfa45, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e9 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 BCD AEQ false executes both delay words and costs three cycles");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(251);
	}
	if (m_phase == 251)
	{
		expect_opcode(0xfa45, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ef &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 BCD AEQ true executes both delay words before target in three cycles");
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xf947}, // CC pmad, ALEQ
			{0x05e3, 0x05ec},
			{0x05e4, 0x75d6}, // Fallthrough marker
			{0x05e5, 0x0124}, {0x05e6, 0xf5e1},
			{0x05ec, 0x75d6}, // Called marker
			{0x05ed, 0x0124}, {0x05ee, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(252);
	}
	if (m_phase == 252)
	{
		expect_opcode(0xf947, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e7 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 CC ALEQ false does not push and costs three cycles");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff00000000ULL}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(253);
	}
	if (m_phase == 253)
	{
		expect_opcode(0xf947, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
				data.read_word(0x02ff) == 0x05e4 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ef &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"ROM4 CC ALEQ tests the 40-bit sign, pushes return PC, and costs five cycles");
		program.write_word(0x05e2, 0xf930); // CC pmad,TC.
		data.write_word(0x02ff, 0xbeef);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_B, 2}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_SP, 0x0300},
			{cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6587);
	}
	if (m_phase == 6587 || m_phase == 6588)
	{
		const bool taken = m_phase == 6588;
		expect_opcode(0xf930,
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (taken ? 0x02ff : 0x0300) &&
			data.read_word(0x02ff) == (taken ? 0x05e4 : 0xbeef) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x05ef : 0x05e7) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 2 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (taken ? 0x1800 : 0x0800) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 5),
			"ROM4 CC TC preserves arithmetic state, pushes extension-complete return PC only when taken and costs five/three cycles");
		if (!taken)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_ST0, 0x1800}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(6588);
		}
		program.write_word(0x05e2, 0x6e8f); // BANZD pmad, *AR7-
		program.write_word(0x05e3, 0x05ec);
		program.write_word(0x05e4, 0xe801); // First delay word
		program.write_word(0x05e5, 0xe802); // Second delay word
		program.write_word(0x05e6, 0x75d6); // Fallthrough marker
		program.write_word(0x05e7, 0x0124);
		program.write_word(0x05e8, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR7, 0}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(254);
	}
	if (m_phase == 254)
	{
		expect_opcode(0x6e8f, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0xffff &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e9 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 BANZD false decrements AR7, executes delay words, and costs two cycles");
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR7, 1}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(255);
	}
	if (m_phase == 255)
	{
		expect_opcode(0x6e8f, m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05ef &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ROM4 BANZD branches on pre-decrement AR7 after delay words in two cycles");
		program.write_word(0x05e2, 0x6e82); // BANZD pmad,*AR2 (no modification).
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_AR2, 0},
			{cpu_regs::STATE_AR7, 0x1234}, {cpu_regs::STATE_AR6, 0x0a03}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6545);
	}
	if (m_phase == 6545 || m_phase == 6546)
	{
		const bool taken = m_phase == 6546;
		expect_opcode(0x6e82,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (taken ? 1 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x05ef : 0x05e9) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
			"ROM4 BANZD tests AR2 without modification, executes both delay words and selects the zero/nonzero continuation in two cycles");
		if (!taken)
		{
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 1}, {cpu_regs::STATE_AR6, 0x0a03},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(6546);
		}
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
		return begin_atomic_case(register_bit_examples()[15], 263);
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
		program.write_word(0x05e2, 0x8091); // STL A,*AR1+
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f20, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_AR1, 0x0f20}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(265);
	}
	if (m_phase == 265)
	{
		expect_opcode(0x8091, data.read_word(0x0f20) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f21 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 STL A,*AR1+ stores AL and postincrements in one cycle");
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
		write_words(program, {
			{0x05e0, 0x75f8}, // Absolute marker does not modify AR6.
			{0x05e1, 0x0d00}, {0x05e2, 0x0124},
			{0x05e3, 0x1086}, // LD *AR6,A
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0d00, 0x1234);
		data.write_word(0x0f20, 0xff80);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x0f20);
		return resume_atomic_at(0x05e0, 269);
	}
	if (m_phase == 269)
	{
		expect_opcode(0x1086, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffff80ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD *AR6,A sign-extends without pointer update in one cycle");
		program.write_word(0x05e0, 0x75f8);
		program.write_word(0x05e1, 0x0d00);
		program.write_word(0x05e2, 0x0124);
		program.write_word(0x05e3, 0x6f8b); // LD *AR3-,0,A
		write_words(program, {
			{0x05e4, 0x0c40}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f21, 0xff80);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0f21);
		return resume_atomic_at(0x05e0, 270);
	}
	if (m_phase == 270)
	{
		expect_opcode(0x6f8b, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffffff80ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 extended LD *AR3-,0,A sign-extends and costs two cycles");
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
		program.write_word(0x05e0, 0x75f8);
		program.write_word(0x05e1, 0x0d00);
		program.write_word(0x05e2, 0x0124);
		program.write_word(0x05e3, 0xf310); // SUB #lk,B
		write_words(program, {
			{0x05e4, 0xff80}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 5);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 273);
	}
	if (m_phase == 273)
	{
		expect_opcode(0xf310, m_cpu->state_int(tms320c54x_device::STATE_B) == 133 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 SUB #lk,B sign-extends with SXM and costs two cycles");
		write_words(program, {
			{0x05e0, 0x75f8}, {0x05e1, 0x0d00}, {0x05e2, 0x0124},
			{0x05e3, 0x6d8a}, // MAR *AR2-
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f21);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // Standard addressing mode.
		return resume_atomic_at(0x05e0, 274);
	}
	if (m_phase == 274)
	{
		expect_opcode(0x6d8a, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 MAR *AR2- modifies the pointer in one cycle");
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
		program.write_word(0x05e0, 0x75f8);
		program.write_word(0x05e1, 0x0d00);
		program.write_word(0x05e2, 0x0124);
		program.write_word(0x05e3, 0x11f8); // LD *(lk),B
		write_words(program, {
			{0x05e4, 0x0f21}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f21, 0xff80);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 277);
	}
	if (m_phase == 277)
	{
		expect_opcode(0x11f8, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffffff80ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 absolute LD Smem,B sign-extends and costs two cycles");
		program.write_word(0x05e3, 0xf110); // SUB #lk,A,B
		program.write_word(0x05e4, 0xff80);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 5);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 278);
	}
	if (m_phase == 278)
	{
		expect_opcode(0xf110, m_cpu->state_int(tms320c54x_device::STATE_B) == 133 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 5 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 SUB #lk,A,B reads A, writes B, and costs two cycles");
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
		program.write_word(0x05e0, 0x75f8);
		program.write_word(0x05e1, 0x0d00);
		program.write_word(0x05e2, 0x0124);
		program.write_word(0x05e3, 0x56f8); // DLD *(lk),A
		write_words(program, {
			{0x05e4, 0x0f20}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f20, 0xff80);
		data.write_word(0x0f21, 0x1234);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 280);
	}
	if (m_phase == 280)
	{
		expect_opcode(0x56f8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffff801234ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 DLD absolute reads high/low words and costs two cycles");
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
		write_words(program, {
			{0x05e0, 0x75d6}, {0x05e1, 0x0124},
			{0x05e2, 0xec0e}, // RPT #14
			{0x05e3, 0x0082}, // ADD *AR2,A
			{0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0f21, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 0x0f21}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(283);
	}
	if (m_phase == 283)
	{
		expect_opcode(0xec0e, m_cpu->state_int(tms320c54x_device::STATE_A) == 15 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f21 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 18,
				"ROM4 RPT #14 runs ADD fifteen times after one-cycle setup");
		program.write_word(0x05e2, 0xe753); // MVMM AR5,AR3
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR5, 0x1234}, {cpu_regs::STATE_AR3, 0}, {cpu_regs::STATE_AR6, 0x0a03},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(284);
	}
	if (m_phase == 284)
	{
		expect_opcode(0xe753, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1234 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 MVMM copies AR5 to AR3 in one cycle");
		program.write_word(0x05e2, 0xe723); // MVMM AR2,AR3.
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0xabcd}, {cpu_regs::STATE_AR3, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6521);
	}
	if (m_phase == 6521)
	{
		expect_opcode(0xe723,
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1234 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"ROM4 MVMM selects AR2 rather than AR5, preserves source and costs one cycle");
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
		program.write_word(0x05e0, 0x75f8);
		program.write_word(0x05e1, 0x0d00);
		program.write_word(0x05e2, 0x0124);
		program.write_word(0x05e3, 0xf063); // AND #lk,16,A
		write_words(program, {
			{0x05e4, 0x0ff0}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // C
		return resume_atomic_at(0x05e0, 289);
	}
	if (m_phase == 289)
	{
		expect_opcode(0xf063, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x02300000 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 AND #lk,16,A masks upper word and costs two cycles");
		write_words(program, {
			{0x05e3, 0x8184}, // STL B,*AR4
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0f21, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_AR4, 0x0f21}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(290);
	}
	if (m_phase == 290)
	{
		expect_opcode(0x8184, data.read_word(0x0f21) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f21 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 STL B,*AR4 stores BL without pointer update in one cycle");
		program.write_word(0x05e3, 0x8180); // STL B,*AR0.
		data.write_word(0x0f23, 0);
		data.write_word(0x0f24, 0xcafe);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffabcd1234ULL}, {cpu_regs::STATE_AR0, 0x0f23}, {cpu_regs::STATE_ST0, 0x1800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(6573);
	}
	if (m_phase == 6573)
	{
		expect_opcode(0x8180,
			data.read_word(0x0f23) == 0x5678 && data.read_word(0x0f24) == 0xcafe &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f23 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f21 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffabcd1234ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1800 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 STL B through stationary AR0 preserves accumulators/status/pointers and the adjacent word in one cycle");
		program.write_word(0x05e3, 0x7708); // STM #lk,AL.
		write_words(program, {
			{0x05e4, 0xbeef}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 6574);
	}
	if (m_phase == 6574)
	{
		expect_opcode(0x7708,
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffabcdbeefULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1800 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e9 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"ROM4 STM to AL replaces only the low word, preserves A high/guard/B/status and consumes its literal in two cycles");
		program.write_word(0x05e3, 0xf162); // LD #lk,16,B
		write_words(program, {
			{0x05e4, 0xff80}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 291);
	}
	if (m_phase == 291)
	{
		expect_opcode(0xf162, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff800000ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 LD #lk,16,B sign-extends and costs two cycles");
		write_words(program, {
			{0x05e3, 0x6d96}, // MAR *AR6+
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR6, 0x0f42}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(292);
	}
	if (m_phase == 292)
	{
		expect_opcode(0x6d96, m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0f43 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 MAR *AR6+ increments AR6 in one cycle");
		program.write_word(0x05e3, 0x6d95); // MAR *AR5+.
		data.write_word(0x0f50, 0xbeef);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f50);
		return resume_atomic_at(0x05e0, 6537);
	}
	if (m_phase == 6537)
	{
		expect_opcode(0x6d95,
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f51 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x0f43 &&
			data.read_word(0x0f50) == 0xbeef &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 MAR increments AR5 exactly once without changing AR6 or pointed memory in one cycle");
		write_words(program, {
			{0x05e3, 0xe712}, // MVMM AR1,AR2
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR1, 0x1a23}, {cpu_regs::STATE_AR2, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(293);
	}
	if (m_phase == 293)
	{
		expect_opcode(0xe712, m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1a23 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x1a23 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 MVMM AR1,AR2 copies without modifying source in one cycle");
		write_words(program, {
			{0x05e3, 0xe713}, // MVMM AR1,AR3
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0);
		return resume_atomic_at(0x05e0, 294);
	}
	if (m_phase == 294)
	{
		expect_opcode(0xe713, m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1a23 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x1a23 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 MVMM AR1,AR3 copies without modifying source in one cycle");
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
		program.write_word(0x05e3, 0x06ea); // ADDC *+AR2(5),A
		write_words(program, {
			{0x05e4, 5}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f05, 4);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x13);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // C
		return resume_atomic_at(0x05e0, 312);
	}
	if (m_phase == 312)
	{
		expect_opcode(0x06ea, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x18 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ADDC *+AR2(lk),A preupdates AR and costs two cycles");
		program.write_word(0x05e3, 0x02e2); // ADDS *AR2(5),A
		data.write_word(0x0f05, 0xffff);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(313);
	}
	if (m_phase == 313)
	{
		expect_opcode(0x02e2, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ADDS *AR2(lk),A uses offset address without updating AR");
		program.write_word(0x05e3, 0x02f2); // ADDS *+AR2(2)%,A
		program.write_word(0x05e4, 2);
		data.write_word(0x0f01, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 1}, {cpu_regs::STATE_AR2, 0x0f03}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(314);
	}
	if (m_phase == 314)
	{
		expect_opcode(0x02f2, m_cpu->state_int(tms320c54x_device::STATE_A) == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f01 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ADDS *+AR2(lk)%,A wraps circular address before read in two cycles");
		program.write_word(0x05e3, 0x80ea); // STL A,*+AR2(-2)
		program.write_word(0x05e4, 0xfffe);
		data.write_word(0x0f0e, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_AR2, 0x0f10}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(315);
	}
	if (m_phase == 315)
	{
		expect_opcode(0x80ea, data.read_word(0x0f0e) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f0e &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"STL A,*+AR2(lk) stores after signed preupdate in two cycles");
		program.write_word(0x05e3, 0x6bea); // ADDM #1,*+AR2(5)
		write_words(program, {
			{0x05e4, 5}, {0x05e5, 1}, {0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		data.write_word(0x0f05, 3);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 316);
	}
	if (m_phase == 316)
	{
		expect_opcode(0x6bea, data.read_word(0x0f05) == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ADDM *+AR2(lk) consumes offset before immediate in three cycles");
		program.write_word(0x05e3, 0x68e2); // ANDM #0f0f,*AR2(5)
		program.write_word(0x05e4, 0x0f0f);
		program.write_word(0x05e5, 5);
		data.write_word(0x0f05, 0xff00);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800); // C
		return resume_atomic_at(0x05e0, 317);
	}
	if (m_phase == 317)
	{
		expect_opcode(0x68e2, data.read_word(0x0f05) == 0x0f00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ANDM *AR2(lk) uses offset without AR update or status change");
		program.write_word(0x05e3, 0x69f2); // ORM #8000,*+AR2(2)%
		program.write_word(0x05e4, 0x8000);
		program.write_word(0x05e5, 2);
		data.write_word(0x0f01, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0f03}, {cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(318);
	}
	if (m_phase == 318)
	{
		expect_opcode(0x69f2, data.read_word(0x0f01) == 0x8001 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f01 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ORM *+AR2(lk)% wraps circular address and preserves carry");
		program.write_word(0x05e3, 0x76ea); // ST #cafe,*+AR2(5)
		program.write_word(0x05e4, 5);
		program.write_word(0x05e5, 0xcafe);
		data.write_word(0x0f05, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 319);
	}
	if (m_phase == 319)
	{
		expect_opcode(0x76ea, data.read_word(0x0f05) == 0xcafe &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"ST *+AR2(lk) consumes offset before immediate in three cycles");
		program.write_word(0x05e3, 0x82ea); // STH A,*+AR2(5)
		write_words(program, {
			{0x05e4, 5}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f05, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(320);
	}
	if (m_phase == 320)
	{
		expect_opcode(0x82ea, data.read_word(0x0f05) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"STH A,*+AR2(lk) stores high word after preupdate in two cycles");
		program.write_word(0x05e3, 0x83e2); // STH B,*AR2(5)
		data.write_word(0x0f05, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x56789abc}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(321);
	}
	if (m_phase == 321)
	{
		expect_opcode(0x83e2, data.read_word(0x0f05) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"STH B,*AR2(lk) stores high word without AR update in two cycles");
		program.write_word(0x05e3, 0x8cea); // ST T,*+AR2(5)
		data.write_word(0x0f05, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 0x4321}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(322);
	}
	if (m_phase == 322)
	{
		expect_opcode(0x8cea, data.read_word(0x0f05) == 0x4321 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ST T,*+AR2(lk) stores after preupdate in two cycles");
		program.write_word(0x05e3, 0x10ea); // LD *+AR2(5),A
		data.write_word(0x0f05, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 323);
	}
	if (m_phase == 323)
	{
		expect_opcode(0x10ea, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"LD *+AR2(lk),A sign-extends in two cycles");
		program.write_word(0x05e3, 0x12e2); // LDU *AR2(5),A
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 324);
	}
	if (m_phase == 324)
	{
		expect_opcode(0x12e2, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"LDU *AR2(lk),A zero-extends without AR update in two cycles");
		program.write_word(0x05e3, 0x45ea); // LD *+AR2(5),16,B
		data.write_word(0x0f05, 0x1234);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 325);
	}
	if (m_phase == 325)
	{
		expect_opcode(0x45ea, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x12340000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"LD *+AR2(lk),16,B shifts in two cycles");
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
		program.write_word(0x05e3, 0x61ea); // BITF *+AR2(5),#8000
		write_words(program, {
			{0x05e4, 5}, {0x05e5, 0x8000}, {0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		data.write_word(0x0f05, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(337);
	}
	if (m_phase == 337)
	{
		expect_opcode(0x61ea, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1800) == 0x1800 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"long-offset BITF sets TC and preserves carry in three cycles");
		program.write_word(0x05e3, 0x60e2); // CMPM *AR2(5),#1235
		program.write_word(0x05e4, 0x1235);
		program.write_word(0x05e5, 5);
		data.write_word(0x0f05, 0x1234);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 338);
	}
	if (m_phase == 338)
	{
		expect_opcode(0x60e2, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1800) == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"long-offset CMPM clears TC without AR update in three cycles");
		program.write_word(0x05e3, 0x6180); // BITF *AR0,#0001
		write_words(program, {
			{0x05e4, 1}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f20, 4);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR0, 0x0f20}, {cpu_regs::STATE_ST0, 0x1800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(339);
	}
	if (m_phase == 339)
	{
		expect_opcode(0x6180, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1800) == 0x0800 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"BITF *AR0 clears TC when masked bit is absent in two cycles");
		write_words(program, {
			{0x05e3, 0xe736}, // MVMM AR3,AR6
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x1357}, {cpu_regs::STATE_AR6, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(340);
	}
	if (m_phase == 340)
	{
		expect_opcode(0xe736, m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1357 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1357 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"MVMM AR3,AR6 transfers the MMR word in one cycle");
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
		program.write_word(0x05e3, 0x4bea); // PSHD *+AR2(5)
		write_words(program, {
			{0x05e4, 5}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f05, 0x1234);
		data.write_word(0x02ff, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(346);
	}
	if (m_phase == 346)
	{
		expect_opcode(0x4bea, data.read_word(0x02ff) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"long-offset PSHD preupdates AR and pushes in two cycles");
		program.write_word(0x05e3, 0x8bea); // POPD *+AR2(5)
		data.write_word(0x02ff, 0xbeef);
		data.write_word(0x0f05, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 347);
	}
	if (m_phase == 347)
	{
		expect_opcode(0x8bea, data.read_word(0x0f05) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"long-offset POPD preupdates AR and pops in two cycles");
		program.write_word(0x05e3, 0x4fea); // DST B,*+AR2(6)
		write_words(program, {
			{0x05e4, 6}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		data.write_word(0x0f06, 0);
		data.write_word(0x0f07, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(348);
	}
	if (m_phase == 348)
	{
		expect_opcode(0x4fea, data.read_word(0x0f06) == 0x1234 &&
				data.read_word(0x0f07) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f06 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"long-offset DST preupdates AR and stores high/low in three cycles");
		program.write_word(0x05e3, 0x57e2); // DLD *AR2(6),B
		data.write_word(0x0f06, 0x8001);
		data.write_word(0x0f07, 0x2345);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 349);
	}
	if (m_phase == 349)
	{
		expect_opcode(0x57e2, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff80012345ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"long-offset DLD keeps AR and sign-extends in two cycles");
		program.write_word(0x05e3, 0x40ea); // SUB *+AR2(5),16,A
		program.write_word(0x05e4, 5);
		data.write_word(0x0f05, 1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x30000}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(350);
	}
	if (m_phase == 350)
	{
		expect_opcode(0x40ea, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x20000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"long-offset SUB preupdates AR and takes two cycles");
		program.write_word(0x05e3, 0x34e2); // BITT *AR2(5)
		program.write_word(0x05e4, 5);
		data.write_word(0x0f05, 0x8000);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 0}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(351);
	}
	if (m_phase == 351)
	{
		expect_opcode(0x34e2, bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"long-offset BITT keeps AR and takes two cycles");
		program.write_word(0x05e3, 0x20ea); // MPY *+AR2(5),A
		program.write_word(0x05e4, 5);
		data.write_word(0x0f05, 3);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_T, 2}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(352);
	}
	if (m_phase == 352)
	{
		expect_opcode(0x20ea, m_cpu->state_int(tms320c54x_device::STATE_A) == 6 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"long-offset MPY preupdates AR and takes two cycles");
		write_words(program, {
			{0x05e3, 0x1282}, // LDU *AR2,A
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0f00, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM must not sign-extend LDU.
		return resume_atomic_at(0x05e0, 353);
	}
	if (m_phase == 353)
	{
		expect_opcode(0x1282, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LDU *AR2,A zero-extends without changing AR2 in one cycle");
		program.write_word(0x05e3, 0x1082); // LD *AR2,A
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(354);
	}
	if (m_phase == 354)
	{
		expect_opcode(0x1082, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f00 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD *AR2,A sign-extends without changing AR2 in one cycle");
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
		write_words(program, {
			{0x05e3, 0x8914}, // STLM B,AR4
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x5678cafe}, {cpu_regs::STATE_AR4, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(360);
	}
	if (m_phase == 360)
	{
		expect_opcode(0x8914, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xcafe &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 STLM B,AR4 stores BL in one cycle");
		program.write_word(0x05e3, 0x71ea); // MVDK *+AR2(5),0f20
		write_words(program, {
			{0x05e4, 5}, {0x05e5, 0x0f20}, {0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
		});
		data.write_word(0x0f05, 0xbeef);
		data.write_word(0x0f20, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 361);
	}
	if (m_phase == 361)
	{
		expect_opcode(0x71ea, data.read_word(0x0f20) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"long-offset MVDK consumes offset before destination in three cycles");
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
		program.write_word(0x05e3, 0x70ea); // MVKD 0f20,*+AR2(5)
		program.write_word(0x05e4, 5);
		program.write_word(0x05e5, 0x0f20);
		data.write_word(0x0f20, 0x5678);
		data.write_word(0x0f05, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 363);
	}
	if (m_phase == 363)
	{
		expect_opcode(0x70ea, data.read_word(0x0f05) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"long-offset MVKD consumes offset before source and takes three cycles");
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
		program.write_word(0x05e3, 0x7eea); // READA *+AR2(5)
		program.write_word(0x0600, 0xabcd);
		data.write_word(0x0f05, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 366);
	}
	if (m_phase == 366)
	{
		expect_opcode(0x7eea, data.read_word(0x0f05) == 0xabcd &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 9,
				"long-offset READA stores the A-addressed program word in six cycles");
		program.write_word(0x05e3, 0x7cea); // MVPD 0600,*+AR2(5)
		write_words(program, {
			{0x05e4, 0x0600}, {0x05e5, 5}, {0x05e6, 0x75f8}, {0x05e7, 0x0d00}, {0x05e8, 0x0124}, {0x05e9, 0xf5e1},
			{0x0600, 0xbeef},
		});
		data.write_word(0x0f05, 0);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0f00);
		return resume_atomic_at(0x05e0, 367);
	}
	if (m_phase == 367)
	{
		expect_opcode(0x7cea, data.read_word(0x0f05) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 7,
				"long-offset MVPD consumes pmad before offset in four cycles");
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
		program.write_word(0x05e3, 0x6fea); // LD *+AR2(5),0,A
		program.write_word(0x05e4, 5);
		program.write_word(0x05e5, 0x0c40);
		data.write_word(0x0f05, 0x2345);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 0x0f00}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(369);
	}
	if (m_phase == 369)
	{
		expect_opcode(0x6fea, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x2345 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f05 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 6,
				"long-offset shifted LD consumes offset before shift in three cycles");
		write_words(program, {
			{0x05e3, 0xe58b}, // MVDD *AR2+,*AR5+
			{0x05e4, 0x75f8}, {0x05e5, 0x0d00}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		data.write_word(0x0d20, 0x4321);
		data.write_word(0x0e20, 0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0d20}, {cpu_regs::STATE_AR5, 0x0e20}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(370);
	}
	if (m_phase == 370)
	{
		expect_opcode(0xe58b, data.read_word(0x0e20) == 0x4321 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0d21 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0e21 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 MVDD *AR2+,*AR5+ copies before both increments in one cycle");
		program.write_word(0x05e3, 0x13d2); // LDU *AR2+%,B
		data.write_word(0x0f23, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_BK, 4}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR2, 0x0f23},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(371);
	}
	if (m_phase == 371)
	{
		expect_opcode(0x13d2, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8001 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 unsigned LDU wraps AR2 circularly after the read in one cycle");
		program.write_word(0x05e3, 0x1882); // AND *AR2,A
		data.write_word(0x0f20, 0x0f0f);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_AR2, 0x0f20}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(372);
	}
	if (m_phase == 372)
	{
		expect_opcode(0x1882, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0204 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 AND *AR2,A masks without updating the pointer in one cycle");
		program.write_word(0x05e3, 0x1081); // LD *AR1,A
		data.write_word(0x0f20, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0f20);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 373);
	}
	if (m_phase == 373)
	{
		expect_opcode(0x1081, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xffffff8001ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 LD *AR1,A sign-extends under SXM without updating AR1 in one cycle");
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
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		program.write_word(0x05e3, 0xee01); // FRAME #1
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x03ff);
		return resume_atomic_at(0x05e0, 374);
	}
	if (m_phase == 374)
	{
		expect_opcode(0xee01, m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 FRAME #1 advances SP across a page boundary in one cycle");
		program.write_word(0x05e3, 0xf340); // OR #lk,B
		write_words(program, {
			{0x05e4, 0x00f0}, {0x05e5, 0x75f8}, {0x05e6, 0x0d00}, {0x05e7, 0x0124}, {0x05e8, 0xf5e1},
		});
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xab00123400ULL);
		return resume_atomic_at(0x05e0, 375);
	}
	if (m_phase == 375)
	{
		expect_opcode(0xf340, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xab001234f0ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 OR #lk,B preserves upper accumulator bits and takes two cycles");
		program.write_word(0x05e3, 0xf230); // AND #lk,B,A
		program.write_word(0x05e4, 0x0ff0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x123456}, {cpu_regs::STATE_B, 0xffabcdf123ULL}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(376);
	}
	if (m_phase == 376)
	{
		expect_opcode(0xf230, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0120 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffabcdf123ULL &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 AND #lk,B,A masks into A without changing B in two cycles");
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
		write_words(program, {
			{0x05e2, 0xf495}, // NOP between marker and target
			{0x05e3, 0xf032}, // AND #lk,2,A
			{0x05e4, 0x0f0f}, {0x05e5, 0x75d6}, {0x05e6, 0x0124}, {0x05e7, 0xf5e1},
		});
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff12345678ULL}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(450);
	}
	if (m_phase == 450)
	{
		expect_opcode(0xf032, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1438 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 AND #lk,2,A masks the shifted immediate without changing carry");
		program.write_word(0x05e3, 0xf035); // AND #lk,5,A
		program.write_word(0x05e4, 0x00ff);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL);
		return resume_atomic_at(0x05e0, 451);
	}
	if (m_phase == 451)
	{
		expect_opcode(0xf035, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1660 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
				"ROM4 AND #lk,5,A uses five-bit immediate alignment in two cycles");
		program.write_word(0x05e3, 0xf0fb); // SFTL A,-5,A
		program.write_word(0x05e4, 0x75d6);
		program.write_word(0x05e5, 0x0124);
		program.write_word(0x05e6, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff80000013ULL}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(452);
	}
	if (m_phase == 452)
	{
		expect_opcode(0xf0fb, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x04000000 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 SFTL A,-5 clears guard and copies outgoing bit four to carry");
		program.write_word(0x05e3, 0xf0fe); // SFTL A,-2,A
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff80000003ULL}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(453);
	}
	if (m_phase == 453)
	{
		expect_opcode(0xf0fe, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x20000000 &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 SFTL A,-2 logically shifts bit 31 and publishes bit one");
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
		program.write_word(0x05e2, 0xe741); // MVMM AR4,AR1
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR1, 0x1234}, {cpu_regs::STATE_AR4, 0xcdef}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(456);
	}
	if (m_phase == 456)
	{
		expect_opcode(0xe741, m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0xcdef &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xcdef &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 MVMM AR4,AR1 copies only the destination in one cycle");
		program.write_word(0x05e2, 0x1181); // LD *AR1,B
		data.write_word(0x0f20, 0x8001);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x0f20);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 457);
	}
	if (m_phase == 457)
	{
		expect_opcode(0x1181, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffffff8001ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 LD *AR1,B sign-extends under SXM in one cycle");
		program.write_word(0x05e2, 0x1a8a); // OR *AR2-,A
		data.write_word(0x0f20, 0x00f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x120000}, {cpu_regs::STATE_AR2, 0x0f20}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(458);
	}
	if (m_phase == 458)
	{
		expect_opcode(0x1a8a, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1200f0 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f1f &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 OR *AR2-,A uses the old address and decrements in one cycle");
		program.write_word(0x05e2, 0x12d2); // LDU *AR2+%,A
		data.write_word(0x0f23, 0x8001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 0x0f23}, {cpu_regs::STATE_BK, 4},
			{cpu_regs::STATE_ST1, 0x0100}, // SXM must not sign-extend LDU.
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(459);
	}
	if (m_phase == 459)
	{
		expect_opcode(0x12d2, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x8001 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 LDU *AR2+%,A zero-extends and wraps after the read in one cycle");
		write_words(program, {
			{0x05e2, 0x1cf8}, // XOR *(lk),A
			{0x05e3, 0x0f20}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0f20, 0x00f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12000f}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(460);
	}
	if (m_phase == 460)
	{
		expect_opcode(0x1cf8, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1200ff &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 absolute XOR uses its extension, preserves status, and costs two cycles");
		program.write_word(0x05e2, 0x6982); // ORM #lk,*AR2
		program.write_word(0x05e3, 0x00a0);
		data.write_word(0x0f20, 0x1001);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR2, 0x0f20}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(461);
	}
	if (m_phase == 461)
	{
		expect_opcode(0x6982, data.read_word(0x0f20) == 0x10a1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0f20 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 ORM #lk,*AR2 preserves pointer and status in two cycles");
		program.write_word(0x05e2, 0x6984); // ORM #lk,*AR4
		program.write_word(0x05e3, 0x0005);
		data.write_word(0x0f24, 0x8000);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x0f24);
		return resume_atomic_at(0x05e0, 462);
	}
	if (m_phase == 462)
	{
		expect_opcode(0x6984, data.read_word(0x0f24) == 0x8005 &&
				data.read_word(0x0f20) == 0x10a1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0f24 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 ORM #lk,*AR4 selects a distinct data address without pointer motion");
		program.write_word(0x05e2, 0x1b8d); // OR *AR5-,B
		program.write_word(0x05e3, 0x75d6);
		program.write_word(0x05e4, 0x0124);
		program.write_word(0x05e5, 0xf5e1);
		data.write_word(0x0f25, 0x00f0);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x34000f}, {cpu_regs::STATE_AR5, 0x0f25}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(463);
	}
	if (m_phase == 463)
	{
		expect_opcode(0x1b8d, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x3400ff &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f24 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 OR *AR5-,B preserves status and decrements after one cycle");
		program.write_word(0x05e2, 0x4595); // LD *AR5+,16,B
		data.write_word(0x0f30, 0xff80);
		m_port_writes = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0f30);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100); // SXM
		return resume_atomic_at(0x05e0, 464);
	}
	if (m_phase == 464)
	{
		expect_opcode(0x4595, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff800000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0f31 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 LD *AR5+,16,B sign-extends and postincrements in one cycle");
		program.write_word(0x05e2, 0x4917); // LDM AR7,B
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR7, 0x8001}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(465);
	}
	if (m_phase == 465)
	{
		expect_opcode(0x4917, m_cpu->state_int(tms320c54x_device::STATE_B) == 0x8001 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x8001 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
				"ROM4 LDM AR7,B ignores SXM and preserves AR7 in one cycle");
		write_words(program, {
			{0x05e2, 0x6883}, // ANDM #lk,*AR3
			{0x05e3, 0x0ff0}, {0x05e4, 0x75d6}, {0x05e5, 0x0124}, {0x05e6, 0xf5e1},
		});
		data.write_word(0x0f33, 0xff0f);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_AR3, 0x0f33}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(466);
	}
	if (m_phase == 466)
	{
		expect_opcode(0x6883, data.read_word(0x0f33) == 0x0f00 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f33 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 ANDM #lk,*AR3 preserves pointer and status in two cycles");
		program.write_word(0x05e2, 0x6983); // ORM #lk,*AR3
		program.write_word(0x05e3, 0x00f0);
		m_port_writes = 0;
		return resume_atomic_at(0x05e0, 467);
	}
	if (m_phase == 467)
	{
		expect_opcode(0x6983, data.read_word(0x0f33) == 0x0ff0 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0f33 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
				m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
				"ROM4 ORM #lk,*AR3 preserves pointer and status in two cycles");
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
		return begin_atomic_case(register_bit_examples()[16], 470);
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
		verify_atomic_case(register_bit_examples()[17]);
		return begin_atomic_case(register_bit_examples()[18], 475);
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
		return begin_atomic_case(register_bit_examples()[19], 493);
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
		return begin_atomic_case(atomic_shift_examples().front(), 543);
	}
	if ((m_phase >= 543 && m_phase <= 547) || (m_phase >= 6576 && m_phase <= 6583) || (m_phase >= 9232 && m_phase <= 9246) || m_phase == 9250 || m_phase == 9251)
	{
		// Compatibility routing stays here, outside the independent case data.
		static constexpr unsigned phases[] = {
			543, 544, 545, 546, 547, 6576, 6577, 6578, 6579, 6580, 6581, 6582, 6583,
			9232, 9233, 9234, 9235, 9236, 9237, 9238, 9239, 9240, 9241, 9242, 9243,
			9244, 9245, 9246, 9250, 9251
		};
		auto const &cases = atomic_shift_examples();
		expect(cases.size() == std::size(phases), "Atomic shift case/continuation map is complete");
		unsigned const index = unsigned(std::find(std::begin(phases), std::end(phases), m_phase) - std::begin(phases));
		verify_atomic_case(cases[index]);
		if (index + 1 < cases.size()) return begin_atomic_case(cases[index + 1], phases[index + 1]);
		return begin_atomic_case(square_case(0), 548);
	}
	return false;
}

} // namespace c54x_test
