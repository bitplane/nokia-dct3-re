// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_scalars.h"

namespace c54x_test {

// Synthetic basic-operation sequence.
bool tms320c54x_test_state::check_atomic_basic_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	auto prepare_scalar = [&](unsigned index) {
		auto const &test = scalar_vectors().at(index).test;
		if (test.port_writes) m_port_writes = 0;
		prepare_case(*m_cpu, test);
	};
	if ((m_phase >= 45 && m_phase <= 60) || (m_phase >= 67 && m_phase <= 68) ||
		(m_phase >= 70 && m_phase <= 77) || (m_phase >= 9100 && m_phase <= 9107))
	{
		unsigned const index = m_phase >= 9100 ? m_phase - 9100 : m_phase >= 70 ? m_phase - 70 + 26 :
			m_phase >= 67 ? m_phase - 67 + 24 : m_phase - 45 + 8;
		auto const &test = scalar_vectors().at(index).test;
		if (index < 8) check_case(*m_cpu, test, m_port_writes, m_last_port_cycle - m_first_port_cycle);
		else check_case_state(*m_cpu, test);
		if (index < 8 || m_phase == 45 || (m_phase >= 75 && m_phase <= 77))
			expect_opcode(test.opcode, true, "Scalar boundary reference");
		if (profile_is("legacy-atomic")) osd_printf_info("C54x ISA case: PASS id=%s\n", test.id);
		switch (m_phase)
		{
		case 9107:
			m_cpu->set_state_int(tms320c54x_device::STATE_B, 3);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			program.write_word(0x0448, 0xf4e3); // CALA A
			write_words(program, {
				{0x0449, 0xf5e1}, {0x0450, 0x76f8}, {0x0451, 0x0944}, {0x0452, 0xbeef}, {0x0453, 0xfc00},
			});
			set_registers(*m_cpu, {
				{cpu_regs::STATE_PC, 0x0448}, {cpu_regs::STATE_A, 0x0450}, {cpu_regs::STATE_SP, 0x0300},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(42);
		case 60:
			program.write_word(0x04e0, 0xec03); // RPT #3
			program.write_word(0x04e1, 0x0082); // ADD *AR2, A
			program.write_word(0x04e2, 0xf5e1);
			program.write_word(0x0048, 0x0083); // Observe A on ISR entry.
			program.write_word(0x0049, 0xf49b);
			m_repeat_reads = 0;
			m_irq_accumulator = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_AR2, 0x0060}, {cpu_regs::STATE_AR3, 0x0061},
				{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_IMR, 4},
				{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_PC, 0x04e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(61);
		case 68:
			program.write_word(0x0580, 0x0082);
			program.write_word(0x0581, 0xf020); // LD #1234, A
			program.write_word(0x0582, 0x1234);
			program.write_word(0x0583, 0x0082);
			program.write_word(0x0584, 0xf5e1);
			m_repeat_reads = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0580);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(69);
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
		default:
			prepare_scalar(index + 1);
			return next_atomic_phase(m_phase + 1);
		}
	}
	if (m_phase == 5)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE),
				"long-immediate RPT terminal IDLE3");
		expect_opcode(0xf070, m_cpu->state_int(tms320c54x_device::STATE_AR0) == 3,
				"long-immediate RPT iteration count");
		expect_opcode(0xf062, data.read_word(0x090c) == 0x1234 &&
				data.read_word(0x090d) == 0,
				"long-immediate load and long-memory store");
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0360}, {cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_A, 0},
		});
		return next_atomic_phase(6);
	}
	if (m_phase == 6)
	{
		expect(data.read_word(0x0910) == 0x5678,
				"absolute STM extension order");
		expect_opcode(0x7214, m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0xabcd,
				"data-memory to MMR move");
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(0x12345678 ^ ((u64(1) << 40) - 1)),
				"absolute double-word load and accumulator complement");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x00ff0f00,
				"shifted long-immediate accumulator XOR");
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0370}, {cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_AR0, 0},
			{cpu_regs::STATE_B, 0x1234},
		});
		return next_atomic_phase(7);
	}
	if (m_phase == 7)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 0,
				"repeat-with-zero clears its accumulator");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR0) == 2,
				"repeat-with-zero iteration count");
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0374);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
		return next_atomic_phase(8);
	}
	if (m_phase == 8)
	{
		expect(data.read_word(0x091a) == 2,
				"memory-counted multiword repeat iteration count");
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x037a);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0920);
		return next_atomic_phase(9);
	}
	if (m_phase == 9)
	{
		osd_printf_info("TMS320C54x repeated MVDP result: %04x,%04x\n",
				m_cpu->space(AS_PROGRAM).read_word(0x0924),
				m_cpu->space(AS_PROGRAM).read_word(0x0925));
		expect(m_cpu->space(AS_PROGRAM).read_word(0x0924) == 0xaaaa &&
				m_cpu->space(AS_PROGRAM).read_word(0x0925) == 0xbbbb,
				"repeated MVDP advances its program destination");
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0380);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(10);
	}
	if (m_phase == 10)
	{
		expect_opcode(0xf273, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0391,
				"delayed branch executes both delay-slot words");
		data.write_word(0x02ff, 0x0398);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0392}, {cpu_regs::STATE_SP, 0x02ff}, {cpu_regs::STATE_ST1, 0x0800},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(11);
	}
	if (m_phase == 11)
	{
		expect_opcode(0xf4eb, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0399 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
				"interrupt return restores PC/SP and enables interrupts");
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x039a);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(12);
	}
	if (m_phase == 12)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x03a1,
				"delayed accumulator-equal branch");
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03a2);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(13);
	}
	if (m_phase == 13)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0xaaaa00,
				"extended absolute load with positive shift");
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03a6);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x03b0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(14);
	}
	if (m_phase == 14)
	{
		expect_opcode(0xf5e2, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x03b1,
				"accumulator-indirect branch");
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03b2);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 5);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(15);
	}
	if (m_phase == 15)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 4,
				"data-memory subtract from accumulator B");
		data.write_word(0x02ff, 0x03c0);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03b6}, {cpu_regs::STATE_SP, 0x02ff}, {cpu_regs::STATE_B, (u64(1) << 40) - 1},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(16);
	}
	if (m_phase == 16)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x03c1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"conditional return on negative accumulator B");
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03c2}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_A, 0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(17);
	}
	if (m_phase == 17)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x03d1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x02ff &&
				data.read_word(0x02ff) == 0x03c4,
				"conditional call on non-positive accumulator A");
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03d2}, {cpu_regs::STATE_A, 3}, {cpu_regs::STATE_B, 9}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(18);
	}
	if (m_phase == 18)
	{
		expect_opcode(0xf520, m_cpu->state_int(tms320c54x_device::STATE_B) == 6,
				"accumulator subtract with independent destination");
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03d4}, {cpu_regs::STATE_A, 0x0940}, {cpu_regs::STATE_AR2, 0x0920},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(19);
	}
	if (m_phase == 19)
	{
		expect(m_cpu->space(AS_PROGRAM).read_word(0x0940) == 0xaaaa &&
				m_cpu->space(AS_PROGRAM).read_word(0x0941) == 0xbbbb,
				"repeated accumulator-addressed program write");
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03d8}, {cpu_regs::STATE_AR2, 0x0918}, {cpu_regs::STATE_ST1, 2},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(20);
	}
	if (m_phase == 20)
	{
		expect_opcode(0x3292, (m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x1f) == 1,
				"data-memory load into ST1.ASM");
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03da);
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(21);
	}
	if (m_phase == 21)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0301,
				"signed stack-frame adjustment");
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x03e0);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x35);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(22);
	}
	if (m_phase == 22)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1a,
				"arithmetic accumulator shift right");
		program.write_word(0x03e4, 0x07f8); // ADDC *(absolute), B
		program.write_word(0x03e5, 0x0920);
		program.write_word(0x03e6, 0xf5e1);
		data.write_word(0x0920, 0xabcd);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03e4}, {cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(23);
	}
	if (m_phase == 23)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 0xbe02,
				"ADDC absolute operand and carry input");
		expect(!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800),
				"ADDC 32-bit carry output");
		program.write_word(0x03e8, 0x6e8f); // BANZD 03efh, *AR7-
		write_words(program, {
			{0x03e9, 0x03ef}, {0x03ea, 0xe801}, {0x03eb, 0xe902}, {0x03ec, 0xe803}, {0x03ef, 0xf5e1},
		});
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03e8}, {cpu_regs::STATE_AR7, 1}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(24);
	}
	if (m_phase == 24)
	{
		expect_opcode(0xe801, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2,
				"BANZD executes two delay words");
		expect_opcode(0xe902, m_cpu->state_int(tms320c54x_device::STATE_B) == 2,
				"BANZD second delay word loads B");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0,
				"BANZD address-register modification");
		program.write_word(0x03f0, 0x24f8); // MPYU *(absolute), A
		program.write_word(0x03f1, 0x0922);
		program.write_word(0x03f2, 0xf5e1);
		data.write_word(0x0922, 2);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03f0}, {cpu_regs::STATE_T, 0xffff}, {cpu_regs::STATE_A, 0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(25);
	}
	if (m_phase == 25)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1fffe,
				"MPYU unsigned operands");
		program.write_word(0x03f4, 0x31f8); // MPYA *(absolute)
		program.write_word(0x03f5, 0x0924);
		program.write_word(0x03f6, 0xf5e1);
		data.write_word(0x0924, 0xfffe);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03f4}, {cpu_regs::STATE_A, 3U << 16}, {cpu_regs::STATE_B, 0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(26);
	}
	if (m_phase == 26)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) ==
				((u64(1) << 40) - 6), "MPYA signed product");
		expect(m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe,
				"MPYA loads T");
		program.write_word(0x03f8, 0xf944); // CC 0400h, ANEQ
		program.write_word(0x03f9, 0x0400);
		program.write_word(0x03fa, 0xf5e1);
		program.write_word(0x0400, 0xe85a);
		program.write_word(0x0401, 0xfc00);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x03f8}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_A, 1},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(27);
	}
	if (m_phase == 27)
	{
		expect_opcode(0xf944, m_cpu->state_int(tms320c54x_device::STATE_A) == 0x5a,
				"conditional call ANEQ");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"conditional call return stack");
		program.write_word(0x0404, 0x7ef8); // READA *(absolute)
		program.write_word(0x0405, 0x0926);
		program.write_word(0x0406, 0x7ff8); // WRITA *(absolute)
		program.write_word(0x0407, 0x0927);
		program.write_word(0x0408, 0xf5e1);
		program.write_word(0x0925, 0xcafe);
		data.write_word(0x0927, 0xbeef);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0404);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0925);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(28);
	}
	if (m_phase == 28)
	{
		expect(data.read_word(0x0926) == 0xcafe,
				"READA accumulator-addressed data read");
		expect(program.read_word(0x0925) == 0xbeef,
				"WRITA accumulator-addressed data write");
		program.write_word(0x040c, 0xf485); // ABS A, A
		program.write_word(0x040d, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x040c);
		m_cpu->set_state_int(tms320c54x_device::STATE_A,
				(u64(1) << 40) - 7);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(29);
	}
	if (m_phase == 29)
	{
		expect_opcode(0xf485, m_cpu->state_int(tms320c54x_device::STATE_A) == 7,
				"ABS signed accumulator magnitude");
		program.write_word(0x0410, 0x1ef8); // SUBC *(absolute), A
		program.write_word(0x0411, 0x0928);
		program.write_word(0x0412, 0xf5e1);
		data.write_word(0x0928, 1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0410}, {cpu_regs::STATE_A, 0x10000}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(30);
	}
	if (m_phase == 30)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10001,
				"SUBC conditional subtract and quotient bit");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800,
				"SUBC successful subtraction carry");
		program.write_word(0x0414, 0x1092); // LD *AR2+, A
		program.write_word(0x0415, 0xf5e1);
		data.write_word(0x0930, 0x1234);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0414}, {cpu_regs::STATE_AR2, 0x0930}, {cpu_regs::STATE_ST0, 0xa000},
			{cpu_regs::STATE_ST1, 0x0000}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(31);
	}
	if (m_phase == 31)
	{
		expect((m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe000) == 0xa000,
				"standard-mode indirect operand preserves ST0.ARP");
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0414}, {cpu_regs::STATE_AR2, 0x0930}, {cpu_regs::STATE_ST0, 0xa000},
			{cpu_regs::STATE_ST1, 0x0020}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(32);
	}
	if (m_phase == 32)
	{
		expect((m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0xe000) == 0x4000,
				"compatibility-mode indirect operand updates ST0.ARP");
		program.write_word(0x0418, 0xf0b0); // OR A, -16, A
		program.write_word(0x0419, 0xf5e1);
		constexpr u64 negative = (u64(0xff) << 32) | 0x80000000U;
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0418);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, negative);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(33);
	}
	if (m_phase == 33)
	{
		constexpr u64 negative = (u64(0xff) << 32) | 0x80000000U;
		osd_printf_info("TMS320C54x logical shift: actual=%010llx expected=%010llx\n",
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
				(unsigned long long)((negative | (negative >> 16)) & ((u64(1) << 40) - 1)));
		expect_opcode(0xf0b0, m_cpu->state_int(tms320c54x_device::STATE_A) ==
				((negative | (negative >> 16)) & ((u64(1) << 40) - 1)),
				"logical accumulator right shift zero-fills guard bits");
		program.write_word(0x041c, 0xed18); // LD #-8, ASM
		program.write_word(0x041d, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x041c);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0xa5a5);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(34);
	}
	if (m_phase == 34)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0xa1b8,
				"short-immediate load into ST1.ASM");
		program.write_word(0x0420, 0x771a); // STM #1, BRC
		program.write_word(0x0421, 0x0001);
		program.write_word(0x0422, 0xf272); // RPTBD 0428h
		program.write_word(0x0423, 0x0428);
		program.write_word(0x0424, 0xe801); // delay slot 1
		program.write_word(0x0425, 0xe902); // delay slot 2
		program.write_word(0x0426, 0x6d90); // MAR *AR0+ (I=1).
		program.write_word(0x0427, 0xf495);
		program.write_word(0x0428, 0xf495);
		program.write_word(0x0429, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0420}, {cpu_regs::STATE_AR0, 0}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0); // Standard addressing: MAR names physical AR0.
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(35);
	}
	if (m_phase == 35)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2,
				"RPTBD executes both delay slots once");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR0) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000),
				"RPTBD repeats its body and retires BRAF");
		program.write_word(0x042c, 0xa43a); // MPY *AR5, *AR4+, A
		program.write_word(0x042d, 0xf5e1);
		data.write_word(0x0940, 3);
		data.write_word(0x0950, 0xfffe);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x042c}, {cpu_regs::STATE_AR4, 0x0940}, {cpu_regs::STATE_AR5, 0x0950},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(36);
	}
	if (m_phase == 36)
	{
		expect_opcode(0xa43a, m_cpu->state_int(tms320c54x_device::STATE_A) ==
				((u64(1) << 40) - 6) &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffe,
				"dual-memory multiply result and T load");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0941 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0950,
				"dual-memory multiply address updates");
		program.write_word(0x0430, 0xb336); // MAC *AR5, *AR4-, B, B
		program.write_word(0x0431, 0xf5e1);
		data.write_word(0x0941, 4);
		data.write_word(0x0950, 0xfffd);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0430}, {cpu_regs::STATE_AR4, 0x0941}, {cpu_regs::STATE_AR5, 0x0950},
			{cpu_regs::STATE_B, 20}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(37);
	}
	if (m_phase == 37)
	{
		expect_opcode(0xb336, m_cpu->state_int(tms320c54x_device::STATE_B) == 8 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xfffd,
				"dual-memory signed multiply-accumulate");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x0940 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0950,
				"dual-memory MAC address updates");
		program.write_word(0x0434, 0xd631); // ST B,*AR3 || MACR *AR5,A
		program.write_word(0x0435, 0xf5e1);
		data.write_word(0x0950, 3);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0434}, {cpu_regs::STATE_AR3, 0x0960}, {cpu_regs::STATE_AR5, 0x0950},
			{cpu_regs::STATE_A, 0x10001}, {cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_T, 2},
			{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(38);
	}
	if (m_phase == 38)
	{
		expect(data.read_word(0x0960) == 0x1234,
				"parallel store uses the pre-accumulate source");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x10000,
				"parallel rounded multiply-accumulate");
		program.write_word(0x0438, 0xe210); // SQDST *AR3,*AR2
		program.write_word(0x0439, 0xf5e1);
		data.write_word(0x0960, 5);
		data.write_word(0x0970, 8);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0438);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0970);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0960);
		m_cpu->set_state_int(tms320c54x_device::STATE_A,
				(u64(0xff) << 32) | (u64(0xfffe) << 16));
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 10);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(39);
	}
	if (m_phase == 39)
	{
		expect_opcode(0xe210, m_cpu->state_int(tms320c54x_device::STATE_A) ==
				((u64(1) << 40) - 0x30000),
				"square-distance signed vector difference");
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 14,
				"square-distance accumulation of old A high half");
		program.write_word(0x043c, 0xfa44); // BCD 0442h, ANEQ
		write_words(program, {
			{0x043d, 0x0442}, {0x043e, 0xe802}, {0x043f, 0xe903}, {0x0440, 0xf5e1}, {0x0442, 0xf5e1},
		});
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x043c}, {cpu_regs::STATE_A, 1}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(40);
	}
	if (m_phase == 40)
	{
		expect_opcode(0xfa44, m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0443 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 3,
				"delayed accumulator-not-equal branch and delay slots");
		expect_opcode(0xe903, m_cpu->state_int(tms320c54x_device::STATE_B) == 3,
				"conditional branch second delay word loads B");
		program.write_word(0x0444, 0xf484); // NEG A
		program.write_word(0x0445, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0444}, {cpu_regs::STATE_A, 5}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(41);
	}
	if (m_phase == 41)
	{
		expect_opcode(0xf484, m_cpu->state_int(tms320c54x_device::STATE_A) ==
				((u64(1) << 40) - 5) &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800),
				"accumulator negate result and carry");
		prepare_scalar(0);
		return next_atomic_phase(9100);
	}
	if (m_phase == 42)
	{
		expect(data.read_word(0x0944) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x044a &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"CALA A target and return address");
		program.write_word(0x0460, 0xf5e3); // CALA B
		write_words(program, {
			{0x0461, 0xf5e1}, {0x0468, 0x76f8}, {0x0469, 0x0945}, {0x046a, 0xcafe}, {0x046b, 0xfc00},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0460);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0468);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(43);
	}
	if (m_phase == 43)
	{
		expect(data.read_word(0x0945) == 0xcafe &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0462 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"CALA B target and return address");
		program.write_word(0x0470, 0xf6e3); // CALAD A
		write_words(program, {
			{0x0471, 0xe801}, {0x0472, 0xe902}, {0x0473, 0xf5e1}, {0x0478, 0x76f8}, {0x0479, 0x0946}, {0x047a, 0x1234},
			{0x047b, 0xfc00},
		});
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0470);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0478);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(44);
	}
	if (m_phase == 44)
	{
		expect(data.read_word(0x0946) == 0x1234 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0474 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 2,
				"CALAD delay slots and return address");
		prepare_scalar(8);
		return next_atomic_phase(45);
	}
	if (m_phase == 61)
	{
		osd_printf_info("repeat IRQ: reads=%u isr_a=%llx a=%llx pc=%04x idle=%u\n",
				m_repeat_reads, (unsigned long long)m_irq_accumulator,
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)));
		expect_opcode(0xec03, m_repeat_reads == 4 && m_irq_accumulator == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE),
				"pending interrupt defers until the complete single-repeat body retires");
		expect(m_last_operand_cycle - m_first_operand_cycle == 3,
				"repeated ADD consumes one cycle per body iteration");
		osd_printf_info("TMS320C54x interrupt boundary observation: "
				"repeat_reads=%u body_cycles=%llu final_read_to_vector_read=%llu "
				"pipeline_fidelity_claim=0\n", m_repeat_reads,
				(unsigned long long)(m_last_operand_cycle - m_first_operand_cycle),
				(unsigned long long)(m_irq_operand_cycle - m_last_operand_cycle));
		m_cpu->set_input_line(2, CLEAR_LINE);
		program.write_word(0x04f0, 0x0082); // Cycle marker before BD.
		program.write_word(0x04f1, 0xf273);
		program.write_word(0x04f2, 0x04f6);
		program.write_word(0x04f3, 0x0082); // IRQ raised in first delay slot.
		program.write_word(0x04f4, 0x0082);
		program.write_word(0x04f5, 0xffff); // Must not execute.
		program.write_word(0x04f6, 0xf5e1);
		m_repeat_reads = 0;
		m_irq_trigger_read = 2;
		m_irq_accumulator = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04f0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(62);
	}
	if (m_phase == 62)
	{
		expect(m_repeat_reads == 3 && m_irq_accumulator == 3 &&
				m_last_operand_cycle - m_first_operand_cycle == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x04f7 &&
				!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
				"BD takes two cycles and defers IRQ through both delay slots");
		m_cpu->set_input_line(2, CLEAR_LINE);
		program.write_word(0x0500, 0x7726); // STM #TSS, TCR
		program.write_word(0x0501, 0x0010);
		program.write_word(0x0502, 0xf070); // RPT #65535
		program.write_word(0x0503, 0xffff);
		program.write_word(0x0504, 0x0082);
		program.write_word(0x0505, 0xf5e1);
		m_repeat_reads = 0;
		m_irq_trigger_read = 1;
		m_irq_accumulator = 0;
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0500);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(63);
	}
	if (m_phase == 63)
	{
		expect(m_repeat_reads > 0 && m_repeat_reads < 65536 &&
				(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 4),
				"save checkpoint has active repeat and pending IRQ");
		m_active_repeat.reads = m_repeat_reads;
		expect(capture_snapshot(machine(), m_active_repeat.saved) == STATERR_NONE,
				"write active-repeat save state");
		m_phase = 64;
		m_check_timer->adjust(attotime::from_msec(6));
		return true;
	}
	if (m_phase == 64 || m_phase == 65)
	{
		osd_printf_info("repeat state phase=%u reads=%u isr=%llu a=%llu pc=%04x\n", m_phase,
				m_repeat_reads, (unsigned long long)m_irq_accumulator,
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
		expect(m_repeat_reads == 65536 && m_irq_accumulator == 65536 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 65536 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0506,
				"active-repeat save replay completes identically and services pending IRQ");
		if (m_phase == 64)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_active_repeat.saved) == STATERR_NONE,
					"restore active-repeat save state");
			m_repeat_reads = m_active_repeat.reads;
			m_irq_accumulator = 0;
			m_phase = 65;
			m_check_timer->adjust(attotime::from_msec(6));
			return true;
		}
		osd_printf_info("TMS320C54x active repeat restore: PASS iterations=65536 pending_irq=1 save_replay=1 live_and_replay=2 transport=0\n");
		m_cpu->set_input_line(2, CLEAR_LINE);
		program.write_word(0x0520, 0x0082);
		program.write_word(0x0521, 0xf274); // CALLD 0540
		write_words(program, {
			{0x0522, 0x0540}, {0x0523, 0xf495}, {0x0524, 0xf495}, {0x0525, 0x0082}, {0x0526, 0xf5e1}, {0x0540, 0x0082},
		});
		program.write_word(0x0541, 0xfe00); // RETD
		program.write_word(0x0542, 0xf495);
		program.write_word(0x0543, 0xf495);
		m_repeat_reads = 0;
		m_irq_trigger_read = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_SP, 0x0300}, {cpu_regs::STATE_PC, 0x0520},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(66);
	}
	if (m_phase == 66)
	{
		expect_opcode(0xf274, m_repeat_reads == 3 && m_last_operand_cycle - m_first_operand_cycle == 11 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0527 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300,
				"CALLD two-cycle and RETD three-cycle timing with balanced delayed return");
		prepare_scalar(24);
		return next_atomic_phase(67);
	}
	if (m_phase == 69)
	{
		expect(m_repeat_reads == 2 && m_last_operand_cycle - m_first_operand_cycle == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1235,
				"long-immediate LD consumes two cycles");
		prepare_scalar(26);
		return next_atomic_phase(70);
	}
	return false;
}

} // namespace c54x_test
