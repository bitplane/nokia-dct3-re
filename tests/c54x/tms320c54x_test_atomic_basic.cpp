// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

// Synthetic basic-operation sequence.
bool tms320c54x_test_state::check_atomic_basic_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
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
		program.write_word(0x05e0, 0x75f8);
		program.write_word(0x05e1, 0x0d00);
		program.write_word(0x05e2, 0x0124);
		program.write_word(0x05e3, 0xf684); // NEG B,A.
		program.write_word(0x05e4, 0x75f8);
		program.write_word(0x05e5, 0x0d00);
		program.write_word(0x05e6, 0x0124);
		program.write_word(0x05e7, 0xf5e1);
		m_port_writes = 0;
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_B, 0xfffffffffeULL}, {cpu_regs::STATE_ST0, 0x1800},
			{cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(9100);
	}
	if (m_phase >= 9100 && m_phase <= 9107)
	{
		const unsigned index = m_phase - 9100;
		const bool b = BIT(index, 0);
		const u64 source = index < 2 ? 0xfffffffffeULL : index < 4 ? 0 : 0xff80000000ULL;
		const u64 result = index < 2 ? 2 : index < 4 ? 0 : index < 6 ? 0x80000000 : 0x7fffffff;
		const u16 st0 = index < 2 ? 0x1000 : index < 4 ? 0x1800 : b ? 0x1200 : 0x1400;
		const u16 st1 = index < 6 ? 0x0100 : 0x0300;
		expect_opcode(b ? 0xf784 : 0xf684,
			m_cpu->state_int(tms320c54x_device::STATE_A) == (b ? 0x12345678 : result) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (b ? result : source) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == st1 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e8 &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 4,
			"ROM4 NEG B selects A/B destination, preserves unrelated accumulator/TC, sets carry only for zero and obeys destination overflow/OVM in one cycle");
		if (index < 7)
		{
			const unsigned next = index + 1;
			program.write_word(0x05e3, BIT(next, 0) ? 0xf784 : 0xf684);
			m_port_writes = 0;
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x12345678},
				{cpu_regs::STATE_B, next < 2 ? 0xfffffffffeULL : next < 4 ? 0 : 0xff80000000ULL},
				{cpu_regs::STATE_ST0, 0x1800}, {cpu_regs::STATE_ST1, next < 6 ? 0x0100 : 0x0300},
				{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
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
		program.write_word(0x0480, 0xf120); // LD #ffff, B
		program.write_word(0x0481, 0xffff);
		program.write_word(0x0482, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PC, 0x0480}, {cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(45);
	}
	if (m_phase >= 45 && m_phase <= 48)
	{
		static constexpr u64 expected[] = {
			0x000000ffff, 0xffffff8000, 0xff80000000, 0x00ffff0000
		};
		osd_printf_info("TMS320C54x immediate B load: phase=%u pc=%04x a=%010llx b=%010llx expected=%010llx\n",
				m_phase, unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				m_cpu->state_int(tms320c54x_device::STATE_A),
				m_cpu->state_int(tms320c54x_device::STATE_B), expected[m_phase - 45]);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0483 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == expected[m_phase - 45],
				"long-immediate B load destination, SXM and guard extension");
		if (m_phase == 45)
			expect_opcode(0xf120, m_cpu->state_int(tms320c54x_device::STATE_B) == 0xffff,
					"long-immediate LD B preserves unsigned value with SXM clear");
		if (m_phase != 48)
		{
			++m_phase;
			program.write_word(0x0480, m_phase == 46 ? 0xf120 : 0xf162);
			program.write_word(0x0481, m_phase == 48 ? 0xffff : 0x8000);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0480);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, m_phase == 48 ? 0 : 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x0480, 0xf02f); // LD #8000, 15, A
		program.write_word(0x0481, 0x8000);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0480);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(49);
	}
	if (m_phase == 49 || m_phase == 50)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 49 ? 0x0040000000ULL : 0xffc0000000ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x00ffff0000,
				"shifted immediate A load respects SXM and preserves B");
		if (m_phase == 49)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0480);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(50);
		}
		program.write_word(0x0490, 0xf585); // ABS A, B
		program.write_word(0x0491, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0312345678ULL}, {cpu_regs::STATE_ST1, 0x0200}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x0490}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(51);
	}
	if (m_phase == 51 || m_phase == 52)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(m_phase == 51 ? 0x007fffffffULL : 0) &&
				bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
				(m_phase == 52), "ABS saturation and zero carry");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0200,
				"ABS guard-bit overflow is sticky across a zero result");
		if (m_phase == 51)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0490);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(52);
		}
		program.write_word(0x04a0, 0xf000); // ADD #1, A
		program.write_word(0x04a1, 1);
		program.write_word(0x04a2, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x007fffffffULL}, {cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_ST1, 0x0200},
			{cpu_regs::STATE_PC, 0x04a0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(53);
	}
	if (m_phase == 53 || m_phase == 54)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 53 ? 0x007fffffffULL : 0xff80000000ULL),
				"immediate ADD/SUB signed saturation");
		expect((m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) ==
				(m_phase == 53 ? 0x0400 : 0x0c00),
				"immediate ADD/SUB overflow and bit-32 carry");
		if (m_phase == 53)
		{
			program.write_word(0x04a0, 0xf010); // SUB #1, A
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff80000000ULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04a0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(54);
		}
		program.write_word(0x04b0, 0xf47f); // SFTA A, -1
		program.write_word(0x04b1, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffffffffffULL}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x04b0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(55);
	}
	if (m_phase == 55 || m_phase == 56)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 55 ? 0x7fffffffffULL : 0xffffffffffULL) &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800),
				"SFTA right shift respects SXM and copies outgoing bit to carry");
		if (m_phase == 55)
		{
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0xffffffffffULL}, {cpu_regs::STATE_ST1, 0x0100}, {cpu_regs::STATE_PC, 0x04b0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(56);
		}
		program.write_word(0x04c0, 0x00f8); // ADD *abs, A
		program.write_word(0x04c1, 0x0a80);
		program.write_word(0x04c2, 0xf5e1);
		data.write_word(0x0a80, 1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xffffffffffULL}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_PC, 0x04c0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(57);
	}
	if (m_phase == 57 || m_phase == 58)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 57 ? 0 : 0xffffffffffULL) &&
				bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) ==
				(m_phase == 57), "memory ADD carry and SUB borrow at bit 32");
		if (m_phase == 57)
		{
			program.write_word(0x04c0, 0x08f8); // SUB *abs, A
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04c0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(58);
		}
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0800);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200);
		program.write_word(0x04d0, 0xf02f);
		program.write_word(0x04d1, 0xffff);
		program.write_word(0x04d2, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x04d0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(59);
	}
	if (m_phase == 59)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x007fff8000ULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800),
				"shifted unsigned immediate load preserves carry");
		program.write_word(0x04d0, 0xf482); // LD A, ASM, A
		program.write_word(0x04d1, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x0040000000ULL}, {cpu_regs::STATE_ST1, 0x0202}, {cpu_regs::STATE_PC, 0x04d0},
			{cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(60);
	}
	if (m_phase == 60)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x007fffffffULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0c00,
				"ASM shifted load saturates and preserves carry");
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
		m_transport_repeat.reads = m_repeat_reads;
		static constexpr u8 payload[] = { 0x12, 0x34, 0x56, 0x78 };
		m_transport->peer_shared_w(0x1c8 / 2, 0x100 / 2);
		m_transport->peer_shared_w(0x1ca / 2, 0x100 / 2);
		expect(m_transport->enqueue_rx_packet(0x83, payload, sizeof(payload)) &&
				m_transport->enqueue_rx_packet(0x89, payload, sizeof(payload)),
				"queue transport packets before save");
		m_transport->dspif_w(0, 0x5a);
		for (unsigned i = 0; i != m_transport_repeat.transport.size(); ++i)
			m_transport_repeat.transport[i] = m_transport->shared_word(i);
		m_transport_repeat.saved.str(std::string());
		expect(machine().save().write_stream(m_transport_repeat.saved) == STATERR_NONE,
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
			for (unsigned i = 0; i != m_transport_repeat.transport.size(); ++i)
				m_transport->peer_shared_w(i, 0);
			m_transport->dspif_w(0, 0);
			m_transport_repeat.saved.clear();
			m_transport_repeat.saved.seekg(0);
			expect(machine().save().read_stream(m_transport_repeat.saved) == STATERR_NONE,
					"restore active-repeat save state");
			for (unsigned i = 0; i != m_transport_repeat.transport.size(); ++i)
				expect(m_transport->shared_word(i) == m_transport_repeat.transport[i],
						"restore queued DSPIF payloads and ring cursors");
			expect(m_transport->dspif_r(0) == 0x5a,
					"restore DSPIF interface registers");
			m_repeat_reads = m_transport_repeat.reads;
			m_irq_accumulator = 0;
			m_phase = 65;
			m_check_timer->adjust(attotime::from_msec(6));
			return true;
		}
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
		program.write_word(0x0560, 0xf484);
		program.write_word(0x0561, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0x8000000000ULL}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_PC, 0x0560}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(67);
	}
	if (m_phase == 67 || m_phase == 68)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(m_phase == 67 ? 0x8000000000ULL : 0x007fffffffULL) &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0c00) == 0x0400,
				"NEG 40-bit minimum sets overflow and obeys OVM");
		if (m_phase == 67)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0200);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0560);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			return next_atomic_phase(68);
		}
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
	}
	if (m_phase == 69)
	{
		expect(m_repeat_reads == 2 && m_last_operand_cycle - m_first_operand_cycle == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1235,
				"long-immediate LD consumes two cycles");
		program.write_word(0x05a0, 0xf0ff); // SFTL A, -1
		program.write_word(0x05a1, 0xf5e1);
		m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05a0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		return next_atomic_phase(70);
	}
	if (m_phase >= 70 && m_phase <= 72)
	{
		const u64 expected = m_phase == 70 ? 0x007fffffffULL :
				m_phase == 71 ? 0x00fffffffeULL : 0x00ffffffffULL;
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected &&
				bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == (m_phase != 72),
				"SFTL uses low 32 bits clears guards and publishes carry");
		if (m_phase != 72)
		{
			program.write_word(0x05a0, m_phase == 70 ? 0xf0e1 : 0xf0e0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xffffffffffULL);
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x05a0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		program.write_word(0x05c0, 0xf765); // SFTA B, +5, B
		program.write_word(0x05c1, 0xf5e1);
		set_registers(*m_cpu, {
			{cpu_regs::STATE_B, 0x80aa001234ULL}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0},
			{cpu_regs::STATE_PC, 0x05c0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(73);
	}
	if (m_phase == 73 || m_phase == 74)
	{
		osd_printf_info("SFTA left phase=%u b=%010llx st0=%04x\n", m_phase,
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)));
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(m_phase == 73 ? 0x1540024680ULL : 0xff80000000ULL) &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0a00) == 0x0200,
				"SFTA guard-bit left shift carry overflow and negative saturation");
		if (m_phase == 73)
		{
			set_registers(*m_cpu, {
				{cpu_regs::STATE_B, 0x80aa001234ULL}, {cpu_regs::STATE_ST1, 0x0200}, {cpu_regs::STATE_PC, 0x05c0},
				{cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(74);
		}
		program.write_word(0x05e0, 0xf491); // ROL A
		program.write_word(0x05e1, 0xf5e1); // IDLE
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff80000000ULL}, {cpu_regs::STATE_B, 0x0012345678ULL}, {cpu_regs::STATE_ST0, 0},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(75);
	}
	if (m_phase == 75 || m_phase == 76)
	{
		const bool first = m_phase == 75;
		expect_opcode(first ? 0xf491 : 0xf591, m_cpu->state_int(tms320c54x_device::STATE_A) ==
				(first ? 0 : 0x0012345678ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) ==
				(first ? 0x0012345678ULL : 0x002468acf1ULL) &&
				bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) == first,
				"ROL rotates through carry, clears guard bits and preserves the other accumulator");
		if (first)
		{
			program.write_word(0x05e0, 0xf591); // ROL B
			set_registers(*m_cpu, {
				{cpu_regs::STATE_A, 0x0012345678ULL}, {cpu_regs::STATE_B, 0x0012345678ULL},
				{cpu_regs::STATE_ST0, 0x0800}, {cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
			});
			return next_atomic_phase(76);
		}
		program.write_word(0x05e0, 0xf050); // XOR #lk, 0, A, A
		program.write_word(0x05e1, 0x00ff);
		program.write_word(0x05e2, 0xf5e1); // IDLE
		set_registers(*m_cpu, {
			{cpu_regs::STATE_A, 0xff12345678ULL}, {cpu_regs::STATE_B, 0x0012345678ULL}, {cpu_regs::STATE_ST0, 0x0800},
			{cpu_regs::STATE_PC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
		});
		return next_atomic_phase(77);
	}
	if (m_phase == 77)
	{
		expect_opcode(0xf050, m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12345687ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x0012345678ULL &&
				(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0800) &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x05e3,
				"XOR long immediate consumes extension and preserves carry, guards and B");
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
	}
	return false;
}

} // namespace c54x_test
