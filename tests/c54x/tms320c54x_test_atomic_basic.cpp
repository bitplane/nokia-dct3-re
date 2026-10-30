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
	if (m_phase == 9107 || m_phase == 68 || m_phase == 77)
	{
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
		case 68:
			program.write_word(0x0580, 0x0082);
			program.write_word(0x0581, 0xf020); // LD #1234, A
			program.write_word(0x0582, 0x1234);
			program.write_word(0x0583, 0x0082);
			program.write_word(0x0584, 0xf5e1);
			m_repeat_reads = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			return resume_atomic_at(0x0580, 69);
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
		}
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
		return true;
	}
	if (m_phase == 61)
	{
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
		return resume_atomic_at(0x04f0, 62);
	}
	if (m_phase == 62)
	{
		expect(m_repeat_reads == 3 && m_irq_accumulator == 3 &&
				m_last_operand_cycle - m_first_operand_cycle == 4 &&
				m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x04f7 &&
				!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
				"BD takes two cycles and defers IRQ through both delay slots");
		m_cpu->set_input_line(2, CLEAR_LINE);
		return true;
	}
	if (m_phase == 65)
	{
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
		return true;
	}
	if (m_phase == 69)
	{
		expect(m_repeat_reads == 2 && m_last_operand_cycle - m_first_operand_cycle == 3 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1235,
				"long-immediate LD consumes two cycles");
		return true;
	}
	return false;
}

} // namespace c54x_test
