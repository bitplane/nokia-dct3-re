// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_repeat_endpoint_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	u16 const body = index % 3 == 0 ? 0x0802 : index % 3 == 1 ? 0xfffd : 0xfffe;
	bool const long_count = (index / 3) & 1, nmi = index >= 6;
	u16 const start = body - (long_count ? 2 : 1);
	program.write_word(0x010000 | start, long_count ? 0xf070 : 0xec02);
	if (long_count) program.write_word(0x010000 | u16(start + 1), 2);
	program.write_word(0x010000 | body, 0x7312); // MVMD AR2,dmad
	program.write_word(0x010000 | u16(body + 1), 0x0600);
	program.write_word(0x010000 | u16(body + 2), 0xf4e1);
	program.write_word(nmi ? 0x010004 : 0x010040, 0xf4e1); // Stop at accepted vector.
	for (unsigned i = 0; i < 4; ++i) data.write_word(0x0600 + i, 0xdead);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(STATE_GENPC, 0x010000 | start);
	m_repeat_boundary_requests = 0;
	m_repeat_boundary_tap.remove();
	m_repeat_boundary_tap = data.install_write_tap(0x0601, 0x0601, "repeat_final_request",
		[this, nmi](offs_t, u16 &, u16) {
			if (!machine().side_effects_disabled() && !m_repeat_boundary_requests++)
				m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		});
	m_phase = 83000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_native_final_gap_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	static constexpr u16 opcodes[] = {0x9a5f, 0xf20f, 0xf47d, 0xf50f, 0xf736, 0xf761};
	unsigned const family = index / 2;
	bool const variant = index & 1;
	program.write_word(0x0800, opcodes[family]);
	program.write_word(0x0801, family == 1 ? 1 : 0xf4e1);
	program.write_word(0x0802, 0xf4e1);
	u64 const a = family == 0 || family == 3 ? (variant ? 0xfffffffffeULL : 2) :
		family == 2 ? 0xfffffffff7ULL : 3;
	u64 const b = family == 1 ? (variant ? 0xfffffffff0ULL : 5) :
		family == 4 ? (variant ? 0xfffffffc00ULL : 0x400) : family == 5 ? 0x40000000 : 5;
	m_cpu->set_state_int(tms320c54x_device::STATE_A, a);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, b);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0600);
	m_cpu->space(AS_DATA).write_word(0x0600, 0xdead);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
		family == 2 ? (variant ? 0 : 0x0100) : family == 4 ? 0x0100 : family == 5 && variant ? 0x0200 : 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_phase = 150000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_native_math_gap_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	static constexpr u16 opcodes[] = {0xf67b, 0x3185, 0xf301, 0xf648};
	unsigned const family = index / 2;
	bool const variant = index & 1;
	program.write_word(0x0800, opcodes[family]);
	program.write_word(0x0801, family == 2 ? 0x4000 : 0xf4e1);
	program.write_word(0x0802, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0600, 0xfffd);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0600);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, family == 1 ? 0x20000 : 0x1234);
	u64 const b = family == 0 ? 0xffffffffdfULL : family == 1 ? 0x1234 :
		family == 2 ? (variant ? 0xfffffffff0ULL : 5) : (variant ? 0xfffffffffeULL : 0x123456);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, b);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, family == 1 ? 0x0800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
		family == 0 ? (variant ? 0x0100 : 0) : family == 1 ? (variant ? 0x0040 : 0) : 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_phase = 90000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_native_gap_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	unsigned const family = index / 2;
	program.write_word(0x0800, family == 0 ? 0x6f95 : family == 1 ? 0x1087 : 0x1182);
	program.write_word(0x0801, family == 0 ? 0x0e20 : 0xf4e1);
	program.write_word(0x0802, 0xf4e1);
	data.write_word(0x0600, 0x8001);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 3);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 5);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0600);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x0600);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x0600);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, (index & 1) ? 0x0100 : 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_phase = 88000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_cala_rtn_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, index ? 0xf5e3 : 0xf4e3); // CALA A/B
	program.write_word(0x0801, 0xf4e1);
	program.write_word(0x0900, 0xf4e1); // Inspect the paused callee before RETF.
	program.write_word(0x0901, 0xf49b);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0900);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x0900);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0400);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_phase = 87000 + index * 2;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_operand_fetch_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	unsigned const family = index >= 45 ? 10 : index >= 25 ? 6 + (index - 25) / 4 : index / 4;
	unsigned const form = index == 45 ? 4 : index >= 46 ? 3 : index >= 25 ? (index - 25) % 4 : index % 4;
	static constexpr u16 families[] = { 0x7100, 0x7d00, 0x7500, 0x7400, 0x7e00, 0x7000, 0x6000, 0x6100, 0x8b00, 0x7f00, 0x7c00 };
	static constexpr u8 modes[] = { 0x00, 0x92, 0xe2, 0xf8, 0x9a };
	static constexpr u16 operands[] = { 0x0700, 0x0700, 0x0124, 0x0123, 0, 0x0700, 0xbeef, 0x8000 };
	u16 pc = 0x0800;
	if (index == 24)
	{
		program.write_word(pc++, 0xe001); // FIRS *AR2,*AR3,0900
		program.write_word(pc++, 0x0900);
	}
	else
	{
		program.write_word(pc++, families[family] | (index >= 46 ? 0xf9 + index - 46 : modes[form]));
		if (form == 2)
		{
			if (family == 10)
			{
				program.write_word(pc++, 0x0900);
				program.write_word(pc++, 5);
			}
			else if (family == 0 || family == 4 || family == 5 || family >= 6)
			{
				program.write_word(pc++, 5);
				if (family != 4 && family < 8) program.write_word(pc++, operands[family]);
			}
			else
			{
				program.write_word(pc++, operands[family]);
				program.write_word(pc++, 5);
			}
		}
		else
		{
			if (form == 3) program.write_word(pc++, 0x0600);
			if (family == 10) program.write_word(pc++, 0x0900);
			if (family != 4 && family < 8) program.write_word(pc++, operands[family]);
		}
	}
	m_operand_extension_count = pc - 0x0801;
	program.write_word(pc, 0xf4e1);
	program.write_word(0x0900, family == 9 ? 0xdead : 0xbeef);
	program.write_word(0x0700, 0);
	data.write_word(0x0600, family == 8 || family == 10 ? 0xdead : 0xbeef); data.write_word(0x0601, 0xcafe);
	data.write_word(0x05ff, 0xcafe);
	data.write_word(0x0700, family == 5 || family == 8 ? 0xbeef : 0);
	if (family == 8) m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0700);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, form == 4 ? 0x05ff : form == 2 ? 0x05fb : 0x0600);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0601);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, index == 24 ? 0 : 0x0900);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x000c);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0800);
	m_port_reads = m_port_writes = 0;
	m_operand_extensions = m_operand_reads = 0;
	m_operand_order_violation = false;
	m_operand_fetch_tap = program.install_read_tap(0x0801, 0x0802, "operand_extension_fetch",
		[this](offs_t address, u16 &, u16) {
			if (!machine().side_effects_disabled() && address - 0x0801 < m_operand_extension_count)
				m_operand_extensions |= 1U << (address - 0x0801);
		});
	auto observe = [this](offs_t, u16 &, u16) {
		if (!machine().side_effects_disabled())
		{
			++m_operand_reads;
			m_operand_order_violation |= m_operand_extensions != ((1U << m_operand_extension_count) - 1);
		}
	};
	m_operand_data_tap = data.install_read_tap(0x0600, 0x0700, "operand_data_read", observe);
	m_operand_io_tap = m_cpu->space(AS_IO).install_read_tap(0x0123, 0x0123, "operand_io_read", observe);
	m_operand_program_tap = program.install_read_tap(0x0900, 0x0900, "operand_program_read", observe);
	m_phase = 86000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_loop_xc_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	bool const long_body = index & 1, nmi = index >= 2;
	program.write_word(0x0800, 0xf072); // RPTB end
	program.write_word(0x0801, long_body ? 0x0805 : 0x0804);
	program.write_word(0x0802, 0xe809); // LD #9,A: the next loop's first word.
	program.write_word(0x0803, long_body ? 0xff4d : 0xfd4d); // XC 2/1,BEQ
	program.write_word(0x0804, long_body ? 0xf020 : 0xe801); // LD #1,A
	if (long_body) program.write_word(0x0805, 1);
	program.write_word(long_body ? 0x0806 : 0x0805, 0xf4e1);
	u16 const vector = nmi ? 0x0004 : 0x0048;
	program.write_word(vector, 0x0083); // Passive ISR observation through *AR3.
	program.write_word(vector + 1, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x0061);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 4);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0800);
	m_irq_accumulator = 0xdead;
	m_xc_irq_fetches = 0;
	m_xc_irq_tap.remove();
	m_xc_irq_tap = program.install_read_tap(0x0804, 0x0804, "xc_loop_pending_irq",
		[this, nmi](offs_t, u16 &, u16) {
			if (!machine().side_effects_disabled() && !m_xc_irq_fetches++)
				m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		});
	m_phase = 85000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_nested_repeat_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	bool const long_count = index >= 2;
	u16 const count = index & 1 ? 2 : 0;
	u16 const body = long_count ? 0x0804 : 0x0803;
	program.write_word(0x0800, 0xf072); // RPTB body
	program.write_word(0x0801, body);
	program.write_word(0x0802, long_count ? 0xf070 : 0xec00 | count);
	if (long_count) program.write_word(0x0803, count);
	program.write_word(body, 0x6d91); // MAR *AR1+
	program.write_word(body + 1, 0xf4e1); // IDLE1
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0800);
	m_phase = 84000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_braf_control_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	if (index < 3)
	{
		program.write_word(0x0800, 0xf072); // RPTB end
		program.write_word(0x0801, index == 1 ? 0x0804 : 0x0803);
		program.write_word(0x0802, 0x6d91); // MAR *AR1+
		program.write_word(0x0803, index == 0 ? 0xf6bf : index == 1 ? 0x7707 : 0x8a07);
		program.write_word(0x0804, index == 1 ? 0 : 0xf4e1);
		program.write_word(0x0805, 0xf4e1);
		for (unsigned i = 0; i < 4; ++i) data.write_word(0x1000 + i, 0);
	}
	else
	{
		// Restore the architectural loop registers and BRAF without RPTB.
		data.write_word(0x001b, 0x0802); data.write_word(0x001c, 0x0803);
		program.write_word(0x0800, 0x7707); program.write_word(0x0801, 0x8000);
		program.write_word(0x0802, 0x6d91); program.write_word(0x0803, 0xf495);
		program.write_word(0x0804, 0xf4e1);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0800);
	m_phase = 82000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_pipeline_frontend_case(bool memory, bool memory_b)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_memory_case = memory;
	m_pipeline_memory_b = memory_b;
	m_pipeline_operand_reads = 0;
	m_vector_prefetch_tap.remove(); m_vector_stack_tap.remove();
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i)
	{
		program.write_word(0x600 + i, 0xf495);
		program.write_word(0x40 + i, 0xf495);
	}
	program.write_word(0x600, 0xe811); // Decoded before INTR, retires at cycle 6.
	if (memory) program.write_word(0x600, memory_b ? 0x1120 : 0x1020);
	program.write_word(0x601, 0xe822); // Fetched, not decoded; must not execute.
	program.write_word(0x602, 0xe833); // Prefetch already in flight; Fetch must drain.
	program.write_word(0x40, 0xe955); // One-word first ISR instruction, Execute at 10.
	data.write_word(0xfff, 0xbeef);
	data.write_word(0x420, 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 8);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_expected_cycle = 1;
	m_pipeline_replay = false;
	m_vector_reads = m_vector_acks = 0;
	m_pipeline_drained_fetches = 0;
	if (memory)
		m_pipeline_operand_tap = data.install_read_tap(0x420, 0x420, "pipeline_operand",
			[this](offs_t, u16 &, u16) {
				if (machine().side_effects_disabled()) return;
				expect(m_cpu->pipeline_cycles() == 5, "DP-relative operand is read in Read, not Execute");
				++m_pipeline_operand_reads;
			});
	m_pipeline_fetch_tap = program.install_read_tap(0x602, 0x602, "pipeline_flush_fetch",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 4, "Already-prefetched flush word reaches Fetch at cycle 4");
			++m_pipeline_drained_fetches;
		});
	m_vector_prefetch_tap = program.install_read_tap(0x40, 0x40, "pipeline_vector",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 6 && m_vector_acks == 1,
				"Vector Fetch occurs at cycle 6 once, after IACK");
			++m_vector_reads;
		});
	m_vector_stack_tap = data.install_write_tap(0xfff, 0xfff, "pipeline_return_pc",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7 && value == 0x601,
				"INTR Execute writes the first not-decoded continuation at cycle 7");
		});
	m_phase = 251000;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_empty_pipeline_case(bool nmi)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_empty_nmi = nmi;
	m_pipeline_expected_cycle = 1;
	m_vector_reads = m_vector_acks = 0;
	u16 const vector = nmi ? 4 : 0x40;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(vector + i, 0xf495);
	program.write_word(vector, 0xe955);
	program.write_word(0x600, 0xe811);
	m_cpu->space(AS_DATA).write_word(0xfff, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, nmi ? 0x0800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, nmi ? 0 : 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, nmi ? 2 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_vector_prefetch_tap = program.install_read_tap(vector, vector, "empty_pipeline_vector",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 4 && m_vector_acks == 1,
				"Empty frontend vector Fetch follows IACK at cycle 4");
			++m_vector_reads;
		});
	m_phase = 251030;
	m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, ASSERT_LINE);
	m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, CLEAR_LINE);
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_rmw_case(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_rmw_case = variant;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_vector_acks = 0;
	m_pipeline_extensions_reads = 0;
	m_pipeline_replay = false;
	m_pipeline_expected_cycle = 1;
	unsigned const kind = (variant / 2) % 3;
	bool const absolute = kind == 1;
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i)
	{
		program.write_word(0x600 + i, 0xf495);
		program.write_word(0x40 + i, 0xf495);
	}
	program.write_word(0x600, (variant & 1 ? 0x6900 : 0x6800) | (absolute ? 0xf8 : kind == 2 ? 0x91 : 0x20));
	program.write_word(0x601, absolute ? 0x420 : 0xe833);
	if (absolute) program.write_word(0x602, 0xe833);
	program.write_word(0x40, 0xe955);
	data.write_word(0x420, 0x55aa); data.write_word(0x4a0, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 8);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x420);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_fetch_tap = program.install_read_tap(0x601, 0x602, "pipeline_rmw_extensions",
		[this](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled() || (address == 0x602 && (m_pipeline_rmw_case / 2) % 3 != 1)) return;
			expect(m_cpu->pipeline_cycles() == (address == 0x601 ? 3 : 4),
				"RMW extension words use independent program-bus Fetch clocks");
			++m_pipeline_extensions_reads;
		});
	m_pipeline_operand_tap = data.install_read_tap(0x420, 0x420, "pipeline_rmw_read",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == ((m_pipeline_rmw_case / 2) % 3 == 1 ? 7 : 6),
				"RMW target is sampled in Read, before Execute");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = data.install_write_tap(0x420, 0x420, "pipeline_rmw_write",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == ((m_pipeline_rmw_case / 2) % 3 == 1 ? 8 : 7) &&
				value == (m_pipeline_rmw_case & 1 ? 0xfdbb : 0x4022),
				"RMW writes the captured Read value combined with its owned literal in Execute");
			++m_pipeline_store_writes;
		});
	m_phase = 251110;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mask_rmw_case()
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i)
	{
		program.write_word(0x600 + i, 0xf495);
		program.write_word(0x40 + i, 0xf495);
	}
	program.write_word(0x600, 0x68f8); // ANDM #fffe,*(IMR).
	program.write_word(0x601, 0); program.write_word(0x602, 0xfffe);
	program.write_word(0x40, 0xe955);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false;
	m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_vector_acks = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0, "pipeline_imr_read",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7 && value == 1, "RMW samples the real IMR register in Read");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0, "pipeline_imr_write",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 8 && value == 0, "Pending RMW clears IMR through its actual Execute-stage bus write");
			++m_pipeline_store_writes;
		});
	m_phase = 251130;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_return_case(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_return_case = variant;
	u32 const base = variant & 2 ? 0x10000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i)
	{
		program.write_word(base + 0x600 + i, 0xf495);
		program.write_word(base + 0x40 + i, 0xf495);
	}
	program.write_word(base + 0x600, 0xe811);
	program.write_word(base + 0x601, 0xe844);
	bool const multiword = variant >= 8 && variant < 12;
	program.write_word(base + 0x40, !multiword && (variant & 1) ? 0xf49b : 0xf69b);
	program.write_word(base + 0x41, multiword ? 0xf020 : 0xe822);
	program.write_word(base + 0x42, multiword ? 0x1234 : 0xe933);
	m_cpu->set_input_line(0, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false;
	m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_vector_acks = 0;
	m_pipeline_drained_fetches = 0;
	m_pipeline_extensions_reads = 0;
	if (multiword)
		m_pipeline_write_tap = program.install_read_tap(base + 0x42, base + 0x42, "fast_return_delay_literal",
			[this](offs_t, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				expect(m_cpu->pipeline_cycles() == 8 && value == 0x1234, "Two-word delay instruction fetches its literal once");
				++m_pipeline_extensions_reads;
			});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "fast_return_target_fetch",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == (m_pipeline_drained_fetches ? 9 : 3), "Fast return fetches its target after Access, before Execute");
			++m_pipeline_drained_fetches;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xfff, 0xfff, "fast_return_stack_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_phase = 251150;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_branch_case(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_branch_case = variant;
	u32 const base = variant & 2 ? 0x10000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i)
	{
		program.write_word(base + 0x600 + i, 0xf495);
		program.write_word(base + 0x700 + i, 0xf495);
	}
	program.write_word(base + 0x600, variant & 1 ? 0xf073 : 0xf273);
	program.write_word(base + 0x601, 0x700);
	program.write_word(base + 0x602, 0xe822);
	program.write_word(base + 0x603, 0xe933);
	program.write_word(base + 0x604, 0x77e1); // Unsupported indirect STM must not reach Decode.
	program.write_word(base + 0x700, 0xe844);
	if (variant >= 8)
	{
		for (unsigned i = 0; i < 12; ++i) program.write_word(base + 0x40 + i, 0xf495);
		program.write_word(base + 0x40, 0xe955);
	}
	m_cpu->set_input_line(0, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, variant >= 8 ? 0 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false;
	m_pipeline_expected_cycle = 1;
	m_pipeline_drained_fetches = m_pipeline_extensions_reads = 0;
	m_vector_acks = 0;
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "branch_target_fetch",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 6, "Branch target Fetch follows literal Access");
			++m_pipeline_drained_fetches;
		});
	m_pipeline_operand_tap = program.install_read_tap(base + 0x601, base + 0x601, "branch_literal_fetch",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 3 && value == 0x700, "Branch literal is fetched once independently of target instructions");
			++m_pipeline_extensions_reads;
		});
	m_phase = 251170;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delay_asm_fixture tms320c54x_test_state::delay_asm_case(unsigned v)
{
	delay_asm_fixture f{}; f.control = v % 4; v /= 4; f.form = v % 5; v /= 5;
	f.raw = v % 32; v /= 32; f.pattern = v % 2; v /= 2; f.flags = v % 4; v /= 4;
	f.base = v % 2 ? 0x20000 : 0; v /= 2;
	int const shift = f.raw < 16 ? int(f.raw) : int(f.raw) - 32;
	u64 const source = f.pattern ? 0xfffffffff9ULL : 7;
	if (shift >= 0) f.result = (source << shift) & 0xffffffffffULL;
	else if (f.pattern && (f.flags & 1)) f.result = u64(-((7 + (s64(1) << -shift) - 1) >> -shift)) & 0xffffffffffULL;
	else f.result = source >> -shift;
	f.checkpoint = std::array<unsigned, 3>{3 + f.prefix(), f.read(), f.read() + 1}[v];
	return f;
}

void tms320c54x_test_state::start_delay_asm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = delay_asm_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.prefix()) program.write_word(f.base + 0x600, f.form == 1 ? 0x8000 : 0x8002);
	unsigned const start = 0x600 + f.prefix();
	program.write_word(f.base + start, f.control == 1 || f.control == 2 ? 0xfa45 : 0xf273); program.write_word(f.base + start + 1, 0x700);
	program.write_word(f.base + start + 2, std::array<u16, 5>{0x3202, 0x3200, 0x3293, 0x32e3, 0x32f8}[f.form]);
	program.write_word(f.base + start + 3, f.long_form() ? f.form == 3 ? 3 : 0x1002 : 0xf682);
	if (f.long_form()) program.write_word(f.base + f.target(), 0xf682);
	program.write_word(f.base + f.store(), 0x880f); program.write_word(f.base + f.store() + 1, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa620); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1() | (f.raw ^ 31));
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.prefix() ? 0x1234000000ULL | f.source_word() : f.control == 1 ? 0 : f.control == 2 ? 1 : 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.pattern ? 0xfffffffff9ULL : 7); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1002);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 3 ? f.form == 3 ? 0xfff : 0x1002 : 0x5100 + i);
	data.write_word(0x1002, f.prefix() ? 0xdead : f.source_word());
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "delay_asm_source", [this, f](offs_t address, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(address == 0x1002 && word == f.source_word() && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles() == f.read(), "Delay ASM captures its DP/SP/indirect operand once at Read"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "delay_asm_writes", [this, f](offs_t address, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (address == 0x1002) { expect(f.prefix() && word == f.source_word() && !m_pipeline_register_writes++ && m_cpu->pipeline_cycles() == 6, "The original-like older store supplies the stack/RAM ASM word"); return; } expect(address == 0xf && word == u16(f.result) && !m_pipeline_store_writes++ && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() >= f.read() + 3, "The selected branch target observes the ASM-shifted result once"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + start + 3, f.base + start + 3, "delay_asm_extension", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || !f.long_form()) return; expect(word == (f.form == 3 ? 3 : 0x1002) && !m_pipeline_extensions_reads++ && m_cpu->pipeline_cycles() == f.literal_clock(), "Delay ASM retains its one address extension"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 311000;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_far_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	unsigned const form = variant % 36, prefix = variant >= 36;
	u32 const base = form & 1 ? 0x10000 : 0;
	u32 const target = (base ^ 0x10000) + 0x700;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 20; ++i) { program.write_word(base + 0x600 + i, 0xe933); program.write_word((target & 0x10000) + 0x700 + i, 0xf495); }
	bool const delayed = form >= 12;
	if (prefix) program.write_word(base + 0x600, 0x8a11); // POPM AR1 immediately before the return.
	program.write_word(base + 0x600 + prefix, (form % 12 < 6 ? 0xf4e5 : 0xf4e4) | (delayed ? 0x200 : 0));
	if (delayed) {
		program.write_word(base + 0x601 + prefix, form >= 24 ? 0xf020 : 0xe811);
		program.write_word(base + 0x602 + prefix, form >= 24 ? 0x1234 : 0xe922);
	}
	program.write_word(target, 0xe955);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000 - prefix);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + 1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->space(AS_DATA).write_word(0x1000, 0x80 | (target >> 16));
	m_cpu->space(AS_DATA).write_word(0x1001, 0x700);
	m_cpu->space(AS_DATA).write_word(0x0fff, 0x42);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x1000, 0x1001, "staged_far_return_stack",
		[this, target, delayed, prefix](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == 0x1000 + m_pipeline_operand_reads && value == (address == 0x1000 ? (0x80 | (target >> 16)) : 0x700) &&
				m_cpu->pipeline_cycles() == (delayed ? 4 : 5) + prefix + m_pipeline_operand_reads, "Far return samples XPC then PC once in the declared retained-return model");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(target, target, "staged_far_return_target",
		[this, delayed, prefix](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == (delayed ? 6 : 8) + prefix, "Far-return target Fetch follows plain or delayed control redirection"); ++m_pipeline_extensions_reads;
		});
	m_phase = 251290; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_intm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const set = BIT(variant, 0);
	unsigned const words = (variant / 2) % 3 + 1;
	unsigned const adjacent = variant >= 6;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(0x600 + i, 0xf495);
	for (unsigned i = 0; i < 24; ++i) program.write_word(0x44 + i, 0xf495);
	program.write_word(0x600, (set != bool(adjacent)) ? 0xf7bb : 0xf6bb);
	if (adjacent) program.write_word(0x601, set ? 0xf7bb : 0xf6bb);
	program.write_word(0x601 + adjacent, words == 1 ? 0xe842 : words == 2 ? 0xf020 : 0x76f8);
	if (words >= 2) program.write_word(0x602 + adjacent, words == 2 ? 0x1234 : 0x2000);
	if (words == 3) program.write_word(0x603 + adjacent, 0x1234);
	program.write_word(0x44, 0xe955);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, (set != bool(adjacent)) ? 0 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 2);
	m_cpu->space(AS_DATA).write_word(0x2000, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_fetch_tap = program.install_read_tap(0x44, 0x44, "staged_intm_vector",
		[this, words, adjacent](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() > 6 + words + adjacent &&
				(words == 3 ? m_pipeline_store_writes == 1 :
				m_cpu->state_int(tms320c54x_device::STATE_A) == (words == 1 ? 0x42 : 0x1234)),
				"INTM guard retires the complete follower before vector Fetch");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x2000, 0x2000, "staged_intm_follower",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x1234 && m_pipeline_operand_reads == 0, "INTM-protected absolute store precedes IRQ acknowledgement");
			++m_pipeline_store_writes;
		});
	m_phase = 251310; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_status_bit(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	u16 const opcode = 0xf4b0 | ((variant & 0x30) << 4) | (variant & 15);
	u16 const initial = BIT(variant, 6) ? 0xffff : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, i ? 0xf495 : opcode);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, initial);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, initial);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_phase = 251330; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_cpl(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const stlm = !strcmp(machine().options().bios(), "stlm-cpl") || !strcmp(machine().options().bios(), "stlm-cmpt");
	bool const stm = !strcmp(machine().options().bios(), "stm-cpl") || !strcmp(machine().options().bios(), "stm-cmpt");
	bool const cmpt = !strcmp(machine().options().bios(), "mvmd-cmpt") || !strcmp(machine().options().bios(), "stlm-cmpt") || !strcmp(machine().options().bios(), "stm-cmpt");
	bool const mvmd = stm || stlm || cmpt || !strcmp(machine().options().bios(), "mvmd-cpl");
	bool const set = BIT(variant, 0), store = BIT(variant, 3);
	bool const dp = BIT(variant, 4);
	unsigned const gap = (variant / 2) % 4;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 20; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, dp ? (set ? 0xf5b0 : 0xf4b0) : (set ? 0xf7be : 0xf6be));
	program.write_word(0x601 + gap, store ? 0x8027 : 0x1027);
	if (mvmd)
	{
		program.write_word(0x600, 0x730b); program.write_word(0x601, 7);
		program.write_word(0x602 + gap, store ? 0x8027 : 0x1027);
		if (!gap) program.write_word(0x603, 0xf495);
		else program.write_word(0x601 + gap, 0xf495);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, set ? 0x4800 : 0x800);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, dp && !set ? 0x0f : 0x0e);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, dp || set ? 0x800 : 0x4800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7777);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0x727, 0x1111);
	m_cpu->space(AS_DATA).write_word(dp ? 0x7a7 : 0x1027, 0x2222);
	if (cmpt)
	{
		program.write_word(0x602 + gap, store ? 0x8090 : 0x1090);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x6000);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, set ? 0x800 : 0x820);
		m_cpu->set_state_int(tms320c54x_device::STATE_B, set ? 0x820 : 0x800);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1500);
		m_cpu->space(AS_DATA).write_word(0x1400, 0x1111); m_cpu->space(AS_DATA).write_word(0x1500, 0x2222);
	}
	if (stlm)
	{
		program.write_word(0x600, 0x8907); program.write_word(0x601, 0xf495);
		program.write_word(0x602 + gap, 0xf495);
		program.write_word(0x601 + gap, cmpt ? (store ? 0x8090 : 0x1090) : (store ? 0x8027 : 0x1027));
	}
	if (stm) { program.write_word(0x600, 0x7707); program.write_word(0x601, set ? (cmpt ? 0x820 : 0x4800) : 0x800); }
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_phase = 251350; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_dp_immediate(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	unsigned const page = variant & 511;
	bool const chain = BIT(variant, 9);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0xea00 | (chain ? page ^ 1 : page));
	if (chain) program.write_word(0x601, 0xea00 | page);
	program.write_word(0x601 + unsigned(chain), 0x107f);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600 | (page ^ 1));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(((page ^ 1) << 7) | 0x7f, 0x1111);
	m_cpu->space(AS_DATA).write_word((page << 7) | 0x7f, 0x2222);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_phase = 251390; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_frame(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	s8 const offset = variant & 255;
	u16 const target = 0x1000 + offset - 1;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0xee00 | (variant & 255));
	program.write_word(0x601, 0x4a11);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0e);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x5678);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(target, 0xaaaa);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_store_writes = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(target, target, "staged_frame_push",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x5678 && m_cpu->pipeline_cycles() == 7, "FRAME's adjacent push writes its retained register to the adjusted stack");
			++m_pipeline_store_writes;
		});
	m_phase = 251410; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mu4_prologue(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	u32 const base = BIT(variant, 0) ? 0x20000 : 0;
	// Original R060 aa22 skip-worker path through RET at 02:3da5.
	static constexpr u16 prefix[] = {0xeeff, 0x4a06, 0x4a07, 0xf6be, 0xf6b9,
		0x4a08, 0x4a09, 0x4a0a, 0x4a0b, 0x4a0c, 0x4a0d, 0xea00, 0x577c, 0xf300, 0x0001, 0x4f7c, 0x487e,
		0xf030, 0xc004, 0xf050, 0xc004, 0xfa44, 0x3d9a, 0x467f, 0xf495};
	static constexpr u16 tail[] = {0x1036, 0xf7be, 0x8a0d, 0x8a0c, 0x8a0b,
		0x8006, 0x8a0a, 0x8a09, 0x8a08, 0x8a07, 0x8a06, 0xfc00};
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x3d73 + i, i < std::size(prefix) ? prefix[i] : 0xf495);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x3d9a + i, i < std::size(tail) ? tail[i] : 0xf495);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x4044 + i, 0xf495);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa61b);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4a00);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0xff7; i <= 0xfff; ++i) m_cpu->space(AS_DATA).write_word(i, 0xaaaa);
	m_cpu->space(AS_DATA).write_word(0x7c, 0x1357);
	m_cpu->space(AS_DATA).write_word(0x7d, 0x2468);
	m_cpu->space(AS_DATA).write_word(0x7e, 0x8001);
	m_cpu->space(AS_DATA).write_word(0x7f, 0xe012);
	m_cpu->space(AS_DATA).write_word(0x936, 0x4044);
	m_cpu->set_state_int(STATE_GENPC, base + 0x3d73);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_store_writes = 0;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xff7, 0xfff, "staged_mu4_context_save",
		[this](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			static constexpr u16 expected[] = {0xa61b, 0x4a00, 0x789a, 0x3456, 0x12, 0x5566, 0x3344, 0x22, 0x4044};
			expect(m_pipeline_store_writes < 9 && address == (m_pipeline_store_writes < 8 ? 0xffe - m_pipeline_store_writes : 0xfff) &&
				value == expected[m_pipeline_store_writes], "Original MU4 prologue saves status and accumulator words in stack order");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xfff, 0xfff, "staged_mu4_reserved_return",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x4044 && m_cpu->pipeline_cycles() == 46, "Original RET reads the reserved slot after the older POPM ST0 increment");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x4044, base + 0x4044, "staged_mu4_reserved_continuation",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 48, "Original RET reaches the reserved continuation in its current program page");
			++m_pipeline_extensions_reads;
		});
	m_phase = 251470; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_dld(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const compiler = BIT(variant, 5);
	u16 const offset = 0x7c + BIT(variant, 4);
	u16 const address = (compiler ? 0x500 : 0x400) + offset;
	u16 const high = BIT(variant, 3) ? 0x8001 : 0x1234;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, i ? 0xf495 : 0x5600 | (BIT(variant, 0) << 8) | offset);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (BIT(variant, 1) << 8) | (BIT(variant, 2) << 7) | (compiler ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x500);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1111222233ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x4444555566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(address, high);
	m_cpu->space(AS_DATA).write_word(address ^ 1, 0xabcd);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(address & ~1, address | 1, "staged_dld_pair",
		[this, address, high](offs_t observed, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_pipeline_operand_reads < 2 && observed == (address ^ m_pipeline_operand_reads) &&
				value == (m_pipeline_operand_reads ? 0xabcd : high) && m_cpu->pipeline_cycles() == 5,
				"DLD retains its ordered address/xor-one word pair in Read");
			++m_pipeline_operand_reads;
		});
	m_phase = 251490; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_immediate_add(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const pattern = BIT(variant, 6);
	unsigned const shift = std::array<unsigned, 4>{0, 1, 7, 15}[(variant >> 4) & 3];
	u16 const literal = pattern ? 0xf495 : 1;
	u64 const source = pattern ? 0xff80000000ULL : 0x007fffffffULL;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0xf000 | (BIT(variant, 0) << 9) | (BIT(variant, 1) << 8) | shift);
	program.write_word(0x601, literal);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1b);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (BIT(variant, 2) << 8) | (BIT(variant, 3) << 9));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, BIT(variant, 0) ? 0x1111222233ULL : source);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, BIT(variant, 0) ? source : 0x4444555566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(0x601, 0x601, "staged_add_literal",
		[this, literal](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == literal && m_cpu->pipeline_cycles() == 3, "Immediate ADD fetches its retained literal once");
			++m_pipeline_operand_reads;
		});
	m_phase = 251510; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_conditional_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const b = BIT(variant, 0), eq = BIT(variant, 1), nonzero = BIT(variant, 2);
	u32 const base = BIT(variant, 3) ? 0x10000 : 0;
	bool const taken = eq ? !nonzero : nonzero;
	bool const memory_dp = variant >= 48;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, 0xfa44 | (b << 3) | eq);
	program.write_word(base + 0x601, 0x700);
	program.write_word(base + 0x602, memory_dp ? 0x467f : 0xe811);
	program.write_word(base + 0x603, memory_dp ? 0xf495 : 0xe944);
	program.write_word(base + 0x604, 0xe966);
	program.write_word(base + 0x700, memory_dp ? 0x107e : 0xe955);
	if (memory_dp) program.write_word(base + 0x701, 0xe955);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, b ? 0x9999 : nonzero ? 0x0100000000ULL : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, b ? (nonzero ? 0x0100000000ULL : 0) : 0x7777);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	if (memory_dp) {
		m_cpu->space(AS_DATA).write_word(0x47f, 0xe012);
		m_cpu->space(AS_DATA).write_word(0x97e, 0x3333);
	}
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_drained_fetches = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = program.install_read_tap(base + 0x700, base + 0x700, "staged_conditional_target",
		[this, taken, memory_dp](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(taken && m_cpu->pipeline_cycles() == (memory_dp ? 10 : 8), "BCD uses the retained predicate and conditional prefetch bubble");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x602, base + 0x603, "staged_conditional_delays",
		[this, base, memory_dp](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && address == base + 0x602 + m_pipeline_extensions_reads &&
				value == (memory_dp ? (m_pipeline_extensions_reads ? 0xf495 : 0x467f) : (m_pipeline_extensions_reads ? 0xe944 : 0xe811)) &&
				m_cpu->pipeline_cycles() == (m_pipeline_extensions_reads ? 6 : 4), "BCD fetches both delay words with one intervening bubble");
			++m_pipeline_extensions_reads;
		});
	if (memory_dp) m_pipeline_write_tap = m_cpu->space(AS_DATA).install_read_tap(0x47f, 0x97e, "staged_conditional_dp_data",
		[this, taken](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0x47f) {
				expect(value == 0xe012 && m_cpu->pipeline_cycles() == 7, "BCD memory-DP delay samples its original page once");
				++m_pipeline_drained_fetches;
			} else if (address == 0x97e) {
				expect(taken && value == 0x3333 && m_cpu->pipeline_cycles() == 13, "Taken BCD continuation uses DP published by its delay slot");
				++m_pipeline_store_writes;
			}
		});
	m_phase = 251610; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_plain_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const pushed = variant >= 12, adjacent = pushed && variant < 24, enable = BIT(variant, 1);
	unsigned const prefix = pushed ? (adjacent ? 1 : 2) : 0;
	u16 const target = adjacent ? 0x720 : 0x700;
	u32 const base = BIT(variant, 0) ? 0x10000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xe933); program.write_word(base + 0x700 + i, 0xf495); program.write_word(base + 0x720 + i, 0xf495); }
	if (pushed) program.write_word(base + 0x600, 0x4a11);
	if (prefix == 2) program.write_word(base + 0x601, 0xf495);
	program.write_word(base + 0x600 + prefix, enable ? 0xf4eb : 0xfc00);
	program.write_word(base + 0x700, 0xe955);
	program.write_word(base + 0x720, 0xe999);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa61b);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x700);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0xfff, 0x720);
	m_cpu->space(AS_DATA).write_word(0x1000, 0x700);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(pushed ? 0xfff : 0x1000, pushed ? 0xfff : 0x1000, "staged_plain_return_stack",
		[this, prefix, target](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == target && m_cpu->pipeline_cycles() == 5 + prefix, "RET samples old DARAM before concurrent push Write; one NOP exposes the new value");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + target, base + target, "staged_plain_return_target",
		[this, prefix](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7 + prefix, "RET target Fetch uses its retained stack sample");
			++m_pipeline_extensions_reads;
		});
	if (pushed) m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xfff, 0xfff, "staged_plain_return_push",
		[this, adjacent](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x700 && m_cpu->pipeline_cycles() == 6 && m_pipeline_operand_reads == unsigned(adjacent), "Push writes in the second half-cycle, after any same-clock return Read");
			++m_pipeline_store_writes;
		});
	m_phase = 251630; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const t_delay = variant >= 24;
	u16 const parameter = t_delay ? std::array<u16, 9>{6, 0, 0x8000, 0xf495, 0xffff, 0x76e6, 0x4ee6, 0x7001, 0x54e6}[(variant - 24) / 12] : 0;
	unsigned const adjacent = t_delay ? BIT(variant, 1) : variant >= 12;
	bool const delayed = t_delay || BIT(variant, 1);
	u32 const base = BIT(variant, 0) ? 0x10000 : 0;
	u16 const continuation = 0x600 + adjacent + (delayed ? 4 : 2);
	u16 const stack = 0xfff - adjacent;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	if (adjacent) program.write_word(base + 0x600, 0x4a11);
	program.write_word(base + 0x600 + adjacent, delayed ? 0xf274 : 0xf074);
	program.write_word(base + 0x601 + adjacent, 0x700);
	if (delayed) { program.write_word(base + 0x602 + adjacent, t_delay ? 0x770e : 0xe811); program.write_word(base + 0x603 + adjacent, t_delay ? parameter : 0xe922); }
	program.write_word(base + continuation, 0xe966);
	program.write_word(base + 0x700, t_delay ? 0x490e : 0xe955);
	program.write_word(base + 0x701, 0xfc00);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa61b);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_RTN, 0x8888);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x7777);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0xffe, 0xaaaa);
	m_cpu->space(AS_DATA).write_word(0xfff, 0xaaaa);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_drained_fetches = 0;
	m_pipeline_extensions_reads = 0;
	m_pipeline_register_writes = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x0e, 0xfff, "staged_call_stack",
		[this, stack, continuation, adjacent, t_delay, parameter](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0x0e) {
				expect(t_delay && value == parameter && m_cpu->pipeline_cycles() == 7 + adjacent, "CALLD's complete two-word STM delay publishes T once before callee Read");
				++m_pipeline_register_writes;
			} else if (address == stack) {
				expect(value == continuation && m_cpu->pipeline_cycles() == 6 + adjacent, "CALL writes its retained return word in the opcode's Execute clock");
				++m_pipeline_store_writes;
			} else if (address == 0xfff) {
				expect(adjacent && address == 0xfff && value == 0x7777 && m_cpu->pipeline_cycles() == 6, "Older push and younger CALL own distinct stack destinations");
				++m_pipeline_drained_fetches;
			}
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(stack, stack, "staged_call_return_pop",
		[this, continuation, adjacent](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == continuation && m_cpu->pipeline_cycles() == 10 + adjacent, "Callee RET reads exactly the caller's retained stack word");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "staged_call_program_observer",
		[this, adjacent](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 6 + adjacent, "CALL target Fetch coincides with its return-address write");
			++m_pipeline_extensions_reads;
		});
	m_phase = 251650; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_direct_move(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const indirect = variant >= 60;
	unsigned const move_case = indirect ? variant - 60 : variant;
	bool const conflict = !indirect && variant >= 48;
	u16 const destination = indirect ? std::array<u16, 4>{0x10, 0x12, 0x18, 0x700}[move_case & 3] : std::array<u16, 4>{0x12, 0x18, 0x19, 0x700}[variant & 3];
	u16 const source = indirect ? 0x510 : conflict ? 0x7f : BIT(variant, 2) ? 0x531 : 0x431;
	u16 const value = BIT(move_case, 3) ? 0xbeef : 0x123;
	unsigned const write_clock = conflict ? 7 : destination < 0x60 ? 5 : 6;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	if (conflict) program.write_word(0x600, 0x8011); // Older STL uses the DAGEN bank.
	program.write_word(0x600 + conflict, indirect ? (BIT(move_case, 2) ? 0x7182 : 0x7192) : conflict ? 0x717f : 0x7131);
	program.write_word(0x601 + conflict, destination);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, conflict ? 0xa600 : 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (!conflict && BIT(variant, 2) ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x500);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, indirect ? 0x510 : 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x5678);
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, 0x40);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(source, value);
	m_cpu->space(AS_DATA).write_word(0x700, 0xaaaa);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(source, source, "staged_mvd_source",
		[this, value, conflict](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == value && m_cpu->pipeline_cycles() == (conflict ? 6 : 5), "MVDK retains its source sample once at the opcode's Read clock");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(destination, destination, "staged_mvd_destination",
		[this, value, write_clock](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == value && m_cpu->pipeline_cycles() == write_clock, "MVDK writes its retained value at the destination-owned clock");
			++m_pipeline_store_writes;
		});
	m_phase = 251670; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_worker_xc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const branch = variant >= 8;
	u32 const origin = branch ? 0x23dbc : 0x600;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(origin + i, 0xf495); if (branch) program.write_word(0x23ddb + i, 0xf495); }
	std::array<u16, 9> const prefix{0xfd00, 0x4a07, 0xfd00, 0xf7bb, 0x7192, 0x0010, 0x1092, 0xf844, 0x3ddb};
	for (unsigned i = 0; i < (branch ? 9 : variant >= 4 ? 7 : 4); ++i) program.write_word(origin + i, prefix[i]);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x500);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x5678);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x510);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
	m_cpu->space(AS_DATA).write_word(0x510, 0xbeef);
	m_cpu->space(AS_DATA).write_word(0x511, branch ? BIT(variant, 0) : 0x1357);
	m_cpu->set_state_int(STATE_GENPC, origin);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x4ff, 0x4ff, "worker_xc_push",
		[this](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0 && m_cpu->pipeline_cycles() == 7, "Original XC prefix saves old ST1 once before setting INTM");
			++m_pipeline_store_writes;
		});
	if (variant >= 4)
		m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x510, 0x511, "original_worker_reads",
			[this, variant, branch](offs_t address, u16 &word, u16) {
				if (machine().side_effects_disabled()) return;
				expect(word == (address == 0x510 ? 0xbeef : branch ? BIT(variant, 0) : 0x1357) &&
					m_cpu->pipeline_cycles() == (address == 0x510 ? 9 : 11), "Original worker MVDK and adjacent LD sample successive source words once");
				++m_pipeline_operand_reads;
			});
	if (branch)
		m_pipeline_fetch_tap = program.install_read_tap(0x23dc7, 0x23ddb, "original_worker_bc_route",
			[this, variant](offs_t address, u16 &, u16) {
				if (machine().side_effects_disabled()) return;
				if (address != 0x23dc7 && address != 0x23ddb) return;
				expect(bool(BIT(variant, 0)) == (address == 0x23ddb) && m_cpu->pipeline_cycles() == 14, "Original worker BC consumes adjacent load and fetches the selected route once");
				if (address == 0x23ddb) ++m_pipeline_extensions_reads;
				else ++m_pipeline_drained_fetches;
			});
	m_phase = 251690; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_xc_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const zero = variant & 1, invert = variant & 2, wide = variant & 4;
	bool const selected = zero != invert;
	unsigned const source = (variant / 16) % 3; u32 const base = variant & 8 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, invert ? 0xfd4c : 0xfd4d); program.write_word(base + 0x601, 0xfc00); program.write_word(base + 0x603, 0xf4e1);
	program.write_word(base + 0x700, wide ? 0x7660 : 0x8c60); if (wide) program.write_word(base + 0x701, 0xbeef);
	// The ordinary status probe is beyond the protected word's retirement;
	// adjacent status admission remains a separate conservative boundary.
	program.write_word(base + 0x706, 0x7707); program.write_word(base + 0x707, 0x0740); program.write_word(base + 0x708, 0xf4e1);
	program.write_word(base + 0x40, 0xf4e1); program.write_word(base + 4, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa020); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x100);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, zero ? 0 : 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	data.write_word(0x3000, 0x700); data.write_word(0x1060, 0xdead);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_vector_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "xc_return_stack", [this, selected](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(selected && at == 0x3000 && word == 0x700 && !m_pipeline_operand_reads++, "Selected XC return samples one stack target"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "xc_return_writes", [this, selected, source](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x3000) { expect(selected && source && m_pipeline_store_writes == 1 && !m_pipeline_register_writes++, "Pending interrupt waits for the resumed protected word before stacking"); return; } if (at == 7) { expect(selected && word == 0x0740 && !m_pipeline_extensions_reads++, "Later ordinary STM ST1 is not trapped by stale XC protection"); return; } expect(selected && at == 0x1060 && word == 0xbeef && !m_pipeline_store_writes++, "Resumed protected instruction writes once"); });
	m_vector_prefetch_tap = program.install_read_tap(base + (source == 2 ? 4 : 0x40), base + (source == 2 ? 4 : 0x40), "xc_return_vector", [this, selected, source](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return; expect(selected && source && m_pipeline_store_writes == 1 && !m_vector_reads++, "XC protection expires after the resumed word and permits vector Fetch"); m_cpu->set_input_line(source == 2 ? INPUT_LINE_NMI : 0, CLEAR_LINE); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62176;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_xc_tc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	bool const producer = variant >= 64;
	unsigned const gap = producer ? (variant - 64) / 16 : 0;
	unsigned const offset = producer ? 1 + gap : 0;
	bool const multiword = variant >= 32 && !producer;
	if (producer) program.write_word(0x600, BIT(variant, 0) ? 0xf4bc : 0xf5bc);
	program.write_word(0x600 + offset, 0xfd20 | ((multiword || BIT(variant, 1)) << 9) | (BIT(variant, 2) << 4));
	program.write_word(0x601 + offset, multiword ? 0x7600 : 0x8000);
	program.write_word(0x602 + offset, multiword ? 0xfd30 : 0x8001);
	if (multiword) program.write_word(0x603, 0x8001);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa60e | (BIT(variant, 0) << 12));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_store_writes = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x700, 0x701, "xc_tc_stores",
		[this, variant](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			bool const producer = variant >= 64;
			unsigned const gap = producer ? (variant - 64) / 16 : 0;
			unsigned const offset = producer ? 1 + gap : 0;
			bool const sample = producer && gap < 2 ? BIT(variant, 0) : !BIT(variant, 0);
			bool const execute = sample == BIT(variant, 2);
			bool const multiword = variant >= 32 && !producer;
			expect(word == (multiword && address == 0x700 ? 0xfd30 : 0x1234) &&
				m_cpu->pipeline_cycles() == (multiword ? 8 : 7) + offset + address - 0x700 &&
				(execute || (address == 0x701 && (multiword || !BIT(variant, 1)))), "XC uses Access-sampled TC and rejected words never write data");
			++m_pipeline_store_writes;
		});
	m_phase = 251730; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_plain_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const b = BIT(variant, 0), eq = BIT(variant, 1), nonzero = BIT(variant, 2);
	bool const taken = eq ? !nonzero : nonzero;
	u32 const base = BIT(variant, 3) ? 0x10000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, 0xf844 | (b << 3) | eq);
	program.write_word(base + 0x601, 0x700);
	program.write_word(base + 0x602, 0x1070); program.write_word(base + 0x603, 0x1071);
	program.write_word(base + 0x604, 0xe866); program.write_word(base + 0x700, 0xe855);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, b ? 0x9999 : nonzero ? 0x0100000000ULL : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, b ? (nonzero ? 0x0100000000ULL : 0) : 0x7777);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0x470, 0x1111); m_cpu->space(AS_DATA).write_word(0x471, 0x2222);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = program.install_read_tap(base + 0x700, base + 0x700, "plain_bc_target",
		[this, taken](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(taken && m_cpu->pipeline_cycles() == 7, "Taken BC fetches its target after discarding sequential Decode");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x602, base + 0x603, "plain_bc_sequential",
		[this, base](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == base + 0x602 + m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == (address == base + 0x602 ? 4 : 6), "BC retains each sequential Fetch exactly once across its empty slot");
			++m_pipeline_extensions_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_read_tap(0x470, 0x471, "plain_bc_data",
		[this, taken](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(!taken && word == (address == 0x470 ? 0x1111 : 0x2222) && m_cpu->pipeline_cycles() == 8 + address - 0x470, "Only not-taken BC decodes sequential data reads");
			++m_pipeline_store_writes;
		});
	m_phase = 251750; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::signed_xc_fixture tms320c54x_test_state::signed_xc_case(unsigned variant)
{
	unsigned const condition = variant % 6, pattern = (variant / 12) % 8;
	bool const b = (variant / 6) % 2;
	u64 const value = std::array<u64, 8>{0, 1, 0x8000000000ULL, 0xffffffffffULL, 0x7fffffffffULL, 0x0100000000ULL, 0x0080000000ULL, 0xff80000000ULL}[pattern];
	s64 const signed_value = value & 0x8000000000ULL ? s64(value) - (s64(1) << 40) : s64(value);
	bool const taken = std::array<bool, 6>{signed_value >= 0, signed_value < 0, value != 0, value == 0, signed_value > 0, signed_value <= 0}[condition];
	u64 const opposite = taken ? std::array<u64, 6>{0x8000000000ULL, 0, 0, 1, 0, 1}[condition] : std::array<u64, 6>{0, 0x8000000000ULL, 1, 0, 1, 0}[condition];
	return {condition, (variant / 96) % 3, b, taken, value, opposite, (variant / 288) % 2 ? 0x20000U : 0};
}

void tms320c54x_test_state::start_signed_xc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = signed_xc_case(variant); bool const multiword = f.layout == 2;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xfd42 + f.condition + (f.b ? 8 : 0) + (f.layout ? 0x200 : 0));
	program.write_word(f.base + 0x601, multiword ? 0x7600 : 0x8c00);
	program.write_word(f.base + 0x602, multiword ? 0xfd43 : 0x8c01);
	if (multiword) program.write_word(f.base + 0x603, 0x8c01);
	program.write_word(f.base + 0x603 + multiword, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa60e); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.b ? 0x1234 : f.opposite); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ? f.opposite : 0x3456);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "signed_xc_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Signed XC slots never read a data operand"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "signed_xc_writes",
		[this, f, multiword](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect((at == 0x700 || at == 0x701) && (f.taken || (at == 0x701 && (multiword || !f.layout))) && word == (at == 0x700 && multiword ? 0xfd43 : 0x5aa5) && m_cpu->pipeline_cycles() == 7 + unsigned(multiword) + at - 0x700, "Signed XC retains its Access predicate and never executes a rejected slot"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x602, "signed_xc_words",
		[this, f](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return; unsigned const bit = 1U << (at - f.base - 0x601); expect(!(m_pipeline_extensions_reads & bit), "Each protected word is fetched once, including opcode-shaped literal"); m_pipeline_extensions_reads |= bit; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254880;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_delayed_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const xc = variant >= 12, two_word = !xc && BIT(variant, 1);
	u32 const base = BIT(variant, 0) ? 0x10000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); program.write_word(base + 0x40 + i, 0xf495); }
	program.write_word(base + 0x600, 0xfe00);
	program.write_word(base + 0x601, xc ? 0xfd45 : two_word ? 0x770e : 0xe811);
	program.write_word(base + 0x602, xc ? 0xf6bb : two_word ? 0x0006 : 0xe944);
	program.write_word(base + 0x603, 0xffff); // Must never become a third delay word.
	program.write_word(base + 0x700, 0xe955);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, xc && BIT(variant, 1) ? 0x800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, xc ? 1 : 0);
	m_cpu->space(AS_DATA).write_word(0x1000, 0x700);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x1000, 0x1000, "retd_stack",
		[this](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0x700 && m_cpu->pipeline_cycles() == 5, "RETD samples its stack return once in Read"); ++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "retd_target",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7, "RETD fetches its retained target after two empty Fetch slots"); ++m_pipeline_extensions_reads;
		});
	m_pipeline_write_tap = program.install_read_tap(base + 0x601, base + 0x603, "retd_delay_fetch",
		[this, base](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == base + 0x601 + m_pipeline_store_writes && m_pipeline_store_writes < 2 &&
				m_cpu->pipeline_cycles() == 3 + address - base - 0x601, "RETD fetches two retained delay words once and no third word"); ++m_pipeline_store_writes;
		});
	m_phase = 251790; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_original_worker_tail(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const masked = BIT(variant, 0); u16 const saved_status = masked ? 0x0834 : 0x0034;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(0x23ddb + i, 0xf495); program.write_word(0x23d96 + i, 0xf495); program.write_word(0x20040 + i, 0xf495); }
	std::array<u16, 7> const tail{0x8a08, 0xf030, 0x0800, 0xf495, 0xfe00, 0xfd45, 0xf6bb};
	for (unsigned i = 0; i < tail.size(); ++i) program.write_word(0x23ddb + i, tail[i]);
	program.write_word(0x23de2, 0xffff); program.write_word(0x23d96, 0xe955);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0xfff);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1122334455ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->space(AS_DATA).write_word(0xfff, saved_status); m_cpu->space(AS_DATA).write_word(0x1000, 0x3d96);
	m_cpu->set_state_int(STATE_GENPC, 0x23ddb); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xfff, 0x1000, "original_worker_tail_stack",
		[this, saved_status](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == (address == 0xfff ? saved_status : 0x3d96) && m_cpu->pipeline_cycles() == (address == 0xfff ? 5 : 9), "Original tail samples saved ST1 then its return word once"); ++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(0x23d96, 0x23d96, "original_worker_tail_target",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 11, "Original tail fetches its page-2 caller continuation once"); ++m_pipeline_extensions_reads;
		});
	m_pipeline_write_tap = program.install_read_tap(0x23de0, 0x23de2, "original_worker_tail_delays",
		[this](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == 0x23de0 + m_pipeline_drained_fetches && m_pipeline_drained_fetches < 2 && m_cpu->pipeline_cycles() == 7 + address - 0x23de0, "Original return keeps exactly XC and RSBX as its two delay words"); ++m_pipeline_drained_fetches;
		});
	m_vector_prefetch_tap = program.install_read_tap(0x20040, 0x20040, "original_worker_tail_irq",
		[this, masked](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(!masked && m_cpu->pipeline_cycles() == 18 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55, "Original tail restores unmasked state only after the first continuation retires"); ++m_pipeline_store_writes;
		});
	m_phase = 251810; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_original_context_call(unsigned variant, bool full_body)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = BIT(variant, 0) ? 0x20000 : 0;
	u16 const saved_status = 0x4200 | (BIT(variant, 1) ? 0x800 : 0) | (full_body && BIT(variant, 2) ? 0x100 : 0);
	std::array<u16, 51> const main{0xeeff, 0x4a06, 0x4a07, 0xf6be, 0xf6b9,
		0x4a08, 0x4a09, 0x4a0a, 0x4a0b, 0x4a0c, 0x4a0d, 0xea00, 0x577c, 0xf300, 0x0001, 0x4f7c, 0x487e,
		0xf030, 0xc004, 0xf050, 0xc004, 0xfa44, 0x3d9a, 0x467f, 0xf495,
		0x4a10, 0x4a12, 0x4a13, 0x4a0e, 0x7131, 0x0012, 0xf274, 0x3dbc, 0x770e, 0x0006,
		0x8a0e, 0x8a13, 0x8a12, 0x8a10, 0x1036, 0xf7be, 0x8a0d, 0x8a0c, 0x8a0b,
		0x8006, 0x8a0a, 0x8a09, 0x8a08, 0x8a07, 0x8a06, 0xfc00};
	std::array<u16, 9> const entry{0xfd00, 0x4a07, 0xfd00, 0xf7bb, 0x7192, 0x0010, 0x1092, 0xf844, 0x3ddb};
	std::array<u16, 7> const tail{0x8a08, 0xf030, 0x0800, 0xf495, 0xfe00, 0xfd45, 0xf6bb};
	std::array<u16, 22> const body{0x6b82, 2, 0x4b92, 0x7182, 0x0013, 0x8b93, 0x8393, 0x8193, 0xf4ab, 0x8c93,
		0x108a, 0x6d8a, 0xfd30, 0x8082, 0x6dea, 3, 0xf000, 4, 0x1892, 0x1a8a, 0x6d8a, 0x8082};
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 144; ++i) program.write_word(base + 0x3d73 + i, 0xf495);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x4044 + i, 0xf495);
	for (unsigned i = 0; i < main.size(); ++i) program.write_word(base + 0x3d73 + i, main[i]);
	for (unsigned i = 0; i < entry.size(); ++i) program.write_word(base + 0x3dbc + i, entry[i]);
	if (full_body) for (unsigned i = 0; i < body.size(); ++i) program.write_word(base + 0x3dc5 + i, body[i]);
	for (unsigned i = 0; i < tail.size(); ++i) program.write_word(base + 0x3ddb + i, tail[i]);
	program.write_word(base + 0x3de2, 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa61b);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, saved_status);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1357);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x2468);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x369c);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x8888);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0xff0; i <= 0x1000; ++i) data.write_word(i, 0xaaaa);
	data.write_word(0x7c, 0x1357); data.write_word(0x7d, 0x2468);
	data.write_word(0x7e, 0xc004); data.write_word(0x7f, 0xe012);
	data.write_word(0x931, 0x510); data.write_word(0x936, 0x4044);
	data.write_word(0x510, full_body ? BIT(variant, 3) ? 0x472 : 0x474 : 0xbeef); data.write_word(0x511, full_body ? 0 : 1);
	if (full_body)
	{
		data.write_word(0x512, 0xff0f); data.write_word(0x513, 0x470);
		data.write_word(0x514, 0xff0f); data.write_word(0x515, 0x00f0);
		for (unsigned i = 0; i < 4; ++i) data.write_word(0x470 + i, 0x7777);
	}
	m_cpu->set_state_int(STATE_GENPC, base + 0x3d73); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_drained_fetches = m_vector_reads = 0;
	m_pipeline_write_tap = data.install_write_tap(full_body ? 0xff0 : 0xff1, 0xfff, "original_context_call_stack",
		[this, saved_status, full_body](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 16> const expected{0xa61b, saved_status, 0x789a, 0x3456, 0x12, 0x5566, 0x3344, 0x22,
				0x1357, 0x2468, 0x369c, 0x8888, 0x3d96, u16(saved_status & 0x900), full_body ? u16(0xff11) : u16(0x4044), 0x4044};
			unsigned const final = full_body ? 15 : 14;
			expect(m_pipeline_store_writes <= final && address == (m_pipeline_store_writes < final ? 0xffe - m_pipeline_store_writes : 0xfff) &&
				word == expected[m_pipeline_store_writes], "Original context call route owns all nested saves and reserved return write"); ++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = data.install_read_tap(0xfff, 0xfff, "original_context_call_return",
		[this](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0x4044, "Original call route reads the reserved return after restoring context"); ++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x4044, base + 0x4044, "original_context_call_continuation",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			++m_pipeline_extensions_reads;
		});
	if (full_body)
	{
		bool const matched = BIT(variant, 3);
		m_vector_stack_tap = data.install_write_tap(0x470, 0x515, "full_context_body_writes",
			[this, matched](offs_t address, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				std::array<u16, 7> const addresses{0x512, 0x470, 0x471, 0x472, 0x473, 0x511, 0x513};
				std::array<u16, 7> const values{0xff11, 0xff11, 0x1357, 0x2469, 6, 0x470, 0x4f4};
				unsigned const index = m_pipeline_drained_fetches + unsigned(!matched && m_pipeline_drained_fetches >= 5);
				expect(index < addresses.size() && address == addresses[index] && value == values[index],
					"Outer context body publishes its counter-derived B words and retained source/store effects once");
				++m_pipeline_drained_fetches;
			});
		m_vector_prefetch_tap = data.install_read_tap(0x510, 0x515, "full_context_body_reads",
			[this, matched](offs_t address, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				std::array<u16, 7> const addresses{0x510, 0x511, 0x512, 0x513, 0x513, 0x514, 0x515};
				std::array<u16, 7> const values{u16(matched ? 0x472 : 0x474), 0, 0xff0f, 0x470, 0x470, 0xff0f, 0x00f0};
				expect(m_vector_reads < addresses.size() && address == addresses[m_vector_reads] && value == values[m_vector_reads],
					"Outer context body consumes each source in original order without duplicate replay reads");
				++m_vector_reads;
			});
	}
	m_phase = full_body ? 252070 : 251850; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_retd_data_order(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const prefix = BIT(variant, 1) ? 2 : 1; u16 const target = prefix == 1 ? 0x720 : 0x700;
	u32 const base = BIT(variant, 0) ? 0x10000 : 0; auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); program.write_word(base + 0x720 + i, 0xf495); }
	program.write_word(base + 0x600, 0x4a11); program.write_word(base + 0x600 + prefix, 0xfe00);
	program.write_word(base + 0x603 + prefix, 0xffff);
	program.write_word(base + 0x700, 0xe955); program.write_word(base + 0x720, 0xe999);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x700);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0xfff, 0x720);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xfff, 0xfff, "retd_read_first",
		[this, prefix, target](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == target && m_cpu->pipeline_cycles() == 5 + prefix, "RETD reads old DARAM before same-clock push; one NOP exposes completed write"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xfff, 0xfff, "retd_push_write",
		[this, prefix](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0x700 && m_cpu->pipeline_cycles() == 6 && m_pipeline_operand_reads == unsigned(prefix == 1), "RETD source Read is ordered before concurrent push Write"); ++m_pipeline_store_writes;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + target, base + target, "retd_order_target",
		[this, prefix](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7 + prefix, "RETD retains its sampled old/new target across delay release"); ++m_pipeline_extensions_reads;
		});
	m_phase = 251910; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_cmpr(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const sequence = variant >= 432;
	bool const producer = variant >= 384 && !sequence;
	unsigned const kind = producer ? (variant - 384) / 12 : 0;
	unsigned const gap = producer ? ((variant - 384) / 3) % 4 : 0;
	unsigned const ar = producer ? 3 : (variant / 3) % 8;
	unsigned const cc = producer ? 0 : (variant / 24) % 4;
	unsigned const pair = producer || sequence ? 0 : variant / 96;
	std::array<u16, 4> const left{0, 0, 0xffff, 0x8000}, right{0, 0xffff, 0, 0x7fff};
	u32 const base = BIT(variant, 0) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	unsigned prefix = 0;
	if (producer)
	{
		if (kind == 0 || kind == 1)
		{
			program.write_word(base + 0x600, kind == 0 ? 0x7713 : 0x7170);
			program.write_word(base + 0x601, kind == 0 ? 0xbeef : 0x0013);
			prefix = 2;
		}
		else { program.write_word(base + 0x600, kind == 2 ? 0x8a13 : 0x8013); prefix = 1; }
	}
	program.write_word(base + 0x600 + prefix + gap, 0xf4a8 | (cc << 8) | ar);
	if (sequence)
	{
		// Original R060 02:3dcb..3dce, without a substitute NOP.
		std::array<u16, 4> const words{0x8393, 0x8193, 0xf4ab, 0x8c93};
		for (unsigned i = 0; i < words.size(); ++i) program.write_word(base + 0x600 + i, words[i]);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, (producer ? 0xa600 : 0xa608) | (BIT(variant, 0) ? 0x1000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 6);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, sequence ? 0x471 : producer ? 0xbeef : right[pair]);
	if (ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, producer ? 0x1234 : left[pair]);
	if (sequence) m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x470);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0x70, 0xbeef);
	m_cpu->space(AS_DATA).write_word(0x1000, 0xbeef);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_store_writes = 0;
	if (sequence)
		m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x470, 0x472, "cmpr_original_stores",
			[this](offs_t address, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				expect(m_pipeline_store_writes < 3 && address == 0x470 + m_pipeline_store_writes &&
					value == std::array<u16, 3>{0x3344, 0x5566, 6}[m_pipeline_store_writes] &&
					m_cpu->pipeline_cycles() == std::array<unsigned, 3>{6, 7, 9}[m_pipeline_store_writes],
					"Original store/CMPR sequence retains all three writes and pointer updates");
				++m_pipeline_store_writes;
			});
	m_phase = 251970; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mar(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const sequence = variant >= 288;
	unsigned const form = sequence ? 0 : variant / 48;
	unsigned const ar = sequence ? 2 : (variant / 6) % 8;
	bool const compatibility = !sequence && BIT(variant / 3, 0);
	unsigned const selected = compatibility && !ar ? 4 : ar;
	u16 const initial = sequence ? 0x470 : 0x400 + selected * 0x10;
	s16 const step = std::array<s16, 6>{0, -1, 1, 3, -1, -32768}[form];
	u16 const destination = initial + step;
	u32 const base = BIT(variant, 0) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	if (sequence)
	{
		// Contiguous original R060 02:3dcf..3dd4, both XC outcomes.
		std::array<u16, 6> const words{0x108a, 0x6d8a, 0xfd30, 0x8082, 0x6dea, 3};
		for (unsigned i = 0; i < words.size(); ++i) program.write_word(base + 0x600 + i, words[i]);
	}
	else
	{
		program.write_word(base + 0x600, 0x6d00 | std::array<u16, 6>{0x80, 0x88, 0x90, 0xe8, 0xe8, 0xe8}[form] | ar);
		if (form >= 3) program.write_word(base + 0x601, u16(step));
		program.write_word(base + 0x600 + (form >= 3 ? 2 : 1), 0x1080 | ar);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8608 | (variant >= 291 ? 0x1000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (compatibility ? 0x20 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x400 + i * 0x10);
	if (sequence) m_cpu->set_state_int(tms320c54x_device::STATE_AR2, initial);
	m_cpu->space(AS_DATA).write_word(sequence ? initial : destination, 0xcafe);
	if (sequence) m_cpu->space(AS_DATA).write_word(0x46e, 0x7777);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_extensions_reads = 0;
	if (sequence || form >= 3)
		m_pipeline_fetch_tap = program.install_read_tap(base + 0x600 + (sequence ? 5 : 1), base + 0x600 + (sequence ? 5 : 1), "mar_offset_fetch",
			[this, sequence, step](offs_t, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				expect(value == u16(sequence ? 3 : step) && m_cpu->pipeline_cycles() == (sequence ? 7 : 3),
					"MAR offset is fetched once through its retained program-word slot");
				++m_pipeline_extensions_reads;
			});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x100, 0xffff, "mar_following_operand",
		[this, sequence, destination, form](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == (sequence ? 0x470 : destination) && value == 0xcafe &&
				m_cpu->pipeline_cycles() == (sequence ? 5 : form >= 3 ? 7 : 6),
				"MAR does not read memory; following LD reads its captured updated pointer once");
			++m_pipeline_operand_reads;
		});
	if (sequence)
		m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x46e, 0x46e, "mar_original_conditional_store",
			[this, variant](offs_t, u16 &value, u16) {
				if (machine().side_effects_disabled()) return;
				expect(variant >= 291 && value == 0xcafe && m_cpu->pipeline_cycles() == 9,
					"Original XC accepts or rejects the store while long MAR retains its own offset");
				++m_pipeline_store_writes;
			});
	m_phase = 251990; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mu4_full_worker(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const matched = BIT(variant, 0), masked = BIT(variant, 1), sxm = BIT(variant, 2);
	u16 const saved_status = (masked ? 0x800 : 0) | (sxm ? 0x100 : 0);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 96; ++i) program.write_word(0x23d90 + i, 0xf495);
	std::array<u16, 4> const call{0xf274, 0x3dbc, 0x770e, 6};
	std::array<u16, 38> const worker{
		0xfd00, 0x4a07, 0xfd00, 0xf7bb, 0x7192, 0x0010, 0x1092, 0xf844, 0x3ddb,
		0x6b82, 2, 0x4b92, 0x7182, 0x0013, 0x8b93, 0x8393, 0x8193, 0xf4ab, 0x8c93,
		0x108a, 0x6d8a, 0xfd30, 0x8082, 0x6dea, 3, 0xf000, 4, 0x1892, 0x1a8a, 0x6d8a, 0x8082,
		0x8a08, 0xf030, 0x0800, 0xf495, 0xfe00, 0xfd45, 0xf6bb};
	for (unsigned i = 0; i < call.size(); ++i) program.write_word(0x23d92 + i, call[i]);
	for (unsigned i = 0; i < worker.size(); ++i) program.write_word(0x23dbc + i, worker[i]);
	program.write_word(0x23de2, 0xffff); program.write_word(0x23d96, 0xe955);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, saved_status);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1111);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x510);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x2222);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	auto &data = m_cpu->space(AS_DATA);
	data.write_word(0x510, matched ? 0x472 : 0x474); data.write_word(0x511, 0);
	data.write_word(0x512, 0xff0f); data.write_word(0x513, 0x470);
	data.write_word(0x514, 0xff0f); data.write_word(0x515, 0x00f0);
	for (unsigned i = 0; i < 4; ++i) data.write_word(0x470 + i, 0x7777);
	m_cpu->set_state_int(STATE_GENPC, 0x23d92); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0x400, 0x1000, "full_worker_reads",
		[this, matched, saved_status](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 10> const addresses{0x510, 0x511, 0x512, 0x513, 0xffd, 0x513, 0x514, 0x515, 0xffe, 0xfff};
			std::array<u16, 10> const values{u16(matched ? 0x472 : 0x474), 0, 0xff0f, 0x470, 0xff11, 0x470, 0xff0f, 0x00f0, saved_status, 0x3d96};
			std::array<unsigned, 10> const clocks{13, 15, 20, 22, 24, 29, 37, 38, 41, 45};
			unsigned const index = m_pipeline_operand_reads;
			if (index >= addresses.size() || address != addresses[index] || value != values[index] || m_cpu->pipeline_cycles() != clocks[index])
				osd_printf_info("Full worker read mismatch: index=%u cycle=%llu address=%04x value=%04x\n", index, (unsigned long long)m_cpu->pipeline_cycles(), unsigned(address), value);
			expect(index < addresses.size() && address == addresses[index] && value == values[index] && m_cpu->pipeline_cycles() == clocks[index],
				"Complete original worker retains each source, forwarded RMW word and return slot in order");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = data.install_write_tap(0x400, 0x1000, "full_worker_writes",
		[this, matched, saved_status](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 10> const addresses{0xfff, 0xffe, 0x512, 0xffd, 0x470, 0x471, 0x472, 0x473, 0x511, 0x513};
			std::array<u16, 10> const values{0x3d96, saved_status, 0xff11, 0xff11, 0xff11, 0x3344, 0x5566, 6, 0x470, 0x4f4};
			std::array<unsigned, 10> const clocks{6, 11, 21, 22, 25, 26, 27, 29, 33, 41};
			unsigned const index = m_pipeline_store_writes + unsigned(!matched && m_pipeline_store_writes >= 8);
			if (index >= addresses.size() || address != addresses[index] || value != values[index] || m_cpu->pipeline_cycles() != clocks[index])
				osd_printf_info("Full worker write mismatch: index=%u cycle=%llu address=%04x value=%04x\n", index, (unsigned long long)m_cpu->pipeline_cycles(), unsigned(address), value);
			expect(index < addresses.size() && address == addresses[index] && value == values[index] && m_cpu->pipeline_cycles() == clocks[index],
				"Complete worker publishes stack, copied word, B/T and conditional/final stores once");
			++m_pipeline_store_writes;
		});
	m_pipeline_fetch_tap = program.install_read_tap(0x23d96, 0x23de2, "full_worker_route",
		[this](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled() || (address != 0x23d96 && address != 0x23de2)) return;
			expect(address == 0x23d96 && m_cpu->pipeline_cycles() == 47, "Complete worker returns on page 2 without a third RETD delay word");
			++m_pipeline_extensions_reads;
		});
	m_phase = 252050; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_far_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const target_page = std::array<unsigned, 5>{0, 1, 2, 3, 0x7f}[variant / 8];
	u32 const source = BIT(variant, 0) ? 0x20000 : 0, target = target_page << 16;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(source + 0x600 + i, 0xf495); program.write_word(target + 0x700 + i, 0xf495); }
	program.write_word(source + 0x600, 0xf880 | target_page); program.write_word(source + 0x601, 0x700);
	program.write_word(source + 0x602, 0xe811); program.write_word(source + 0x603, 0xe922);
	program.write_word(target + 0x700, 0xe844);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, source + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(source + 0x601, source + 0x601, "far_branch_target_literal",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x700 && m_cpu->pipeline_cycles() == 3, "FB retains its target literal from one program Fetch");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(target + 0x700, target + 0x700, "far_branch_target_page",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0xe844 && m_cpu->pipeline_cycles() == 6, "FB redirects to its retained seven-bit page without a stack write");
			++m_pipeline_extensions_reads;
		});
	m_phase = 252090; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_far_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const page = std::array<unsigned, 5>{0, 1, 2, 3, 0x7f}[variant / 16];
	u32 const source = BIT(variant, 0) ? 0x20000 : 0, target = page << 16;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(source + 0x600 + i, 0xf495); program.write_word(target + 0x700 + i, 0xf495); }
	program.write_word(source + 0x600, 0xf980 | page); program.write_word(source + 0x601, 0x700);
	program.write_word(source + 0x602, 0xe844); program.write_word(source + 0x603, 0xe811);
	program.write_word(target + 0x700, 0xe955); program.write_word(target + 0x701, 0xf4e4); program.write_word(target + 0x702, 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_RTN, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, source + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_drained_fetches = m_fast_stack_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xffe, 0xfff, "far_call_stack_pair",
		[this, source](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_pipeline_store_writes < 2 && address == 0xfff - m_pipeline_store_writes &&
				value == (m_pipeline_store_writes ? source >> 16 : 0x602) && m_cpu->pipeline_cycles() == 6 + m_pipeline_store_writes,
				"FCALL writes PC then retained caller XPC on separate clocks");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xffe, 0xfff, "far_call_return_pair",
		[this, source](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_fast_stack_reads < 2 && address == 0xffe + m_fast_stack_reads &&
				value == (m_fast_stack_reads ? 0x602 : source >> 16) && m_cpu->pipeline_cycles() == 10 + m_fast_stack_reads,
				"FRET reads its saved caller page then PC without re-reading either operand");
			++m_fast_stack_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(target + 0x700, target + 0x700, "far_call_target",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 6, "FCALL target Fetch has class-29B branch timing"); ++m_pipeline_extensions_reads;
		});
	m_vector_prefetch_tap = program.install_read_tap(source + 0x601, source + 0x602, "far_call_literal_and_continuation",
		[this, source](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == source + 0x601) { expect(m_cpu->pipeline_cycles() == 3, "FCALL target literal is fetched once"); ++m_pipeline_operand_reads; }
			else { expect(m_cpu->pipeline_cycles() == (m_pipeline_drained_fetches ? 13 : 4), "Caller continuation is first flushed, then refetched after FRET"); ++m_pipeline_drained_fetches; }
		});
	m_phase = 252170; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_explicit_sp(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const kind = variant % 5, gap = (variant / 5) % 4;
	bool const store = BIT(variant / 20, 0);
	u32 const base = BIT(variant / 40, 0) ? 0x20000 : 0;
	unsigned const words = kind == 1 || kind == 2 || kind == 4 ? 2 : 1;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	if (kind == 0) program.write_word(base + 0x600, 0x8a18);
	if (kind == 1) { program.write_word(base + 0x600, 0x7718); program.write_word(base + 0x601, 0x1020); }
	if (kind == 2) { program.write_word(base + 0x600, 0x7182); program.write_word(base + 0x601, 0x0018); }
	if (kind == 3) program.write_word(base + 0x600, 0xee20);
	if (kind == 4) { program.write_word(base + 0x600, 0x8081); program.write_word(base + 0x601, 0x8a18); }
	program.write_word(base + 0x600 + words + gap, store ? 0x8020 : 0x1020);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x7777);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x11);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x70);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	auto &data = m_cpu->space(AS_DATA);
	data.write_word(0x70, 0x1020); data.write_word(0x1000, 0x1020);
	data.write_word(0x1020, 0x1111); data.write_word(0x1040, 0x2222);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	unsigned const access = 4 + words + gap;
	bool const new_sp = gap >= (kind == 0 ? 2 : kind == 4 ? 3 : 1);
	u16 const address = new_sp ? 0x1040 : 0x1020;
	m_pipeline_operand_tap = data.install_read_tap(0x70, 0x1040, "explicit_sp_sources",
		[this, kind, store, access, address, new_sp](offs_t at, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			bool const source = at == (kind == 2 ? 0x70 : 0x1000);
			expect(source ? (kind == 0 || kind == 2 || kind == 4) && value == 0x1020 && m_cpu->pipeline_cycles() == (kind == 4 ? 6 : 5) :
				!store && at == address && value == (new_sp ? 0x2222 : 0x1111) && m_cpu->pipeline_cycles() == access + 1,
				"Explicit SP source and basic compiler operand use their documented separate visibility");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = data.install_write_tap(0x18, 0x1040, "explicit_sp_publications",
		[this, kind, store, access, address](offs_t at, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (at == 0x18)
			{
				expect(kind != 3 && value == 0x1020 && m_cpu->pipeline_cycles() == (kind == 4 ? 7 : 5), "Explicit SP publishes once; only DAGEN conflict defers POPM");
				++m_pipeline_register_writes;
			}
			else
			{
				expect(store && at == address && value == 0x7777 && m_cpu->pipeline_cycles() == access + 2, "Compiler store retains its old or new SP-based address");
				++m_pipeline_store_writes;
			}
		});
	m_phase = 252190; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_pmst(unsigned variant)
{
	bool const popd = !strcmp(machine().options().bios(), "pmst-popd");
	bool const repeat = !strcmp(machine().options().bios(), "pop-repeat");
	bool const iptr = !strcmp(machine().options().bios(), "pmst-iptr");
	bool const boundary = !strcmp(machine().options().bios(), "pmst-prefetch");
	bool const pop = popd || repeat || iptr || boundary || !strcmp(machine().options().bios(), "pmst-pop");
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_pipeline_peripheral_case = variant;
	u16 const value = (repeat ? std::array<u16, 4>{0, 1, 0x8000, 0xffff} : iptr ? std::array<u16, 4>{0, 0x20, 0x2020, 0x3020} : std::array<u16, 4>{0, 0x20, 0x2020, 0xffaa})[variant % 4];
	u16 const destination = repeat ? 0x1a + variant / 64 : 0x1d;
	u32 const base = (BIT(variant, 2) ? 3 : 2) << 16;
	u16 const target = BIT(variant, 3) ? 0x8700 : 0x700;
	u16 const entry = boundary ? 0x600 : 0x9000;
	auto &program = m_cpu->space(AS_PROGRAM);
	if (boundary)
		for (unsigned i = 0; i < 64; ++i) { program.write_word(base + entry + i, 0xf495); program.write_word(entry + i, 0xf495); }
	for (unsigned i = 0; i < 64; ++i) { program.write_word(base + 0x9000 + i, 0xf495); program.write_word(base + target + i, 0xf495); program.write_word(target + i, 0xf495); }
	std::array<u16, 5> const code{0x771d, value, 0xf495, 0xf073, target};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(base + 0x9000 + i, code[i]);
	if (pop)
	{
		std::array<u16, 6> const pop_code{u16(popd ? 0x8b87 : 0x8a00 | destination), 0xf495, 0xf495, 0xf495, 0xf073, target};
		for (unsigned i = 0; i < pop_code.size(); ++i)
		{
			program.write_word(base + entry + i, pop_code[i]);
			if (boundary) program.write_word(entry + i, pop_code[i]);
		}
		if (boundary)
		{
			// Deliberate debugger-state conformance input, not a firmware fixture.
			// Only the bank not selected by the first Prefetch has a bad opcode.
			program.write_word(BIT(value ^ 0x20, 5) ? base + entry : entry, 0xffff);
		}
		m_cpu->space(AS_DATA).write_word(0x1000, value);
	}
	program.write_word(base + target, 0xe844); program.write_word(target, 0xe811);
	if (iptr)
	{
		u16 const offset = (16 + variant / 64) * 4;
		u16 const old_vector = ((value ^ 0x100) & 0xff80) | offset;
		u16 const new_vector = (value & 0xff80) | offset;
		for (unsigned i = 0; i < 32; ++i)
		{
			program.write_word(base + old_vector + i, 0xf495); program.write_word(old_vector + i, 0xf495);
			program.write_word(base + new_vector + i, 0xf495); program.write_word(new_vector + i, 0xf495);
		}
		program.write_word(base + old_vector, 0xe966); program.write_word(old_vector, 0xe966);
		program.write_word(base + new_vector, 0xe955); program.write_word(new_vector, 0xe955);
		for (unsigned line = 0; line < 16; ++line) m_cpu->set_input_line(line, CLEAR_LINE);
		m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, repeat ? 0 : value ^ (iptr ? 0x120 : 0x20));
	if (repeat)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 0x1234);
		m_cpu->set_state_int(tms320c54x_device::STATE_RSA, 0x5678);
		m_cpu->set_state_int(tms320c54x_device::STATE_REA, 0x9abc);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, iptr ? 0 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	if (popd) m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x1d);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, iptr ? u16(1U << (variant / 64)) : 0);
	if (iptr) m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(STATE_GENPC, base + entry); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_store_writes = m_pipeline_operand_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(destination, destination, "staged_control_pop_publication",
		[this, value, pop](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == value && m_cpu->pipeline_cycles() == (pop ? 6 : 7) && !m_pipeline_store_writes,
				"STM PMST publishes its retained value once, separate from DAGEN early writes"); ++m_pipeline_store_writes;
		});
	m_pipeline_fetch_tap = (pop ? m_cpu->space(AS_DATA) : program).install_read_tap(pop ? 0x1000 : base + 0x9001, pop ? 0x1000 : base + 0x9001, "staged_pmst_source",
		[this, value](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == value && !m_pipeline_operand_reads, "STM PMST fetches its original literal exactly once"); ++m_pipeline_operand_reads;
		});
	m_phase = 252270; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_literal_guard(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const destination = std::array<u16, 3>{0x7cf8, 0x7df8, 0x7ff8}[variant % 3];
	unsigned const count = (variant / 3) % 3;
	u32 const base = (variant / 9) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	std::array<u16, 6> const code{0xf072, 0x604, 0x76f8, destination, 0x1357, 0xf4e1};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(base + 0x600 + i, code[i]);
	m_cpu->space(AS_DATA).write_word(destination, 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa624);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, count);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_pipeline_store_writes = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "literal_guard_store",
		[this, destination, count](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == destination && word == 0x1357 && m_pipeline_store_writes <= count, "Opcode-shaped extension remains the ST destination through block repetition"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = literal_guard_phase;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::repeated_writa_fixture tms320c54x_test_state::repeated_writa_case(unsigned variant, bool standard_width)
{
	repeated_writa_fixture f{};
	f.standard_width = standard_width;
	if (standard_width) variant = variant % 4800 + (variant / 4800) * 9600;
	f.form = variant % 5; f.setup = (variant / 5) % 5; f.reg = (variant / 25) % 8;
	f.count = std::array<unsigned, 4>{0, 1, 7, 255}[(variant / 200) % 4];
	f.target = (standard_width ? std::array<u32, 3>{0x3200, 0x7ffe, 0xfffe} : std::array<u32, 3>{0x033200, 0x037ffe, 0x7ffffe})[(variant / 800) % 3];
	f.mapped = (variant / 2400) % 2; f.page = (variant / 4800) % 2 ? 0x20000 : 0;
	f.words = f.setup == 0 || f.setup == 2 ? 1 : 2;
	f.read = 5 + f.words + (f.setup >= 2 ? 2 : 0); f.write = f.read + 5;
	f.checkpoint = std::array<unsigned, 4>{f.read, f.read + 1, f.write, f.write + 3}[variant / 9600];
	f.source = f.form < 2 ? 0x1201 : 0x3000; f.direction = f.form == 3 ? -1 : f.form == 4 ? 1 : 0;
	f.count_address = f.setup == 2 ? 0x1202 : f.setup == 3 ? 0x1400 : 0x10 + ((f.reg + 1) % 8);
	return f;
}

u16 tms320c54x_test_state::repeated_writa_word(unsigned reg, unsigned index)
{ return 0x2468 ^ (reg * 0x1111) ^ (index * 0x0101); }

u32 tms320c54x_test_state::repeated_writa_destination(repeated_writa_fixture const &f, unsigned index)
{
	if (f.standard_width) return (f.target + index) & 0xffff;
	u32 const logical = (f.target + index) & 0x7fffff;
	return f.mapped && !(logical & 0x8000) ? logical & 0xffff : logical;
}

void tms320c54x_test_state::start_repeated_writa(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const standard_width = !strcmp(machine().options().bios(), "rpt-writa16");
	auto const f = repeated_writa_case(variant, standard_width); m_cpu->set_extended_program(!standard_width);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.page + 0x9000 + i, 0xf495);
	program.write_word(f.page + 0x9000, f.setup == 0 ? 0xec00 | f.count : f.setup == 1 ? 0xf070 : f.setup == 2 ? 0x4702 : 0x47f8);
	if (f.words == 2) program.write_word(f.page + 0x9001, f.setup == 1 ? f.count : f.count_address);
	program.write_word(f.page + 0x9000 + f.words, 0x7f00 | (f.form < 2 ? 1 : (f.form == 2 ? 0x80 : f.form == 3 ? 0x88 : 0x90) | f.reg));
	program.write_word(f.page + 0x9001 + f.words, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa824); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900 | (f.form == 1 ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.mapped ? 0x20 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff00000000ULL | f.target); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x81abcdef01ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1200); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, f.form >= 2 && ar == f.reg ? f.source : 0x4000 + ar);
	if (f.setup == 4) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ((f.reg + 1) % 8), f.count);
	else if (f.setup >= 2) data.write_word(f.count_address, f.count);
	data.write_word(f.source, repeated_writa_word(f.reg, 0));
	m_pipeline_operand_reads = m_pipeline_store_writes = m_port_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "repeat_writa_reads",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (f.setup >= 2 && at == f.count_address) { expect(!m_port_reads && word == f.count, "WRITA memory-repeat setup samples its count once"); ++m_port_reads; return; }
			unsigned const n = m_pipeline_operand_reads;
			expect(n <= f.count && n == m_pipeline_store_writes && at == u16(f.source + f.direction * int(n)) && word == repeated_writa_word(f.reg, n) && m_cpu->pipeline_cycles() == (n ? f.write + n : f.read), "Repeated WRITA reads each fresh source once, including its retained first operand"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "repeat_writa_no_data_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "WRITA writes program memory, never data memory"); });
	m_pipeline_fetch_tap = program.install_write_tap(0, 0x7fffff, "repeat_writa_program_write",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const n = m_pipeline_store_writes;
			expect(n <= f.count && m_pipeline_operand_reads == n + 1 && at == repeated_writa_destination(f, n) && word == repeated_writa_word(f.reg, n) && m_cpu->pipeline_cycles() == f.write + n, "WRITA advances retained logical PAR and maps each one-clock stream write correctly"); ++m_pipeline_store_writes;
			if (n < f.count) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(u16(f.source + f.direction * int(n + 1)), repeated_writa_word(f.reg, n + 1)); }
		});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = repeated_writa_phase;
	m_cpu->set_state_int(STATE_GENPC, f.page + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delayed_condition_return_fixture tms320c54x_test_state::delayed_condition_return_case(unsigned v)
{
	delayed_condition_return_fixture f{};
	f.condition = v % 14; unsigned const pattern = (v / 14) % 8;
	f.layout = (v / 112) % 2; f.sp = 0x1000 + (v / 224) % 2; f.base = (v / 448) % 2 ? 0x20000 : 0;
	f.checkpoint = std::array<unsigned, 5>{3, 4, 5, 6, 8}[v / 896];
	u64 const raw = std::array<u64, 8>{0, 1, 0xffffffffffULL, 0x7fffffff, 0x80000000, 0xff80000000ULL, 0x7fffffffffULL, 0x8000000000ULL}[pattern];
	f.a = f.condition < 6 ? raw : 0x12345678; f.b = f.condition >= 6 && f.condition < 12 ? raw : 0x23456789;
	f.opcode = f.condition >= 12 ? (f.condition == 13 ? 0xfe30 : 0xfe20) : 0xfe42 + f.condition % 6 + (f.condition >= 6 ? 8 : 0);
	f.st0 = 0xa608 | (pattern & 1 ? 0x1000 : 0);
	if (f.condition >= 12) f.taken = bool(pattern & 1) == (f.condition == 13);
	else
	{
		s64 const value = s64(raw << 24) >> 24;
		switch (f.condition % 6) { case 0: f.taken = value >= 0; break; case 1: f.taken = value < 0; break; case 2: f.taken = value != 0; break; case 3: f.taken = value == 0; break; case 4: f.taken = value > 0; break; default: f.taken = value <= 0; break; }
	}
	f.final_a = f.layout ? f.a : 0; f.final_b = f.layout || f.condition >= 12 ? f.b : 0;
	f.final_st0 = !f.layout && f.condition >= 12 ? f.st0 ^ 0x1000 : f.st0;
	return f;
}

void tms320c54x_test_state::start_delayed_condition_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = delayed_condition_return_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); }
	program.write_word(f.base + 0x600, f.opcode);
	program.write_word(f.base + 0x601, f.layout ? 0x7712 : f.condition >= 12 ? (f.st0 & 0x1000 ? 0xf4bc : 0xf5bc) : 0xe800);
	program.write_word(f.base + 0x602, f.layout ? 0xf4e1 : f.condition >= 12 ? 0xe800 : 0xe900);
	for (u32 start : {f.base + 0x603, f.base + 0x700})
	{
		program.write_word(start, 0x80f8); program.write_word(start + 1, 0x1004); program.write_word(start + 2, 0x81f8); program.write_word(start + 3, 0x1005);
		program.write_word(start + 4, start == f.base + 0x700 ? 0xe955 : 0xe933); program.write_word(start + 5, 0xf4e1);
	}
	data.write_word(f.sp, 0x700); data.write_word(0x1004, 0xdead); data.write_word(0x1005, 0xdead);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.sp); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1234);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = data.install_read_tap(f.sp, f.sp, "rcd_stack", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(f.taken && !m_pipeline_operand_reads && word == 0x700, "Only a taken RCD samples its stack target once"); ++m_pipeline_operand_reads; auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.sp, 0x710); });
	m_pipeline_write_tap = data.install_write_tap(0x1004, 0x1005, "rcd_delay_observation", [this, f](offs_t address, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(address == 0x1004 + m_pipeline_store_writes && word == u16(m_pipeline_store_writes ? f.final_b : f.final_a), "RCD executes its delay instructions once before the selected continuation"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x700, "rcd_fetch", [this, f](offs_t address, u16 &, u16) { if (machine().side_effects_disabled()) return; if (address == f.base + 0x601 || address == f.base + 0x602) { unsigned const bit = 1U << (address - f.base - 0x601); expect(!(m_pipeline_extensions_reads & bit), "RCD retains each delay word without refetch including opcode-shaped literals"); m_pipeline_extensions_reads |= bit; } if (address == f.base + 0x603 || address == f.base + 0x700) { expect(address == f.base + (f.taken ? 0x700 : 0x603) && !m_pipeline_drained_fetches, "RCD fetches only its selected continuation once"); ++m_pipeline_drained_fetches; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 276000;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_tc_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const condition = variant % 2, tc = (variant / 2) % 2, producer = (variant / 4) % 3, gap = (variant / 12) % 2, selector = (variant / 24) % 2 ? 15 : 0;
	unsigned const words = producer == 2 ? 2 : producer;
	u32 const base = (variant / 48) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x9000 + i, 0xf495);
	if (producer) { program.write_word(base + 0x9000, producer == 2 ? 0x34f8 : 0x3401); if (producer == 2) program.write_word(base + 0x9001, 0x06d1); }
	program.write_word(base + 0x9000 + words + gap, condition ? 0xfc30 : 0xfc20);
	program.write_word(base + 0x9001 + words + gap, 0xf4e1); program.write_word(base + 0x9100, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x2000, 0x9100);
	u16 const value = tc ? u16(1U << (15 - selector)) : 0;
	m_cpu->space(AS_DATA).write_word(producer == 2 ? 0x06d1 : 0x1201, value);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa824 | ((producer ? !tc : tc) ? 0x1000 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0xb00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2000); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xa5f0 | selector);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x81abcdef01ULL);
	m_pipeline_operand_reads = m_fast_stack_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "tc_return_reads",
		[this, producer, value, tc, condition](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == 0x2000) { expect(tc == condition && !m_fast_stack_reads && word == 0x9100, "Only a taken TC/NTC return reads its stack target once"); ++m_fast_stack_reads; }
			else { expect(producer && !m_pipeline_operand_reads && at == (producer == 2 ? 0x06d1 : 0x1201) && word == value, "TC return consumes its organic BITT producer once"); ++m_pipeline_operand_reads; } });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "tc_return_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Conditional return never writes data memory"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = tc_return_phase;
	m_cpu->set_state_int(STATE_GENPC, base + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::bd_read_a_fixture tms320c54x_test_state::bd_read_a_case(unsigned v)
{
	bd_read_a_fixture f{};
	f.slot=v%2; v/=2; f.form=v%4; v/=4; f.pattern=v%4; v/=4;
	f.base=v%2 ? 0x20000 : 0; v/=2; f.source=(v%2 ? 0x20000 : 0)+0x2700; v/=2; f.snapshot=v;
	return f;
}

void tms320c54x_test_state::start_bd_read_a(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case=variant;
	auto const f=bd_read_a_case(variant); auto &program=m_cpu->space(AS_PROGRAM); auto &data=m_cpu->space(AS_DATA);
	for(unsigned i=0;i<16;++i) program.write_word(f.base+0x600+i,0xf495);
	program.write_word(f.base+0x600,0xf273); program.write_word(f.base+0x601,0x700); program.write_word(f.base+0x700,0xf4e1);
	if(f.slot) program.write_word(f.base+0x602,0x0084);
	program.write_word(f.base+0x602+f.slot,0x7e00|(f.form==0 || f.form==3 ? 0x20 : f.form==1 ? 0x83 : 0x93));
	program.write_word(f.source,f.value()); data.write_word(0x4020,0xdead); data.write_word(0x5100,5);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0,0x1080); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,0x900|(f.form==3 ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.source-5*f.slot); m_cpu->set_state_int(tms320c54x_device::STATE_B,0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T,0x5aa5);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x4000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3,0x4020); m_cpu->set_state_int(tms320c54x_device::STATE_AR4,0x5100);
	m_pipeline_operand_reads=m_pipeline_store_writes=m_port_reads=0;
	m_pipeline_fetch_tap=program.install_read_tap(f.source,f.source,"bd_read_a_source",[this,f](offs_t,u16 &word,u16) { if(machine().side_effects_disabled()) return; expect(word==f.value() && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles()==f.read()+4,"BD READA samples its retained program address once after the four held clocks"); auto const disable=machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.source,u16(~f.value())); });
	m_pipeline_operand_tap=data.install_read_tap(0,0xffff,"bd_read_a_data",[this,f](offs_t at,u16 &word,u16) { if(machine().side_effects_disabled()) return; expect(f.slot && at==0x5100 && word==5 && !m_port_reads++,"The first delay word supplies READA's address through ordinary ADD"); });
	m_pipeline_write_tap=data.install_write_tap(0,0xffff,"bd_read_a_write",[this,f](offs_t at,u16 &word,u16) { if(machine().side_effects_disabled()) return; expect(at==0x4020 && word==f.value() && !m_pipeline_store_writes++ && m_cpu->pipeline_cycles()==f.read()+5,"BD READA commits one retained program word to its original RAM destination"); });
	m_pipeline_replay=false; m_idle_fixture_tick=0; m_phase=bd_read_a_phase;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

void tms320c54x_test_state::start_rptz(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const target = variant % 2, count = std::array<unsigned, 4>{0, 1, 3, 17}[(variant / 2) % 4];
	u32 const base = (variant / 128) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 12; ++i) program.write_word(base + 0x9000 + i, 0xf495);
	program.write_word(base + 0x9000, 0xf071 | (target << 8)); program.write_word(base + 0x9001, count);
	program.write_word(base + 0x9002, 0x7693); program.write_word(base + 0x9003, 0x2468); program.write_word(base + 0x9004, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa024 | (((variant / 8) % 4) << 11));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (((variant / 32) % 4) << 8));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x81abcdef01ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1200);
	m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "rptz_writes",
		[this, count](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_store_writes <= count && at == 0x1200 + m_pipeline_store_writes && word == 0x2468, "RPTZ issues exactly count-plus-one ordered real stores"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x9001, base + 0x9001, "rptz_literal",
		[this, count](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == count && !m_pipeline_extensions_reads, "RPTZ retains its count literal once"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = rptz_phase;
	m_cpu->set_state_int(STATE_GENPC, base + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_sp_rmw(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const initial = std::array<u16, 4>{0x1200, 0xfffe, 0, 0x8000}[variant % 4];
	u16 const delta = std::array<u16, 4>{0x3ff, 1, 0xffff, 0x8000}[(variant / 4) % 4];
	u16 const sum = initial + delta;
	u32 const base = BIT(variant, 5) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 40; ++i) program.write_word(base + 0x600 + i, 0xf495);
	std::array<u16, 8> const words{0x7718, initial, 0x6bf8, 0x18, delta, 0x68f8, 0x18, 0xfffe};
	for (unsigned i = 0; i < words.size(); ++i) program.write_word(base + 0x600 + i, words[i]);
	program.write_word(base + 0x60b, 0x4918);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (BIT(variant, 4) ? 0x100 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x4000);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x18, 0x18, "sp_rmw_sources",
		[this, initial, sum](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 3> const values{initial, sum, u16(sum & 0xfffe)};
			std::array<unsigned, 3> const cycles{9, 12, 16};
			expect(m_pipeline_operand_reads < 3 && value == values[m_pipeline_operand_reads] && m_cpu->pipeline_cycles() == cycles[m_pipeline_operand_reads],
				"Original SP setup reads the published STM value, then ADDM result, then aligned SP once each"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x18, 0x18, "sp_rmw_publications",
		[this, initial, sum](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			std::array<u16, 3> const values{initial, sum, u16(sum & 0xfffe)};
			std::array<unsigned, 3> const cycles{5, 10, 13};
			expect(m_pipeline_store_writes < 3 && value == values[m_pipeline_store_writes] && m_cpu->pipeline_cycles() == cycles[m_pipeline_store_writes],
				"STM, absolute ADDM and ANDM own distinct SP publications with unsigned 16-bit wrap"); ++m_pipeline_store_writes;
		});
	m_phase = 252330; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_arp(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const arp = variant % 8, encoded = BIT(variant, 3) ? 2 : 0;
	bool const compatibility = BIT(variant, 4), advance = BIT(variant, 5);
	unsigned const selected = compatibility && !encoded ? arp : encoded;
	u32 const base = BIT(variant, 6) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf4a0 | arp);
	program.write_word(base + 0x601, 0x1000 | (advance ? 0x90 : 0x80) | encoded);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (compatibility ? 0x20 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 8; ++i)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x420 + i * 0x10);
		m_cpu->space(AS_DATA).write_word(0x420 + i * 0x10, 0x1100 + i);
	}
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x420, 0x490, "arp_selected_source",
		[this, selected](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(!m_pipeline_operand_reads && at == 0x420 + selected * 0x10 && word == 0x1100 + selected && m_cpu->pipeline_cycles() == 6,
				"Immediate ARP has zero latency to the following compatibility consumer; encoded registers still select themselves"); ++m_pipeline_operand_reads;
		});
	m_phase = 252350; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_read_a(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const ram = variant >= 192;
	unsigned const page = std::array<unsigned, 3>{0, 2, 0x7f}[variant % 3];
	bool const upper = (variant / 3) % 2, ovly = (variant / 6) % 2;
	unsigned const gap = (variant / 12) % 4;
	u32 const logical = (page << 16) | (upper ? 0x8700 : 0x700);
	u32 const physical = ovly && !upper ? logical & 0xffff : logical;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 48; ++i) program.write_word(0x19000 + i, 0xf495);
	std::array<u16, 4> const code{0xf000, 1, 0x7ef8, 0x11};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(0x19000 + i, code[i]);
	program.write_word(0x19004 + gap, 0x1081);
	if (ram)
	{
		program.write_word(0x19002, 0x7e92); program.write_word(0x19003, 0xf495);
		for (unsigned i = 4; i < 8; ++i) program.write_word(0x19000 + i, i == 5 ? 0x1083 : 0xf495);
	}
	program.write_word(physical, 0x440); program.write_word(physical + 1, 0x450);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, ovly ? 0x20 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x410);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x420);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x420);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, logical - 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0x410, 0x1111); m_cpu->space(AS_DATA).write_word(0x440, 0x4444);
	m_cpu->space(AS_DATA).write_word(0x420, 0xffff);
	m_cpu->set_state_int(STATE_GENPC, 0x19000); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = program.install_read_tap(physical, physical + 1, "read_a_accumulator_source",
		[this, physical, ram](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(at == physical && word == 0x440 && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == (ram ? 11 : 12),
				"READA samples the preceding ADD result and retains the full accumulator-selected program address through its held bus clocks"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(ram ? 0x420 : 0x11, ram ? 0x420 : 0x11, "read_a_publication",
		[this, ram](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0x440 && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (ram ? 12 : 13), "READA publishes its sampled word once, not an early DAGEN write"); ++m_pipeline_store_writes;
		});
	m_vector_prefetch_tap = m_cpu->space(AS_DATA).install_read_tap(0x410, 0x440, "read_a_ar1_consumer",
		[this, gap, ram](offs_t at, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(at == (ram ? 0x420 : gap >= 2 ? 0x440 : 0x410) && word == (ram ? 0x440 : gap >= 2 ? 0x4444 : 0x1111) && !m_pipeline_drained_fetches &&
				m_cpu->pipeline_cycles() == (ram ? 14 : 13 + gap), "READA's publication respects the younger indirect address and data Read ownership"); ++m_pipeline_drained_fetches;
		});
	m_phase = 252370; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_banz(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const value = std::array<u16, 3>{0, 1, 0xffff}[variant % 3];
	unsigned const modification = (variant / 3) % 3, encoded = (variant / 9) % 2;
	bool const compatibility = (variant / 18) % 2;
	unsigned const selected = compatibility && !encoded ? 3 : encoded;
	u32 const base = (variant / 36) % 2 ? 0x20000 : 0;
	u16 const opcode = 0x6c00 | (modification == 0 ? 0x80 : modification == 1 ? 0x88 : 0x90) | encoded;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, opcode); program.write_word(base + 0x601, 0x700);
	program.write_word(base + 0x602, 0xe811); program.write_word(base + 0x603, 0xe922);
	program.write_word(base + 0x700, 0xe844); program.write_word(base + 0x740, 0xe866);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, compatibility ? 0x6608 : 0x608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (compatibility ? 0x20 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == selected ? value : 0x410 + i * 0x10);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "banz_no_data_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BANZ tests its AR without reading data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "banz_no_data_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BANZ modifies its AR without a data-memory write"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "banz_literal",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == 0x700 && !m_pipeline_extensions_reads, "BANZ retains its single target literal"); ++m_pipeline_extensions_reads; });
	m_vector_prefetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "banz_target",
		[this, value](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(value && word == 0xe844 && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 6, "Taken BANZ fetches its retained same-page target once"); ++m_pipeline_operand_reads; });
	m_phase = 252390; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_instruction_observer(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const xc = variant >= 32, two = xc && BIT(variant - 32, 5);
	bool const staged = BIT(variant, 0), overlay = BIT(variant, 2);
	u32 const base = BIT(variant, 1) ? 0x20000 : 0, physical = overlay ? 0 : base;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 48; ++i) program.write_word(physical + 0x600 + i, 0xf495);
	std::array<u16, 10> const code{0xec02, 0xf495, 0xf073, 0x0608, 0xe966, 0xf495, 0xf495, 0xf495, 0xe977, 0xf4e1};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(physical + 0x600 + i, code[i]);
	if (xc)
	{
		std::array<u16, 7> const conditional{u16(two ? 0xff45 : 0xfd45), 0xe966, u16(two ? 0xe955 : 0xf495),
			u16(two ? 0xff44 : 0xfd44), 0xe977, u16(two ? 0xf495 : 0xf4e1), 0xf4e1};
		for (unsigned i = 0; i < conditional.size(); ++i) program.write_word(physical + 0x600 + i, conditional[i]);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, overlay ? 0x20 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, xc ? 1 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_observed_instructions.clear(); m_saved_observed_instructions.clear();
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(staged);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 292000;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_add_sp_context(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const copy = !strcmp(machine().options().bios(), "mvkd-sp");
	unsigned const producer = variant % 4, gap = std::array<unsigned, 3>{0, 1, 3}[(variant / 4) % 3];
	u16 const initial_sp = BIT(variant / 12, 0) ? 0x9000 : 0x1000;
	bool const compiler = BIT(variant / 24, 0), b = BIT(variant / 96, 0);
	u32 const base = BIT(variant / 48, 0) ? 0x20000 : 0;
	u16 const operand = initial_sp + std::array<int, 4>{-4, -1, 1, -8}[producer];
	unsigned const read_tick = 6 + (producer == 3 ? 2 : 1) + gap;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 48; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, std::array<u16, 4>{0xeefc, 0x4a11, 0x8a11, 0x7718}[producer]);
	if (producer == 3) program.write_word(base + 0x601, operand);
	unsigned const add = 0x600 + (producer == 3 ? 2 : 1) + gap;
	program.write_word(base + add, copy ? 0x7092 : b ? 0x01f8 : 0x00f8); program.write_word(base + add + 1, 0x18);
	// A younger FRAME must not change the older instruction's sampled SP.
	program.write_word(base + add + 2, 0xee02);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa008);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900 | (compiler ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, initial_sp);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1357);
	if (copy) m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1400);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, b ? 0x2233445566ULL : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, b ? 0 : 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(initial_sp, 0xbeef);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x18, 0x18, "add_sp_context_operand",
		[this, operand, read_tick](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == operand && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == read_tick,
				"Mapped SP ADD reads the older stack update once before the younger FRAME"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x60, 0xffff, "mapped_sp_context_writes",
		[this, copy, initial_sp, operand](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (copy && at == 0x1400) { expect(word == operand && !m_pipeline_extensions_reads, "MVKD retains one SP publication after an older stack update"); ++m_pipeline_extensions_reads; }
			else { expect(at == u16(initial_sp - 1) && word == 0x1357 && !m_pipeline_store_writes, "PSHM retains one ordered stack write"); ++m_pipeline_store_writes; } });
	m_pipeline_extensions_reads = 0;
	m_phase = 291000; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_add_sp_exclusion(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const copy = !strcmp(machine().options().bios(), "mvkd-sp");
	bool const destination_edge = copy && variant >= 20;
	u16 const source = destination_edge ? 0x18 : std::array<u16, 10>{0, 1, 6, 7, 0x1a, 0x1b, 0x1c, 0x1d, 0x24, 0x58}[variant % 10];
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, copy ? destination_edge ? 0x70f8 : variant >= 10 ? 0x7212 : 0x7092 : variant >= 10 ? 0x01f8 : 0x00f8);
	program.write_word(0x601, destination_edge ? std::array<u16, 5>{0, 0x18, 0x19, 0x1a, 0x1f}[variant - 20] : source);
	if (destination_edge) program.write_word(0x602, source);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa008);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	if (copy) { m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1400); m_cpu->space(AS_DATA).write_word(0x1400, 0xaaaa); }
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_pipeline_operand_reads = m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(source, source, "add_sp_excluded_source",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true);
	m_phase = 291020; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_direct_rmw(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const operation = variant % 3, offset = (variant / 3) % 2 ? 0x20 : 0;
	u16 const initial = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 6) % 4];
	u16 const immediate = (variant / 24) % 2 ? 0x00ff : 0xffff;
	bool const compiler = (variant / 48) % 2;
	u32 const base = (variant / 96) % 2 ? 0x20000 : 0;
	u16 const target = (compiler ? 0x1000 : 0x400) + offset;
	u16 const opcode = std::array<u16, 3>{0x6800, 0x6900, 0x6b00}[operation] | offset;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, opcode); program.write_word(base + 0x601, immediate);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900 | (compiler ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(target, initial);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x400, 0x1040, "direct_rmw_source",
		[this, target, initial](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == target && word == initial && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 6,
			"Direct RMW retains Decode CPL/DP and Access SP instead of live context at Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x400, 0x1040, "direct_rmw_destination",
		[this, target, initial, immediate, operation](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			u16 const expected = operation == 0 ? initial & immediate : operation == 1 ? initial | immediate : u16(initial + immediate);
			expect(at == target && word == expected && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 7,
				"Direct RMW writes its owned address and sampled operand once after context/RAM changes"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "direct_rmw_immediate",
		[this, immediate](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == immediate && !m_pipeline_extensions_reads,
			"Direct RMW retains its immediate without refetch"); ++m_pipeline_extensions_reads; });
	m_phase = 252450; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mmr_move(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	unsigned const source = variant % 9, destination = (variant / 9) % 9;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xf495}[(variant / 81) % 4];
	u32 const base = (variant / 324) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xe700 | (source << 4) | destination);
	if (destination < 8) program.write_word(base + 0x601, 0x1080 | destination);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1111);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 9; ++i) m_cpu->space(AS_DATA).write_word(0x10 + i, 0x1000 + i);
	m_cpu->space(AS_DATA).write_word(0x10 + source, value);
	if (value >= 0x60) m_cpu->space(AS_DATA).write_word(value, 0x55aa);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "mvmm_decode_read",
		[this, source, destination, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (m_cpu->pipeline_cycles() == 3)
			{
				expect(at == 0x10 + source && word == value && !m_pipeline_operand_reads, "MVMM reads its selected MMR once in Decode"); ++m_pipeline_operand_reads;
			}
			else
			{
				expect(destination < 8 && at == value && word == (value < 0x60 ? 0 : 0x55aa) && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == 6,
					"An adjacent indirect consumer sees the Access-published AR with no gap"); ++m_pipeline_extensions_reads;
			}
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "mvmm_access_write",
		[this, destination, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == 0x10 + destination && word == value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 4,
				"MVMM writes its retained Decode sample once in Access"); ++m_pipeline_store_writes; });
	m_phase = 252690; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_repeat_nop(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const long_form = variant % 2;
	unsigned const count = std::array<unsigned, 5>{0, 1, 3, 32, 255}[(variant / 2) % 5], interrupt = (variant / 10) % 3;
	u32 const base = (variant / 30) % 2 ? 0x20000 : 0, body = base + (long_form ? 0x602 : 0x601), continuation = body + 1;
	unsigned const last = (long_form ? 8 : 7) + count;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, long_form ? 0xf070 : 0xec00 | count);
	if (long_form) program.write_word(base + 0x601, count);
	program.write_word(body, 0xf495); program.write_word(continuation, 0xe855); program.write_word(continuation + 1, 0xf4e1);
	if (interrupt)
	{
		// With OVLY clear, vectors inherit XPC even for NMI's low slot 4.
		u32 const vector = base + (interrupt == 2 ? 4 : 0x40);
		program.write_word(vector, 0x4a1e); program.write_word(vector + 1, 0xf495); program.write_word(vector + 2, 0xf4e5);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, interrupt == 1 ? 0 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, interrupt == 1 ? 1 : 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = program.install_read_tap(body, body, "repeat_nop_body",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == 0xf495 && !m_pipeline_operand_reads, "Repeated NOP body is fetched once, not reread from poisoned program memory"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(continuation, continuation, "repeat_nop_continuation",
		[this, last](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == (m_pipeline_extensions_reads ? 0xe866 : 0xe855) && (!m_pipeline_extensions_reads || m_cpu->pipeline_cycles() > last),
				"Repeat discards speculative continuation and refetches after its final NOP"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "repeat_nop_no_data_write",
		[this, interrupt, last](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(interrupt && m_cpu->pipeline_cycles() > last && at >= 0xffe && at <= 0xfff && m_pipeline_store_writes < 2,
				"Repeated NOP has no data write; only a later interrupt can write its stack"); ++m_pipeline_store_writes; });
	m_phase = 252750; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_long_move(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const compatibility = (variant / 4800) % 2;
	auto const f = long_accumulator_store_fixture((variant % 100) + (compatibility ? 6400 : 0));
	unsigned const destination_kind = (variant / 100) % 12;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 1200) % 4];
	u16 const destination = destination_kind < 10 ? 0x10 + destination_kind : destination_kind == 10 ? 0xf495 : f.address;
	u32 const base = f.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x7100 | u8(f.opcode));
	program.write_word(base + 0x601, f.literal); program.write_word(base + 0x602, destination);
	if (destination_kind < 8) program.write_word(base + 0x603, 0xf4a8 | destination_kind);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x1007);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, f.initial);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1200); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(f.address, value);
	if (destination_kind == 10) m_cpu->space(AS_DATA).write_word(destination, 0xdead);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "long_mvdk_source",
		[this, f, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 6,
				"Long MVDK samples its addressed source once before aliasing destination publication"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "long_mvdk_destination",
		[this, destination, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == destination && word == value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (destination < 0x60 ? 6 : 7),
				"Long MVDK writes once at its DAGEN/RAM publication clock"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x602, "long_mvdk_literals",
		[this, f, destination](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && (at & 0xffff) == 0x601 + m_pipeline_extensions_reads &&
				word == (m_pipeline_extensions_reads ? destination : f.literal), "Long MVDK retains source offset before destination literal"); ++m_pipeline_extensions_reads; });
	m_phase = 252830; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_original_long_move(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const value = variant % 2 ? 0x1000 : 0x1234;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(0x600 + i, 0xf495);
	std::array<u16, 6> const words{0x80e6, 0x0010, 0x71e6, 0x0011, 0x0012, 0xf4aa};
	for (unsigned i = 0; i < words.size(); ++i) program.write_word(0x600 + i, words[i]);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x33fa); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1122);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0x1010, 0xdead); m_cpu->space(AS_DATA).write_word(0x1011, value);
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "original_long_move_source",
		[this, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == 0x1011 && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 8,
				"Original MVDK offset 0011 reads after the preceding STL offset 0010 write"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "original_long_move_writes",
		[this, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_store_writes < 2 && (at == 0x1010 ? word == 0x33fa && m_cpu->pipeline_cycles() == 7 && !m_pipeline_store_writes :
				at == 0x12 && word == value && m_cpu->pipeline_cycles() == 8 && m_pipeline_store_writes == 1),
				"Original pair retires the STL and AR2 publication once at their owned clocks"); ++m_pipeline_store_writes; });
	m_phase = 252850; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_tc_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const carry = !strcmp(machine().options().bios(), "carry-branch");
	if (carry) variant = variant % 8 + 16 + ((variant / 8) % 3) * 32 + (variant / 24) * 96;
	u16 const flag = carry ? 0x0800 : 0x1000;
	bool const tc = variant % 2, delayed = (variant / 2) % 2, value = (variant / 4) % 2;
	unsigned const producer = (variant / 8) % 4, gap = (variant / 32) % 3;
	u32 const base = (variant / 96) % 2 ? 0x20000 : 0;
	bool const taken = value == tc;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, std::array<u16, 4>{0x9634, 0xf4aa, u16(value ? 0xf5bc : 0xf4bc), 0xf5aa}[producer]);
	if (carry) program.write_word(base + 0x600, value ? 0xf5bb : 0xf4bb);
	program.write_word(base + 0x601 + gap, (carry ? 0xf808 | (tc << 2) : 0xf820 | (tc << 4)) | (delayed << 9));
	program.write_word(base + 0x602 + gap, 0x700);
	program.write_word(base + 0x603 + gap, carry ? value ? 0xf4bb : 0xf5bb : value ? 0xf4bc : 0xf5bc); // Opposite flag after evaluation.
	program.write_word(base + 0x604 + gap, 0xe922);
	program.write_word(base + 0x605 + gap, 0xe811); program.write_word(base + 0x606 + gap, 0xf4e1);
	program.write_word(base + 0x700, 0xe855); program.write_word(base + 0x701, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608 | (value ? 0 : flag));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1200);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, producer == 3 ? (value ? 0x11ff : 0x1200) : value ? 0x1200 : 0x1201);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0x1000, value ? 0x0800 : 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = program.install_read_tap(base + 0x700, base + 0x700, "tc_branch_target",
		[this, taken, delayed, gap](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(taken && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == (delayed ? 9 : 8) + gap,
				"TC branch target is fetched once through its retained decision and conditional bubble"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x602 + gap, base + 0x602 + gap, "tc_branch_literal",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == 0x700 && !m_pipeline_extensions_reads, "TC branch retains its target literal once"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "tc_branch_bit_source",
		[this, producer, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(!producer && at == 0x1000 && word == (value ? 0x0800 : 0) && !m_pipeline_register_writes && m_cpu->pipeline_cycles() == 5,
				"Only the BIT producer has one memory sample before TC publication"); ++m_pipeline_register_writes; });
	m_operand_data_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "tc_branch_no_data_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "TC branch and its register producers do not write data memory"); });
	m_phase = 252870; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_long_rmw(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const peripheral = !strcmp(machine().options().bios(), "rmw-long-periph");
	bool const xorm = !strcmp(machine().options().bios(), "xorm-absolute");
	unsigned const operation = xorm ? 3 : variant % 3, arithmetic = (variant / (xorm ? 4 : peripheral ? 576 : 300)) % 64;
	bool const compatibility = (variant / (peripheral ? 288 : 19200)) % 2;
	auto const f = xorm ? absolute_xorm_fixture(variant) : peripheral ? peripheral_long_rmw_fixture(variant) : long_accumulator_store_fixture((variant / 3) % 100 + (compatibility ? 6400 : 0));
	u16 const initial_st0 = (f.st0 & ~0x0400) | (BIT(arithmetic, 2) << 10) | 0x0800;
	u16 const modified_st0 = compatibility && f.reg && f.kind != 3 ? (initial_st0 & 0x1fff) | (f.reg << 13) : initial_st0;
	auto const expected = long_rmw_expected(arithmetic, operation, modified_st0), input = addm_inputs(arithmetic);
	u32 const base = (peripheral ? bool((variant / 36864) % 2) : f.reg == 6) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, std::array<u16, 4>{0x6b00, 0x6800, 0x6900, 0x6a00}[operation] | u8(f.opcode));
	program.write_word(base + 0x601, f.literal); program.write_word(base + 0x602, input[1]);
	if (xorm) program.write_word(base + 0x603, 0x1080 | f.selected);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, initial_st0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1 | (BIT(arithmetic, 0) << 8) | (BIT(arithmetic, 1) << 9));
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, peripheral ? 0x5555 : 0x1007);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, peripheral ? u16(f.address - f.literal) : f.initial);
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(f.address, input[0]);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "long_rmw_source",
		[this, f, input](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == input[0] && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == (f.address >= 0x20 && f.address < 0x60 ? 8 : 7),
				"Long RMW retains one Read sample through its captured address, not the changed pointer"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "long_rmw_publication",
		[this, f, expected](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == expected[0] && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (f.address >= 0x20 && f.address < 0x60 ? 9 : 8) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[1], "Long RMW publishes its retained value and operation-owned flags once in Execute"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x602, "long_rmw_literals",
		[this, f, input](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && (at & 0xffff) == 0x601 + m_pipeline_extensions_reads &&
				word == (m_pipeline_extensions_reads ? input[1] : f.literal), "Long RMW retains the offset before the distinct arithmetic/mask literal"); ++m_pipeline_extensions_reads; });
	m_phase = 252890; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

u64 tms320c54x_test_state::signed_branch_source(unsigned variant)
{
	return std::array<u64, 8>{0, 1, 0xffffffffffULL, 0x7fffffff, 0xff80000000ULL, 0x0100000000ULL, 0x8000000000ULL, 0x7fffffffffULL}[(variant / 24) % 8];
}

bool tms320c54x_test_state::signed_branch_taken(unsigned variant)
{
	u64 const bits = signed_branch_source(variant);
	s64 const value = bits & (1ULL << 39) ? s64(bits) - (1LL << 40) : s64(bits);
	return std::array<bool, 6>{value >= 0, value < 0, value != 0, value == 0, value > 0, value <= 0}[(variant / 2) % 6];
}

void tms320c54x_test_state::start_pipeline_signed_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const b = variant % 2, delayed = (variant / 12) % 2, taken = signed_branch_taken(variant);
	unsigned const condition = (variant / 2) % 6, flags = (variant / 192) % 4;
	u32 const base = (variant / 1536) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, 0xf800 | (0x42 + condition) | (b << 3) | (delayed << 9));
	program.write_word(base + 0x601, 0x700);
	program.write_word(base + 0x602, b ? 0xe907 : 0xe807);
	program.write_word(base + 0x603, b ? 0xe822 : 0xe922);
	program.write_word(base + 0x604, b ? 0xe811 : 0xe911); program.write_word(base + 0x605, 0xf4e1);
	program.write_word(base + 0x700, b ? 0xe855 : 0xe955); program.write_word(base + 0x701, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | ((flags & 1) ? 0x100 : 0) | ((flags & 2) ? 0x200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, b ? 0 : signed_branch_source(variant));
	m_cpu->set_state_int(tms320c54x_device::STATE_B, b ? signed_branch_source(variant) : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(base + 0x700, base + 0x700, "signed_branch_target",
		[this, taken, delayed](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(taken && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == (delayed ? 8 : 7),
				"Signed branch target is fetched once from the full-width retained condition"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "signed_branch_literal",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == 0x700 && !m_pipeline_extensions_reads, "Signed branch retains the target literal once"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "signed_branch_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Signed branch has no data-memory write"); });
	m_operand_data_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "signed_branch_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Signed branch reads its live accumulator, not data memory"); });
	m_phase = 252970; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_signed_branch_producer(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const count = std::array<u16, 4>{0, 1, 2, 0xffff}[variant % 4];
	u32 const base = variant % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	std::array<u16, 7> const words{0x4811, 0x08e6, 0x0011, 0xf846, 0x0700, 0xe911, 0xf4e1};
	for (unsigned i = 0; i < words.size(); ++i) program.write_word(base + 0x600 + i, words[i]);
	program.write_word(base + 0x700, 0xe955); program.write_word(base + 0x701, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, count); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0x1011, 1);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "signed_branch_producer_reads",
		[this, count](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_operand_reads < 2 && (m_pipeline_operand_reads ? at == 0x1011 && word == 1 && m_cpu->pipeline_cycles() == 7 :
				at == 0x11 && word == count && m_cpu->pipeline_cycles() == 5), "LDM and long-offset SUB retire their ordered source samples before AGT evaluates"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "signed_branch_producer_target",
		[this, count](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(count > 1 && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == 10,
				"Original AGT family consumes the preceding SUB result with its retained redirect"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "signed_branch_producer_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "LDM/SUB/AGT producer chain does not write data memory"); });
	m_phase = 252990; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_extended_banz(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const delayed = !strcmp(machine().options().bios(), "banzd-long") && variant >= 2304, two_word = delayed && variant >= 4608;
	auto const f = extended_banz_case(variant % 2304);
	bool const compatibility = (variant / 192) % 2;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); }
	program.write_word(f.base + 0x600, (delayed ? 0x6e00 : 0x6c00) | (0xe0 + f.form * 8 + f.reg));
	program.write_word(f.base + 0x601, f.offset); program.write_word(f.base + 0x602, 0x700);
	program.write_word(f.base + 0x603, two_word ? 0xf020 : 0xe811); program.write_word(f.base + 0x604, two_word ? 0x33 : 0xe922);
	program.write_word(f.base + 0x700, 0xe844); program.write_word(f.base + 0x740, 0xe866);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, compatibility ? 0x6608 : 0x608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (compatibility ? 0x20 : 0));
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == f.selected ? f.initial : 0x410 + ar * 16);
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "extended_banz_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BANZ tests a resolved address without reading data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "extended_banz_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BANZ modifies its AR without a data-memory write"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x602, "extended_banz_literals",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && at == f.base + 0x601 + m_pipeline_extensions_reads &&
				word == (m_pipeline_extensions_reads ? 0x700 : f.offset), "BANZ retains offset then target, including opcode-shaped offsets"); ++m_pipeline_extensions_reads; });
	m_vector_prefetch_tap = program.install_read_tap(f.base + 0x700, f.base + 0x700, "extended_banz_target",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(f.resolved && word == 0xe844 && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 7,
				"Taken long-offset BANZ fetches its retained same-page target once"); ++m_pipeline_operand_reads; });
	m_phase = 253090; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::banzd_fixture tms320c54x_test_state::banzd_case(unsigned variant)
{
	unsigned const v = variant % 768, encoded = (v / 4) % 8, modification = (v / 32) % 3;
	bool const compatibility = (v / 96) % 2, two_word = (v / 384) % 2;
	u16 const value = std::array<u16, 4>{0, 1, 0xffff, 0x8000}[v % 4];
	return {encoded, compatibility && !encoded ? 3U : encoded, modification, compatibility, two_word,
		value, u16(value + (modification == 1 ? -1 : modification == 2 ? 1 : 0)), (v / 192) % 2 ? 0x20000U : 0};
}

void tms320c54x_test_state::start_pipeline_banzd(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = banzd_case(variant);
	u16 const opcode = 0x6e00 | (f.modification == 0 ? 0x80 : f.modification == 1 ? 0x88 : 0x90) | f.encoded;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); }
	program.write_word(f.base + 0x600, opcode); program.write_word(f.base + 0x601, 0x700);
	program.write_word(f.base + 0x602, f.two_word ? 0xf020 : 0xe811); program.write_word(f.base + 0x603, f.two_word ? 0x33 : 0xe922);
	program.write_word(f.base + 0x604, 0xe866); program.write_word(f.base + 0x605, 0xf4e1);
	program.write_word(f.base + 0x700, 0xe844); program.write_word(f.base + 0x701, 0xf4e1);
	program.write_word(f.base + 0x740, 0xe8aa);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.compatibility ? 0x6608 : 0x608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (f.compatibility ? 0x20 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == f.selected ? f.value : 0x410 + ar * 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "banzd_no_data_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BANZD and its register delay slots do not read data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "banzd_no_data_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BANZD and its register delay slots do not write data memory"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "banzd_target_literal",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == 0x700 && !m_pipeline_extensions_reads, "BANZD retains its target literal once"); ++m_pipeline_extensions_reads; });
	m_vector_prefetch_tap = program.install_read_tap(f.base + 0x700, f.base + 0x700, "banzd_target_fetch",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(f.value && word == 0xe844 && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 6, "Taken BANZD fetches the retained target while delay words drain"); ++m_pipeline_operand_reads; });
	m_phase = 253250; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_swap_body(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = variant & 256 ? 0x20000 : 0;
	u16 const flags = (variant >> 4) & 15, source = swap_body_source(variant);
	static constexpr u16 words[] = {0x1386, 0xf678, 0xf3e8, 0xf1a0, 0x8196, 0xf4e1};
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, i < std::size(words) ? words[i] : 0xf495);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, variant & 1); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xe08);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | ((flags & 1) ? 0x100 : 0) | ((flags & 2) ? 0x200 : 0) | ((flags & 4) ? 0x40 : 0) | ((flags & 8) ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xff80000000ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x5000); m_cpu->space(AS_DATA).write_word(0x5000, source);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "original_swap_body_read",
		[this, source](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == 0x5000 && word == source && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 5, "Original byte-swap body retains one unsigned Read sample"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "original_swap_body_write",
		[this, source](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == 0x5000 && word == u16((source << 8) | (source >> 8)) && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 10, "Original byte-swap body writes one transformed word in Execute"); ++m_pipeline_store_writes; });
	m_phase = 253310; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_swap_join(unsigned variant)
{
	start_pipeline_swap_body(variant % 512);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	u32 const base = variant & 256 ? 0x20000 : 0;
	u16 const first = swap_body_source(variant), second = first ^ 0x5a3c;
	static constexpr u16 words[] = {0x1386, 0xf678, 0xf3e8, 0xf1a0, 0x8196};
	for (unsigned i = 0; i < 10; ++i) m_cpu->space(AS_PROGRAM).write_word(base + 0x600 + i, words[i % 5]);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x60a, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x5001, second);
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "original_swap_join_read",
		[this, first, second](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const index = m_pipeline_operand_reads;
			if (index >= 2 || at != 0x5000 + index || word != (index ? second : first))
				osd_printf_info("Swap join read mismatch: index=%u address=%04x word=%04x cycle=%llu\n", index, unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles());
			expect(index < 2 && at == 0x5000 + index && word == (index ? second : first) && m_cpu->pipeline_cycles() == 5 + index * 5,
				"Consecutive original bodies read successive words without an artificial loop gap"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "original_swap_join_write",
		[this, first, second](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const index = m_pipeline_store_writes; u16 const source = index ? second : first;
			expect(index < 2 && at == 0x5000 + index && word == u16((source << 8) | (source >> 8)) && m_cpu->pipeline_cycles() == 10 + index * 5,
				"Consecutive original bodies write successive byte-swapped words once"); ++m_pipeline_store_writes; });
	m_phase = 253330; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

unsigned tms320c54x_test_state::rptb_count(unsigned variant)
{ return variant >= 6144 ? 2 : std::array<unsigned, 4>{0, 1, 2, 7}[variant & 3]; }

u16 tms320c54x_test_state::rptb_source(unsigned variant, unsigned index)
{ return swap_body_source(variant >= 6144 ? 8 : variant >> 2) ^ u16(index * 0x1357); }

void tms320c54x_test_state::start_pipeline_rptb(unsigned variant)
{
	bool const irq = variant >= 6144;
	unsigned const flags = irq ? 0 : (variant >> 7) & 15;
	u32 const base = (irq ? (variant - 6144) & 2 : variant & 64) ? 0x20000 : 0;
	start_pipeline_swap_body((flags << 4) | (base ? 256 : 0));
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	static constexpr u16 words[] = {0xf072, 0x0606, 0x1386, 0xf678, 0xf3e8, 0xf1a0, 0x8196, 0xf4e1};
	for (unsigned i = 0; i < 24; ++i) m_cpu->space(AS_PROGRAM).write_word(base + 0x600 + i, i < std::size(words) ? words[i] : 0xf495);
	for (unsigned i = 0; i <= rptb_count(variant); ++i) m_cpu->space(AS_DATA).write_word(0x5000 + i, rptb_source(variant, i));
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, rptb_count(variant));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	if (irq) m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, irq ? 4 : 0);
	u16 const vector = (variant - 6144) & 1 ? 0x0004 : 0x0048;
	m_cpu->space(AS_PROGRAM).write_word(base | vector, 0xf495); m_cpu->space(AS_PROGRAM).write_word((base | vector) + 1, 0xf4eb);
	m_pipeline_extensions_reads = 0;
	m_vector_prefetch_tap = m_cpu->space(AS_PROGRAM).install_read_tap(base | vector, base | vector, "rptb_vector",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_extensions_reads; });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x5000, 0x50ff, "rptb_body_read",
		[this, variant, irq](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const index = m_pipeline_operand_reads;
			expect(index <= rptb_count(variant) && at == 0x5000 + index && word == rptb_source(variant, index) &&
				(irq || m_cpu->pipeline_cycles() == 9 + index * 5), "RPTB reads each successive source once with zero-overhead body spacing"); ++m_pipeline_operand_reads;
			if (irq && index == 1) m_cpu->set_input_line((variant - 6144) & 1 ? INPUT_LINE_NMI : 2, ASSERT_LINE); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x5000, 0x50ff, "rptb_body_write",
		[this, variant, irq](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const index = m_pipeline_store_writes; u16 const source = rptb_source(variant, index);
			expect(index <= rptb_count(variant) && at == 0x5000 + index && word == u16((source << 8) | (source >> 8)) &&
				(irq || m_cpu->pipeline_cycles() == 14 + index * 5), "RPTB writes each transformed word once through interrupt and restore"); ++m_pipeline_store_writes; });
	m_phase = 253350; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

unsigned tms320c54x_test_state::rptbd_seed(unsigned v)
{ return v < 512 ? v : (v - 512) % 16 + (((v - 512) / 16) % 2) * 64; }

unsigned tms320c54x_test_state::rptbd_count(unsigned v)
{ return std::array<unsigned, 4>{0, 1, 2, 7}[rptbd_seed(v) % 4]; }

unsigned tms320c54x_test_state::rptbd_layout(unsigned v)
{ return (rptbd_seed(v) / 4) % 4; }

u32 tms320c54x_test_state::rptbd_base(unsigned v)
{ return (rptbd_seed(v) / 64) % 2 ? 0x20000 : 0; }

u16 tms320c54x_test_state::rptbd_source(unsigned v, unsigned i)
{ return std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(rptbd_seed(v) / 16) % 4] ^ u16(i * 0x1357); }

tms320c54x_test_state::basic_repeat_fixture tms320c54x_test_state::basic_repeat_case(unsigned v)
{
	unsigned const edge = v >= 10368 ? v - 10368 : 8;
	if (edge < 8) v = ((edge / 2) % 2) * 576 + (edge / 4) * 3456;
	basic_repeat_fixture f{};
	f.form = v % 36; v /= 36; f.count = std::array<unsigned, 4>{0, 1, 7, 255}[v % 4]; v /= 4;
	if (edge < 8) f.count = edge % 2 ? 0xffff : 0x8000;
	f.compiler = v % 2; v /= 2; f.compatibility = v % 2; v /= 2;
	f.base = v % 2 ? 0x20000 : 0; v /= 2; f.irq = v % 3; v /= 3;
	f.checkpoint = std::array<unsigned, 3>{3, 6, 12}[v];
	f.absolute = f.form >= 34;
	f.mode = f.form < 2 ? (f.form ? 0x7f : 0) : f.form < 26 ? 0x80 + f.form - 2 : f.form < 34 ? 0x10 + f.form - 26 : 0xf8;
	f.selected = f.mode & 7;
	if (f.compatibility && !f.selected) f.selected = 5;
	f.modification = f.form >= 2 && f.form < 26 ? (f.mode & 0x78) >> 3 : 0;
	f.st0 = 0xa600 | (f.form >= 26 && f.form < 34 ? 0 : 8);
	f.st1 = (f.compiler ? 0x4000 : 0) | (f.compatibility ? 0x20 : 0) | (f.irq ? 0 : 0x800);
	f.address = f.absolute ? (f.form == 35 ? 0x15 : 0x5000) : f.form >= 2 && f.form < 26 ? 0x5000 :
		f.compiler ? 0x3000 + f.mode : ((f.st0 & 0x1ff) << 7) | f.mode;
	f.body = f.base + (f.absolute ? 0x602 : 0x601);
	return f;
}

void tms320c54x_test_state::start_basic_repeat(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = basic_repeat_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x4700 | f.mode);
	if (f.absolute) program.write_word(f.base + 0x601, f.address);
	program.write_word(f.base + 0x60f, 0xf4e1);
	u32 const vector = f.base + (f.irq == 2 ? 4 : 0x48);
	program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	m_cpu->set_pipeline_frontend(false); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_RC, 0); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 13);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq == 1 ? 4 : 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.address == 0x10 + i ? f.count : 0x5000);
	if (f.address >= 0x60) data.write_word(f.address, f.count);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_observed_instructions.clear(); m_saved_observed_instructions.clear(); m_rptbd_counts = {}; m_rptbd_return = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_fetch_tap = program.install_read_tap(f.body, f.body, "basic_repeat_body", [this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == 0xf495 && !m_rptbd_counts.fetches++, "Memory-count repeat fetches its body once"); });
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "basic_repeat_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.irq && !m_rptbd_counts.stack_reads++ && word == m_rptbd_return, "Memory repeat interrupt consumes its retained continuation"); return; }
		expect(at == f.address && word == f.count && !m_rptbd_counts.reads++ && m_cpu->pipeline_cycles() == (f.absolute ? 6 : 5), "Memory repeat samples the count exactly once through the decoded address"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "basic_repeat_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.irq && at == 0x2fff && !m_rptbd_counts.stack_writes++ && word >= u16(f.body + 1) && word <= 0x60f, "Memory repeat writes only its optional post-body interrupt frame"); m_rptbd_return = word; });
	m_phase = 310440; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_basic_repeat_edge(unsigned variant)
{
	bool const write_only = variant >= 36; unsigned const edge = write_only ? variant - 36 : variant;
	unsigned const seed = write_only ? ((edge / 16) % 2) * 576 + ((edge / 8) % 2) * 288 : ((variant / 9) % 2) * 576;
	start_basic_repeat(seed); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	u32 const base = basic_repeat_case(seed).base; unsigned const kind = write_only ? 9 : variant % 9; auto &program = m_cpu->space(AS_PROGRAM);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, kind == 6 ? 0xa608 : 0xa600);
	program.write_word(base + 0x600, write_only ? 0x4798 | (edge % 8) : std::array<u16, 9>{0x4700, 0x4707, 0x471a, 0x4720, 0x4799, 0x47e3, 0x4700, 0xf273, 0xf072}[kind]);
	if (kind == 4) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1f);
	if (kind == 5) program.write_word(base + 0x601, 0);
	if (kind == 6) program.write_word(base + 0x601, 0x4700);
	if (kind == 7 || kind == 8) { program.write_word(base + 0x601, kind == 7 ? 0x700 : 0x603); program.write_word(base + 0x602, 0x4700); }
	m_rptbd_counts = {}; m_pipeline_replay = false;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "basic_repeat_edge_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd_counts.reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "basic_repeat_edge_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd_counts.writes; });
	m_phase = 310460; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

tms320c54x_test_state::rptbd_square_fixture tms320c54x_test_state::rptbd_square_case(unsigned v) const
{
	rptbd_square_fixture f{};
	f.memory_square = !strcmp(machine().options().bios(), "rptbd-squr");
	if (!f.memory_square) { f.operation = v % 2; v /= 2; }
	f.destination = v % 2; v /= 2; f.pattern = v % 8; v /= 8; f.flags = v % 16; v /= 16;
	unsigned const layouts = f.memory_square ? 5 : 4;
	f.layout = v % layouts; v /= layouts; f.form = v % 3; v /= 3; f.count = std::array<unsigned, 3>{0, 1, 7}[v % 3]; v /= 3;
	f.base = v % 2 ? 0x20000 : 0; v /= 2; f.snapshot = v;
	return f;
}

void tms320c54x_test_state::start_rptbd_square(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = rptbd_square_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, 0x605);
	program.write_word(f.base + 0x602, f.opcode(0)); program.write_word(f.base + 0x603, f.opcode(1));
	program.write_word(f.base + 0x604, 0x8092); program.write_word(f.base + 0x605, 0x8193); program.write_word(f.base + 0x606, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.flags & 4 ? 2 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0()); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1());
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.result(0)[0]); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.result(0)[1]);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x6000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x6100); m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x5000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	for (unsigned i = 0; i < f.operations(); ++i) data.write_word(f.address(i), f.memory(i));
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "rptbd_square_source", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_operand_reads++; expect(i < f.operations() && at == f.address(i) && word == f.memory(i) && m_cpu->pipeline_cycles() == 7 + f.slot(i), "RPTBD square delay samples each memory word once at Read"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "rptbd_square_body", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; bool const b = at >= 0x6100; unsigned &count = b ? m_pipeline_register_writes : m_pipeline_store_writes; expect(m_pipeline_operand_reads == f.operations() && count <= f.count && at == (b ? 0x6100 : 0x6000) + count && word == u16(f.result(f.operations())[b]), "RPTBD body sees completed square results on every iteration"); ++count; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x603, "rptbd_square_words", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_extensions_reads++; expect(i < 3 && at == f.base + 0x601 + i && word == (i ? f.opcode(i - 1) : 0x605), "RPTBD owns one endpoint and two non-repeated delay words"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62178;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_rptbd(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = rptbd_base(variant); unsigned const layout = rptbd_layout(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf272); program.write_word(base + 0x601, 0x606);
	program.write_word(base + 0x602, std::array<u16, 4>{0x6d95, 0x770e, 0x10e1, 0x6deb}[layout]);
	program.write_word(base + 0x603, std::array<u16, 4>{0x6d96, 0x1234, 3, 0x0a40}[layout]);
	program.write_word(base + 0x604, 0xe589); program.write_word(base + 0x605, 0x6d94);
	// Keep the IRQ fixture's terminal IDLE beyond the interrupted body;
	// this tests loop return, not the separately covered IDLE wake path.
	program.write_word(base + (variant >= 512 ? 0x60f : 0x607), 0xf4e1);
	if (variant >= 512)
	{
		u32 const vector = base + (rptbd_irq_line(variant) == INPUT_LINE_NMI ? 4 : 0x48);
		program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	}
	for (unsigned i = 0; i < 8; ++i) { data.write_word(0x5000 + i, rptbd_source(variant, i)); data.write_word(0x6000 + i, 0xdead); }
	data.write_word(0x4003, rptbd_source(variant, 0));
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	if (variant >= 512) { m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x100); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, rptbd_irq_line(variant) == 2 ? 4 : 0); }
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x4000); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, layout == 3 ? 0x55c0 : 0x6000); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x7000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x7100); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x7200);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, rptbd_count(variant));
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_rptbd_counts = {}; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x603, "rptbd_retained_words",
		[this, base, layout](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const i = m_rptbd_counts.fetches++;
			u16 const expected = i == 0 ? 0x606 : i == 1 ? std::array<u16, 4>{0x6d95, 0x770e, 0x10e1, 0x6deb}[layout] : std::array<u16, 4>{0x6d96, 0x1234, 3, 0x0a40}[layout];
			expect(i < 3 && at == base + 0x601 + i && word == expected, "RPTBD fetches endpoint and both delay words exactly once"); });
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "rptbd_ordered_reads",
		[this, variant, layout](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == 0x2fff && variant >= 512) { expect(!m_rptbd_counts.stack_reads && m_rptbd_counts.stack_writes == 1 && word == m_rptbd_return, "RPTBD ISR reads its original continuation frame once"); ++m_rptbd_counts.stack_reads; return; }
			if (at == 0x4003) { expect(layout == 2 && !m_rptbd_counts.delay_reads && word == rptbd_source(variant, 0), "RPTBD delay load samples its operand once"); ++m_rptbd_counts.delay_reads; return; }
			unsigned const i = m_rptbd_counts.reads++;
			expect(i <= rptbd_count(variant) && at == 0x5000 + i && word == rptbd_source(variant, i), "RPTBD repeats ordered source reads, excluding delay work"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "rptbd_ordered_writes",
		[this, variant, layout](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == 0x2fff && variant >= 512) { expect(!m_rptbd_counts.stack_writes && word >= 0x604 && word <= 0x60f, "RPTBD interrupt stacks one continuation after the complete delay span"); ++m_rptbd_counts.stack_writes; m_rptbd_return = word; return; }
			if (at == 0x0e) { expect(layout == 1 && !m_rptbd_counts.delay_writes && word == 0x1234, "RPTBD delay STM publishes T once"); ++m_rptbd_counts.delay_writes; return; }
			unsigned const i = m_rptbd_counts.writes++;
			expect(i <= rptbd_count(variant) && at == 0x6000 + i && word == rptbd_source(variant, i), "RPTBD repeats ordered destination writes without replay duplicates");
			if (!i && variant >= 512 && rptbd_late_irq(variant)) m_cpu->set_input_line(rptbd_irq_line(variant), ASSERT_LINE); });
	m_phase = 255320; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_rptbd_edge(unsigned variant)
{
	start_rptbd(2 + ((variant / 7) % 2) * 64);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	unsigned const kind = variant % 7; u32 const base = (variant / 7) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(base + 0x601, kind == 5 ? 0x605 : 0x606);
	program.write_word(base + 0x602, std::array<u16, 7>{0x771a, 0x881a, 0x8a1a, 0xf073, 0xf495, 0xf495, 0xf272}[kind]);
	program.write_word(base + 0x603, kind == 4 ? 0x10e1 : kind == 0 ? 1 : kind == 3 || kind == 6 ? 0x604 : 0xf495);
	program.write_word(base + 0x604, kind == 5 ? 0xf272 : kind == 4 ? 3 : 0xf495);
	program.write_word(base + 0x605, kind == 5 ? 0x606 : 0xf495);
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "rptbd_edge_no_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd_counts.reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "rptbd_edge_no_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd_counts.writes; });
	m_phase = 255340; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

void tms320c54x_test_state::start_pipeline_rptb_edge(unsigned variant)
{
	start_pipeline_rptb(variant & 8 ? 64 : 0);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_vector_prefetch_tap.remove();
	unsigned const kind = variant & 7; u32 const base = variant & 8 ? 0x20000 : 0;
	u16 const op = std::array<u16, 8>{0x6d96, 0xf020, 0xf272, 0xf073, 0x891a, 0xf020, 0x76f8, 0x8186}[kind];
	u16 const end = kind == 1 || kind == 2 || kind == 3 || kind == 6 ? 0x603 : 0x602;
	m_cpu->space(AS_PROGRAM).write_word(base + 0x600, 0xf072);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x601, end);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x602, op);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x603, kind == 1 || kind == 5 ? 0x1234 : kind == 6 ? 0x5000 : kind == 3 ? 0x602 : kind == 2 ? 0x604 : 0xf4e1);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x604, kind == 6 ? 0xbeef : 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR6, kind == 7 ? 0x1a : 0x5000);
	m_pipeline_extensions_reads = 0;
	m_pipeline_fetch_tap = m_cpu->space(AS_PROGRAM).install_read_tap(base + 0x603, base + 0x603, "rptb_edge_literal",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "rptb_edge_no_writes",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_phase = 253370; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

unsigned tms320c54x_test_state::block_branch_count(unsigned variant)
{ return std::array<unsigned, 4>{0, 1, 2, 7}[variant % 4]; }

u16 tms320c54x_test_state::block_branch_source(unsigned variant, unsigned i)
{
	unsigned const pattern = (variant / 4) % 4;
	return pattern == 0 ? 0 : pattern == 1 ? 1 : pattern == 2 ? i & 1 : i == block_branch_count(variant) ? 0 : 1;
}

tms320c54x_test_state::block_exit_fixture tms320c54x_test_state::block_exit_case(unsigned v, bool delayed)
{
	unsigned const forms = delayed ? 13 : 3;
	unsigned const count = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4], pattern = (v / 4) % 4, form = (v / 16) % forms;
	unsigned const exit = !form || !pattern ? 0 : pattern == 1 ? count + 1 : pattern == 2 ? count / 2 : count;
	bool const taken = exit <= count, endpoint_store = !((v / (16 * forms)) % 2);
	unsigned const body_writes = taken ? exit : count + 1;
	return {count, exit, taken ? exit + 1 : count + 1, body_writes,
		body_writes + unsigned(taken && endpoint_store), std::array<unsigned, 6>{3, 8, 9, 10, 12, 18}[v / ((delayed ? 128 : 64) * forms)], form,
		(v / (32 * forms)) % 2 ? 0x20000U : 0, u16((delayed ? 2 : 0) + (endpoint_store ? 0x606 : 0x609)), delayed ? form >= 7 : form == 2, taken, endpoint_store, delayed, delayed && (v / (64 * forms)) % 2};
}

void tms320c54x_test_state::start_block_exit(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = block_exit_case(variant, !strcmp(machine().options().bios(), "rptb-exit-delay")); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 0x120; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, f.end);
	program.write_word(f.base + 0x602, f.b ? 0x1192 : 0x1092);
	program.write_word(f.base + 0x603, f.delayed ? f.form ? 0xfa42 + ((f.form - 1) % 6) + (f.b ? 8 : 0) : 0xf273 : f.form ? f.b ? 0xf84d : 0xf845 : 0xf073);
	program.write_word(f.base + 0x604, 0x700);
	if (f.delayed)
	{
		program.write_word(f.base + 0x605 + unsigned(f.reverse_delay), f.b ? 0x1195 : 0x1095);
		program.write_word(f.base + 0x606 - unsigned(f.reverse_delay), f.b ? 0x8194 : 0x8094);
	}
	program.write_word(f.base + f.tail(), 0x7693); program.write_word(f.base + f.tail() + 1, 0x1111);
	program.write_word(f.base + f.end + 1, 0xf4e1);
	program.write_word(f.base + 0x700, 0x76f8); program.write_word(f.base + 0x701, 0x5200); program.write_word(f.base + 0x702, 0xcccc);
	program.write_word(f.base + 0x703, 0xf6bf);
	// SPRU131G 7.5.8.2: do not revisit the old endpoint until six
	// complete words after software BRAF clear. The branch adds more spacing.
	program.write_word(f.base + 0x70a, 0xf073); program.write_word(f.base + 0x70b, f.endpoint_store ? f.tail() : f.end);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2345); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x5100);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x5300); m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x5400);
	for (unsigned i = 0; i <= f.count + 1; ++i) { data.write_word(0x5000 + i, f.value(i == f.exit)); data.write_word(0x5100 + i, 0xaaaa); data.write_word(0x5400 + i, f.value(i != f.exit)); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = m_fast_stack_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_exit_read", [this, f](offs_t address, u16 &value, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.delayed && address >= 0x5400 && address <= 0x5408)
		{
			unsigned const i = m_fast_stack_reads++;
			expect(i < f.reads && address == 0x5400 + i && value == f.value(i != f.exit), "Each delayed exit consumes its opposite-condition input once"); return;
		}
		unsigned const i = m_pipeline_operand_reads++;
		expect(i < f.reads && address == 0x5000 + i && value == f.value(i == f.exit), "Block exit consumes each selected iteration once without speculative body reads");
		expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == f.count - i && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (i < f.count), "Block exit retains Decode-owned count and final-iteration BRAF");
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_exit_write", [this, f](offs_t address, u16 &value, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.delayed && address >= 0x5300 && address <= 0x5308)
		{
			unsigned const i = m_pipeline_extensions_reads++;
			expect(i < f.reads && address == 0x5300 + i && value == f.value(f.reverse_delay ? i == f.exit : i != f.exit), "Retained delay words change the tested accumulator after the decision"); return;
		}
		if (address == 0x5200)
		{
			expect(f.taken && !m_pipeline_register_writes && value == 0xcccc && m_pipeline_operand_reads == f.reads && m_pipeline_store_writes == f.body_writes &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == f.count - f.exit && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (f.exit < f.count), "Taken exit annuls its remaining body without automatically clearing BRAF or consuming BRC");
			++m_pipeline_register_writes; return;
		}
		unsigned const i = m_pipeline_store_writes++;
		expect(i < f.writes && address == 0x5100 + i && value == 0x1111 &&
			(i < f.body_writes || (m_pipeline_register_writes == 1 && !(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000))), "Only untaken iterations and one deliberate post-clear endpoint store may publish");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62064;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const interrupt = variant / 4096;
	bool const endpoint = (variant / 16) % 2, b = (variant / 32) % 2;
	u32 const base = (variant / 64) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf072); program.write_word(base + 0x601, endpoint ? 0x607 : 0x60d);
	program.write_word(base + 0x602, b ? 0x1192 : 0x1092);
	if (endpoint)
	{
		program.write_word(base + 0x603, 0x7693); program.write_word(base + 0x604, 0x1111);
		program.write_word(base + 0x605, b ? 0xf84d : 0xf845); program.write_word(base + 0x606, 0x607); program.write_word(base + 0x608, 0xf4e1);
	}
	else
	{
		program.write_word(base + 0x603, b ? 0xf84d : 0xf845); program.write_word(base + 0x604, 0x60a);
		program.write_word(base + 0x605, 0x7693); program.write_word(base + 0x606, 0x1111);
		program.write_word(base + 0x607, 0xf073); program.write_word(base + 0x608, 0x60c);
		program.write_word(base + 0x60a, 0x7693); program.write_word(base + 0x60b, 0x2222);
		program.write_word(base + 0x60c, b ? 0xf300 : 0xf000); program.write_word(base + 0x60d, 1);
	}
	// A later ordinary branch replaces the held tags before restoration,
	// so replay must recover the saved in-block tags rather than stale ones.
	unsigned const tail = endpoint ? 0x608 : 0x60e;
	program.write_word(base + tail, b ? 0xf84a : 0xf842); program.write_word(base + tail + 1, tail + 2); program.write_word(base + tail + 2, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, interrupt == 1 ? 0x100 : 0x900);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, interrupt == 1 ? 4 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	unsigned const vector = interrupt == 2 ? 4 : 0x48; program.write_word(base + vector, 0xf495); program.write_word(base + vector + 1, 0xf4eb);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2345); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x5100); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, block_branch_count(variant));
	for (unsigned i = 0; i <= block_branch_count(variant); ++i) { data.write_word(0x5000 + i, block_branch_source(variant, i)); data.write_word(0x5100 + i, 0xaaaa); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_branch_read", [this, variant, interrupt](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff) { expect(interrupt && !m_fast_stack_reads && m_pipeline_register_writes == 1 && word >= 0x602 && word <= 0x610, "Block branch RETE consumes its retained foreground continuation once"); ++m_fast_stack_reads; return; } unsigned const i = m_pipeline_operand_reads++; expect(i <= block_branch_count(variant) && at == 0x5000 + i && word == block_branch_source(variant, i), "Block branches consume exactly count-plus-one ordered source words"); expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == block_branch_count(variant) - i && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (i < block_branch_count(variant)), "Branch redirects preserve Decode-owned BRC and final-iteration BRAF"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_branch_write", [this, variant, endpoint, interrupt](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff) { expect(interrupt && !m_pipeline_register_writes && word >= 0x602 && word <= 0x610, "Block branch interrupt stacks one unexecuted foreground continuation"); ++m_pipeline_register_writes; return; } unsigned const i = m_pipeline_store_writes++; bool const valid = i <= block_branch_count(variant) && at == 0x5100 + i && word == (!endpoint && !block_branch_source(variant, i) ? 0x2222 : 0x1111); if (!valid) osd_printf_info("block_branch_write: case=%u replay=%u i=%u address=%04x word=%04x cycle=%llu\n", variant, unsigned(m_pipeline_replay), i, unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles()); expect(valid, "Only the selected block arm publishes once per iteration"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254360;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_branch_edge(unsigned variant)
{
	start_block_branch(variant & 8 ? 64 : 0);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	u32 const base = variant & 8 ? 0x20000 : 0;
	unsigned const kind = variant % 8;
	m_cpu->space(AS_PROGRAM).write_word(base + 0x603, std::array<u16, 8>{0xf073, 0xf073, 0xf273, 0xf845, 0xf845, 0xf945, 0xf072, 0xf880}[kind]);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x604, kind == 0 || kind == 3 ? 0x601 : kind == 1 || kind == 4 ? 0x700 : 0x60a);
	if (kind == 1 || kind == 4)
		m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0x604); // Branch extension at REA remains unvalidated.
	if (kind == 2)
	{
		// Internal BD is admitted, but its complete delay span must end
		// before REA; endpoint/control ownership is a separate contract.
		m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0x606);
		m_cpu->space(AS_PROGRAM).write_word(base + 0x604, 0x602);
	}
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "block_branch_edge_read", [this](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return; expect(at == 0x5000 && !m_pipeline_operand_reads, "Rejected block control permits only an older legal source read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "block_branch_edge_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254380;
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_square_case(unsigned variant)
{
	auto f = delay_accumulator_multiply_case(variant);
	f.square = true; f.destination = f.operation & 1; f.operation >>= 1;
	if (f.kind != 1 && f.kind != 2) f.a = f.b ^ 0x55;
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

tms320c54x_test_state::block_xc_fixture tms320c54x_test_state::block_xc_case(unsigned v)
{
	return {v % 17, (v / 17) % 4, std::array<unsigned, 4>{0, 1, 2, 7}[(v / 68) % 4], (v / 1088) % 4, v / 4352, bool((v / 272) % 2), (v / 544) % 2 ? 0x20000U : 0};
}

void tms320c54x_test_state::start_block_xc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = block_xc_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	u16 const end = f.endpoint ? (f.layout == 3 ? 0x606 : 0x607) : 0x60b;
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, end);
	program.write_word(f.base + 0x602, 0x1092 | (f.b() ? 0x100 : 0));
	u16 const predicate = f.predicate < 12 ? 0xfd42 + f.predicate % 6 + (f.b() ? 8 : 0) : f.predicate == 12 ? 0xfd20 : f.predicate == 13 ? 0xfd30 : f.predicate == 15 ? 0xfd0c : f.predicate == 16 ? 0xfd08 : 0xfd00;
	program.write_word(f.base + 0x605, predicate | (f.layout == 1 || f.layout == 2 ? 0x200 : 0));
	program.write_word(f.base + 0x606, f.layout == 2 ? 0x7693 : 0x8093 | (f.b() ? 0x100 : 0));
	program.write_word(f.base + 0x607, f.layout == 2 ? 0xfd43 : 0x8094 | (f.b() ? 0x100 : 0));
	if (f.layout == 2) program.write_word(f.base + 0x608, 0x8094 | (f.b() ? 0x100 : 0));
	program.write_word(f.base + end + 9, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608 | (f.rotation ? 0x1800 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x5100); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x5200);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	for (unsigned i = 0; i <= f.count; ++i) data.write_word(0x5000 + i, f.word(i));
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_xc_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_operand_reads++;
		expect(i <= f.count && at == 0x5000 + i && word == f.word(i) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == f.count - i && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (i < f.count), "Every XC iteration reads one source with its own counter and BRAF state");
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_xc_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x5100 && at < 0x5200) { unsigned const i = f.selected(m_pipeline_store_writes); expect(i <= f.count && at == 0x5100 + m_pipeline_store_writes++ && word == (f.layout == 2 ? 0xfd43 : f.word(i)), "Only selected first slots write in source order, including a captured opcode-shaped literal"); return; }
		unsigned const i = f.layout == 1 ? f.selected(m_pipeline_register_writes) : f.layout >= 2 && f.endpoint ? f.count : m_pipeline_register_writes;
		expect(i <= f.count && at == 0x5200 + m_pipeline_register_writes++ && word == f.word(i), "Second slots preserve XC1 fallthrough, XC2 selection and post-loop continuation");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255680;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_xc_edge(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = (variant / 3) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 12; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf072); program.write_word(base + 0x601, 0x602 + (variant % 3 == 2)); program.write_word(base + 0x602, variant % 3 ? 0xff00 : 0xfd00);
	program.write_word(base + 0x603, 0x8c93); program.write_word(base + 0x604, 0x8d94);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 3); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x5100); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x5200);
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_xc_edge_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Cross-end XC exclusion has no data reads"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_xc_edge_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Cross-end XC exclusion has no data writes"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255700;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_move_repeat_owner(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = variant & 2 ? 0x20000 : 0;
	u16 const value = variant & 4 ? 0x5aa5 : 0x1400, count = variant & 8 ? 3 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	unsigned cursor = 0x600;
	auto move = [&]() { program.write_word(base + cursor++, 0x710f); program.write_word(base + cursor++, 0x12); };
	auto repeat = [&]() { program.write_word(base + cursor++, variant & 16 ? 0xf070 : 0xec00 | count); if (variant & 16) program.write_word(base + cursor++, count); };
	if (!(variant & 1)) move();
	repeat();
	if (variant & 1) move(); else program.write_word(base + cursor++, 0xf495);
	program.write_word(base + cursor, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_TRN, value); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x3333);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 299000;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_delayed_call_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0109d0, index < 4 ? 0xfb44 : 0xfb76); program.write_word(0x0109d1, 0x09e0);
	program.write_word(0x0109d2, 0xe800); // ANEQ must use A before this delay word.
	program.write_word(0x0109d3, 0xe901);
	program.write_word(0x0109d4, 0xf4e1);
	program.write_word(0x0109e0, 0xe902); program.write_word(0x0109e1, 0xfc00);
	m_cpu->space(AS_DATA).write_word(0x0fff, 0xbeef);
	m_cpu->set_state_int(STATE_GENPC, 0x0109d0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0900);
	static constexpr u64 values[] = {0, 1, 0xffffffffffULL, 0x0100000000ULL, 0, 1, 0xffffffffffULL, 1, 0x0100000000ULL};
	m_cpu->set_state_int(tms320c54x_device::STATE_A, values[index]);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, index >= 4 && index != 7 ? 0x0400 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 3337 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_return_alt_case(unsigned index)
{
	static constexpr u64 values[] = {0xffffffffffULL, 0x8000000000ULL, 0, 1, 0x7fffffffffULL};
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0109d0, 0xfc43); program.write_word(0x0109d1, 0xf4e1);
	program.write_word(0x0109e0, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x1000, 0x09e0);
	m_cpu->set_state_int(STATE_GENPC, 0x0109d0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, values[index]);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 2696 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_carry_branch_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned cursor = 0x0109a0;
	program.write_word(cursor++, 0xf808 | ((index & 2) ? 4 : 0) | ((index & 4) ? 0x200 : 0));
	program.write_word(cursor++, 0x09b0);
	if (index & 4) { program.write_word(cursor++, 0xf495); program.write_word(cursor++, 0xf495); }
	program.write_word(cursor++, 0xf020); program.write_word(cursor++, 0x1111); program.write_word(cursor, 0xf4e1);
	program.write_word(0x0109b0, 0xf020); program.write_word(0x0109b1, 0x2222); program.write_word(0x0109b2, 0xf4e1);
	m_cpu->set_state_int(STATE_GENPC, 0x0109a0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x02a5 | ((index & 1) ? 0x800 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 1620 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_far_case(unsigned index)
{
	if (!strcmp(machine().options().bios(), "far-atomic"))
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
		m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	}
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	const u16 op = far_opcodes[index];
	const bool immediate = (op & 0xfc80) == 0xf880;
	const bool returning = (op & 0xfdfe) == 0xf4e4;
	const bool call = !returning && (immediate ? BIT(op, 8) : BIT(op, 0));
	unsigned next = 0x010600;
	program.write_word(next++, op);
	if (immediate)
		program.write_word(next++, 0x0700);
	if (BIT(op, 9))
	{
		program.write_word(next++, 0xe905);
		program.write_word(next++, 0xe906);
	}
	program.write_word(next, 0xf4e1);
	program.write_word(0x020700, 0xe807);
	program.write_word(0x020701, call ? 0xf4e4 : 0xf4e1);
	data.write_word(0x03fe, 2);
	data.write_word(0x03ff, 0x0700);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, returning ? 0x03fe : 0x0400);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x020700);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x020700);
	m_cpu->set_state_int(STATE_GENPC, 0x010600);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 765 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_pipeline_rmw_forward(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const v = variant % 256;
	bool const copy = !strcmp(machine().options().bios(), "mvkd-forward");
	bool const immediate = copy || !strcmp(machine().options().bios(), "imm-forward");
	unsigned const consumer = immediate ? (variant / 768) % 3 : 0, form = immediate ? variant / 2304 : 0, extra = form == 2;
	u16 const literal = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[v & 3];
	u16 const initial = immediate ? 0x2468 : std::array<u16, 4>{0, 0x7fff, 0x8000, 0xffff}[v & 3];
	bool const sxm = BIT(v, 2), ovm = BIT(v, 3), compiler = BIT(v, 4), gap = BIT(v, 5), different = BIT(v, 7);
	u32 const base = BIT(v, 6) ? 0x20000 : 0;
	u16 const target = (compiler ? 0x1000 : 0x400) + 2;
	s32 const sum = (sxm ? s16(initial) : s32(initial)) + 1;
	u16 const result = immediate ? literal : ovm ? u16(std::clamp(sum, s32(-0x8000), s32(0x7fff))) : u16(sum);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, copy ? form == 2 ? 0x70f8 : form == 1 ? 0x7092 : 0x7002 : immediate ? form == 2 ? 0x76f8 : form == 1 ? 0x7692 : 0x7602 : 0x6b02);
	program.write_word(base + 0x601, form == 2 ? target : copy ? 0x11 : immediate ? literal : 1);
	if (form == 2) program.write_word(base + 0x602, copy ? 0x11 : literal);
	program.write_word(base + 0x602 + extra + gap, (consumer == 1 ? 0x0000 : consumer == 2 ? 0x1100 : 0x1000) | (different ? 3 : 2));
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 8);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (compiler ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, target);
	if (copy) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, literal);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(target, initial); m_cpu->space(AS_DATA).write_word(target + 1, 0x1357);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	if (copy) m_operand_data_tap = m_cpu->space(AS_DATA).install_read_tap(0x11, 0x11, "mvkd_forward_source",
		[this, literal, extra](offs_t, u16 &value, u16) { if (machine().side_effects_disabled()) return; expect(value == literal && !m_pipeline_register_writes++ && m_cpu->pipeline_cycles() == 6 + extra, "MVKD captures its mapped source once before forwarding or real destination publication"); });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(target, target + 1, "rmw_forward_reads",
		[this, target, initial, result, gap, different, immediate, extra](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (!immediate && !m_pipeline_operand_reads) expect(address == target && value == initial && m_cpu->pipeline_cycles() == 6, "RMW captures one original RAM operand");
			else expect((gap || different) && address == target + different && value == (different ? 0x1357 : result) && m_cpu->pipeline_cycles() == 7 + extra + gap, "Only a nonforwarded follower performs a real memory read");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(target, target, "rmw_forward_write",
		[this, result, extra](offs_t, u16 &value, u16) { if (machine().side_effects_disabled()) return;
			expect(value == result && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 7 + extra, "Forwarding leaves the producer Write clock unchanged"); ++m_pipeline_store_writes; });
	m_phase = 253390; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delay_load_fixture tms320c54x_test_state::delay_dld_case(unsigned v)
{
	auto f = delay_load_case(v);
	bool const odd = (v / 192) % 2 == 0;
	f.address = odd ? 0x5007 : 0x5004;
	f.literal = f.form == 3 ? f.address : odd ? 2 : 0xffff;
	if (f.form == 1 || f.form == 2) f.final[f.selected] = f.address;
	f.st1 = (f.st1 & ~u16(0x200)) | (f.flags & 2 ? 0x80 : 0);
	f.memory_low = std::array<u16, 4>{0x1234, 0xffff, 0, 0x8001}[(v / 768) % 4];
	f.result = (u64(f.memory) << 16) | f.memory_low;
	if ((f.flags & 1) && (f.memory & 0x8000)) f.result |= 0xff00000000ULL;
	return f;
}

tms320c54x_test_state::delay_load_fixture tms320c54x_test_state::delay_add_case(unsigned v)
{
	delay_load_fixture f{};
	f.kind = std::array<unsigned, 4>{0, 3, 4, 5}[v % 4]; v /= 4;
	f.form = v % 6; v /= 6; unsigned const operand = v % 4; v /= 4;
	unsigned const initial = v % 4; v /= 4; f.destination = v % 2; v /= 2; f.flags = v % 4; v /= 4;
	f.caller = v % 2 ? 0x20000 : 0; v /= 2; f.target = f.kind == 4 ? f.caller ^ 0x20000 : f.caller;
	f.address = f.literal = std::array<u16, 6>{0x500, 0x13, 8, 9, 0x0b, 0x0c}[f.form];
	f.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[operand];
	u64 const left = std::array<u64, 4>{0, 0x7fffffff, 0xff80000000ULL, 0xffffffffffULL}[initial];
	f.a = f.destination ? 0x123456789aULL : left; f.b = f.destination ? left : 0xfedcba9876ULL;
	if (f.address == 8 || f.address == 9) { unsigned const shift = f.address == 9 ? 16 : 0; f.a = (f.a & ~(0xffffULL << shift)) | (u64(f.memory) << shift); }
	if (f.address == 0x0b || f.address == 0x0c) { unsigned const shift = f.address == 0x0c ? 16 : 0; f.b = (f.b & ~(0xffffULL << shift)) | (u64(f.memory) << shift); }
	f.st0 = 0xa008 | (operand & 1 ? 0x600 : 0) | (initial & 1 ? 0x800 : 0);
	f.st1 = 0x800 | (f.flags & 1 ? 0x100 : 0) | (f.flags & 2 ? 0x200 : 0);
	u64 const source = f.destination ? f.b : f.a;
	s64 const addend = f.flags & 1 ? s16(f.memory) : f.memory;
	s64 const sum = (source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source)) + addend;
	bool const overflow = sum > 0x7fffffffLL || sum < -0x80000000LL;
	bool const carry = u64(u32(source)) + u32(addend) > 0xffffffffULL;
	f.result = overflow && (f.flags & 2) ? (sum < 0 ? 0xff80000000ULL : 0x7fffffffULL) : u64(sum) & 0xffffffffffULL;
	f.final_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x5100 + i;
	if (f.address == 0x13) f.initial[3] = f.memory;
	f.final = f.initial; f.read = delay_multiply_fixture::operand_read(f.kind, 1);
	f.checkpoint = std::array<unsigned, 3>{f.literal_clock(), f.read, f.read + 1}[v];
	return f;
}

tms320c54x_test_state::block_back_fixture tms320c54x_test_state::block_back_case(unsigned v)
{
	block_back_fixture f{};
	f.outer = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4]; f.inner = std::array<unsigned, 4>{1, 2, 4, 8}[(v / 4) % 4];
	f.kind = (v / 16) % 2; f.b = (v / 32) % 2; f.base = (v / 64) % 2 ? 0x20000 : 0; f.interrupt = (v / 128) % 3;
	f.checkpoint = std::array<unsigned, 4>{9, 12, 15, 19}[v / 384]; f.end = f.kind ? 0x60d : 0x60a; f.st1 = f.interrupt ? 0x100 : 0x900;
	return f;
}

void tms320c54x_test_state::start_block_back(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = block_back_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, f.end);
	program.write_word(f.base + 0x602, f.b ? 0xf120 : 0xf020); program.write_word(f.base + 0x603, f.inner - 1);
	program.write_word(f.base + 0x604, 0xe589); program.write_word(f.base + 0x605, f.b ? 0xf300 : 0xf000); program.write_word(f.base + 0x606, 0xffff);
	program.write_word(f.base + 0x607, (f.b ? 0xf84a : 0xf842) | (f.kind ? 1 : 0)); program.write_word(f.base + 0x608, f.kind ? 0x60c : 0x604);
	if (f.kind) { program.write_word(f.base + 0x609, 0xf073); program.write_word(f.base + 0x60a, 0x604); }
	program.write_word(f.base + f.end - 1, 0x6d94); program.write_word(f.base + f.end + 1, 0xf4e1);
	unsigned const vector = f.interrupt == 2 ? 4 : 0x48; program.write_word(f.base | vector, 0xf495); program.write_word((f.base | vector) + 1, 0xf4eb);
	for (unsigned i = 0; i < 128; ++i) { data.write_word(0x5000 + i, 0x9000 + i); data.write_word(0x6000 + i, 0); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.interrupt == 1 ? 4 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa002); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.outer);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x6000); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x7000);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_back_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff) { expect(f.interrupt && !m_fast_stack_reads && m_pipeline_register_writes == 1 && word >= 0x602 && word <= f.end + 1, "Backward block IRQ restores one foreground continuation"); ++m_fast_stack_reads; return; } unsigned const i = m_pipeline_operand_reads; expect(i < (f.outer + 1) * f.inner && at == 0x5000 + i && word == 0x9000 + i, "Backward inner loop reads every source once without consuming BRC"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_back_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff) { expect(f.interrupt && !m_pipeline_register_writes && word >= 0x602 && word <= f.end + 1, "Backward block IRQ stacks one retained continuation"); ++m_pipeline_register_writes; return; } unsigned const i = m_pipeline_store_writes; expect(i < (f.outer + 1) * f.inner && at == 0x6000 + i && word == 0x9000 + i, "Backward inner loop writes only its ordered selected copies"); ++m_pipeline_store_writes; if (!i && f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255240;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::block_page_fixture tms320c54x_test_state::block_page_case(unsigned variant)
{
	bool const delayed = variant >= 192; variant %= 192;
	unsigned const page = (variant / 8) % 3;
	return {std::array<unsigned, 4>{0, 1, 2, 7}[variant % 4],
		std::array<unsigned, 4>{6, 10, 16, 24}[variant / 48],
		std::array<u32, 3>{0, 0x20000, 0x7f0000}[page],
		std::array<u32, 3>{0x20000, 0x7f0000, 0}[page],
		bool((variant / 4) % 2), bool((variant / 24) % 2), delayed};
}

void tms320c54x_test_state::start_block_page(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = block_page_case(variant); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.caller + 0x600 + i, 0xf495); program.write_word(f.callee + 0x600 + i, 0xf495); }
	program.write_word(f.caller + 0x600, 0xf072); program.write_word(f.caller + 0x601, 0x609);
	program.write_word(f.caller + 0x602, 0x6d94);
	program.write_word(f.caller + 0x603, (f.delayed ? 0xfb80 : 0xf980) | (f.callee >> 16)); program.write_word(f.caller + 0x604, f.hazard ? 0x608 : 0x610);
	if (f.delayed) program.write_word(f.caller + 0x605, 0x6d97);
	program.write_word(f.caller + 0x60a, 0xf4e1);
	program.write_word(f.callee + 0x602, 0x6d95);
	program.write_word(f.callee + 0x609, 0x6d96); program.write_word(f.callee + 0x60a, 0xf4e4);
	program.write_word(f.callee + 0x610, 0x6d96); program.write_word(f.callee + 0x611, 0xf4e4);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1500); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x1600);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x1700);
	m_pipeline_store_writes = m_pipeline_operand_reads = m_idle_fixture_tick = 0; m_pipeline_replay = false;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "block_page_frame_writes",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const part = m_pipeline_store_writes++ % 2;
			expect(at == 0x2fff - part && word == (part ? f.caller >> 16 : f.delayed ? 0x607 : 0x605), "Cross-page FCALL publishes one ordered PC/page frame"); });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "block_page_frame_reads",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const part = m_pipeline_operand_reads++ % 2;
			expect(at == 0x2ffe + part && word == (part ? f.delayed ? 0x607 : 0x605 : f.caller >> 16), "Cross-page FRET consumes the matching page/PC frame"); });
	m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(f.staged);
	m_phase = 294000; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::block_far_call_fixture tms320c54x_test_state::block_far_call_case(unsigned v, bool delayed)
{
	block_far_call_fixture f{};
	f.delayed = delayed; f.delay_form = delayed ? v / 768 : 0; v %= 768;
	f.count = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4]; f.layout = (v / 4) % 2; f.caller = (v / 8) % 2 ? 0x20000 : 0;
	f.callee = (((v / 16) % 2 ? f.caller ^ 0x20000 : f.caller) | 0x700); f.nested = (f.callee ^ 0x20000) & 0x7f0000; f.nested |= 0x800;
	f.depth = 1 + (v / 32) % 2; f.interrupt = (v / 64) % 3; f.checkpoint = std::array<unsigned, 4>{8, 10, 15, 23}[v / 192];
	f.call = f.layout ? 0x606 : 0x603; f.st1 = f.interrupt ? 0x100 : 0x900;
	return f;
}

void tms320c54x_test_state::start_block_far_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = block_far_call_case(variant, !strcmp(machine().options().bios(), "rptb-fcalld"));
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.caller + 0x600 + i, 0xf495);
	program.write_word(f.caller + 0x600, 0xf072); program.write_word(f.caller + 0x601, 0x609); program.write_word(f.caller + 0x602, 0x6d94);
	program.write_word(f.caller + f.call, (f.delayed ? 0xfb80 : 0xf980) | (f.callee >> 16)); program.write_word(f.caller + f.call + 1, 0x700);
	program.write_word(f.caller + f.call + 2, !f.delayed || !f.delay_form ? 0x6d95 : f.delay_form == 1 ? 0x770e : 0xe801);
	if (f.delayed) program.write_word(f.caller + f.call + 3, !f.delay_form ? 0x6d97 : f.delay_form == 1 ? 0x1234 : 0xf495);
	program.write_word(f.caller + 0x60a, 0xf4e1);
	program.write_word(f.callee, 0xe589);
	program.write_word(f.callee + 1, f.depth == 2 ? 0xf980 | (f.nested >> 16) : 0xf4e4);
	program.write_word(f.callee + 2, 0x800); program.write_word(f.callee + 3, 0xf4e4);
	program.write_word(f.nested, 0x6d96); program.write_word(f.nested + 1, 0xf4e4);
	unsigned const vector = f.interrupt == 2 ? 4 : 0x48;
	for (u32 base : {0U, 0x20000U}) { program.write_word(base | vector, 0xf495); program.write_word((base | vector) + 1, 0xf4eb); }
	for (unsigned i = 0; i < 16; ++i) { data.write_word(0x5000 + i, 0x9000 + i); data.write_word(0x6000 + i, 0); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.interrupt == 1 ? 4 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 2 ? 0x5000 : i == 3 ? 0x6000 : i == 4 ? 0x7000 : i == 5 ? 0x7100 : 0x7200 + i);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_vector_acks = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_far_call_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x2ff0 && at < 0x3000)
		{
			if (at == 0x2ffe)
			{
				auto const disable = machine().disable_side_effects();
				expect(word == f.caller >> 16 && m_cpu->space(AS_DATA).read_word(0x2fff) == f.call + (f.delayed ? 4 : 2), "Outer FRET consumes the paired caller page and actual post-delay continuation");
			}
			++m_fast_stack_reads; expect(m_fast_stack_reads <= m_pipeline_register_writes, "Block callee returns consume only published stack words"); return;
		}
		unsigned const i = m_pipeline_operand_reads; expect(i <= f.count && at == 0x5000 + i && word == 0x9000 + i, "Block callee reads one ordered source per caller iteration"); ++m_pipeline_operand_reads;
		if (!i && f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_far_call_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x2ff0 && at < 0x3000)
		{
			// An IRQ after FRET can reuse 2fff. Check the two-word call
			// as a page/PC pair, not every write to that stack address.
			if (at == 0x2ffe)
			{
				auto const disable = machine().disable_side_effects();
				expect(word == f.caller >> 16 && m_cpu->space(AS_DATA).read_word(0x2fff) == f.call + (f.delayed ? 4 : 2), "Outer FCALL publishes the paired caller page and actual post-delay continuation");
			}
			++m_pipeline_register_writes; return;
		}
		if (f.delayed && f.delay_form == 1 && at == 0x0e) { expect(word == 0x1234 && m_pipeline_extensions_reads <= f.count, "Two-word delay owns one T publication per block iteration"); ++m_pipeline_extensions_reads; return; }
		unsigned const i = m_pipeline_store_writes; expect(i <= f.count && at == 0x6000 + i && word == 0x9000 + i, "Block callee writes each copied word once"); ++m_pipeline_store_writes;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255200;
	m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_far_call_edge(unsigned variant)
{
	u32 const base = (variant / 5) % 2 ? 0x20000 : 0; unsigned const kind = variant % 5;
	start_block_far_call(2 + (base ? 8 : 0)); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM); for (unsigned i = 0x602; i <= 0x609; ++i) program.write_word(base + i, 0xf495);
	unsigned const call = kind == 1 ? 0x608 : kind == 4 ? 0x607 : 0x603;
	program.write_word(base + call, kind == 3 ? 0xf074 : (kind == 0 || kind == 4 ? 0xfb80 : 0xf980) | (base >> 16)); program.write_word(base + call + 1, kind == 0 || kind == 2 || kind == 3 ? 0x602 : 0x700);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "block_fcall_edge_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "block_fcall_edge_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_phase = 255220; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

tms320c54x_test_state::block_near_call_fixture tms320c54x_test_state::block_near_call_case(unsigned v)
{
	block_near_call_fixture f{};
	f.count = std::array<unsigned, 4>{0, 1, 5, 15}[v % 4]; v /= 4;
	f.layout = v % 3; v /= 3; f.base = std::array<u32, 3>{0, 0x20000, 0x7f0000}[v % 3]; v /= 3;
	f.depth = 1 + v % 2; v /= 2; f.interrupt = v % 3; v /= 3;
	f.delayed_return = v % 2; v /= 2; f.delayed = v % 2; v /= 2;
	f.checkpoint = std::array<unsigned, 4>{5, 8, 13, 25}[v];
	f.call = f.layout == 0 ? 0x603 : f.layout == 1 ? (f.delayed ? 0x606 : 0x608) : 0x602;
	f.end = f.layout == 2 ? f.call + (f.delayed ? 3 : 1) : 0x609;
	f.st1 = f.interrupt ? 0x100 : 0x900;
	return f;
}

void tms320c54x_test_state::start_block_near_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_observed_instructions.clear();
	auto const f = block_near_call_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); program.write_word(f.base + 0x800 + i, 0xf495); }
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, f.end);
	if (f.layout != 2) program.write_word(f.base + 0x602, 0x6d94);
	program.write_word(f.base + f.call, f.delayed ? 0xf274 : 0xf074); program.write_word(f.base + f.call + 1, 0x700);
	if (f.delayed || f.layout == 0) program.write_word(f.base + f.call + 2, 0x6d95);
	// A pending IRQ can legitimately wake an adjacent IDLE (SPRU131G
	// 6.11.1). Check fallthrough once, then drain before parking.
	program.write_word(f.base + f.end + 1, 0x6d91); program.write_word(f.base + f.end + 10, 0xf4e1);
	program.write_word(f.base + 0x700, 0xe589);
	if (f.depth == 2) { program.write_word(f.base + 0x701, 0xf074); program.write_word(f.base + 0x702, 0x800); }
	unsigned const ret = f.depth == 2 ? 0x703 : 0x701;
	program.write_word(f.base + ret, f.delayed_return ? 0xfe00 : 0xfc00);
	if (f.delayed_return) program.write_word(f.base + ret + 1, 0x6d97);
	program.write_word(f.base + 0x800, 0x6d96); program.write_word(f.base + 0x801, f.delayed_return ? 0xfe00 : 0xfc00);
	if (f.delayed_return) program.write_word(f.base + 0x802, 0x6d97);
	unsigned const vector = f.interrupt == 2 ? 4 : 0x48;
	program.write_word(f.base | vector, 0xf495); program.write_word((f.base | vector) + 1, 0xf4eb);
	for (unsigned i = 0; i < 16; ++i) { data.write_word(0x5000 + i, 0x9000 + i); data.write_word(0x6000 + i, 0); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.interrupt == 1 ? 4 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 2 ? 0x5000 : i == 3 ? 0x6000 : 0x6c00 + 0x100 * i);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_near_call_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x2ff0 && at < 0x3000) { if (!f.interrupt && at == 0x2fff) expect(word == f.call + (f.delayed ? 4 : 2), "Near RET consumes the ordinary post-call continuation"); ++m_fast_stack_reads; expect(m_fast_stack_reads <= m_pipeline_register_writes, "Near RET consumes only committed frames"); return; }
		unsigned const i = m_pipeline_operand_reads++;
		if (i > f.count || at != 0x5000 + i || word != 0x9000 + i)
		{
			osd_printf_info("block_near_call_read: case=%u index=%u address=%04x value=%04x pc=%llx clock=%llu sp=%llx brc=%llx\n", m_pipeline_peripheral_case, i, unsigned(at), word, (unsigned long long)m_cpu->state_int(STATE_GENPC), (unsigned long long)m_cpu->pipeline_cycles(), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_BRC));
			for (auto const &entry : m_observed_instructions) osd_printf_info("block_near_call_retired: pc=%06x word=%04x\n", unsigned(entry.first), entry.second);
		}
		expect(i <= f.count && at == 0x5000 + i && word == 0x9000 + i, "Block near callee reads one ordered word per iteration");
		if (!i && f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_near_call_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x2ff0 && at < 0x3000) { if (!f.interrupt && at == 0x2fff) expect(word == f.call + (f.delayed ? 4 : 2), "Near CALL pushes the ordinary post-call continuation"); ++m_pipeline_register_writes; return; }
		unsigned const i = m_pipeline_store_writes++; expect(i <= f.count && at == 0x6000 + i && word == 0x9000 + i, "Block near callee writes one ordered word per iteration");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62150;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_near_call_edge(unsigned variant)
{
	unsigned const kind = variant % 3, page = (variant / 3) % 3;
	start_block_near_call((variant / 9) % 2 + page * 12); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = std::array<u32, 3>{0, 0x20000, 0x7f0000}[page]; auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0x602; i <= 0x60a; ++i) program.write_word(base + i, 0xf495);
	unsigned const call = kind == 0 ? 0x603 : kind == 1 ? 0x607 : 0x609;
	program.write_word(base + call, kind == 1 ? 0xf274 : 0xf074); program.write_word(base + call + 1, kind == 0 ? 0x602 : 0x700);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "block_near_edge_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "block_near_edge_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_phase = 62152; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

tms320c54x_test_state::block_single_fixture tms320c54x_test_state::block_single_case(unsigned v) const
{
	block_single_fixture f{};
	f.memory = !strcmp(machine().options().bios(), "rptb-memory");
	if (f.memory) { f.count_form = v % 5; v /= 5; }
	f.outer = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4]; f.inner = std::array<unsigned, 4>{0, 1, 2, 7}[(v / 4) % 4];
	f.kind = (v / 16) % 3; f.tail = (v / 48) % 2; f.base = (v / 96) % 2 ? 0x20000 : 0;
	f.checkpoint = std::array<unsigned, 4>{7, 10, 13, 17}[v / 192]; f.total = (f.outer + 1) * (f.inner + 1);
	f.end = 0x604 + unsigned(f.kind == 1) + (f.tail ? 0 : 3);
	if (f.memory && f.count_form == 3) f.total += (f.outer + 1) / 2;
	if (f.memory && f.count_form == 4) { ++f.end; ++f.body; }
	return f;
}

void tms320c54x_test_state::start_block_single(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = block_single_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, f.end);
	program.write_word(f.base + 0x602, 0x6d94); program.write_word(f.base + 0x603, 0xec00 | f.inner);
	program.write_word(f.base + 0x604, std::array<u16, 3>{0xe589, 0x7693, 0xf495}[f.kind]);
	if (f.kind == 1) program.write_word(f.base + 0x605, 0x1234);
	if (!f.tail) program.write_word(f.base + 0x605 + unsigned(f.kind == 1), 0x6d95);
	if (f.memory)
	{
		program.write_word(f.base + 0x603, std::array<u16, 5>{0x4760, 0x4760, 0x4780, 0x4790, 0x47f8}[f.count_form]);
		if (f.count_form == 4) program.write_word(f.base + 0x604, 0x1060);
		program.write_word(f.base + f.body, std::array<u16, 3>{0xe589, 0x7693, 0xf495}[f.kind]);
		if (f.kind == 1) program.write_word(f.base + f.body + 1, 0x1234);
		if (!f.tail) program.write_word(f.base + f.body + 1 + unsigned(f.kind == 1), 0x6d95);
		for (unsigned i = 0; i <= f.outer; ++i) data.write_word(0x1060 + i, f.inner + (f.count_form == 3 ? i & 1 : 0));
	}
	program.write_word(f.base + f.end + 1, 0x770e); program.write_word(f.base + f.end + 2, 0x1a2b); program.write_word(f.base + f.end + 3, 0xf4e1);
	for (unsigned i = 0; i < 128; ++i) { data.write_word(0x5000 + i, 0x9000 + i); data.write_word(0x6000 + i, 0); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8820); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.outer); m_cpu->set_state_int(tms320c54x_device::STATE_RC, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 2 ? 0x5000 : i == 3 ? 0x6000 : i == 4 ? 0x7000 : i == 5 ? 0x7100 : 0x7200 + i);
	if (f.memory)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1060);
		if (f.count_form == 1) m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4800);
	}
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0x5000, 0x50ff, "block_single_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_operand_reads; expect(f.kind == 0 && i < f.total && at == 0x5000 + i && word == 0x9000 + i, "Inner MVDD reads each outer/inner source once in order"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0x6000, 0x60ff, "block_single_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_store_writes; expect(f.kind < 2 && i < f.total && at == 0x6000 + i && word == (f.kind == 1 ? 0x1234 : 0x9000 + i), "Inner repeat writes each destination once without consuming outer iterations"); ++m_pipeline_store_writes; });
	m_pipeline_extensions_reads = 0;
	if (f.memory) m_pipeline_fetch_tap = data.install_read_tap(0x1060, 0x1067, "block_memory_count", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_extensions_reads++;
		expect(i <= f.outer && at == 0x1060 + (f.count_form == 3 ? i : 0) && word == f.inner + (f.count_form == 3 ? i & 1 : 0), "Each outer iteration reads its original memory count exactly once");
		if (f.count_form == 3) m_cpu->space(AS_DATA).write_word(at, 0xffff);
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255160;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_single_edge(unsigned variant)
{
	u32 const base = (variant / 4) % 2 ? 0x20000 : 0; unsigned const kind = variant % 4;
	start_block_single(2 + (base ? 96 : 0)); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(base + 0x601, kind ? 0x603 : 0x602); program.write_word(base + 0x602, std::array<u16, 4>{0xec07, 0xf070, 0x47f8, 0xf072}[kind]); program.write_word(base + 0x603, kind == 3 ? 0x604 : 1);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "block_single_edge_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "block_single_edge_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_phase = 255180; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

tms320c54x_test_state::branch_repeat_fixture tms320c54x_test_state::branch_repeat_case(unsigned v) const
{
	branch_repeat_fixture f{};
	f.conditional = !strcmp(machine().options().bios(), "bcd-rpt");
	if (f.conditional) { f.condition = v % 12; v /= 12; }
	f.staged = v % 2; v /= 2; f.kind = v % 3; v /= 3;
	f.count = std::array<unsigned,5>{0,1,8,39,255}[v % 5]; v /= 5;
	f.following = std::array<unsigned,3>{0,2,7}[v % 3]; v /= 3;
	f.target = v % 2 ? 0x500 : 0x700; v /= 2; unsigned const pattern = v % 4; v /= 4;
	f.seed = std::array<u16,4>{0,0xffff,0x8001,0x0fff}[pattern]; f.status = 0x800 | (pattern << 8);
	f.predicate_pattern = pattern;
	f.base = v % 2 ? 0x20000 : 0; v /= 2;
	f.checkpoint = std::array<unsigned,4>{3,8,9+f.count/2,20+f.count}[v % 4];
	f.limit = 4*(f.count+f.following+2)+100; return f;
}

void tms320c54x_test_state::start_branch_repeat(unsigned variant)
{
	bool const returning = !strcmp(machine().options().bios(), "retd-rpt");
	bool const calling = !strcmp(machine().options().bios(), "calld-rpt");
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	auto const f = branch_repeat_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base+0x600+i,0xf495);
	program.write_word(f.base+0x600,0xf273); program.write_word(f.base+0x601,f.target);
	if (calling) program.write_word(f.base+0x600,0xf274);
	if (f.conditional) program.write_word(f.base+0x600,0xfa40 | (f.condition < 6 ? f.condition + 2 : f.condition + 4));
	program.write_word(f.base+0x602,0xec00|f.count); program.write_word(f.base+0x603,f.body()); program.write_word(f.base+0x604,0xf4e1);
	if (returning)
	{
		program.write_word(f.base+0x600,0xfe00); program.write_word(f.base+0x601,0xec00|f.count);
		program.write_word(f.base+0x602,f.body()); program.write_word(f.base+0x603,0xf495);
		data.write_word(0x2fff,f.target);
	}
	program.write_word(f.base+f.target,0xec00|f.following); program.write_word(f.base+f.target+1,0x8193); program.write_word(f.base+f.target+2,0xf4e1);
	if (calling) program.write_word(f.base+f.target+2,0xfc00);
	if (f.conditional) { program.write_word(f.base+0x604,0xec00|f.following); program.write_word(f.base+0x605,0x8193); program.write_word(f.base+0x606,0xf4e1); }
	for (unsigned i = 0; i < 256; ++i) { data.write_word(0x1800+i,u16(f.seed+i*257)); data.write_word(0x1c00+i,0xdead); }
	for (unsigned i = 0; i < 32; ++i) data.write_word(0x2000+i,0xdead);
	for (u16 address : {0x17ff,0x1900,0x1bff,0x1d00,0x1fff,0x2020}) data.write_word(address,0x2468);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0,0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,f.status);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a()); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b());
	m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_T,0x5aa5);
	if (returning) m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x2fff);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC,13); m_cpu->set_state_int(tms320c54x_device::STATE_RSA,0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_REA,0x6666); m_cpu->set_state_int(tms320c54x_device::STATE_RC,0);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0+ar,ar == 3 ? 0x2000 : ar == 4 ? 0x1800 : ar == 5 ? 0x1c00 : 0x4000+ar);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_rptbd_counts.reads = 0;
	m_pipeline_register_writes = 0;
	m_observed_instructions.clear(); m_saved_observed_instructions.clear(); m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"branch_repeat_read",[this,f,returning,calling](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		if ((returning || calling) && at == 0x2fff)
		{
			expect(!m_rptbd_counts.reads++ && word == (calling ? 0x604 : f.target), "Return samples its owned original continuation once");
			return;
		}
		unsigned const i = m_pipeline_operand_reads++;
		expect(f.kind == 1 && i <= f.count && at == 0x1800+i && word == u16(f.seed+i*257),"Delay-owned MVDD reads exactly its ordered original source words");
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"branch_repeat_write",[this,f,calling](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		if (calling && at == 0x2fff) { expect(!m_pipeline_register_writes++ && word == 0x604 && !m_pipeline_store_writes && !m_pipeline_extensions_reads, "CALLD publishes one continuation before delay-body and target stores"); return; }
		if (at >= 0x2000 && at < 0x2020)
		{
			unsigned const i = m_pipeline_extensions_reads++;
			expect(i <= f.following && at == 0x2000+i && word == u16(f.b(true)) && m_pipeline_store_writes == (f.kind == 2 ? 0 : f.count+1),"Target repeat starts only after the entire delay-owned repeat has completed");
		}
		else
		{
			unsigned const i = m_pipeline_store_writes++;
			expect(f.kind != 2 && i <= f.count && !m_pipeline_extensions_reads && at == (f.kind ? 0x1c00 : 0x1800)+i && word == (f.kind ? u16(f.seed+i*257) : u16(f.a(true))),"Repeated delay body writes before any target-side publication");
		}
	});
	if (f.conditional && f.staged) m_pipeline_fetch_tap = program.install_read_tap(f.base+0x603,f.base+0x603,"bcd_repeat_body",[this,f](offs_t at,u16 &word,u16) { if (machine().side_effects_disabled()) return; expect(word == f.body(), "BCD repeat captures its original delay opcode once"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(at,0xe8ff); });
	m_phase = 600040; m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(f.staged); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

void tms320c54x_test_state::start_block_delay_repeat(unsigned variant)
{
	start_branch_repeat(variant);
	auto const f = branch_repeat_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned const end = f.target == 0x500 ? 0x604 : 0x605;
	program.write_word(f.base + 0x600, 0xf272);
	program.write_word(f.base + 0x601, end);
	program.write_word(f.base + 0x604, 0xf495);
	program.write_word(f.base + end, 0x8193);
	program.write_word(f.base + end + 1, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.following);
	m_phase = 600060;
}

void tms320c54x_test_state::start_branch_repeat_edge(unsigned variant)
{
	u32 const base = (variant/8)%2 ? 0x20000 : 0; unsigned const kind = variant%8;
	start_branch_repeat(1+(base ? 720 : 0)); m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	if (kind == 0) { program.write_word(base+0x602,0xf495); program.write_word(base+0x603,0xec01); }
	if (kind == 1) program.write_word(base+0x600,0x6e87);
	if (kind == 2) { program.write_word(base+0x600,0xf274); program.write_word(base+0x602,0xf495); program.write_word(base+0x603,0xec01); }
	if (kind == 3) { program.write_word(base+0x600,0xfe00); program.write_word(base+0x601,0xf495); program.write_word(base+0x602,0xec01); }
	if (kind == 4) program.write_word(base+0x602,0xf070);
	if (kind == 5) program.write_word(base+0x602,0x4780);
	if (kind >= 6) { program.write_word(base+0x602,0xec01); program.write_word(base+0x603,kind == 6 ? 0x76f8 : 0x71f8); }
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	if (kind == 3) m_cpu->space(AS_DATA).write_word(0x3000,0x700);
	// RETD reads its return word before releasing delay Decode; CALLD
	// already reserved SP in Decode. Neither belongs to the rejected RPT.
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0,0xffff,"branch_repeat_edge_reads",[this,kind](offs_t at,u16 &word,u16) { if (!machine().side_effects_disabled()) { expect(kind == 3 && at == 0x3000 && word == 0x700 && !m_pipeline_operand_reads,"Only RETD's preceding return-word Read may occur before rejection"); ++m_pipeline_operand_reads; } });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0,0xffff,"branch_repeat_edge_writes",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_phase = 600050; m_cpu->set_state_int(STATE_GENPC,base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
}

void tms320c54x_test_state::start_block_delay_repeat_edge(unsigned ordinal)
{
	unsigned const kind = std::array<unsigned, 5>{0, 4, 5, 6, 7}[ordinal % 5];
	start_branch_repeat_edge(kind + (ordinal / 5) * 8);
	u32 const base = (ordinal / 5) % 2 ? 0x20000 : 0;
	m_cpu->space(AS_PROGRAM).write_word(base + 0x600, 0xf272);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0x606);
}

void tms320c54x_test_state::start_conditional_delay_repeat_edge(unsigned ordinal)
{
	unsigned const kind = std::array<unsigned,5>{0,4,5,6,7}[ordinal % 5];
	start_branch_repeat_edge(kind + (ordinal / 5) * 8);
	u32 const base = (ordinal / 5) % 2 ? 0x20000 : 0;
	m_cpu->space(AS_PROGRAM).write_word(base + 0x600,0xfa47);
}

void tms320c54x_test_state::set_repeat_count_source(u16 address, u16 value)
{
	if (address >= 0x60) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(address, value); }
	else if (address < 14)
	{
		unsigned const part = (address - 8) % 3; auto const state = address < 11 ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B;
		u64 const mask = part == 2 ? 0xff : 0xffff;
		m_cpu->set_state_int(state, (m_cpu->state_int(state) & ~(mask << (16 * part))) | ((u64(value) & mask) << (16 * part)));
	}
	else m_cpu->set_state_int(address == 14 ? tms320c54x_device::STATE_T : address == 15 ? tms320c54x_device::STATE_TRN : address < 24 ? tms320c54x_device::STATE_AR0 + address - 16 : address == 24 ? tms320c54x_device::STATE_SP : tms320c54x_device::STATE_BK, value);
}

void tms320c54x_test_state::start_delayed_far_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const layout = variant % 3, caller_page = std::array<unsigned, 3>{0, 2, 0x7f}[(variant / 3) % 3], target_page = std::array<unsigned, 3>{0, 2, 0x7f}[(variant / 9) % 3];
	u32 const caller = caller_page << 16, target = target_page << 16;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(caller + 0x600 + i, 0xf495); program.write_word(target + 0x700 + i, 0xf495); program.write_word(caller + i, 0xf495); program.write_word(target + i, 0xf495); }
	program.write_word(caller + 0x600, 0xfb80 | target_page); program.write_word(caller + 0x601, 0x700);
	program.write_word(caller + 0x602, layout == 0 ? 0x481e : layout == 1 ? 0x770e : 0xf495);
	program.write_word(caller + 0x603, layout == 0 ? 0x891a : layout == 1 ? 0x1234 : 0x4816);
	program.write_word(caller + 0x604, 0xe955); program.write_word(target + 0x700, 0xe933); program.write_word(target + 0x703, 0xf4e4);
	for (u32 page : {caller, target}) { program.write_word(page + 4, 0xf4eb); program.write_word(page + 0x40, 0xf4eb); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 0x9876); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x4567);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_store_writes = m_fast_stack_reads = m_pipeline_extensions_reads = m_pipeline_operand_reads = m_pipeline_drained_fetches = m_vector_acks = m_pipeline_register_writes = 0;
	m_pipeline_write_tap = data.install_write_tap(0xffd, 0xfff, "delayed_far_call_stack",
		[this, caller_page](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (m_pipeline_store_writes < 2) expect(at == 0xfff - m_pipeline_store_writes && word == (m_pipeline_store_writes ? caller_page : 0x604), "FCALLD stacks the caller page and continuation after both delay words"); else expect(m_pipeline_store_writes == 2 && at == 0xffd, "Pending interrupt owns a separate stack word"); ++m_pipeline_store_writes; });
	m_pipeline_operand_tap = data.install_read_tap(0xffd, 0xfff, "delayed_far_call_return",
		[this, caller_page, variant](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; bool const interrupt = (variant / 27) % 3; if (interrupt && !m_fast_stack_reads) expect(at == 0xffd, "RETE removes only the interrupt frame"); else { unsigned const part = m_fast_stack_reads - unsigned(interrupt); expect(part < 2 && at == 0xffe + part && word == (part ? 0x604 : caller_page), "FRET restores the retained caller page and post-delay PC"); } ++m_fast_stack_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(caller + 0x601, caller + 0x604, "delayed_far_call_words",
		[this, caller, layout](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == caller + 0x601) { expect(!m_pipeline_extensions_reads && word == 0x700, "FCALLD owns one target literal"); ++m_pipeline_extensions_reads; } else if (at == caller + 0x603 && layout == 1) { expect(!m_pipeline_operand_reads && word == 0x1234, "The complete two-word delay owns its literal"); ++m_pipeline_operand_reads; } else if (at == caller + 0x604) { expect(!m_pipeline_drained_fetches, "Post-delay continuation is fetched once after far return"); ++m_pipeline_drained_fetches; } });
	m_vector_prefetch_tap = program.install_read_tap(target + 0x700, target + 0x700, "delayed_far_call_target",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_register_writes, "Delayed far target is fetched once"); ++m_pipeline_register_writes; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253670;
	m_cpu->set_state_int(STATE_GENPC, caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

std::array<u64,3> tms320c54x_test_state::square_repeat_step(poly_fixture const &f, std::array<u64,3> state, unsigned ordinal)
{
	unsigned const destination = f.operation & 1;
	s64 const old = state[destination] & 0x8000000000ULL ? s64(state[destination]) - 0x10000000000LL : s64(state[destination]);
	s64 const sample = s16(square_repeat_memory(f, ordinal));
	s64 product = sample * (f.tmac ? s16(f.t) : sample) * (f.st1 & 0x40 ? 2 : 1);
	if (f.tmac && (f.pmst & 2) && (f.st1 & 0x240) == 0x240) product = std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
	s64 result = old + (f.operation & (f.tmac ? 4 : 2) ? -product : product);
	if (f.tmac && (f.operation & 2)) result = (result + 32768) & ~s64(65535);
	if (result < -2147483648LL || result > 2147483647LL)
	{
		state[2] |= destination ? 0x200 : 0x400;
		if (f.st1 & 0x200) result = result < 0 ? -2147483648LL : 2147483647LL;
	}
	state[destination] = u64(result) & 0xffffffffffULL; return state;
}

tms320c54x_test_state::fcala_delay_fixture tms320c54x_test_state::fcala_delay_case(unsigned v)
{
	return {bool(v & 1), bool((v / 18) % 2), (v / 36) % 3,
		std::array<unsigned, 6>{3, 6, 7, 8, 10, 12}[v / 108],
		u32(std::array<unsigned, 3>{0, 2, 0x7f}[(v / 2) % 3] << 16),
		u32(std::array<unsigned, 3>{0, 3, 0x7f}[(v / 6) % 3] << 16)};
}

void tms320c54x_test_state::start_fcala_delay(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = fcala_delay_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) { program.write_word(f.caller + 0x600 + i, 0xf495); program.write_word(f.target + 0x700 + i, 0xf495); }
	program.write_word(f.caller + 0x600, f.b ? 0xf7e7 : 0xf6e7);
	program.write_word(f.caller + 0x601, f.wide ? 0x7693 : f.b ? 0x890e : 0x880e);
	program.write_word(f.caller + 0x602, f.wide ? 0x5aa5 : f.b ? 0x890f : 0x880f);
	program.write_word(f.caller + 0x603, 0xf4e1);
	program.write_word(f.target + 0x700, f.b ? 0x8192 : 0x8092); program.write_word(f.target + 0x708, 0xf4e4);
	unsigned const vector = f.source == 2 ? 4 : 0x40; program.write_word(f.target + vector, 0xf4eb);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.source == 1 ? 1 : 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.source == 1 ? 0x100 : 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1500); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x4400);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.b ? 0x1234 : 0x5500000700ULL | f.target);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ? 0x5500000700ULL | f.target : 0x2345);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x2468); data.write_word(0x1400, 0xdead);
	m_fcala_delay_counts = {}; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "fcala_delay_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_fcala_delay_counts;
		if (at == 0xfff || at == 0xffe) { unsigned const i = c.frames++; expect(i < 2 && at == 0xfff - i && word == (i ? f.caller >> 16 : 0x603) && m_cpu->pipeline_cycles() == 5 + i, "FCALAD owns PC+3 then caller XPC without a nondelayed hold"); return; }
		if (at == 0xffd) { expect(f.source && c.frames == 2 && word >= 0x700 && word <= 0x708, "Interrupt frame follows the complete far-call frame"); c.irq_pc = word; return; }
		if (at == 0x1500) { expect(!c.callee && word == 0xbeef && c.delays == (f.wide ? 1U : 2U) && (f.source ? m_cpu->pipeline_cycles() >= 10 : m_cpu->pipeline_cycles() == 10), "Target executes after the four-cycle call and complete delay work"); ++c.callee; return; }
		unsigned const i = c.delays++; expect(i < (f.wide ? 1U : 2U) && at == (f.wide ? 0x1400 : 0x0e + i) && word == (f.wide ? 0x5aa5 : 0xbeef) && m_cpu->pipeline_cycles() == (f.wide ? 8 : 7 + i), "Both delay words execute once after the retained accumulator target");
	});
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "fcala_delay_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_fcala_delay_counts;
		if (at == 0xffd) { expect(f.source && !c.irq_reads && word == c.irq_pc, "RETE consumes only its own interrupt frame"); ++c.irq_reads; return; }
		unsigned const i = c.reads++; expect(i < 2 && at == 0xffe + i && word == (i ? 0x603 : f.caller >> 16), "FRET balances the retained PC+3 and XPC frame");
	});
	m_pipeline_fetch_tap = program.install_read_tap(f.target + 0x700, f.target + 0x700, "fcala_delay_target", [this, f](offs_t, u16 &, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_fcala_delay_counts;
		expect(c.target_fetches ? f.source && c.frames == 2 && c.target_fetches == 1 : c.frames == 1 && m_cpu->pipeline_cycles() == 6, "FCALAD target Fetch follows Read; only an interrupt may refetch it"); ++c.target_fetches;
	});
	m_vector_prefetch_tap = program.install_read_tap(f.target + vector, f.target + vector, "fcala_delay_vector", [this, f](offs_t, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_fcala_delay_counts;
		expect(f.source && !c.vectors && word == 0xf4eb && c.frames == 2 && c.delays == (f.wide ? 1U : 2U), "IRQ/NMI cannot enter before all delay effects and call-frame words"); ++c.vectors; m_cpu->set_input_line(f.source == 2 ? INPUT_LINE_NMI : 0, CLEAR_LINE);
	});
	m_phase = 255820; m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delayed_conditional_call_fixture tms320c54x_test_state::delayed_conditional_call_case(unsigned v, bool tc)
{
	delayed_conditional_call_fixture f{};
	if(tc)
	{
		f.tc=true; f.condition=v%2; f.initial_tc=(v/2)%2; f.b=(v/32)%2; f.layout=(v/64)%2;
		f.base=(v/128)%2 ? 0x20000 : 0; f.checkpoint=std::array<unsigned,5>{3,6,7,10,13}[v/256];
		f.initial=std::array<u64,8>{0,1,0x8000000000ULL,0xffffffffffULL,0x7fffffffffULL,0x0100000000ULL,0x0080000000ULL,0x00ffffffffULL}[(v/4)%8];
		f.taken=bool(f.condition)==f.initial_tc; f.changed=f.initial;
		return f;
	}
	f.condition = v % 6; f.b = (v / 48) % 2; f.layout = (v / 96) % 2;
	f.base = (v / 192) % 2 ? 0x20000 : 0;
	f.checkpoint = std::array<unsigned, 5>{3, 6, 7, 10, 13}[v / 384];
	f.initial = std::array<u64, 8>{0, 1, 0x8000000000ULL, 0xffffffffffULL, 0x7fffffffffULL, 0x0100000000ULL, 0x0080000000ULL, 0x00ffffffffULL}[(v / 6) % 8];
	s64 const signed_value = f.initial & 0x8000000000ULL ? s64(f.initial) - 0x10000000000LL : s64(f.initial);
	f.taken = std::array<bool, 6>{signed_value >= 0, signed_value < 0, signed_value != 0, signed_value == 0, signed_value > 0, signed_value <= 0}[f.condition];
	f.literal = f.taken ? std::array<u16, 6>{0xffff, 0, 0, 1, 0, 1}[f.condition] : std::array<u16, 6>{0, 0xffff, 1, 0, 1, 0}[f.condition];
	f.changed = f.layout ? u64(s64(s16(f.literal))) & 0xffffffffffULL : f.initial ^ 0xffffffffffULL;
	return f;
}

void tms320c54x_test_state::start_delayed_conditional_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = delayed_conditional_call_case(variant, !strcmp(machine().options().bios(),"tc-delay-call")); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); }
	program.write_word(f.base + 0x600, f.opcode()); program.write_word(f.base + 0x601, 0x700);
	program.write_word(f.base + 0x602, f.delay(0));
	program.write_word(f.base + 0x603, f.delay(1));
	program.write_word(f.base + 0x604, 0x6d94); program.write_word(f.base + 0x605, 0xf4e1);
	program.write_word(f.base + 0x700, 0x6d96); program.write_word(f.base + 0x701, 0xfc00);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.status()); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_RTN, 0x5555);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.b ? 0x123456789aULL : f.initial); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ? f.initial : 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1500); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x1600);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_fast_stack_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "ccd_taken_frame", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.taken && !m_pipeline_store_writes && at == 0x2fff && word == 0x604 && m_cpu->pipeline_cycles() == 7, "Only taken CCD writes one PC+4 frame at the retained Execute boundary"); ++m_pipeline_store_writes; });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "ccd_return_frame", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.taken && !m_pipeline_operand_reads && m_pipeline_store_writes == 1 && at == 0x2fff && word == 0x604, "CCD callee returns through the original retained frame once"); ++m_pipeline_operand_reads;
		auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x2fff, 0xdead); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x603, "ccd_literal_and_delay", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == f.base + 0x601) { expect(!m_pipeline_extensions_reads && word == 0x700, "CCD retains one target literal"); ++m_pipeline_extensions_reads; return; }
		unsigned const i = m_fast_stack_reads++;
		u16 const expected = f.delay(i);
		expect(i < 2 && at == f.base + 0x602 + i && word == expected, "CCD fetches each delay word once on both paths"); });
	m_operand_data_tap = program.install_read_tap(f.base + 0x700, f.base + 0x700, "ccd_selected_target", [this, f](offs_t, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.taken && !m_pipeline_drained_fetches && word == 0x6d96, "Only taken CCD reaches the callee once"); ++m_pipeline_drained_fetches; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255420;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_delayed_conditional_call_edge(unsigned variant)
{
	u32 const base = (variant / 8) % 2 ? 0x20000 : 0;
	start_delayed_conditional_call(base ? 192 : 0); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_operand_data_tap.remove();
	m_cpu->space(AS_PROGRAM).write_word(base + 0x600, 0xfb00 | std::array<u16, 8>{0, 2, 3, 8, 0x0c, 0x24, 0x34, 0x70}[variant % 8]);
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "ccd_edge_no_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "ccd_edge_no_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_phase = 255440; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

tms320c54x_test_state::block_conditional_call_fixture tms320c54x_test_state::block_conditional_call_case(unsigned v)
{
	return {std::array<unsigned, 4>{0, 1, 2, 7}[v % 4], std::array<unsigned, 4>{4, 12, 13, 18}[v / 256],
		bool((v / 4) % 2), bool((v / 8) % 2), bool((v / 16) % 2), bool((v / 32) % 2), bool((v / 64) % 2), (v / 128) % 2 ? 0x20000U : 0U};
}

void tms320c54x_test_state::start_block_conditional_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = block_conditional_call_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); }
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, f.end());
	program.write_word(f.base + 0x604, (f.b ? 0xe900 : 0xe800) | unsigned(!f.zero)); program.write_word(f.base + 0x605, 0xe589);
	program.write_word(f.base + 0x606, (f.equal ? 0xfb45 : 0xfb44) + (f.b ? 8 : 0)); program.write_word(f.base + 0x607, 0x700);
	program.write_word(f.base + 0x608, f.long_delay ? (f.b ? 0xf120 : 0xf020) : (f.b ? 0xe900 : 0xe800) | unsigned(f.zero));
	program.write_word(f.base + 0x609, f.long_delay ? unsigned(f.zero) : 0x6d95);
	if (!f.endpoint) program.write_word(f.base + 0x60a, 0x6d94);
	program.write_word(f.base + f.end() + 1, 0x6d97); program.write_word(f.base + f.end() + 2, 0xf4e1);
	program.write_word(f.base + 0x700, 0x6d96); program.write_word(f.base + 0x701, 0xfc00);
	for (unsigned i = 0; i <= f.count; ++i) { data.write_word(0x5000 + i, 0x1357 ^ (i * 0x2345)); data.write_word(0x6000 + i, 0xdead); }
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_RTN, 0x5555);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.b ? 0x123456789aULL : 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ? 0 : 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x6000);
	for (unsigned r = 4; r < 8; ++r) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + r, 0x1000 + r * 0x100);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_fast_stack_reads = m_pipeline_register_writes = m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_ccd_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.taken() && m_fast_stack_reads < m_pipeline_register_writes && word == 0x60a, "Block CCD callee reads the original PC+4 frame once per taken call"); ++m_fast_stack_reads; auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x2fff, 0xdead); return; }
		unsigned const i = m_pipeline_operand_reads++; expect(i <= f.count && at == 0x5000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Block CCD preserves each ordered body read across calls"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_ccd_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.taken() && m_pipeline_register_writes <= f.count && word == 0x60a && m_pipeline_register_writes == m_fast_stack_reads, "Block CCD stacks PC+4, not a synthesized RSA continuation"); ++m_pipeline_register_writes; return; }
		unsigned const i = m_pipeline_store_writes++; expect(i <= f.count && at == 0x6000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Block CCD preserves each ordered body write across calls"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x607, f.base + 0x607, "block_ccd_target_literal", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_extensions_reads <= f.count && word == 0x700, "Each block iteration fetches the same CCD target once"); ++m_pipeline_extensions_reads; });
	m_operand_data_tap = program.install_read_tap(f.base + 0x700, f.base + 0x700, "block_ccd_target", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(f.taken() && m_pipeline_drained_fetches <= f.count && word == 0x6d96, "Only taken block CCD reaches its callee"); ++m_pipeline_drained_fetches; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255460;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

u16 tms320c54x_test_state::block_bcd_source(unsigned variant, unsigned iteration)
{
	unsigned const pattern = (variant / 48) % 4;
	return pattern == 0 ? 0 : pattern == 1 ? 1 : pattern == 2 ? 0xffff : iteration & 1 ? 0 : 0x8000;
}

bool tms320c54x_test_state::block_bcd_taken(unsigned variant, u16 word)
{
	s16 const value = word;
	switch (variant % 6)
	{
	case 0: return value >= 0; case 1: return value < 0; case 2: return value != 0;
	case 3: return value == 0; case 4: return value > 0; default: return value <= 0;
	}
}

void tms320c54x_test_state::start_block_bcd(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const unconditional = !strcmp(machine().options().bios(), "rptb-bd");
	bool const tc = !strcmp(machine().options().bios(), "rptb-tc-delay");
	bool const banz = !strcmp(machine().options().bios(), "rptb-banz");
	if (unconditional || tc) variant *= 6;
	unsigned const count = std::array<unsigned, 4>{0, 1, 2, 7}[banz ? variant % 4 : (variant / 12) % 4];
	bool const b = !banz && variant % 12 >= 6, wide = !banz && (variant / 192) % 2;
	u32 const base = banz ? block_banz_case(variant).base : (variant / 384) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf272); program.write_word(base + 0x601, 0x610);
	program.write_word(base + 0x604, tc ? 0x3492 : b ? 0x1192 : 0x1092);
	program.write_word(base + 0x605, tc ? b ? 0xfa30 : 0xfa20 : unconditional ? 0xf273 : 0xfa42 + variant % 6 + (b ? 8 : 0)); program.write_word(base + 0x606, 0x60d);
	program.write_word(base + 0x607, wide ? b ? 0xf300 : 0xf000 : 0x6d94); program.write_word(base + 0x608, wide ? 1 : 0x6d95);
	if (tc && !wide) program.write_word(base + 0x607, b ? 0xf4bc : 0xf5bc);
	program.write_word(base + 0x609, 0x7693); program.write_word(base + 0x60a, 0x1111);
	program.write_word(base + 0x60b, 0xf073); program.write_word(base + 0x60c, 0x60f);
	program.write_word(base + 0x60d, 0x7693); program.write_word(base + 0x60e, 0x2222);
	program.write_word(base + 0x60f, 0x6d96); program.write_word(base + 0x611, 0x6d97); program.write_word(base + 0x612, 0xf4e1);
	if (banz)
	{
		auto const f = block_banz_case(variant);
		program.write_word(base + 0x605, 0x6ce0 + f.form * 8);
		program.write_word(base + 0x606, f.offset); program.write_word(base + 0x607, 0x60d);
		program.write_word(base + 0x608, 0x6d95);
	}
	for (unsigned i = 0; i <= count; ++i) { data.write_word(0x5000 + i, block_bcd_source(variant, i)); data.write_word(0x6000 + i, 0xdead); }
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, count); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2345);
	if (tc) m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
	for (unsigned ar = 2; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == 2 ? 0x5000 : ar == 3 ? 0x6000 : 0x1000 + ar * 0x100);
	if (banz) { m_cpu->set_state_int(tms320c54x_device::STATE_AR0, block_banz_case(variant).initial); m_cpu->set_state_int(tms320c54x_device::STATE_BK, block_banz_case(variant).bk); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_bcd_read", [this, variant, count](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_operand_reads++; expect(i <= count && at == 0x5000 + i && word == block_bcd_source(variant, i) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == count - i && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (i < count), "Delayed internal branches retain ordered sources and Decode-owned loop counters");
		auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, word ^ 0xffff);
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_bcd_write", [this, variant, count, unconditional, banz, tc, b](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_store_writes++; bool const taken = tc ? bool(block_bcd_source(variant, i) & 0x8000) == b : banz ? block_banz_resolved(variant, i) != 0 : unconditional || block_bcd_taken(variant, block_bcd_source(variant, i)); expect(i <= count && at == 0x6000 + i && word == (taken ? 0x2222 : 0x1111), "Internal branch publishes only its selected arm"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + (banz ? 0x607 : 0x606), base + (banz ? 0x607 : 0x606), "block_bcd_target", [this, count](offs_t, u16 &word, u16) { if (!machine().side_effects_disabled()) { expect(word == 0x60d && m_pipeline_extensions_reads <= count, "Internal branch captures its target exactly once per iteration"); ++m_pipeline_extensions_reads; } });
	m_phase = 255560; m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_repeat_rearm(unsigned variant)
{
	bool const edge = !strcmp(machine().options().bios(), "rearm-edge");
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const first = std::array<unsigned, 4>{0, 1, 3, 7}[variant % 4], second = (variant / 4) % 2 ? 2 : 0;
	u32 const base = (edge ? variant % 2 : (variant / 8) % 2) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	std::array<u16, 14> const words{0xf072, 0x604, 0x6d92, 0xf495, 0xf495, 0x771a, u16(second), 0xf072, 0x60b, 0x6d93, 0xf495, 0xf495, 0x6d94, 0xf4e1};
	for (unsigned i = 0; i < words.size(); ++i) program.write_word(base + 0x600 + i, words[i]);
	if (edge) { program.write_word(base + 0x601, 0x60b); program.write_word(base + 0x602, 0x771a); program.write_word(base + 0x603, 0); }
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, edge ? 3 : first); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1200); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1300); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1400);
	m_pipeline_store_writes = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "repeat_rearm_no_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Repeat rearm does not read data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "repeat_rearm_write", [this, second, edge](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!edge && at == 0x1a && word == second && !m_pipeline_store_writes, "Only the post-loop STM publishes the next BRC once"); ++m_pipeline_store_writes; });
	m_phase = edge ? 255540 : 255520; m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::saved_repeat_fixture tms320c54x_test_state::body_repeat_case(unsigned v)
{
	return {std::array<unsigned, 4>{1, 2, 7, 56}[v % 4], std::array<unsigned, 4>{0, 1, 3, 63}[(v / 4) % 4], v / 128,
		false, bool((v / 64) % 2), bool((v / 16) % 2), (v / 32) % 2 ? 0x20000U : 0U, 0x3000};
}

u16 tms320c54x_test_state::body_repeat_word(saved_repeat_fixture const &f, unsigned slot) const
{
	return slot == 1 ? m_saved_repeat_counts.return_pc : slot == 2 ? f.base >> 16 :
		slot == 3 ? 0xa608 : slot == 4 ? 0x8900 : slot == 5 ? f.count : slot == 6 ? 0x604 : 0x60b;
}

void tms320c54x_test_state::start_body_repeat(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = body_repeat_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x900 + i, 0xf495); }
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, 0x60b);
	program.write_word(f.base + 0x604, 0xe589); program.write_word(f.base + 0x60a, 0x6d96);
	program.write_word(f.base + 0x60c, 0x6d97); program.write_word(f.base + 0x60d, 0xf4e1);
	program.write_word(f.base + f.vector(), 0x4a1e);
	program.write_word(f.base + f.vector() + 1, 0xf880 | (f.base >> 16)); program.write_word(f.base + f.vector() + 2, 0x900);
	// Software owns the only repeat context: save it, use the controller
	// for an ISR loop, then restore REA/RSA/BRC before ST1 and FRETE[D].
	std::array<u16, 25> const handler{0x4a06, 0x4a07, 0x4a1a, 0x4a1b, 0x4a1c, 0xf495,
		0x771a, u16(f.inner), 0xf072, 0x90b, 0x7694, 0x55aa, 0xf495,
		0x8a1c, 0x8a1b, 0x8a1a, 0xf495, 0xf495, 0xf495, 0xf495, 0xf495, 0xf495,
		0x8a07, 0x8a06, u16(f.delayed ? 0xf6e5 : 0xf4e5)};
	for (unsigned i = 0; i < handler.size(); ++i) program.write_word(f.base + 0x900 + i, handler[i]);
	for (unsigned i = 0; i < 64; ++i) { data.write_word(0x5000 + i, 0x1357 ^ (i * 0x2345)); data.write_word(0x6000 + i, 0xdead); data.write_word(0x7100 + i, 0xdead); }
	m_cpu->set_pipeline_frontend(false); m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x100); m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.sp);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2345);
	for (unsigned ar = 2; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, std::array<u16, 6>{0x5000, 0x6000, 0x7100, 0x1500, 0x1600, 0x1700}[ar - 2]);
	m_saved_repeat_counts = {}; m_saved_repeat_checkpoint = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "body_repeat_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_saved_repeat_counts;
		if (at >= 0x5000 && at < 0x5040) { unsigned const i = c.outer_reads++; expect(i <= f.count && at == 0x5000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Direct IRQ return retains ordered outer sources"); return; }
		if (at >= f.sp - 7 && at < f.sp) { expect(word == body_repeat_word(f, f.sp - at), "ISR pops the CPU-published return and software-saved repeat frame"); ++c.stack_reads; auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xdead); return; }
		std::array<u16, 6> const registers{0x1e, 6, 7, 0x1a, 0x1b, 0x1c};
		expect(c.mapped_reads < registers.size() && at == registers[c.mapped_reads] && word == body_repeat_word(f, c.mapped_reads + 2), "ISR saves XPC status and loop registers in order"); ++c.mapped_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "body_repeat_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_saved_repeat_counts;
		if (at >= 0x6000 && at < 0x6040) { unsigned const i = c.outer_writes++; expect(i <= f.count && at == 0x6000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Direct IRQ return preserves every outer destination"); if (!c.assertions) { ++c.assertions; m_cpu->set_input_line(f.nmi ? INPUT_LINE_NMI : 0, ASSERT_LINE); } return; }
		if (at >= 0x7100 && at < 0x7140) { unsigned const i = c.inner_writes++; expect(i <= f.inner && at == 0x7100 + i && word == 0x55aa, "ISR loop executes its own count"); if (!i) c.inner_cycle = m_cpu->pipeline_cycles(); return; }
		if (at >= f.sp - 7 && at < f.sp) { if (at == f.sp - 1) { expect(c.vectors == 1 && word >= 0x605 && word <= 0x60b, "IRQ interrupts the loop body without a near-return intermediary"); c.return_pc = word; } expect(word == body_repeat_word(f, f.sp - at), "One hardware entry and six software pushes balance the ISR frame"); ++c.stack_writes; return; }
		std::array<u16, 6> const registers{0x1a, 0x1c, 0x1b, 0x1a, 7, 6};
		std::array<u16, 6> const words{u16(f.inner), 0x60b, 0x604, u16(f.count), 0x8900, 0xa608};
		expect(c.controls < registers.size() && at == registers[c.controls] && word == words[c.controls], "ISR restores the architectural repeat context before direct return"); ++c.controls;
		if (c.controls == 4) c.restore_cycle = m_cpu->pipeline_cycles();
		if (c.controls == 5) c.st1_cycle = m_cpu->pipeline_cycles();
	});
	m_vector_prefetch_tap = program.install_read_tap(f.base + f.vector(), f.base + f.vector(), "body_repeat_vector", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; auto &c = m_saved_repeat_counts; expect(c.assertions == 1 && !c.vectors && word == 0x4a1e, "One physical IRQ reaches the ISR"); ++c.vectors; c.vector_cycle = m_cpu->pipeline_cycles(); m_cpu->set_input_line(f.nmi ? INPUT_LINE_NMI : 0, CLEAR_LINE); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x605, f.base + 0x60b, "body_repeat_resumed", [this, f](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return; auto &c = m_saved_repeat_counts; if (c.controls == 6 && !c.resumed && at == f.base + c.return_pc) { ++c.resumed; c.resume_cycle = m_cpu->pipeline_cycles(); } });
	m_phase = 255840; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::repeat_context_fixture tms320c54x_test_state::repeat_context_case(unsigned v)
{
	return {bool(v % 2), bool((v / 8) % 2), std::array<unsigned, 4>{0, 1, 2, 7}[(v / 2) % 4],
		std::array<unsigned, 6>{4, 12, 18, 22, 26, 34}[v / 64], (v / 16) % 2 ? 0x20000U : 0U, u16(0x3000 + (v / 32) % 2)};
}

u16 tms320c54x_test_state::repeat_context_word(repeat_context_fixture const &f, unsigned part) const
{
	return part == 0 ? f.end() : part == 1 ? f.start() : f.replace ? f.count : f.count - (m_repeat_context_counts.body_reads - 1);
}

void tms320c54x_test_state::start_repeat_context(unsigned variant)
{
	bool const pmst = !strcmp(machine().options().bios(), "repeat-pmst");
	bool const indirect = !strcmp(machine().options().bios(), "repeat-popd");
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = repeat_context_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); program.write_word(f.base + f.start() + i, 0xf495); }
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, f.old_end());
	program.write_word(f.base + 0x604, 0xe589); program.write_word(f.base + 0x605, 0xe800);
	program.write_word(f.base + 0x606, 0xfb45); program.write_word(f.base + 0x607, 0x700);
	program.write_word(f.base + 0x60a, 0x6d94); program.write_word(f.base + f.old_end() + 1, 0x6d97); program.write_word(f.base + f.old_end() + 2, 0xf4e1);
	if (f.replace)
	{
		// A synthetic CPU context-switch program, not a handset fixture:
		// restore a RAM frame and replace the return PC through an actual ST.
		program.write_word(f.base + 0x700, 0xeefd);
		program.write_word(f.base + 0x701, 0x8a1c); program.write_word(f.base + 0x702, 0x8a1b); program.write_word(f.base + 0x703, 0x8a1a);
		program.write_word(f.base + 0x704, 0x76f8); program.write_word(f.base + 0x705, f.sp - 1); program.write_word(f.base + 0x706, f.start());
		program.write_word(f.base + 0x70d, 0xfc00);
		data.write_word(f.sp - 4, f.end()); data.write_word(f.sp - 3, f.start()); data.write_word(f.sp - 2, f.count);
		program.write_word(f.base + f.start(), 0xe589); program.write_word(f.base + f.start() + 1, 0x6d94);
		program.write_word(f.base + f.end() + 1, 0x6d97); program.write_word(f.base + f.end() + 2, 0xf4e1);
	}
	else
	{
		program.write_word(f.base + 0x700, 0x4a1a); program.write_word(f.base + 0x701, 0x4a1b); program.write_word(f.base + 0x702, 0x4a1c);
		program.write_word(f.base + 0x703, 0x8a1c); program.write_word(f.base + 0x704, 0x8a1b); program.write_word(f.base + 0x705, 0x8a1a);
		program.write_word(f.base + 0x70c, 0xfc00);
	}
	if (indirect)
	{
		unsigned const first = f.replace ? 0x701 : 0x705;
		if (!f.replace)
		{
			program.write_word(f.base + 0x703, 0xf495); program.write_word(f.base + 0x704, 0xf495);
			program.write_word(f.base + 0x70c, 0xf495); program.write_word(f.base + 0x70e, 0xfc00);
		}
		program.write_word(f.base + first, 0x8b80);
		program.write_word(f.base + first + 1, 0x8b81);
		program.write_word(f.base + first + 2, 0x8b85);
	}
	if (pmst)
	{
		program.write_word(f.base + (f.replace ? 0x70d : 0x70c), 0xf495);
		program.write_word(f.base + 0x70e, 0x4a1d);
		program.write_word(f.base + 0x711, 0x8b80);
		program.write_word(f.base + 0x718, 0xfc00);
	}
	// Six NOP words after the BRC pop separate restoration from a loop
	// endpoint (SPRU131G table 7-25); shorter gaps are not accepted here.
	for (unsigned i = 0; i < 16; ++i) { data.write_word(0x5000 + i, 0x1357 ^ (i * 0x2345)); data.write_word(0x6000 + i, 0xdead); }
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900); m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.sp);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.replace ? 3 : f.count); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x6000); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x1700);
	if (pmst) m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1d);
	if (indirect)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1c);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1b);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1a);
	}
	m_repeat_context_counts = {}; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "repeat_context_reads", [this, f, pmst](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_repeat_context_counts;
		if (pmst && at == 0x1d) { expect(!word && c.pmst_reads == c.pmst_writes, "PSHM captures unchanged PMST once per active-context call"); ++c.pmst_reads; return; }
		if (pmst && at == f.sp - 2 && c.pmst_saves > c.pmst_pops) { expect(!word, "POPD captures the saved PMST once"); ++c.pmst_pops; auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xffff); return; }
		if (at >= 0x1a && at <= 0x1c) { unsigned const part = c.mapped_reads++ % 3; expect(!f.replace && at == 0x1a + part && word == repeat_context_word(f, 2 - part), "PSHM captures the actual current repeat context in order"); return; }
		if (at == f.sp - 1) { expect(c.frame_reads < f.calls() && word == (f.replace ? f.start() : 0x60a), "Near return captures the CPU-published context continuation once"); ++c.frame_reads; auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xdead); return; }
		if (at >= f.sp - 4 && at <= f.sp - 2) { unsigned const part = c.restore_reads++ % 3; expect(at == f.sp - 4 + part && word == repeat_context_word(f, part), "POPM reads one retained REA RSA BRC frame in order"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xdead); return; }
		unsigned const i = c.body_reads++; expect(i < f.copies() && at == 0x5000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Restored repeat context owns each ordered body source once"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "repeat_context_writes", [this, f, pmst](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_repeat_context_counts;
		if (pmst && at == 0x1d) { expect(!word && c.pmst_writes < c.pmst_pops, "POPD publishes retained PMST once outside the active body"); ++c.pmst_writes; return; }
		if (pmst && at == f.sp - 2 && c.pmst_reads > c.pmst_saves) { expect(!word, "PSHM publishes its retained PMST frame once"); ++c.pmst_saves; return; }
		if (at >= 0x1a && at <= 0x1c) { unsigned const part = c.control_writes++ % 3; expect(at == 0x1c - part && word == repeat_context_word(f, part) && c.control_writes <= c.restore_reads, "POPM publishes the captured repeat registers once in order"); return; }
		if (at == f.sp - 1) { bool const replacement = f.replace && c.frame_writes; expect(c.frame_writes < (f.replace ? 2U : f.calls()) && word == (replacement ? f.start() : 0x60a), "Only CPU call and explicit context-switch ST publish the return frame"); ++c.frame_writes; return; }
		if (at >= f.sp - 4 && at <= f.sp - 2) { unsigned const part = c.saved_writes++ % 3; expect(!f.replace && at == f.sp - 2 - part && word == repeat_context_word(f, 2 - part), "PSHM saves BRC RSA REA once before their corresponding pops"); return; }
		unsigned const i = c.body_writes++; expect(i < f.copies() && at == 0x6000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Restored context owns each ordered body destination once"); });
	m_phase = 255480; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_conditional_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const tc_call = !strcmp(machine().options().bios(), "tc-call");
	bool const returning = !strcmp(machine().options().bios(), "conditional-ret");
	bool const signed_call = !strcmp(machine().options().bios(), "signed-call"), all_conditions = returning || signed_call;
	unsigned const patterns = all_conditions ? 8 : 4, condition = all_conditions ? (variant / 32) % 6 : 3;
	u64 const value = std::array<u64, 8>{0, 1, 0x8000000000ULL, 0xffffffffffULL, 0x7fffffffffULL, 0x0100000000ULL, 0x0080000000ULL, 0x00ffffffffULL}[variant % patterns];
	s64 const signed_value = value & 0x8000000000ULL ? s64(value) - 0x10000000000LL : s64(value);
	bool const b = (variant / patterns) % 2;
	bool const taken = tc_call ? bool((variant / 2) & 1) == bool(variant & 1) : std::array<bool, 6>{signed_value >= 0, signed_value < 0, signed_value != 0, signed_value == 0, signed_value > 0, signed_value <= 0}[condition];
	u32 const base = (variant / (patterns * 2)) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, returning ? 0xfc42 + condition + (b ? 8 : 0) : signed_call ? 0xf942 + condition + (b ? 8 : 0) : b ? 0xf94d : 0xf945); program.write_word(base + 0x601, returning ? 0xf4e1 : 0x700); program.write_word(base + 0x602, 0xf4e1); program.write_word(base + 0x701, 0xfc00);
	if (tc_call) program.write_word(base + 0x600, variant & 1 ? 0xf930 : 0xf920);
	if (returning) { program.write_word(base + 0x700, 0xf4e1); m_cpu->space(AS_DATA).write_word(0x1000, 0x700); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	if (tc_call) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608 | (((variant / 2) & 1) << 12));
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, b ? 0x1234 : value); m_cpu->set_state_int(tms320c54x_device::STATE_B, b ? value : 0x2345);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_fast_stack_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "cc_taken_frame",
		[this, taken, returning](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!returning && taken && !m_pipeline_store_writes && at == 0xfff && word == 0x602 && m_cpu->pipeline_cycles() == 7, "Only taken CC writes a stack frame; RC never writes data"); ++m_pipeline_store_writes; });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "cc_return_frame",
		[this, taken, returning](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(taken && !m_pipeline_operand_reads && at == (returning ? 0x1000 : 0xfff) && word == (returning ? 0x700 : 0x602) && (!returning || m_cpu->pipeline_cycles() == 5), "RC/RET consumes one frame only on the taken path"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "cc_target",
		[this, taken, returning](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return; expect(taken && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == (returning ? 7 : 8), "Conditional target Fetch follows owned stack publication or sampling"); ++m_pipeline_extensions_reads; });
	m_operand_data_tap = program.install_read_tap(base + (returning ? 0x900 : 0x601), base + (returning ? 0x900 : 0x601), "cc_literal",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_fast_stack_reads && word == 0x700, "CC retains its target literal once"); ++m_fast_stack_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253810;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::conditional_register_fixture tms320c54x_test_state::conditional_register_case(unsigned index)
{
	unsigned const condition = index % 12, pattern = (index / 12) % 6, x = (index / 72) % 16;
	u64 const tested = std::array<u64, 6>{0, 1, 0xffffffffffULL, 0x7fffffff, 0xff80000000ULL, 0x0100000000ULL}[pattern];
	bool const negative = pattern == 2 || pattern == 4;
	bool const predicates[] = {!negative, negative, pattern != 0, pattern == 0, !negative && pattern != 0, negative || pattern == 0};
	u16 const address = 0x1403 + (x & 3) * 16;
	u16 const next = (x >> 2) == 0 ? address : (x >> 2) == 1 ? address - 1 : (x >> 2) == 2 ? address + 1 : address - 2;
	return {bool((index / 4608) % 2), predicates[condition % 6], x,
		std::array<u16, 4>{0, 0xffff, 0x1234, 0x8000}[(index / 1152) % 4], address, next,
		condition < 6 ? tested : 0x23456789ULL, condition < 6 ? 0x23456789ULL : tested};
}

void tms320c54x_test_state::start_xc_barrier(unsigned index)
{
	unsigned const kind = index % 3;
	bool const initial = (index / 3) % 2;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x600, initial ? 0xf4bc : 0xf5bc);
	program.write_word(0x601, kind == 0 ? 0xfc00 : kind == 1 ? 0xfc45 : 0xfc44);
	program.write_word(0x602, initial ? 0xfd20 : 0xfd30);
	program.write_word(0x603, 0xe933); program.write_word(0x604, 0xf495); program.write_word(0x605, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x1000, 0x602);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x600);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xe05 | (initial ? 0x1000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_pipeline_frontend(index < 12);
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 524000 + index;
	if (!((index / 6) % 2))
	{
		m_saved_pipeline.str({}); m_saved_pipeline.clear();
		expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save status producer and sequential-target return barrier");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::xc_latency_fixture tms320c54x_test_state::xc_latency_case(unsigned index)
{
	unsigned const kind = (index / 16) % 6;
	return {index % 4, kind, 1 + (index / 96) % 2, kind >= 4 ? 2U : 1U,
		bool((index / 8) % 2), bool((index / 4) % 2), index < 384, bool((index / 192) % 2)};
}

void tms320c54x_test_state::start_xc_latency(unsigned index)
{
	auto const f = xc_latency_case(index);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 20; ++i) program.write_word(0x600 + i, 0xf495);
	unsigned pc = 0x600;
	if (f.kind < 2) program.write_word(pc++, (f.initial ? 0xf4b0 : 0xf5b0) | (f.kind ? 11 : 12));
	else if (f.kind < 4) program.write_word(pc++, 0xe800 | ((f.kind & 1) << 8) | !f.initial);
	else { program.write_word(pc++, 0xf020 | ((f.kind & 1) << 8)); program.write_word(pc++, !f.initial); }
	pc += f.gap;
	u16 const predicate = f.kind == 0 ? (f.polarity ? 0x30 : 0x20) : f.kind == 1 ? (f.polarity ? 0x0c : 8) :
		0x40 | ((f.kind & 1) << 3) | (f.polarity ? 4 : 5);
	program.write_word(pc++, (f.slots == 2 ? 0xff00 : 0xfd00) | predicate);
	if (f.slots == 2)
		program.write_word(pc++, f.kind < 2 ? ((f.polarity ? 0xf4b0 : 0xf5b0) | (f.kind ? 11 : 12)) :
			0xe800 | ((f.kind & 1) << 8) | !f.polarity);
	program.write_word(pc++, f.kind >= 2 && !(f.kind & 1) ? 0xe933 : 0xe833);
	program.write_word(pc + 1, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x600);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.kind >= 2 && !(f.kind & 1) ? f.initial : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.kind >= 2 && (f.kind & 1) ? f.initial : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.status(false));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_pipeline_frontend(f.staged);
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 523000 + index;
	if (!f.snapshot)
	{
		m_saved_pipeline.str({}); m_saved_pipeline.clear();
		expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save XC condition source before its producer");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_xc_status(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned const mode = index % 32, gap = mode % 4;
	bool const set = (mode / 4) & 1, initial = (mode / 8) & 1, tc = (mode / 16) & 1;
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, set ? 0xf5bc : 0xf4bc);
	program.write_word(0x601 + gap, tc ? 0xfd30 : 0xfd20);
	program.write_word(0x602 + gap, 0xe833);
	program.write_word(0x604 + gap, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x600);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xe05 | (initial ? 0x1000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_pipeline_frontend(index < 64);
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 522000 + index;
	if (!(index & 32))
	{
		m_saved_pipeline.str({}); m_saved_pipeline.clear();
		expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save XC/status producer before Access sampling or Execute publication");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_atomic_conditional_return(unsigned index)
{
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	auto const f = conditional_register_case(index % 72);
	unsigned const condition = index % 12;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	program.write_word(0x600, 0xfc40 | (condition < 6 ? condition + 2 : condition + 4));
	program.write_word(0x601, 0xe811); program.write_word(0x602, 0xf4e1);
	program.write_word(0x700, 0xe822); program.write_word(0x701, 0xf4e1);
	data.write_word(0x1000, 0x700);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x600);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1e05);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "atomic_conditional_return_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.take && at == 0x1000 && word == 0x700 && !m_pipeline_operand_reads++, "Atomic conditional return reads one stack word only when taken"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "atomic_conditional_return_write", [this](offs_t, u16 &, u16) {
		if (!machine().side_effects_disabled()) expect(false, "Atomic conditional return never writes the stack"); });
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 519000 + index;
	if (index < 72)
	{
		m_recurring_irq_snapshot = {0};
		m_saved_pipeline.str({}); m_saved_pipeline.clear();
		expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save atomic return before predicate and optional stack read");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_atomic_overflow_branch(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned const flags = (index >> 3) & 3, accumulator = (index >> 1) & 1;
	program.write_word(0x600, 0xf860 | ((index & 1) << 9) | (accumulator << 3) | (((index >> 2) & 1) << 4));
	program.write_word(0x601, 0x700);
	program.write_word(0x602, 0xf5b0 | (10 - accumulator));
	program.write_word(0x603, 0xe955);
	program.write_word(0x604, 0xe811); program.write_word(0x605, 0xf4e1);
	program.write_word(0x700, 0xe822); program.write_word(0x701, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x600);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1805 | ((flags & 1) << 10) | ((flags & 2) << 8));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 305000 + index;
	if (!(index & 32))
	{
		m_saved_pipeline.str({}); m_saved_pipeline.clear();
		expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save overflow branch before predicate sampling");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_repeated_writa_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == repeated_writa_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const standard_width = !strcmp(machine().options().bios(), "rpt-writa16"); auto const f = repeated_writa_case(variant, standard_width);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Short-source WRITA accepts immediate and memory repeat setup");
		if (f.setup >= 2 && m_port_reads && !m_pipeline_register_writes)
		{
			auto const disable = machine().disable_side_effects();
			if (f.setup == 4) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ((f.reg + 1) % 8), f.count ^ 0xffff);
			else m_cpu->space(AS_DATA).write_word(f.count_address, f.count ^ 0xffff);
			m_pipeline_register_writes = 1;
		}
		if (tick == f.read) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead); }
		if (tick == f.read + 1) m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0056789a);
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_port_reads, m_pipeline_register_writes}; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save WRITA before PAR capture, at first write or during stream");
		}
		if (tick == f.write + f.count + 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_pipeline_operand_reads == f.count + 1 && m_pipeline_store_writes == f.count + 1 && m_port_reads == unsigned(f.setup >= 2) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0056789a && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x81abcdef01ULL && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa824 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x900 | (f.form == 1 ? 0x4000 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1200 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && (f.form < 2 || m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.reg) == u16(f.source + f.direction * int(f.count + 1))), "WRITA owns count-plus-one transfers and pointer progression without updating A or unrelated state");
			{ auto const disable = machine().disable_side_effects(); for (unsigned n = 0; n <= f.count; ++n) expect(m_cpu->space(AS_PROGRAM).read_word(repeated_writa_destination(f, n)) == repeated_writa_word(f.reg, n), "Repeated WRITA publishes actual program RAM, not just write notifications"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_port_reads = m_recurring_irq_snapshot[2]; m_pipeline_register_writes = m_recurring_irq_snapshot[3];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore repeated WRITA without duplicate first writes or lost PAR progression");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = repeated_writa_phase + 1; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (standard_width ? 19199 : 38399)) { start_repeated_writa(variant + 1); return true; }
			if (standard_width)
				osd_printf_info("TMS320C54x repeated WRITA: PASS cases=19200 restore=19200 sources=DP,SP,plain,postdec,postinc count_sources=short,long,direct,absolute_RAM,absolute_AR registers=8 counts=0,1,7,255 targets=ordinary,halfspace_cross,wrap16 address_bits=16 ovly=ignored pages=1 snapshots=4 retained_first_source=1 retained_par=1 count_reads=once data_reads=count+1 program_writes=count+1 stream_clocks=1 silicon_phase_claim=0\n");
			else
				osd_printf_info("TMS320C54x repeated WRITA: PASS cases=38400 restore=38400 sources=DP,SP,plain,postdec,postinc count_sources=short,long,direct,absolute_RAM,absolute_AR registers=8 counts=0,1,7,255 targets=ordinary,overlay_cross,wrap23 address_bits=23 ovly=2 pages=2 snapshots=4 retained_first_source=1 retained_par=1 count_reads=once data_reads=count+1 program_writes=count+1 stream_clocks=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeated WRITA complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == repeated_writa_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = repeated_writa_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_tc_return_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == tc_return_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, producer = (variant / 4) % 3, gap = (variant / 12) % 2;
		unsigned const offset = (producer == 2 ? 2 : producer) + gap, checkpoint = std::array<unsigned, 4>{3, 4, 5, 7}[variant / 96] + offset;
		bool const tc = (variant / 2) % 2, taken = tc == bool(variant % 2);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RC TC/NTC and adjacent BITT are admitted");
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) == (tick >= (taken ? 11 : 9) + offset), "TC/NTC return retains Class 32's taken/untaken cadence");
		if (tick == 5 + offset)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x2000, 0xdead);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa824 | (!tc ? 0x1000 : 0));
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_fast_stack_reads}; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save TC return around producer, condition sampling and retained target");
		}
		if (tick == 32)
		{
			u32 const base = (variant / 48) % 2 ? 0x20000 : 0;
			expect(m_cpu->state_int(STATE_GENPC) == base + (taken ? 0x9101 : 0x9002 + offset) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x2000 + unsigned(taken) &&
				m_pipeline_operand_reads == unsigned(producer != 0) && m_fast_stack_reads == unsigned(taken) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa824 | (!tc ? 0x1000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0xb00 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12345678ULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x81abcdef01ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == (0xa5f0 | ((variant / 24) % 2 ? 15 : 0)), "TC/NTC return retains predicate and target, preserves registers and never touches an untaken stack");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_fast_stack_reads = m_recurring_irq_snapshot[1]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore TC return without duplicate producer or stack accesses");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = tc_return_phase + 1; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_tc_return(variant + 1); return true; }
			osd_printf_info("TMS320C54x TC returns: PASS cases=384 restore=384 predicates=TC,NTC outcomes=2 producers=status,BITT_short,BITT_absolute gaps=0,1 selectors=0,15 pages=2 snapshots=4 predicate_retained=1 target_retained=1 untaken_stack_reads=0 data_writes=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated TC returns complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == tc_return_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = tc_return_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_bd_read_a_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == bd_read_a_phase)
	{
		auto const f=bd_read_a_case(m_pipeline_peripheral_case); unsigned const tick=++m_idle_fixture_tick;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"Short READA in BD delay spans is admitted");
		if(tick==f.read()) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR3)==unsigned(f.form==2 ? 0x4021 : 0x4020),"READA owns exactly one destination update at Read"); m_cpu->set_state_int(tms320c54x_device::STATE_AR3,0x7000); }
		if(tick==f.read()+1) { expect(m_cpu->state_int(tms320c54x_device::STATE_A)==f.source,"READA's program address includes the older ADD result"); m_cpu->set_state_int(tms320c54x_device::STATE_A,0x5a5a); }
		if(tick==f.checkpoint() && !m_pipeline_replay) { m_recurring_irq_snapshot={m_pipeline_operand_reads,m_pipeline_store_writes,m_port_reads}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline)==STATERR_NONE,"Save BD READA before or across retained program transfer"); }
		if(tick==32)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC)==f.base+0x701 && m_cpu->state_int(tms320c54x_device::STATE_SP)==0x4000 && m_cpu->state_int(tms320c54x_device::STATE_A)==0x5a5a && m_cpu->state_int(tms320c54x_device::STATE_B)==0xfeabcd9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T)==0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_ST0)==0x1080 && m_cpu->state_int(tms320c54x_device::STATE_ST1)==(0x900|(f.form==3 ? 0x4000 : 0)),"BD READA retains the branch path, program source, modes and sibling registers");
			expect(m_pipeline_operand_reads==1 && m_pipeline_store_writes==1 && m_port_reads==f.slot && m_cpu->state_int(tms320c54x_device::STATE_AR3)==0x7000 && m_cpu->state_int(tms320c54x_device::STATE_AR4)==0x5100,"READA neither resamples poisoned addresses nor duplicates transfers");
			if(!m_pipeline_replay) { m_pipeline_replay=true; m_idle_fixture_tick=f.checkpoint(); m_pipeline_operand_reads=m_recurring_irq_snapshot[0]; m_pipeline_store_writes=m_recurring_irq_snapshot[1]; m_port_reads=m_recurring_irq_snapshot[2]; m_cpu->set_pipeline_frontend(true); m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline)==STATERR_NONE,"Restore BD READA after clearing future pipeline state"); m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_phase=bd_read_a_phase+1; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if(m_pipeline_peripheral_case<511) { start_bd_read_a(m_pipeline_peripheral_case+1); return true; }
			osd_printf_info("TMS320C54x BD READA: PASS cases=512 restore=512 slots=2 forms=DP,plain,postinc,SP values=4 caller_pages=2 source_pages=2 snapshots=4 older_ADD=slot1 retained_PAR=1 retained_destination=1 source_poison=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated BD READA complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if(m_phase==bd_read_a_phase+1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=bd_read_a_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_rptz_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == rptz_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, target = variant % 2;
		unsigned const count = std::array<unsigned, 4>{0, 1, 3, 17}[(variant / 2) % 4], checkpoint = std::array<unsigned, 3>{3, 5, 7}[variant / 256];
		u32 const base = (variant / 128) % 2 ? 0x20000 : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged RPTZ setup and immediate-store body are admitted");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (tick >= 7 && !target ? 0 : 0xff12345678ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (tick >= 7 && target ? 0 : 0x81abcdef01ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa024 | (((variant / 8) % 4) << 11)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (((variant / 32) % 4) << 8)), "RPTZ clears only its selected 40-bit accumulator at Execute and preserves status");
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x9001, 0xffff); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot[0] = m_pipeline_extensions_reads; m_recurring_irq_snapshot[1] = m_pipeline_store_writes; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save RPTZ before or after count retention and clearing");
		}
		if (tick == 64)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_pipeline_extensions_reads == 1 &&
				m_pipeline_store_writes == count + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1200 + count + 1, "RPTZ uses retained count-plus-one and reaches its continuation once");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_extensions_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore staged RPTZ setup without count refetch");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = rptz_phase + 1; m_check_timer->enable(true); return true;
			}
			m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_rptz(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged RPTZ: PASS cases=768 restore=768 accumulators=2 counts=0,1,3,17 c_tc=4 sxm_ovm=4 pages=2 snapshots=3 count_plus_one=1 literal_reads=1 status_preserved=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated staged RPTZ complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == rptz_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = rptz_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_literal_guard_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == literal_guard_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, count = (variant / 3) % 3;
		unsigned const checkpoint = std::array<unsigned, 3>{4, 7, 12}[variant / 18];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Instruction admission must not classify a retained extension as an opcode");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_store_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save a block with an opcode-shaped extension");
		}
		if (tick == 64)
		{
			expect(m_pipeline_store_writes == count + 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE), "Literal-bearing block retires each complete ST exactly once");
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(std::array<u16, 3>{0x7cf8, 0x7df8, 0x7ff8}[variant % 3]) == 0x1357, "Block literal names the committed RAM location"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_store_writes = m_recurring_irq_snapshot[0];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore a retained extension without duplicate block stores");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = literal_guard_phase + 1; m_check_timer->enable(true); return true;
			}
			m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 53) { start_literal_guard(variant + 1); return true; }
			osd_printf_info("TMS320C54x opcode-shaped extensions: PASS cases=54 restore=54 literals=7cf8,7df8,7ff8 block_counts=0,1,2 pages=2 snapshots=3 instruction=ST_absolute writes=count+1 transfer_admission=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated opcode-shaped extensions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == literal_guard_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = literal_guard_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_carry_branch_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 1619)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 0xc800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x4800 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"RPTB publishes bit-15 BRAF while preserving independent bit-14 CPL on retirement");
		osd_printf_info("TMS320C54x BRAF/CPL independence: PASS\n");
		start_carry_branch_case(0);
		return true;
	}
	if (m_phase >= 1620 && m_phase < 1628)
	{
		unsigned const index = m_phase - 1620;
		bool const taken = bool(index & 1) == bool(index & 2);
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 0x2222 : 0x1111) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x02a5 | ((index & 1) ? 0x800 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE), "BC/BCD C/NC uses carry and preserves status");
		if (index < 7) { start_carry_branch_case(index + 1); return true; }
		osd_printf_info("TMS320C54x carry branch conformance: PASS variants=8\n");
		start_cmpm_offset_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_return_alt_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 2696 && m_phase < 2701)
	{
		unsigned const index = m_phase - 2696;
		bool const taken = index < 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (taken ? 0x1001 : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x09e1 : 0x09d2) &&
			m_cpu->state_int(tms320c54x_device::STATE_XPC) == 1,
			"RC ALT tests signed 40-bit A, consumes only a taken return and preserves XPC");
		if (index < 4) { start_return_alt_case(index + 1); return true; }
		osd_printf_info("TMS320C54x RC ALT conformance: PASS variants=5\n");
		start_software_interrupt_case(0);
		return true;
	}
	if (m_phase >= 2701 && m_phase < 2765)
	{
		unsigned const index = m_phase - 2701;
		unsigned const vector = index & 31;
		u16 const base = (index & 1) ? 0x4000 : 0x2000;
		u16 const flags = vector >= 16 ? u16(0xffff & ~(1U << (vector - 16))) : 0xffff;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == base + vector * 4 + 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_XPC) == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff && data.read_word(0x0fff) == 0x09d3 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0980 &&
			m_cpu->state_int(tms320c54x_device::STATE_IMR) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_IFR) == flags &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"INTR selects IPTR:K, ignores masks, saves next PC, sets INTM, clears only selected IFR and costs three cycles");
		program.write_word(0x10000 | (base + vector * 4 + 3), 0xf49b); // RETF
		program.write_word(0x0109d3, 0xf4e1);
		data.write_word(0x0fff, 0xdead); // RTN, not this stack word, must supply PC.
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 61000 + index;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_delayed_call_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 3337 && m_phase < 3346)
	{
		unsigned const index = m_phase - 3337;
		bool const taken = index == 1 || index == 2 || index == 3 || index == 5 || index == 8;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (taken ? 2 : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
			data.read_word(0x0fff) == (taken ? 0x09d4 : 0xbeef) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x09d5 &&
			m_cpu->state_int(tms320c54x_device::STATE_XPC) == 1,
			"CCD captures full-width A and combined overflow conditions, executes delay words and returns after them without changing XPC");
		if (index < 8) { start_delayed_call_case(index + 1); return true; }
		osd_printf_info("TMS320C54x delayed conditional call conformance: PASS variants=9\n");
		start_asm_store_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_braf_control_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 82000 && m_phase < 82004)
	{
		unsigned const index = m_phase - 82000;
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "BRAF control fixture terminates");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == (index == 3 ? 4 : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_BRC) == (index == 3 ? 0 : 3) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (index == 2 ? 0x1001 : 0x1000) &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000),
			"Architectural BRAF controls repeat cancellation and software loop restoration");
		if (index < 3) { start_braf_control_case(index + 1); return true; }
		osd_printf_info("TMS320C54x BRAF control: PASS cases=4 clear=RSBX,STM,POPM restored_loop=1\n");
		start_repeat_endpoint_case(0); return true;
	}
	if (m_phase >= 83000 && m_phase < 83012)
	{
		unsigned const index = m_phase - 83000;
		u16 const body = index % 3 == 0 ? 0x0802 : index % 3 == 1 ? 0xfffd : 0xfffe;
		osd_printf_info("C54x repeat endpoint case=%u body=%04x final_word=%04x continuation=%04x\n",
			index, body, data.read_word(0x0602), data.read_word(0x0fff));
		expect(m_repeat_boundary_requests == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Repeat endpoint fixture reaches interrupt vector");
		expect(data.read_word(0x0600) == 0x1234 && data.read_word(0x0601) == 0x1234 &&
			data.read_word(0x0602) == 0x1234 && data.read_word(0x0603) == 0xdead &&
			data.read_word(0x0fff) == u16(body + 2) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
			m_cpu->state_int(STATE_GENPC) == (index >= 6 ? 0x010005 : 0x010041),
			"Repeat final iteration retains address progression and IRQ/NMI protection at every endpoint");
		m_repeat_boundary_tap.remove();
		m_cpu->set_input_line(index >= 6 ? INPUT_LINE_NMI : 0, CLEAR_LINE);
		if (index < 11) { start_repeat_endpoint_case(index + 1); return true; }
		osd_printf_info("TMS320C54x repeat endpoints: PASS cases=12 endpoints=ordinary,ffff,0000 counts=short,long sources=irq,nmi\n");
		start_nested_repeat_case(0); return true;
	}
	if (m_phase >= 84000 && m_phase < 84004)
	{
		unsigned const index = m_phase - 84000;
		osd_printf_info("C54x nested repeat case=%u ar1=%04x brc=%04x st1=%04x\n", index,
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Nested repeat fixture terminates");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == (index & 1 ? 12 : 4) &&
			m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000),
			"Single repeat at block endpoint completes both architectural loop counts");
		if (index < 3) { start_nested_repeat_case(index + 1); return true; }
		osd_printf_info("TMS320C54x nested repeats: PASS cases=4 counts=zero,two forms=short,long\n");
		start_loop_xc_case(0); return true;
	}
	if (m_phase >= 85000 && m_phase < 85004)
	{
		unsigned const index = m_phase - 85000;
		auto &data = m_cpu->space(AS_DATA);
		osd_printf_info("C54x loop XC case=%u entry_a=%04x continuation=%04x\n", index,
			unsigned(m_irq_accumulator), data.read_word(0x0fff));
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Loop XC fixture reaches ISR");
		expect(m_irq_accumulator == (index & 1 ? 1 : 9) &&
			data.read_word(0x0fff) == (index & 1 ? 0x0802 : 0x0803),
			"XC word protection survives block-loop redirection and expires after two words");
		m_xc_irq_tap.remove();
		m_cpu->set_input_line(index >= 2 ? INPUT_LINE_NMI : 2, CLEAR_LINE);
		if (index < 3) { start_loop_xc_case(index + 1); return true; }
		osd_printf_info("TMS320C54x loop XC protection: PASS cases=4 body_words=1,2 sources=irq,nmi\n");
		start_operand_fetch_case(0); return true;
	}
	if (m_phase >= 86000 && m_phase < 86053)
	{
		unsigned const index = m_phase - 86000;
		unsigned const family = index >= 45 ? 10 : index >= 25 ? 6 + (index - 25) / 4 : index / 4;
		unsigned const form = index == 45 ? 4 : index >= 46 ? 3 : index >= 25 ? (index - 25) % 4 : index % 4;
		osd_printf_info("C54x operand fetch case=%u extensions=%u reads=%u early_read=%u\n",
			index, m_operand_extension_count, m_operand_reads, m_operand_order_violation);
		m_operand_fetch_tap.remove(); m_operand_data_tap.remove();
		m_operand_io_tap.remove(); m_operand_program_tap.remove();
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Operand fetch fixture terminates");
		expect(!m_operand_order_violation && m_operand_reads == (index == 24 ? 3 : 1),
			"Instruction operand words are fetched before data, I/O or program operand reads");
		auto &data = m_cpu->space(AS_DATA);
		if (index == 24)
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x189ed0000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0, "FIRS operand data preserved");
		else
		{
			if (family == 0) expect(data.read_word(0x0700) == 0xbeef, "MVDK operand data preserved");
			if (family == 1) expect(m_cpu->space(AS_PROGRAM).read_word(0x0700) == 0xbeef, "MVDP operand data preserved");
			if (family == 2) expect(m_port_writes == 1 && m_first_port_value == 0xbeef, "PORTW operand data preserved");
			if (family >= 3) expect(data.read_word(0x0600) == (family == 3 ? 0xabcd : 0xbeef), "Input operand data preserved");
			if (family == 6 || family == 7) expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000, "Comparison/test publishes TC");
			if (family == 8) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0701 &&
				data.read_word(0x0700) == 0xbeef, "POPD increments SP once and preserves popped stack contents");
			if (family == 9) expect(m_cpu->space(AS_PROGRAM).read_word(0x0900) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0900, "WRITA writes the distinct program destination without changing A");
			if (family == 10) expect(data.read_word(0x05ff) == 0xcafe &&
				m_cpu->space(AS_PROGRAM).read_word(0x0900) == 0xbeef, "MVPD preserves the preceding destination and program source");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) ==
				(form == 1 ? 0x0601 : form == 2 ? 0x05fb : 0x0600), "Address modifier preserved");
		}
		if (index < 52) { start_operand_fetch_case(index + 1); return true; }
		osd_printf_info("TMS320C54x operand fetch order: PASS cases=53 families=12 forms=direct,indirect,long,absolute,preincrement\n");
		start_cala_rtn_case(0); return true;
	}
	if (m_phase >= 87000 && m_phase < 87004)
	{
		unsigned const index = (m_phase - 87000) / 2;
		auto &data = m_cpu->space(AS_DATA);
		if (!(m_phase & 1))
		{
			expect(m_cpu->state_int(STATE_GENPC) == 0x0901 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x03ff &&
				data.read_word(0x03ff) == 0x0801, "CALA stacks its continuation");
			data.write_word(0x03ff, 0xdead);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(STATE_GENPC) == 0x0802 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			data.read_word(0x03ff) == 0xdead, "CALA publishes RTN independently of its stack word");
		if (!index) { start_cala_rtn_case(1); return true; }
		osd_printf_info("TMS320C54x accumulator call fast return: PASS cases=2 sources=A,B\n");
		start_native_gap_case(0); return true;
	}
	if (m_phase >= 88000 && m_phase < 88006)
	{
		unsigned const index = m_phase - 88000, family = index / 2;
		bool const signed_operand = index & 1;
		u64 const loaded = signed_operand ? 0xffffff8001ULL : 0x8001;
		u64 const a = family == 0 ? (signed_operand ? 0x8004 : 0xffffff8004ULL) : family == 1 ? loaded : 3;
		u64 const b = family == 2 ? loaded : 5;
		expect_opcode(family == 0 ? 0x6f95 : family == 1 ? 0x1087 : 0x1182,
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == a &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == b &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == (family == 0 ? 0x0601 : 0x0600) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0600 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x0600 &&
			m_cpu->space(AS_DATA).read_word(0x0600) == 0x8001,
			"Native 6f95/0e20 and 1087/1182 preserve source, addressing and SXM results");
		if (index < 5) { start_native_gap_case(index + 1); return true; }
		osd_printf_info("TMS320C54x native gap arithmetic/load: PASS words=3 cases=6 sxm=0,1\n");
		start_native_address_gap_case(0); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_native_math_gap_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 90000 && m_phase < 90008)
	{
		unsigned const index = m_phase - 90000, family = index / 2;
		bool const variant = index & 1;
		static constexpr u16 opcodes[] = {0xf67b, 0x3185, 0xf301, 0xf648};
		u64 const a = family == 0 ? (variant ? 0xfffffffffeULL : 0x07fffffffeULL) :
			family == 1 ? 0x20000 : family == 2 ? 0x1234 : (variant ? 0xfffffffe00ULL : 0x12345600);
		u64 const b = family == 0 ? 0xffffffffdfULL : family == 1 ?
			(variant ? 0xfffffffff4ULL : 0xfffffffffaULL) : family == 2 ?
			(variant ? 0x7ff0 : 0x8005) : (variant ? 0xfffffffffeULL : 0x123456);
		u16 const flags = family < 2 || (family == 2 && variant) ? 0x0800 : 0;
		expect_opcode(opcodes[family], m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == a &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == b &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (family == 1 ? 0xfffd : 0xbeef) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == flags &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x0600 &&
			m_cpu->space(AS_DATA).read_word(0x0600) == 0xfffd,
			"Native shifts, accumulator multiply and immediate add preserve exact results and carry");
		if (index < 7) { start_native_math_gap_case(index + 1); return true; }
		osd_printf_info("TMS320C54x native gap math: PASS words=4 cases=8 sxm=0,1 frct=0,1 carry=0,1\n");
		start_native_memory_gap_case(0); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_native_final_gap_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 150000 && m_phase < 150012)
	{
		unsigned const index = m_phase - 150000, family = index / 2;
		bool const variant = index & 1;
		static constexpr u16 opcodes[] = {0x9a5f, 0xf20f, 0xf47d, 0xf50f, 0xf736, 0xf761};
		u64 const a = family == 0 || family == 3 ? (variant ? 0xfffffffffeULL : 2) :
			family == 1 ? (variant ? 0x7ff0 : 0x8005) :
			family == 2 ? (variant ? 0x1ffffffffeULL : 0xfffffffffeULL) : 3;
		u64 const b = family == 1 ? (variant ? 0xfffffffff0ULL : 5) :
			family == 3 ? (variant ? 0xffffff0005ULL : 0x10005) :
			family == 4 ? (variant ? 0xfffffffc01ULL : 0x3ff) :
			family == 5 ? (variant ? 0x7fffffff : 0x80000000) : 5;
		u16 const flags = family == 2 || (family == 1 && variant) || (family == 4 && !variant) ? 0x0800 : family == 5 ? 0x0200 : 0;
		expect_opcode(opcodes[family], m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == a &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == b &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == flags &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == (family == 0 ? 0x05ff : 0x0600) &&
			m_cpu->space(AS_DATA).read_word(0x0600) == (family == 0 ? (variant ? 0xffff : 1) : 0xdead),
			"Final native gaps preserve shift/add/subtract results, saturation, carry and compact store addressing");
		if (index < 11) { start_native_final_gap_case(index + 1); return true; }
		osd_printf_info("TMS320C54x native final gaps: PASS words=6 cases=12 signed_shift=1 saturation=1 compact_store=1\n");
		start_indirect_popm_case(0); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_frontend_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251000)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		osd_printf_info("TMS320C54x staged tick: cycle=%llu expected=%llu replay=%u pc=%06x a=%llx b=%llx sp=%04x ifr=%04x illegal=%u\n",
			static_cast<unsigned long long>(cycle), static_cast<unsigned long long>(m_pipeline_expected_cycle), m_pipeline_replay,
			unsigned(m_cpu->state_int(STATE_GENPC)), static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_A)),
			static_cast<unsigned long long>(m_cpu->state_int(tms320c54x_device::STATE_B)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"Staged frontend advances exactly one clock per observation without unsupported forms");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle < 6 || m_pipeline_memory_b ? 0 : m_pipeline_memory_case ? 0x1234 : 0x11) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 10 ? 0x55 : cycle >= 6 && m_pipeline_memory_b ? 0x1234 : 0),
			"Decoded foreground retires at 6; first ISR retires at 10; flushed immediate loads never execute");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle < 5 ? 0x1000 : 0xfff) &&
			m_cpu->space(AS_DATA).read_word(0xfff) == (cycle < 7 ? 0xbeef : 0x601),
			"INTR Access and Execute own separate cycles 5 and 7");
		if (cycle == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		u64 const checkpoint = m_pipeline_memory_case ? 5 : 4;
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			if (m_pipeline_memory_case)
			{
				expect(m_pipeline_operand_reads == 1, "Operand bus read completed once before checkpoint");
				m_cpu->space(AS_DATA).write_word(0x420, 0x5678);
			}
			expect(machine().scheduler().can_save(), "Decoded foreground and INTR checkpoint is saveable");
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE,
				"Save decoded foreground and pending INTR before Access");
		}
		if (cycle == 10)
		{
			expect(m_vector_reads == 1 && m_vector_acks == 1 && m_pipeline_drained_fetches == 1,
				"Each leg drains its prefetched word and fetches/acknowledges one vector");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_vector_reads = m_vector_acks = 0;
				// The flush word was already fetched at the saved cycle-4 boundary.
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == checkpoint,
					"Restore pending INTR and the decoded foreground slot");
				// Loading inside a live callback rewinds device local times while
				// the callback timestamp is still the old one. Let the scheduler
				// settle with this bench CPU suspended before single-clock replay.
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_phase = 251010;
				m_pipeline_expected_cycle = checkpoint + 1;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
				return true;
			}
			if (m_pipeline_memory_case)
			{
				expect(m_pipeline_operand_reads == 1, "Retirement and restored retirement reuse the captured operand without another bus read");
				if (m_pipeline_memory_b)
					osd_printf_info("TMS320C54x staged direct load: PASS cases=2 read=5 execute=6 retained_operand=1 restore_after_read=1 bus_reads_per_case=1\n");
			}
			else
				osd_printf_info("TMS320C54x staged frontend: PASS foreground_execute=6 vector_fetch=6 stack_write=7 isr_execute=10 pending_restore=1 coverage=register_one_word native_enabled=0\n");
			m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_vector_stack_tap.remove();
			m_pipeline_operand_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (!m_pipeline_memory_case) { start_pipeline_frontend_case(true); return true; }
			if (!m_pipeline_memory_b) { start_pipeline_frontend_case(true, true); return true; }
			m_cpu->space(AS_PROGRAM).write_word(0x600, 0x77e1);
			m_cpu->set_state_int(STATE_GENPC, 0x600);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800);
			m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0);
			m_cpu->set_pipeline_frontend(true);
			m_phase = 251020;
			m_check_timer->adjust(attotime::from_ticks(3, m_cpu->clock()));
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251010)
	{
		expect(m_cpu->pipeline_cycles() == (m_pipeline_memory_case ? 5 : 4), "Scheduler settlement does not retire restored instructions");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251000;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251020)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0,
			"Unsupported staged multiword form stops at Decode without atomic fallback");
		m_cpu->set_pipeline_frontend(false);
		osd_printf_info("TMS320C54x staged frontend exclusions: PASS unsupported_stops=1 atomic_fallback=0\n");
		start_empty_pipeline_case(false);
		return true;
	}
	if (m_phase == 251030)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"Empty frontend advances one clock without fallback");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle < 8 ? 0 : 0x55),
			"Undecoded foreground is flushed; ISR retires at cycle 8");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle < 3 ? 0x1000 : 0xfff) &&
			m_cpu->space(AS_DATA).read_word(0xfff) == (cycle < 5 ? 0xbeef : 0x600),
			"Empty frontend stacks the unexecuted first instruction at cycle 5");
		if (cycle < 8)
		{
			++m_pipeline_expected_cycle;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
			return true;
		}
		expect(m_vector_reads == 1 && m_vector_acks == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_IFR) == (m_pipeline_empty_nmi ? 2 : 0),
			"Empty IRQ/NMI acknowledges once and preserves unrelated pending sources");
		m_vector_prefetch_tap.remove();
		m_cpu->set_pipeline_frontend(false);
		if (!m_pipeline_empty_nmi) { start_empty_pipeline_case(true); return true; }
		osd_printf_info("TMS320C54x staged empty frontend: PASS irq=1 nmi=1 return_pc=0600 vector_fetch=4 stack_write=5 isr_execute=8\n");
		start_multiword_pipeline_case();
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_rmw_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251110)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const kind = (m_pipeline_rmw_case / 2) % 3;
		bool const absolute = kind == 1;
		bool const after_read = m_pipeline_rmw_case >= 6;
		u64 const read_cycle = absolute ? 7 : 6;
		u64 const execute_cycle = read_cycle + 1;
		u64 const isr_cycle = execute_cycle + 4;
		u64 const checkpoint = after_read ? read_cycle : absolute ? 4 : 3;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"RMW frontend advances one clock without atomic fallback");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= isr_cycle ? 0x55 : 0),
			"Opcode-shaped RMW literals are data, not accumulator loads");
		expect(m_pipeline_store_writes == (cycle >= execute_cycle ? 1 : 0) &&
			m_cpu->space(AS_DATA).read_word(0x4a0) == 0xbeef,
			"RMW never commits before Execute or redirects to later DP/AR values");
		if (cycle == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		if (cycle == read_cycle - 1)
		{
			if (kind == 2) expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x421,
				"RMW post-increment belongs to Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 9);
			m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x4a0);
		}
		if (cycle == read_cycle)
		{
			// Bench mutation is not an emulated CPU bus transaction.
			auto const suppress = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(0x420, 0x3333);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE,
				"Save partial RMW Decode or pending RMW write after Read");
		}
		if (cycle == isr_cycle)
		{
			expect(m_vector_acks == 1 && m_pipeline_store_writes == 1 &&
				m_pipeline_operand_reads == (m_pipeline_replay && after_read ? 0 : 1) &&
				m_pipeline_extensions_reads == (absolute ? 2 : 1),
				"RMW replay preserves extensions and captured data without duplicate bus traffic");
			expect(m_cpu->space(AS_DATA).read_word(0xfff) == (absolute ? 0x603 : 0x602),
				"RMW continuation points after every owned extension word");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_vector_acks = m_pipeline_store_writes = m_pipeline_operand_reads = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == checkpoint,
					"Restore incomplete RMW assembly or its sampled Read operand");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1;
				m_phase = 251120;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
				return true;
			}
			m_pipeline_fetch_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_rmw_case < 11) { start_pipeline_rmw_case(m_pipeline_rmw_case + 1); return true; }
			osd_printf_info("TMS320C54x staged RMW: PASS cases=12 forms=DP,postincrement,absolute extension_restore=6 sampled_read_restore=6 duplicate_reads=0\n");
			start_pipeline_mask_rmw_case();
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251120)
	{
		u64 const checkpoint = m_pipeline_rmw_case >= 6 ? ((m_pipeline_rmw_case / 2) % 3 == 1 ? 7 : 6) :
			(m_pipeline_rmw_case / 2) % 3 == 1 ? 4 : 3;
		expect(m_cpu->pipeline_cycles() == checkpoint, "Scheduler settlement preserves pending RMW state");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251110;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251130)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IMR) == (cycle < 8 ? 1 : 0),
			"Mask RMW changes IMR only when its retained operation retires");
		expect(m_pipeline_store_writes == (cycle < 8 ? 0 : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle < 12 ? 0 : 0x55),
			"Accepted entry completes after the staged mask write without a second commit");
		if (cycle == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		if (cycle == 7 && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE,
				"Save sampled IMR operand before the hardware mask write");
		}
		if (cycle == 12)
		{
			// Controller conformance: remove INTM as a confound, then pulse
			// another physical IRQ while the firmware-owned IMR write holds.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_input_line(0, CLEAR_LINE);
			m_cpu->set_input_line(0, ASSERT_LINE);
		}
		if (cycle == 18)
		{
			expect(m_vector_acks == 1 && m_pipeline_operand_reads == (m_pipeline_replay ? 0 : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_IFR) == 1,
				"New IRQ remains pending under IMR=0 despite INTM being clear");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_vector_acks = m_pipeline_operand_reads = m_pipeline_store_writes = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == 7,
					"Restore sampled mask RMW without rereading the MMR");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = 8;
				m_phase = 251140;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
				return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			osd_printf_info("TMS320C54x staged mask RMW: PASS read=7 write=8 accepted_irq=1 subsequent_masked=1 pending_write_restore=1 duplicate_reads=0\n");
			start_pipeline_return_case();
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251140)
	{
		expect(m_cpu->pipeline_cycles() == 7, "Scheduler settlement does not perform a pending mask write");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251130;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251150)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const pending_irq = m_pipeline_return_case >= 12;
		bool const multiword = m_pipeline_return_case >= 8 && !pending_irq;
		bool const delayed = multiword || !(m_pipeline_return_case & 1);
		u64 const acceptance = delayed ? 12 : 10;
		u64 const checkpoint = pending_irq ? 11 : multiword ? (m_pipeline_return_case & 1 ? 9 : 8) : (m_pipeline_return_case & 4 ? 8 : 7);
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Fast return advances one frontend cycle");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == ((cycle >= 5 && cycle < 8) || (pending_irq && cycle >= acceptance + 2) ? 0xfff : 0x1000), "Fast return and pending entry modify SP in their Access stages only");
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x800) == ((cycle >= 6 && cycle < 10) || (pending_irq && cycle >= acceptance + 3)), "Fast return clears INTM before the next accepted interrupt sets it");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle < 6 ? 0 : cycle >= 13 ? 0x44 : multiword ? (cycle >= 12 ? 0x1234 : 0x11) : delayed && cycle >= 11 ? 0x22 : 0x11), "Fast return preserves or discards the two delay words");
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == (!multiword && delayed && cycle >= 12 ? 0x33 : 0), "Return delay words belong to complete instructions, not independent literal opcodes");
		if (cycle == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		if (pending_irq && cycle == 9)
		{
			m_cpu->set_input_line(0, CLEAR_LINE);
			m_cpu->set_input_line(0, ASSERT_LINE);
		}
		// Input-line writes are scheduler-synchronized after this callback.
		if (pending_irq && cycle > 9)
		{
			if (m_cpu->state_int(tms320c54x_device::STATE_IFR) != (cycle < acceptance + 3 ? 1 : 0))
				osd_printf_info("Pending return IRQ mismatch: case=%u cycle=%llu ifr=%04x ack=%u\n", m_pipeline_return_case, (unsigned long long)cycle, unsigned(m_cpu->state_int(tms320c54x_device::STATE_IFR)), m_vector_acks);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == (cycle < acceptance + 3 ? 1 : 0), "Pending return IRQ remains latched until its vector Fetch");
			expect(m_vector_acks == (cycle < acceptance + 3 ? 1 : 2), "Return delay words finish before the second vector acknowledgment");
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save fast return around Access");
		}
		if (cycle == (pending_irq ? acceptance + 4 : 13))
		{
			expect(m_vector_acks == (pending_irq ? 2 : 1) && !m_pipeline_operand_reads && m_pipeline_drained_fetches == 2, "Fast return uses RTN without reading stack memory and fetches its continuation once");
			if (pending_irq)
			{
				auto const suppress = machine().disable_side_effects();
				expect(m_cpu->space(AS_DATA).read_word(0xfff) == (delayed ? 0x604 : 0x602), "Second IRQ stacks the first not-decoded continuation after return delay retirement");
			}
			expect(!multiword || m_pipeline_extensions_reads == 1, "Restored delay instruction does not reread its retained literal");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_vector_acks = 1;
				m_pipeline_drained_fetches = checkpoint >= 9 ? 2 : 1;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == checkpoint, "Restore fast-return redirect and delay ownership");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1;
				m_phase = 251160;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
				return true;
			}
			m_pipeline_operand_tap.remove();
			m_pipeline_fetch_tap.remove();
			if (multiword) m_pipeline_write_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_return_case < 15) { start_pipeline_return_case(m_pipeline_return_case + 1); return true; }
			osd_printf_info("TMS320C54x staged fast returns: PASS cases=8 target_fetch=9 return_execute=10 delay_execute=11,12 resumed_execute=13 stack_reads=0 restore=8\n");
			osd_printf_info("TMS320C54x staged return multiword delay: PASS cases=4 partial_decode_restore=2 completed_decode_restore=2 delay_execute=12 resumed_execute=13\n");
			osd_printf_info("TMS320C54x staged pending return IRQ: PASS cases=4 request=9 vector_fetch=13,15 stack_write=14,16 continuation=0602,0604 pending_restore=4\n");
			start_pipeline_branch_case();
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251160)
	{
		u64 const checkpoint = m_pipeline_return_case >= 12 ? 11 : m_pipeline_return_case >= 8 ? (m_pipeline_return_case & 1 ? 9 : 8) : (m_pipeline_return_case & 4 ? 8 : 7);
		expect(m_cpu->pipeline_cycles() == checkpoint, "Settlement retains the fast-return checkpoint");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251150;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251170)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const pending_irq = m_pipeline_branch_case >= 8;
		u64 const checkpoint = pending_irq ? (m_pipeline_branch_case & 4 ? 8 : 6) : (m_pipeline_branch_case & 4 ? 5 : 3);
		bool const delayed = !(m_pipeline_branch_case & 1);
		u64 const acceptance = delayed ? 9 : 7;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Branch frontend advances without executing flushed words");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (pending_irq && cycle >= acceptance + 2 ? 0xfff : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (!pending_irq || cycle >= acceptance + 3 ? 0x800 : 0), "Only accepted IRQ entry changes SP and INTM around the branch");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 10 ? 0x44 : delayed && cycle >= 8 ? 0x22 : 0), "Branch delay or flush precedes target Execute");
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == (pending_irq && cycle >= acceptance + 7 ? 0x55 : delayed && cycle >= 9 ? 0x33 : 0), "Branch delay words drain before the ISR executes");
		if (pending_irq && cycle == 5) m_cpu->set_input_line(0, ASSERT_LINE);
		if (pending_irq && cycle > 5)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == (cycle < acceptance + 3 ? 1 : 0), "Branch IRQ remains pending until vector Fetch");
			expect(m_vector_acks == (cycle < acceptance + 3 ? 0 : 1), "Branch IRQ vector is acknowledged once after the protected operation");
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save branch partial Decode or redirected Prefetch");
		}
		if (cycle == (pending_irq ? acceptance + 7 : 10))
		{
			expect(m_pipeline_drained_fetches == 1 && m_pipeline_extensions_reads == 1, "Branch restoration duplicates neither literal nor target fetch");
			if (pending_irq)
				expect(m_cpu->space(AS_DATA).read_word(0xfff) == (delayed ? 0x703 : 0x701), "Branch IRQ stacks the first not-decoded target continuation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_drained_fetches = checkpoint >= 6 ? 1 : 0;
				m_vector_acks = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == checkpoint, "Restore branch target and flush/delay ownership");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1;
				m_phase = 251180;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
				return true;
			}
			m_pipeline_fetch_tap.remove(); m_pipeline_operand_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_branch_case < 15) { start_pipeline_branch_case(m_pipeline_branch_case + 1); return true; }
			osd_printf_info("TMS320C54x staged branches: PASS cases=8 literal_fetch=3 target_fetch=6 delay_execute=8,9 target_execute=10 restore=8\n");
			osd_printf_info("TMS320C54x staged branch IRQ: PASS cases=8 request=5 vector_fetch=10,12 stack_write=11,13 continuation=0701,0703 pending_restore=8\n");
			start_pipeline_stm_case();
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251180)
	{
		u64 const checkpoint = m_pipeline_branch_case >= 8 ? (m_pipeline_branch_case & 4 ? 8 : 6) : (m_pipeline_branch_case & 4 ? 5 : 3);
		expect(m_cpu->pipeline_cycles() == checkpoint, "Settlement does not advance branch Prefetch");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251170;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 311000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delay_asm_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.st1() | (tick > f.read() ? f.raw : f.raw ^ 31)), "Delayed ASM publishes only its five bits at Execute");
		if (f.long_form() && tick == f.literal_clock()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x603 + f.prefix(), 0xdead); }
		if (tick == f.read() - 1) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR3) == (f.form == 2 ? 0x1003 : f.form == 3 ? 0xfff : 0x1002), "ASM source updates its selected pointer once in Access"); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x4444); }
		if (tick == f.read()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1002, 0xdead); }
		if (tick == f.checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_pipeline_extensions_reads}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save ASM source, control-delay words and adjacent shift across publication"); }
		if (tick == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.store() + 2 && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == f.prefix() && m_pipeline_extensions_reads == unsigned(f.long_form()) && m_cpu->state_int(tms320c54x_device::STATE_A) == f.result && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.pattern ? 0xfffffffff9ULL : 7) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == u16(f.result) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1002 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa620 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x4444, "ASM delay and selected target preserve the sampled shift, source, status and one observed result");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[3]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay delayed ASM without repeated source reads or target writes"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 311010; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 30719) { start_delay_asm(variant + 1); return true; }
			osd_printf_info("TMS320C54x delayed LD ASM: PASS cases=30720 restore=30720 controls=BD,BCD_taken,BCD_untaken,BD_store_prefix forms=DP,SP,postincrement,offset,absolute shifts=32 patterns=2 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=owned t_preserved=1 adjacent_consumer=LD_B_ASM_A target_observes_result=1 call_interrupt_repeat_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed LD ASM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 311010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 311000; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_pipeline_far_return_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251290)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const form = m_pipeline_peripheral_case % 36, prefix = m_pipeline_peripheral_case >= 36;
		bool const delayed = form >= 12;
		bool const long_delay = form >= 24;
		unsigned const first_read = (delayed ? 4 : 5) + prefix, target_fetch = (delayed ? 6 : 8) + prefix, target_execute = (delayed ? 10 : 12) + prefix;
		u64 const checkpoint = first_read - 1 + (m_pipeline_peripheral_case % 6) / 2;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= first_read) + unsigned(cycle > first_read) &&
			m_pipeline_extensions_reads == unsigned(cycle >= target_fetch), "Far return retains both stack samples and fetches the target once across replay");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 - prefix + unsigned(prefix && cycle >= 5) + unsigned(cycle >= first_read) + unsigned(cycle > first_read) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0 + 1) == (prefix && cycle >= 5 ? 0x42 : 0) &&
			bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x800) == (form % 12 >= 6 || cycle < (delayed ? 6 : 7) + prefix) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (long_delay ? (cycle >= 8 + prefix ? 0x1234 : 0) : delayed && cycle >= 7 + prefix ? 0x11 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= target_execute ? 0x55 : delayed && !long_delay && cycle >= 8 + prefix ? 0x22 : 0), "Far return preserves older POPM, its INTM contract and exactly its two delay words");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save before or between retained far-return stack accesses");
		}
		if (cycle == target_execute)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_operand_reads = unsigned(checkpoint >= first_read) + unsigned(checkpoint > first_read); m_pipeline_extensions_reads = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore far-return stack samples and pending extra clock");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251300;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 71) { start_pipeline_far_return(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged far return: PASS cases=72 plain=24 delayed=48 multiword_delay=24 preceding_popm=36 restore=72 duplicate_reads=0 silicon_phase_claim=0\n");
			start_pipeline_far_irq(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251300)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251290;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_intm_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251310)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const set = BIT(m_pipeline_peripheral_case, 0);
		unsigned const words = (m_pipeline_peripheral_case / 2) % 3 + 1;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "INTM staged stream advances without unsupported forms");
		if (cycle == 3) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 2);
		if (cycle == 4 && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save Decode-owned INTM follower protection with pending IRQ");
		}
		if (cycle == 20)
		{
			expect(m_pipeline_operand_reads == unsigned(!set) && m_pipeline_store_writes == unsigned(words == 3) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (set ? 0 : 0x55) &&
				m_cpu->state_int(tms320c54x_device::STATE_IFR) == (set ? 2 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == (set ? 0x1000 : 0xfff),
				"RSBX admits one IRQ only after the follower; SSBX retains pending IRQ without vector entry");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore Decode-owned INTM protection and multiword follower");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = 5; m_phase = 251320;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 11) { start_pipeline_intm(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged INTM protection: PASS cases=12 follower_words=1,2,3 adjacent=6 restore=12 duplicate_vectors=0\n");
			start_pipeline_status_bit(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251320)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251310;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251330)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		u16 const initial = BIT(m_pipeline_peripheral_case, 6) ? 0xffff : 0;
		u16 const mask = u16(1) << (m_pipeline_peripheral_case & 15);
		u16 const result = BIT(m_pipeline_peripheral_case, 4) ? initial | mask : initial & ~mask;
		bool const st1 = BIT(m_pipeline_peripheral_case, 5);
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged status-bit operation advances without fallback");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (cycle >= 6 && !st1 ? result : initial) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((cycle >= 6 && st1 ? result : initial) & ~u16(0x400)),
			"RSBX/SSBX modifies only its selected status bit at Execute");
		expect(m_xf_pin == bool(BIT(cycle >= 6 && st1 ? result : initial, 13)),
			"Staged status-bit retirement publishes XF at Execute and replay restores the pin");
		if (cycle == 5 && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save staged status-bit operation before Execute");
		}
		if (cycle == 9)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore status-bit operation before Execute");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = 6; m_phase = 251340;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 127) { start_pipeline_status_bit(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged status bits: PASS cases=128 registers=2 bits=16 operations=2 initial_patterns=2 restore=128 xf_output=1 reserved_st1_bit10=0\n");
			if (!strcmp(machine().options().bios(), "st1-bits")) throw emu_fatalerror(0, "TMS320C54x isolated status bits complete");
			start_pipeline_cpl(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251340)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251330;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251350)
	{
		bool const stlm = !strcmp(machine().options().bios(), "stlm-cpl") || !strcmp(machine().options().bios(), "stlm-cmpt");
		bool const stm = !strcmp(machine().options().bios(), "stm-cpl") || !strcmp(machine().options().bios(), "stm-cmpt");
		bool const cmpt = !strcmp(machine().options().bios(), "mvmd-cmpt") || !strcmp(machine().options().bios(), "stlm-cmpt") || !strcmp(machine().options().bios(), "stm-cmpt");
		bool const mvmd = stm || stlm || cmpt || !strcmp(machine().options().bios(), "mvmd-cpl");
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const gap = (m_pipeline_peripheral_case / 2) % 4;
		bool const set = BIT(m_pipeline_peripheral_case, 0), store = BIT(m_pipeline_peripheral_case, 3);
		bool const dp = BIT(m_pipeline_peripheral_case, 4);
		bool const alternate = gap >= (mvmd && !stlm ? 2 : 3) ? set : !set;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "CPL/DP consumer executes without unsupported direct mode");
		if (cycle == 4 + gap && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save retained Decode CPL before consumer address generation");
		}
		if (cycle == 11)
		{
			if (cmpt)
			{
				expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) == (set ? 0x820 : 0x800) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x6000 && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1400 + unsigned(!alternate) && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1500 + unsigned(alternate), "MVMD CMPT consumer updates only its latency-selected auxiliary register");
				expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (store ? 0x7777 : alternate ? 0x2222 : 0x1111) && m_cpu->space(AS_DATA).read_word(0x1400) == (store && !alternate ? 0x7777 : 0x1111) && m_cpu->space(AS_DATA).read_word(0x1500) == (store && alternate ? 0x7777 : 0x2222), "MVMD CMPT consumer samples the old mode with zero/one NOP and the new mode with two/three");
			}
			else
			{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST1) == (dp || !set ? 0x800 : 0x4800) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (dp && set ? 0x0f : 0x0e),
				"CPL/DP producer retires independently of retained consumer status");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (store ? 0x7777 : alternate ? 0x2222 : 0x1111) &&
				m_cpu->space(AS_DATA).read_word(0x727) == (store && !alternate ? 0x7777 : 0x1111) &&
				m_cpu->space(AS_DATA).read_word(dp ? 0x7a7 : 0x1027) == (store && alternate ? 0x7777 : 0x2222),
				"Basic direct LD/ST respects the producer-specific CPL/DP latency");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore Decode-sampled direct addressing mode");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = 5 + gap; m_phase = 251360;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_cpu->set_pipeline_frontend(false);
			if (mvmd)
			{
				if (m_pipeline_peripheral_case < 15) { start_pipeline_cpl(m_pipeline_peripheral_case + 1); return true; }
				if (stm)
				{
					osd_printf_info("TMS320C54x staged STM %s: PASS cases=16 restore=16 directions=2 consumers=LD,ST gaps=0,1,2,3 latency=2 interrupt_composition=unvalidated silicon_phase_claim=0\n", cmpt ? "CMPT" : "CPL");
					throw emu_fatalerror(0, "TMS320C54x isolated STM addressing mode complete");
				}
				if (stlm)
				{
					osd_printf_info("TMS320C54x staged STLM %s: PASS cases=16 restore=16 directions=2 consumers=LD,ST gaps=0,1,2,3 latency=3 silicon_phase_claim=0\n", cmpt ? "CMPT" : "CPL");
					throw emu_fatalerror(0, "TMS320C54x isolated STLM addressing mode complete");
				}
				if (cmpt)
				{
					osd_printf_info("TMS320C54x staged MVMD CMPT: PASS cases=16 restore=16 directions=2 consumers=LD,ST gaps=0,1,2,3 latency=2 pointer_updates=1 interrupt_composition=unvalidated silicon_phase_claim=0\n");
					throw emu_fatalerror(0, "TMS320C54x isolated MVMD CMPT complete");
				}
				osd_printf_info("TMS320C54x staged MVMD CPL: PASS cases=16 restore=16 directions=2 consumers=LD,ST gaps=0,1,2,3 latency=2 interrupt_composition=unvalidated silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated MVMD CPL complete");
			}
			if (m_pipeline_peripheral_case < 31) { start_pipeline_cpl(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged CPL/DP latency: PASS cases=32 fields=2 directions=2 consumers=LD,ST gaps=0,1,2,3 restore=32\n");
			start_pipeline_dp_immediate(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251360)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251350;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251390)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const page = m_pipeline_peripheral_case & 511;
		unsigned const chain = BIT(m_pipeline_peripheral_case, 9);
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Immediate DP stream advances without unsupported forms");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa600 | (cycle >= 6 + chain ? page : page ^ 1)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 7 + chain ? 0x2222 : 0),
			"Immediate DP forwards to adjacent LD without early ST0 commitment or unrelated status changes");
		if (cycle == 4 + chain && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save forwarded immediate DP before either instruction retires");
		}
		if (cycle == 10)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore forwarded DP with old architectural ST0");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = 5 + chain; m_phase = 251400;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 1023) { start_pipeline_dp_immediate(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged immediate DP: PASS cases=1024 pages=512 nearest_producer=512 old_st0_until_execute=1 restore=1024\n");
			start_pipeline_frame(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251400)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251390;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251410)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		s8 const offset = m_pipeline_peripheral_case & 255;
		u64 const checkpoint = std::array<unsigned, 3>{3, 4, 6}[m_pipeline_peripheral_case / 256];
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == unsigned(0x1000 + (cycle >= 4 ? offset : 0) - (cycle >= 6 ? 1 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0e &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			m_pipeline_store_writes == unsigned(cycle >= 7),
			"FRAME applies its signed offset once before the following push without status changes");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save FRAME before/after SP update or following push sample");
		}
		if (cycle == 10)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore FRAME and retained adjacent push without duplicate SP allocation");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251420;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 767) { start_pipeline_frame(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged FRAME: PASS offsets=256 restore=768 adjacent_push=768 duplicate_writes=0 silicon_phase_claim=0\n");
			start_pipeline_push_sp_latency(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251420)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251410;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_mu4_prologue_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251470)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		u64 const checkpoint = std::array<unsigned, 3>{6, 13, 39}[m_pipeline_peripheral_case / 2];
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Original MU4 prologue advances without atomic fallback");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save in-flight original MU4 context prologue");
		}
		if (cycle == 56)
		{
			{
				auto const disable = machine().disable_side_effects();
			expect(m_pipeline_store_writes == 9 && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa61b && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x4a00 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL &&
				m_cpu->space(AS_DATA).read_word(0xfff) == 0x4044 &&
				m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2469,
				"Original skip-worker path restores context and writes the reserved return slot using the pre-POPM SP sample");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_store_writes = checkpoint == 6 ? 0 : checkpoint == 13 ? 5 : 8;
				m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore overlapping original prologue sources and stack writes");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251480;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 5) { start_pipeline_mu4_prologue(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged original MU4 context: PASS words=37 cases=6 context_saves=8 context_restored=1 returned=4044 restore=6 full_handler_claim=0\n");
			start_pipeline_dld(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251480)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251470;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251490)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const b = BIT(variant, 0), sxm = BIT(variant, 1), negative = BIT(variant, 3), compiler = BIT(variant, 5);
		u16 const address = (compiler ? 0x500 : 0x400) + 0x7c + BIT(variant, 4);
		u64 const result = (sxm && negative ? 0xff00000000ULL : 0) | (negative ? 0x8001abcdULL : 0x1234abcdULL);
		u64 const checkpoint = BIT(variant, 6) ? 5 : 4;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == (cycle >= 5 ? 2 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 6 && !b ? result : 0x1111222233ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 6 && b ? result : 0x4444555566ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (sxm << 8) | (BIT(variant, 2) << 7) | (compiler ? 0x4000 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x500,
			"DLD retires the retained pair with SXM guard extension without changing the other accumulator or status");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save DLD before/after paired Read");
		}
		if (cycle == 5) {
			m_cpu->space(AS_DATA).write_word(address, 1);
			m_cpu->space(AS_DATA).write_word(address ^ 1, 2);
		}
		if (cycle == 9)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = checkpoint >= 5 ? 2 : 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore retained DLD operands without duplicate bus reads");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251500;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 127) { start_pipeline_dld(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged DLD: PASS cases=128 destinations=A,B sxm=0,1 c16=0,1 parity=0,1 addressing=DP,CPL restore=128 duplicate_reads=0 silicon_phase_claim=0\n");
			start_pipeline_immediate_add(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251500)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251490;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251510)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const source_b = BIT(variant, 0), destination_b = BIT(variant, 1), sxm = BIT(variant, 2), ovm = BIT(variant, 3), pattern = BIT(variant, 6);
		unsigned const shift = std::array<unsigned, 4>{0, 1, 7, 15}[(variant >> 4) & 3];
		u16 const literal = pattern ? 0xf495 : 1;
		s64 const source = pattern ? -2147483648LL : 2147483647LL;
		s64 const operand = (sxm ? s64(s16(literal)) : s64(literal)) * (s64(1) << shift);
		s64 const sum = source + operand;
		bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
		bool const carry = u64(u32(source)) + u32(operand) > 0xffffffffULL;
		u64 const result = u64(ovm && overflow ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
		u64 const initial_a = source_b ? 0x1111222233ULL : u64(source) & 0xffffffffffULL;
		u64 const initial_b = source_b ? u64(source) & 0xffffffffffULL : 0x4444555566ULL;
		u64 const checkpoint = BIT(variant, 7) ? 4 : 3;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 3) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 7 && !destination_b ? result : initial_a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 7 && destination_b ? result : initial_b) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x1b | (cycle >= 7 ? (carry ? 0x800 : 0) | (overflow ? (destination_b ? 0x200 : 0x400) : 0) : 0)),
			"Immediate ADD retires the retained literal with source/destination, carry, overflow and saturation semantics");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save partial/complete immediate ADD Decode");
		}
		if (cycle == 3) m_cpu->space(AS_PROGRAM).write_word(0x601, 0);
		if (cycle == 9)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = 1;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore retained ADD literal without a second program read");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251520;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 255) { start_pipeline_immediate_add(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged immediate ADD: PASS cases=256 selectors=4 shifts=0,1,7,15 sxm=0,1 ovm=0,1 restore=256 duplicate_literals=0\n");
			start_pipeline_dst(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251520)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251510;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_conditional_branch_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251610)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const b = BIT(variant, 0), eq = BIT(variant, 1), nonzero = BIT(variant, 2), taken = eq ? !nonzero : nonzero;
		u64 const initial = nonzero ? 0x0100000000ULL : 0, opposite = nonzero ? 0 : 0x0100000000ULL;
		bool const memory_dp = variant >= 48;
		u64 const checkpoint = memory_dp ? std::array<unsigned, 3>{6, 8, 11}[(variant / 16) % 3] : std::array<unsigned, 3>{4, 6, 8}[variant / 16];
		if (cycle == 6) m_cpu->set_state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, opposite);
		u64 const expected_a = memory_dp ? (taken && cycle >= 14 ? 0x3333 : !b && cycle >= 6 ? opposite : b ? 0x9999 : initial) :
			cycle >= 8 ? 0x11 : !b && cycle >= 6 ? opposite : b ? 0x9999 : initial;
		u64 const expected_b = memory_dp ? (taken && cycle >= 15 ? 0x55 : !taken && cycle >= 13 ? 0x66 : b && cycle >= 6 ? opposite : b ? initial : 0x7777) :
			taken && cycle >= 12 ? 0x55 : !taken && cycle >= 11 ? 0x66 : cycle >= 10 ? 0x44 : b && cycle >= 6 ? opposite : b ? initial : 0x7777;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(taken && cycle >= (memory_dp ? 10 : 8)) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 4) + unsigned(cycle >= 6) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == expected_a && m_cpu->state_int(tms320c54x_device::STATE_B) == expected_b &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (memory_dp && cycle >= 10 ? 0xa612 : 0xa608) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			(!memory_dp || (m_pipeline_drained_fetches == unsigned(cycle >= 7) && m_pipeline_store_writes == unsigned(taken && cycle >= 13))),
			"BCD retains the sampled 40-bit zero predicate, executes both delays and chooses only its proper continuation");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save BCD bubble, sampled predicate or pending delay retirement");
		}
		if (cycle == 18)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(taken && checkpoint >= (memory_dp ? 10 : 8));
				m_pipeline_extensions_reads = unsigned(checkpoint >= 4) + unsigned(checkpoint >= 6);
				m_pipeline_drained_fetches = unsigned(memory_dp && checkpoint >= 7); m_pipeline_store_writes = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore BCD target/predicate ownership without duplicate fetches");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251620;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 95) { start_pipeline_conditional_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged conditional branches: PASS cases=96 conditions=AEQ,ANEQ,BEQ,BNEQ pages=2 outcomes=2 memory_dp_delay=48 restore=96 duplicate_fetches=0 silicon_phase_claim=0\n");
			start_pipeline_plain_return(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251620)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251610;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251630)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const pushed = m_pipeline_peripheral_case >= 12, adjacent = pushed && m_pipeline_peripheral_case < 24;
		unsigned const prefix = pushed ? (adjacent ? 1 : 2) : 0;
		bool const enable = BIT(m_pipeline_peripheral_case, 1);
		u64 const checkpoint = 3 + prefix + (m_pipeline_peripheral_case / 4) % 3;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 5 + prefix) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 7 + prefix) &&
			m_pipeline_store_writes == unsigned(pushed && cycle >= 6) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (!pushed && cycle >= 4 ? 0x1001 : prefix == 2 && cycle == 5 ? 0xfff : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa61b &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (enable && cycle >= 6 + prefix ? 0 : 0x800) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 11 + prefix ? (adjacent ? 0x99 : 0x55) : 0),
			"Plain return owns one pop/read/target fetch, preserves RET's INTM and clears RETE only at retirement");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save plain return before/after Access or stack Read");
		}
		if (cycle == 14)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_operand_reads = unsigned(checkpoint >= 5 + prefix); m_pipeline_extensions_reads = 0;
				m_pipeline_store_writes = unsigned(pushed && checkpoint >= 6);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore ordinary return and adjacent push ownership without duplicate bus operations");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251640;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 35) { start_pipeline_plain_return(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged plain returns: PASS cases=36 forms=RET,RETE pages=2 adjacent_old_word=12 nop_new_word=12 restore=36 duplicate_accesses=0\n");
			start_pipeline_call(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251640)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251630;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251650)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const t_delay = m_pipeline_peripheral_case >= 24;
		u16 const parameter = t_delay ? std::array<u16, 9>{6, 0, 0x8000, 0xf495, 0xffff, 0x76e6, 0x4ee6, 0x7001, 0x54e6}[(m_pipeline_peripheral_case - 24) / 12] : 0;
		unsigned const adjacent = t_delay ? BIT(m_pipeline_peripheral_case, 1) : m_pipeline_peripheral_case >= 12;
		bool const delayed = t_delay || BIT(m_pipeline_peripheral_case, 1);
		u16 const continuation = 0x600 + adjacent + (delayed ? 4 : 2);
		u64 const checkpoint = 4 + adjacent + (m_pipeline_peripheral_case / 4) % 3;
		u64 const sp = cycle < 4 + adjacent ? 0x1000 : cycle >= 9 + adjacent ? 0x1000 - adjacent : 0xfff - adjacent;
		u64 const expected_b = cycle >= 16 + adjacent ? 0x66 : cycle >= 10 + adjacent ? (t_delay ? parameter : 0x55) : delayed && !t_delay && cycle >= 9 + adjacent ? 0x22 : 0;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_store_writes == unsigned(cycle >= 6 + adjacent) &&
			m_pipeline_drained_fetches == unsigned(adjacent && cycle >= 6) &&
			m_pipeline_operand_reads == unsigned(cycle >= 10 + adjacent) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 6 + adjacent) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == sp &&
			m_cpu->state_int(tms320c54x_device::STATE_RTN) == (cycle >= 5 + adjacent ? continuation : 0x8888) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (delayed && !t_delay && cycle >= 8 + adjacent ? 0x11 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (t_delay && cycle >= 7 + adjacent ? parameter : 0xbeef) &&
			m_pipeline_register_writes == unsigned(t_delay && cycle >= 7 + adjacent) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == expected_b,
			"CALL owns its stack/RTN phases, executes only delay slots and returns to the correct continuation");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save CALL after allocation/RTN/write ownership");
		}
		if (cycle == 22)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_store_writes = m_pipeline_extensions_reads = unsigned(checkpoint >= 6 + adjacent);
				m_pipeline_drained_fetches = unsigned(adjacent && checkpoint >= 6); m_pipeline_operand_reads = 0;
				m_pipeline_register_writes = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore retained CALL target and return word without duplicate stack writes");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251660;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 131) { start_pipeline_call(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged calls: PASS cases=132 forms=CALL,CALLD pages=2 adjacent_push=66 stm_t_delay=108 parameters=9 restore=132 duplicate_stack_writes=0\n");
			start_pipeline_direct_move(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251660)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251650;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251670)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const indirect = variant >= 60;
		unsigned const move_case = indirect ? variant - 60 : variant;
		bool const conflict = !indirect && variant >= 48;
		u16 const destination = indirect ? std::array<u16, 4>{0x10, 0x12, 0x18, 0x700}[move_case & 3] : std::array<u16, 4>{0x12, 0x18, 0x19, 0x700}[variant & 3];
		u16 const source = indirect ? 0x510 : conflict ? 0x7f : BIT(variant, 2) ? 0x531 : 0x431;
		u16 const value = BIT(move_case, 3) ? 0xbeef : 0x123;
		u16 const initial = indirect ? std::array<u16, 4>{0x5678, 0x510, 0x500, 0xaaaa}[move_case & 3] : std::array<u16, 4>{0x1234, 0x500, 0x40, 0xaaaa}[variant & 3];
		unsigned const read_clock = conflict ? 6 : 5;
		unsigned const write_clock = conflict ? 7 : destination < 0x60 ? 5 : 6;
		u64 const checkpoint = indirect ? 4 + move_case / 16 : conflict ? 5 + (variant - 48) / 4 : 4 + variant / 16;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= read_clock) && m_pipeline_store_writes == unsigned(cycle >= write_clock) &&
			m_cpu->space(AS_DATA).read_word(destination) == (cycle >= write_clock ? value : initial) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (conflict ? 0xa600 : 0xa608) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (!conflict && BIT(variant, 2) ? 0x4000 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL,
			"MVDK copies retained unsigned data with destination-specific write timing and no status/accumulator changes");
		if (indirect && destination != 0x12)
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == (cycle >= 5 && !BIT(move_case, 2) ? 0x511 : 0x510), "Indirect MVDK updates its source AR once after retaining the old-address sample");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save MVDK before/after source read or destination write");
		}
		if (cycle == read_clock) m_cpu->space(AS_DATA).write_word(source, 0);
		if (cycle == 10)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= read_clock); m_pipeline_store_writes = unsigned(checkpoint >= write_clock);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore retained move value without duplicate reads/writes");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251680;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 107) { start_pipeline_direct_move(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged MVDK: PASS cases=108 direct=60 indirect=48 restore=108 bank_conflicts=9 source_ar_alias=12 duplicate_accesses=0\n");
			start_pipeline_worker_xc(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251680)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251670;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251690)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const composed = m_pipeline_peripheral_case >= 4;
		bool const branch = m_pipeline_peripheral_case >= 8;
		bool const nonzero = BIT(m_pipeline_peripheral_case, 0);
		u64 const checkpoint = branch ? 11 + (m_pipeline_peripheral_case - 8) / 2 : 3 + 2 * (m_pipeline_peripheral_case & 3);
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_store_writes == unsigned(cycle >= 7) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle >= 6 ? 0x4ff : 0x500) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (cycle >= 9 ? 0x800 : 0),
			"Original unconditional XC worker prefix preserves PSHM/INTM ordering");
		if (composed)
			expect(m_pipeline_operand_reads == unsigned(cycle >= 9) + unsigned(cycle >= 11) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == (cycle >= 9 ? 0xbeef : 0x5678) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == (cycle >= 10 ? 0x512 : cycle >= 9 ? 0x511 : 0x510) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 12 ? (branch ? (cycle >= 14 ? !nonzero : nonzero) : 0x1357) : 0x1234),
				"Original worker prefix composes XC, stack, INTM, MVDK and next indirect load without duplicate source updates");
		if (branch)
			expect(m_pipeline_extensions_reads == unsigned(nonzero && cycle >= 14) &&
				m_pipeline_drained_fetches == unsigned(!nonzero && cycle >= 14), "Original worker BC retains its page-2 target and sampled route on replay");
		if (branch && cycle == 13) m_cpu->set_state_int(tms320c54x_device::STATE_A, !nonzero);
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original XC prefix");
		}
		if (composed && cycle == 9) m_cpu->space(AS_DATA).write_word(0x510, 0);
		if (branch && cycle == 11) m_cpu->space(AS_DATA).write_word(0x511, 0xdead);
		if (cycle == (branch ? 21 : 16))
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = unsigned(checkpoint >= 7);
				m_pipeline_operand_reads = composed ? unsigned(checkpoint >= 9) + unsigned(checkpoint >= 11) : 0;
				m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore original XC prefix without duplicate push");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251700;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 13) { start_pipeline_worker_xc(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged original worker XC prefix: PASS cases=14 restore=14 composed_seven_words=4 composed_nine_words=6 duplicate_pushes=0\n");
			start_pipeline_xc_irq(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251700)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251690;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_xc_tc_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251730)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const producer = variant >= 64;
		unsigned const gap = producer ? (variant - 64) / 16 : 0;
		unsigned const offset = producer ? 1 + gap : 0;
		u64 const checkpoint = producer ? 4 + gap + BIT(variant, 3) : std::array<unsigned, 4>{3, 4, 6, 8}[(variant / 8) & 3];
		bool const sample = producer && gap < 2 ? BIT(variant, 0) : !BIT(variant, 0);
		bool const execute = sample == BIT(variant, 2);
		auto const writes_at = [execute, variant, offset, producer](u64 clock) {
			if (variant >= 32 && !producer) return unsigned(execute && clock >= 8) + unsigned(clock >= 9);
			return unsigned(execute && clock >= 7 + offset) + unsigned((execute || !BIT(variant, 1)) && clock >= 8 + offset);
		};
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_pipeline_store_writes == writes_at(cycle), "XC TC/NTC preserves accepted/rejected slot ownership");
		if (cycle == 3 && !producer) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa60e | (!BIT(variant, 0) << 12));
		if (cycle == 4 && !producer) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa60e | (BIT(variant, 0) << 12));
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save XC before/after Access sampling or slot retirement");
		}
		if (cycle == 12)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = writes_at(checkpoint);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore XC decision and pending rejected-word count");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251740;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 127) { start_pipeline_xc_tc(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged XC TC sampling: PASS cases=128 forms=TC,NTC words=1,2 restore=128 two_word_slots=32 producer_cases=64 access_sampled=1 rejected_writes=0\n");
			start_pipeline_xc_accumulator(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251740)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251730;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251750)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const taken = BIT(variant, 1) ? !BIT(variant, 2) : BIT(variant, 2);
		u64 const checkpoint = 4 + variant / 16;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(taken && cycle >= 7) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 4) + unsigned(cycle >= 6) &&
			m_pipeline_store_writes == (taken ? 0 : unsigned(cycle >= 8) + unsigned(cycle >= 9)),
			"BC retains sequential Fetch but suppresses taken-path data effects");
		if (cycle == 6) m_cpu->set_state_int(BIT(variant, 0) ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, BIT(variant, 2) ? 0 : 1);
		if (cycle >= 11) expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 0x55 : 0x66), "BC uses retained outcome despite accumulator changes after evaluation");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save BC before/after held sequential word or decision");
		}
		if (cycle == 14)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
				m_pipeline_extensions_reads = unsigned(checkpoint >= 4) + unsigned(checkpoint >= 6);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore BC held word and retained route without repeated Fetch");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251760;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 47) { start_pipeline_plain_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged plain conditional branches: PASS cases=48 conditions=AEQ,ANEQ,BEQ,BNEQ pages=2 outcomes=2 restore=48 duplicate_fetches=0 taken_data_reads=0\n");
			if (!strcmp(machine().options().bios(), "plain-branch")) throw emu_fatalerror(0, "TMS320C54x isolated plain conditional branches complete");
			start_pipeline_delayed_return(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251760)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251750;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_delayed_return_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251790)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		bool const xc = variant >= 12, two_word = !xc && BIT(variant, 1);
		bool const irq = xc && !BIT(variant, 1);
		u64 const checkpoint = xc ? 6 + (variant - 12) / 4 : 4 + variant / 4;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 5) && m_pipeline_extensions_reads == unsigned(cycle >= 7) &&
			m_pipeline_store_writes == unsigned(cycle >= 3) + unsigned(cycle >= 4) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (irq && cycle >= 13 ? 0x1000 : cycle >= 4 ? 0x1001 : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (irq && cycle >= 10 && cycle < 14 ? 0 : 0x800) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (xc ? (BIT(variant, 1) ? 0x800 : 0) : !two_word && cycle >= 9 ? 0x11 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 11 ? 0x55 : !xc && !two_word && cycle >= 10 ? 0x44 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (two_word && cycle >= 8 ? 6 : 0),
			"RETD retains its pop and two words, preserves INTM and executes each delay before the target");
		if (xc && cycle == 6) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1);
		if (xc) expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == (irq && cycle >= 14 ? 0 : cycle >= 6 ? 1 : 0), "XC in RETD protects the resumed target before a pending IRQ is acknowledged");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save RETD queued delay words and return sample");
		}
		if (cycle == 15)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= 5); m_pipeline_extensions_reads = unsigned(checkpoint >= 7);
				m_pipeline_store_writes = 2;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore RETD without repeated stack or delay Fetch");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251800;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 23) { start_pipeline_delayed_return(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged delayed returns: PASS cases=24 pages=2 delay_forms=3 restore=24 xc_irq_cases=12 duplicate_stack_reads=0 third_delay_fetches=0\n");
			start_pipeline_original_worker_tail(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251800)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251790;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251810)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		bool const masked = BIT(variant, 0); u16 const saved_status = masked ? 0x0834 : 0x0034;
		u64 const checkpoint = std::array<unsigned, 5>{5, 8, 9, 11, 14}[variant / 2];
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 5) + unsigned(cycle >= 9) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 11) &&
			m_pipeline_drained_fetches == unsigned(cycle >= 7) + unsigned(cycle >= 8) &&
			m_pipeline_store_writes == unsigned(!masked && cycle >= 18) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (!masked && cycle >= 17 ? 0x1000 : cycle >= 8 ? 0x1001 : cycle >= 5 ? 0x1000 : 0xfff) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 8 ? (masked ? 0x800 : 0) : cycle >= 6 ? 0x2233440000ULL | saved_status : 0x2233445566ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 15 ? 0x55 : 0x1122334455ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (!masked && cycle >= 14 && cycle < 18 ? 0 : 0x800),
			"Original worker tail restores the saved INTM branch, balances pops and protects resumed execution");
		if (cycle == 10) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1);
		if (cycle == 5) m_cpu->space(AS_DATA).write_word(0xfff, 0xdead);
		if (cycle == 9) m_cpu->space(AS_DATA).write_word(0x1000, 0xdead);
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original tail around stack samples, AND and delay decision");
		}
		if (cycle == 24)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= 5) + unsigned(checkpoint >= 9);
				m_pipeline_extensions_reads = unsigned(checkpoint >= 11); m_pipeline_store_writes = 0;
				m_pipeline_drained_fetches = unsigned(checkpoint >= 7) + unsigned(checkpoint >= 8);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore original tail without repeated stack or delay reads");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251820;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 9) { start_pipeline_original_worker_tail(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original worker return tail: PASS cases=10 original_words=7 saved_intm_states=2 restore=10 duplicate_stack_reads=0 irq_after_continuation=1\n");
			start_pipeline_original_worker_roundtrip(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251820)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251810;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_original_context_call_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251850)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		u16 const saved_status = BIT(variant, 1) ? 0x4a00 : 0x4200;
		u64 const checkpoint = std::array<unsigned, 10>{6, 13, 31, 40, 51, 58, 64, 74, 79, 85}[variant / 4];
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Original outer context call route advances without atomic fallback");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_pipeline_register_writes = m_pipeline_store_writes;
			m_pipeline_branch_case = m_pipeline_operand_reads; m_pipeline_stm_case = m_pipeline_extensions_reads;
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save coherent outer handler at ten nested ownership boundaries");
		}
		if (cycle == 100)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_store_writes == 15 && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa61b &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == saved_status && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1357 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2468 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x369c &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8888 && m_cpu->space(AS_DATA).read_word(0xfff) == 0x4044 &&
					m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2469,
					"Original outer handler calls the skip-body worker and restores all saved context with one counter increment");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = m_pipeline_register_writes;
				m_pipeline_operand_reads = m_pipeline_branch_case; m_pipeline_extensions_reads = m_pipeline_stm_case;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore complete context call route without duplicated saves or return fetch");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251860;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 39) { start_pipeline_original_context_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original context call route: PASS cases=40 original_words=67 pages=2 saved_intm_states=2 restore=40 context_restored=1 counter_increments=1 full_isr_claim=0\n");
			start_pipeline_addm(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251860)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251850;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_retd_data_order_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251910)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		unsigned const prefix = BIT(variant, 1) ? 2 : 1; u64 const checkpoint = 4 + prefix + variant / 4;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 5 + prefix) && m_pipeline_store_writes == unsigned(cycle >= 6) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 7 + prefix) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (prefix == 2 && cycle == 5 ? 0xfff : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 11 + prefix ? (prefix == 1 ? 0x99 : 0x55) : 0),
			"RETD retains read-first DARAM target without changing pop, delays or interrupt mask semantics");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save RETD before/after same-address read/write ordering");
		}
		if (cycle == 15)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= 5 + prefix); m_pipeline_store_writes = unsigned(checkpoint >= 6); m_pipeline_extensions_reads = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore RETD target without late forwarding or duplicate stack access");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251920;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 11) { start_pipeline_retd_data_order(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged RETD DARAM ordering: PASS cases=12 adjacent_old_word=6 nop_new_word=6 restore=12 duplicate_accesses=0\n");
			start_pipeline_data_pop(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251920)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251910;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_cmpr_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 251970)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const sequence = variant >= 432;
		bool const producer = variant >= 384 && !sequence;
		unsigned const kind = producer ? (variant - 384) / 12 : 0;
		unsigned const gap = producer ? ((variant - 384) / 3) % 4 : 0;
		unsigned const ar = producer ? 3 : (variant / 3) % 8;
		unsigned const cc = producer ? 0 : (variant / 24) % 4;
		unsigned const pair = producer || sequence ? 0 : variant / 96;
		std::array<u16, 4> const left{0, 0, 0xffff, 0x8000}, right{0, 0xffff, 0, 0x7fff};
		u16 const lhs = ar ? left[pair] : right[pair];
		bool const conditions[] = {lhs == right[pair], lhs < right[pair], lhs > right[pair], lhs != right[pair]};
		bool const result = sequence ? true : producer ? gap >= (kind < 2 ? 0 : kind == 2 ? 1 : 2) : conditions[cc];
		unsigned const access = sequence ? 6 : 4 + (producer ? (kind < 2 ? 2 : 1) + gap : 0);
		unsigned const execute = access + 2;
		unsigned const checkpoint = access + variant % 3;
		u64 const cycle = m_cpu->pipeline_cycles();
		if (m_cpu->state_int(tms320c54x_device::STATE_ST0) != ((producer ? 0xa600 : 0xa608) | ((cycle >= execute ? result : BIT(variant, 0)) ? 0x1000 : 0)))
			osd_printf_info("CMPR mismatch: case=%u cycle=%llu replay=%u st0=%04x access=%u execute=%u result=%u ar0=%04x ar3=%04x\n",
				variant, (unsigned long long)cycle, unsigned(m_pipeline_replay), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)),
				access, execute, unsigned(result), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR3)));
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((producer ? 0xa600 : 0xa608) | ((cycle >= execute ? result : BIT(variant, 0)) ? 0x1000 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
			"CMPR samples unsigned AR operands in Access and only commits TC in Execute");
		auto const writes_at = [](u64 clock) { return unsigned(clock >= 6) + unsigned(clock >= 7) + unsigned(clock >= 9); };
		if (sequence)
			expect(m_pipeline_store_writes == writes_at(cycle) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x470 + unsigned(cycle >= 5) + unsigned(cycle >= 6) + unsigned(cycle >= 8),
				"CMPR observes the Access-time pointer, not younger store Read or final pointer");
		if (!producer && !sequence && cycle == access)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x4321);
			if (ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x9876);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save retained CMPR condition around Access and Execute");
		}
		if (cycle == execute + 4)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = sequence ? writes_at(checkpoint) : 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore sampled CMPR without observing later AR mutations");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 251980;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			if (sequence) m_pipeline_write_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (variant < 434) { start_pipeline_cmpr(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged CMPR: PASS cases=435 conditions=4 auxiliary_registers=8 unsigned_pairs=4 producer_latency_cases=48 producers=STM,MVDK,POPM,STL original_store_sequences=3 restore=435\n");
			start_pipeline_mar(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251980)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251970;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251990)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const sequence = variant >= 288;
		unsigned const form = sequence ? 0 : variant / 48;
		unsigned const ar = sequence ? 2 : (variant / 6) % 8;
		bool const compatibility = !sequence && BIT(variant / 3, 0);
		unsigned const selected = compatibility && !ar ? 4 : ar;
		u16 const initial = sequence ? 0x470 : 0x400 + selected * 0x10;
		s16 const step = std::array<s16, 6>{0, -1, 1, 3, -1, -32768}[form];
		unsigned const access = form >= 3 ? 5 : 4;
		unsigned const read = sequence ? 5 : access + 2;
		unsigned const execute = read + 1;
		unsigned const checkpoint = sequence ? std::array<unsigned, 3>{5, 9, 11}[(variant - 288) % 3] :
			form >= 3 ? std::array<unsigned, 3>{3, 5, 7}[variant % 3] : std::array<unsigned, 3>{3, 4, 6}[variant % 3];
		u64 const cycle = m_cpu->pipeline_cycles();
		u16 const pointer = sequence ? initial - unsigned(cycle >= 4) - unsigned(cycle >= 5) + 3 * unsigned(cycle >= 9) :
			cycle > access + 1 ? 0x650 : cycle >= access ? u16(initial + step) : initial;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= read) &&
			m_pipeline_extensions_reads == unsigned((sequence || form >= 3) && cycle >= (sequence ? 7 : 3)) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0 + selected) == pointer &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= execute ? 0xcafe : 0x1234) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (compatibility && ar && cycle >= access ? (0x0608 | ar << 13) : 0x8608 | (variant >= 291 ? 0x1000 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
			"MAR updates one selected pointer, preserves context and retains its modifier through replay");
		if (sequence)
		{
			auto const disable = machine().disable_side_effects();
			expect(m_pipeline_store_writes == unsigned(variant >= 291 && cycle >= 9) &&
				m_cpu->space(AS_DATA).read_word(0x46e) == (variant >= 291 && cycle >= 9 ? 0xcafe : 0x7777),
				"Original conditional store preserves rejected memory and uses its retained pre-MAR destination");
		}
		if (!sequence && cycle == access + 1) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + selected, 0x650);
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save MAR and follower around pointer update and operand capture");
		}
		if (cycle == 12)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= read);
				m_pipeline_store_writes = unsigned(sequence && variant >= 291 && checkpoint >= 9);
				m_pipeline_extensions_reads = unsigned((sequence || form >= 3) && checkpoint >= (sequence ? 7 : 3));
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore MAR without modifying its pointer or reading an extension twice");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252000;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove();
			if (sequence) m_pipeline_write_tap.remove();
			if (sequence || form >= 3) m_pipeline_fetch_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (variant < 293) { start_pipeline_mar(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged MAR: PASS cases=294 forms=6 auxiliary_registers=8 compatibility_states=2 original_worker_words=6 original_xc_outcomes=2 restore=294 duplicate_data_reads=0 duplicate_offset_fetches=0\n");
			start_pipeline_memory_logic(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252000)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251990;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_mu4_full_worker_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252050)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const matched = BIT(variant, 0), masked = BIT(variant, 1), sxm = BIT(variant, 2);
		unsigned const checkpoint = std::array<unsigned, 6>{17, 22, 27, 34, 42, 46}[variant / 8];
		u64 const cycle = m_cpu->pipeline_cycles();
		auto const reads_at = [](u64 clock) {
			unsigned count = 0; for (unsigned at : {13, 15, 20, 22, 24, 29, 37, 38, 41, 45}) count += clock >= at; return count;
		};
		auto const writes_at = [matched](u64 clock) {
			unsigned count = matched && clock >= 33; for (unsigned at : {6, 11, 21, 22, 25, 26, 27, 29, 41}) count += clock >= at; return count;
		};
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == reads_at(cycle) && m_pipeline_store_writes == writes_at(cycle) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 47), "Complete original body advances with retained bus ownership");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save full original zero-control worker at composed ownership boundaries");
		}
		if (cycle == 58)
		{
			auto const disable = machine().disable_side_effects();
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((masked ? 0x800 : 0) | (sxm ? 0x100 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (matched ? 0xb600 : 0xa600) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (masked ? 0x800 : 0) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55 &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 6 && m_cpu->state_int(tms320c54x_device::STATE_AR0) == (matched ? 0x472 : 0x474) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x513 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x474 &&
				m_cpu->space(AS_DATA).read_word(0x511) == (matched ? 0x470 : 0) && m_cpu->space(AS_DATA).read_word(0x512) == 0xff11 &&
				m_cpu->space(AS_DATA).read_word(0x513) == 0x4f4, "Full worker balances its stack, restores INTM and completes both conditional body outcomes");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = reads_at(checkpoint); m_pipeline_store_writes = writes_at(checkpoint); m_pipeline_extensions_reads = unsigned(checkpoint >= 47);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore complete original body without duplicate source/stack or store effects");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252060;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 47) { start_pipeline_mu4_full_worker(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged complete MU4 worker: PASS cases=48 original_worker_words=38 call_words=4 control=zero cmpr_outcomes=2 saved_intm_states=2 sxm_states=2 restore=48 duplicate_reads=0 duplicate_writes=0\n");
			start_pipeline_original_context_call(0, true); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252060)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252050;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252070)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		u16 const saved_status = 0x4200 | (BIT(variant, 1) ? 0x800 : 0) | (BIT(variant, 2) ? 0x100 : 0);
		unsigned const checkpoint = std::array<unsigned, 8>{6, 13, 31, 51, 65, 75, 90, 104}[variant / 16];
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"Complete outer-context body route advances without replacing original instructions");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_pipeline_register_writes = m_pipeline_store_writes; m_pipeline_branch_case = m_pipeline_operand_reads;
			m_pipeline_stm_case = m_pipeline_extensions_reads; m_pipeline_store_case = m_pipeline_drained_fetches;
			m_pipeline_indirect_case = m_vector_reads;
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original outer handler with active body at eight ownership boundaries");
		}
		if (cycle == 130)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_store_writes == 16 && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 &&
					m_pipeline_drained_fetches == (BIT(variant, 3) ? 7 : 6) && m_vector_reads == 7 &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa61b &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == saved_status && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1357 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2468 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x369c &&
					m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8888 && m_cpu->space(AS_DATA).read_word(0xfff) == 0x4044 &&
					m_cpu->space(AS_DATA).read_word(0x7c) == 0x1357 && m_cpu->space(AS_DATA).read_word(0x7d) == 0x2469 &&
					m_cpu->space(AS_DATA).read_word(0x511) == (BIT(variant, 3) ? 0x470 : 0) &&
					m_cpu->space(AS_DATA).read_word(0x513) == 0x4f4,
					"Original active-body context route restores all saved registers and keeps its exact memory effects");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = m_pipeline_register_writes; m_pipeline_operand_reads = m_pipeline_branch_case;
				m_pipeline_extensions_reads = m_pipeline_stm_case; m_pipeline_drained_fetches = m_pipeline_store_case; m_vector_reads = m_pipeline_indirect_case;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore complete outer-context body route without repeating its work");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252080;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove();
			m_vector_stack_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 127) { start_pipeline_original_context_call(variant + 1, true); return true; }
			osd_printf_info("TMS320C54x staged complete MU4 context body: PASS cases=128 original_words=89 pages=2 saved_intm_states=2 sxm_states=2 cmpr_outcomes=2 restore=128 context_restored=1 counter_increments=1 full_isr_claim=0\n");
			start_pipeline_far_branch(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252080)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252070;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252090)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		unsigned const target_page = std::array<unsigned, 5>{0, 1, 2, 3, 0x7f}[variant / 8];
		unsigned const source_page = BIT(variant, 0) ? 2 : 0;
		unsigned const checkpoint = std::array<unsigned, 4>{3, 5, 7, 10}[(variant % 8) / 2];
		u64 const cycle = m_cpu->pipeline_cycles();
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 3) && m_pipeline_extensions_reads == unsigned(cycle >= 6) &&
			// Fetch exposes the next page before retirement; that intermediate
			// debugger value is not an asserted architectural commit phase.
			(cycle == 6 || m_cpu->state_int(tms320c54x_device::STATE_XPC) == (cycle >= 7 ? target_page : source_page)) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 10 ? 0x44 : 0) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
			"FB commits the explicit target page and flushes sequential instructions without call semantics");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save FB partial target decode, redirected fetch and page commitment");
		}
		if (cycle == 14)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= 3); m_pipeline_extensions_reads = unsigned(checkpoint >= 6);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore FB page and literal without duplicate fetch or fallthrough");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252100;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 39) { start_pipeline_far_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged FB: PASS cases=40 source_pages=2 target_pages=5 restore=40 duplicate_literal_fetches=0 duplicate_target_fetches=0\n");
			start_pipeline_mu4_timer_context(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252100)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252090;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_far_call_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252170)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		unsigned const source_page = BIT(variant, 0) ? 2 : 0;
		unsigned const checkpoint = std::array<unsigned, 8>{3, 4, 5, 6, 7, 10, 11, 12}[(variant % 16) / 2];
		u64 const cycle = m_cpu->pipeline_cycles();
		auto const writes_at = [](u64 clock) { return unsigned(clock >= 6) + unsigned(clock >= 7); };
		auto const reads_at = [](u64 clock) { return unsigned(clock >= 10) + unsigned(clock >= 11); };
		auto const caller_fetches = [](u64 clock) { return unsigned(clock >= 4) + unsigned(clock >= 13); };
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_store_writes == writes_at(cycle) && m_fast_stack_reads == reads_at(cycle) &&
			m_pipeline_operand_reads == unsigned(cycle >= 3) && m_pipeline_extensions_reads == unsigned(cycle >= 6) && m_pipeline_drained_fetches == caller_fetches(cycle) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle >= 11 ? 0x1000 : cycle >= 10 ? 0xfff : cycle >= 5 ? 0xffe : cycle >= 4 ? 0xfff : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 18 ? 0x11 : cycle >= 17 ? 0x44 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 10 ? 0x55 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0xbeef,
			"FCALL and FRET balance their two-word frame and execute only the callee then real caller continuation");
		if (cycle >= 12) expect(m_cpu->state_int(tms320c54x_device::STATE_XPC) == source_page, "FRET restores the held source page even after target-page fetches");
		if (cycle == 10 || cycle == 11)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(cycle == 10 ? 0xffe : 0xfff, 0x7fff);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save FCALL's partial frame and FRET's retained caller operands");
		}
		if (cycle == 20)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = writes_at(checkpoint); m_fast_stack_reads = reads_at(checkpoint);
				m_pipeline_operand_reads = unsigned(checkpoint >= 3); m_pipeline_extensions_reads = unsigned(checkpoint >= 6); m_pipeline_drained_fetches = caller_fetches(checkpoint);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore far call/return without duplicated pushes, reads or continuation execution");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252180;
				m_check_timer->enable(true); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 79) { start_pipeline_far_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged FCALL/FRET: PASS cases=80 source_pages=2 target_pages=5 restore=80 stack_words=2 duplicate_stack_writes=0 duplicate_stack_reads=0\n");
			start_pipeline_explicit_sp(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252180)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252170;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252190)
	{
		unsigned const variant = m_pipeline_peripheral_case, kind = variant % 5, gap = (variant / 5) % 4;
		bool const store = BIT(variant / 20, 0);
		unsigned const words = kind == 1 || kind == 2 || kind == 4 ? 2 : 1, access = 4 + words + gap;
		unsigned const publish = kind == 3 ? 4 : kind == 4 ? 7 : 5;
		unsigned const checkpoint = std::array<unsigned, 4>{3, publish, access, access + 1}[variant / 80];
		bool const new_sp = gap >= (kind == 0 ? 2 : kind == 4 ? 3 : 1);
		u64 const cycle = m_cpu->pipeline_cycles();
		auto const reads_at = [kind, store, access](u64 clock) { return unsigned((kind == 0 || kind == 2 || kind == 4) && clock >= (kind == 4 ? 6 : 5)) + unsigned(!store && clock >= access + 1); };
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == reads_at(cycle) && m_pipeline_store_writes == unsigned(store && cycle >= access + 2) &&
			m_pipeline_register_writes == unsigned(kind != 3 && cycle >= publish) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle >= publish ? 0x1020 : kind == 4 && cycle >= 6 ? 0x1001 : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (!store && cycle >= access + 2 ? new_sp ? 0x2222 : 0x1111 : 0x7777) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa600 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x4800,
			"Architectural SP and basic compiler SP advance at distinct table-7-9 boundaries");
		if ((kind == 0 || kind == 2 || kind == 4) && cycle == (kind == 4 ? 6 : 5))
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(kind == 2 ? 0x70 : 0x1000, 0xdead);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save explicit SP source, publication and compiler forwarding window");
		}
		if (cycle == 14)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = reads_at(checkpoint); m_pipeline_store_writes = unsigned(store && checkpoint >= access + 2);
				m_pipeline_register_writes = unsigned(kind != 3 && checkpoint >= publish);
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore explicit SP without a duplicated pop, register write or compiler address update");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252200;
				m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 319) { start_pipeline_explicit_sp(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged explicit SP: PASS cases=320 producers=POPM,STM,MVDK,FRAME,POPM_conflict compiler_consumers=LD,ST gap_nops=0,1,2,3 restore=320 duplicate_reads=0 duplicate_writes=0\n");
			start_pipeline_idle_sp_tail(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252200)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252190;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_sp_rmw_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252330)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		u16 const initial = std::array<u16, 4>{0x1200, 0xfffe, 0, 0x8000}[variant % 4];
		u16 const delta = std::array<u16, 4>{0x3ff, 1, 0xffff, 0x8000}[(variant / 4) % 4];
		u16 const aligned = u16(initial + delta) & 0xfffe;
		unsigned const tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = std::array<unsigned, 4>{7, 9, 10, 12}[variant / 64];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Absolute SP ADDM and ANDM have retained RMW ownership");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save SP setup before/after ADDM Read/write or ANDM Read");
		}
		if (tick == 22)
		{
			expect(m_pipeline_operand_reads == 3 && m_pipeline_store_writes == 3 && m_cpu->state_int(tms320c54x_device::STATE_SP) == aligned &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == aligned && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (BIT(variant, 4) ? 0x100 : 0)),
				"Original SP adjustment/alignment preserves status and publishes the wrapped aligned result without duplicate operations");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore SP RMW state without repeating a read or publication");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252340; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 255) { start_pipeline_sp_rmw(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged SP RMW: PASS cases=256 initial_values=4 deltas=4 sxm_states=2 pages=2 restore=256 ordered_reads=1 ordered_writes=1\n");
			start_pipeline_arp(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252340)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252330;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252350)
	{
		unsigned const variant = m_pipeline_peripheral_case, arp = variant % 8, encoded = BIT(variant, 3) ? 2 : 0;
		bool const compatibility = BIT(variant, 4), advance = BIT(variant, 5);
		unsigned const selected = compatibility && !encoded ? arp : encoded;
		unsigned const tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = 3 + variant / 128;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged immediate ARP is distinct from compare and status-bit operations");
		if (tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x420 + selected * 0x10, 0x9999); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_pipeline_branch_case = m_pipeline_operand_reads;
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save ARP load, selected address, auxiliary update or retained Read");
		}
		if (tick == 10)
		{
			expect(m_pipeline_operand_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1100 + selected &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x608 | ((compatibility && encoded ? encoded : arp) << 13)) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (compatibility ? 0x20 : 0)),
				"ARP load preserves unrelated status and does not re-publish over a younger encoded indirect ARP update");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == 0x420 + i * 0x10 + (advance && i == selected ? 1 : 0),
				"ARP selection modifies only its owned auxiliary register once");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_pipeline_branch_case;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore ARP consumer without retargeting or repeating its auxiliary update");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252360; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 511) { start_pipeline_arp(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged immediate ARP: PASS cases=512 arp_values=8 encoded_modes=2 cmpt_states=2 postincrement_states=2 pages=2 restore=512 zero_latency=1 duplicate_reads=0 duplicate_auxiliary_updates=0\n");
			start_pipeline_read_a(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252360)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252350;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252370)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const ram = variant >= 192;
		unsigned const page = std::array<unsigned, 3>{0, 2, 0x7f}[variant % 3];
		bool const upper = (variant / 3) % 2, ovly = (variant / 6) % 2;
		unsigned const gap = (variant / 12) % 4;
		u32 const logical = (page << 16) | (upper ? 0x8700 : 0x700), physical = ovly && !upper ? logical & 0xffff : logical;
		unsigned const tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = ram ? std::array<unsigned, 4>{5, 7, 9, 11}[(variant - 192) / 12] : std::array<unsigned, 4>{6, 8, 10, 12}[variant / 48];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "READA has retained accumulator-source and AR1-destination ownership");
		if (tick == (ram ? 8 : 9)) m_cpu->set_state_int(tms320c54x_device::STATE_A, logical + 1);
		if (tick == (ram ? 11 : 12)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(physical, 0x450); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_drained_fetches};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save READA literal, held clock, captured A or sampled program word");
		}
		if (tick == 22)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_drained_fetches == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == (ram ? 0x410 : 0x440) && m_cpu->state_int(tms320c54x_device::STATE_A) == (ram ? 0x440 : gap >= 2 ? 0x4444 : 0x1111) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == (ram ? 0x421 : 0x420) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
				"READA's sampled program word and exact younger address selection reproduce with no repeated bus access or register write");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_drained_fetches = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore READA source/wait/operand without retargeting the transfer");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252380; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant == 191) osd_printf_info("TMS320C54x staged absolute READA AR1: PASS cases=192 source_pages=3 source_regions=2 ovly_states=2 consumer_gaps=4 restore=192 retained_accumulator_source=1 duplicate_program_reads=0 duplicate_register_writes=0\n");
			if (variant < 239) { start_pipeline_read_a(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged short READA RAM: PASS cases=48 source_pages=3 source_regions=2 ovly_states=2 restore=48 retained_accumulator_source=1 postincrement_once=1 duplicate_program_reads=0 duplicate_data_writes=0\n");
			start_pipeline_banz(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252380)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252370;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252390)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		u16 const value = std::array<u16, 3>{0, 1, 0xffff}[variant % 3];
		unsigned const modification = (variant / 3) % 3, encoded = (variant / 9) % 2;
		bool const compatibility = (variant / 18) % 2;
		unsigned const selected = compatibility && !encoded ? 3 : encoded;
		u32 const base = (variant / 36) % 2 ? 0x20000 : 0;
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = 4 + variant / 72;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Short non-delayed BANZ has owned predicate, auxiliary update and branch flow");
		if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0x740); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save BANZ target, before/after predicate and AR update, or redirect");
		}
		if (tick == 22)
		{
			expect(m_pipeline_extensions_reads == 1 && m_pipeline_operand_reads == unsigned(value != 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (value ? 0x44 : 0x11) && m_cpu->state_int(tms320c54x_device::STATE_B) == (value ? 0 : 0x22) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x608 | ((compatibility ? encoded ? encoded : 3 : 0) << 13)) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
				"BANZ uses the pre-modification condition and does not execute discarded fall-through instructions");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (i == selected ? u16(value + (modification == 1 ? -1 : modification == 2 ? 1 : 0)) : 0x410 + i * 0x10),
				"BANZ modifies exactly the selected auxiliary once on either branch outcome");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore BANZ without changing its target or repeating the auxiliary modification");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252400; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 287) { start_pipeline_banz(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged BANZ: PASS cases=288 values=3 modifications=3 encoded_modes=2 cmpt_states=2 pages=2 restore=288 premodify_condition=1 duplicate_auxiliary_updates=0\n");
			start_pipeline_memory_repeat_read_a(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252400)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252390;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_instruction_observer_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 292000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const xc = variant >= 32, two = xc && BIT(variant - 32, 5);
		u32 const base = BIT(variant, 1) ? 0x20000 : 0, physical = BIT(variant, 2) ? 0 : base;
		unsigned const checkpoint = std::array<unsigned, 4>{1, 3, 6, 9}[(variant % 32) / 8];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Observer fixture executes only accepted instructions");
		// Debugger-style inspection and plain program-bus reads are not execution.
		unsigned const before = m_observed_instructions.size();
		m_cpu->space(AS_PROGRAM).read_word(physical + 0x600);
		{ auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).read_word(physical + 0x601); }
		expect(m_observed_instructions.size() == before, "Program/data inspection cannot issue instruction observations");
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(physical + 0x600, 0x0000);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_saved_observed_instructions = m_observed_instructions;
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save before or after observer entry without saving callback wiring");
		}
		if (tick == 40)
		{
			std::vector<std::pair<offs_t, u16>> expected{
				{base + 0x600, 0xec02}, {base + 0x601, 0xf495}, {base + 0x601, 0xf495},
				{base + 0x601, 0xf495}, {base + 0x602, 0xf073}, {base + 0x608, 0xe977}, {base + 0x609, 0xf4e1}};
			if (xc)
			{
				expected = {{base + 0x600, u16(two ? 0xff45 : 0xfd45)}};
				if (!two) expected.emplace_back(base + 0x602, 0xf495);
				expected.emplace_back(base + 0x603, two ? 0xff44 : 0xfd44);
				expected.emplace_back(base + 0x604, 0xe977);
				if (two) expected.emplace_back(base + 0x605, 0xf495);
				expected.emplace_back(base + (two ? 0x606 : 0x605), 0xf4e1);
			}
			if (m_observed_instructions != expected)
			{
				osd_printf_info("Instruction observer mismatch: case=%u replay=%u entries=%zu pc=%06x\n", variant, unsigned(m_pipeline_replay), m_observed_instructions.size(), unsigned(m_cpu->state_int(STATE_GENPC)));
				for (auto const &[address, opcode] : m_observed_instructions) osd_printf_info("  entry=%06x opcode=%04x\n", unsigned(address), opcode);
			}
			expect(m_observed_instructions == expected && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x77 && m_cpu->state_int(tms320c54x_device::STATE_A) == (xc ? 1 : 0) && m_cpu->state_int(tms320c54x_device::STATE_IDLE),
				"Execute entries retain opcode and logical page, include real repeats and exclude extension/annulled fetches");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint;
				m_observed_instructions = m_saved_observed_instructions;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore instruction identity without extra observer emissions");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 292010; m_check_timer->enable(true); return true;
			}
			m_cpu->set_pipeline_frontend(false);
			if (variant == 31) osd_printf_info("TMS320C54x instruction observer: PASS cases=32 restore=32 frontends=atomic,staged pages=2 ovly_states=2 snapshots=4 events_each=7 logical_address=1 captured_opcode=1 repeat_entries=3 ordinary_reads=0 annulled_fetches=0 observer_writes=0\n");
			if (variant < 95) { start_instruction_observer(variant + 1); return true; }
			osd_printf_info("TMS320C54x observer XC annulment: PASS cases=64 restore=64 frontends=atomic,staged pages=2 ovly_states=2 snapshots=4 forms=XC1,XC2 events_each=5 rejected_slots=absent accepted_source_nops=present observer_writes=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated instruction observer complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 292010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 292000; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 294000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_page_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Cross-page repeat hazard uses retained ordinary instructions and a balanced far call");
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_store_writes, m_pipeline_operand_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save before, during or after the foreign-page repeat endpoint");
		}
		if (tick == 240)
		{
			unsigned const calls = f.hazard ? 1 : f.count + 1;
			if (m_cpu->state_int(tms320c54x_device::STATE_AR4) != 0x1400 + calls || m_cpu->state_int(tms320c54x_device::STATE_AR5) != 0x1500 + (f.hazard ? f.count : 0))
				osd_printf_info("block_page_mismatch: case=%u staged=%u hazard=%u count=%u caller=%06x callee=%06x ar4=%04x ar5=%04x ar6=%04x pc=%06x brc=%04x st1=%04x stack_writes=%u stack_reads=%u\n", variant, unsigned(f.staged), unsigned(f.hazard), f.count, unsigned(f.caller), unsigned(f.callee), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR4)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR5)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), m_pipeline_store_writes, m_pipeline_operand_reads);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.caller + 0x60b && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400 + calls && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1500 + (f.hazard ? f.count : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1600 + f.count + 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1700 + (f.delayed ? calls : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x609 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 &&
				m_pipeline_store_writes == 2 * calls && m_pipeline_operand_reads == 2 * calls,
				"REA compares only the low PC; a foreign-page match consumes the loop and wraps within that foreign page");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_operand_reads = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore the actual page, repeat count and pending call frame without duplicating work"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 294010; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_block_page(variant + 1); return true; }
			osd_printf_info("TMS320C54x repeat page comparator: PASS cases=384 restore=384 frontends=atomic,staged counts=0,1,2,7 caller_pages=0,2,127 targets=foreign call_forms=FCALL,FCALLD hazard_controls=2 snapshots=4 rea_width=16 wrap_page=current balanced_frames=1 delay_effects=once physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeat page comparator complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 294010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 294000; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_pipeline_add_sp_context_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 291000)
	{
		unsigned const variant = m_pipeline_peripheral_case, producer = variant % 4;
		bool const copy = !strcmp(machine().options().bios(), "mvkd-sp");
		unsigned const gap = std::array<unsigned, 3>{0, 1, 3}[(variant / 4) % 3];
		u16 const initial_sp = BIT(variant / 12, 0) ? 0x9000 : 0x1000;
		u16 const operand = initial_sp + std::array<int, 4>{-4, -1, 1, -8}[producer];
		bool const compiler = BIT(variant / 24, 0), b = BIT(variant / 96, 0);
		unsigned const read_tick = 6 + (producer == 3 ? 2 : 1) + gap;
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 3>{4, read_tick, read_tick + 2}[variant / 192];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Absolute ADD accepts SP after an evidenced stack update");
		if (tick == read_tick + 2)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == u16(operand + 2), "Younger FRAME updates SP after the ADD samples its older value");
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2468);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save SP producer, retained ADD input or younger FRAME state");
		}
		if (tick == 24)
		{
			u64 const expected = u64(s64(s16(operand))) & 0xffffffffffULL;
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == unsigned(producer == 1) &&
				m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == (copy ? 0 : expected) &&
				m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x2233445566ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x2468 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == (producer == 2 ? 0xbeef : 0x1357) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa008 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x900 | (compiler ? 0x4000 : 0)),
				"Mapped SP ADD retains the producer value independently of CPL, younger FRAME and later SP replacement");
			if (copy) { auto const disable = machine().disable_side_effects(); expect(m_pipeline_extensions_reads == 1 && m_cpu->space(AS_DATA).read_word(0x1400) == operand && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1401, "Mapped SP copy retains the older stack value and modifies its destination pointer once"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore mapped-SP operand without duplicate stack effects or reads");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 291010; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 575) { start_pipeline_add_sp_context(variant + 1); return true; }
			if (copy) osd_printf_info("TMS320C54x staged MVKD SP contexts: PASS cases=576 restore=576 producers=FRAME,PSHM,POPM,STM gaps=0,1,3 cpl_states=2 pages=2 accumulators=2 younger_frame=1 retained_source=1 silicon_phase_claim=0\n");
			else osd_printf_info("TMS320C54x staged absolute ADD SP contexts: PASS cases=576 restore=576 producers=FRAME,PSHM,POPM,STM gaps=0,1,3 cpl_states=2 pages=2 accumulators=2 younger_frame=1 retained_source=1 silicon_phase_claim=0\n");
			start_pipeline_add_sp_exclusion(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 291010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 291000; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 291020)
	{
		bool const copy = !strcmp(machine().options().bios(), "mvkd-sp");
		if (++m_idle_fixture_tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_operand_reads &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa008,
				"SP admission does not admit control, repeat, mapping or peripheral sources");
			if (copy) { auto const disable = machine().disable_side_effects(); expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1400 && m_cpu->space(AS_DATA).read_word(0x1400) == 0xaaaa, "Excluded MVKD and MVDM do not update the destination pointer or publish data"); }
			m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < (copy ? 24 : 19)) { start_pipeline_add_sp_exclusion(m_pipeline_peripheral_case + 1); return true; }
			if (copy)
			{
				osd_printf_info("TMS320C54x staged MVKD SP exclusions: PASS cases=25 sources=10 source_cases=20 destination_cases=5 forms=MVKD,MVDM operand_reads=0 architectural_results=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated MVKD SP complete");
			}
			osd_printf_info("TMS320C54x staged absolute ADD SP exclusions: PASS cases=20 sources=10 accumulators=2 operand_reads=0 architectural_results=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated absolute ADD SP complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252430)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		bool const bk = !strcmp(machine().options().bios(), "add-bk");
		bool const sp = !strcmp(machine().options().bios(), "add-sp");
		u16 const operand = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[variant % 4];
		u64 const initial = std::array<u64, 4>{0, 0x7fffffff, 0xff80000000ULL, 0xffffffffffULL}[(variant / 4) % 4];
		bool const sxm = BIT(variant, 4), ovm = BIT(variant, 5), b = BIT(variant, 6), ram = BIT(variant, 7);
		u32 const base = BIT(variant, 8) ? 0x20000 : 0;
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 4>{4, 6, 7, 8}[(variant / 512) % 4];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Absolute AR/RAM accumulator ADD is a retained arithmetic form");
		if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0x510); }
		if (tick == 6)
		{
			if (ram) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x500, 0x1234); }
			else m_cpu->set_state_int(bk ? tms320c54x_device::STATE_BK : sp ? tms320c54x_device::STATE_SP : tms320c54x_device::STATE_AR1, 0x1234);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save absolute ADD address, sampled operand or arithmetic publication");
		}
		if (tick == 20)
		{
			constexpr u64 mask = 0xffffffffffULL;
			s64 const left = initial & 0x8000000000ULL ? s64(initial) - 0x10000000000LL : s64(initial);
			s64 const right = sxm ? s16(operand) : operand;
			s64 const sum = left + right;
			bool const overflow = sum > 0x7fffffffLL || sum < -0x80000000LL;
			bool const carry = u64(u32(initial)) + u32(right) > 0xffffffffULL;
			u64 const expected = overflow && ovm ? (sum < 0 ? 0xff80000000ULL : 0x7fffffffULL) : u64(sum) & mask;
			u16 const status = 0xa008 | (carry ? 0x800 : 0) | (overflow ? b ? 0x200 : 0x400 : 0);
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 && m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == expected &&
				m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x2233445566ULL && m_cpu->state_int(tms320c54x_device::STATE_ST0) == status &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (sp && BIT(variant, 11) ? 0x4000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (sp && !ram ? 0x1234 : 0x1000),
				"Memory ADD retains its sampled operand and reproduces independent 40-bit result, carry, overflow and saturation expectations");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore memory ADD without repeating its read or arithmetic update");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252440; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (sp ? 4095 : 2047)) { start_pipeline_absolute_add(variant + 1); return true; }
			if (sp)
			{
				osd_printf_info("TMS320C54x staged absolute ADD SP: PASS cases=4096 operands=4 initial_values=4 sxm_states=2 ovm_states=2 accumulators=2 sources=SP,RAM pages=2 cpl_states=2 restore=4096 duplicate_reads=0 independent_arithmetic=1 source_retained=1\n");
				start_pipeline_add_sp_context(); return true;
			}
			if (bk)
			{
				osd_printf_info("TMS320C54x staged absolute ADD BK: PASS cases=2048 operands=4 initial_values=4 sxm_states=2 ovm_states=2 accumulators=2 sources=BK,RAM pages=2 restore=2048 duplicate_reads=0 independent_arithmetic=1 source_retained=1\n");
				throw emu_fatalerror(0, "TMS320C54x isolated absolute ADD BK complete");
			}
			osd_printf_info("TMS320C54x staged absolute ADD: PASS cases=2048 operands=4 initial_values=4 sxm_states=2 ovm_states=2 accumulators=2 sources=AR,RAM pages=2 restore=2048 duplicate_reads=0 independent_arithmetic=1\n");
			if (!strcmp(machine().options().bios(), "add-absolute")) throw emu_fatalerror(0, "TMS320C54x isolated absolute ADD complete");
			start_pipeline_direct_rmw(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252440)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252430;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252450)
	{
		unsigned const variant = m_pipeline_peripheral_case, operation = variant % 3, offset = (variant / 3) % 2 ? 0x20 : 0;
		u16 const initial = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 6) % 4], immediate = (variant / 24) % 2 ? 0x00ff : 0xffff;
		bool const compiler = (variant / 48) % 2;
		u32 const base = (variant / 96) % 2 ? 0x20000 : 0;
		u16 const target = (compiler ? 0x1000 : 0x400) + offset;
		unsigned const tick = ++m_idle_fixture_tick, checkpoint = 4 + variant / 192;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Compiler and noncompiler direct RMW have explicit retained addressing");
		if (tick == 4)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, m_cpu->state_int(tms320c54x_device::STATE_ST1) ^ 0x4000);
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead);
		}
		if (tick == 5)
		{
			if (compiler) m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1020);
			else m_cpu->set_state_int(tms320c54x_device::STATE_ST0, (m_cpu->state_int(tms320c54x_device::STATE_ST0) & ~0x1ff) | 9);
		}
		if (tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(target, 0x5555); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save direct RMW captured context, address, operand or completed write");
		}
		if (tick == 20)
		{
			{
				auto const disable = machine().disable_side_effects();
				u16 const expected = operation == 0 ? initial & immediate : operation == 1 ? initial | immediate : u16(initial + immediate);
				expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == 1 && m_cpu->space(AS_DATA).read_word(target) == expected &&
					m_cpu->state_int(tms320c54x_device::STATE_SP) == (compiler ? 0x1020 : 0x1000) && (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1ff) == (compiler ? 8 : 9) &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((0x900 | (compiler ? 0x4000 : 0)) ^ 0x4000),
					"Direct RMW preserves subsequent context changes while using its original owned address and operands");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore direct RMW without resampling modified context, literal or memory");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252460; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_pipeline_direct_rmw(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged direct RMW: PASS cases=768 operations=ANDM,ORM,ADDM offsets=2 values=4 immediates=2 cpl_states=2 pages=2 restore=768 retained_context=1 duplicate_reads=0 duplicate_writes=0\n");
			start_pipeline_peripheral_logic(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252460)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252450;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_branch_repeat_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 600060)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = branch_repeat_case(variant);
		unsigned const end = f.target == 0x500 ? 0x604 : 0x605;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RPTBD delay repeat is separate from its outer block");
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save RPTBD delay and short-repeat state");
		}
		if (tick == f.limit)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + end + 2 &&
				m_pipeline_operand_reads == (f.kind == 1 ? f.count + 1 : 0) &&
				m_pipeline_store_writes == (f.kind == 2 ? 0 : f.count + 1) && m_pipeline_extensions_reads == f.following + 1,
				"Delay body executes count+1 once before the outer block executes BRC+1 times");
			if (m_cpu->state_int(tms320c54x_device::STATE_ST1) != f.status)
				osd_printf_info("RPTBD delay state mismatch case=%u staged=%u replay=%u st1=%04x expected=%04x\n", variant, unsigned(f.staged), unsigned(m_pipeline_replay), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), f.status);
			expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == end &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.status &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x2001 + f.following &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1800 + (f.kind == 2 ? 0 : f.count + 1) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1c00 + (f.kind == 1 ? f.count + 1 : 0),
				"Both loop controllers and their pointers settle independently");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				m_cpu->set_pipeline_frontend(f.staged);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore RPTBD delay repeat after clearing the live pipeline");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 600061; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 5759) { start_block_delay_repeat(variant + 1); return true; }
			osd_printf_info("TMS320C54x RPTBD delay repeat: PASS cases=5760 restore=5760 frontends=2 bodies=3 inner_counts=0,1,8,39,255 outer_counts=0,2,7 block_layouts=2 patterns=4 pages=2 snapshots=4 delay_effects=once reset_before_restore=1 physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated RPTBD delay repeat complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 600061) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600060; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 600050)
	{
		bool const block = !strcmp(machine().options().bios(), "rptbd-rpt-edge");
		bool const conditional = !strcmp(machine().options().bios(), "bcd-rpt-edge");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = variant < 16 ? 3 : 10;
		unsigned const kind = variant%8;
		expect(m_pipeline_operand_reads <= unsigned(kind == 3) && !m_pipeline_store_writes,"Unqualified delay repeats perform no transaction beyond their owner's preceding return Read");
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads,m_pipeline_store_writes}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE,"Save rejected delay-repeat state before or after the guard"); }
		if (tick == 24)
		{
			bool const state_ok = m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (block ? 0x8800 : 0x800) && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_SP) == (kind == 2 ? 0x2fff : kind == 3 ? 0x3001 : 0x3000) && m_pipeline_operand_reads == unsigned(kind == 3) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234560000ULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcdffffULL;
			if (!state_ok) osd_printf_info("BD repeat exclusion mismatch case=%u replay=%u illegal=%llu st0=%04x st1=%04x rc=%04x brc=%04x sp=%04x t=%04x a=%010llx b=%010llx\n",variant,unsigned(m_pipeline_replay),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_RC)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_T)),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B));
			expect(state_ok,"Delay-repeat exclusions preserve counters and arithmetic state while retaining the enclosing control's stack effect");
			for (unsigned ar = 0; ar < 8; ++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0+ar) == (ar == 3 ? 0x2000 : ar == 4 ? 0x1800 : ar == 5 ? 0x1c00 : 0x4000+ar),"Rejected delay repeats preserve every address register");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(true); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE,"Restore unsupported delay repeat without introducing side effects"); m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_phase = 600051; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (block || conditional)
			{
				unsigned const ordinal = (variant / 8) * 5 + (kind == 0 ? 0 : kind - 3);
				if (conditional) { if (ordinal < 19) { start_conditional_delay_repeat_edge(ordinal + 1); return true; } osd_printf_info("TMS320C54x BCD delay repeat exclusions: PASS cases=20 restore=20 forms=late,long_count,memory_count,store_spill,copy_spill pages=2 snapshots=2 reads=0 writes=0 ar_changes=0 counter_changes=0 reset_before_restore=1 silicon_trap_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated BCD delay repeat exclusions complete"); }
				if (ordinal < 19) { start_block_delay_repeat_edge(ordinal + 1); return true; }
				osd_printf_info("TMS320C54x RPTBD delay repeat exclusions: PASS cases=20 restore=20 forms=late,long_count,memory_count,store_spill,copy_spill pages=2 snapshots=2 reads=0 writes=0 ar_changes=0 repeat_counter_changes=0 owner_setup_retained=1 reset_before_restore=1 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated RPTBD delay repeat exclusions complete");
			}
			if (variant < 31) { start_branch_repeat_edge(variant+1); return true; }
			osd_printf_info("TMS320C54x BD delay repeat exclusions: PASS cases=32 restore=32 forms=late,BANZD,CALLD_late,RETD,long_count,memory_count,store_spill,copy_spill pages=2 snapshots=2 reads=RETD_stack_only writes=0 ar_changes=0 counter_changes=0 owner_stack=CALLD,RETD reset_before_restore=1 silicon_trap_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated BD delay repeat exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == 600051) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600050; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	if (m_phase == 600040)
	{
		bool const returning = !strcmp(machine().options().bios(), "retd-rpt");
		bool const calling = !strcmp(machine().options().bios(), "calld-rpt");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = branch_repeat_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"Short RPT in BD's first delay word has independent branch and repeat ownership");
		if (!returning && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base+0x601,0x620); }
		if (returning && tick == 8) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x2fff,0x620); }
		if (f.conditional && f.staged && tick == 6) { m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a(true)); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b(true)); }
		if (f.staged && !f.conditional && tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base+(returning ? 0x602 : 0x603),0xe8ff); }
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_extensions_reads}; m_saved_observed_instructions = m_observed_instructions;
			m_recurring_irq_snapshot[3] = m_rptbd_counts.reads;
			m_recurring_irq_snapshot[4] = m_pipeline_register_writes;
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE,"Save delayed branch continuation and repeat before, during or after the delay body");
		}
		if (tick == f.limit)
		{
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(STATE_GENPC) != f.base+(calling ? 0x605 : f.continuation()+3))
				osd_printf_info("BD repeat mismatch case=%u staged=%u replay=%u pc=%06x reads=%u old_writes=%u target_writes=%u\n",variant,unsigned(f.staged),unsigned(m_pipeline_replay),unsigned(m_cpu->state_int(STATE_GENPC)),m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_extensions_reads);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base+(calling ? 0x605 : f.continuation()+3) && m_pipeline_operand_reads == (f.kind == 1 ? f.count+1 : 0) && m_pipeline_store_writes == (f.kind == 2 ? 0 : f.count+1) && m_pipeline_extensions_reads == f.following+1,"Delay repeat drains once, then the retained target runs its own complete repeat");
			unsigned bodies = 0, target_bodies = 0, branches = 0, headers = 0;
			if (returning) expect(m_rptbd_counts.reads == 1, "RETD never resamples its poisoned return word");
			if (calling) expect(m_rptbd_counts.reads == 1 && m_pipeline_register_writes == 1, "CALLD and RET own one balanced continuation frame");
			for (auto const &[address,word] : m_observed_instructions)
			{
				if (address == f.base+(returning ? 0x602 : 0x603)) { expect(word == f.body(),"Repeated delay retains its original opcode"); ++bodies; }
				if (address == f.base+f.continuation()+1) ++target_bodies;
				if (address == f.base+0x600) ++branches;
				if (address == f.base+f.continuation()) { expect(bodies == f.count+1,"Target repeat cannot execute before all delay-body copies"); ++headers; }
			}
			expect(bodies == f.count+1 && target_bodies == f.following+1 && branches == 1 && headers == 1,"Execution-owned observations distinguish delay copies from one fresh target sequence");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x2001+f.following && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1800+(f.kind == 2 ? 0 : f.count+1) && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1c00+(f.kind == 1 ? f.count+1 : 0),"Both repeat lifecycles update their own pointers exactly once per copy");
			expect(m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x5555 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x6666 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.status && m_cpu->state_int(tms320c54x_device::STATE_A) == f.a(true) && m_cpu->state_int(tms320c54x_device::STATE_B) == f.b(true),"Delay repeat preserves unrelated counters, stack, status and accumulator state");
			{
				auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
				for (unsigned i = 0; i < 256; ++i)
				{
					expect(data.read_word(0x1800+i) == (f.kind == 0 && i <= f.count ? u16(f.a(true)) : u16(f.seed+i*257)),"Delay source/store buffer remains exact after replay");
					expect(data.read_word(0x1c00+i) == (f.kind == 1 && i <= f.count ? u16(f.seed+i*257) : 0xdead),"Delay copy buffer has no omitted or extra words");
				}
				for (unsigned i = 0; i < 32; ++i) expect(data.read_word(0x2000+i) == (i <= f.following ? u16(f.b(true)) : 0xdead),"Target repeat has an independent count and untouched tail");
				for (u16 address : {0x17ff,0x1900,0x1bff,0x1d00,0x1fff,0x2020}) expect(data.read_word(address) == 0x2468,"Delayed repeat preserves every buffer guard");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_observed_instructions = m_saved_observed_instructions;
				m_rptbd_counts.reads = m_recurring_irq_snapshot[3];
				m_pipeline_register_writes = m_recurring_irq_snapshot[4];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(f.staged); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE,"Restore pending delayed target after clearing live pipeline state");
				m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_phase = 600041; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.conditional ? 69119U : 5759U)) { start_branch_repeat(variant+1); return true; }
			if (f.conditional) { osd_printf_info("TMS320C54x BCD delay repeat: PASS cases=69120 restore=69120 frontends=atomic,staged predicates=12 signed_patterns=4 bodies=STL,MVDD,NOP counts=0,1,8,39,255 path_counts=0,2,7 directions=2 pages=2 snapshots=4 taken_fallthrough=1 predicate_poison=staged_after_Read target_poison=1 opcode_poison=staged_only ordered_copies=1 fresh_path_repeat=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated BCD delay repeat complete"); }
			if (calling) { osd_printf_info("TMS320C54x CALLD delay repeat: PASS cases=5760 restore=5760 frontends=atomic,staged bodies=STL,MVDD,NOP counts=0,1,8,39,255 target_counts=0,2,7 directions=2 patterns=4 pages=2 snapshots=4 stack_reads=1 stack_writes=1 balanced_stack=1 target_poison=1 ordered_copies=1 fresh_target_repeat=1 opcode_poison=staged_only reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated CALLD delay repeat complete"); }
			if (returning)
			{
				osd_printf_info("TMS320C54x RETD delay repeat: PASS cases=5760 restore=5760 frontends=atomic,staged bodies=STL,MVDD,NOP counts=0,1,8,39,255 target_counts=0,2,7 directions=2 patterns=4 pages=2 snapshots=4 stack_reads=1 balanced_stack=1 target_poison=1 ordered_copies=1 fresh_target_repeat=1 opcode_poison=staged_only reset_before_restore=1 physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated RETD delay repeat complete");
			}
			osd_printf_info("TMS320C54x BD delay repeat: PASS cases=5760 restore=5760 frontends=atomic,staged bodies=STL,MVDD,NOP counts=0,1,8,39,255 target_counts=0,2,7 directions=2 patterns=4 pages=2 snapshots=4 ordered_copies=1 target_retained=1 fresh_target_repeat=1 opcode_poison=staged_only reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated BD delay repeat complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == 600041) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600040; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_pipeline_repeat_nop_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252750)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const long_form = variant % 2;
		unsigned const count = std::array<unsigned, 5>{0, 1, 3, 32, 255}[(variant / 2) % 5], interrupt = (variant / 10) % 3;
		unsigned const first = long_form ? 8 : 7, last = first + count;
		u32 const base = (variant / 30) % 2 ? 0x20000 : 0, body = base + (long_form ? 0x602 : 0x601);
		unsigned const checkpoint = std::array<unsigned, 4>{3, first, first + count / 2, last}[variant / 60];
		unsigned const remaining = tick < first - 1 ? 0 : tick <= first ? count : tick <= last ? last - tick : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_RC) == remaining,
			"RPT count loads once, first NOP retains count, and each further NOP consumes one saved counter clock");
		// Input-line changes synchronize through MAME. Queue before Decode
		// so the cycle-3 checkpoint saves a latched event, not an unsaved queue.
		if (interrupt && tick == 2) { int const line = interrupt == 2 ? INPUT_LINE_NMI : 0; m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE); }
		if (interrupt == 1 && tick == 3) expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 1, "Repeat Decode checkpoint owns a delivered pending IRQ");
		if (tick < last) expect(!m_vector_acks && !m_pipeline_store_writes, "RPT suppresses IRQ and NMI until its count+1 NOP body completes");
		if (tick == (long_form ? 4 : 3))
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(body, 0xe811);
		}
		if (tick == (long_form ? 5 : 4))
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(body + 1, 0xe866);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_vector_acks};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save repeat Decode, first/middle/final NOP with pending interrupt");
		}
		if (tick == last + 30)
		{
			bool const success = m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == (interrupt && !count ? 3 : 2) &&
				m_pipeline_store_writes == (interrupt ? 2 : 0) && m_vector_acks == unsigned(bool(interrupt)) &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x66 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000;
			if (!success) osd_printf_info("Staged RPT NOP failure: case=%u replay=%u count=%u interrupt=%u cycle=%llu idle=%u a=%010llx b=%010llx sp=%04x body_reads=%u continuation_reads=%u stack_writes=%u acks=%u\n",
				variant, m_pipeline_replay, count, interrupt, (unsigned long long)m_cpu->pipeline_cycles(), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)),
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), m_pipeline_operand_reads, m_pipeline_extensions_reads, m_pipeline_store_writes, m_vector_acks);
			expect(success,
				"Repeated NOP preserves its body, completes exact duration and returns any deferred interrupt to its fresh continuation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0];
				m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore repeat counter/body/interrupt without restarting the loop");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252760; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 239) { start_pipeline_repeat_nop(variant + 1); return true; }
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			osd_printf_info("TMS320C54x staged repeat NOP: PASS cases=240 forms=short,long counts=0,1,3,32,255 sources=none,IRQ,NMI pages=2 restore=240 count_plus_one=1 counter_each_clock=1 duplicate_body_fetches=0 deferred_interrupts=1\n");
			start_pipeline_xmem_bit(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252760)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252750;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_long_move_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252830)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, destination_kind = (variant / 100) % 12;
		bool const compatibility = (variant / 4800) % 2;
		auto const f = long_accumulator_store_fixture((variant % 100) + (compatibility ? 6400 : 0));
		u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 1200) % 4];
		u16 const destination = destination_kind < 10 ? 0x10 + destination_kind : destination_kind == 10 ? 0xf495 : f.address;
		unsigned const checkpoint = std::array<unsigned, 4>{3, 5, 6, 7}[variant / 9600];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Long-offset MVDK has staged ownership");
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects();
			u32 const base = f.reg == 6 ? 0x20000 : 0;
			m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead); m_cpu->space(AS_PROGRAM).write_word(base + 0x602, 0xdead);
		}
		if (tick == 6)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0x7777);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save long MVDK during Decode or before/after publication");
		}
		if (tick == 16)
		{
			u16 st0 = compatibility && f.reg && f.kind != 3 ? (f.st0 & 0x1fff) | (f.reg << 13) : f.st0;
			if (destination_kind < 8)
			{
				u16 const ar0 = destination_kind == 0 ? value : f.selected == 0 ? f.next_ar : 0x1007;
				st0 = (st0 & ~0x1000) | (value == ar0 ? 0x1000 : 0);
			}
			for (unsigned ar = 0; ar < 8; ++ar)
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (destination == 0x10 + ar ? value : ar == f.selected ? f.next_ar : 0x1007),
					"MVDK address modification precedes aliasing destination publication and is not replayed twice");
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == 2 &&
					m_cpu->space(AS_DATA).read_word(destination) == value && m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_SP) == (destination_kind == 8 ? value : 0x1200) &&
					m_cpu->state_int(tms320c54x_device::STATE_BK) == (destination_kind == 9 ? value : f.bk) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL,
					"MVDK preserves unsigned values and exposes early AR publication to the immediate CMPR consumer");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore long MVDK without source resampling or duplicate writes");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252840; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 38399) { start_pipeline_long_move(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged long MVDK: PASS cases=38400 forms=25 offsets=4 destinations=12 values=4 compatibility=2 restore=38400 source_alias=1 zero_gap_cmpr=1 duplicate_reads=0 duplicate_writes=0\n");
			start_pipeline_original_long_move(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252840)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252830;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252850)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		u16 const value = variant % 2 ? 0x1000 : 0x1234;
		unsigned const checkpoint = std::array<unsigned, 4>{4, 7, 8, 9}[variant / 2];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Original STL/MVDK pair has coherent staged ownership");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original STL/MVDK pair across its two retirement boundaries");
		}
		if (tick == 16)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 2 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == value &&
				m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x608 | (variant % 2 ? 0x1000 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x33fa && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1122,
				"Original pair retires both writes and the immediate AR2 consumer without changing the pointer or accumulator");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore original pair without repeating either write");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252860; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 7) { start_pipeline_original_long_move(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original long MVDK pair: PASS cases=8 restore=8 stl_write=7 source_read=8 ar2_write=8 zero_gap_cmpr=1 duplicate_writes=0\n");
			if (!strcmp(machine().options().bios(), "long-mvkd")) { start_pipeline_long_rmw(); return true; }
			start_pipeline_tc_branch(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252860)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252850;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252870)
	{
		unsigned const raw_variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const carry = !strcmp(machine().options().bios(), "carry-branch");
		unsigned const variant = carry ? raw_variant % 8 + 16 + ((raw_variant / 8) % 3) * 32 + (raw_variant / 24) * 96 : raw_variant;
		u16 const flag = carry ? 0x0800 : 0x1000;
		bool const tc = variant % 2, delayed = (variant / 2) % 2, value = (variant / 4) % 2, poison = variant / 768;
		unsigned const producer = (variant / 8) % 4, gap = (variant / 32) % 3;
		bool const taken = value == tc;
		unsigned const checkpoint = std::array<unsigned, 4>{4, 6, 7, 10}[(variant / 192) % 4] + gap;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "TC/NTC conditional branches have staged ownership");
		if (poison && tick == 7 + gap)
			// CPU conformance mutation after Evaluate, never a native-boot hook.
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, m_cpu->state_int(tms320c54x_device::STATE_ST0) ^ flag);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads, m_pipeline_register_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save TC branch before/after evaluation and delay execution");
		}
		if (tick == 32)
		{
			bool const final_tc = delayed || !taken || poison ? !value : value;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == unsigned(taken) &&
				m_pipeline_extensions_reads == 1 && m_pipeline_register_writes == unsigned(!producer) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 0x55 : 0x11) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (delayed || !taken ? 0x22 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa608 | (final_tc ? flag : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1000,
				"TC branch consumes the published condition once; delay writes and later TC mutation cannot change the retained redirect");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore TC decision/held word without duplicate target or source fetches");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252880; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (raw_variant < (carry ? 383 : 1535)) { start_pipeline_tc_branch(raw_variant + 1); return true; }
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			if (carry)
			{
				osd_printf_info("TMS320C54x staged carry branches: PASS cases=384 forms=BC,BCD conditions=C,NC producer=status_bit gaps=0,1,2 pages=2 restore=384 post_evaluate_change=0,1 duplicate_fetches=0 arithmetic_producers=unvalidated\n");
				throw emu_fatalerror(0, "TMS320C54x isolated carry branches complete");
			}
			osd_printf_info("TMS320C54x staged TC branches: PASS cases=1536 forms=BC,BCD conditions=TC,NTC producers=BIT,CMPR_EQ,status_bit,CMPR_LT gaps=0,1,2 pages=2 restore=1536 post_evaluate_change=0,1 duplicate_fetches=0\n");
			if (!strcmp(machine().options().bios(), "tc-branch")) throw emu_fatalerror(0, "TMS320C54x isolated TC branches complete");
			start_pipeline_long_rmw(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252880)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252870;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_signed_branch_taken_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252970)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, flags = (variant / 192) % 4;
		bool const b = variant % 2, delayed = (variant / 12) % 2, poison = (variant / 768) % 2, taken = signed_branch_taken(variant);
		u64 const original = signed_branch_source(variant), changed = original == 1 ? 0 : 1;
		unsigned const checkpoint = std::array<unsigned, 4>{4, 5, 6, 8}[variant / 3072];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "All six signed accumulator branch conditions have staged ownership");
		if (tick == 4)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(((variant / 1536) % 2 ? 0x20000 : 0) + 0x601, 0xdead);
		}
		if (poison && tick == 6) m_cpu->set_state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, changed);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save signed branch before/after predicate and delay-slot effects");
		}
		if (tick == 30)
		{
			u64 const compared = delayed || !taken ? 7 : poison ? changed : original, other = taken ? 0x55 : 0x11;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == unsigned(taken) && m_pipeline_extensions_reads == 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (b ? other : compared) && m_cpu->state_int(tms320c54x_device::STATE_B) == (b ? compared : other) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | ((flags & 1) ? 0x100 : 0) | ((flags & 2) ? 0x200 : 0)),
				"Signed branch uses all 40 bits independent of SXM/OVM and retains its decision through later source changes");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore signed branch without refetching its literal or target");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252980; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 12287) { start_pipeline_signed_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged signed branches: PASS cases=12288 conditions=GE,LT,NE,EQ,GT,LE accumulators=A,B forms=BC,BCD patterns=8 sxm_ovm=4 pages=2 restore=12288 post_evaluate_change=0,1 duplicate_fetches=0\n");
			if (!strcmp(machine().options().bios(), "signed-branch")) throw emu_fatalerror(0, "TMS320C54x isolated signed branches complete");
			start_pipeline_signed_branch_producer(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252980)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252970;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252990)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		u16 const count = std::array<u16, 4>{0, 1, 2, 0xffff}[variant % 4];
		u64 const result = count ? count - 1 : 0xffffffffffULL;
		unsigned const checkpoint = std::array<unsigned, 3>{6, 8, 9}[variant / 4];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "The original LDM/long-SUB/AGT chain has coherent staged ownership");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original signed-branch producer across SUB retirement and evaluation");
		}
		if (tick == 30)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 2 && m_pipeline_extensions_reads == unsigned(count > 1) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == result && m_cpu->state_int(tms320c54x_device::STATE_B) == (count > 1 ? 0x55 : 0x11) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa608 | (count ? 0x800 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1000,
				"AGT consumes the retired signed SUB result for negative, zero and positive counts without input or firmware-state forcing");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore original AGT producer without duplicate source or target reads");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253000; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			if (variant < 11) { start_pipeline_signed_branch_producer(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged signed branch producer: PASS cases=12 counts=0,1,2,65535 restore=12 ldm_read=5 sub_read=7 target_fetch=10 duplicate_reads=0 data_writes=0\n");
			start_pipeline_bitfield(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253000)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252990;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_extended_banz_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253090)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const delayed = !strcmp(machine().options().bios(), "banzd-long") && variant >= 2304, two_word = delayed && variant >= 4608;
		auto const f = extended_banz_case(variant % 2304);
		bool const compatibility = (variant / 192) % 2;
		unsigned const checkpoint = std::array<unsigned, 3>{4, 5, 6}[(variant % 2304) / 768];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Long-offset BANZ owns its predicate, offset and target");
		if (delayed && tick == 10)
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (two_word ? 0x33 : 0x11) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (two_word ? 0 : 0x22),
				"Both delay words execute before the target can overwrite their accumulator result");
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xbeef); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x602, 0x740);
		}
		if (tick == 6)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == f.next_ar,
				"Long-offset BANZ tests the computed address, preserving ordinary AR or applying premodification once");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, f.resolved ? 0 : 1);
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 32);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save BANZ after first literal, complete Decode or Access decision");
		}
		if (tick == 18)
		{
			u16 const st0 = compatibility && f.reg ? (0x6608 & ~0xe000) | (f.reg << 13) : compatibility ? 0x6608 : 0x608;
			expect(m_pipeline_extensions_reads == 2 && m_pipeline_operand_reads == unsigned(f.resolved != 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (f.resolved ? 0x44 : two_word ? 0x33 : 0x11) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (delayed ? two_word ? 0 : 0x22 : f.resolved ? 0 : 0x22) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (compatibility ? 0x20 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == (f.resolved ? 0 : 1),
				"BANZ retains its predicate and target despite later AR/BK/literal changes, without data-memory access or flag changes");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore BANZ without target refetch or duplicate pointer modification");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253100; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (delayed ? 6911 : 2303)) { start_pipeline_extended_banz(variant + 1); return true; }
			if (delayed)
			{
				osd_printf_info("TMS320C54x staged long-offset BANZD: PASS cases=4608 restore=4608 forms=offset,preincrement,circular registers=8 offset_pairs=8 compatibility=0,1 pages=2 delay_forms=two_single,one_double offset_fetches=1 target_fetches=1 data_accesses=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated long BANZD complete");
			}
			osd_printf_info("TMS320C54x staged long-offset BANZ: PASS cases=2304 forms=offset,preincrement,circular registers=8 offset_pairs=8 compatibility=0,1 pages=2 restore=2304 offset_fetches=1 target_fetches=1 data_accesses=0 silicon_phase_claim=0\n");
			if (!strcmp(machine().options().bios(), "banzd-long")) { start_pipeline_extended_banz(2304); return true; }
			start_pipeline_immediate_arithmetic(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253100)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253090;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_banzd_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253250)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = banzd_case(variant);
		u64 const a = tick >= 10 ? f.value ? 0x44 : 0x66 : f.two_word ? tick >= 9 ? 0x33 : 0 : tick >= 8 ? 0x11 : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == a &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (!f.two_word && tick >= 9 ? 0x22 : 0x1234), "BANZD executes its two delay words once before the selected continuation");
		if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0x740); }
		if (tick == 5)
		{
			for (unsigned ar = 0; ar < 8; ++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == f.selected ? f.next_ar : 0x410 + ar * 0x10), "BANZD tests the pre-modification count and changes only its selected AR in Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2222);
		}
		if (tick == std::array<unsigned, 4>{4, 5, 8, 9}[variant / 768] && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save BANZD target/predicate or draining delay words");
		}
		if (tick == 22)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_extensions_reads == 1 && m_pipeline_operand_reads == unsigned(bool(f.value)) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == 0x2222 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x608 | (f.compatibility ? f.selected << 13 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (f.compatibility ? 0x20 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
				"BANZD preserves its captured target/count through changed AR/literal and restores without duplicate modification");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore delayed count branch without refetching target or repeating delay words");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253260; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 3071) { start_pipeline_banzd(variant + 1); return true; }
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			osd_printf_info("TMS320C54x staged BANZD: PASS cases=3072 registers=8 values=4 modifications=3 cmpt_states=2 pages=2 delay_forms=two_single,one_double restore=3072 duplicate_target_fetches=0 data_accesses=0 silicon_phase_claim=0\n");
			start_pipeline_stlm_t(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253260)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253250; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_swap_body_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253310)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		u16 const source = swap_body_source(variant), flags = (variant >> 4) & 15;
		u16 const st1 = 0x800 | ((flags & 1) ? 0x100 : 0) | ((flags & 2) ? 0x200 : 0) | ((flags & 4) ? 0x40 : 0) | ((flags & 8) ? 0x4000 : 0);
		u64 const b = tick >= 9 ? (u32(source) << 8) | (source >> 8) : tick >= 8 ? u32(source) << 8 : tick >= 6 ? source : 0xff80000000ULL;
		u16 const st0 = tick >= 8 ? 0x608 : tick >= 7 ? 0x608 | ((source & 0x80) ? 0x800 : 0) : 0xe08;
		if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) || m_cpu->state_int(tms320c54x_device::STATE_A) != (tick >= 7 ? source >> 8 : 0xdead) ||
			m_cpu->state_int(tms320c54x_device::STATE_B) != b || m_cpu->state_int(tms320c54x_device::STATE_AR6) != (tick >= 9 ? 0x5001 : 0x5000) ||
			m_cpu->state_int(tms320c54x_device::STATE_ST0) != st0 || m_cpu->state_int(tms320c54x_device::STATE_ST1) != st1)
			osd_printf_info("Swap body mismatch: case=%u tick=%u source=%04x a=%010llx b=%010llx ar6=%04x st0=%04x st1=%04x expected_b=%010llx expected_st0=%04x\n",
				variant, tick, source, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), (unsigned long long)b, st0);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (tick >= 7 ? source >> 8 : 0xdead) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == b && m_cpu->state_int(tms320c54x_device::STATE_AR6) == (tick >= 9 ? 0x5001 : 0x5000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == st1,
			"Original byte-swap body preserves dependent accumulator order and modifies AR6 once in store Read");
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x5000, 0xdead); }
		if (tick == std::array<unsigned, 3>{3, 5, 9}[variant / 512] && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original byte-swap body before/after Read or before store Execute");
		}
		if (tick == 18)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 &&
					m_cpu->space(AS_DATA).read_word(0x5000) == u16((source << 8) | (source >> 8)), "Original byte-swap body reaches IDLE with one retained read and one write");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore original body without duplicate reads, writes or pointer modification");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253320; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1535) { start_pipeline_swap_body(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original swap body: PASS cases=1536 patterns=16 flags=16 pages=2 restore=1536 read=Read write=Execute ar6_update=Read duplicate_reads=0 duplicate_writes=0 block_repeat_claim=0 silicon_phase_claim=0\n");
			if (!strcmp(machine().options().bios(), "swap-body"))
				throw emu_fatalerror(0, "TMS320C54x isolated swap body complete");
			start_pipeline_swap_join(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253320)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253310; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253330)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		if (tick == 5 || tick == 10)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(tick == 5 ? 0x5000 : 0x5001, 0xdead);
		}
		if (tick == std::array<unsigned, 3>{3, 9, 10}[variant / 512] && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save original body join before Read, after postincrement or after overlapping read/write");
		}
		if (tick == 24)
		{
			{
				auto const disable = machine().disable_side_effects();
				u16 const first = swap_body_source(variant), second = first ^ 0x5a3c;
				expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
					m_pipeline_operand_reads == 2 && m_pipeline_store_writes == 2 && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x5002 &&
					m_cpu->space(AS_DATA).read_word(0x5000) == u16((first << 8) | (first >> 8)) &&
					m_cpu->space(AS_DATA).read_word(0x5001) == u16((second << 8) | (second >> 8)), "Two original bodies preserve adjacent source words through save-state replay");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore consecutive bodies without repeating address updates or bus transactions");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253340; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1535) { start_pipeline_swap_join(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged original swap join: PASS cases=1536 patterns=16 flags=16 pages=2 restore=1536 bodies=2 gap_words=0 reads=2 writes=2 ar6_updates=2 block_repeat_claim=0 silicon_phase_claim=0\n");
			if (!strcmp(machine().options().bios(), "swap-join")) throw emu_fatalerror(0, "TMS320C54x isolated swap join complete");
			start_pipeline_rptb(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253340)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253330; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_repeat_rearm_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255540)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = 6 + variant / 2;
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save actual in-body BRC exclusion before or after its Access guard"); }
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_store_writes && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 3 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x60b && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1200 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1300 && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400, "Actual in-body STM BRC remains excluded before publication or body effects");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore the in-body guard without enabling a BRC write"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255550; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 7) { start_repeat_rearm(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-body STM BRC exclusion: PASS cases=8 restore=8 pages=2 snapshots=4 write_owner=Access publications=0 body_effects=0 silicon_trap_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated in-body STM BRC exclusion complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255550) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255540; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255520)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const first = std::array<unsigned, 4>{0, 1, 3, 7}[variant % 4], second = (variant / 4) % 2 ? 2 : 0;
		u32 const base = (variant / 8) % 2 ? 0x20000 : 0;
		unsigned const checkpoint = 10 + first * 3 + variant / 16;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Post-loop STM must not inherit the older endpoint's body ownership");
		if (tick == checkpoint && !m_pipeline_replay) { m_pipeline_store_case = m_pipeline_store_writes; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save adjacent block rearm across endpoint and STM publication"); }
		if (tick == 100)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == base + 0x60e && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1201 + first && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1301 + second && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1401 && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x609 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x60b && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_pipeline_store_writes == 1, "Consecutive blocks own separate counts and execute their continuation once");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_store_writes = m_pipeline_store_case; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore rearm without duplicate BRC publication or iterations"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255530; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 63) { start_repeat_rearm(variant + 1); return true; }
			osd_printf_info("TMS320C54x adjacent block rearm: PASS cases=64 restore=64 first_counts=0,1,3,7 second_counts=0,2 pages=2 snapshots=4 brc_publications=1 actual_iterations=1 write_owner=Access interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated adjacent block rearm complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255530) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255520; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255820)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = fcala_delay_case(variant); auto const &c = m_fcala_delay_counts;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Delayed accumulator far call and its complete delay words remain admitted");
		if (tick == 5)
		{
			m_cpu->set_state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, 0xdeadbeef);
			if (f.source) m_cpu->set_input_line(f.source == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		}
		if (tick == f.checkpoint && !m_pipeline_replay) { m_fcala_delay_snapshot = c; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save delayed accumulator target frame delays and settled pending interrupt"); }
		if (tick == 64)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.caller + 0x604 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && c.frames == 2 && c.reads == 2 && c.delays == (f.wide ? 1U : 2U) && c.callee == 1 && c.vectors == unsigned(bool(f.source)) && c.irq_reads == unsigned(bool(f.source)) && c.target_fetches == 1 + unsigned(f.source && c.irq_pc == 0x700), "FCALAD reaches its retained target, retires delay work once and balances both call and interrupt frames");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (f.b ? 0x1234 : 0xdeadbeef) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.b ? 0xdeadbeef : 0x2345) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1501 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == (f.wide ? 0x1401 : 0x1400) && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x4400 && m_cpu->state_int(tms320c54x_device::STATE_T) == (f.wide ? 0x5aa5 : 0xbeef) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == (f.wide ? 0x2468 : 0xbeef) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.source ? 0x100 : 0x900), "FCALAD preserves flags and uses the accumulator value sampled before delay effects");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_fcala_delay_counts = m_fcala_delay_snapshot;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore delayed far call without duplicate target frame or IRQ"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255830; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 647) { start_fcala_delay(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged FCALAD: PASS cases=648 restore=648 accumulators=2 caller_pages=3 target_pages=3 delay_layouts=2 sources=none,IRQ,NMI snapshots=6 target_sample=Read return=PC_plus_3 frames=balanced delay_effects=once nominal_callee_execute=10 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated staged FCALAD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255830) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255820; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	if (m_phase == 255840)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = body_repeat_case(variant); auto const &c = m_saved_repeat_counts;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Direct repeat-body IRQ composition remains admitted");
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const checkpoint = f.snapshot == 0 ? tick == 4 : f.snapshot == 1 ? c.vectors && cycle >= c.vector_cycle + 2 :
			f.snapshot == 2 ? c.inner_writes && cycle >= c.inner_cycle + 2 : f.snapshot == 3 ? c.controls >= 4 && cycle >= c.restore_cycle + 2 :
			f.snapshot == 4 ? c.controls >= 5 && cycle >= c.st1_cycle + 1 : c.resumed && cycle >= c.resume_cycle + 2;
		if (checkpoint && !m_pipeline_replay && !m_saved_repeat_checkpoint) { m_saved_repeat_checkpoint = tick; m_saved_repeat_snapshot = c; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save direct IRQ repeat before and after context restoration"); }
		if (m_cpu->state_int(tms320c54x_device::STATE_IDLE) || tick == 5000)
		{
			unsigned const iterations = f.count + 1;
			if (c.outer_writes != iterations) osd_printf_info("Body-repeat mismatch: case=%u replay=%u pc=%06x brc=%04x st1=%04x outer=%u expected=%u inner=%u\n", variant, unsigned(m_pipeline_replay), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), c.outer_writes, iterations, c.inner_writes);
			expect(m_saved_repeat_checkpoint && c.outer_reads == iterations && c.outer_writes == iterations && c.inner_writes == f.inner + 1 && c.assertions == 1 && c.vectors == 1 && c.mapped_reads == 6 && c.stack_reads == 7 && c.stack_writes == 7 && c.controls == 6 && c.resumed == 1, "Direct ISR return completes every outer iteration and balances one software-saved frame");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x60e && m_cpu->state_int(tms320c54x_device::STATE_SP) == f.sp &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x100 && !m_cpu->state_int(tms320c54x_device::STATE_BRC) &&
				m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x60b &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2345 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7101 + f.inner && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1600 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1701,
				"FRETE and FRETED reactivate restored architectural loop bounds without a hidden context stack");
			{ auto const disable = machine().disable_side_effects(); for (unsigned i = 0; i < iterations; ++i) expect(m_cpu->space(AS_DATA).read_word(0x6000 + i) == u16(0x1357 ^ (i * 0x2345)), "Final copied memory matches the independent iteration oracle"); }
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = m_saved_repeat_checkpoint; m_saved_repeat_counts = m_saved_repeat_snapshot; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore direct IRQ repeat without replaying input or stack transactions"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255850; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_body_repeat(variant + 1); return true; }
			osd_printf_info("TMS320C54x direct IRQ repeat restoration: PASS cases=768 restore=768 outer_counts=1,2,7,56 inner_counts=0,1,3,63 sources=IRQ,NMI pages=2 returns=FRETE,FRETED snapshots=6 ordered_copies=1 balanced_frames=1 near_return=0 physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated direct IRQ repeat restoration complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255850) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255840; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_repeat_context_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255480)
	{
		bool const pmst = !strcmp(machine().options().bios(), "repeat-pmst");
		bool const indirect = !strcmp(machine().options().bios(), "repeat-popd");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = repeat_context_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Repeat-context restoration outside the loop body remains admitted");
		if (tick == f.checkpoint && !m_pipeline_replay) { m_repeat_context_saved_counts = m_repeat_context_counts; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save active repeat context around push pop publication and return"); }
		if (tick == 600)
		{
			auto const &c = m_repeat_context_counts; unsigned const calls = f.calls();
			if (pmst) expect(c.pmst_reads == calls && c.pmst_saves == calls && c.pmst_pops == calls && c.pmst_writes == calls && !m_cpu->state_int(tms320c54x_device::STATE_PMST), "Active-repeat PMST restoration preserves one complete stack transaction per call");
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(STATE_GENPC) != f.base + f.end() + 3 || c.body_reads != f.copies())
				osd_printf_info("Repeat-context mismatch: case=%u replay=%u replace=%u pc=%06x brc=%04x rsa=%04x rea=%04x st1=%04x reads=%u writes=%u restore_reads=%u publications=%u\n", variant, unsigned(m_pipeline_replay), unsigned(f.replace), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_RSA)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_REA)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), c.body_reads, c.body_writes, c.restore_reads, c.control_writes);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end() + 3 && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_RSA) == f.start() && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end() &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_SP) == f.sp && m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + f.copies() && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + f.copies() && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400 + f.count + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1701 &&
				c.body_reads == f.copies() && c.body_writes == f.copies() && c.mapped_reads == (f.replace ? 0 : 3 * calls) && c.saved_writes == (f.replace ? 0 : 3 * calls) && c.restore_reads == 3 * calls && c.control_writes == 3 * calls && c.frame_reads == calls && c.frame_writes == (f.replace ? 2 : calls),
				"Same or replaced active repeat context resumes its actual saved registers with ordered copies and balanced frames");
			{ auto const disable = machine().disable_side_effects(); for (unsigned i = 0; i < f.copies(); ++i) expect(m_cpu->space(AS_DATA).read_word(0x6000 + i) == u16(0x1357 ^ (i * 0x2345)), "Restored repeat context final buffer matches independent oracle"); }
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_repeat_context_counts = m_repeat_context_saved_counts; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore suspended repeat context without duplicate stack or body work"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255490; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_repeat_context(variant + 1); return true; }
			if (indirect)
			{
				osd_printf_info("TMS320C54x active repeat POPD context: PASS cases=384 restore=384 contexts=same,replaced counts=0,1,2,7 layouts=2 pages=2 stack_alignments=2 snapshots=6 control_order=REA,RSA,BRC source_retained=1 publications=once actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated active repeat POPD context complete");
			}
			if (pmst)
			{
				osd_printf_info("TMS320C54x active repeat POPD PMST: PASS cases=384 restore=384 contexts=same,replaced counts=0,1,2,7 layouts=2 pages=2 stack_alignments=2 snapshots=6 pmst_unchanged=1 stack_retained=1 publications=once actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated active repeat POPD PMST complete");
			}
			osd_printf_info("TMS320C54x active repeat context: PASS cases=384 restore=384 modes=same,replaced counts=0,1,2,7 layouts=2 pages=2 stack_alignments=2 snapshots=6 control_order=REA,RSA,BRC source_reads=once publications=once source_mutation_after_read=1 ordered_copies=1 balanced_frames=1 brc_gap_words=6 interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated active repeat context complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255490) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255480; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255460)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_conditional_call_case(variant); unsigned const iterations = f.count + 1, calls = f.taken() ? iterations : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Block CCD and complete delay span remain admitted");
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_fast_stack_reads, m_pipeline_register_writes, m_pipeline_extensions_reads, m_pipeline_drained_fetches};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save block CCD before predicate, frame, endpoint or return");
		}
		if (tick == 400)
		{
			if (m_pipeline_operand_reads != iterations || m_pipeline_store_writes != iterations || m_cpu->state_int(tms320c54x_device::STATE_BRC) || m_cpu->state_int(STATE_GENPC) != f.base + f.end() + 3)
				osd_printf_info("Block CCD mismatch: case=%u replay=%u endpoint=%u taken=%u pc=%06x brc=%04x st1=%04x reads=%u writes=%u frames=%u returns=%u calls=%u expected_iterations=%u\n", variant, unsigned(m_pipeline_replay), unsigned(f.endpoint), unsigned(f.taken()), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_pipeline_drained_fetches, iterations);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end() + 3 && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end() &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == unsigned(f.zero) && m_cpu->state_int(f.b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (f.b ? 0x123456789aULL : 0xfedcba9876ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400 + (f.endpoint ? 0 : iterations) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1500 + (f.long_delay ? 0 : iterations) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1600 + calls && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1701 &&
				m_pipeline_operand_reads == iterations && m_pipeline_store_writes == iterations && m_pipeline_extensions_reads == iterations && m_fast_stack_reads == calls && m_pipeline_register_writes == calls && m_pipeline_drained_fetches == calls,
				"Block CCD balances calls and returns through every Decode-owned outer iteration, including endpoint delay spans");
			{ auto const disable = machine().disable_side_effects(); for (unsigned i = 0; i < iterations; ++i) expect(m_cpu->space(AS_DATA).read_word(0x6000 + i) == u16(0x1357 ^ (i * 0x2345)), "Block CCD final memory matches independent copy oracle"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_fast_stack_reads = m_recurring_irq_snapshot[2]; m_pipeline_register_writes = m_recurring_irq_snapshot[3]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[4]; m_pipeline_drained_fetches = m_recurring_irq_snapshot[5];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore block CCD without losing loop state or duplicating transactions"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255470; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1023) { start_block_conditional_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x delayed conditional calls in block: PASS cases=1024 restore=1024 counts=0,1,2,7 conditions=AEQ,ANEQ,BEQ,BNEQ outcomes=2 positions=middle,endpoint delay_layouts=2 pages=2 snapshots=4 ordered_copies=1 balanced_frames=1 stack_return=PC+4 outer_count=Decode interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed conditional calls in block complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255470) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255460; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255440)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = 2 + variant / 16;
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save unadmitted CCD predicate before or after Decode guard"); }
		if (tick == 16)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->pipeline_cycles() == 3 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x5555 &&
				!m_pipeline_operand_reads && !m_pipeline_store_writes && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900, "Unadmitted CCD conditions stop before predicate side effects or stack reservation");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_pipeline_store_writes = 0; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore unadmitted CCD condition without fallback execution"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255450; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 31) { start_delayed_conditional_call_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x delayed conditional call exclusions: PASS cases=32 restore=32 conditions=8 pages=2 snapshots=2 stack_reads=0 stack_writes=0 stack_reservation=0 status_changes=0 silicon_trap_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed conditional call exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255450) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255440; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255420)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delayed_conditional_call_case(variant,!strcmp(machine().options().bios(),"tc-delay-call"));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "CCD and complete delay instructions stay in supported forms");
		if (!f.taken) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Untaken CCD never reserves a stack frame");
		if (tick == 3 || tick == 4 || tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + (tick == 3 ? 0x601 : tick == 4 ? 0x602 : 0x603), 0xdead); }
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_fast_stack_reads, m_pipeline_drained_fetches}; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save CCD predicate, pending stack write, delay execution or return");
		}
		if (tick == 40)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x606 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_RTN) == (f.taken ? 0x604 : 0x5555) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.status(true) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 &&
				m_cpu->state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == f.changed && m_cpu->state_int(f.b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (f.b ? 0x123456789aULL : 0xfedcba9876ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1401 && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1500 + unsigned(f.tc || !f.layout) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1600 + unsigned(f.taken) &&
				m_pipeline_operand_reads == unsigned(f.taken) && m_pipeline_store_writes == unsigned(f.taken) && m_pipeline_drained_fetches == unsigned(f.taken) && m_pipeline_extensions_reads == 1 && m_fast_stack_reads == 2,
				"CCD retains the original predicate, runs delay work once on both paths and balances only a taken frame");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_pipeline_drained_fetches = m_recurring_irq_snapshot[4];
				m_cpu->set_pipeline_frontend(true); m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore delayed conditional call without duplicate effects or a changed decision"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255430; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.tc ? 1279 : 1919)) { start_delayed_conditional_call(variant + 1); return true; }
			if(f.tc) { osd_printf_info("TMS320C54x TC delayed calls: PASS cases=1280 restore=1280 predicates=TC,NTC initial_tc=2 accumulator_patterns=8 seed_accumulators=2 delay_layouts=2 pages=2 snapshots=5 tc_flip=organic_delay target_delay_poison=1 taken_frame=PC+4 untaken_stack_accesses=0 balanced_return=1 reset_before_restore=1 block_interrupt_acceptance=0 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated TC delayed calls complete"); }
			osd_printf_info("TMS320C54x delayed conditional call: PASS cases=1920 restore=1920 conditions=12 patterns=8 delay_layouts=2 pages=2 snapshots=5 target_literals=1 delay_words=2 delay_effects=once predicate_retained=1 taken_frame=PC+4 untaken_stack_accesses=0 balanced_return=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed conditional call complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255430) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255420; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_basic_repeat_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 310460)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = (variant < 36 ? variant < 18 : variant < 68) ? 3 : 10;
		expect(!m_rptbd_counts.reads && !m_rptbd_counts.writes, "Excluded memory repeat performs no data transaction");
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save excluded count before or after rejection"); }
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_AR1) == (variant < 36 && variant % 9 == 4 ? 0x1f : 0x5000), "Excluded memory count preserves stack, repeat counters and preincrement pointer");
			if (variant >= 36) for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == 0x5000, "Write-only MOD=3 RPT forms reject before any auxiliary-register update");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore excluded memory count without side effects"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 310470; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 99) { start_basic_repeat_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x basic memory repeat exclusions: PASS cases=100 restore=100 forms=control,peripheral,write_only_MOD3,long_offset,nested,delay,block mod3_cases=64 arf=8 cmpt=2 pages=2 snapshots=2 data_reads=0 data_writes=0 pointer_changes=0 silicon_trap_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated basic memory repeat exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310470) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310460; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 310440)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = basic_repeat_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Basic memory repeat retains an admitted count and body");
		if (tick == (f.absolute ? 5U : 4U)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.body, 0xe8ff); }
		if (tick == (f.absolute ? 6U : 5U) && f.address >= 0x60) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); }
		if (f.irq && tick == 3) m_cpu->set_input_line(f.irq == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; m_rptbd_saved_return = m_rptbd_return; m_saved_observed_instructions = m_observed_instructions;
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save memory repeat before count capture, during hold or body");
		}
		if (tick == f.count + 80)
		{
			unsigned iterations = 0; for (auto const &[address, word] : m_observed_instructions) if (address == f.body) { expect(word == 0xf495, "Memory repeat executes the retained body word"); ++iterations; }
			if (iterations != f.count + 1 || m_rptbd_counts.reads != 1 || m_rptbd_counts.fetches != 1 || m_rptbd_counts.acks != unsigned(bool(f.irq)))
				osd_printf_info("Memory repeat mismatch case=%u replay=%u count=%u iterations=%u reads=%u fetches=%u acks=%u stack=%u/%u pc=%06x rc=%04x\n", variant, unsigned(m_pipeline_replay), f.count, iterations, m_rptbd_counts.reads, m_rptbd_counts.fetches, m_rptbd_counts.acks, m_rptbd_counts.stack_reads, m_rptbd_counts.stack_writes, unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_RC)));
			expect(iterations == f.count + 1 && m_rptbd_counts.reads == 1 && m_rptbd_counts.fetches == 1 &&
				m_rptbd_counts.acks == unsigned(bool(f.irq)) && m_rptbd_counts.stack_reads == unsigned(bool(f.irq)) && m_rptbd_counts.stack_writes == unsigned(bool(f.irq)), "Memory repeat executes count+1 bodies with one count read and a balanced deferred interrupt");
			u16 const st0 = f.compatibility && f.form >= 2 && f.form < 26 && (f.mode & 7) ? (f.st0 & ~0xe000) | ((f.mode & 7) << 13) : f.st0;
			for (unsigned i = 0; i < 8; ++i)
			{
				u16 const ar = f.address == 0x10 + i ? f.count : 0x5000 + (f.form >= 2 && f.form < 26 && i == f.selected ? f.modification == 1 ? -1 : f.modification == 2 ? 1 : 0 : 0);
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == ar, "Count addressing modifies its selected pointer once, never once per body");
			}
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x610 && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL, "Memory repeat preserves flags and unrelated state through its fresh continuation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_rptbd_counts = m_rptbd_saved_counts; m_rptbd_return = m_rptbd_saved_return; m_observed_instructions = m_saved_observed_instructions;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore sampled count and pointer without resampling or duplicated effects"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 310450; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 10375) { start_basic_repeat(variant + 1); return true; }
			osd_printf_info("TMS320C54x basic memory repeat: PASS cases=10376 restore=10376 forms=36 counts=0,1,7,255,32768,65535 pages=2 cpl=2 cmpt=2 sources=none,IRQ,NMI snapshots=3 count_reads=1 body_fetches=1 body_count=rc+1 pointer_effect=once retained_count=1 balanced_frames=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated basic memory repeat complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310450) { m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_phase = 310451; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 310451)
	{
		auto const f = basic_repeat_case(m_pipeline_peripheral_case);
		// Restore the fixture's external asserted-until-ack input too:
		// the earliest snapshot can precede queued line synchronization.
		if (f.irq && f.checkpoint == 3) m_cpu->set_input_line(f.irq == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310440; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_rptbd_layout_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255320)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, layout = rptbd_layout(variant), iterations = rptbd_count(variant) + 1;
		bool const interrupt = variant >= 512;
		unsigned const checkpoint = interrupt ? std::array<unsigned, 6>{3, 4, 6, 8, 11, 17}[(variant - 512) / 128] : std::array<unsigned, 4>{3, 6, 8, 13}[variant / 128];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RPTBD fixture remains in supported staged forms");
		// Replace fetched words and sampled RAM, never firmware architectural state.
		if (tick == 3) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(rptbd_base(variant) + 0x601, 0xdead); }
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(rptbd_base(variant) + 0x603, 0xdead); }
		if (tick == 8 && layout == 2) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x4003, 0xdead); }
		if (interrupt && tick == 5 && !rptbd_late_irq(variant)) m_cpu->set_input_line(rptbd_irq_line(variant), ASSERT_LINE);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			m_rptbd_saved_return = m_rptbd_return;
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save RPTBD header, delay span or active loop");
		}
		if (tick == (interrupt ? 160 : 96))
		{
			u64 const a = layout == 2 ? u64(s64(s16(rptbd_source(variant, 0)))) & 0xffffffffffULL : 0x123456789aULL;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == rptbd_base(variant) + (interrupt ? 0x610 : 0x608) &&
				!m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x606 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (interrupt ? 0x100 : 0x900) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x4000 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7100 + unsigned(layout == 0) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x7200 + unsigned(layout == 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == (layout == 1 ? 0x1234 : 0x5aa5) &&
				m_rptbd_counts.reads == iterations && m_rptbd_counts.writes == iterations && m_rptbd_counts.fetches == 3 && m_rptbd_counts.delay_reads == unsigned(layout == 2) && m_rptbd_counts.delay_writes == unsigned(layout == 1),
				"RPTBD executes delay work once and body BRC+1 times, preserving unrelated state");
			expect(m_rptbd_counts.acks == unsigned(interrupt) && m_rptbd_counts.stack_reads == unsigned(interrupt) && m_rptbd_counts.stack_writes == unsigned(interrupt), "RPTBD balances one optional interrupt frame and acknowledges its source once");
			{ auto const disable = machine().disable_side_effects(); for (unsigned i = 0; i < iterations; ++i) expect(m_cpu->space(AS_DATA).read_word(0x6000 + i) == rptbd_source(variant, i), "RPTBD final copy matches independent source oracle"); }
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_rptbd_counts = m_rptbd_saved_counts;
				m_rptbd_return = m_rptbd_saved_return;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore RPTBD without fetching retained words or duplicating delay effects");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255330; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (interrupt ? 1279U : 511U)) { start_rptbd(variant + 1); return true; }
			if (interrupt)
			{
				osd_printf_info("TMS320C54x delayed block repeat interrupts: PASS cases=768 restore=768 counts=0,1,2,7 delay_layouts=4 pages=2 sources=IRQ,NMI arrivals=delay,first_write snapshots=6 delay_effects=once ordered_copies=1 balanced_frames=1 retained_words=1 physical_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated delayed block repeat interrupts complete");
			}
			osd_printf_info("TMS320C54x delayed block repeat: PASS cases=512 restore=512 counts=0,1,2,7 delay_layouts=4 patterns=4 pages=2 snapshots=4 delay_effects=once ordered_copies=1 retained_words=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed block repeat complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255330)
	{
		if (m_pipeline_peripheral_case >= 512) { m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_phase = 255331; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255320; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255331) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255320; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255340)
	{
		unsigned const variant = m_pipeline_peripheral_case, kind = variant % 7, tick = ++m_idle_fixture_tick;
		unsigned const guard = kind == 5 ? 8 : kind == 4 ? 6 : 5, checkpoint = guard - 1 + variant / 14;
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save RPTBD exclusion before or after the guard");
		}
		if (tick == 16)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->pipeline_cycles() == guard && !m_rptbd_counts.reads && !m_rptbd_counts.writes &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == 2 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5,
				"RPTBD exclusions stop before bus effects, pointer updates, BRC writes or frame allocation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_rptbd_counts = m_rptbd_saved_counts;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore RPTBD exclusion at its exact retained guard");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255350; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 27) { start_rptbd_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x delayed block repeat exclusions: PASS cases=28 restore=28 forms=STM_BRC,STLM_BRC,POPM_BRC,branch,spill,nested_body,nested_delay pages=2 snapshots=2 data_reads=0 data_writes=0 pointer_updates=0 counters_preserved=1 silicon_trap_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed block repeat exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255350) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255340; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 253350)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, count = rptb_count(variant);
		bool const irq = variant >= 6144;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RPTB fixture remains in supported staged forms");
		if (!irq)
		{
			unsigned const ended = tick < 11 ? 0 : (tick - 11) / 5 + 1;
			expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == (ended < count ? count - ended : 0) &&
				bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (tick >= 4 && tick < 7 + count * 5), "RPTB count and final BRAF decision are Decode-owned, not retirement-owned");
		}
		unsigned const checkpoint = irq ? std::array<unsigned, 6>{3, 6, 9, 13, 14, 20}[(variant - 6144) / 4] : std::array<unsigned, 3>{3, 6, 13}[variant / 2048];
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save block setup, prefetch/Decode loop state or pending interrupt");
		}
		if (tick == 96)
		{
			{
				auto const disable = machine().disable_side_effects();
				if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_pipeline_operand_reads != count + 1 || m_pipeline_store_writes != count + 1 ||
					m_cpu->state_int(tms320c54x_device::STATE_AR6) != 0x5001 + count || m_cpu->state_int(tms320c54x_device::STATE_BRC) ||
					(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1000 || m_pipeline_extensions_reads != unsigned(irq))
					osd_printf_info("RPTB mismatch: case=%u replay=%u pc=%06x idle=%u reads=%u writes=%u ar6=%04x brc=%04x st1=%04x sp=%04x vectors=%u\n", variant, unsigned(m_pipeline_replay),
						unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)), m_pipeline_operand_reads, m_pipeline_store_writes,
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)),
						unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), m_pipeline_extensions_reads);
				expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == count + 1 && m_pipeline_store_writes == count + 1 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x5001 + count && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 &&
					!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
					m_pipeline_extensions_reads == unsigned(irq), "RPTB completes count+1 bodies and one optional interrupt without losing loop context");
				for (unsigned i = 0; i <= count; ++i) { u16 const source = rptb_source(variant, i); expect(m_cpu->space(AS_DATA).read_word(0x5000 + i) == u16((source << 8) | (source >> 8)), "RPTB final buffer matches independent byte-swap oracle"); }
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore speculative and architectural block states without duplicate work");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253360; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 6167) { start_pipeline_rptb(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged RPTB: PASS cases=6144 irq_nmi_cases=24 restore=6168 counts=0,1,2,7 zero_overhead_body=5 counter=Decode final_braf=Decode silicon_phase_claim=0\n");
			start_pipeline_rptb_edge(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253360)
	{
		if (m_pipeline_peripheral_case >= 6144)
		{
			// Let synchronized pin events run after the load has completed.
			m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
			m_phase = 253361; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253350; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253361)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253350; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253370)
	{
		unsigned const variant = m_pipeline_peripheral_case, kind = variant & 7, tick = ++m_idle_fixture_tick;
		if (tick == 2 && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save block edge before Decode");
		}
		if (tick == 32)
		{
			expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (kind >= 2) && !m_pipeline_store_writes, "Block control/count modifications and malformed endpoints fail closed without memory writes");
			if (kind < 2)
				expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && !(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR6) == (kind ? 0x5000 : 0x5004) &&
					(!kind || m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234), "One-word and two-word endpoint bodies complete four iterations");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = 2; m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore block edge without atomic fallback");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253380; m_check_timer->enable(true); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 15) { start_pipeline_rptb_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged RPTB edges: PASS cases=16 restore=16 one_word=2 two_word=2 exclusions=12 atomic_fallback=0 silicon_phase_claim=0\n");
			if (!strcmp(machine().options().bios(), "rptb")) throw emu_fatalerror(0, "TMS320C54x isolated RPTB complete");
			start_pipeline_rmw_forward(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253380)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253370; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253390)
	{
		unsigned const variant = m_pipeline_peripheral_case, v = variant % 256, tick = ++m_idle_fixture_tick;
		bool const copy = !strcmp(machine().options().bios(), "mvkd-forward");
		bool const immediate = copy || !strcmp(machine().options().bios(), "imm-forward");
		unsigned const consumer = immediate ? (variant / 768) % 3 : 0, form = immediate ? variant / 2304 : 0, extra = form == 2;
		unsigned const checkpoint = 5 + (variant / 256) % 3 + extra;
		bool const sxm = BIT(v, 2), ovm = BIT(v, 3), compiler = BIT(v, 4), gap = BIT(v, 5), different = BIT(v, 7);
		u16 const literal = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[v & 3];
		u16 const initial = immediate ? 0x2468 : std::array<u16, 4>{0, 0x7fff, 0x8000, 0xffff}[v & 3];
		s32 const sum = (sxm ? s16(initial) : s32(initial)) + 1;
		u16 const result = immediate ? literal : ovm ? u16(std::clamp(sum, s32(-0x8000), s32(0x7fff))) : u16(sum), loaded = different ? 0x1357 : result;
		u64 const a = sxm ? u64(s64(s16(loaded))) & 0xffffffffffULL : loaded;
		u64 const added = (0x1234 + a) & 0xffffffffffULL;
		u16 const st0 = immediate ? 8 | (consumer == 1 && u64(0x1234) + u32(a) > 0xffffffffULL ? 0x800 : 0) : 8 | (sum < -0x8000 || sum > 0x7fff ? 0x400 : 0) |
			(u64(u32(sxm ? s16(initial) : s32(initial))) + 1 > 0xffffffffULL ? 0x800 : 0);
		bool const retired = tick >= 8 + extra + gap;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (retired && consumer != 2 ? consumer == 1 ? added : a : 0x1234) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (retired && consumer == 2 ? a : 0x5678) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((immediate ? retired : tick >= 7) ? st0 : 8), "Adjacent RAM consumer sees the pending producer without early accumulator or flag retirement");
		if (immediate) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == (compiler ? 0x1002 : 0x402) + unsigned(form == 1 && tick >= (copy ? 5 : 6)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (compiler ? 0x4000 : 0)), "Immediate store updates its pointer once and preserves addressing/arithmetic modes");
		if (copy && tick == 6 + extra) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, literal ^ 0x5a5a);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes}; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save dependency before Read, before Write or after forwarding");
			if (immediate)
				// A forwarded value must use the captured literal, not program RAM.
				m_cpu->space(AS_PROGRAM).write_word((BIT(v, 6) ? 0x20000 : 0) + 0x601 + extra, (copy ? 0x11 : literal) ^ 0x5a5a);
		}
		if (tick == 12 + extra)
		{
			expect(m_pipeline_operand_reads == unsigned(!immediate) + unsigned(bool(gap || different)) && m_pipeline_store_writes == 1, "Internal-bus bypass avoids stale RAM reads and duplicate writes");
			if (copy) expect(m_pipeline_register_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_AR1) == (literal ^ 0x5a5a), "MVKD forwarding uses its retained source, not the changed mapped register");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore dependency with its retained producer operand and follower");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253400; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); if (copy) m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (immediate ? 6911 : 767)) { start_pipeline_rmw_forward(variant + 1); return true; }
			if (copy)
			{
				osd_printf_info("TMS320C54x staged MVKD forwarding: PASS cases=6912 restore=6912 forms=direct,postincrement,absolute consumers=LD_A,ADD_A,LD_B same_and_different=1 gap_words=0,1 sxm_ovm=4 pages=2 cpl=2 source_reads=1 writes_each=1 retained_source=1 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated MVKD forwarding complete");
			}
			if (immediate)
			{
				osd_printf_info("TMS320C54x staged immediate forwarding: PASS cases=6912 restore=6912 forms=direct,postincrement,absolute consumers=LD_A,ADD_A,LD_B same_and_different=1 gap_words=0,1 sxm_ovm=4 pages=2 cpl=2 writes_each=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated immediate forwarding complete");
			}
			osd_printf_info("TMS320C54x staged RMW forwarding: PASS cases=768 restore=768 same_and_different=1 gap_words=0,1 flags=4 pages=2 cpl=2 silicon_phase_claim=0\n");
			if (!strcmp(machine().options().bios(), "rmw-forward")) throw emu_fatalerror(0, "TMS320C54x isolated RMW forwarding complete");
			start_pipeline_negate(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253400)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253390; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_rptbd_square_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62178)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = rptbd_square_case(variant);
		unsigned completed = 0; for (unsigned i = 0; i < f.operations(); ++i) completed += tick > 7 + f.slot(i);
		auto const out = f.result(completed); unsigned const last_read = 7 + f.slot(f.operations() - 1), checkpoint = std::array<unsigned, 4>{5, 7, 8, 9}[f.snapshot];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == out[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == out[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == out[2], "RPTBD delay squares retain sequential accumulator, carry and sticky overflow ownership");
		for (unsigned i = 0; i < f.operations(); ++i) if (tick == 7 + f.slot(i)) expect(m_cpu->state_int(tms320c54x_device::STATE_T) == f.memory(i), "Delay square publishes its own T word only at Read");
		if (tick == 6) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x602, 0xdead); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x603, 0xbeef); }
		if (tick == last_read - 1) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR5) == (f.form == 2 ? 0x5000 + f.operations() : 0x5000), "RPTBD square addressing updates its pointer once per source"); m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x7000); }
		if (tick == last_read) { auto const disabled = machine().disable_side_effects(); for (unsigned i = 0; i < f.operations(); ++i) m_cpu->space(AS_DATA).write_word(f.address(i), 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); }
		if (tick > last_read) expect(m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead, "Delay square Execute cannot overwrite a newer T value");
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_pipeline_extensions_reads}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save RPTBD delay square operands and pending block prediction"); }
		if (tick == 64)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == f.operations() && m_pipeline_extensions_reads == 3 && m_pipeline_store_writes == f.count + 1 && m_pipeline_register_writes == f.count + 1 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1() && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x6001 + f.count && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6101 + f.count && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7000 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "RPTBD delay work executes once before all count-plus-one body writes");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[3]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(false); m_cpu->set_pipeline_frontend(true); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore square delay and block state without future pipeline data"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62179; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.memory_square ? 92159U : 147455U)) { start_rptbd_square(variant + 1); return true; }
			if (f.memory_square) { osd_printf_info("TMS320C54x RPTBD memory SQUR: PASS cases=92160 restore=92160 destinations=2 layouts=first,second,both_same,both_split,SQUR_MPYA forms=DP,indirect,postincrement patterns=8 flags=16 counts=0,1,7 pages=2 snapshots=4 source_reads=once delay_words=once t_publish=Read newer_t_preserved=1 smul_product_clamp=0 body_observes_result=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated RPTBD memory SQUR complete"); }
			osd_printf_info("TMS320C54x RPTBD square delays: PASS cases=147456 restore=147456 operations=SQURA,SQURS destinations=A,B layouts=first,second,both_same,both_split forms=DP,indirect,postincrement patterns=8 flags=16 counts=0,1,7 pages=2 snapshots=4 source_reads=once delay_words=once t_publish=Read newer_t_preserved=1 body_observes_result=1 count_plus_one=1 reset_before_restore=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated RPTBD square delays complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62179) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62178; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62176)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, source = (variant / 16) % 3;
		bool const selected = bool(variant & 1) != bool(variant & 2); unsigned const checkpoint = std::array<unsigned, 4>{3, 5, 8, 12}[variant / 48];
		if (tick == 7 && selected && source) m_cpu->set_input_line(source == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "XC return protection must not leak into a later ordinary status write");
		if (m_pipeline_operand_reads && !m_pipeline_register_writes) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x3000, 0xdead); }
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_vector_reads, m_pipeline_extensions_reads}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save XC return, resumed protection and pending vector"); }
		if (tick == 32)
		{
			if (m_vector_reads != unsigned(selected && source)) osd_printf_info("xc_return_vector: case=%u replay=%u vectors=%u ifr=%llx imr=%llx st1=%llx\n", variant, unsigned(m_pipeline_replay), m_vector_reads, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IFR), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IMR), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1));
			bool const valid = m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == unsigned(selected) && m_pipeline_store_writes == unsigned(selected) && m_pipeline_register_writes == unsigned(selected && source) && m_vector_reads == unsigned(selected && source) && (source || m_pipeline_extensions_reads == unsigned(selected)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (selected && !source ? 0x3001 : 0x3000) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (selected ? (m_pipeline_extensions_reads ? 0x340 : 0x100) | (source ? 0x800 : 0) : 0x100);
			if (!valid) osd_printf_info("xc_return_final: case=%u replay=%u selected=%u source=%u read=%u write=%u frame=%u vector=%u status_write=%u sp=%llx st1=%llx pc=%llx idle=%llu\n", variant, unsigned(m_pipeline_replay), unsigned(selected), source, m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_vector_reads, m_pipeline_extensions_reads, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1), (unsigned long long)m_cpu->state_int(STATE_GENPC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE));
			expect(valid, "Selected and annulled XC returns preserve one target sample, balanced stack effects and bounded protection");
			// Do not queue synchronized input changes before read_stream:
			// it restores scheduler timers, not device_input's pending queue.
			// IFR/NMI and any accepted entry are already in the CPU snapshot;
			// a fresh assertion would duplicate the original physical event.
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_vector_reads = m_recurring_irq_snapshot[3]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[4]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(false); m_cpu->set_pipeline_frontend(true); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore XC return protection without retaining future pipeline state"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62177; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 191) { start_xc_return(variant + 1); return true; }
			osd_printf_info("TMS320C54x XC return protection: PASS cases=192 restore=192 predicates=BEQ,BNEQ outcomes=2 resumed_words=one,two pages=2 snapshots=4 sources=none,IRQ,NMI irq_selected_only=1 return_reads=once resumed_write=once later_STM_ST1=ordinary reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated XC return protection complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62177) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62176; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_conditional_call_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253810)
	{
		bool const tc_call = !strcmp(machine().options().bios(), "tc-call");
		bool const returning = !strcmp(machine().options().bios(), "conditional-ret");
		bool const signed_call = !strcmp(machine().options().bios(), "signed-call"), all_conditions = returning || signed_call;
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = (returning ? std::array<unsigned, 4>{3, 4, 5, 7} : std::array<unsigned, 4>{5, 6, 7, 9})[variant / (all_conditions ? 192 : 16)];
		unsigned const patterns = all_conditions ? 8 : 4, condition = all_conditions ? (variant / 32) % 6 : 3;
		u64 const value = std::array<u64, 8>{0, 1, 0x8000000000ULL, 0xffffffffffULL, 0x7fffffffffULL, 0x0100000000ULL, 0x0080000000ULL, 0x00ffffffffULL}[variant % patterns];
		s64 const signed_value = value & 0x8000000000ULL ? s64(value) - 0x10000000000LL : s64(value);
		bool const b = (variant / patterns) % 2;
		bool const taken = tc_call ? bool((variant / 2) & 1) == bool(variant & 1) : std::array<bool, 6>{signed_value >= 0, signed_value < 0, signed_value != 0, signed_value == 0, signed_value > 0, signed_value <= 0}[condition];
		u64 const changed = all_conditions ? (taken ? std::array<u64, 6>{0x8000000000ULL, 0, 0, 1, 0, 1} : std::array<u64, 6>{0, 0x8000000000ULL, 1, 0, 1, 0})[condition] : u64(taken);
		u32 const base = (variant / (patterns * 2)) % 2 ? 0x20000 : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Only admitted CC/RC predicates execute in the staged frontend");
		if (!taken) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000, "Untaken CC leaves SP unchanged throughout the transaction");
		if (returning) expect(bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) == (tick >= (taken ? 11 : 9)), "RC preserves Class 32's five-cycle taken and three-cycle untaken cadence");
		if (tick == 5) { auto const disable = machine().disable_side_effects(); if (returning) m_cpu->space(AS_DATA).write_word(0x1000, 0xdead); else m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead); }
		if (tick == (returning ? 5 : 6)) m_cpu->set_state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, changed);
		if (tc_call && tick == 6) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608 | ((((variant / 2) & 1) ^ 1) << 12));
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_fast_stack_reads}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save CC around predicate, held clock and conditional publication");
		}
		if (tick == 32)
		{
			expect(m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == changed && m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (b ? 0x1234 : 0x2345), "CC/RC preserves the changed predicate source and unselected accumulator");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == base + (returning ? taken ? 0x701 : 0x602 : 0x603) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 + unsigned(returning && taken) && m_pipeline_operand_reads == unsigned(taken) && m_pipeline_store_writes == unsigned(!returning && taken) && m_pipeline_extensions_reads == unsigned(taken) && m_fast_stack_reads == unsigned(!returning) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa608 | (tc_call ? ((((variant / 2) & 1) ^ 1) << 12) : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800, "CC/RC retains its decision and target; untaken paths never touch SP or stack memory");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay conditional allocation and call/return without duplicate accesses"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253820; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (all_conditions ? 767 : 63)) { start_conditional_call(variant + 1); return true; }
			if (returning)
			{
				osd_printf_info("TMS320C54x staged conditional return: PASS cases=768 restore=768 conditions=AGEQ,ALT,ANEQ,AEQ,AGT,ALEQ,BGEQ,BLT,BNEQ,BEQ,BGT,BLEQ patterns=8 pages=2 snapshots=4 taken_reads=1 data_writes=0 untaken_stack_accesses=0 predicate_retained=1 stack_target_retained=1 delayed_acceptance=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated conditional return complete");
			}
			if (tc_call)
			{
				osd_printf_info("TMS320C54x TC conditional calls: PASS cases=64 restore=64 predicates=TC,NTC initial_tc=2 pages=2 snapshots=4 literal_fetches=1 taken_frame=1 balanced_return=1 untaken_stack_accesses=0 predicate_retained=1 target_retained=1 delayed_acceptance=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated TC conditional calls complete");
			}
			if (signed_call)
			{
				osd_printf_info("TMS320C54x signed conditional calls: PASS cases=768 restore=768 predicates=AGEQ,ALT,ANEQ,AEQ,AGT,ALEQ,BGEQ,BLT,BNEQ,BEQ,BGT,BLEQ patterns=8 pages=2 snapshots=4 literal_fetches=1 taken_frame=1 balanced_return=1 untaken_stack_accesses=0 predicate_retained=1 target_retained=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated signed conditional calls complete");
			}
			osd_printf_info("TMS320C54x staged conditional call: PASS cases=64 restore=64 conditions=AEQ,BEQ patterns=4 pages=2 literal_fetches=1 taken_frame=1 untaken_stack_accesses=0 predicate_retained=1 delayed_acceptance=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated conditional call complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253820) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253810; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_delayed_far_call_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253670)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, layout = variant % 3, caller_page = std::array<unsigned, 3>{0, 2, 0x7f}[(variant / 3) % 3], interrupt = (variant / 27) % 3;
		unsigned const checkpoint = std::array<unsigned, 3>{6, 8, 14}[variant / 81];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Delayed far call and both delay layouts remain admitted");
		if (tick == 5 && interrupt) m_cpu->set_input_line(interrupt == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_store_writes, m_fast_stack_reads, m_pipeline_extensions_reads, m_pipeline_operand_reads, m_pipeline_drained_fetches, m_vector_acks, m_pipeline_register_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save delayed far call with retained frame and settled input");
		}
		if (tick == 50)
		{
			// Mapped XPC visibility during delay Read is not fixed by the instruction manual.
			if ((layout != 0 && m_cpu->state_int(tms320c54x_device::STATE_A) != (layout == 2 ? 0x4567 : 0)) || m_cpu->state_int(tms320c54x_device::STATE_B) != 0x55 || m_cpu->state_int(tms320c54x_device::STATE_T) != (layout == 1 ? 0x1234 : 0xbeef) || m_cpu->state_int(tms320c54x_device::STATE_BRC) != (layout == 0 ? 0x1234 : 0x9876) || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1000 || m_cpu->state_int(tms320c54x_device::STATE_XPC) != caller_page)
				osd_printf_info("FCALLD failure: case=%u replay=%u layout=%u source_page=%u irq=%u A=%llx B=%llx T=%llx BRC=%llx SP=%llx XPC=%llx PC=%llx writes=%u reads=%u target=%u continuation=%u acks=%u\n", variant, unsigned(m_pipeline_replay), layout, caller_page, interrupt, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_T), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_BRC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_XPC), (unsigned long long)m_cpu->state_int(STATE_GENPC), m_pipeline_store_writes, m_fast_stack_reads, m_pipeline_register_writes, m_pipeline_drained_fetches, m_vector_acks);
			expect((layout == 0 || m_cpu->state_int(tms320c54x_device::STATE_A) == (layout == 2 ? 0x4567 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_T) == (layout == 1 ? 0x1234 : 0xbeef) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == (layout == 0 ? 0x1234 : 0x9876) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_XPC) == caller_page, "FCALLD executes caller-fetched delay words and returns to its stacked page and continuation");
			expect(m_pipeline_store_writes == 2 + unsigned(bool(interrupt)) && m_fast_stack_reads == m_pipeline_store_writes && m_pipeline_extensions_reads == 1 && m_pipeline_operand_reads == unsigned(layout == 1) && m_pipeline_drained_fetches == 1 && m_pipeline_register_writes == 1 && m_vector_acks == unsigned(bool(interrupt)), "Far call, delay and IRQ frames each own exactly their accesses");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_fast_stack_reads = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_pipeline_operand_reads = m_recurring_irq_snapshot[3]; m_pipeline_drained_fetches = m_recurring_irq_snapshot[4]; m_vector_acks = m_recurring_irq_snapshot[5]; m_pipeline_register_writes = m_recurring_irq_snapshot[6];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay caller page, stacked continuation and complete delayed call"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253680; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_cpu->set_pipeline_frontend(false);
			if (variant < 242) { start_delayed_far_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged FCALLD: PASS cases=243 restore=243 caller_pages=3 target_pages=3 layouts=probe_XPC_STLM,STM,original_NOP_LDM sources=none,IRQ,NMI frame_words=2 complete_delays=2 return_roundtrip=1 xpc_delay_visibility=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed far call complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253680) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253670; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_branch_count_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 254360)
	{
		// Input changes are scheduler events, not yet saved CPU latches.
		// Defer a coincident snapshot until the next emulated clock.
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, count = block_branch_count(variant), interrupt = variant / 4096, arrival = std::array<unsigned, 4>{9, 10, 12, 14}[(variant / 1024) % 4], requested_checkpoint = std::array<unsigned, 8>{3, 8, 9, 10, 11, 12, 15, 24}[(variant % 1024) / 128], checkpoint = requested_checkpoint + unsigned(interrupt && requested_checkpoint == arrival);
		bool const endpoint = (variant / 16) % 2, b = (variant / 32) % 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Forward block branches must use owned branch and loop state");
		if (interrupt && tick == arrival) m_cpu->set_input_line(interrupt == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_vector_acks}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save in-block branch decisions, held words and speculative loop state"); }
		if (tick == 256)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == count + 1 && m_pipeline_store_writes == count + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5001 + count && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5101 + count && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (interrupt ? 0x100 : 0x900) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_pipeline_register_writes == unsigned(bool(interrupt)) && m_fast_stack_reads == unsigned(bool(interrupt)) && m_vector_acks == unsigned(bool(interrupt)), "In-block branches complete the loop once with exact counters, pointers, interrupt frame and status");
			expect(m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == block_branch_source(variant, count) + unsigned(!endpoint) && m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (b ? 0x1234 : 0x2345), "Both branch arms join the endpoint without corrupting the other accumulator");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_vector_acks = m_recurring_irq_snapshot[4]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Replay block branches without duplicate body reads or writes"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 254370; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 12287) { start_block_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-block branches: PASS cases=12288 restore=12288 layouts=diamond,endpoint counts=0,1,2,7 patterns=4 accumulators=2 pages=2 snapshots=8 arrivals=9,10,12,14 sources=none,IRQ,NMI source_reads_each=1 selected_writes_each=1 brc_decode=1 braf_final_decode=1 held_tags_restored=1 interrupt_return=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated in-block branches complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254370) { m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_phase = 254371; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254371) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254360; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_xc_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255700)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 3>{3, 7, 9}[variant / 6];
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save before and after cross-end XC rejection"); }
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 3 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5100 && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x5200, "Cross-end XC stays excluded before counter or protected-slot effects");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore cross-end exclusion without issuing protected words"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255710; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 17) { start_block_xc_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x block XC exclusions: PASS cases=18 restore=18 forms=XC1,XC2 selected_span_crossings=3 pages=2 snapshots=3 data_reads=0 data_writes=0 pointer_effects=0 silicon_trap_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated block XC exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255710) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255700; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255680)
	{
		auto const f = block_xc_case(m_pipeline_peripheral_case); unsigned const tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 4>{4, 11, 13, 24}[f.snapshot];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Conditional block slots remain staged");
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save block counters and selected or rejected XC slots"); }
		if (tick == 120 + f.count * 12)
		{
			u64 const result = u64(s64(s16(f.word(f.count)))) & 0xffffffffffULL;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == f.count + 1 && m_pipeline_store_writes == f.first() && m_pipeline_register_writes == f.second() && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5001 + f.count && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5100 + f.first() && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x5200 + f.second() && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa608 | (f.rotation ? 0x1800 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.b() ? 0x123456789aULL : result) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.b() ? result : 0xfedcba9876ULL), "XC preserves per-iteration selection, counters, status and exact pointer effects");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore XC decisions without duplicate source or store transactions"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255690; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 8703) { start_block_xc(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x block XC: PASS cases=8704 restore=8704 predicates=17 layouts=XC1_single,XC2_two_single,XC2_one_double,XC1_penultimate counts=0,1,2,7 positions=interior,endpoint pages=2 snapshots=4 source_rotations=2 ordered_reads=1 selected_writes=1 rejected_writes=0 opcode_literal=1 counter_braf_owned=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated block XC complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255690) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255680; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_move_repeat_owner_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 299000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = variant & 32 ? 7 : 4;
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save mapped move alongside independent repeat header"); }
		if (tick == 40)
		{
			bool const rejected = variant & 1;
			expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == rejected && m_cpu->state_int(tms320c54x_device::STATE_AR2) == (rejected ? 0x3333 : variant & 4 ? 0x5aa5 : 0x1400), "Repeat guard belongs to its body, not an older mapped move");
			if (!rejected) expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE), "Independent repeat and its continuation finish");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_cpu->set_pipeline_frontend(true); m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore repeat ownership after clearing live pipeline"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 299010; m_check_timer->enable(true); return true; }
			m_cpu->set_pipeline_frontend(false);
			if (variant < 63) { start_move_repeat_owner(variant + 1); return true; }
			osd_printf_info("TMS320C54x mapped move repeat ownership: PASS cases=64 restore=64 before_header=1 repeated_body_rejected=1 headers=short,long pages=2 values=2 counts=0,3 snapshots=2 physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeat ownership complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 299010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 299000; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_branch_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 254380)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = variant < 16 ? 3 : 8;
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes}; m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save before excluded in-block control decoding"); }
		if (tick == 16)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_store_writes, "Unaccepted in-block control remains fail-closed without a control frame or body write");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore excluded branch without atomic fallback"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 254390; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 31) { start_block_branch_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-block branch exclusions: PASS cases=32 restore=32 kinds=8 pages=2 snapshots=2 below_block_targets=1 endpoint_overlap=1 delayed_control=1 conditional_call=1 nested_blocks=1 far_branch=1 data_writes=0 atomic_fallback=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated in-block branch exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254390) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254380; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_delay_dld_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255260)
	{
		bool const shifted = !strcmp(machine().options().bios(), "shift-delay");
		bool const shift_alu = !strcmp(machine().options().bios(), "shift-alu-delay");
		bool const add = !strcmp(machine().options().bios(), "add-delay");
		bool const dld = !strcmp(machine().options().bios(), "dld-delay");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = shift_alu ? delay_shift_alu_case(variant) : dld ? delay_dld_case(variant) : add ? delay_add_case(variant) : delay_load_case(variant, shifted);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (tick > f.read && !f.destination ? f.result : f.a) && m_cpu->state_int(tms320c54x_device::STATE_B) == (tick > f.read && f.destination ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((add || shift_alu ? tick > f.read : tick >= f.read - 1) ? f.final_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5, "Delay arithmetic/load publishes its retained result and owned flags in Execute");
		for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (add || tick < f.read - 1 ? f.initial[i] : tick == f.read - 1 ? f.final[i] : 0x2200 + i), "Delay addressing preserves or updates the selected pointer once");
		if (tick == f.literal_clock()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.caller + 0x601 + f.delay(), 0xdead); }
		if (!add && tick == f.read - 1) for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x2200 + i);
		if (tick == f.read && (!add || f.address >= 0x60)) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); if (dld) m_cpu->space(AS_DATA).write_word(f.address ^ 1, 0xbeef); }
		if (tick == f.checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_pipeline_extensions_reads}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save delayed LD around literal Access and Read ownership"); }
		if (tick == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == (f.kind == 2 ? f.caller + 0x606 : f.target + 0x702) && m_pipeline_operand_reads == (dld ? 2U : 1U) && m_pipeline_store_writes == 1 && m_pipeline_register_writes == (f.kind == 4 ? 2U : unsigned(f.kind == 3)) && m_fast_stack_reads == unsigned(f.kind == 5) && m_pipeline_extensions_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_TRN) == u16(f.result) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.kind == 4 ? 0x2ffe : f.kind == 3 ? 0x2fff : f.kind == 5 ? 0x3001 : 0x3000), "Delayed LD reaches the selected target with correct page frame and observed data");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[4]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore delayed LD without duplicate data literal or frame transactions"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255270; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (shift_alu ? 442367U : add ? 18431U : shifted ? 110591U : 147455U)) { start_delay_load(variant + 1); return true; }
			if (shift_alu) { osd_printf_info("TMS320C54x delayed shifted ALU: PASS cases=442368 restore=442368 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=DP,indirect,postincrement operations=ADD,SUB source_destination_pairs=4 shifts=32 memory_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed shifted ALU complete"); }
			if (dld) { osd_printf_info("TMS320C54x delayed DLD: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=offset,preincrement,circular,absolute registers=8 offsets=-1,2 parity=even,odd destinations=2 patterns=4 sxm_c16=4 compatibility_states=2 pages=2 snapshots=3 ordered_reads=2 literal_fetches=1 pointer_updates=once target_observes_result=1 interrupt_repeat_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed DLD complete"); }
			if (add) { osd_printf_info("TMS320C54x delayed absolute ADD: PASS cases=18432 restore=18432 controls=BD,CALLD,FCALLD,RETD sources=RAM,AR3,AL,AH,BL,BH destinations=2 memory_values=4 initial_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 literal_fetches=1 call_frames=owned target_observes_result=1 conditional_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed absolute ADD complete"); }
			if (shifted) { osd_printf_info("TMS320C54x delayed shifted LD: PASS cases=110592 restore=110592 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=DP,indirect,postincrement destinations=2 shifts=32 memory_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed shifted LD complete"); }
			osd_printf_info("TMS320C54x delayed LD: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=offset,preincrement,circular,absolute registers=8 offsets=2 destinations=2 memory_values=4 sxm_ovm=4 compatibility_states=2 pages=2 snapshots=3 source_reads=1 literal_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated delayed LD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255270) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255260; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255240)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_back_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Internal backward branch retains the active outer block");
		if (tick == f.checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_vector_acks}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save backward block branch predicate held word and interrupt state"); }
		if (tick == 2000)
		{
			unsigned const total = (f.outer + 1) * f.inner;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end + 2 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa002 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Backward inner loop completes the outer count and final BRAF once");
			expect(m_pipeline_operand_reads == total && m_pipeline_store_writes == total && m_pipeline_register_writes == unsigned(bool(f.interrupt)) && m_fast_stack_reads == unsigned(bool(f.interrupt)) && m_vector_acks == unsigned(bool(f.interrupt)) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + total && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + total && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7001 + f.outer && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.b ? 0x123456789aULL : 0xffffffffffULL) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.b ? 0xffffffffffULL : 0xfeabcd9876ULL) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5555, "Backward branches preserve ordered memory work and independent inner/outer counters");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_vector_acks = m_recurring_irq_snapshot[4]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore backward inner loop without duplicate copies or lost outer ownership"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255250; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1535) { start_block_back(variant + 1); return true; }
			osd_printf_info("TMS320C54x backward in-block branches: PASS cases=1536 restore=1536 outer_counts=4 inner_counts=4 forms=conditional_back,unconditional_back accumulators=2 pages=2 sources=none,IRQ,NMI snapshots=4 ordered_copies=1 outer_counts_preserved=1 held_words_restored=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated backward in-block branches complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255250) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255240; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255220)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, guard = std::array<unsigned, 5>{9, 14, 9, 9, 13}[variant % 5], checkpoint = guard - 1 + variant / 10;
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (tick >= guard) && !m_pipeline_operand_reads && !m_pipeline_store_writes, "Unsupported block call forms reject before stack or data work");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 2 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (tick < 4 ? 0x900 : 0x8900) && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000, "Excluded block calls preserve frame and loop ownership");
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save block call exclusion before or after Decode rejection"); }
		if (tick == 20)
		{
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore rejected block call without frame allocation"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255230; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 19) { start_block_far_call_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-block far call exclusions: PASS cases=20 restore=20 forms=delayed_internal,endpoint,internal_target,near_internal,delay_spill pages=2 snapshots=2 data_reads=0 data_writes=0 frame_allocation=0 counters_preserved=1 silicon_trap_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated in-block far call exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255230) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255220; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255200)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = block_far_call_case(variant, !strcmp(machine().options().bios(), "rptb-fcalld"));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Far call preserves its enclosing block while executing outside it");
		if (tick == f.checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_vector_acks, m_pipeline_extensions_reads}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save active block across far call stack callee or interrupt state"); }
		if (tick == 1000)
		{
			unsigned const iterations = f.count + 1, stack = iterations * 2 * f.depth + unsigned(bool(f.interrupt));
			if (m_pipeline_operand_reads != iterations || m_pipeline_store_writes != iterations || m_pipeline_register_writes != stack || m_fast_stack_reads != stack || m_vector_acks != unsigned(bool(f.interrupt)) || m_cpu->state_int(tms320c54x_device::STATE_AR6) != 0x7206 + (f.depth == 2 ? iterations : 0))
				osd_printf_info("block_far_call_effects: case=%u replay=%u read=%u write=%u stack_write=%u stack_read=%u expected_stack=%u ack=%u ar2=%llx ar3=%llx ar4=%llx ar5=%llx ar6=%llx t=%llx\n", variant, unsigned(m_pipeline_replay), m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, stack, m_vector_acks, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR2), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR3), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR4), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR5), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR6), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_T));
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.caller + 0x60b && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x609 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608, "Far call returns to the caller page and completes the original block without count or BRAF corruption");
			expect(m_pipeline_operand_reads == iterations && m_pipeline_store_writes == iterations && m_pipeline_register_writes == stack && m_fast_stack_reads == stack && m_vector_acks == unsigned(bool(f.interrupt)) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7100 + (!f.delayed || !f.delay_form ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x7206 + (f.depth == 2 ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x7207 + (f.delayed && !f.delay_form ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.delayed && f.delay_form == 2 ? 1 : 0x123456789aULL) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == (f.delayed && f.delay_form == 1 ? 0x1234 : 0x5555) && m_pipeline_extensions_reads == (f.delayed && f.delay_form == 1 ? iterations : 0), "Nested far calls, delay words and optional interrupt balance every stack and data transaction once");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_vector_acks = m_recurring_irq_snapshot[4]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[5]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore far callee and caller loop with pending interrupt and retained stack"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255210; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.delayed ? 2303 : 767)) { start_block_far_call(variant + 1); return true; }
			if (f.delayed) { osd_printf_info("TMS320C54x in-block delayed far calls: PASS cases=2304 restore=2304 counts=0,1,2,7 positions=middle,endpoint delay_layouts=two_single,one_double,original_LD_NOP caller_pages=2 callee_pages=same,cross depths=1,2 sources=none,IRQ,NMI snapshots=4 ordered_copy=1 balanced_stack=1 return_pc=call+4 caller_loop_preserved=1 delay_effects=once physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated in-block delayed far calls complete"); }
			osd_printf_info("TMS320C54x in-block far calls: PASS cases=768 restore=768 counts=0,1,2,7 layouts=early,near_end caller_pages=2 callee_pages=same,cross depths=1,2 sources=none,IRQ,NMI snapshots=4 ordered_copy=1 balanced_stack=1 caller_loop_preserved=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated in-block far calls complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255210) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255200; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255180)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, guard = variant % 4 ? 8 : 7, checkpoint = guard - 1 + variant / 8;
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (tick >= guard) && !m_pipeline_operand_reads && !m_pipeline_store_writes, "Unsupported inner repeat forms reject before any data transactions");
		expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == 2 && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (tick < 4 ? 0x800 : 0x8800) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5555 && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000, "Excluded repeat setup preserves counters and body state");
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save unsupported inner repeat before or after rejection"); }
		if (tick == 16)
		{
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore inner repeat exclusion without changing outer ownership"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255190; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 15) { start_block_single_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x inner repeat exclusions: PASS cases=16 restore=16 forms=header_at_end,long_immediate,memory_count,nested_block pages=2 snapshots=2 data_reads=0 data_writes=0 counters_preserved=1 silicon_trap_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated inner repeat exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255190) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255180; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255160)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_single_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Single repeat inside a block uses independent RPTC and BRC ownership");
		if (tick == f.checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save inner repeat with active outer block ownership"); }
		if (tick == 600)
		{
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(STATE_GENPC) != f.base + f.end + 4 || m_cpu->state_int(tms320c54x_device::STATE_BRC) || m_cpu->state_int(tms320c54x_device::STATE_ST1) != (f.memory && f.count_form == 1 ? 0x4800 : 0x800) || m_cpu->state_int(tms320c54x_device::STATE_T) != 0x1a2b)
				osd_printf_info("block_single_state: case=%u pc=%llx end=%04x idle=%llu brc=%llu rc=%llu st1=%llx t=%llx reads=%u writes=%u\n", variant, (unsigned long long)m_cpu->state_int(STATE_GENPC), f.end, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_BRC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_RC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_T), m_pipeline_operand_reads, m_pipeline_store_writes);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end + 4 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.memory && f.count_form == 1 ? 0x4800 : 0x800) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x8820 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1a2b, "Inner repeat preserves outer completion count BRAF and fallthrough");
			if (f.memory) expect(m_pipeline_extensions_reads == f.outer + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1060 + (f.count_form == 3 ? f.outer + 1 : 0), "Memory repeat retains one count per outer iteration and updates its pointer once");
			expect(m_pipeline_operand_reads == (f.kind == 0 ? f.total : 0) && m_pipeline_store_writes == (f.kind < 2 ? f.total : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + (f.kind == 0 ? f.total : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + (f.kind < 2 ? f.total : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000 + f.outer + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7100 + (f.tail ? 0 : f.outer + 1) && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL, "Inner repeat composes ordered memory effects with one prefix and suffix per outer iteration");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore inner and outer repeat without lost or duplicate bus work"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255170; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.memory ? 3839 : 767)) { start_block_single(variant + 1); return true; }
			if (f.memory) { osd_printf_info("TMS320C54x memory repeat in block: PASS cases=3840 restore=3840 forms=DP,SP,indirect,postincrement,absolute outer_counts=4 inner_counts=4 bodies=MVDD,ST,NOP positions=middle,end pages=2 snapshots=4 fresh_counts=1 consumed_counts_poisoned=1 outer_counts_preserved=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated memory repeat in block complete"); }
			osd_printf_info("TMS320C54x single repeat in block: PASS cases=768 restore=768 outer_counts=4 inner_counts=4 bodies=MVDD,ST,NOP positions=middle,end pages=2 snapshots=4 ordered_memory=1 outer_counts_preserved=1 duplicate_transactions=0 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated single repeat in block complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255170) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255160; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_delayed_condition_return_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 276000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delayed_condition_return_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RCD signed accumulator and TC predicates are admitted outside block and delay contexts");
		if (tick == f.checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_pipeline_drained_fetches}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save RCD predicate stack sample and retained delay words"); }
		if (tick == 96)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + (f.taken ? 0x706 : 0x609) && m_cpu->state_int(tms320c54x_device::STATE_SP) == f.sp + unsigned(f.taken) && m_cpu->state_int(tms320c54x_device::STATE_A) == f.final_a && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.taken ? 0x55 : 0x33) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.final_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == (f.layout ? 0xf4e1 : 0x1234), "RCD retains its predicate and target while both delay words retire and only taken paths pop");
			expect(m_pipeline_operand_reads == unsigned(f.taken) && m_pipeline_store_writes == 2 && m_pipeline_extensions_reads == 3 && m_pipeline_drained_fetches == 1, "RCD has no untaken stack access no duplicate delay fetch and exactly one continuation");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_pipeline_drained_fetches = m_recurring_irq_snapshot[3]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore RCD without repeated stack or delay transactions"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 276010; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 4479) { start_delayed_condition_return(variant + 1); return true; }
			osd_printf_info("TMS320C54x conditional delayed returns: PASS cases=4480 restore=4480 predicates=14 patterns=8 delay_layouts=2 stack_alignments=2 pages=2 snapshots=5 predicate_before_delay=1 taken_stack_reads=1 untaken_stack_reads=0 retained_target=1 delay_fetches=once interrupt_block_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated conditional delayed returns complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 276010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 276000; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_near_call_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62152)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 4>{5, 9, 12, 16}[variant / 18];
		expect(!m_pipeline_operand_reads && !m_pipeline_store_writes && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Unsupported block near calls allocate no frame or data transaction");
		if (tick == checkpoint && !m_pipeline_replay) { m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save near call rejection before or after Decode"); }
		if (tick == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Internal targets and incomplete call spans remain excluded");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore near call exclusion without side effects"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62153; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 71) { start_block_near_call_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-block near call exclusions: PASS cases=72 restore=72 forms=internal_target,delay_spill,literal_spill pages=3 counts=2 snapshots=4 reads=0 writes=0 stack_allocation=0 silicon_trap_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated in-block near call exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62153) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62152; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62150)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_near_call_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Near call preserves its caller block outside the loop address range");
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_vector_acks}; expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save block near call stack callee and return state");
		}
		if (tick == 1800)
		{
			unsigned const iterations = f.count + 1, stack = iterations * f.depth + unsigned(bool(f.interrupt));
			if (m_pipeline_operand_reads != iterations || m_pipeline_store_writes != iterations || m_pipeline_register_writes != stack || m_fast_stack_reads != stack || m_cpu->state_int(STATE_GENPC) != f.base + f.end + 11)
				osd_printf_info("block_near_call_effects: case=%u replay=%u pc=%llx reads=%u writes=%u stack_writes=%u stack_reads=%u expected_stack=%u brc=%llx st1=%llx ar4=%llx ar5=%llx ar6=%llx ar7=%llx\n", variant, unsigned(m_pipeline_replay), (unsigned long long)m_cpu->state_int(STATE_GENPC), m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, stack, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_BRC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR4), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR5), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR6), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR7));
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end + 11 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x6d01, "Near return completes the caller block and its single post-loop sentinel");
			expect(m_pipeline_operand_reads == iterations && m_pipeline_store_writes == iterations && m_pipeline_register_writes == stack && m_fast_stack_reads == stack && m_vector_acks == unsigned(bool(f.interrupt)), "Block near calls balance every data stack and interrupt transaction");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000 + (f.layout != 2 ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7100 + (f.delayed || f.layout == 0 ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x7200 + (f.depth == 2 ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x7300 + (f.delayed_return ? f.depth * iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5555, "Near call and return delays run once per caller iteration without register corruption");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_vector_acks = m_recurring_irq_snapshot[4]; m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore block near call without duplicated memory or frames"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62151; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 3455) { start_block_near_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-block near calls: PASS cases=3456 restore=3456 counts=0,1,5,15 layouts=middle,endpoint,whole_block pages=3 depths=1,2 sources=none,IRQ,NMI calls=CALL,CALLD returns=RET,RETD snapshots=4 ordered_copy=1 balanced_stack=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated in-block near calls complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62151) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62150; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_signed_xc_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 254880)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = signed_xc_case(variant);
		unsigned const checkpoint = std::array<unsigned, 4>{3, 4, 5, 8}[variant / 576];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa60e && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5, "Signed XC preserves status and T");
		if (tick == 3 || tick == 4) m_cpu->set_state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, tick == 3 ? f.value : f.opposite);
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_store_writes, m_pipeline_extensions_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear(); expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save signed XC before/after predicate and protected-slot decisions");
		}
		if (tick == 16)
		{
			unsigned const writes = unsigned(f.taken) + unsigned(f.taken || f.layout != 1);
			u16 const opcode = 0xfd42 + f.condition + (f.b ? 8 : 0) + (f.layout ? 0x200 : 0);
			expect_opcode(opcode, m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_store_writes == writes && m_pipeline_extensions_reads == 3 && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.b ? 0x1234 : f.opposite) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.b ? f.opposite : 0x3456), "Signed XC uses a retained 40-bit comparison, legal slot layout and no duplicate effects");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore signed XC without resampling predicate or decoding rejected literal");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 254890; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 2303) { start_signed_xc(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged signed XC: PASS cases=2304 restore=2304 conditions=GE,LT,NE,EQ,GT,LE accumulators=A,B patterns=8 layouts=XC1_single,XC2_two_single,XC2_one_double pages=2 snapshots=4 predicate=Access rejected_writes=0 opcode_literal=1 signed_irq_composition=unvalidated\n");
			throw emu_fatalerror(0, "TMS320C54x isolated signed XC complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254890) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254880; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_exit_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62064)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = block_exit_case(variant, !strcmp(machine().options().bios(), "rptb-exit-delay"));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Forward block exits and spaced outside BRAF clear are admitted");
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_pipeline_extensions_reads, m_fast_stack_reads}; m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save block exit with held words, pending endpoint predictions and software-clear state");
		}
		if (tick == 256)
		{
			if (f.delayed) expect(m_fast_stack_reads == f.reads && m_pipeline_extensions_reads == f.reads &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x5300 + f.reads && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x5400 + f.reads,
				"Every taken or untaken delayed exit owns exactly two delay-word effects");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == f.reads && m_pipeline_store_writes == f.writes && m_pipeline_register_writes == unsigned(f.taken) &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == (f.taken ? f.count - f.exit : 0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 &&
				m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + f.reads && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5100 + f.writes &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Exit, explicit BRAF clear and spaced old-endpoint revisit preserve loop registers and exact body effects");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2];
				m_pipeline_extensions_reads = m_recurring_irq_snapshot[3]; m_fast_stack_reads = m_recurring_irq_snapshot[4];
				m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore block exit without lost held tags or duplicated outside publication");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62065; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.delayed ? 9983U : 1151U)) { start_block_exit(variant + 1); return true; }
			if (f.delayed)
			{
				osd_printf_info("TMS320C54x delayed forward block exits: PASS cases=9984 restore=9984 branches=BD,BCD_signed12 counts=0,1,2,7 exits=first,middle,last,never endpoint_layouts=2 delay_layouts=2 pages=2 snapshots=6 delay_words_once=1 decision_before_opposite_load=1 braf_not_auto_cleared=1 outside_rsbx=1 spaced_endpoint_revisit=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated delayed forward block exits complete");
			}
			osd_printf_info("TMS320C54x forward block exits: PASS cases=1152 restore=1152 branches=B,BC_AEQ,BC_BEQ counts=0,1,2,7 exits=first,middle,last,never endpoint_layouts=2 pages=2 snapshots=6 source_reads=ordered body_writes=selected braf_not_auto_cleared=1 outside_rsbx=1 spaced_endpoint_revisit=1 interrupt_composition=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated forward block exits complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62065) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62064; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_xc_barrier_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 524000 && m_phase < 524024)
	{
		unsigned const index = m_phase - 524000;
		bool const initial = (index / 3) % 2, taken = index % 3 != 2;
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x602)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save return barrier after its optional stack read");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Return barrier and XC remain admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		if (m_cpu->state_int(tms320c54x_device::STATE_B) != 0x33)
			osd_printf_info("XC return barrier divergence: case=%u staged=%u kind=%u initial_tc=%u B=%04x expected_B=0033\n", index, index < 12, index % 3, initial,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_B)));
		expect(m_rom4_codec_replays == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x33 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x606 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 + unsigned(taken) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xe05 | (!initial ? 0x1000 : 0)),
			"Return pipeline drain makes preceding status visible even at a sequential target or untaken conditional return");
		if (!m_rom4_codec_replay)
		{
			m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
			expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore return drain and selected XC without reusing stale history");
			m_phase = 524000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index == 11) osd_printf_info("TMS320C54x staged XC return barriers: PASS cases=12 restore=12 forms=RET,RC_taken,RC_untaken sequential_target=1 snapshots=2 physical_phase_claim=0\n");
		if (index < 23) { start_xc_barrier(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic XC return barriers: PASS cases=12 restore=12 forms=RET,RC_taken,RC_untaken sequential_target=1 snapshots=2 physical_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated XC return barriers complete");
	}
	if (m_phase >= 523000 && m_phase < 523768)
	{
		unsigned const index = m_phase - 523000;
		auto const f = xc_latency_case(index);
		if (!m_rom4_codec_replays && (f.staged ? m_cpu->pipeline_cycles() == 5 + f.prefix : m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x600 + f.prefix))
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save XC after its producer without replacing the retained Access condition");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "XC producer and single predicate remain admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		u64 const a = f.kind >= 2 && !(f.kind & 1) ? f.source() : f.take() ? 0x33 : 0;
		u64 const b = f.kind >= 2 && (f.kind & 1) ? f.source() : f.kind >= 2 && f.take() ? 0x33 : 0;
		if (m_cpu->state_int(tms320c54x_device::STATE_A) != a || m_cpu->state_int(tms320c54x_device::STATE_B) != b || m_cpu->state_int(tms320c54x_device::STATE_ST0) != f.status(true))
			osd_printf_info("XC condition divergence: case=%u staged=%u kind=%u gap=%u slots=%u initial=%u polarity=%u A=%04x expectedA=%04x B=%04x expectedB=%04x ST0=%04x expectedST0=%04x\n",
				index, f.staged, f.kind, f.gap, f.slots, f.initial, f.polarity, unsigned(m_cpu->state_int(tms320c54x_device::STATE_A)), unsigned(a),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_B)), unsigned(b), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), f.status(true));
		expect(m_rom4_codec_replays == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == b &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.status(true) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x603 + f.prefix + f.gap + f.slots && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
			"XC retains old or settled TC/C/A/B once for all controlled words across one/two-word producers");
		if (!m_rom4_codec_replay)
		{
			m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
			expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore XC condition history and selected/annulled words");
			m_phase = 523000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index == 383) osd_printf_info("TMS320C54x staged XC conditions: PASS cases=384 restore=384 sources=TC,C,A,B producer_words=1,2 gaps=0,1,2,3 controlled_words=1,2 snapshots=2 physical_phase_claim=0\n");
		if (index < 767) { start_xc_latency(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic XC conditions: PASS cases=384 restore=384 sources=TC,C,A,B producer_words=1,2 gaps=0,1,2,3 controlled_words=1,2 snapshots=2 physical_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated XC conditions complete");
	}
	if (m_phase >= 522000 && m_phase < 522128)
	{
		unsigned const index = m_phase - 522000, mode = index % 32, gap = mode % 4;
		bool const staged = index < 64, set = (mode / 4) & 1, initial = (mode / 8) & 1, tc = (mode / 16) & 1;
		if (!m_rom4_codec_replays && (staged ? m_cpu->pipeline_cycles() == 6 : m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x601))
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save XC/status after publication with an older retained predicate");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "XC and status producers remain admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		bool const take = (gap < 2 ? initial : set) == tc;
		if (m_cpu->state_int(tms320c54x_device::STATE_A) != (take ? 0x33 : 0))
			osd_printf_info("XC/status divergence: staged=%u case=%u gap=%u initial_tc=%u producer_tc=%u predicate_tc=%u actual_A=%04x expected_A=%04x\n", staged, index, gap, initial, set, tc,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_A)), take ? 0x33 : 0);
		expect(m_rom4_codec_replays == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == (take ? 0x33 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0 && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x605 + gap &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xe05 | (set ? 0x1000 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
			"XC tests its Access-stage status rather than either preceding Execute publication");
		if (!m_rom4_codec_replay)
		{
			m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
			expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore XC/status with the original retained predicate");
			m_phase = 522000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index == 63) osd_printf_info("TMS320C54x staged XC/status: PASS cases=64 restore=64 producers=SSBX,RSBX gaps=0,1,2,3 old_condition_words=2 snapshots=2 physical_phase_claim=0\n");
		if (index < 127) { start_xc_status(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic XC/status: PASS cases=64 restore=64 producers=SSBX,RSBX gaps=0,1,2,3 old_condition_words=2 snapshots=2 physical_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated XC/status complete");
	}
	return false;
}

bool tms320c54x_test_state::check_conditional_register_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 519000 && m_phase < 519144)
	{
		unsigned const index = m_phase - 519000;
		auto const f = conditional_register_case(index % 72);
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == (f.take ? 0x700 : 0x601))
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save atomic return after its optional stack read and PC publication");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "All twelve atomic signed accumulator return predicates are admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		expect(m_rom4_codec_replays == 1 && m_pipeline_operand_reads == unsigned(f.take) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (f.take ? 0x22 : 0x11) && m_cpu->state_int(tms320c54x_device::STATE_B) == f.b &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 + unsigned(f.take) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (f.take ? 0x702 : 0x603) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1e05 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
			"Atomic conditional return tests the full signed accumulator and preserves status and exact stack ownership");
		if (!m_rom4_codec_replay)
		{
			m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
			expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore atomic conditional return without duplicating its stack read");
			m_pipeline_operand_reads = m_recurring_irq_snapshot[0];
			m_phase = 519000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 143) { start_atomic_conditional_return(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic conditional returns: PASS cases=144 restore=144 predicates=12 patterns=6 snapshots=2 taken_reads=1 untaken_reads=0 stack_writes=0 silicon_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated atomic conditional returns complete");
	}
	if (m_phase >= 500000 && m_phase < 518432)
	{
		unsigned const index = m_phase - 500000;
		auto const f = conditional_register_case(index);
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x601)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save conditional register store after its read/write and address update");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Conditional register store remains admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		auto const disable = machine().disable_side_effects();
		expect(m_rom4_codec_replays == 1 && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 &&
			data.read_word(f.address) == (f.take ? f.source : 0xa55a) && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x602 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == f.a && m_cpu->state_int(tms320c54x_device::STATE_B) == f.b &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (f.brc ? 0xbeef : f.source) &&
			m_cpu->state_int(tms320c54x_device::STATE_BRC) == (f.brc ? f.source : 0x55aa) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1e05 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
			"Conditional register store preserves source, predicates and status while completing one bus pair");
		for (unsigned ar = 2; ar < 6; ++ar)
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == 2 + (f.x & 3) ? f.next : 0x1403 + (ar - 2) * 16), "Conditional register store updates only its selected Xmem address, even when false");
		if (!m_rom4_codec_replay)
		{
			m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
			expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore conditional register store without duplicating a completed bus transaction");
			m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
			m_phase = 500000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 18431) { start_conditional_register_store(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic conditional register stores: PASS cases=18432 restore=18432 forms=STRCD,SRCCD predicates=12 patterns=6 xmem=16 source_values=4 snapshots=2 false_read_writeback=1 circular_update=1 silicon_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated conditional register stores complete");
	}
	if (m_phase >= 305000 && m_phase < 305064)
	{
		unsigned const index = m_phase - 305000, flags = (index >> 3) & 3, accumulator = (index >> 1) & 1;
		u16 const mask = 1U << (10 - accumulator);
		bool const take = BIT(flags, accumulator) == BIT(index, 2), delayed = index & 1;
		u16 const initial = 0x1805 | ((flags & 1) << 10) | ((flags & 2) << 8);
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == (delayed || !take ? 0x602 : 0x700))
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (initial & ~mask), "Overflow branch consumes only its selected flag before any delay word");
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save consumed overflow predicate and optional delayed transfer");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Atomic single-overflow branch is admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
		{
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		bool const slots = delayed || !take;
		expect(m_rom4_codec_replays == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == (take ? 0x22 : 0x11) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (slots ? 0x55 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (take ? 0x702 : 0x606) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((initial & ~mask) | (slots ? mask : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
			"Overflow branch retains its original condition across flag-changing delay words");
		if (!m_rom4_codec_replay)
		{
			m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
			expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore overflow predicate or pending delayed transfer");
			m_phase = 305000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 63) { start_atomic_overflow_branch(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic overflow branches: PASS cases=64 restore=64 forms=BC,BCD predicates=AOV,ANOV,BOV,BNOV flags=4 snapshots=2 selected_flag_clear=1 delay_condition_retained=1 silicon_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated overflow branches complete");
	}
	return false;
}

} // namespace c54x_test
