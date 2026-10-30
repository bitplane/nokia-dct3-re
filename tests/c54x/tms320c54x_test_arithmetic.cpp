// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_memory_immediate_schedule.h"
#include "tms320c54x_test_rmw_schedule.h"
#include "tms320c54x_test_store_schedule.h"
#include "tms320c54x_test_shifted_schedule.h"
#include "tms320c54x_test_register_schedule.h"

namespace c54x_test {

void tms320c54x_test_state::start_register_unary(register_unary_kind kind, unsigned phase)
{
	m_register_sequence = std::make_unique<register_sequence_fixture>(machine(), *m_cpu, *m_check_timer,
		register_unary_count(kind), [kind](unsigned key) { return register_unary_case<cpu_regs>(key, kind); });
	if (kind == register_unary_kind::shift)
		m_register_sequence->set_opcode_observer([this](u16 word, u16 expected) {
			expect_opcode(word, word == expected, "Register shift fetch matches its checked sequence");
		});
	m_phase = phase; m_register_sequence->start();
}

void tms320c54x_test_state::report_register_unary(register_unary_kind kind) const
{
	osd_printf_info("%s\n", register_unary_result(kind, m_register_sequence->count(),
		m_register_sequence->restores(), m_register_sequence->digest()).c_str());
}

tms320c54x_test_state::rounded_multiply_case const & tms320c54x_test_state::rounded_multiply_vector(unsigned index)
{
	// Explicit results from SPRU172C MPYR/MACR/MASR arithmetic, not
	// a second implementation of the core's rounding helper.
	static constexpr rounded_multiply_case cases[] = {
		{0x2200, 0x8000, 0x8000, 0x0040, 0, 0x0080000000ULL, true},
		{0x2200, 0x8000, 0x8000, 0x0240, 0, 0x007fffffffULL, true},
		{0x2a00, 1, 1, 0, 0x007fff7ffeULL, 0x007fff0000ULL, false},
		{0x2a00, 1, 1, 0, 0x007fff7fffULL, 0x0080000000ULL, true},
		{0x2a00, 1, 1, 0x0200, 0x007fff7fffULL, 0x007fffffffULL, true},
		{0x2e00, 0x0100, 0x0100, 0, 0xff80000000ULL, 0xff7fff0000ULL, true},
		{0x2e00, 0x0100, 0x0100, 0x0200, 0xff80000000ULL, 0xff80000000ULL, true},
		{0x2e00, 1, 1, 0, 0xff80008000ULL, 0xff80000000ULL, false},
		// SPRU131G SMUL: clamp the fractional product before MAC/MAS,
		// then apply the instruction's rounding and destination saturation.
		{0x2a00, 0x8000, 0x8000, 0x0240, 0xffffff8000ULL, 0x007fffffffULL, true, 0},
		{0x2a00, 0x8000, 0x8000, 0x0240, 0xffffff8000ULL, 0x007fff0000ULL, false, 2},
		{0x2e00, 0x8000, 0x8000, 0x0240, 0x0000007fffULL, 0xff80000000ULL, false, 0},
		{0x2e00, 0x8000, 0x8000, 0x0240, 0x0000007fffULL, 0xff80010000ULL, false, 2},
		// SMUL is inert when either OVM or FRCT is clear.
		{0x2a00, 0x8000, 0x8000, 0x0040, 0xffffff8000ULL, 0x0080000000ULL, true, 2},
		{0x2a00, 0x8000, 0x8000, 0x0200, 0, 0x0040000000ULL, false, 2},
		// SMUL controls MAC/MAS pre-accumulation, not MPYR rounding.
		{0x2200, 0x8000, 0x8000, 0x0240, 0, 0x007fffffffULL, true, 2},
		{0x2200, 0x8000, 0x8000, 0x0040, 0, 0x0080000000ULL, true, 2},
		{0x2200, 0x8000, 0x8000, 0x0200, 0, 0x0040000000ULL, false, 2},
		{0x2200, 0x8000, 0x8000, 0, 0, 0x0040000000ULL, false, 2},
		{0x2200, 1, 0x8000, 0, 0, 0, false, 2},
		{0x2200, 1, 0x8000, 0x0040, 0, 0xffffff0000ULL, false, 2},
		{0x2200, 1, 0x7fff, 0, 0, 0, false, 2},
		{0x2200, 1, 0x4000, 0x0040, 0, 0x0000010000ULL, false, 2},
		// Rounded accumulation crosses the 40-bit guard boundary; OVM
		// saturates the mathematical result before accumulator truncation.
		{0x2a00, 1, 1, 0, 0x7fffffffffULL, 0x8000000000ULL, true},
		{0x2a00, 1, 1, 0x0200, 0x7fffffffffULL, 0x007fffffffULL, true},
		{0x2e00, 0x0100, 0x0100, 0, 0x8000000000ULL, 0x7fffff0000ULL, true},
		{0x2e00, 0x0100, 0x0100, 0x0200, 0x8000000000ULL, 0xff80000000ULL, true}
	};
	return cases[index];
}

void tms320c54x_test_state::start_rounded_multiply_case(unsigned index)
{
	auto const &test = rounded_multiply_vector(index / 8);
	bool const destination_b = BIT(index, 0);
	bool const absolute = BIT(index, 1);
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x010980, test.opcode | (destination_b ? 0x0100 : 0) | (absolute ? 0xf8 : 0x82));
	program.write_word(0x010981, absolute ? 0x0500 : 0xf4e1);
	program.write_word(0x010982, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0500, test.memory);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1800 | (destination_b ? 0x0400 : 0x0200) |
		(BIT(index, 2) ? (destination_b ? 0x0200 : 0x0400) : 0));
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ST1, 0x0800 | test.status}, {cpu_regs::STATE_PMST, test.pmst},
		{cpu_regs::STATE_A, destination_b ? 0x12345678 : test.initial},
		{cpu_regs::STATE_B, destination_b ? test.initial : 0x12345678}, {cpu_regs::STATE_T, test.t},
		{cpu_regs::STATE_AR2, 0x0500}, {STATE_GENPC, 0x010980}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 260000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

bool tms320c54x_test_state::check_rounded_multiply()
{
	if (m_phase < 260000 || m_phase >= 260208)
		return false;
	unsigned const index = m_phase - 260000;
	auto const &test = rounded_multiply_vector(index / 8);
	bool const destination_b = BIT(index, 0);
	u16 const opcode = test.opcode | (destination_b ? 0x0100 : 0) | (BIT(index, 1) ? 0xf8 : 0x82);
	u16 const expected_status = 0x1800 | (destination_b ? 0x0400 : 0x0200) |
		((test.overflow || BIT(index, 2)) ? (destination_b ? 0x0200 : 0x0400) : 0);
	expect_opcode(opcode,
		m_cpu->state_int(destination_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == test.expected &&
		m_cpu->state_int(destination_b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x12345678 &&
		m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected_status &&
		m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x0800 | test.status) &&
		m_cpu->state_int(tms320c54x_device::STATE_PMST) == test.pmst &&
		m_cpu->state_int(tms320c54x_device::STATE_T) == test.t &&
		m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0500 &&
		m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
		!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
		"SPRU172C rounded multiplier signed boundaries preserve opposite accumulator, carry, TC, T and address");
	if (index < 207) { start_rounded_multiply_case(index + 1); return true; }
	osd_printf_info("TMS320C54x rounded multiply boundaries: PASS vectors=26 destinations=2 addressing_modes=2 sticky_overflow_states=2\n");
	start_idle_nmi_case(0);
	return true;
}

void tms320c54x_test_state::start_smul_control_case(unsigned index)
{
	unsigned const family = index / 64, kind = (index / 16) % 4, controls = (index / 2) & 7;
	bool const destination_b = index & 1;
	u64 const seed = kind < 2 ? 0xffffffffffULL : 1;
	u16 const status = 0x1800 | (destination_b ? 0x0400 : 0x0200);
	u16 op;
	if (family == 0)
		op = 0x2893 | (kind << 9) | (destination_b ? 0x100 : 0);
	else
		op = (family == 1 ? 0xb089 : 0xd089) | ((kind & 1) << 10) |
			((kind >> 1) << 11) | (destination_b ? 0x100 : 0) |
			((family == 1 ? destination_b : !destination_b) ? 0x200 : 0);
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, op);
	program.write_word(0x0801, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0600, 0x8000);
	m_cpu->space(AS_DATA).write_word(0x0610, 0x8000);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x0600}, {cpu_regs::STATE_AR3, family == 0 ? 0x0600 : 0x0610},
		{cpu_regs::STATE_T, 0x8000}, {cpu_regs::STATE_A, destination_b ? 0x12340000 : seed},
		{cpu_regs::STATE_B, destination_b ? seed : 0x12340000}, {cpu_regs::STATE_ST0, status},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
		0x0900 | (controls & 1 ? 0x40 : 0) | (controls & 2 ? 0x200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, controls & 4 ? 2 : 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 290000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_store_shift_case(unsigned index)
{
	auto const &vector = STORE_SHIFTS[index / 40];
	unsigned const form = (index / 8) % 5;
	bool const source_b = index & 1, sxm = index & 2, sst = index & 4;
	auto &program = m_cpu->space(AS_PROGRAM);
	if (form < 2)
	{
		program.write_word(0x0800, 0x8493 | (form << 9) | (source_b ? 0x100 : 0));
		program.write_word(0x0801, 0xf4e1);
	}
	else if (form < 4)
	{
		program.write_word(0x0800, 0x6f93);
		program.write_word(0x0801, (form == 2 ? 0x0c80 : 0x0c60) |
			(source_b ? 0x100 : 0) | (vector.shift & 31));
		program.write_word(0x0802, 0xf4e1);
	}
	else
	{
		program.write_word(0x0800, 0xe489 | (source_b ? 0x200 : 0)); // ST src,*AR3+ || LD *AR2+,T.
		program.write_word(0x0801, 0xf4e1);
	}
	m_cpu->space(AS_DATA).write_word(0x0600, 0xa55a);
	m_cpu->space(AS_DATA).write_word(0x0610, 0x1234);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x0610}, {cpu_regs::STATE_AR3, 0x0600}, {cpu_regs::STATE_T, 0},
		{cpu_regs::STATE_A, source_b ? 1 : vector.value}, {cpu_regs::STATE_B, source_b ? vector.value : 1},
		{cpu_regs::STATE_ST0, 0x1c00},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
		0x0800 | (sxm ? 0x0100 : 0) | (vector.shift & 31));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, sst ? 1 : 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 280000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_saccd_boundary_case(unsigned index)
{
	auto const &vector = SACCD_BOUNDARIES[index / 16];
	bool const source_b = index & 1, sxm = index & 2, sst = index & 4, take = !(index & 8);
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, 0x9e80 | (source_b ? 0x0100 : 0) |
		(source_b ? 0 : 8) | (take ? 6 : 3)); // *AR2+, test the other accumulator.
	program.write_word(0x0801, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0600, 0xa55a);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x0600}, {cpu_regs::STATE_A, source_b ? 1 : vector.value},
		{cpu_regs::STATE_B, source_b ? vector.value : 1}, {cpu_regs::STATE_ST0, 0x1c00},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
		0x0800 | (sxm ? 0x0100 : 0) | (vector.shift & 31));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, sst ? 1 : 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 270000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_firs_sum_case(unsigned index)
{
	static constexpr u16 x[] = {0xffff, 0xffff, 0x7fff, 0x8000};
	static constexpr u16 y[] = {1, 0xffff, 1, 0x8000};
	unsigned const pair = index % 4;
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, 0xe001); // FIRS *AR2,*AR3,0900.
	program.write_word(0x0801, 0x0900);
	program.write_word(0x0802, 0xf4e1);
	program.write_word(0x0900, 2);
	m_cpu->space(AS_DATA).write_word(0x0600, x[pair]);
	m_cpu->space(AS_DATA).write_word(0x0601, y[pair]);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x0600}, {cpu_regs::STATE_AR3, 0x0601}, {cpu_regs::STATE_A, 0x10000},
		{cpu_regs::STATE_B, 3}, {cpu_regs::STATE_T, 0x1234},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0,
		0x1200 | (pair == 2 ? 0x0800 : 0) | (index >= 16 ? 0x0400 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
		(index & 4 ? 0x0100 : 0) | (index & 8 ? 0x0200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 230000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_poly_boundary_case(unsigned index)
{
	static constexpr u64 before[] = {0x7fff7fff, 0x7fff8000, 0xffffffffffULL, 0xffffff7fffULL};
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, 0x3682); // POLY *AR2.
	program.write_word(0x0801, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0600, 0xffff);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0600);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x10000);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, before[index % 4]);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1a00 | (index & 32 ? 0x0400 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
		(index & 4 ? 0x0040 : 0) | (index & 8 ? 0x0200 : 0) | (index & 16 ? 0x0100 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 250000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_abdst_overflow_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, 0xe301); // ABDST *AR2,*AR3.
	program.write_word(0x0801, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0600, 0);
	m_cpu->space(AS_DATA).write_word(0x0601, 0);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x0600}, {cpu_regs::STATE_AR3, 0x0601}, {cpu_regs::STATE_A, 0xff00000000ULL},
		{cpu_regs::STATE_B, index & 4 ? 0xffffffffffULL : 0x7fffffff}, {cpu_regs::STATE_T, 0x1234},
		{cpu_regs::STATE_ST0, 0x1400 | (index & 8 ? 0x0200 : 0)},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
		0x0100 | (index & 1 ? 0x0040 : 0) | (index & 2 ? 0x0200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 240000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_sqdst_difference_case(unsigned index)
{
	static constexpr u16 x[] = {0xffff, 0, 0x8000, 0x7fff};
	static constexpr u16 y[] = {1, 0xffff, 0x7fff, 0x8000};
	unsigned const pair = index % 4;
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, 0xe201);
	program.write_word(0x0801, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0600, x[pair]);
	m_cpu->space(AS_DATA).write_word(0x0601, y[pair]);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x0600}, {cpu_regs::STATE_AR3, 0x0601}, {cpu_regs::STATE_A, 0x10000},
		{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0x1234},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0,
		0x1200 | (x[pair] < y[pair] ? 0x0800 : 0) | (index >= 16 ? 0x0400 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,
		(index & 4 ? 0x0100 : 0) | (index & 8 ? 0x0200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 220000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_sqdst_overflow_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, 0xe201); // SQDST *AR2,*AR3.
	program.write_word(0x0801, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0600, 0);
	m_cpu->space(AS_DATA).write_word(0x0601, 0);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x0600}, {cpu_regs::STATE_AR3, 0x0601}, {cpu_regs::STATE_A, 0x0080000000ULL},
		{cpu_regs::STATE_B, index & 2 ? 0xffffffffffULL : 0}, {cpu_regs::STATE_T, 0x1234},
		{cpu_regs::STATE_ST0, 0x1400 | (index & 4 ? 0x0200 : 0)},
		{cpu_regs::STATE_ST1, 0x0140 | (index & 1 ? 0x0200 : 0)},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); // No pre-accumulation SMUL saturation.
	m_cpu->set_state_int(STATE_GENPC, 0x0800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 210000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_rounding_boundary_case(unsigned index)
{
	static constexpr u64 inputs[] = {0, 0xffffffffffULL, 0x7fffffff,
		0x80000000, 0xff80000000ULL, 0x7fffffffffULL,
		0x8000000000ULL, 0x0123456789ULL};
	m_cpu->reset();
	unsigned const selectors = index % 4;
	u64 const source = inputs[(index / 4) % 8];
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, 0xf49f | ((selectors & 2) << 8) | ((selectors & 1) << 8));
	program.write_word(0x0801, 0xf4e1); // Stop after exactly one RND.
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, selectors & 2 ? 0x123456789aULL : source},
		{cpu_regs::STATE_B, selectors & 2 ? source : 0x123456789aULL}, {cpu_regs::STATE_ST0, index >= 64 ? 0xabcd : 0},
		{cpu_regs::STATE_ST1, (index / 32) & 1 ? 0x0200 : 0}, {STATE_GENPC, 0x0800}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 200000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_shifted_arithmetic_selector_case(unsigned index)
{
	m_cpu->reset();
	unsigned const selectors = index % 8;
	static constexpr int boundary_shifts[] = {-16, -1, 0, 1, 15};
	int const shift = index < 24 ? int(index / 8) - 1 : boundary_shifts[((index - 24) / 8) % 5];
	bool const signed_memory = index >= 64;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, 0x6f82); // ADD/SUB *AR2,shift,src,dst
	program.write_word(0x0801, 0x0c00 | ((selectors & 4) ? 0x20 : 0) |
			((selectors & 2) ? 0x200 : 0) | ((selectors & 1) ? 0x100 : 0) | (shift & 31));
	program.write_word(0x0802, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0600, index < 24 ? 8 : 0xfff8);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x0600}, {cpu_regs::STATE_A, 10}, {cpu_regs::STATE_B, 30}, {cpu_regs::STATE_ST0, 0},
		{cpu_regs::STATE_ST1, signed_memory ? 0x0100 : 0}, {STATE_GENPC, 0x0800},
	});
	m_phase = 170000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_multiword_pipeline_case(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_long_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i)
	{
		program.write_word(0x600 + i, 0xf495);
		program.write_word(0x40 + i, 0xf495);
	}
	constexpr u16 opcodes[] = { 0xf020, 0xf120, 0xf02f, 0xf162 };
	constexpr u16 literals[] = { 0xe833, 0xf495, 0x7fff, 0x8001 };
	program.write_word(0x600, opcodes[variant]);
	program.write_word(0x601, literals[variant]); // Opcode-shaped data is never executed.
	program.write_word(0x602, 0xe822); // First not-decoded continuation.
	program.write_word(0x40, 0xe955);
	m_cpu->space(AS_DATA).write_word(0xfff, 0xbeef);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST1, variant == 1 || variant == 2 ? 0x0100 : 0},
		{cpu_regs::STATE_IMR, 1}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, 0},
		{cpu_regs::STATE_B, 0}, {STATE_GENPC, 0x600},
	});
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_expected_cycle = 1;
	m_pipeline_replay = false;
	m_vector_reads = m_vector_acks = 0;
	m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(0x601, 0x601, "pipeline_literal",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 3, "Immediate literal uses its own Fetch clock");
			++m_pipeline_operand_reads;
		});
	m_vector_prefetch_tap = program.install_read_tap(0x40, 0x40, "multiword_vector",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7 && m_vector_acks == 1,
				"Interrupt waits for complete multiword Decode before vector Fetch");
			++m_vector_reads;
		});
	m_phase = 251040;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_multiword_stream_case()
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0xf020); program.write_word(0x601, 0xe833);
	program.write_word(0x602, 0xf162); program.write_word(0x603, 0xf495);
	program.write_word(0x604, 0xe877);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ST1, 0x0800}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0},
		{STATE_GENPC, 0x600},
	});
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(0x601, 0x603, "pipeline_stream_literals",
		[this](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled() || address == 0x602) return;
			expect(m_cpu->pipeline_cycles() == (address == 0x601 ? 3 : 5),
				"Consecutive literals retain their independent Fetch clocks");
			++m_pipeline_operand_reads;
		});
	m_phase = 251060;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::accumulator_stm_fixture tms320c54x_test_state::accumulator_stm_case(unsigned v)
{
	accumulator_stm_fixture f{};
	f.lane = v % 6; f.literal = std::array<u16, 6>{0, 1, 0x7fff, 0x8000, 0xff00, 0xffff}[(v / 6) % 6];
	u64 const initial = std::array<u64, 4>{0, 0xffffffffffULL, 0x123456789aULL, 0x8000000000ULL}[(v / 36) % 4];
	f.a = initial; f.b = initial ^ 0x123456789aULL;
	f.final_a = f.a ^ 0xffffffffffULL; f.final_b = f.b ^ 0xffffffffffULL;
	unsigned const shift = 16 * (f.lane % 3);
	u64 const mask = (f.lane % 3 == 2 ? 0xffULL : 0xffffULL) << shift;
	u64 &selected = f.lane < 3 ? f.final_a : f.final_b;
	selected = (selected & ~mask) | ((u64(f.literal) << shift) & mask);
	f.st1 = 0x800 | (((v / 144) % 4) << 8);
	f.base = (v / 576) % 2 ? 0x20000 : 0; f.snapshot = v / 1152;
	return f;
}

void tms320c54x_test_state::start_accumulator_stm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = accumulator_stm_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x7708 + f.lane); program.write_word(f.base + 0x601, f.literal);
	program.write_word(f.base + 0x602, 0x7108 + f.lane); program.write_word(f.base + 0x603, 0x1200); program.write_word(f.base + 0x604, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_pipeline_store_writes = m_pipeline_operand_reads = 0;
	u16 const observed = f.lane % 3 == 2 ? u8(f.literal) : f.literal;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "accumulator_stm_read", [this, f, observed](offs_t at, u16 &value, u16) { if (machine().side_effects_disabled()) return; expect(at == 8 + f.lane && value == observed && m_cpu->pipeline_cycles() == 7 && m_pipeline_store_writes == 1 && !m_pipeline_operand_reads, "Adjacent MVDK reads the completed mapped write with no accumulator conflict"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "accumulator_stm_write", [this, f, observed](offs_t at, u16 &value, u16) { if (machine().side_effects_disabled()) return; unsigned const n = m_pipeline_store_writes++; expect(n < 2 && at == (n ? 0x1200 : 8 + f.lane) && value == (n ? observed : f.literal) && m_cpu->pipeline_cycles() == (n ? 8 : 7), "STM writes its original literal once at Execute before the retained MVDK RAM publication"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62132;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_logical_immediate(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const high = variant >= 768;
	unsigned const operation = 3 + (variant % 768) / 256;
	unsigned const shift = high ? 16 : std::array<unsigned, 4>{0, 1, 7, 15}[(variant >> 4) & 3];
	u16 const literal = BIT(variant, 6) ? 0xf495 : 0x8001;
	u64 const source = BIT(variant, 6) ? 0xff89abcdefULL : 0x0012345678ULL;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, (high ? 0xf060 | operation : 0xf000 | (operation << 4) | shift) | (BIT(variant, 0) << 9) | (BIT(variant, 1) << 8));
	program.write_word(0x601, literal);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xefff},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (BIT(variant, 2) << 8) | (BIT(variant, 3) << 9));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, BIT(variant, 0) ? 0x1111222233ULL : source);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, BIT(variant, 0) ? source : 0x4444555566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(0x601, 0x601, "staged_logical_literal",
		[this, literal](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == literal && m_cpu->pipeline_cycles() == 3, "Logical immediate fetches its unsigned retained literal once");
			++m_pipeline_operand_reads;
		});
	m_phase = 251570; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}



std::array<u16, 2> tms320c54x_test_state::addm_inputs(unsigned variant)
{
	return std::array<std::array<u16, 2>, 8>{{{0, 2}, {0x7fff, 1}, {0x8007, 0xfff8}, {0xffff, 1},
		{0x8000, 0x8000}, {0x7fff, 0x7fff}, {0xffff, 0xffff}, {0x1234, 0xffff}}}[(variant >> 3) & 7];
}

std::array<u16, 2> tms320c54x_test_state::addm_expected(unsigned variant)
{
	auto const input = addm_inputs(variant);
	s32 const left = BIT(variant, 0) ? s16(input[0]) : s32(input[0]);
	s32 const right = BIT(variant, 0) ? s16(input[1]) : s32(input[1]);
	s32 const sum = left + right; bool const overflow = sum < -32768 || sum > 32767;
	u16 const word = overflow && BIT(variant, 1) ? u16(sum < -32768 ? 0x8000 : 0x7fff) : u16(sum);
	u16 const flags = u16((0xaa08 | (BIT(variant, 2) << 10)) & ~0x800) |
		(u64(u32(left)) + u32(right) > 0xffffffffULL ? 0x800 : 0) | (overflow ? 0x400 : 0);
	return {word, flags};
}

void tms320c54x_test_state::start_pipeline_addm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const form = (variant / 64) & 3; bool const absolute = form == 3;
	auto const input = addm_inputs(variant); auto const expected = addm_expected(variant);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, std::array<u16, 4>{0x6b20, 0x6b82, 0x6b92, 0x6bf8}[form]);
	program.write_word(0x601, absolute ? 0x420 : input[1]);
	if (absolute) program.write_word(0x602, input[1]);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xaa08 | (BIT(variant, 2) << 10));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | (BIT(variant, 0) << 8) | (BIT(variant, 1) << 9));
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x420}, {cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0x2233445566ULL},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0x420, input[0]);
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x420, 0x420, "staged_addm_source",
		[this, input, absolute](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == input[0] && m_cpu->pipeline_cycles() == 6 + absolute, "ADDM samples the original target once in Read"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x420, 0x420, "staged_addm_write",
		[this, expected, absolute](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == expected[0] && m_cpu->pipeline_cycles() == 7 + absolute &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[1], "ADDM publishes retained arithmetic and flags once at Execute"); ++m_pipeline_store_writes;
		});
	m_phase = 251870; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_memory_logic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	unsigned const kind = variant % 6, form = (variant / 6) % 4, pair = (variant / 24) % 4;
	bool const sxm = BIT(variant / 96, 0);
	u16 const address = form == 0 ? 0xa0 : form == 1 ? 0x1020 : 0x470;
	u16 const operand = std::array<u16, 4>{0xffff, 0x8000, 0x00ff, 0x8001}[pair];
	u64 const source = std::array<u64, 4>{0, 0xffff, 0x123456789aULL, 0xff80000000ULL}[pair];
	u32 const base = BIT(variant, 0) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i)
		program.write_word(base + 0x600 + i, i ? 0xf495 : (0x1800 + (kind / 2) * 0x200) | (BIT(kind, 0) ? 0x100 : 0) | std::array<u16, 4>{0x20, 0x20, 0x92, 0x8a}[form]);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xb601}, {cpu_regs::STATE_ST1, 0x800 | (sxm ? 0x100 : 0) | (form == 1 ? 0x4000 : 0)},
		{cpu_regs::STATE_SP, 0x1000},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_A, BIT(kind, 0) ? 0x2233445566ULL : source);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, BIT(kind, 0) ? source : 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x470);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(address, operand);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(address, address, "logical_retained_operand",
		[this, operand](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == operand && m_cpu->pipeline_cycles() == 5, "Logical Smem samples one zero-extended word in Read");
			++m_pipeline_operand_reads;
		});
	m_phase = 252010; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_mu4_logic_tail(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	// Contiguous original R060 aa22 02:3dd5..3dda.
	std::array<u16, 6> const words{0xf000, 4, 0x1892, 0x1a8a, 0x6d8a, 0x8082};
	for (unsigned i = 0; i < 24; ++i) program.write_word(0x23dd5 + i, i < words.size() ? words[i] : 0xf495);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa600}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x2233445566ULL}, {cpu_regs::STATE_AR2, 0x470},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0x470, 0xff0f);
	m_cpu->space(AS_DATA).write_word(0x471, 0x00f0);
	m_cpu->space(AS_DATA).write_word(0x46f, 0x7777);
	m_cpu->set_state_int(STATE_GENPC, 0x23dd5);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x470, 0x471, "original_mu4_logical_reads",
		[this](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == 0x470 + m_pipeline_operand_reads && value == (address == 0x470 ? 0xff0f : 0x00f0) &&
				m_cpu->pipeline_cycles() == 7 + m_pipeline_operand_reads, "Original AND/OR sample separate words before their Execute stages");
			++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x46f, 0x46f, "original_mu4_logical_write",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x12f8 && m_cpu->pipeline_cycles() == 11, "Original logical tail writes its composed result to the retained destination");
			++m_pipeline_store_writes;
		});
	m_phase = 252030; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::ar_addm_fixture tms320c54x_test_state::ar_addm_case(unsigned variant) const
{
	unsigned const flags = (variant / 128) % 16;
	u16 const initial = std::array<u16, 4>{0, 0x7fff, 0x8000, 0xffff}[(variant / 8) % 4];
	u16 const immediate = std::array<u16, 4>{1, 0x0800, 0x7fff, 0xffff}[(variant / 32) % 4];
	s32 const left = flags & 1 ? s16(initial) : initial, right = flags & 1 ? s16(immediate) : immediate;
	s32 const sum = left + right;
	bool const overflow = sum < -32768 || sum > 32767;
	bool const carry = u64(u32(left)) + u32(right) > 0xffffffffULL;
	u16 const st0 = 0xa024 | (flags & 4 ? 0x800 : 0) | (flags & 8 ? 0x400 : 0);
	return {variant % 8, std::array<unsigned, 3>{6, 7, 8}[variant / 8192], (variant / 4096) % 2 ? 0x20000U : 0,
		initial, immediate, u16((flags & 2) && overflow ? sum < 0 ? -32768 : 32767 : sum), st0,
		u16(0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | ((variant / 2048) % 2 ? 0x20 : 0)),
		u16((st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? 0x400 : 0))};
}

tms320c54x_test_state::ar_addm_fixture tms320c54x_test_state::mapped_addm_case(unsigned variant) const
{
	bool const brc = profile_is("addm-brc");
	bool const t = profile_is("addm-t");
	bool const short_ar = profile_is("addm-ar-short");
	auto f = ar_addm_case(t ? (variant / 2) * 8 : brc ? variant * 8 : short_ar ? variant / 2 : variant);
	if (brc) f.reg = 10;
	if (t)
	{
		f.t = true; f.short_form = variant % 2;
		if (f.short_form)
		{
			--f.read; --f.write; --f.checkpoint; --f.extensions;
			f.st0 &= ~0x1ff; f.final_st0 &= ~0x1ff;
		}
	}
	if (short_ar)
	{
		f.short_ar_form = variant % 2 + 1; f.short_form = true;
		--f.read; --f.write; --f.checkpoint; --f.extensions;
		f.st0 &= ~0x1ff; f.final_st0 &= ~0x1ff;
		if (f.short_ar_form == 2 && (f.st1 & 0x20))
			f.final_st0 = (f.final_st0 & ~0xe000) | ((f.reg == 4 ? 5 : 4) << 13);
	}
	return f;
}

void tms320c54x_test_state::start_ar_addm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const brc = profile_is("addm-brc");
	auto f = mapped_addm_case(variant);
	u16 const address = f.t ? 0x0e : 0x10 + f.reg;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x9000 + i, 0xf495);
	program.write_word(f.base + 0x9000, 0x6bf8); program.write_word(f.base + 0x9001, 0x10 + f.reg);
	program.write_word(f.base + 0x9002, f.immediate); program.write_word(f.base + 0x9003, 0xf4e1);
	program.write_word(f.base + 0x9001, address);
	unsigned const pointer = f.reg == 4 ? 5 : 4;
	if (f.short_form) { program.write_word(f.base + 0x9000, 0x6b00 | (f.t ? 0x0e : f.short_ar_form == 2 ? 0x80 | pointer : address)); program.write_word(f.base + 0x9001, f.immediate); program.write_word(f.base + 0x9002, 0xf4e1); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, !f.t && ar == f.reg ? f.initial : 0x3000 + ar);
	if (f.short_ar_form == 2) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + pointer, address);
	if (brc) m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.initial);
	if (f.t) m_cpu->set_state_int(tms320c54x_device::STATE_T, f.initial);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "ar_addm_read",
		[this, f, address](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == address && word == f.initial && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read, "Mapped ADDM samples its original register once in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "ar_addm_write",
		[this, f, address](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == address && word == f.result && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == f.write, "Mapped ADDM publishes its retained arithmetic result once in Execute"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x9001, f.base + 0x9000 + f.extensions, "ar_addm_extensions",
		[this, f](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_extensions_reads < f.extensions && at == f.base + 0x9001 + m_pipeline_extensions_reads, "Mapped ADDM retains its address and immediate extensions once"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = ar_addm_phase;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_addm_brc_loop(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const count = std::array<unsigned, 4>{0, 1, 3, 7}[variant % 4]; bool const subtract = (variant / 4) % 2, call = (variant / 8) % 2, delayed = (variant / 16) % 2;
	u32 const base = (variant / 32) % 2 ? 0x20000 : 0, setup = base + (call ? 0x700 : 0x604), body = setup + (delayed ? 4 : 2);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, 0x6bf8); program.write_word(base + 0x601, 0x1a); program.write_word(base + 0x602, subtract ? 0xffff : 1);
	if (call) { program.write_word(base + 0x603, 0xf074); program.write_word(base + 0x604, 0x700); program.write_word(base + 0x605, 0xf4e1); }
	program.write_word(setup, delayed ? 0xf272 : 0xf072); program.write_word(setup + 1, u16(body + 1)); program.write_word(body, 0x6d90); program.write_word(body + 2, call ? 0xfc00 : 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, u16(subtract ? count + 1 : count - 1)); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa024); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1200);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x1a, 0x1a, "addm_brc_loop_read", [this, count, subtract](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads && word == u16(subtract ? count + 1 : count - 1), "ADDM samples its BRC source once before the subsequent loop"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x1a, 0x1a, "addm_brc_loop_write", [this, count](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_store_writes && word == count, "ADDM publishes the derived BRC count once"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 276020;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::branch_arithmetic_fixture tms320c54x_test_state::branch_arithmetic_case(unsigned variant)
{
	branch_arithmetic_fixture f{};
	f.operation = variant % 4; f.destination = (variant / 4) % 2; f.branch_b = (variant / 8) % 2; f.taken = (variant / 16) % 2; f.form = (variant / 32) % 3;
	unsigned const operand = (variant / 96) % 4, pattern = (variant / 384) % 4;
	f.flags = (variant / 1536) % 4; f.page = (variant / 6144) % 2 ? 0x20000 : 0;
	f.checkpoint = std::array<unsigned, 3>{5, 8, 9}[variant / 12288];
	f.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[operand]; f.source = f.form ? 0x1201 : 0x06b3;
	u64 const initial = std::array<u64, 4>{0, 1, 0x7fffffffULL, 0xff80000000ULL}[pattern];
	f.a = f.destination ? 0x123456789aULL : initial; f.b = f.destination ? initial : 0x81abcdef01ULL;
	(f.branch_b ? f.b : f.a) = f.taken ? 0 : 1;
	u64 const source = f.destination ? f.b : f.a;
	s64 const left = source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source);
	s64 const right = !(f.operation & 2) && (f.flags & 1) ? s16(f.memory) : s64(f.memory);
	s64 const result = (f.operation & 1) ? left - right : left + right;
	bool const overflow = result > 0x7fffffffLL || result < -0x80000000LL;
	bool const carry = (f.operation & 1) ? u32(source) >= u32(right) : u64(u32(source)) + u32(right) > 0xffffffffULL;
	f.st0 = 0xa024 | ((pattern & 1) ? 0x800 : 0) | ((operand & 1) ? 0x600 : 0);
	f.final_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	f.result = overflow && (f.flags & 2) ? (result < 0 ? 0xff80000000ULL : 0x7fffffffULL) : u64(result) & 0xffffffffffULL;
	return f;
}

void tms320c54x_test_state::start_branch_arithmetic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = branch_arithmetic_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 12; ++i) program.write_word(f.page + 0x9000 + i, 0xf495);
	program.write_word(f.page + 0x9000, f.branch_b ? 0xfa4d : 0xfa45); program.write_word(f.page + 0x9001, 0x9100);
	program.write_word(f.page + 0x9002, std::array<u16, 4>{0x0000, 0x0800, 0x0200, 0x0a00}[f.operation] | (f.destination << 8) | std::array<u16, 3>{0xf8, 0xe3, 0xeb}[f.form]);
	program.write_word(f.page + 0x9003, f.form ? 1 : f.source); program.write_word(f.page + 0x9004, 0xf4e1); program.write_word(f.page + 0x9100, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(f.source, f.memory);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800 | ((f.flags & 1) ? 0x100 : 0) | ((f.flags & 2) ? 0x200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1200);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2000); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "branch_arithmetic_read",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads && at == f.source && word == f.memory && m_cpu->pipeline_cycles() == 9, "Delayed arithmetic reads its retained memory address once after the branch decision"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "branch_arithmetic_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BCD arithmetic never writes data memory or a call frame"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.page + 0x9003, f.page + 0x9003, "branch_arithmetic_literal",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "BCD arithmetic retains its operand extension once"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = branch_arithmetic_phase;
	m_cpu->set_state_int(STATE_GENPC, f.page + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_bitt(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const form = variant % 8, selector = (variant / 8) % 16;
	bool const invert = (variant / 256) % 2;
	u16 const t = selector | ((variant / 128) % 2 ? 0xa5f0 : 0), mask = u16(1U << (15 - selector)), value = invert ? u16(~mask) : mask;
	u16 const source = form == 5 ? 0x06d1 : 0x1201;
	u32 const base = (variant / 1024) % 2 ? 0x20000 : 0;
	bool const extended = form >= 5;
	u8 const mode = std::array<u8, 8>{1, 1, 0x83, 0x8b, 0x93, 0xf8, 0xe3, 0xeb}[form];
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 12; ++i) program.write_word(base + 0x9000 + i, 0xf495);
	program.write_word(base + 0x9000, 0x3400 | mode);
	if (extended) program.write_word(base + 0x9001, form == 5 ? source : 1);
	program.write_word(base + 0x9001 + unsigned(extended), 0xf4e1);
	m_cpu->space(AS_DATA).write_word(source, value);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa824 | ((variant / 512) % 2 ? 0x1000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0xb00 | (form == 1 ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1200); m_cpu->set_state_int(tms320c54x_device::STATE_T, t);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0xff12345678ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x81abcdef01ULL);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == 3 ? (form >= 6 ? 0x1200 : 0x1201) : 0x4000 + ar);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "bitt_source",
		[this, source, value, extended](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads && at == source && word == value && m_cpu->pipeline_cycles() == (extended ? 6 : 5), "BITT samples its memory operand once in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "bitt_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BITT never writes its memory operand"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x9001, base + 0x9001, "bitt_extension",
		[this, extended](offs_t, u16 &, u16) { if (!machine().side_effects_disabled() && extended) { expect(!m_pipeline_extensions_reads, "BITT retains its address extension once"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = bitt_phase;
	m_cpu->set_state_int(STATE_GENPC, base + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_bitt_retd(unsigned variant)
{
	unsigned const form = variant % 2, selector = (variant / 2) % 16;
	unsigned const basic = (form ? 6 : 5) + selector * 8 + ((variant / 32) % 2) * 128 + ((variant / 64) % 2) * 256 + ((variant / 128) % 2) * 512 + ((variant / 256) % 2) * 1024;
	start_bitt(basic);
	u32 const base = (variant / 256) % 2 ? 0x20000 : 0;
	u16 const source = form ? 0x1201 : 0x06d1, mask = u16(1U << (15 - selector)), value = (variant / 64) % 2 ? u16(~mask) : mask;
	m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove();
	{
		auto const disable = machine().disable_side_effects(); auto &program = m_cpu->space(AS_PROGRAM);
		program.write_word(base + 0x9000, 0xfe00); program.write_word(base + 0x9001, form ? 0x34e3 : 0x34f8); program.write_word(base + 0x9002, form ? 1 : source);
		program.write_word(base + 0x9003, 0xffff); program.write_word(base + 0x9100, 0xf4e1);
		m_cpu->space(AS_DATA).write_word(0x2000, 0x9100);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2000);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "retd_bitt_reads",
		[this, source, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_operand_reads < 2 && (m_pipeline_operand_reads ? at == source && word == value : at == 0x2000 && word == 0x9100), "RETD reads one return word before BITT reads its retained memory operand"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = m_cpu->space(AS_PROGRAM).install_read_tap(base + 0x9002, base + 0x9002, "retd_bitt_extension",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "RETD retains BITT's delay extension without refetch"); ++m_pipeline_extensions_reads; } });
	m_pipeline_peripheral_case = variant; m_phase = bitt_phase + 2;
}

tms320c54x_test_state::delay_bitt_fixture tms320c54x_test_state::delay_bitt_case(unsigned v)
{
	delay_bitt_fixture f{}; f.control = v % 8; v /= 8; f.form = v % 4; v /= 4;
	f.slot = v % 2; v /= 2; f.selector = v % 16; v /= 16; f.upper = v % 2; v /= 2;
	f.invert = v % 2; v /= 2; f.initial_tc = v % 2; v /= 2;
	f.base = v % 2 ? 0x20000 : 0; v /= 2; f.snapshot = v;
	return f;
}

void tms320c54x_test_state::start_delay_bitt(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	auto const f = delay_bitt_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	for (unsigned i = 0; i < 8; ++i) program.write_word(f.base+0x600+i,0xf495);
	program.write_word(f.base+0x600,f.control == 4 ? 0xfb80 | (f.target_base() >> 16) : std::array<u16,8>{0xf273,0xfa45,0xfa45,0xf274,0,0xfe00,0xfa20,0xfa30}[f.control]);
	if (f.control != 5) program.write_word(f.base+0x601,0x700);
	if (f.slot) program.write_word(f.base+f.delay_start(),0x3083);
	program.write_word(f.base+f.delay_start()+f.slot,f.opcode());
	for (u32 at : {f.base+0x604,f.target_base()+0x700}) { program.write_word(at,0x7760); program.write_word(at+1,u16(at)); program.write_word(at+2,0xf4e1); }
	data.write_word(0x60,0xdead); data.write_word(0x5100,f.t()); data.write_word(f.source(),f.value());
	if (f.control == 5) data.write_word(0x3000,0x700);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0,f.status(0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,f.st1());
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a()); m_cpu->set_state_int(tms320c54x_device::STATE_B,0x81abcdef01ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_T,f.slot ? f.t() ^ 15 : f.t());
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC,13);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0+i,i == 3 ? 0x5100 : i == 4 ? 0x5000 : 0x4000+i);
	m_pipeline_operand_reads = m_port_reads = m_fast_stack_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"delay_bitt_read",[this,f](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x3000) { expect(f.control == 5 && !m_fast_stack_reads++ && word == 0x700,"RETD retains one original return word"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at,0x620); }
		else if (at == 0x5100) { expect(f.slot && !m_port_reads++ && word == f.t() && m_cpu->pipeline_cycles() == f.producer_read(),"Delay-slot LD publishes its original T source once"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at,u16(~word)); }
		else { if (at != f.source() || word != f.value() || m_cpu->pipeline_cycles() != f.read()) osd_printf_info("Delay BITT source mismatch: case=%u control=%u slot=%u form=%u address=%04x word=%04x clock=%llu expected=%u\n",m_pipeline_peripheral_case,f.control,f.slot,f.form,unsigned(at),word,(unsigned long long)m_cpu->pipeline_cycles(),f.read()); expect(!m_pipeline_operand_reads++ && at == f.source() && word == f.value() && m_cpu->pipeline_cycles() == f.read(),"Delay BITT reads one captured RAM source"); }
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"delay_bitt_write",[this,f](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff || at == 0x2ffe) { unsigned const i = m_pipeline_register_writes++; expect((f.control == 3 || f.control == 4) && i < unsigned(f.control == 4 ? 2 : 1) && at == 0x2fff-i && word == (i ? f.base >> 16 : 0x604),"Delay BITT preserves only its call owner's continuation frame"); }
		else expect(at == 0x60 && word == u16(f.selected()) && !m_pipeline_store_writes++ && m_pipeline_operand_reads == 1,"Only the path selected before BITT publishes its marker");
	});
	m_pipeline_fetch_tap = program.install_read_tap(f.base+f.delay_start()+f.slot,f.base+f.delay_start()+f.slot,"delay_bitt_opcode",[this,f](offs_t at,u16 &word,u16) { if (machine().side_effects_disabled()) return; expect(word == f.opcode(),"Delay BITT retains one original opcode"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(at,0xe8ff); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = delay_bitt_phase;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

void tms320c54x_test_state::start_ar_addm_load(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const reg = variant % 8, pattern = (variant / 8) % 4;
	bool const long_load = (variant / 32) % 2;
	// SPRU131G table 7-7: both unshifted LD forms are Category I consumers.
	unsigned const gap = 2 + (variant / 64) % 2;
	u32 const base = (variant / 128) % 2 ? 0x20000 : 0;
	u16 const initial = std::array<u16, 4>{0x1200, 0x7ff0, 0xfff0, 0x8000}[pattern];
	u16 const immediate = std::array<u16, 4>{0x0800, 0x0020, 0x1040, 0x9800}[pattern], result = initial + immediate;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x9000 + i, 0xf495);
	program.write_word(base + 0x9000, 0x6bf8); program.write_word(base + 0x9001, 0x10 + reg); program.write_word(base + 0x9002, immediate);
	program.write_word(base + 0x9003 + gap, 0x1000 | (long_load ? 0xe0 : 0x80) | reg);
	if (long_load) program.write_word(base + 0x9004 + gap, 0);
	program.write_word(base + 0x9004 + gap + long_load, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(initial, 0x5678); m_cpu->space(AS_DATA).write_word(result, 0x4000 + pattern);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa024); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == reg ? initial : 0x3000 + ar);
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "ar_addm_load_read",
		[this, reg, initial, result, pattern](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (!m_pipeline_operand_reads) expect(at == 0x10 + reg && word == initial, "Mapped ADDM reads the original AR before the consumer");
			else expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && at == result && word == 0x4000 + pattern, "Legal-gap consumer uses the newly published address rather than stale RAM");
			++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "ar_addm_load_write",
		[this, reg, result](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_store_writes && at == 0x10 + reg && word == result, "AR result publishes once before a legal indirect consumer"); ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = ar_addm_phase + 2;
	m_cpu->set_state_int(STATE_GENPC, base + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

u64 tms320c54x_test_state::ts_load_expected(u16 memory, u16 t, bool sxm)
{
	u64 bits = sxm && (memory & 0x8000) ? 0xffffff0000ULL | memory : memory;
	int const shift = (t & 0x20) ? int(t & 0x3f) - 64 : t & 0x3f;
	for (int i = 0; i < shift; ++i) bits = (bits * 2) & 0xffffffffffULL;
	for (int i = 0; i < -shift; ++i) bits = (bits >> 1) | (sxm && (bits & 0x8000000000ULL) ? 0x8000000000ULL : 0);
	return bits;
}

tms320c54x_test_state::ts_load_fixture tms320c54x_test_state::ts_load_case(unsigned variant, bool producer)
{
	unsigned v = variant, form = 2, code, kind = 0;
	if (producer) { kind = v % 3; v /= 3; code = std::array<unsigned, 8>{0, 1, 15, 16, 31, 32, 48, 63}[v % 8]; v /= 8; }
	else { form = v % 17; v /= 17; if (form >= 5) ++form; code = v % 64; v /= 64; }
	unsigned const pattern = v % 4; v /= 4;
	unsigned const flags = v % 4; v /= 4;
	unsigned const destination = v % 2; v /= 2;
	unsigned const compatibility = producer ? 0 : v % 2;
	unsigned const reg = producer ? 1 : code / 8;
	auto f = memory_arithmetic_case(form + 18 * reg + destination * 144 + pattern * 576 + flags * 4608 + compatibility * 18432);
	unsigned const prefix = producer ? kind == 1 ? 1 : 2 : 0;
	f.read_cycle += prefix;
	u16 const memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern], t = 0xa500 | code;
	u32 const base = (producer ? v % 2 : (destination ^ (code / 32))) ? 0x20000 : 0;
	u64 const a = producer && kind == 2 ? t : 0x123456789aULL, b = 0x2233445566ULL;
	return {f, memory, t, destination, kind, prefix, base, a, b, ts_load_expected(memory, t, flags & 1)};
}

void tms320c54x_test_state::start_ts_load(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const producer = profile_is("ld-ts-producer");
	auto const f = ts_load_case(variant, producer); auto const &o = f.address;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (producer)
	{
		program.write_word(f.base + 0x600, f.producer_kind == 0 ? 0x770e : f.producer_kind == 1 ? 0x3082 : 0x880e);
		if (!f.producer_kind) program.write_word(f.base + 0x601, f.t);
	}
	program.write_word(f.base + 0x600 + f.prefix, 0x1400 | (f.destination << 8) | u8(o.opcode));
	if (!producer && o.form >= 14) program.write_word(f.base + 0x601, o.literal);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, o.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, o.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, o.bk);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, producer ? 0xf : f.t);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5007);
	if (producer) { m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5100); data.write_word(0x5100, f.t); }
	data.write_word(o.address, f.memory);
	m_idle_fixture_tick = 0; m_pipeline_replay = false;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "ts_load_source", [this, f, producer](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		bool const t_read = producer && f.producer_kind == 1 && !m_pipeline_operand_reads;
		expect(m_pipeline_operand_reads < (producer && f.producer_kind == 1 ? 2 : 1) && at == (t_read ? 0x5100 : f.address.address) && word == (t_read ? f.t : f.memory) && m_cpu->pipeline_cycles() == (t_read ? 5 : f.address.read_cycle),
			"TS load and any T producer sample each retained source once"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "ts_load_no_write", [this, f, producer](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(producer && f.producer_kind != 1 && !m_pipeline_store_writes && at == 0xe && word == f.t && m_cpu->pipeline_cycles() == (f.producer_kind == 0 ? 5 : 6),
			"TS load writes no memory and preserves the T producer's owned publication"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "ts_load_literal", [this, f, producer](offs_t, u16 &word, u16) {
		if (machine().side_effects_disabled() || (producer ? f.producer_kind != 0 : f.address.form < 14)) return;
		expect(!m_pipeline_extensions_reads && word == (producer ? f.t : f.address.literal), "TS load retains its own address or producer literal once"); ++m_pipeline_extensions_reads; });
	m_phase = 254580; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_shifted_ar_conflict(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const reg = variant % 8, kind = (variant / 8) % 4;
	bool const same = (variant / 32) % 2, conflict = (variant / 64) % 2;
	u16 const destination = 0x10 + (same ? reg : reg ^ 1), older = conflict ? 0x10 + reg : 0x63;
	u32 const base = (variant / 128) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0x6f00 | older); program.write_word(base + 0x601, 0x0c81);
	program.write_word(base + 0x602, kind == 0 ? 0x7700 | destination : kind == 1 ? 0x7162 : kind == 2 ? 0x8a00 | destination : 0x7600 | destination);
	if (kind != 2) program.write_word(base + 0x603, kind == 1 ? destination : 0xbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xabcdef);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x4000 + i);
	// 0060/0061 belong to the fixture's side-effecting test port, not RAM.
	data.write_word(0x62, 0xbeef); data.write_word(0x1000, 0xbeef);
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "shift_ar_conflict_read", [this, kind](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if ((kind != 1 && kind != 2) || m_pipeline_operand_reads || at != (kind == 1 ? 0x62 : 0x1000) || word != 0xbeef || m_cpu->pipeline_cycles() != 7)
			osd_printf_info("shift_ar_conflict_read: case=%u kind=%u address=%04x word=%04x cycle=%llu\n", m_pipeline_peripheral_case, kind, unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles());
		expect((kind == 1 || kind == 2) && !m_pipeline_operand_reads++ && at == (kind == 1 ? 0x62 : 0x1000) && word == 0xbeef && m_cpu->pipeline_cycles() == 7,
			"Early writer retains its one source read despite the shifted-store bank conflict"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "shift_ar_conflict_write", [this, destination, older, conflict](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == older && word == 0x2468) { expect(!m_pipeline_store_writes++ && m_cpu->pipeline_cycles() == 7, "Shifted store publishes its own Execute word once"); return; }
		expect(at == destination && word == 0xbeef && !m_pipeline_register_writes++ && m_cpu->pipeline_cycles() == (conflict ? 8 : 7),
			"Shared DAGEN bank defers early publication once for same and different AR destinations"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 310100;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


void tms320c54x_test_state::start_pipeline_accumulator_logic()
{
	start_register_unary(register_unary_kind::logic, 252570);
}


void tms320c54x_test_state::start_pipeline_accumulator_shift()
{
	start_register_unary(register_unary_kind::shift, 252610);
}

tms320c54x_test_state::long_pair_fixture_data tms320c54x_test_state::long_pair_fixture(unsigned variant)
{
	if (variant >= 6912) return long_add_fixture(variant - 6912);
	unsigned const form = variant % 9, destination = (variant / 9) % 2, pattern = (variant / 18) % 8;
	bool const dual = (variant / 144) % 2, sxm = (variant / 288) % 2, ovm = (variant / 576) % 2, odd = (variant / 1152) % 2;
	unsigned const reg = odd ? 6 : 1;
	u16 const initial_ar = (form == 7 ? 0x1007 : 0x1000) + odd;
	s16 const offset = odd ? -2 : 2;
	u16 const address = form == 0 ? 0x402 + odd : form == 1 ? 0x1002 + odd : form == 4 ? initial_ar + 2 :
		form >= 5 && form <= 7 ? u16(initial_ar + offset) : form == 8 ? 0x1400 + odd : initial_ar;
	u16 const next_ar = form == 2 || form == 4 ? initial_ar + 2 : form == 3 ? initial_ar - 2 :
		form == 6 || form == 7 ? address : initial_ar;
	u8 const mode = form < 2 ? 2 + odd : form == 8 ? 0xf8 : std::array<u8, 6>{0x90, 0x88, 0x98, 0xe0, 0xe8, 0xf0}[form - 2] | reg;
	u64 const source = std::array<u64, 8>{0, 0x10000, 0x7fffffff, 0xff80000000ULL, 0x0180000000ULL, 0xff7fffffffULL, 0x56783933, 0x80aa001234ULL}[pattern];
	u32 const memory = std::array<u32, 8>{1, 1, 0xffffffffU, 1, 0, 0x80000000U, 0x15343456, 0xdeadbeefU}[pattern];
	u16 const st0 = 0x1808 | (pattern & 1 ? 0x600 : 0);
	u16 const st1 = 0x800 | (dual << 7) | (sxm << 8) | (ovm << 9) | (form == 1 ? 0x4000 : 0);
	u64 result;
	u16 result_st0 = st0;
	if (dual)
	{
		s64 const high_left = sxm ? s16(source >> 16) : u16(source >> 16), high_right = sxm ? s16(memory >> 16) : u16(memory >> 16);
		s64 const high_result = high_left - high_right;
		result = (u64(high_result) & 0xffffff) * 65536 + u16(u16(source) - u16(memory));
		result_st0 = (st0 & ~0x800) | (u16(source >> 16) >= u16(memory >> 16) ? 0x800 : 0);
	}
	else
	{
		s64 const left = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
		s64 const right = sxm && BIT(memory, 31) ? s64(memory) - (s64(1) << 32) : s64(memory);
		s64 const difference = left - right;
		bool const overflow = difference < -2147483648LL || difference > 2147483647LL;
		result = u64(overflow && ovm ? (difference < 0 ? -2147483648LL : 2147483647LL) : difference) & 0xffffffffffULL;
		result_st0 = (st0 & ~0x800) | (u32(source) >= memory ? 0x800 : 0) | (overflow ? (destination ? 0x200 : 0x400) : 0);
	}
	return {form, reg, form >= 5 ? 6U : 5U, address, next_ar, form == 8 ? address : u16(offset),
		u16(0x5400 | (destination << 8) | mode), st0, st1, source, result, memory, result_st0};
}

tms320c54x_test_state::long_pair_fixture_data tms320c54x_test_state::long_add_fixture(unsigned variant)
{
	unsigned const v = variant % 4608, form = v % 9, selector = (v / 9) % 4, pattern = (v / 36) % 8, flags = (v / 288) % 8;
	bool const dual = flags & 1, sxm = flags & 2, ovm = flags & 4, odd = (v / 2304) % 2;
	auto f = long_pair_fixture(form + 9 * (selector & 1) + 18 * pattern + 144 * dual + 288 * sxm + 576 * ovm + 1152 * odd);
	f.opcode = 0x5000 | ((selector >> 1) << 9) | ((selector & 1) << 8) | (f.opcode & 0xff);
	if (form < 2) { f.opcode += 2; f.address += 2; }
	if (dual)
	{
		s64 const left_high = sxm ? s16(f.source >> 16) : u16(f.source >> 16), right_high = sxm ? s16(f.memory >> 16) : u16(f.memory >> 16);
		f.result = ((u64(left_high + right_high) & 0xffffff) << 16) | u16(u16(f.source) + u16(f.memory));
		f.result_st0 = (f.st0 & ~0x800) | (u32(u16(f.source >> 16)) + u16(f.memory >> 16) > 0xffff ? 0x800 : 0);
	}
	else
	{
		s64 const left = BIT(f.source, 39) ? s64(f.source) - (s64(1) << 40) : s64(f.source);
		s64 const right = sxm && BIT(f.memory, 31) ? s64(f.memory) - (s64(1) << 32) : s64(f.memory);
		s64 const sum = left + right;
		bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
		f.result = u64(overflow && ovm ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
		f.result_st0 = (f.st0 & ~0x800) | (u64(u32(f.source)) + f.memory > 0xffffffffULL ? 0x800 : 0) | (overflow ? (selector & 1 ? 0x200 : 0x400) : 0);
	}
	return f;
}

tms320c54x_test_state::long_pair_fixture_data tms320c54x_test_state::long_reverse_fixture(unsigned variant)
{
	auto f = long_pair_fixture(variant);
	f.opcode = (f.opcode & 0x01ff) | 0x5800;
	bool const dual = f.st1 & 0x80, sxm = f.st1 & 0x100, ovm = f.st1 & 0x200;
	if (dual)
	{
		s64 const memory_high = sxm ? s16(f.memory >> 16) : u16(f.memory >> 16);
		s64 const source_high = sxm ? s16(f.source >> 16) : u16(f.source >> 16);
		f.result = ((u64(memory_high - source_high) & 0xffffff) << 16) | u16(u16(f.memory) - u16(f.source));
		f.result_st0 = (f.st0 & ~0x800) | (u16(f.memory >> 16) >= u16(f.source >> 16) ? 0x800 : 0);
	}
	else
	{
		s64 const memory = sxm && (f.memory & 0x80000000U) ? s64(f.memory) - (s64(1) << 32) : s64(f.memory);
		s64 const source = f.source & 0x8000000000ULL ? s64(f.source) - (s64(1) << 40) : s64(f.source);
		s64 const difference = memory - source;
		bool const overflow = difference < -2147483648LL || difference > 2147483647LL;
		f.result = u64(overflow && ovm ? (difference < 0 ? -2147483648LL : 2147483647LL) : difference) & 0xffffffffffULL;
		f.result_st0 = (f.st0 & ~0x800) | (f.memory >= u32(f.source) ? 0x800 : 0) | (overflow ? BIT(f.opcode, 8) ? 0x200 : 0x400 : 0);
	}
	return f;
}

void tms320c54x_test_state::start_pipeline_long_pair(unsigned variant)
{
	if (variant % 9 == 4) ++variant; // Short MOD=3 is write-only; preserve raw IDs for the other forms.
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = profile_is("drsub") ? long_reverse_fixture(variant) : long_pair_fixture(variant);
	bool const source = (f.opcode & 0xfc00) == 0x5000 ? BIT(f.opcode, 9) : BIT(f.opcode, 8);
	u32 const base = f.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, i ? 0xf495 : f.opcode);
	if (f.form >= 5) program.write_word(base + 0x601, f.literal);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, f.st0}, {cpu_regs::STATE_ST1, f.st1}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_AR0 + f.reg, (f.form == 7 ? 0x1007 : 0x1000) + (f.reg == 6)}, {cpu_regs::STATE_BK, 16},
		{cpu_regs::STATE_A, source ? 0x246813579aULL : f.source},
		{cpu_regs::STATE_B, source ? f.source : 0x246813579aULL}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(f.address, u16(f.memory >> 16));
	m_cpu->space(AS_DATA).write_word(f.address ^ 1, u16(f.memory));
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "dsub_long_pair",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_operand_reads < 2 && at == (f.address ^ m_pipeline_operand_reads) &&
				word == (m_pipeline_operand_reads ? u16(f.memory) : u16(f.memory >> 16)) && m_cpu->pipeline_cycles() == f.read_cycle,
				"Long arithmetic samples its retained MSW/LSW pair once in parity-defined order"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "dsub_no_memory_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Long arithmetic does not write data memory"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "dsub_offset",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || f.form < 5) return;
			expect(word == f.literal && !m_pipeline_extensions_reads, "Long arithmetic retains its extension once"); ++m_pipeline_extensions_reads; });
	m_phase = 252630; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::accumulator_store_fixture tms320c54x_test_state::long_accumulator_store_fixture(unsigned variant)
{
	if (variant >= 38400)
	{
		unsigned const v = (variant - 38400) % 57344, form = v % 7, offset = (v / 7) % 4;
		unsigned const selector = (v / 28) % 4, flags = (v / 112) % 4, pattern = (v / 448) % 4;
		unsigned const original_form = form == 6 ? 24 : (form / 2) * 8 + (form & 1 ? 6 : 1);
		auto f = long_accumulator_store_fixture(original_form + offset * 25 + selector * 100 + flags * 400 + pattern * 1600);
		int const shift = int((v / 1792) % 32) - 16;
		u64 const shifted = shift < 0 ? (BIT(f.st1, 8) ? u64((BIT(f.source, 39) ? s64(f.source) - (s64(1) << 40) : s64(f.source)) >> -shift) : f.source >> -shift) & 0xffffffffffULL : (f.source << shift) & 0xffffffffffULL;
		u32 stored = u32(shifted);
		if (f.pmst & 1)
		{
			if (!BIT(f.st1, 8)) stored = shifted > 0xffffffffULL ? 0xffffffffU : u32(shifted);
			else stored = u32(std::clamp(BIT(shifted, 39) ? s64(shifted) - (s64(1) << 40) : s64(shifted), s64(-2147483648LL), s64(2147483647LL)));
		}
		f.value = stored >> (selector & 2 ? 16 : 0);
		return f;
	}
	unsigned const form = variant % 25, kind = form == 24 ? 3 : form / 8, reg = form == 24 ? 0 : form % 8;
	unsigned const selector = (variant / 100) % 4, flags = (variant / 400) % 4;
	bool const compatibility = (variant / 6400) % 2;
	s16 const offset = std::array<s16, 4>{0, 1, -1, s16(0xf495)}[(variant / 25) % 4];
	u64 const source = std::array<u64, 4>{0x12345678, 0x0180000000ULL, 0xff7fffffffULL, 0xffffffffffULL}[(variant / 1600) % 4];
	u16 const initial = kind == 2 ? 0x4007 : 0x1007, bk = kind == 2 ? 8192 : 16;
	u16 const address = kind == 3 ? u16(0x1500 + offset) : kind == 2 ? circular_fixture_next(initial, offset, bk) : u16(initial + offset);
	u32 result = u32(source);
	if (flags & 1)
	{
		if (!(flags & 2)) { if (source > 0xffffffffULL) result = 0xffffffff; }
		else
		{
			s64 const signed_source = source & (1ULL << 39) ? s64(source) - (1LL << 40) : s64(source);
			if (signed_source < -0x80000000LL) result = 0x80000000;
			else if (signed_source > 0x7fffffffLL) result = 0x7fffffff;
		}
	}
	return {reg, compatibility && !reg ? 3U : reg, kind, selector,
		u16(0x8000 | (selector << 8) | (kind == 3 ? 0xf8 : 0xe0 | (kind << 3) | reg)),
		u16(kind == 3 ? address : u16(offset)), address, u16(kind && kind != 3 ? address : initial),
		u16(compatibility ? 0x6608 : 0x608), u16(0x800 | ((flags & 2) ? 0x100 : 0) | (compatibility ? 0x20 : 0)),
		u16(flags & 1), u16(result >> ((selector & 2) ? 16 : 0)), source, initial, bk};
}

void tms320c54x_test_state::start_shifted_store_dld(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	unsigned const reg = variant%8, pattern = (variant/8)%4, gap = (variant/32)%3;
	bool const shifted = (variant/96)%2; u32 const page = (variant/192)%2 ? 0x20000 : 0;
	u64 const source = std::array<u64,4>{0x11223344,0xffffff6b76,0x7fff8000,0xff80000000}[pattern];
	u32 const loaded = std::array<u32,4>{0x1234abcd,0x80000000,0xffff0001,0x7fff8000}[pattern];
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i=0;i<48;++i) program.write_word(page+0x600+i,0xf495);
	unsigned cursor = 0;
	if (shifted)
	{
		program.write_word(page+0x600+cursor++,0x6f90|reg); program.write_word(page+0x600+cursor++,0x0c60);
		program.write_word(page+0x600+cursor++,0x6f90|reg); program.write_word(page+0x600+cursor++,0x0c80);
	}
	else
	{
		program.write_word(page+0x600+cursor++,0x8290|reg);
		program.write_word(page+0x600+cursor++,0x8090|reg);
	}
	program.write_word(page+0x600+cursor+gap,0x5790|reg); // DLD *ARx+,B follows the two word stores.
	for (unsigned i=0;i<6;++i) data.write_word(0x1000+i,0x5a5a);
	data.write_word(0x1002,loaded>>16); data.write_word(0x1003,loaded);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0,0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,0x6100);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0);
	for (unsigned ar=0;ar<8;++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0+ar,ar==reg ? 0x1000 : 0x2000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,source); m_cpu->set_state_int(tms320c54x_device::STATE_B,0);
	m_cpu->set_state_int(STATE_GENPC,page+0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0x1000,0x1005,"shifted_store_dld_read",
		[this](offs_t address,u16 &,u16) { if (machine().side_effects_disabled()) return;
			if (address != 0x1002+m_pipeline_extensions_reads)
				osd_printf_error("Store/DLD address failure: variant=%u shifted=%u gap=%u address=%04x expected=%04x\n",
					m_pipeline_peripheral_case,(m_pipeline_peripheral_case/96)%2,(m_pipeline_peripheral_case/32)%3,unsigned(address),0x1002+m_pipeline_extensions_reads);
			expect(address == 0x1002+m_pipeline_extensions_reads,"DLD consumes the pair after both older store increments"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = data.install_write_tap(0x1000,0x1005,"shifted_store_dld_write",
		[this,source](offs_t address,u16 &word,u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_store_writes<2 && address==0x1000+m_pipeline_store_writes && word==u16(source>>(m_pipeline_store_writes ? 0 : 16)),
				"Younger DLD Access must not retarget the older shifted store"); ++m_pipeline_store_writes; });
	m_phase = 321000; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_long_accumulator_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	if (!variant) { m_long_store_coverage = {}; m_long_store_replays = 0; }
	auto const f = long_accumulator_store_fixture(variant);
	u32 const base = f.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, f.opcode); program.write_word(base + 0x601, f.literal);
	bool const shifted = variant >= 38400;
	if (shifted)
	{
		program.write_word(base + 0x600, 0x6f00 | u8(f.opcode));
		program.write_word(base + 0x602, (f.selector & 2 ? 0x0c60 : 0x0c80) | ((f.selector & 1) << 8) | ((int(((variant - 38400) % 57344 / 1792) % 32) - 16) & 31));
	}
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, f.pmst},
		{cpu_regs::STATE_ILLEGAL, 0},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x1007);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, f.initial);
	m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x11112222); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x33334444);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(f.address, 0xaaaa);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "long_accumulator_store_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Accumulator store does not read its destination"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "long_accumulator_store_write",
		[this, f, shifted](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == f.value && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (shifted ? 8 : 7),
				"Long-offset STL/STH owns one retained-address write with Execute-live source saturation"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601 + shifted, "long_accumulator_store_offset",
		[this, f, shifted, variant, base](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			bool const operation = shifted && at == base + 0x602;
			u16 const expected = operation ? (f.selector & 2 ? 0x0c60 : 0x0c80) | ((f.selector & 1) << 8) | ((int(((variant - 38400) % 57344 / 1792) % 32) - 16) & 31) : f.literal;
			expect(word == expected && m_pipeline_extensions_reads == unsigned(operation), "STL/STH retains address before operation extension"); ++m_pipeline_extensions_reads; });
	m_phase = 252810; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

std::array<u16, 2> tms320c54x_test_state::long_rmw_expected(unsigned arithmetic, unsigned operation, u16 flags)
{
	auto const input = addm_inputs(arithmetic);
	if (operation) return {u16(operation == 1 ? input[0] & input[1] : operation == 2 ? input[0] | input[1] : input[0] ^ input[1]), flags};
	auto const sum = addm_expected(arithmetic);
	return {sum[0], u16((flags & ~0x0c00) | (sum[1] & 0x0c00))};
}

tms320c54x_test_state::accumulator_store_fixture tms320c54x_test_state::absolute_xorm_fixture(unsigned variant)
{
	auto f = long_accumulator_store_fixture(24 + 25 * (variant % 4) + (((variant / 256) % 2) ? 6400 : 0));
	f.reg = (variant / 512) % 2 ? 6 : 0;
	return f;
}

tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::memory_arithmetic_case(unsigned variant, bool unsigned_memory, unsigned shift16_selector)
{
	unsigned const form = variant % 18, reg = (variant / 18) % 8, selector = (variant / 144) % 4, pattern = (variant / 576) % 8, flags = (variant / 4608) % 4;
	bool const shift16 = shift16_selector < 8;
	bool const compatibility = (variant / 18432) % 2, subtract = shift16 ? bool(shift16_selector & 4) : bool(selector & 2), sxm = flags & 1, ovm = flags & 2;
	unsigned const selected = compatibility && !reg ? 3 : reg;
	u64 const source = std::array<u64, 8>{0, 0xffffffff, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x56783933, 0x80aa001234ULL}[pattern];
	u16 const memory = std::array<u16, 8>{1, 1, 1, 0xffff, 0x8000, 0xffff, 0x1234, 0xffff}[pattern];
	s16 const offset = std::array<s16, 8>{0, 1, -1, 17, s16(0xf495), 0x7fff, s16(0x8000), 2}[pattern];
	u8 const mode = form < 2 ? reg * 16 + pattern + (sxm ? 8 : 0) : form == 17 ? 0xf8 : 0x80 | ((form - 2) << 3) | reg;
	// AR0 starts at 0x5007, including when it is the selected pointer.
	// Indexed and long circular forms need a buffer large enough for that
	// step and every signed literal (SPRU131G section 5.5.3.4).
	u16 const bk = form == 11 || form == 13 || form == 16 ? 0x8000 : 16;
	u16 const address = form == 0 ? 0x200 + mode : form == 1 ? 0x1000 + mode : form == 5 ? 0x5008 :
		form == 16 ? circular_fixture_next(0x5007, offset, bk) : form >= 14 && form < 17 ? u16(0x5007 + offset) :
		form == 17 ? u16(0x1500 + offset) : 0x5007;
	u16 const next_ar = form < 2 || form == 17 ? 0x5007 : form >= 14 ? (form == 14 ? 0x5007 : address) :
		form == 11 || form == 13 ? circular_fixture_next(0x5007, form == 11 ? -0x5007 : 0x5007, bk) :
		std::array<u16, 12>{0x5007, 0x5006, 0x5008, 0x5008, 0, 0, 0xa00e, 0x2803, 0x5006, 0x5000, 0x5008, 0x500e}[form - 2];
	u16 const st0 = (shift16 ? 0x1004 | (pattern & 1 ? 0x800 : 0) : 0x1804) | (compatibility ? 0x6000 : 0) | (pattern & 1 ? 0x600 : 0);
	u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (compatibility ? 0x20 : 0) | (form == 1 ? 0x4000 : 0);
	u16 result_st0 = compatibility && reg && form >= 2 && form != 17 ? (st0 & 0x1fff) | (reg << 13) : st0;
	s64 const left = source & (1ULL << 39) ? s64(source) - (1LL << 40) : s64(source);
	s64 const right = (sxm && !unsigned_memory ? s64(s16(memory)) : s64(memory)) * (shift16 ? 65536 : 1), sum = subtract ? left - right : left + right;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = subtract ? u32(source) >= u32(right) : u64(u32(source)) + u32(right) > 0xffffffffULL;
	if (!shift16 || carry != subtract) result_st0 = (result_st0 & ~0x800) | (carry ? 0x800 : 0);
	result_st0 |= overflow ? ((shift16 ? shift16_selector : selector) & 1 ? 0x200 : 0x400) : 0;
	u64 const result = u64(overflow && ovm ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
	return {form, reg, selected, form >= 14 ? 6U : 5U, u16((shift16 ? (subtract ? 0x4000 : 0x3c00) | ((shift16_selector & 3) << 8) : (unsigned_memory ? (subtract ? 0x0a00 : 0x0200) : subtract ? 0x0800 : 0) | ((selector & 1) << 8)) | mode),
		u16(form == 17 ? address : u16(offset)), address, next_ar, memory, st0, st1, result_st0, source, result, bk};
}

tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::memory_shift16_case(unsigned variant)
{
	return memory_arithmetic_case(variant % 144 + ((variant / 1152) % 8) * 576 +
		((variant / 9216) % 4) * 4608 + ((variant / 36864) % 2) * 18432, false, (variant / 144) % 8);
}

void tms320c54x_test_state::start_pipeline_memory_arithmetic(unsigned variant)
{
	using kind = smem_schedule::kind;
	if (!variant)
	{
		m_memory_kind = profile_is("memory-shift16") ? kind::shift16 : profile_is("adds") ? kind::unsigned_add :
			profile_is("subs") ? kind::unsigned_sub : kind::signed_word;
		m_memory_alu_coverage = {}; m_memory_alu_restores = 0;
	}
	if (variant % 18 == 5) ++variant; // Short MOD=3 is write-only; retain raw snapshot IDs.
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const shift16 = m_memory_kind == kind::shift16, adds = m_memory_kind == kind::unsigned_add;
	// Pure input/reference data is fixed for the live/replay pair. Do not
	// rebuild it on every timer tick or derive it from the mutated CPU state.
	m_memory_input = shift16 ? memory_shift16_case(variant) : adds || m_memory_kind == kind::unsigned_sub ?
		memory_unsigned_case(variant, !adds) : memory_arithmetic_case(variant);
	auto const &f = m_memory_input;
	u32 const base = f.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, f.opcode); if (f.form >= 14) program.write_word(base + 0x601, f.literal);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5007);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, BIT(f.opcode, shift16 ? 9 : 8) ? 0x123456789aULL : f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, BIT(f.opcode, shift16 ? 9 : 8) ? f.source : 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(f.address, f.memory);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "memory_arithmetic_sample",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == f.memory && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read_cycle,
				"Memory ADD/SUB samples one word through its captured address in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "memory_arithmetic_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Memory ADD/SUB modifies only its accumulator/status"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "memory_arithmetic_literal",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || f.form < 14) return;
			expect(word == f.literal && !m_pipeline_extensions_reads, "Memory ADD/SUB retains its offset/absolute literal once"); ++m_pipeline_extensions_reads; });
	m_phase = 252910; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::register_arithmetic_case(unsigned variant, bool bk)
{
	unsigned const reg = bk ? (variant / 256) % 2 ? 6 : 0 : variant % 8, selector = bk ? variant % 4 : (variant / 8) % 4, flags = (variant / 32) % 4, pattern = bk ? (variant / 4) % 8 : 2 + (variant / 128) % 2;
	bool const absolute = (variant / (bk ? 128 : 256)) % 2;
	auto f = memory_arithmetic_case(selector * 144 + pattern * 576 + flags * 4608);
	if (bk && !pattern) { f.memory = 0; f.result = 0; f.result_st0 = (f.st0 & ~0x800) | (selector & 2 ? 0x800 : 0); }
	f.reg = f.selected = reg; f.form = absolute ? 17 : 0; f.read_cycle = absolute ? 6 : 5;
	f.address = f.literal = bk ? 0x19 : 0x10 + reg;
	f.opcode = (f.opcode & 0xff00) | (absolute ? 0xf8 : f.address);
	f.st0 &= ~0x1ff; f.result_st0 &= ~0x1ff;
	return f;
}

clocked_register_case tms320c54x_test_state::mapped_arithmetic_sequence(unsigned variant, bool bk)
{
	auto const f = register_arithmetic_case(variant, bk);
	u32 const base = f.reg == 6 ? 0x20000 : 0;
	int const source = bk ? tms320c54x_device::STATE_BK : tms320c54x_device::STATE_AR0 + f.reg;
	bool const dest_b = BIT(f.opcode, 8);
	instruction_case initial{"mapped-arithmetic", f.opcode, {}, {}, {
		{tms320c54x_device::STATE_IDLE, 0}, {tms320c54x_device::STATE_TCR, 0x10},
		{tms320c54x_device::STATE_PMST, 0}, {tms320c54x_device::STATE_ILLEGAL, 0},
		{tms320c54x_device::STATE_ST0, f.st0}, {tms320c54x_device::STATE_ST1, f.st1}, {source, f.memory},
		{tms320c54x_device::STATE_A, dest_b ? 0x123456789aULL : f.source},
		{tms320c54x_device::STATE_B, dest_b ? f.source : 0x123456789aULL},
		{tms320c54x_device::STATE_IFR, 0}, {tms320c54x_device::STATE_IMR, 0}, {STATE_GENPC, base + 0x600}
	}, {{tms320c54x_device::STATE_ILLEGAL, 0}}, 0, 0};
	for (unsigned i = 0; i < 24; ++i) initial.program.push_back({base + 0x600 + i, 0xf495});
	initial.program.push_back({base + 0x600, f.opcode});
	if (f.form == 17) initial.program.push_back({base + 0x601, f.literal});
	// Stop after the tested instruction instead of executing unobserved NOP padding.
	initial.program.push_back({base + 0x601 + unsigned(f.form == 17), 0xf4e1});
	instruction_case final{"mapped-arithmetic", f.opcode, {}, {}, {}, {
		{tms320c54x_device::STATE_ILLEGAL, 0}, {tms320c54x_device::STATE_ST0, f.result_st0},
		{tms320c54x_device::STATE_A, dest_b ? 0x123456789aULL : f.result},
		{tms320c54x_device::STATE_B, dest_b ? f.result : 0x123456789aULL}, {source, 0x2222}
	}, 0, 0};
	clocked_register_case test{{{0, std::move(initial)}, {16, std::move(final)}},
		variant / 512 ? f.read_cycle : f.read_cycle - 1, 16,
		{{f.read_cycle, {{source, 0x2222}}}}, variant};
	test.data_events = std::vector<clocked_data_event>{{false, f.address, f.memory, f.read_cycle}};
	return test;
}

void tms320c54x_test_state::start_pipeline_register_arithmetic()
{
	bool const bk = profile_is("arith-bk");
	m_register_sequence = std::make_unique<register_sequence_fixture>(machine(), *m_cpu, *m_check_timer,
		1024, [bk](unsigned key) { return mapped_arithmetic_sequence(key, bk); });
	m_phase = 252930; m_register_sequence->start();
}

void tms320c54x_test_state::start_pipeline_xc_arithmetic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const operation = variant % 4;
	bool const subtract = operation >= 2, destination_b = operation & 1;
	u32 const base = (variant / 8) % 2 ? 0x20000 : 0;
	u16 const source = subtract ? 0x1011 : 0x1000;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, (variant / 4) % 2 ? 0xff00 : 0xfd00);
	program.write_word(base + 0x601, std::array<u16, 4>{0x00f8, 0x01f8, 0x08e1, 0x09e1}[operation]);
	program.write_word(base + 0x602, subtract ? 0x0011 : source);
	program.write_word(base + 0x603, destination_b ? 0xe866 : 0xe966);
	program.write_word(base + 0x606, 0xf4e1);
	program.write_word(base + 0x40, 0x4a1e); program.write_word(base + 0x41, 0xf495); program.write_word(base + 0x42, 0xf4e5);
	m_cpu->set_input_line(0, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, destination_b ? 0 : 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, destination_b ? 0x1234 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->space(AS_DATA).write_word(source, 1);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(source, source, "xc_arithmetic_source",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == 1 && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 7,
				"Protected two-word arithmetic retains its source once before retirement"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x40, base + 0x40, "xc_arithmetic_vector",
		[this, subtract, destination_b](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(!m_pipeline_extensions_reads && m_cpu->pipeline_cycles() > 8 &&
				m_cpu->state_int(destination_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == (subtract ? 0x1233 : 0x1235),
				"XC releases the pending IRQ only after the full arithmetic instruction retires"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "xc_arithmetic_stack",
		[this](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(at >= 0xffe && at <= 0xfff && m_pipeline_store_writes < 2 && m_cpu->pipeline_cycles() > 8,
				"Only the later interrupt and its XPC save write the protected arithmetic stack"); ++m_pipeline_store_writes; });
	m_phase = 252950; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

std::array<u16, 2> tms320c54x_test_state::bitfield_inputs(unsigned variant, bool compare)
{
	if (compare) return std::array<std::array<u16, 2>, 8>{{{0, 0}, {1, 1}, {0x80, 0x7f}, {0x7f, 0x7f},
		{0x8000, 0x8000}, {0x8000, 0x7fff}, {0xffff, 0xf495}, {0xffff, 0xffff}}}[(variant / 144) % 8];
	return std::array<std::array<u16, 2>, 8>{{{0xffff, 0}, {1, 1}, {0x80, 0x7f}, {0x7f, 0x7f},
		{0x8000, 0x8000}, {0x8000, 0x7fff}, {0xffff, 0xf495}, {0, 0xffff}}}[(variant / 144) % 8];
}

tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::bitfield_address_case(unsigned variant)
{
	return memory_arithmetic_case(variant % 144 + ((variant / 144) % 8) * 576 +
		((variant / 2304) % 4) * 4608 + ((variant / 9216) % 2) * 18432);
}

void tms320c54x_test_state::start_pipeline_bitfield(unsigned variant)
{
	if (!variant) { m_bitfield_coverage = {}; m_bitfield_restores = 0; }
	if (variant % 18 == 5) ++variant; // Preserve raw IDs, omitting write-only short MOD=3.
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const compare = profile_is("cmpm-smem");
	auto const f = bitfield_address_case(variant); auto const input = bitfield_inputs(variant, compare);
	bool const extended = f.form >= 14;
	u32 const base = f.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, (compare ? 0x6000 : 0x6100) | u8(f.opcode));
	program.write_word(base + 0x601, extended ? f.literal : input[1]);
	if (extended) program.write_word(base + 0x602, input[1]);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, (f.st0 & ~0x1000) | (((variant / 1152) % 2) ? 0x1000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5007);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(f.address, input[0]);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "bitfield_sample",
		[this, f, input](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == f.address && word == input[0] && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read_cycle + 1,
				"BITF retains one source word through its captured address in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "bitfield_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BITF must not write its memory operand"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + (extended ? 0x602 : 0x601), "bitfield_literals",
		[this, f, input, extended](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < (extended ? 2 : 1) && (at & 0xffff) == 0x601 + m_pipeline_extensions_reads &&
				word == (extended && !m_pipeline_extensions_reads ? f.literal : input[1]), "BITF retains offset/address before its distinct mask literal"); ++m_pipeline_extensions_reads; });
	m_phase = 253010; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::accumulator_cmpm_fixture tms320c54x_test_state::accumulator_cmpm_case(unsigned v, bool mapped, bool edge)
{
	accumulator_cmpm_fixture f{};
	if (edge)
	{
		unsigned const index = v % 80; v /= 80; f.source = index < 8 ? index : index + 0x10;
		f.form = v % 3; v /= 3; f.initial_tc = v % 2; v /= 2;
		f.st1 = 0x800 | (v % 2 ? 0x4000 : 0); v /= 2;
		f.base = v % 2 ? 0x20000 : 0; v /= 2; f.snapshot = v; f.literal = 1; f.excluded = true;
	}
	else
	{
		f.lane = v % (mapped ? 10 : 6); v /= mapped ? 10 : 6;
		f.value = mapped ? std::array<u16, 8>{0, 1, 0x10, 0x7f, 0x80, 0x7fff, 0x8000, 0xffff}[v % 8] :
			std::array<u16, 4>{0, 1, 0x7f, u16(f.lane % 3 == 2 ? 0x80 : 0x8000)}[v % 4]; v /= mapped ? 8 : 4;
		f.tc = v % 2; v /= 2; f.initial_tc = v % 2; v /= 2;
		f.literal = mapped ? std::array<u16, 6>{0, 1, 0x10, 0x7f, 0x8000, 0xffff}[v % 6] : v % 2 ? 0x7f : 1; v /= mapped ? 6 : 2;
		f.form = v % 3; v /= 3; f.st1 = 0x800 | (v % 2 ? 0x4000 : 0); v /= 2;
		f.base = v % 2 ? 0x20000 : 0; v /= 2; f.snapshot = v;
		f.source = (mapped ? 14 : 8) + f.lane;
	}
	f.read = f.form == 2 ? 7 : 6; f.pointer = mapped ? f.source == 0x17 ? 6 : 7 : 2;
	f.a = 0x0011223344ULL; f.b = 0x0055667788ULL;
	if (!mapped && !edge)
	{
		u64 &accumulator = f.lane < 3 ? f.a : f.b;
		unsigned const shift = 16 * (f.lane % 3);
		accumulator = (accumulator & ~(0xffffULL << shift)) | (u64(f.value) << shift);
	}
	return f;
}

void tms320c54x_test_state::start_accumulator_cmpm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const mapped = profile_is("cmpm-map") || profile_is("cmpm-map-edge");
	bool const edge = profile_is("cmpm-map-edge"); auto const f = accumulator_cmpm_case(variant, mapped, edge);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.form == 2 ? 0x60f8 : f.form == 1 ? 0x6080 | f.pointer : 0x6000 | f.source);
	if (f.form == 2) program.write_word(f.base + 0x601, f.source);
	unsigned const words = f.form == 2 ? 3 : 2;
	program.write_word(f.base + 0x600 + words - 1, f.literal);
	program.write_word(f.base + 0x600 + words, 0xf820 | (f.tc << 4));
	program.write_word(f.base + 0x601 + words, 0x700);
	program.write_word(f.base + 0x602 + words, 0xe811); program.write_word(f.base + 0x603 + words, 0xf4e1);
	program.write_word(f.base + 0x700, 0xe855); program.write_word(f.base + 0x701, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600 | (f.initial_tc ? 0x1000 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0);
	if (mapped)
	{
		for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x1200 + i * 0x100);
		m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0xabcd);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.pointer, f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ^ 0xffffffffffULL);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mapped_cmpm_read", [this, f](offs_t at, u16 &value, u16) { if (machine().side_effects_disabled()) return; expect(!f.excluded && at == f.source && value == f.value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read, "CMPM samples its selected mapped word exactly once at Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mapped_cmpm_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + words - 1 + 0x600, "mapped_cmpm_literals", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(at == f.base + 0x601 + m_pipeline_extensions_reads && word == (f.form == 2 && !m_pipeline_extensions_reads ? f.source : f.literal), "CMPM captures the address before its distinct comparison literal once"); ++m_pipeline_extensions_reads;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62126;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_direct_cmpm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u16 const value = std::array<u16, 4>{0, 1, 0x7f, 0x8000}[variant % 4], literal = (variant / 16) % 2 ? 0x7f : 1;
	bool const tc = (variant / 4) % 2, initial_tc = (variant / 8) % 2, cpl = (variant / 32) % 2;
	u16 const source = cpl ? 0x4014 : 0x1694;
	u32 const base = (variant / 64) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	std::array<u16, 7> const code{0x6014, literal, 0xf495, u16(0xf820 | (tc << 4)), 0x700, 0xe811, 0xf4e1};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(base + 0x600 + i, code[i]);
	program.write_word(base + 0x700, 0xe855); program.write_word(base + 0x701, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa62d | (initial_tc ? 0x1000 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, cpl ? 0x4800 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x4000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678); data.write_word(source, value);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "direct_cmpm_read", [this, source, value](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == source && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == 6, "Direct CMPM reads its retained DP/SP address once"); ++m_pipeline_operand_reads; });
	m_operand_data_tap = program.install_read_tap(base + 0x601, base + 0x601, "direct_cmpm_literal", [this, literal](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == literal && !m_pipeline_drained_fetches, "Direct CMPM captures one comparison literal"); ++m_pipeline_drained_fetches; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "direct_cmpm_branch", [this, tc, value, literal](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return; expect((value == literal) == tc && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == 10, "TC/NTC branch consumes the retained CMPM result"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "direct_cmpm_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "CMPM and its branch do not write data memory"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254030;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_bitfield_ar7(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const short_compare = profile_is("cmpm-branch");
	bool const compare = short_compare || profile_is("cmpm-absolute");
	bool const peripheral = profile_is("bitf-peripheral"), offset = peripheral && (variant / 32) % 2;
	u16 const value = std::array<u16, 4>{0, 1, u16(compare ? 0x7f : 2), 0x8000}[variant % 4], mask = (variant / 16) % 2 ? 0x7f : peripheral ? 2 : 1;
	bool const tc = (variant / 4) % 2, initial_tc = (variant / 8) % 2;
	u32 const base = (short_compare ? (variant / 32) % 2 : peripheral ? (variant / 64) % 2 : variant % 2) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	std::array<u16, 7> const words{u16(short_compare ? 0x6081 : compare ? 0x60f8 : offset ? 0x61e1 : 0x61f8), u16(compare ? 0x3762 : peripheral ? offset ? 1 : 0x49 : 0x0017), mask, u16(0xf820 | (tc << 4)), 0x0700, 0xe811, 0xf4e1};
	for (unsigned i = 0; i < words.size(); ++i)
		if (!short_compare || i != 1) program.write_word(base + 0x600 + i - (short_compare && i > 1), words[i]);
	program.write_word(base + 0x700, 0xe855); program.write_word(base + 0x701, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608 | (initial_tc ? 0x1000 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR7, value);
	if (compare) m_cpu->space(AS_DATA).write_word(0x3762, value);
	if (short_compare) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x3762);
	if (peripheral) { m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x48); m_cpu->space(AS_DATA).write_word(0x49, value); }
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x5678);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	if (compare)
	{
		m_fast_stack_reads = 0;
		m_operand_data_tap = program.install_read_tap(base + 0x601, base + (short_compare ? 0x601 : 0x602), "cmpm_literal_ownership",
			[this, base, mask, short_compare](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
				expect(m_fast_stack_reads < (short_compare ? 1 : 2) && at == base + 0x601 + m_fast_stack_reads && word == (short_compare || m_fast_stack_reads ? mask : 0x3762), "CMPM captures its comparison literal after any address extension exactly once"); ++m_fast_stack_reads; });
	}
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "bitfield_ar7_sample",
		[this, value, compare, peripheral, short_compare](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == (compare ? 0x3762 : peripheral ? 0x49 : 0x17) && word == value && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == (short_compare ? 6 : peripheral ? 8 : 7),
				"Original BITF family samples mapped AR7 once before TC publication"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "bitfield_ar7_target",
		[this, value, tc, mask, compare, peripheral, short_compare](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return;
			if (m_cpu->pipeline_cycles() != (short_compare ? 9 : peripheral ? 11 : 10)) osd_printf_info("Mapped BITF target timing: cycle=%llu value=%04x mask=%04x tc_condition=%u\n",
				(unsigned long long)m_cpu->pipeline_cycles(), value, mask, tc);
			expect((compare ? value == mask : bool(value & mask)) == tc && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == (short_compare ? 9 : peripheral ? 11 : 10),
				"Immediate TC/NTC branch consumes BITF's published mask result"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "bitfield_ar7_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "BITF/branch does not write its register source"); });
	m_phase = 253030; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::shifted_memory_load_case(unsigned variant)
{
	auto f = unsigned_load_case(variant);
	f.opcode = (f.opcode & 0x01ff) | 0x4400;
	s64 const value = BIT(f.st1, 8) ? s16(f.memory) : f.memory;
	f.result = u64(value * 0x10000) & 0xffffffffffULL;
	return f;
}

tms320c54x_test_state::immediate_arithmetic_fixture tms320c54x_test_state::immediate_arithmetic_case(unsigned variant)
{
	bool const producer = variant >= 27648;
	unsigned const v = producer ? (variant - 27648) % 48 : variant % 9216;
	bool const add = !producer && v >= 8704;
	unsigned const u = add ? v - 8704 : v;
	unsigned const shift = producer ? 0 : add ? 16 : u % 17;
	unsigned const selector = producer || add ? u % 4 : (u / 17) % 4;
	unsigned const pattern = producer ? (u / 4) % 4 : add ? (u / 4) % 8 : (u / 68) % 8;
	unsigned const flags = producer ? 1 : add ? (u / 32) % 8 : (u / 544) % 8;
	unsigned const gap = producer ? (u / 16) % 3 : 0;
	u16 const literal = producer ? 1 : std::array<u16, 8>{1, 1, 0xffff, 1, 0x8000, 0xffff, 0xf495, 0xffff}[pattern];
	u64 const source = producer ? u64(s64(s16(std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern]))) & 0xffffffffffULL :
		std::array<u64, 8>{0, 0xffffffff, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x56783933, 0x80aa001234ULL}[pattern];
	u16 const st0 = 0x1804 | (flags & 4 ? 0x600 : 0), st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0);
	s64 const left = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
	s64 const right = (flags & 1 ? s64(s16(literal)) : s64(literal)) * (s64(1) << shift);
	s64 const sum = add ? left + right : left - right;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = add ? u64(u32(source)) + u32(right) > 0xffffffffULL : u32(source) >= u32(right);
	u16 const result_st0 = (st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? (selector & 1 ? 0x200 : 0x400) : 0);
	u64 const result = u64((flags & 2) && overflow ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
	u16 const opcode = (shift == 16 ? 0xf060 | !add : add ? 0xf000 : 0xf010) | ((selector >> 1) << 9) | ((selector & 1) << 8) | (shift == 16 ? 0 : shift);
	u32 const base = (producer ? selector & 1 : add ? (u / 256) % 2 : (u / 4352) % 2) ? 0x20000 : 0;
	return {selector, shift, flags, gap, producer ? 9 + gap : 7, producer, add, opcode, literal, st0, st1, result_st0, source, result, base};
}

void tms320c54x_test_state::start_pipeline_immediate_arithmetic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = immediate_arithmetic_case(variant);
	u32 const sub_pc = f.base + 0x600 + (f.producer ? 2 + f.gap : 0);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.producer) { program.write_word(f.base + 0x600, 0xf020 | ((f.selector >> 1) << 8)); program.write_word(f.base + 0x601, f.source); }
	program.write_word(sub_pc, f.opcode); program.write_word(sub_pc + 1, f.literal);
	if (f.producer) program.write_word(sub_pc + 2, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.producer || (f.selector & 2) ? 0x1111222233ULL : f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.producer || !(f.selector & 2) ? 0x4444555566ULL : f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "immediate_arithmetic_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Immediate arithmetic does not read data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "immediate_arithmetic_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Immediate arithmetic modifies only accumulator/status"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, sub_pc + 1, "immediate_arithmetic_literal",
		[this, f, sub_pc](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled() || (at != f.base + 0x601 && at != sub_pc + 1)) return;
			bool const producer_literal = f.producer && !m_pipeline_extensions_reads;
			expect(at == (producer_literal ? f.base + 0x601 : sub_pc + 1) && word == (producer_literal ? u16(f.source) : f.literal) &&
				m_pipeline_extensions_reads < (f.producer ? 2U : 1U), "Immediate arithmetic retains each literal once, without executing opcode-shaped data"); ++m_pipeline_extensions_reads; });
	m_phase = 253110; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::register_arithmetic_fixture tms320c54x_test_state::register_arithmetic_case_full(unsigned variant)
{
	bool const producer = variant >= 196608;
	unsigned const v = producer ? (variant - 196608) % 96 : variant % 65536;
	unsigned const operation = v % 2, selector = producer ? (v / 2) % 4 : (v / 64) % 4;
	unsigned const pattern = producer ? (v / 8) % 4 : (v / 256) % 8;
	unsigned const flags = producer ? 1 : (v / 2048) % 8, gap = producer ? (v / 32) % 3 : 0;
	bool const asm_shift = !producer && (v / 16384) % 2;
	int const shift = producer ? 0 : int((v / 2) % 32) - 16;
	std::array<u64, 2> initial = producer ? std::array<u64, 2>{0x10, 0x20} : std::array<u64, 2>{
		std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x80aa001234ULL, 0xffffffff55ULL}[pattern],
		std::array<u64, 8>{1, 0xffffffff, 0xff80000000ULL, 0x7fffffff, 0xff7fffffffULL, 0x017fffffffULL, 0x123456789aULL, 0x80000000}[pattern]};
	auto before = initial;
	if (producer) before[selector >> 1] = u64(s64(s16(std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern]))) & 0xffffffffffULL;
	u64 const source = before[selector >> 1];
	u64 operand = 0;
	for (int bit = 0; bit < 40; ++bit)
	{
		int const source_bit = bit - shift;
		if ((source_bit >= 0 && source_bit < 40 && BIT(source, source_bit)) ||
			(shift < 0 && source_bit >= 40 && (flags & 1) && BIT(source, 39))) operand |= u64(1) << bit;
	}
	u64 const destination = before[selector & 1];
	s64 const left = BIT(destination, 39) ? s64(destination) - (s64(1) << 40) : s64(destination);
	s64 const right = BIT(operand, 39) ? s64(operand) - (s64(1) << 40) : s64(operand);
	s64 const sum = operation ? left - right : left + right;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = operation ? u32(destination) >= u32(operand) : u64(u32(destination)) + u32(operand) > 0xffffffffULL;
	u16 const st0 = 0x1804 | (flags & 4 ? 0x600 : 0);
	u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (asm_shift ? shift & 31 : 0x13);
	u16 const result_st0 = (st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? (selector & 1 ? 0x200 : 0x400) : 0);
	auto after = before;
	after[selector & 1] = u64((flags & 2) && overflow ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
	u16 const opcode = (asm_shift ? 0xf480 | operation : 0xf400 | (operation << 5) | (shift & 31)) | ((selector >> 1) << 9) | ((selector & 1) << 8);
	u32 const base = (producer ? selector & 1 : (v / 32768) % 2) ? 0x20000 : 0;
	return {selector, gap, producer ? 8 + gap : 6, producer, opcode, st0, st1, result_st0, initial, before, after, base};
}

void tms320c54x_test_state::start_pipeline_accumulator_arithmetic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	if (!variant) { m_register_bank_coverage = {}; m_register_bank_replays = 0; }
	auto const f = variant >= 196896 ? register_load_case(variant - 196896) : register_arithmetic_case_full(variant);
	u32 const opcode_pc = f.base + 0x600 + (f.producer ? 2 + f.gap : 0);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.producer) { program.write_word(f.base + 0x600, 0xf020 | ((f.selector >> 1) << 8)); program.write_word(f.base + 0x601, f.before[f.selector >> 1]); }
	program.write_word(opcode_pc, f.opcode); if (f.producer) program.write_word(opcode_pc + 1, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.initial[0]); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.initial[1]);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_extensions_reads = m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "register_alu_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Register ALU does not read data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "register_alu_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Register ALU publishes only accumulator/status"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, opcode_pc, "register_alu_fetch",
		[this, f, opcode_pc](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == opcode_pc) { expect(word == f.opcode && !m_pipeline_extensions_reads, "Register ALU fetches its one-word opcode once"); ++m_pipeline_extensions_reads; }
			else if (f.producer && at == f.base + 0x601) { expect(word == u16(f.before[f.selector >> 1]) && !m_pipeline_operand_reads, "The preceding load retains its one literal"); ++m_pipeline_operand_reads; } });
	m_phase = 253130; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::memory_multiply_fixture tms320c54x_test_state::memory_multiply_case(unsigned variant, bool square)
{
	if (square)
	{
		auto f = memory_multiply_case(variant + 55296);
		f.operand.opcode = 0x2600 | (f.operand.opcode & 0x1ff);
		s64 const value = s16(f.operand.memory);
		s64 const product = value * value * (BIT(f.operand.st1, 6) ? 2 : 1);
		bool const overflow = product > 2147483647LL;
		f.result = u64(overflow && BIT(f.operand.st1, 9) ? 2147483647LL : product);
		f.result_st0 = f.operand.st0 | (overflow ? (BIT(f.operand.opcode, 8) ? 0x200 : 0x400) : 0);
		return f;
	}
	if (variant >= 165888)
	{
		auto f = memory_multiply_case(variant - 165888);
		f.operand.opcode = 0x3000 | u8(f.operand.opcode);
		f.result_st0 = f.operand.st0;
		return f;
	}
	if (variant >= 110592)
	{
		unsigned const v = variant - 110592;
		auto f = memory_multiply_case(v + 55296);
		unsigned const operation = v < 30720 ? (v % 10240 / 10) % 4 : ((v - 30720) % 8192 / 16) % 4;
		f.operand.opcode = (operation & 2 ? 0x2200 : 0x2000) | (f.operand.opcode & 0x1ff);
		s64 product = s64(s16(f.t)) * s16(f.operand.memory) * (BIT(f.operand.st1, 6) ? 2 : 1);
		if (operation & 2) product = (product + 32768) & ~s64(65535);
		bool const overflow = product < -2147483648LL || product > 2147483647LL;
		f.result = u64(overflow && BIT(f.operand.st1, 9) ? product < 0 ? -2147483648LL : 2147483647LL : product) & 0xffffffffffULL;
		f.result_st0 = f.operand.st0 | (overflow ? (BIT(f.operand.opcode, 8) ? 0x200 : 0x400) : 0);
		return f;
	}
	if (variant >= 55296)
	{
		unsigned const v = variant - 55296;
		auto f = memory_multiply_case(v);
		unsigned const operation = v < 30720 ? (v % 10240 / 10) % 4 : ((v - 30720) % 8192 / 16) % 4;
		f.operand.opcode = 0x2400 | ((operation & 1) << 8) | u8(f.operand.opcode);
		f.t = operation & 2 ? 0xffff : 0x8000;
		if (f.operand.address == 0x0e) f.operand.memory = f.t;
		s64 const product = s64(f.t) * f.operand.memory * (BIT(f.operand.st1, 6) ? 2 : 1);
		bool const overflow = product > 2147483647LL;
		f.result = u64(overflow && BIT(f.operand.st1, 9) ? 2147483647LL : product);
		f.result_st0 = f.operand.st0 | (overflow ? (BIT(f.operand.opcode, 8) ? 0x200 : 0x400) : 0);
		return f;
	}
	bool const mapped = variant >= 30720;
	unsigned const v = mapped ? (variant - 30720) % 8192 : variant % 10240;
	unsigned const operation = mapped ? (v / 16) % 4 : (v / 10) % 4;
	unsigned const pattern = mapped ? (v / 64) % 8 : (v / 40) % 8, flags = mapped ? (v / 512) % 16 : (v / 320) % 16;
	unsigned const kind = v % 10, reg = (v / 5120) % 2 ? 6 : 1;
	auto f = memory_arithmetic_case(std::array<unsigned, 10>{0, 1, 2, 3, 4, 5, 14, 15, 16, 17}[kind] + 18 * reg + 576 * pattern);
	s32 const high = std::array<s32, 8>{0, 1, -1, 32767, -32768, 65535, -65536, 0x1234}[pattern];
	u64 const a = (pattern & 1 ? 0xfe00000000ULL : 0) | ((u64(high) & 0x1ffff) << 16) | (0x1234 + pattern);
	u64 const b = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1234567833ULL, 0x80aa001234ULL}[pattern];
	if (mapped)
	{
		f.form = 17; f.reg = f.selected = 1; f.read_cycle = 6; f.address = 0x08 + v % 16; f.literal = f.address; f.next_ar = 0x5007;
		f.opcode = (0x3100 + operation * 0x200) | 0xf8;
		f.memory = f.address <= 0x0a ? u16(a >> ((f.address - 8) * 16)) : f.address <= 0x0d ? u16(b >> ((f.address - 0x0b) * 16)) :
			f.address == 0x0e ? 0x9abc : f.address == 0x0f ? 0x1357 : 0x5007;
	}
	else
	{
		f.opcode = (0x3100 + operation * 0x200) | u8(f.opcode);
		if (f.form < 2) { f.opcode = (f.opcode & 0xff00) | (0x0b + (reg == 6)); f.address = (f.form ? 0x1000 : 0x200) + u8(f.opcode); }
		f.memory = std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8000, 0xffff, 0x5678}[pattern];
	}
	f.st0 = 0x1004 | (pattern & 1 ? 0x600 : 0) | (pattern & 2 ? 0x800 : 0);
	f.st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 8 ? 0x100 : 0) | (f.form == 1 ? 0x4000 : 0);
	s64 product = s64(high) * s16(f.memory) * (flags & 1 ? 2 : 1);
	if (operation && (flags & 7) == 7) product = std::clamp(product, s64(-2147483648LL), s64(2147483647LL));
	s64 const signed_b = BIT(b, 39) ? s64(b) - (s64(1) << 40) : s64(b);
	s64 result = !operation ? product : operation == 1 ? signed_b - product : signed_b + product;
	if (operation == 3) result = (result + 32768) & ~s64(65535);
	bool const overflow = result < -2147483648LL || result > 2147483647LL;
	u64 const stored = u64(overflow && (flags & 2) ? (result < 0 ? -2147483648LL : 2147483647LL) : result) & 0xffffffffffULL;
	return {f, a, b, stored, u16(f.st0 | (overflow ? 0x200 : 0)), u16(flags & 4 ? 2 : 0)};
}

void tms320c54x_test_state::start_pipeline_memory_multiply(unsigned variant)
{
	unsigned const raw = variant % 55296;
	if (raw < 30720 && raw % 10 == 5) ++variant; // Omit write-only short MOD=3; keep mapped cases and raw IDs.
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = memory_multiply_case(variant, profile_is("squr-smem")); auto const &o = f.operand;
	u32 const base = o.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, o.opcode); if (o.form >= 14) program.write_word(base + 0x601, o.literal);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, o.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, o.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, o.bk);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5007);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x1357);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	if (o.address >= 0x60) m_cpu->space(AS_DATA).write_word(o.address, o.memory);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "memory_multiply_source",
		[this, o](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == o.address && word == o.memory && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == o.read_cycle, "Memory multiply retains one Read sample"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "memory_multiply_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Memory multiply does not write data memory"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "memory_multiply_literal",
		[this, o](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || o.form < 14) return;
			expect(word == o.literal && !m_pipeline_extensions_reads, "Memory multiply retains its offset/address once"); ++m_pipeline_extensions_reads; });
	m_phase = 253150; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

std::array<u64, 5> tms320c54x_test_state::t_multiply_pair_case(unsigned variant)
{
	unsigned const v = variant % 256;
	u16 const t = std::array<u16, 8>{0, 1, 0xffff, 0x8000, 0x7fff, 0x1234, 0xfff0, 0x8001}[v % 8];
	u16 const memory = std::array<u16, 4>{1, 0xffff, 0x8000, 0x7fff}[(v / 8) % 4];
	bool const destination = (v / 32) % 2, frct = (v / 64) % 2, ovm = (v / 128) % 2;
	s64 const product = s64(s16(t)) * s16(memory) * (frct ? 2 : 1);
	bool const overflow = product < -2147483648LL || product > 2147483647LL;
	return {t, memory, u64(overflow && ovm ? std::clamp(product, s64(-2147483648LL), s64(2147483647LL)) : product) & 0xffffffffffULL,
		u64(0x1800 | (overflow ? destination ? 0x200 : 0x400 : 0)), u64(0x800 | (frct ? 0x40 : 0) | (ovm ? 0x200 : 0))};
}

void tms320c54x_test_state::start_pipeline_t_multiply_pair(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = t_multiply_pair_case(variant); bool const destination = (variant % 256 / 32) % 2;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0x30e1); program.write_word(0x601, 4);
	program.write_word(0x602, 0x20e1 | (destination << 8)); program.write_word(0x603, 3);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1800); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f[4]);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x5007); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x9abc);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x87654321);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->space(AS_DATA).write_word(0x500b, f[0]); m_cpu->space(AS_DATA).write_word(0x500a, f[1]);
	m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "t_multiply_pair_read",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_operand_reads < 2 && at == (m_pipeline_operand_reads ? 0x500a : 0x500b) && word == f[m_pipeline_operand_reads] && m_cpu->pipeline_cycles() == (m_pipeline_operand_reads ? 8 : 6), "LD T followed by MPY samples each source once"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "t_multiply_pair_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "LD T/MPY has no data writes"); });
	m_pipeline_fetch_tap = program.install_read_tap(0x601, 0x603, "t_multiply_pair_extensions",
		[this](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled() || at == 0x602) return;
			expect(at == (m_pipeline_extensions_reads ? 0x603 : 0x601) && word == (m_pipeline_extensions_reads ? 3 : 4), "LD T/MPY retains independent address extensions"); ++m_pipeline_extensions_reads; });
	m_phase = 253190; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::shifted_arithmetic_fixture tms320c54x_test_state::shifted_arithmetic_case(unsigned variant, bool bk)
{
	if (bk)
	{
		auto f = shifted_arithmetic_case(147456 + (variant % 4096) * 8 + ((variant / 4096) % 3) * 32768);
		f.operand.address = f.operand.literal = 0x19;
		f.operand.reg = variant / 12288 ? 6 : 1;
		return f;
	}
	if (variant >= 245760)
	{
		unsigned const n = variant - 245760; bool const mapped = n >= 18432;
		unsigned const v = mapped ? (n - 18432) % 4096 : n % 6144;
		unsigned const destination = mapped ? (v / 8) % 2 : (v / 12) % 2;
		unsigned const pattern = mapped ? (v / 16) % 8 : (v / 24) % 8;
		unsigned const memory_pattern = mapped ? (v / 128) % 8 : (v / 192) % 8;
		unsigned const flags = mapped ? (v / 1024) % 4 : (v / 1536) % 4;
		auto o = memory_arithmetic_case(std::array<unsigned, 6>{0, 1, 4, 14, 15, 17}[v % 6] + 18 * ((v / 6) % 2 ? 6 : 1) + 576 * pattern + 4608 * flags);
		o.opcode = 0x1e00 | destination << 8 | u8(o.opcode);
		o.memory = std::array<u16, 8>{0, 1, 0x7fff, 0x8000, 0xffff, 7, 2, 0x1234}[memory_pattern];
		if (mapped) { o.form = 17; o.reg = o.selected = 1; o.address = 0x10 + v % 8; o.literal = o.address; o.opcode = 0x1ef8 | destination << 8; o.read_cycle = 6; o.st1 &= ~0x4000; }
		u64 const source = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1fffffff, 0xffffffffULL}[pattern];
		s64 const dividend = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
		s64 const divisor = flags & 1 ? s16(o.memory) : s64(o.memory);
		s64 const difference = dividend - divisor * 32768;
		bool const subtract = difference >= 0, overflow = difference < -2147483648LL || difference > 2147483647LL;
		u16 const st0 = (o.st0 & ~0x800) | (subtract ? 0x800 : 0) | (overflow ? destination ? 0x200 : 0x400 : 0);
		u64 const result = u64(subtract ? difference * 2 + 1 : dividend * 2) & 0xffffffffffULL;
		return {o, u16(destination << 8), st0, destination ? 0x12345678 : source, destination ? source : 0x12345678, result};
	}
	bool const mapped = variant >= 147456;
	unsigned const v = mapped ? (variant - 147456) % 32768 : variant % 49152;
	unsigned const selector = mapped ? (v / 8) % 8 : (v / 12) % 8;
	unsigned const pattern = mapped ? (v / 64) % 4 : (v / 96) % 4;
	unsigned const flags = mapped ? (v / 256) % 4 : (v / 384) % 4;
	int const shift = int(mapped ? (v / 1024) % 32 : (v / 1536) % 32) - 16;
	auto o = memory_arithmetic_case(std::array<unsigned, 6>{0, 1, 4, 14, 15, 17}[v % 6] + 18 * ((v / 6) % 2 ? 6 : 1) + 576 * pattern + 4608 * flags);
	o.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern];
	o.opcode = 0x6f00 | u8(o.opcode); ++o.read_cycle;
	if (mapped)
	{
		o.form = 17; o.reg = o.selected = 1; o.address = 0x10 + v % 8; o.literal = o.address;
		o.opcode = 0x6ff8; o.read_cycle = 7; o.st1 &= ~0x4000;
	}
	u64 const a = std::array<u64, 4>{0, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL}[pattern];
	u64 const b = std::array<u64, 4>{0xffffffffffULL, 0xff7fffffffULL, 0x12345678, 0xff80000000ULL}[pattern];
	u64 const source = selector & 2 ? b : a;
	s64 const value = flags & 1 ? s16(o.memory) : s64(o.memory);
	u64 const memory = u64(value) & 0xffffffffffULL;
	u64 const shifted = shift >= 0 ? (memory << shift) & 0xffffffffffULL : flags & 1 ? u64(value >> -shift) & 0xffffffffffULL : memory >> -shift;
	s64 const left = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
	s64 const right = BIT(shifted, 39) ? s64(shifted) - (s64(1) << 40) : s64(shifted);
	s64 const sum = selector & 4 ? left - right : left + right;
	bool const carry = selector & 4 ? u32(source) >= u32(shifted) : u64(u32(source)) + u32(shifted) > 0xffffffffULL;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	u16 const st0 = (o.st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? selector & 1 ? 0x200 : 0x400 : 0);
	u64 const result = u64(overflow && (flags & 2) ? std::clamp(sum, s64(-2147483648LL), s64(2147483647LL)) : sum) & 0xffffffffffULL;
	return {o, u16(0x0c00 | ((selector & 3) << 8) | ((selector & 4) << 3) | (shift & 31)), st0, a, b, result};
}

void tms320c54x_test_state::start_pipeline_shifted_arithmetic(unsigned variant)
{
	if (!variant) { m_shifted_coverage = {}; m_shifted_replays = 0; }
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const bk = profile_is("shifted-bk");
	auto const f = shifted_arithmetic_case(variant, bk); auto const &o = f.operand;
	bool const subc = variant >= 245760;
	u32 const base = o.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, o.opcode);
	if (o.form >= 14) program.write_word(base + 0x601, o.literal);
	if (!subc) program.write_word(base + 0x601 + (o.form >= 14), f.extension);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, o.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, o.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, o.bk);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5007);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1122); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x3344);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x9abc);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	if (o.address >= 0x60) m_cpu->space(AS_DATA).write_word(o.address, o.memory);
	else m_cpu->set_state_int(bk ? tms320c54x_device::STATE_BK : tms320c54x_device::STATE_AR0 + o.address - 0x10, o.memory);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "shifted_arithmetic_read",
		[this, o](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == o.address && word == o.memory && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == o.read_cycle, "Shifted ADD/SUB samples one retained RAM or mapped AR source"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "shifted_arithmetic_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Shifted ADD/SUB has no data writes"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601 + (!subc && o.form >= 14), "shifted_arithmetic_extensions",
		[this, f, o, base, subc](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled() || (subc && o.form < 14)) return;
			bool const operation = !subc && at == base + 0x601 + (o.form >= 14);
			expect(word == (operation ? f.extension : o.literal) && m_pipeline_extensions_reads == unsigned(operation && o.form >= 14), "Shifted arithmetic retains ordered address and operation extensions"); ++m_pipeline_extensions_reads; });
	m_phase = 253210; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

std::array<u64, 2> tms320c54x_test_state::repeat_subc_result(memory_repeat_fixture const &f, unsigned iterations)
{
	u64 accumulator = f.dividend; u16 flags = 0x1804;
	for (unsigned i = 0; i < iterations; ++i)
	{
		s64 const dividend = BIT(accumulator, 39) ? s64(accumulator) - (s64(1) << 40) : s64(accumulator);
		s64 const divisor = f.flags & 1 ? s16(f.divisor) : s64(f.divisor);
		s64 const difference = dividend - divisor * 32768;
		bool const subtract = difference >= 0;
		flags = (flags & ~0x800) | (subtract ? 0x800 : 0);
		if (difference < -2147483648LL || difference > 2147483647LL) flags |= f.destination ? 0x200 : 0x400;
		accumulator = u64(subtract ? difference * 2 + 1 : dividend * 2) & 0xffffffffffULL;
	}
	return {accumulator, flags};
}

tms320c54x_test_state::rptbd_shift_store_fixture tms320c54x_test_state::rptbd_shift_store_case(unsigned v)
{
	rptbd_shift_store_fixture f{};
	f.count = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4]; v /= 4;
	f.form = v % 5; v /= 5; f.kind = v % 4; v /= 4;
	f.shift = std::array<int, 3>{-16, -1, 15}[v % 3]; v /= 3;
	f.flags = v % 4; v /= 4;
	f.source = v % 2 ? 0xff7fff1200ULL : 0x0180001200ULL; v /= 2;
	f.base = v % 2 ? 0x20000 : 0; v /= 2;
	f.compiler = v % 2; v /= 2; f.irq = v % 5; v /= 5;
	f.checkpoint = std::array<unsigned, 3>{4, 8, 12}[v];
	f.mode = std::array<u16, 5>{3, 0x83, 0x8b, 0x93, 0x13}[f.form];
	f.st0 = 0xa600 | (f.form == 4 ? 0 : 8);
	f.st1 = (f.compiler ? 0x4000 : 0) | (f.flags & 2 ? 0x100 : 0) | (f.irq ? 0 : 0x800);
	f.address = f.form >= 1 && f.form <= 3 ? 0x5000 : f.compiler ? 0x3000 + f.mode : (f.st0 & 0x1ff) * 128 + f.mode;
	f.initial_ar3 = f.form >= 1 && f.form <= 3 ? 0x5000 : 0x7103;
	u64 bits = f.source;
	for (int i = 0; i < f.shift; ++i) bits = (bits * 2) & 0xffffffffffULL;
	for (int i = 0; i < -f.shift; ++i) bits = (bits >> 1) | ((f.flags & 2) && (bits & 0x8000000000ULL) ? 0x8000000000ULL : 0);
	u32 stored = u32(bits);
	if (f.flags & 1)
	{
		if (f.flags & 2) stored = u32(std::clamp(s64(bits << 24) >> 24, s64(-2147483648LL), s64(2147483647LL)));
		else if (bits > 0xffffffffULL) stored = 0xffffffffU;
	}
	f.value = stored >> (f.kind & 2 ? 16 : 0);
	f.final_ar3 = f.address == 0x13 ? f.value : f.initial_ar3 + (f.form == 3 ? 1 : f.form == 2 ? -1 : 0);
	return f;
}

void tms320c54x_test_state::start_rptbd_shift_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = rptbd_shift_store_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, 0x605);
	program.write_word(f.base + 0x602, 0x6f00 | f.mode);
	program.write_word(f.base + 0x603, (f.kind & 2 ? 0x0c60 : 0x0c80) | ((f.kind & 1) << 8) | (f.shift & 31));
	program.write_word(f.base + 0x604, 0xe800 | ((f.kind & 1) << 8)); program.write_word(f.base + 0x605, 0x6d94);
	program.write_word(f.base + 0x60f, 0xf4e1);
	int const line = f.irq == 2 || f.irq == 4 ? INPUT_LINE_NMI : 2;
	u32 const vector = f.base + (line == INPUT_LINE_NMI ? 4 : 0x48);
	program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	if (f.address >= 0x60) data.write_word(f.address, 0xdead);
	m_cpu->set_pipeline_frontend(false); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.flags & 1); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.kind & 1 ? 0x123456789aULL : 0x11112222);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.kind & 1 ? 0x11112222 : 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq && line == 2 ? 4 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 13);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 3 ? f.initial_ar3 : i == 4 ? 0x6000 : 0x7000 + i);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_rptbd_counts = {}; m_rptbd_return = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x603, "rptbd_shift_words", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_rptbd_counts.fetches++;
		expect(i < 3 && at == f.base + 0x601 + i && word == (i == 0 ? 0x605 : i == 1 ? 0x6f00 | f.mode : (f.kind & 2 ? 0x0c60 : 0x0c80) | ((f.kind & 1) << 8) | (f.shift & 31)), "RPTBD retains its endpoint and complete shifted-store delay exactly once"); });
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "rptbd_shift_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.irq && at == 0x2fff && !m_rptbd_counts.stack_reads++ && m_rptbd_counts.stack_writes == 1 && word == m_rptbd_return, "Shifted store has no destination read; ISR reads one original continuation"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "rptbd_shift_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.irq && !m_rptbd_counts.stack_writes++ && word >= 0x604 && word <= 0x60f && m_rptbd_counts.delay_writes == 1, "Interrupt stacks one continuation after the complete delay store"); m_rptbd_return = word; return; }
		expect(at == f.address && word == f.value && !m_rptbd_counts.delay_writes++ && m_cpu->pipeline_cycles() == 9, "Shifted store publishes the Execute-live source to its retained delay address once"); });
	m_phase = 310400; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_memory_immediate_multiply(unsigned variant)
{
	if (!variant) { m_memory_immediate_coverage = {}; m_memory_immediate_restores = 0; }
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = memory_immediate_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, (f.accumulate ? 0x6400 : 0x6200) | (f.bsrc ? 0x200 : 0) | (f.bdst ? 0x100 : 0) | f.mode);
	program.write_word(f.base + 0x601, f.words == 3 ? f.extension : f.literal); if (f.words == 3) program.write_word(f.base + 0x602, f.literal); program.write_word(f.base + 0x600 + f.words, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x9abc);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x140c); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.form == 7 ? 13 : 0); data.write_word(f.address, f.word);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "memory_immediate_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == f.address && word == f.word && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read, "Memory/immediate MPY retains one Read-owned source"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "memory_immediate_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Memory/immediate MPY has no data-memory write"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x600 + f.words - 1, "memory_immediate_literals", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_extensions_reads++; expect(i < f.words - 1 && at == f.base + 0x601 + i && word == (f.words == 3 && !i ? f.extension : f.literal), "MPY retains address then literal exactly once"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254340;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_multiply_case(unsigned variant, bool mac, bool rounded)
{
	unsigned const group = mac || rounded ? variant / 18432 : 0;
	variant %= 18432;
	unsigned const kind = variant % 6, form = (variant / 6) % 4, destination = (variant / 24) % 2, pair = (variant / 48) % 16, flags = (variant / 768) % 4;
	u16 const memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pair % 4], t = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pair / 4];
	u32 const caller = (variant / 3072) % 2 ? 0x20000 : 0, target = kind == 4 ? caller ^ 0x20000 : caller;
	s64 const product = s64(s16(memory)) * s16(t) * (flags & 1 ? 2 : 1);
	u64 const result = u64(product > 0x7fffffffLL && (flags & 2) ? 0x7fffffffLL : product) & 0xffffffffffULL;
	unsigned const read = delay_multiply_fixture::operand_read(kind, form);
	delay_multiply_fixture f{kind, form, destination, flags, std::array<unsigned, 3>{3, read - 1, read}[variant / 6144], memory, t,
		std::array<u16, 4>{0x5000, 0x5000, u16(mac ? 0x15 : 0x17), 0x5003}[form], caller, target,
		kind == 1 ? 0 : kind == 2 ? 1 : 0x123456789aULL, 0xfedcba9876ULL, result};
	if (mac)
	{
		f.accumulate = true; f.operation = group % 4; f.flags |= group / 4 ? 4 : 0;
		s64 const value = f.arithmetic();
		f.result = u64(f.overflow() && (f.flags & 2) ? std::clamp(value, s64(-2147483648LL), s64(2147483647LL)) : value) & 0xffffffffffULL;
	}
	if (rounded)
	{
		f.rounded = true; f.flags |= group ? 4 : 0;
		s64 const value = f.arithmetic();
		f.result = u64(f.flags & 2 ? std::clamp(value, s64(-2147483648LL), s64(2147483647LL)) : value) & 0xffffffffffULL;
	}
	return f;
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_accumulator_multiply_case(unsigned variant)
{
	unsigned v = variant;
	delay_multiply_fixture f{};
	f.kind = v % 6; v /= 6; f.form = v % 3; v /= 3;
	f.operation = v % 4; v /= 4; unsigned const pattern = v % 8; v /= 8;
	f.flags = v % 16; v /= 16; f.caller = v % 2 ? 0x20000 : 0; v /= 2;
	f.target = f.kind == 4 ? f.caller ^ 0x20000 : f.caller;
	f.accumulator_source = true; f.destination = 1; f.t = 0x5aa5;
	f.source = f.form ? 0x5000 : 0x1062;
	s32 const high = std::array<s32, 8>{0, 1, -1, 32767, -32768, 65535, -65536, 0x1234}[pattern];
	f.a = f.kind == 1 ? 0 : f.kind == 2 ? 1 : (pattern & 1 ? 0xfe00000000ULL : 0) | ((u64(high) & 0x1ffff) << 16) | (0x1234 + pattern);
	f.b = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1234567833ULL, 0x80aa001234ULL}[pattern];
	f.memory = std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8000, 0xffff, 0x5678}[pattern];
	f.checkpoint = std::array<unsigned, 3>{3, f.read() - 1, f.read()}[v];
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::bd_long_accumulator_case(unsigned v)
{
	delay_multiply_fixture f{};
	f.extended_accumulator = f.accumulator_source = true; f.destination = 1;
	f.form = v % 3; v /= 3; f.address_ar = v % 8; v /= 8;
	f.operation = v % 4; v /= 4; unsigned const pattern = v % 8; v /= 8;
	f.flags = v % 16; v /= 16; f.caller = v % 2 ? 0x20000 : 0; v /= 2;
	f.target = f.caller; f.t = 0x5aa5;
	f.source = f.form == 2 ? 0x5000 : f.form ? 0x3b08 : 0x5003;
	s32 const high = std::array<s32,8>{0,1,-1,32767,-32768,65535,-65536,0x1234}[pattern];
	f.a = (pattern & 1 ? 0xfe00000000ULL : 0) | ((u64(high) & 0x1ffff) << 16) | (0x1234 + pattern);
	f.b = std::array<u64,8>{0,1,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x1234567833ULL,0x80aa001234ULL}[pattern];
	f.memory = std::array<u16,8>{0,1,0xffff,0x7fff,0x8000,0x8000,0xffff,0x5678}[pattern];
	f.checkpoint = std::array<unsigned,4>{3,5,8,9}[v % 4];
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result,s64(-2147483648LL),s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

void tms320c54x_test_state::start_delay_multiply(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const mac = profile_is("mac-delay");
	bool const square = profile_is("square-delay");
	bool const memory_square = profile_is("squr-delay");
	bool const long_acc = profile_is("bd-maca-long");
	bool const acc = square || memory_square || long_acc || profile_is("maca-delay");
	bool const rounded = profile_is("mpyr-delay");
	auto const f = long_acc ? bd_long_accumulator_case(variant) : memory_square ? delay_memory_square_case(variant) : square ? delay_square_case(variant) : acc ? delay_accumulator_multiply_case(variant) : delay_multiply_case(variant, mac, rounded); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.caller + 0x600 + i, 0xf495);
	program.write_word(f.caller + 0x600, f.kind == 4 ? 0xfb80 | (f.target >> 16) : std::array<u16, 6>{0xf273, 0xfa45, 0xfa45, 0xf274, 0, 0xfe00}[f.kind]);
	if (f.kind != 5) program.write_word(f.caller + 0x601, 0x700);
	program.write_word(f.caller + 0x600 + f.delay(), memory_square ? 0x2600 | (f.destination << 8) | (f.form == 0 ? 0x62 : f.form == 1 ? 0x82 : 0x92) : square ? (0x3800 | (f.operation << 9) | (f.destination << 8) | (f.form == 0 ? 0x62 : f.form == 1 ? 0x82 : 0x92)) : acc ? (0x3100 + f.operation * 0x200) | (f.form == 0 ? 0x62 : f.form == 1 ? 0x82 : 0x92) : (mac ? 0x2800 + f.operation * 0x200 : rounded ? 0x2200 : 0x2000) | (f.destination << 8) | (f.form == 0 ? rounded ? 0x92 : 0x82 : f.form == 3 ? 0xe2 : 0xf8));
	if (long_acc) program.write_word(f.caller + 0x602,(0x3100 + f.operation * 0x200) | (f.form == 2 ? 0xf8 : 0xe0 | f.address_ar));
	if (f.form && !acc) program.write_word(f.caller + 0x601 + f.delay(), f.form == 3 ? 3 : f.source);
	if (long_acc) program.write_word(f.caller + 0x603,f.extension());
	for (u32 at : {f.caller + 0x604, f.target + 0x700}) { program.write_word(at, 0x880f | (f.destination << 8)); program.write_word(at + 1, 0xf4e1); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.flags & 4 ? 2 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, acc ? 0xa020 : 0xa008); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, (acc ? 0x800 | (f.flags & 8 ? 0x100 : 0) : 0x900) | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x2468);
	if (long_acc) { m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b ^ 0xffffffffffULL); }
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); data.write_word(0x3000, 0x700); data.write_word(f.source, f.memory);
	if (long_acc) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.address_ar,0x5000);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "delay_multiply_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x3000) { expect(f.kind == 5 && word == 0x700 && !m_fast_stack_reads++, "RETD owns its separate one-word return frame"); return; } if (at != f.source || word != f.memory || m_pipeline_operand_reads || m_cpu->pipeline_cycles() != f.read()) osd_printf_info("delay_multiply_read: case=%u address=%04x word=%04x cycle=%llu expected=%u\n", m_pipeline_peripheral_case, unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles(), f.read()); expect(at == f.source && word == f.memory && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles() == f.read(), "Delay MPY reads one retained RAM or AR operand"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "delay_multiply_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff || at == 0x2ffe) { unsigned const i = m_pipeline_register_writes++; expect((f.kind == 3 || f.kind == 4) && i < (f.kind == 4 ? 2 : 1) && at == 0x2fff - i && word == (i ? f.caller >> 16 : 0x604), "Delayed call owns its continuation and optional caller-page frame"); return; } expect(at == 0x0f && word == u16(f.result) && !m_pipeline_store_writes++ && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() >= f.read() + 2, "The selected callee observes the completed multiply once"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.caller + 0x600, f.caller + 0x604, "delay_multiply_literal", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled() || (f.accumulator_source && !f.extended_accumulator) || (!f.form && !f.extended_accumulator) || at != f.caller + 0x601 + f.delay()) return; expect(word == f.extension() && !m_pipeline_extensions_reads++ && m_cpu->pipeline_cycles() == f.literal_clock(), "Delay multiply fetches its retained address literal once"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254540;
	m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

u16 tms320c54x_test_state::nibble_input(unsigned variant)
{
	return std::array<u16, 8>{0, 0xffff, 0xc410, 0x1234, 0x8001, 0xabcd, 0x00f0, 0xf00f}[(variant / 4) % 8];
}

void tms320c54x_test_state::start_original_nibble(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_cpu->set_pipeline_frontend(false);
	m_pipeline_peripheral_case = variant;
	u32 const base = variant & 64 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	// Original AA99 02:c62b..c641, relocated without changing its words.
	std::array<u16, 23> const source{0x4a11,0xeeff,0xf495,0x8000,0xe803,0xf6b8,0x0804,0x8004,0x7100,0x0011,0x6f04,0x0c82,0x1004,0xf484,0x880e,0xee01,0x1481,0x8a11,0xf030,0x000f,0xf495,0xf495,0xf4e4};
	for (unsigned i = 0; i < source.size(); ++i) program.write_word(base + 0x700 + i, source[i]);
	program.write_word(base + 0x600, 0xf020); program.write_word(base + 0x601, 0x2000);
	program.write_word(base + 0x602, 0xf980 | (base >> 16)); program.write_word(base + 0x603, 0x700); program.write_word(base + 0x604, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4800 | (variant & 32 ? 0x100 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1357);
	data.write_word(0x1400, variant % 4); data.write_word(0x2000, nibble_input(variant));
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0x1400, 0x1400, "nibble_count_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = data.install_read_tap(0x2000, 0x2000, "nibble_input_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = data.install_write_tap(0x1400, 0x1400, "nibble_count_write", [this, variant](offs_t, u16 &value, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const ordinal = m_pipeline_store_writes++;
		expect(ordinal < 2 && value == (3 - variant % 4) * (ordinal ? 4 : 1), "Original nibble helper preserves both real count stores");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 399900;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::immediate_multiply_fixture tms320c54x_test_state::immediate_multiply_case(unsigned variant, bool mac)
{
	bool const mac_source = mac && variant >= 86016;
	if (mac) variant %= 86016;
	bool const producer = variant >= 12288;
	unsigned const v = producer ? (variant - 12288) % 24576 : variant % 4096;
	unsigned const pattern = v % 8, literal_pattern = (v / 8) % 8;
	unsigned const selector = producer ? (v / 64) % 4 : (v / 64) % 2;
	unsigned const flags = producer ? (v / 256) % 16 : (v / 128) % 16;
	bool const source = producer && (selector & 2), destination = selector & 1;
	unsigned const gap = producer ? (v / 8192) % 3 : 0, start = producer ? 1 + gap : 0;
	u64 const value = std::array<u64, 8>{0, 1, 0xffff, 0x8000, 0x7fffffff, 0xff80000000ULL, 0x0180008000ULL, 0xff7fff8000ULL}[pattern];
	u16 t = std::array<u16, 8>{0, 1, 0xffff, 0x8000, 0x7fff, 0x1234, 0xff80, 0x7f00}[pattern];
	u16 const pmst = (flags & 8 ? 2 : 0) | (producer && (v / 4096) % 2 ? 1 : 0);
	if (producer)
	{
		u32 stored = u32(value);
		if (pmst & 1)
		{
			if (!(flags & 4)) stored = value > 0xffffffffULL ? 0xffffffffU : u32(value);
			else stored = u32(std::clamp(BIT(value, 39) ? s64(value) - (s64(1) << 40) : s64(value), s64(-2147483648LL), s64(2147483647LL)));
		}
		t = stored;
	}
	u16 const literal = std::array<u16, 8>{0, 1, 0xffff, 0x8000, 0x7fff, 0xf495, 0x5678, 0xff00}[literal_pattern];
	s64 const product = s64(s16(t)) * s16(literal) * (flags & 1 ? 2 : 1);
	bool const overflow = product < -2147483648LL || product > 2147483647LL;
	u16 const st0 = 0x1804 | ((pattern ^ literal_pattern) & 1 ? 0x600 : 0), st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 4 ? 0x100 : 0) | (producer && (pattern & 2) ? 0x4000 : 0);
	u64 const result = u64(overflow && (flags & 2) ? std::clamp(product, s64(-2147483648LL), s64(2147483647LL)) : product) & 0xffffffffffULL;
	unsigned const checkpoint_index = producer ? (variant - 12288) / 24576 : variant / 4096;
	immediate_multiply_fixture f{producer, source, destination, start, 6 + start, std::array<unsigned, 3>{4 + start, 5 + start, 6 + start}[checkpoint_index],
		t, literal, st0, st1, pmst, u16(st0 | (overflow ? destination ? 0x200 : 0x400 : 0)),
		producer ? source ? 0x2222 : value : 0x12345678, producer ? source ? value : 0x1111 : 0x87654321,
		result, producer ? pattern & 1 ? 0x20000U : 0 : (v / 2048) % 2 ? 0x20000U : 0};
	if (mac)
	{
		s64 p = product;
		if ((pmst & 2) && (flags & 1) && (flags & 2)) p = std::clamp(p, s64(-2147483648LL), s64(2147483647LL));
		u64 const accumulator = mac_source ? f.b : f.a;
		s64 const total = (BIT(accumulator, 39) ? s64(accumulator) - (s64(1) << 40) : s64(accumulator)) + p;
		bool const ov = total < -2147483648LL || total > 2147483647LL;
		f.result = u64(ov && (flags & 2) ? std::clamp(total, s64(-2147483648LL), s64(2147483647LL)) : total) & 0xffffffffffULL;
		f.result_st0 = st0 | (ov ? destination ? 0x200 : 0x400 : 0);
	}
	return f;
}

void tms320c54x_test_state::start_pipeline_immediate_multiply(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const mac = profile_is("mac-t-imm");
	auto const f = immediate_multiply_case(variant, mac); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.producer) program.write_word(f.base + 0x600, 0x880e | (f.source << 8));
	program.write_word(f.base + 0x600 + f.start, (mac ? 0xf067 | (variant >= 86016 ? 0x200 : 0) : 0xf066) | (f.destination << 8)); program.write_word(f.base + 0x601 + f.start, f.literal);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, f.producer ? 0x9abc : f.t);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "immediate_multiply_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Immediate MPY has no memory operand or destination read"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "immediate_multiply_producer_only",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(f.producer && at == 0x0e && word == f.t && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 6, "Only the optional STLM producer may publish one T write"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601 + f.start, f.base + 0x601 + f.start, "immediate_multiply_literal",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == f.literal && !m_pipeline_extensions_reads, "Immediate MPY retains its literal once even when opcode-shaped"); ++m_pipeline_extensions_reads; });
	m_phase = 253290; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::multiply_mapped_fixture tms320c54x_test_state::multiply_mapped_case(unsigned variant)
{
	unsigned const v = variant % 1536;
	u16 const t = std::array<u16, 4>{3, 0xffff, 0x8000, 0x7fff}[v % 4];
	u16 const literal = std::array<u16, 4>{3, 0xffff, 0x8000, 0x7fff}[(v / 4) % 4];
	bool const producer_b = (v / 16) % 2, consumer_b = (v / 96) % 2;
	unsigned const part = (v / 32) % 3, gap = (v / 192) % 2, flags = v / 384;
	s64 const product = s64(s16(t)) * s16(literal) * (flags & 1 ? 2 : 1);
	u64 const result = u64(flags & 2 ? std::clamp(product, s64(-2147483648LL), s64(2147483647LL)) : product) & 0xffffffffffULL;
	return {producer_b, consumer_b, gap, 6 + gap + variant / 1536, t, literal,
		u16((producer_b ? 0x0b : 0x08) + part), u16(0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0)),
		u16(part == 2 ? (result >> 32) & 0xff : result >> (16 * part))};
}

void tms320c54x_test_state::start_pipeline_multiply_mapped(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = multiply_mapped_case(variant); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, 0xf066 | (f.producer_b << 8)); program.write_word(0x601, f.literal);
	program.write_word(0x602 + f.gap, 0x4800 | (f.consumer_b << 8) | f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x1804); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x87654321);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(STATE_GENPC, 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "multiply_mapped_read",
		[this, f](offs_t at, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(at == f.source && value == f.sampled && !m_pipeline_operand_reads,
				"Two-word MPY permits adjacent mapped accumulator read without a gap (SPRU131G table 7-28)"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "multiply_mapped_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "MPY/LDM composition has no data writes"); });
	m_pipeline_fetch_tap = program.install_read_tap(0x601, 0x601, "multiply_mapped_literal",
		[this, f](offs_t, u16 &value, u16) { if (machine().side_effects_disabled()) return; expect(value == f.literal && !m_pipeline_extensions_reads, "MPY literal is fetched once"); ++m_pipeline_extensions_reads; });
	m_phase = 253470; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::immediate_mapped_fixture tms320c54x_test_state::immediate_mapped_case(unsigned variant)
{
	if (variant < 16)
	{
		u16 const bits = std::array<u16, 8>{56,1,7,8,9,64,255,512}[variant % 8];
		return {true, false, 0, 14, 0, 0, 8, u16((bits + 7) / 8), bits, 0x4900,
			variant & 8 ? 0x20000U : 0, 0, (bits + 7) / 8U};
	}
	unsigned v = variant - 16, operation = v % 5; v /= 5;
	bool const b = v % 2; v /= 2;
	unsigned const part = v % 3; v /= 3;
	unsigned const pattern = v % 4; v /= 4;
	unsigned const gap = v % 2; v /= 2;
	bool const sxm = v % 2; v /= 2;
	u64 const initial = std::array<u64, 4>{0,0x123456789aULL,0xff87654321ULL,0x007fffffffULL}[pattern];
	u16 const literal = std::array<u16, 4>{1,0x1357,0xffff,0x8000}[pattern];
	s64 const arithmetic_operand = sxm ? s16(literal) : literal;
	u64 const result = (operation == 0 ? initial + arithmetic_operand : operation == 1 ? initial - arithmetic_operand :
		operation == 2 ? initial & literal : operation == 3 ? initial | literal : initial ^ literal) & 0xffffffffffULL;
	u16 const sampled = part == 2 ? (result >> 32) & 0xff : result >> (part * 16);
	return {false, b, gap, 6, u16(0xf000 | (std::array<unsigned,5>{0,1,3,4,5}[operation] << 4) | (b ? 0x300 : 0)),
		literal, u16((b ? 0x0b : 8) + part), sampled, 0, u16(0x800 | (sxm ? 0x100 : 0)),
		v % 2 ? 0x20000U : 0, initial, result};
}

void tms320c54x_test_state::start_immediate_mapped(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_cpu->set_pipeline_frontend(false); m_pipeline_peripheral_case = variant;
	auto const f = immediate_mapped_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 40; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.original)
	{
		// Original AA99 02:ab09..ab15; the zero-latency AL reload is load-bearing.
		std::array<u16,13> const source{0x10e1,5,0xf010,1,0x7102,0x0011,0x4808,0xf47d,0xf000,1,0x4808,0x4ee1,6};
		for (unsigned i = 0; i < source.size(); ++i) program.write_word(f.base + 0x600 + i, source[i]);
		program.write_word(f.base + 0x60d, 0xf4e1);
		data.write_word(0x1805, f.bits); data.write_word(0x1402, 0x1900);
		data.write_word(0x1906, 0x7777); data.write_word(0x1907, 0x8888);
	}
	else
	{
		program.write_word(f.base + 0x600, f.opcode); program.write_word(f.base + 0x601, f.literal);
		program.write_word(f.base + 0x602 + f.gap, 0x4800 | (f.selected_b ? 0 : 0x100) | f.source);
		program.write_word(f.base + 0x603 + f.gap, 0xf4e1);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.original || !f.selected_b ? f.initial : 0x1234);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, !f.original && f.selected_b ? f.initial : 0x5678);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1800); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1400);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "immediate_alu_mapped_read", [this, f](offs_t at, u16 &value, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.original && at == 0x1805) { expect(value == f.bits && !m_pipeline_extensions_reads++, "Original rounding prefix reads its bit size once"); return; }
		if (f.original && at == 0x1402) { expect(value == 0x1900 && !m_pipeline_register_writes++, "Original rounding prefix retains its destination pointer"); return; }
		u16 const expected = f.original && !m_pipeline_operand_reads ? f.bits - 1 : f.sampled;
		if (at != f.source || value != expected) osd_printf_info("immediate_mapped_read: case=%u address=%04x actual=%04x expected=%04x\n", m_pipeline_peripheral_case, unsigned(at), value, expected);
		expect(at == f.source && value == expected && m_pipeline_operand_reads < unsigned(f.original ? 2 : 1), "Two-word immediate ALU permits the documented adjacent mapped read"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "immediate_alu_mapped_write", [this, f](offs_t at, u16 &value, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.original && ((at == 0x11 && value == 0x1900) || (at == 0x1906 && !value) || (at == 0x1907 && value == f.sampled)), "Original rounding prefix publishes pointer and both count halves only"); ++m_pipeline_store_writes;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 399920;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_shifted_high_store_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned const source = (index / 16) & 1;
	unsigned const modifier = index & 32 ? 5 : 1;
	program.write_word(0x0109d0, 0x9a00 | (source << 8) | (modifier << 4) | (index & 15));
	program.write_word(0x0109d1, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0507, 0xbeef);
	m_cpu->space(AS_DATA).write_word(0x0508, 0xabcd);
	m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	set_registers(*m_cpu, {
		{STATE_GENPC, 0x0109d0}, {cpu_regs::STATE_PMST, index & 64 ? 1 : 0}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_ST0, 0x0aa5}, {cpu_regs::STATE_ST1, 0x0900}, {cpu_regs::STATE_A, 0x12345678},
		{cpu_regs::STATE_B, 0xff87654321ULL}, {cpu_regs::STATE_AR3, 0x0507}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 30000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_acc_delayed_case(unsigned index)
{
	unsigned const predicate = index / 5;
	static constexpr u64 values[] = {0xffffffffffULL, 0, 1, 0x0100000000ULL, 0x8000000000ULL};
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0109d0, 0xfa42 + predicate % 6 + (predicate >= 6 ? 8 : 0)); program.write_word(0x0109d1, 0x09e0);
	program.write_word(0x0109d2, 0xe900); // Both accumulators change after the condition is captured.
	program.write_word(0x0109d3, 0xe800);
	program.write_word(0x0109d4, 0xe801); program.write_word(0x0109d5, 0xf4e1);
	program.write_word(0x0109e0, 0xe802); program.write_word(0x0109e1, 0xf4e1);
	m_cpu->set_state_int(STATE_GENPC, 0x0109d0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0900);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, predicate >= 6 ? values[index % 5] : 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, predicate < 6 ? values[index % 5] : 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 3277 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_parallel_load_multiply_case(unsigned index)
{
	u16 const opcode = 0xa800 | ((index & 1) << 8) | ((index & 2) << 8) |
			((index & 4) << 8) | (index & 32 ? 0xb9 : 0x31);
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x05e0, 0x75d6); program.write_word(0x05e1, 0x0124);
	program.write_word(0x05e2, opcode);
	program.write_word(0x05e3, 0x75d6); program.write_word(0x05e4, 0x0124);
	program.write_word(0x05e5, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0500, 0x8001);
	m_cpu->space(AS_DATA).write_word(0x0600, 0xfffd);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, index & 1 ? 0x18000 : 0x1234}, {cpu_regs::STATE_B, index & 1 ? 0x1234 : 0x18000},
		{cpu_regs::STATE_T, 3}, {cpu_regs::STATE_ST0, 0x0800},
		{cpu_regs::STATE_ST1, 0x0800 | (index & 8 ? 0x0100 : 0) | (index & 16 ? 0x40 : 0)}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_AR5, 0x0500}, {cpu_regs::STATE_AR3, 0x0600}, {cpu_regs::STATE_AR6, 0x1100},
		{STATE_GENPC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_port_writes = 0;
	m_phase = 95000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_rounded_load_case(unsigned index)
{
	static constexpr u16 values[] = {0, 0x7fff, 0x8000, 0xffff};
	static constexpr u8 operands[] = {0x45, 0x80, 0x88, 0xf8};
	unsigned const mode = index / 16;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x05e0, 0x75d6); program.write_word(0x05e1, 0x0124);
	program.write_word(0x05e2, 0x1600 | ((index & 1) << 8) | operands[mode]);
	unsigned next = 0x05e3;
	if (mode == 3) program.write_word(next++, 0x0f66);
	program.write_word(next++, 0x75d6); program.write_word(next++, 0x0124);
	program.write_word(next, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(mode == 0 ? 0x0045 : 0x0f66, values[(index >> 2) & 3]);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, 0x12345678}, {cpu_regs::STATE_B, 0x12345678}, {cpu_regs::STATE_ST0, 0x0e00},
		{cpu_regs::STATE_ST1, 0x0a00 | (index & 2 ? 0x0100 : 0)}, {cpu_regs::STATE_AR0, 0x0f66},
		{cpu_regs::STATE_AR6, 0x1100}, {STATE_GENPC, 0x05e0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_port_writes = 0;
	m_phase = 96000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_bitf_offset_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0600, 0x75d6); program.write_word(0x0601, 0x0124);
	program.write_word(0x0602, (index & 2) ? 0x61e9 : 0x61e1);
	program.write_word(0x0603, 4);
	program.write_word(0x0604, (index & 1) ? 2 : 1);
	program.write_word(0x0605, 0x75d6); program.write_word(0x0606, 0x0124);
	program.write_word(0x0607, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x1004, 2);
	m_cpu->space(AS_DATA).write_word(0x1001, 4); // Reversed-field sentinels.
	m_cpu->space(AS_DATA).write_word(0x1002, 4);
	m_port_writes = 0;
	set_registers(*m_cpu, {
		{STATE_GENPC, 0x0600}, {cpu_regs::STATE_AR1, 0x1000}, {cpu_regs::STATE_ST0, 0x0aa5}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 60030 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_cmpm_offset_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0109c0, (index & 2) ? 0x60e9 : 0x60e1);
	program.write_word(0x0109c1, 4);
	program.write_word(0x0109c2, (index & 1) ? 1 : 2);
	program.write_word(0x0109c3, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x1004, 1);
	set_registers(*m_cpu, {
		{STATE_GENPC, 0x0109c0}, {cpu_regs::STATE_AR1, 0x1000}, {cpu_regs::STATE_ST0, 0x02a5},
		{cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 1628 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_cmpr_case(unsigned index)
{
	unsigned const ar = index & 7, condition = (index / 8) & 3, pair = index / 32;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x010980, 0xf4a8 | (condition << 8) | ar);
	program.write_word(0x010981, 0xf4e1);
	m_cpu->set_state_int(STATE_GENPC, 0x010980);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, cmpr_values[pair][1]);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, cmpr_values[pair][0]);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x0aa5 | ((index & 1) ? 0x1000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 1427 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_bit_case(unsigned index)
{
	unsigned const operand = index / 32;
	unsigned const bitcode = (index / 2) & 15;
	bool const set = index & 1;
	u16 const mask = u16(1) << (15 - bitcode);
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x010950, 0x9600 | (operand << 4) | bitcode);
	program.write_word(0x010951, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0507, set ? mask : u16(~mask));
	set_registers(*m_cpu, {
		{STATE_GENPC, 0x010950}, {cpu_regs::STATE_ST0, 0x0aa5 | (set ? 0 : 0x1000)}, {cpu_regs::STATE_AR0, 3},
		{cpu_regs::STATE_AR2 + (operand & 3), 0x0507}, {cpu_regs::STATE_BK, 8}, {cpu_regs::STATE_A, 0x12345},
		{cpu_regs::STATE_B, 0x6789}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 819 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}


void tms320c54x_test_state::start_pipeline_negate()
{
	start_register_unary(register_unary_kind::negate, 253410);
}

tms320c54x_test_state::mapped_word_arithmetic_fixture tms320c54x_test_state::mapped_word_arithmetic_case(unsigned variant)
{
	unsigned const v = variant % 2304;
	bool const producer = v >= 2048;
	unsigned const p = producer ? v - 2048 : v;
	unsigned const reg = producer ? 0 : p % 8, selector = producer ? p % 4 : (p / 8) % 4;
	unsigned const flags = producer ? (p / 4) % 4 : (p / 32) % 4, pattern = producer ? (p / 128) % 2 : (p / 128) % 4;
	bool const absolute = producer ? (p / 16) % 2 : (p / 512) % 2;
	bool const page = producer ? (p / 32) % 2 : (p / 1024) % 2, gap = producer && (p / 64) % 2;
	bool const subtract = selector >= 2, destination_b = selector & 1, sxm = flags & 1, ovm = flags & 2;
	u64 const a = producer ? 0x1234568001ULL : std::array<u64, 4>{0x20, 0xff80008000ULL, 0x007fffffff, 0x8000000000ULL}[pattern];
	u64 const b = producer ? 0x1e1e2 : std::array<u64, 4>{0x1e1e2, 0x0040000001, 0xff80000000ULL, 0x7fffffffffULL}[pattern];
	u16 const t = std::array<u16, 4>{0x20, 0x8000, 0xffff, 0x7f}[pattern], trn = std::array<u16, 4>{0x10, 0xffff, 0x8000, 0xff}[pattern];
	u16 const produced = pattern ? 0x20 : 0;
	u64 const sample_a = producer && (absolute || gap) ? produced : a;
	u16 const memory = reg < 3 ? u16(sample_a >> (reg * 16)) : reg < 6 ? u16(b >> ((reg - 3) * 16)) : reg == 6 ? t : trn;
	u64 after_a = producer ? produced : a, after_b = b;
	unsigned mutation_state = 0;
	if (!producer)
	{
		if (reg < 3 && destination_b) { after_a = 0x012345abcdULL; mutation_state = tms320c54x_device::STATE_A; }
		else if (reg >= 3 && reg < 6 && !destination_b) { after_b = 0x012345abcdULL; mutation_state = tms320c54x_device::STATE_B; }
		else if (reg >= 6) mutation_state = reg == 6 ? tms320c54x_device::STATE_T : tms320c54x_device::STATE_TRN;
	}
	u64 const accumulator = destination_b ? after_b : after_a;
	s64 const left = BIT(accumulator, 39) ? s64(accumulator) - (s64(1) << 40) : s64(accumulator);
	s64 const right = sxm ? s16(memory) : s64(memory), sum = subtract ? left - right : left + right;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = subtract ? u32(accumulator) >= u32(right) : u64(u32(accumulator)) + u32(right) > 0xffffffffULL;
	u16 const st0 = 0x1800, result_st0 = (st0 & ~u16(0x800)) | (carry ? 0x800 : 0) | (overflow ? destination_b ? 0x200 : 0x400 : 0);
	u64 const result = u64(overflow && ovm ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
	unsigned const prefix = producer ? 1 + unsigned(gap) : 0;
	return {a, b, after_a, after_b, result, t, trn, u16(8 + reg), memory,
		u16((subtract ? 0x800 : 0) | (destination_b ? 0x100 : 0) | (absolute ? 0xf8 : 8 + reg)),
		st0, u16(0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0)), result_st0,
		u32(page ? 0x20000 : 0), prefix, (absolute ? 6U : 5U) + prefix, mutation_state, absolute};
}

clocked_register_case tms320c54x_test_state::mapped_register_sequence(unsigned variant, bool bitfield) const
{
	auto const c = bitfield ? mapped_bitfield_case(variant) : mapped_bitfield_fixture{};
	auto const f = bitfield ? c.registers : mapped_word_arithmetic_case(variant);
	unsigned const mutation = bitfield ? c.mutation_state : f.mutation_state;
	u16 const initial_st0 = bitfield ? c.st0 : f.st0;
	unsigned const finish = bitfield ? 18 : 16;
	u16 const opcode = bitfield ? 0x6100 | (f.absolute ? 0xf8 : f.address) : f.opcode;
	instruction_case initial{"mapped-register", opcode, {}, {}, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_A, f.a}, {cpu_regs::STATE_B, f.b},
		{cpu_regs::STATE_T, f.t}, {cpu_regs::STATE_TRN, f.trn},
		{cpu_regs::STATE_ST0, initial_st0}, {cpu_regs::STATE_ST1, f.st1},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0}, {STATE_GENPC, f.base + 0x600}
	}, {}, 0, 0};
	for (unsigned i = 0; i < 24; ++i) initial.program.push_back({f.base + 0x600 + i, 0xf495});
	if (f.prefix) initial.program.push_back({f.base + 0x600, 0xe800 | u16(f.after_a)});
	initial.program.push_back({f.base + 0x600 + f.prefix, opcode});
	clocked_register_case test{{{0, std::move(initial)}},
		f.read_cycle - 1 + variant / (bitfield ? (status_bitfield() ? 256 : 4608) : 2304), finish, {}, variant};
	u32 const extension = f.base + 0x601 + f.prefix;
	if (f.absolute) test.retained_program_reads.push_back({extension, f.address});
	if (bitfield) test.retained_program_reads.push_back({extension + unsigned(f.absolute), c.mask});
	for (auto const &word : test.retained_program_reads) test.states.front().observation.program.push_back(word);
	// Only the padding after the complete instruction changes: stop in real IDLE.
	test.states.front().observation.program.push_back({extension + unsigned(f.absolute) + unsigned(bitfield), 0xf4e1});
	if (mutation) test.actions.push_back({f.read_cycle, {{mutation,
		mutation == cpu_regs::STATE_A || mutation == cpu_regs::STATE_B ? 0x012345abcdULL : 0x2222}}, true});
	test.data_events = std::vector<clocked_data_event>{{false, f.address, bitfield ? c.memory : f.memory, f.read_cycle}};
	for (unsigned tick = 1; tick <= finish; ++tick)
	{
		u64 a = f.prefix && tick >= 6 ? f.after_a : f.a, b = f.b;
		if (tick >= f.read_cycle)
		{
			if (mutation == cpu_regs::STATE_A) a = bitfield ? 0x012345abcdULL : f.after_a;
			if (mutation == cpu_regs::STATE_B) b = bitfield ? 0x012345abcdULL : f.after_b;
		}
		u16 const current_st0 = tick >= f.read_cycle && mutation == cpu_regs::STATE_ST0 ? 0x2222 : initial_st0;
		u16 const st0 = tick > f.read_cycle ? (bitfield ?
			(current_st0 & ~u16(0x1000)) | ((c.memory & c.mask) ? 0x1000 : 0) : f.result_st0) : current_st0;
		if (!bitfield && tick > f.read_cycle) (BIT(f.opcode, 8) ? b : a) = f.result;
		test.states.push_back({tick, {"mapped-register", opcode, {}, {}, {}, {
			{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_A, a}, {cpu_regs::STATE_B, b},
			{cpu_regs::STATE_ST0, st0},
			{cpu_regs::STATE_ST1, tick >= f.read_cycle && mutation == cpu_regs::STATE_ST1 ? 0x2222ULL : f.st1},
			{cpu_regs::STATE_T, tick >= f.read_cycle && mutation == cpu_regs::STATE_T ? 0x2222ULL : f.t},
			{cpu_regs::STATE_TRN, tick >= f.read_cycle && mutation == cpu_regs::STATE_TRN ? 0x2222ULL : f.trn}
		}, 0, 0}});
	}
	return test;
}

void tms320c54x_test_state::start_pipeline_mapped_word_arithmetic()
{
	m_register_sequence = std::make_unique<register_sequence_fixture>(machine(), *m_cpu, *m_check_timer,
		6912, [this](unsigned key) { return mapped_register_sequence(key, false); });
	m_phase = 253430; m_register_sequence->start();
}

bool tms320c54x_test_state::status_bitfield() const
{ return profile_is("status-bitf"); }

tms320c54x_test_state::mapped_bitfield_fixture tms320c54x_test_state::mapped_bitfield_case(unsigned variant) const
{
	if (status_bitfield())
	{
		auto f = mapped_word_arithmetic_case(0);
		f.address = 6 + variant % 2;
		unsigned const pattern = (variant / 2) % 4;
		f.absolute = (variant / 64) % 2;
		f.base = (variant / 128) % 2 ? 0x20000 : 0;
		f.prefix = 0; f.read_cycle = f.absolute ? 7 : 6;
		f.st0 = (std::array<u16, 4>{0, 0x0800, 0xe600, 0xee00}[pattern] & ~u16(0x1000)) | (((variant / 32) % 2) ? 0x1000 : 0);
		f.st1 = std::array<u16, 4>{0, 0x0800, 0x2000, 0x3b80}[pattern];
		return {f, f.address == 6 ? f.st0 : f.st1,
			std::array<u16, 4>{0, 0x0800, 0x1000, 0xffff}[(variant / 8) % 4],
			f.st0, f.address == 6 ? tms320c54x_device::STATE_ST0 : tms320c54x_device::STATE_ST1};
	}
	unsigned const v = variant % 576;
	unsigned const expanded = v < 512 ? v % 8 + ((v / 8) % 4) * 32 + ((v / 32) % 4) * 128 + ((v / 128) % 2) * 512 + (v / 256) * 1024 :
		2048 + ((v - 512) % 4) * 4 + (((v - 512) / 4) % 2) * 16 + (((v - 512) / 8) % 2) * 32 + (((v - 512) / 16) % 2) * 64 + ((v - 512) / 32) * 128;
	auto f = mapped_word_arithmetic_case(expanded);
	unsigned const reg = f.address - 8;
	u16 const memory = f.prefix ? ((v - 512) / 32 ? 1 : 0) : f.memory;
	if (f.prefix) f.after_a = memory;
	unsigned const mutation_state = f.prefix ? 0 : reg < 3 ? tms320c54x_device::STATE_A : reg < 6 ? tms320c54x_device::STATE_B :
		reg == 6 ? tms320c54x_device::STATE_T : tms320c54x_device::STATE_TRN;
	++f.read_cycle; // BITF owns one additional mask word.
	return {f, memory, std::array<u16, 4>{0, 1, 0x7f, 0xffff}[(variant / 576) % 4],
		u16((f.st0 & ~u16(0x1000)) | (((variant / 2304) % 2) ? 0x1000 : 0)), mutation_state};
}

void tms320c54x_test_state::start_pipeline_mapped_bitfield()
{
	m_register_sequence = std::make_unique<register_sequence_fixture>(machine(), *m_cpu, *m_check_timer,
		status_bitfield() ? 768 : 13824, [this](unsigned key) { return mapped_register_sequence(key, true); });
	m_phase = 253450; m_register_sequence->start();
}

tms320c54x_test_state::accumulator_branch_fixture tms320c54x_test_state::accumulator_branch_case(unsigned v)
{
	accumulator_branch_fixture f{};
	char const *const bios = machine().options().bios();
	f.near = !strcmp(bios, "bacc") || !strcmp(bios, "bacc-producer");
	bool const producer = !strcmp(bios, "bacc-producer") || !strcmp(bios, "fbacc-producer");
	f.b = v & 1;
	unsigned const caller = std::array<unsigned, 3>{0, 2, 0x7f}[(v / 2) % 3];
	f.hint = std::array<unsigned, 3>{0, 3, 0x7f}[(v / 6) % 3];
	f.base = caller << 16; f.destination = ((f.near ? caller : f.hint) << 16) | 0x700; f.table = (u32(f.hint) << 16) | 0x900;
	f.producer = producer ? 1 + v / 216 : 0;
	unsigned const gap = producer ? (v / 18) % 3 : 0;
	f.prefix = f.producer ? f.producer + gap : 0; f.wait = f.producer == 2 ? 4 : 0;
	f.irq = producer ? 0 : v / 72;
	f.st0 = f.producer == 1 ? 0xa628 : 0xa608; f.st1 = 0x100 | (f.irq == 1 ? 0 : 0x800);
	f.initial = f.producer == 1 ? 0xbad0 : 0x5500000000ULL | (u32(f.hint) << 16) | (f.producer == 2 ? 0x900 : 0x700);
	f.other = f.b ? (f.producer == 2 ? 0x5500000000ULL | f.table : 0x1234) : 0x2345;
	unsigned const checkpoint = producer ? (v / 54) % 4 : (v / 18) % 4;
	f.snapshot = std::array<unsigned, 4>{4, f.sample(), f.sample() + 1, f.sample() + 3}[checkpoint];
	return f;
}

void tms320c54x_test_state::start_accumulator_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_acc_branch_fixture = accumulator_branch_case(variant); auto const f = m_acc_branch_fixture;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.destination + i, 0xf495); }
	if (f.producer == 1) { program.write_word(f.base + 0x600, f.b ? 0x5700 : 0x5600); data.write_word(0x1400, 0x8000 | f.hint); data.write_word(0x1401, 0x700); }
	if (f.producer == 2) { program.write_word(f.base + 0x600, 0x7ef8); program.write_word(f.base + 0x601, f.b ? 0x0b : 0x08); program.write_word(f.table, 0x700); }
	program.write_word(f.base + 0x600 + f.prefix, f.opcode()); program.write_word(f.base + 0x601 + f.prefix, 0x880f);
	program.write_word(f.destination, 0x880e); program.write_word(f.destination + 1, 0xf4e1);
	program.write_word((f.destination & 0x7f0000) | 4, 0xf4eb); program.write_word((f.destination & 0x7f0000) | 0x40, 0xf4eb);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_RTN, 0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xaaaa); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x1357);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.b ? f.other : f.initial); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ? f.initial : f.other);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq == 1 ? 1 : 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_register_writes = m_vector_acks = m_port_reads = m_fast_stack_reads = m_pipeline_drained_fetches = m_acc_branch_mapped_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "acc_branch_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.producer == 1 && (at == 0x1400 || at == 0x1401)) { unsigned const part = m_port_reads++; expect(part < 2 && at == 0x1400 + part && word == (part ? 0x700 : 0x8000 | f.hint) && m_cpu->pipeline_cycles() == 5, "DLD captures both target words before branch sampling"); return; }
		expect(f.irq && at == 0xfff && word == 0x700 && !m_pipeline_operand_reads, "Accumulator branch owns no data read except the later IRQ return"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "acc_branch_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.producer == 2 && at == (f.b ? 0x0b : 0x08)) { expect(!m_acc_branch_mapped_writes && word == 0x700 && m_cpu->pipeline_cycles() == 11, "READA owns one mapped accumulator publication before branch sampling"); ++m_acc_branch_mapped_writes; return; }
		if (at == 0x0e) { expect(!m_pipeline_register_writes && word == f.marker() && (f.irq ? m_vector_acks == 1 && m_cpu->pipeline_cycles() >= 12 : m_cpu->pipeline_cycles() == f.sample() + 7), "Branch target executes once at the Class 30A boundary or after interrupt return"); ++m_pipeline_register_writes; return; }
		expect(f.irq && at == 0xfff && word == 0x700 && !m_pipeline_store_writes && m_vector_acks == 1, "Branch creates no call frame; IRQ stacks only the retained target PC"); ++m_pipeline_store_writes;
	});
	m_pipeline_fetch_tap = program.install_read_tap(0, 0x7fffff, "acc_branch_program", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == f.base + 0x600 + f.prefix) { expect(word == f.opcode() && !m_pipeline_drained_fetches, "Accumulator branch opcode is fetched once"); ++m_pipeline_drained_fetches; }
		if (at == f.destination) { expect(word == 0x880e && (f.irq ? m_pipeline_extensions_reads < 2 : !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == f.sample() + 3), "Accumulator branch retains the target across later source mutation"); ++m_pipeline_extensions_reads; }
		if (f.producer == 2 && at == f.table) { expect(word == 0x700 && !m_fast_stack_reads && m_cpu->pipeline_cycles() == 10, "READA samples one program-table target before mapped accumulator publication"); ++m_fast_stack_reads; }
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254240;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::offset_logic_fixture tms320c54x_test_state::offset_logic_case(unsigned v)
{
	offset_logic_fixture f{};
	f.operation = (v % 6) / 2; f.b = v & 1;
	unsigned const mode = (v / 6) % 24, pair = (v / 576) % 4, flags = (v / 2304) % 4;
	f.kind = mode / 8; f.encoded = mode % 8; f.cmpt = (v / 9216) % 2; f.linear = (v / 18432) % 2;
	f.selected = f.cmpt && !f.encoded ? 3 : f.encoded;
	unsigned const offset_index = (v / 144) % 4;
	s16 const offset = offset_index == 3 && f.kind == 2 && !f.linear ? 7 : std::array<s16, 4>{0, 1, -1, 0x1462}[offset_index];
	f.literal = u16(offset);
	u16 const initial = offset_logic_fixture::initial_ar(f.selected);
	// BK=13 uses a 16-word aligned base and an initial queue index of 12.
	int const index = (12 + offset) % 13;
	f.address = f.kind == 2 && !f.linear ? (initial & 0xfff0) | (index < 0 ? index + 13 : index) : u16(initial + offset);
	f.next = f.kind ? f.address : initial;
	f.base = ((v / 36864) % 2) ? 0x20000 : 0;
	f.snapshot = std::array<unsigned, 4>{3, 5, 6, 7}[v / 73728];
	f.st0 = 0x6608 | (pair & 1 ? 0x1800 : 0);
	f.st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (f.cmpt ? 0x20 : 0) | (pair & 2 ? 0x80 : 0);
	f.source = std::array<u64, 4>{0, 0xffff, 0x123456789aULL, 0xff80000000ULL}[pair];
	f.value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pair];
	return f;
}

void tms320c54x_test_state::start_offset_logic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; m_offset_logic_fixture = offset_logic_case(variant); auto const f = m_offset_logic_fixture;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.opcode()); program.write_word(f.base + 0x601, f.literal); program.write_word(f.base + 0x602, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.linear ? 0 : 13);
	for (unsigned r = 0; r < 8; ++r) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + r, offset_logic_fixture::initial_ar(r));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.b ? 0x2233445566ULL : f.source); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ? f.source : 0x2233445566ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_RTN, 0x5555);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); data.write_word(f.address, f.value);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "offset_logic_source", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at != f.address || word != f.value || m_cpu->pipeline_cycles() != 6 || m_pipeline_operand_reads) osd_printf_info("offset_logic_read: case=%u at=%04x expected=%04x word=%04x expected_word=%04x cycle=%llu\n", m_pipeline_peripheral_case, unsigned(at), f.address, word, f.value, (unsigned long long)m_cpu->pipeline_cycles());
		expect(at == f.address && word == f.value && m_cpu->pipeline_cycles() == 6 && !m_pipeline_operand_reads, "Long logical Smem reads its captured address once at Read"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "offset_logic_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "Long memory logic never writes data memory"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "offset_logic_literal", [this, f](offs_t, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(word == f.literal && m_cpu->pipeline_cycles() == 3 && !m_pipeline_extensions_reads, "Long memory logic retains one signed offset literal"); ++m_pipeline_extensions_reads;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254260;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::absolute_logic_fixture tms320c54x_test_state::absolute_logic_case(unsigned v)
{
	absolute_logic_fixture f{};
	f.kind = v % 6;
	// 0060/0061 are the harness's synthetic IRQ peripheral, not RAM.
	f.address = std::array<u16, 4>{0x0062, 0x1001, 0xb582, 0xffff}[(v / 6) % 4];
	unsigned const pair = (v / 24) % 4;
	f.value = std::array<u16, 4>{0xffff, 0x8000, 0x00ff, 0x8001}[pair];
	f.source = std::array<u64, 4>{0, 0xffff, 0x123456789aULL, 0xff80000000ULL}[pair];
	f.status = 0x4a20 | (((v / 96) % 2) ? 0x100 : 0);
	f.base = ((v / 192) % 2) ? 0x20000 : 0;
	f.snapshot = std::array<unsigned, 4>{3, 4, 6, 7}[v / 384];
	return f;
}

void tms320c54x_test_state::start_absolute_logic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = absolute_logic_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.opcode()); program.write_word(f.base + 0x601, f.address); program.write_word(f.base + 0x602, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xb601); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.status);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x2222); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.kind & 1 ? 0x2233445566ULL : f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.kind & 1 ? f.source : 0x2233445566ULL);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	data.write_word(f.address, f.value);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "absolute_logic_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at != f.address || word != f.value || m_cpu->pipeline_cycles() != 6 || m_pipeline_operand_reads) osd_printf_info("absolute_logic_read: case=%u replay=%u at=%04x expected=%04x word=%04x expected_word=%04x cycle=%llu reads=%u\n", m_pipeline_peripheral_case, unsigned(m_pipeline_replay), unsigned(at), f.address, word, f.value, (unsigned long long)m_cpu->pipeline_cycles(), m_pipeline_operand_reads);
		expect(at == f.address && word == f.value && m_cpu->pipeline_cycles() == 6 && !m_pipeline_operand_reads, "Absolute logic reads only its retained address once at Read"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "absolute_logic_write", [this](offs_t, u16 &, u16) {
		if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "Absolute accumulator logic never writes data memory"); }
	});
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "absolute_logic_literal", [this, f](offs_t, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(word == f.address && m_cpu->pipeline_cycles() == 3 && !m_pipeline_extensions_reads, "Absolute logic fetches one address literal"); ++m_pipeline_extensions_reads;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254160;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::ts_arithmetic_fixture tms320c54x_test_state::ts_arithmetic_case(unsigned variant, bool producer)
{
	ts_arithmetic_fixture f{}; f.producer = producer;
	bool const numeric = !producer && variant >= 6912;
	unsigned const v = numeric ? variant - 6912 : variant;
	unsigned form, reg, compatibility, code, flags, pattern, memory_pattern, snapshot;
	if (producer)
	{
		f.producer_kind = v % 3; f.gap = (v / 3) % 2; code = std::array<unsigned, 8>{0, 1, 15, 16, 31, 48, 49, 63}[(v / 6) % 8];
		memory_pattern = (v / 48) % 4; pattern = (v / 48) % 8; flags = (v / 192) % 4; f.selector = (v / 768) % 4;
		f.base = (v / 3072) % 2 ? 0x20000 : 0; snapshot = v / 6144; form = 2; reg = 1; compatibility = 0;
		f.prefix = (f.producer_kind == 0 ? 2 : 1) + f.gap;
	}
	else if (numeric)
	{
		unsigned const index = v % 48; code = index < 32 ? index : index + 16;
		memory_pattern = (v / 48) % 4; pattern = (v / 192) % 8; flags = (v / 1536) % 4; f.selector = (v / 6144) % 4;
		f.base = (v / 24576) % 2 ? 0x20000 : 0; snapshot = v / 49152; form = 0; reg = 2; compatibility = 0;
	}
	else
	{
		form = v % 18; reg = (v / 18) % 8; compatibility = (v / 144) % 2; f.selector = (v / 288) % 4;
		f.base = (v / 1152) % 2 ? 0x20000 : 0; snapshot = v / 2304;
		unsigned const index = v % 48; code = index < 32 ? index : index + 16;
		memory_pattern = (v / 8) % 4; pattern = (v / 96) % 8; flags = (v / 64) % 4;
	}
	f.address = memory_arithmetic_case(form + 18 * reg + f.selector * 144 + pattern * 576 + flags * 4608 + compatibility * 18432);
	auto &o = f.address; o.opcode |= 0x0400; o.read_cycle += f.prefix;
	f.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[memory_pattern]; f.t = 0xa500 | code;
	f.sampled_t = producer && f.producer_kind == 2 && !f.gap ? 15 : f.t;
	f.a = f.selector & 1 ? 0x123456789aULL : o.source; f.b = f.selector & 1 ? o.source : 0xfeabcd9876ULL;
	if (producer && f.producer_kind == 2) f.a = f.t;
	u64 const left_bits = f.selector & 1 ? f.b : f.a;
	u64 const right_bits = ts_load_expected(f.memory, f.sampled_t, flags & 1);
	s64 const left = BIT(left_bits, 39) ? s64(left_bits) - (s64(1) << 40) : s64(left_bits);
	s64 const right = BIT(right_bits, 39) ? s64(right_bits) - (s64(1) << 40) : s64(right_bits);
	s64 const result = f.selector & 2 ? left - right : left + right;
	bool const carry = f.selector & 2 ? u32(left_bits) >= u32(right_bits) : u64(u32(left_bits)) + u32(right_bits) > 0xffffffffULL;
	bool const overflow = result < -2147483648LL || result > 2147483647LL;
	o.result_st0 = compatibility && reg && form >= 2 && form != 17 ? (o.st0 & 0x1fff) | (reg << 13) : o.st0;
	f.result_st0 = (o.result_st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? f.selector & 1 ? 0x200 : 0x400 : 0);
	f.result = u64(overflow && (flags & 2) ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	f.checkpoint = producer ? std::array<unsigned, 3>{o.read_cycle - 1, o.read_cycle, o.read_cycle + 1}[snapshot] : std::array<unsigned, 3>{3, o.read_cycle, o.read_cycle + 1}[snapshot];
	return f;
}

void tms320c54x_test_state::start_ts_arithmetic(unsigned variant)
{
	if (strcmp(machine().options().bios(), "ts-alu-producer") && variant < 6912 && variant % 18 == 5) ++variant;
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const producer = profile_is("ts-alu-producer"); auto const f = ts_arithmetic_case(variant, producer); auto const &o = f.address;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (producer) { program.write_word(f.base + 0x600, f.producer_kind == 0 ? 0x770e : f.producer_kind == 1 ? 0x3082 : 0x880e); if (!f.producer_kind) program.write_word(f.base + 0x601, f.t); }
	program.write_word(f.base + 0x600 + f.prefix, o.opcode); if (!producer && o.form >= 14) program.write_word(f.base + 0x601, o.literal);
	program.write_word(f.base + 0x601 + f.prefix + unsigned(o.form >= 14), 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, o.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, o.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, o.bk); m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, producer ? 15 : f.t);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x5007);
	if (producer && f.producer_kind == 1) { m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5100); data.write_word(0x5100, f.t); } data.write_word(o.address, f.memory);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "ts_arithmetic_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; bool const t_read = f.producer && f.producer_kind == 1 && !m_pipeline_operand_reads; expect(at == (t_read ? 0x5100 : f.address.address) && word == (t_read ? f.t : f.memory) && m_pipeline_operand_reads < (f.producer && f.producer_kind == 1 ? 2U : 1U) && m_cpu->pipeline_cycles() == (t_read ? 5 : f.address.read_cycle), "TS arithmetic retains one source and any instruction-driven T input"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "ts_arithmetic_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(f.producer && f.producer_kind != 1 && at == 0x0e && word == f.t && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == (f.producer_kind == 0 ? 5 : 6), "TS arithmetic writes no memory except the selected T producer"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "ts_arithmetic_literal", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || (f.producer ? f.producer_kind != 0 : f.address.form < 14)) return; expect(word == (f.producer ? f.t : f.address.literal) && !m_pipeline_extensions_reads, "TS arithmetic owns one retained address or producer literal"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255060;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delay_load_fixture tms320c54x_test_state::delay_shift_alu_case(unsigned variant)
{
	unsigned const operation = variant % 4, v = variant / 4;
	auto f = delay_load_case(v, true);
	u64 const shifted = f.result, source = operation & 1 ? f.b : f.a;
	s64 const left = source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source);
	s64 const right = shifted & 0x8000000000ULL ? s64(shifted) - 0x10000000000LL : s64(shifted);
	s64 const result = operation & 2 ? left - right : left + right;
	bool const carry = operation & 2 ? u32(source) >= u32(shifted) : u64(u32(source)) + u32(shifted) > 0xffffffffULL;
	bool const overflow = result < -2147483648LL || result > 2147483647LL;
	f.st0 = 0xa020 | ((v / 1152) % 4 << 9);
	f.final_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	f.literal = 0x0c00 | (operation & 2 ? 0x20 : 0) | ((operation & 1) << 9) | (f.destination << 8) | (f.literal & 31);
	return f;
}

tms320c54x_test_state::asm_mapped_fixture tms320c54x_test_state::asm_mapped_case(unsigned v)
{
	asm_mapped_fixture f{};
	f.source = 8 + v % 16; f.form = (v / 16) % 4; f.raw = (v / 64) % 32; f.flags = (v / 2048) % 4;
	f.base = (v / 8192) % 2 ? 0x20000 : 0; f.ar = f.source == 0x17 ? 6 : 7;
	f.word = (std::array<u16, 4>{0, 0xaaa0, 0x5540, 0xffe0}[f.flags] | f.raw) & (f.source == 10 || f.source == 13 ? 0xff : 0xffff);
	f.st1 = 0x800 | (f.flags & 1 ? 0x140 : 0) | (f.flags & 2 ? 0x200 : 0) | (f.raw ^ 31);
	f.a = 0x123456789aULL; f.b = 0xfeabcd9876ULL; f.t = 0x5aa5; f.trn = 0x1357;
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x1000 + i;
	if (f.source < 14) { unsigned const part = (f.source - 8) % 3; u64 &acc = f.source < 11 ? f.a : f.b; u64 const mask = part == 2 ? 0xff : 0xffff; acc = (acc & ~(mask << (16 * part))) | (u64(f.word) << (16 * part)); }
	else if (f.source == 14) f.t = f.word; else if (f.source == 15) f.trn = f.word; else f.initial[f.source - 16] = f.word;
	f.initial[f.ar] = f.form == 2 ? f.source - 3 : f.source; f.final = f.initial;
	if (f.form == 1) ++f.final[f.ar];
	f.read = f.form >= 2 ? 6 : 5; f.checkpoint = std::array<unsigned, 3>{3, f.read, f.read + 1}[v / 16384];
	return f;
}

void tms320c54x_test_state::start_asm_mapped(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = asm_mapped_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	u16 const mode = f.form == 0 ? f.source : f.form == 1 ? 0x90 | f.ar : f.form == 2 ? 0xe0 | f.ar : 0xf8;
	program.write_word(f.base + 0x600, 0x3200 | mode); if (f.form >= 2) program.write_word(f.base + 0x601, f.form == 2 ? 3 : f.source);
	program.write_word(f.base + 0x601 + unsigned(f.form >= 2), 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0x8800); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, f.trn);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "asm_mapped_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == f.source && word == f.word && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read, "LD ASM retains exactly one mapped register source in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "asm_mapped_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "LD ASM has no data writes"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "asm_mapped_literal", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || f.form < 2) return; expect(word == (f.form == 2 ? 3 : f.source) && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == 3, "Mapped LD ASM retains one address literal"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255140;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delay_compare_fixture tms320c54x_test_state::delay_compare_case(unsigned v)
{
	delay_compare_fixture f{};
	f.kind = v % 3; f.bitfield = (v / 3) % 2; f.form = (v / 6) % 3;
	unsigned const pair = (v / 18) % 16;
	f.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pair % 4];
	f.literal = std::array<u16, 4>{0, 1, 0x8000, 0xf495}[pair / 4];
	f.initial_tc = (v / 288) % 2; f.prefix = (v / 576) % 2 ? 2 : 0;
	bool const branch_tc = f.prefix ? bool(f.memory & 1) : f.initial_tc;
	f.taken = !f.kind || (f.kind == 1 ? branch_tc : !branch_tc);
	f.final_tc = f.bitfield ? bool(f.memory & f.literal) : f.memory == f.literal;
	f.interrupt = (v / 1152) % 3; f.base = (v / 3456) % 2 ? 0x20000 : 0;
	f.read = (f.kind ? 9 : 8) + f.prefix; f.literal_clock = (f.kind ? 6 : 5) + f.prefix;
	f.checkpoint = std::array<unsigned, 4>{f.read - 1, f.read, f.read + 1, f.read + 3}[v / 6912];
	return f;
}

void tms320c54x_test_state::start_delay_compare(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = delay_compare_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); program.write_word(f.base + i, 0xf495); }
	if (f.prefix) { program.write_word(f.base + 0x600, 0x6102); program.write_word(f.base + 0x601, 1); }
	program.write_word(f.base + 0x600 + f.prefix, f.kind == 0 ? 0xf273 : f.kind == 1 ? 0xfa30 : 0xfa20); program.write_word(f.base + 0x601 + f.prefix, 0x700);
	program.write_word(f.base + 0x602 + f.prefix, (f.bitfield ? 0x6100 : 0x6000) | (f.form == 2 ? 0x92 : 2)); program.write_word(f.base + 0x603 + f.prefix, f.literal);
	program.write_word(f.base + 0x604 + f.prefix, 0xe922); program.write_word(f.base + 0x605 + f.prefix, 0xf4e1); program.write_word(f.base + 0x700, 0xe911); program.write_word(f.base + 0x701, 0xf4e1);
	program.write_word(f.base + 4, 0xf4eb); program.write_word(f.base + 0x40, 0xf4eb); data.write_word(0x1002, f.memory);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xae20 | (f.initial_tc ? 0x1000 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x100 | (f.form == 1 ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x55); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1002);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0x1002, 0x1002, "delay_compare_read", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const clock = f.prefix && !m_pipeline_operand_reads ? 6 : f.read; if (m_cpu->pipeline_cycles() != clock) osd_printf_info("delay_compare_read: case=%u actual=%llu expected=%u reads=%u\n", m_pipeline_peripheral_case, (unsigned long long)m_cpu->pipeline_cycles(), clock, m_pipeline_operand_reads); expect(word == f.memory && m_pipeline_operand_reads < (f.prefix ? 2U : 1U) && m_cpu->pipeline_cycles() == clock, "Delay comparison reads its retained source once after any prefix producer"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0x1002, 0x1002, "delay_compare_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "Delay CMPM BITF never writes the operand"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x605 + f.prefix, "delay_compare_literals", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (f.prefix && at == f.base + 0x601) { expect(word == 1 && !m_pipeline_extensions_reads, "Prefix BITF owns its mask literal"); ++m_pipeline_extensions_reads; } else if (at == f.base + 0x601 + f.prefix) { expect(word == 0x700 && m_pipeline_extensions_reads == unsigned(bool(f.prefix)), "Delayed branch owns its target literal"); ++m_pipeline_extensions_reads; } else if (at == f.base + 0x603 + f.prefix) { expect(word == f.literal && m_pipeline_extensions_reads == 1 + unsigned(bool(f.prefix)) && m_cpu->pipeline_cycles() == f.literal_clock, "Two-word comparison owns its delay literal, including opcode-shaped values"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254980;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::parallel_load_fixture tms320c54x_test_state::parallel_arithmetic_case(unsigned variant)
{
	unsigned const extent = 67584, form = variant / extent;
	auto c = parallel_load_case(variant % extent + (form % 4) * extent);
	c.form = form; auto &f = c.memory;
	u64 const old_other = f.destination ? c.a : c.b;
	s64 const other = old_other & 0x8000000000ULL ? s64(old_other) - 0x10000000000LL : s64(old_other);
	s64 const memory = s64(f.st1 & 0x100 ? s16(f.xword) : s32(f.xword)) * 65536;
	s64 result = form >= 4 ? memory - other : other + memory;
	bool const carry = form >= 4 ? u32(memory) >= u32(old_other) : u64(u32(memory)) + u32(old_other) > 0xffffffffULL;
	f.result_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0);
	if (result < -2147483648LL || result > 2147483647LL) { f.result_st0 |= f.destination ? 0x200 : 0x400; if (f.st1 & 0x200) result = std::clamp(result, s64(-2147483648LL), s64(2147483647LL)); }
	c.result_a = f.destination ? c.a : u64(result) & 0xffffffffffULL;
	c.result_b = f.destination ? u64(result) & 0xffffffffffULL : c.b;
	return c;
}

tms320c54x_test_state::parallel_load_fixture tms320c54x_test_state::parallel_multiply_case(unsigned variant)
{
	auto c = parallel_load_case(variant);
	auto &f = c.memory;
	c.t = std::array<u16, 8>{0, 1, 0xffff, 0x8000, 0x7fff, 0x1234, 0x5555, 0xaaaa}[(variant / 8) % 8];
	f.st1 = (f.st1 & ~0x40) | ((variant / 64) % 2 ? 0x40 : 0);
	f.pmst = (f.pmst & ~2) | ((variant / 128) % 2 ? 2 : 0);
	s64 const product = s64(s16(f.xword)) * s16(c.t) * (f.st1 & 0x40 ? 2 : 1);
	bool const overflow = product < -2147483648LL || product > 2147483647LL;
	u64 const result = u64(f.st1 & 0x200 ? std::clamp(product, s64(-2147483648LL), s64(2147483647LL)) : product) & 0xffffffffffULL;
	c.result_a = f.destination ? c.a : result;
	c.result_b = f.destination ? result : c.b;
	f.result_st0 = f.st0 | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	return c;
}

tms320c54x_test_state::sqdst_fixture tms320c54x_test_state::sqdst_case(unsigned variant, bool lms)
{
	bool const numeric = variant >= 9216;
	unsigned const n = numeric ? variant - 9216 : variant;
	sqdst_fixture c{};
	auto &f = c.memory;
	f = dual_multiply_case(numeric ? 0 : n % 256 + 512 * (n / 256), false);
	static constexpr std::array<u16, 8> words{0, 1, 0xffff, 0x8000, 0x7fff, 0x1234, 0x5555, 0xaaaa};
	if (numeric)
	{
		f.x = 0; f.y = 1; f.xaddress = 0x1000; f.yaddress = 0x1100;
		f.initial = {0x1000, 0x1100, 0x1200, 0x1300}; f.final = f.initial;
		f.xword = words[n % 8]; f.yword = words[(n / 8) % 8];
		f.flags = (n / 4096) % 32; f.snapshot = n / 131072;
		f.base = (n / 64) & 1 ? 0x20000 : 0; f.linear = true;
		f.st0 = 0xa002 | (f.flags & 16 ? 0x1e00 : 0);
		f.st1 = 0x800 | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0) | (f.flags & 8 ? 0x100 : 0);
		f.pmst = f.flags & 4 ? 2 : 0;
	}
	c.a = std::array<u64, 8>{0, 0x007fff0000ULL, 0x0080000000ULL, 0x00ffff0000ULL, 0x0100000000ULL, 0x017fff0000ULL, 0xff80000000ULL, 0xffffffffffULL}[(n / (numeric ? 64 : 128)) % 8];
	c.b = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x0180000000ULL, 0xfe7fffffffULL, 0x123456789aULL, 0xffffffffffULL}[(n / 512) % 8];
	s64 const high = s64((c.a >> 16) & 0xffff) - (c.a & (u64(1) << 32) ? 65536 : 0);
	s64 square = high * high * (f.flags & 1 ? 2 : 1);
	if ((f.flags & 7) == 7) square = std::min(square, s64(2147483647LL));
	s64 const sum = (c.b & (u64(1) << 39) ? s64(c.b) - (s64(1) << 40) : s64(c.b)) + square;
	s64 const difference = (s64(f.flags & 8 ? s16(f.xword) : s32(f.xword)) - s64(f.flags & 8 ? s16(f.yword) : s32(f.yword))) * 65536;
	bool const ova = difference < -2147483648LL || difference > 2147483647LL;
	bool const ovb = sum < -2147483648LL || sum > 2147483647LL;
	c.result_a = u64((f.flags & 2) ? std::clamp(difference, s64(-2147483648LL), s64(2147483647LL)) : difference) & 0xffffffffffULL;
	c.result_b = u64((f.flags & 2) ? std::clamp(sum, s64(-2147483648LL), s64(2147483647LL)) : sum) & 0xffffffffffULL;
	c.result_st0 = (f.st0 & ~0x800) | (f.xword >= f.yword ? 0x800 : 0) | (ova ? 0x400 : 0) | (ovb ? 0x200 : 0);
	if (lms)
	{
		s64 const operand = s64(f.flags & 8 ? s16(f.xword) : s32(f.xword)) * 65536 + 32768;
		s64 const a = (c.a & 0x8000000000ULL ? s64(c.a)-0x10000000000LL : s64(c.a)) + operand;
		s64 product = s64(s16(f.xword)) * s16(f.yword) * (f.flags & 1 ? 2 : 1);
		if ((f.flags & 7) == 7) product = std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
		s64 const b = (c.b & 0x8000000000ULL ? s64(c.b)-0x10000000000LL : s64(c.b)) + product;
		bool const carry = u64(u32(c.a)) + u32(operand) > 0xffffffffULL;
		c.result_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0) | (a < -2147483648LL || a > 2147483647LL ? 0x400 : 0) | (b < -2147483648LL || b > 2147483647LL ? 0x200 : 0);
		c.result_a = u64(f.flags & 2 ? std::clamp(a,s64(-2147483648LL),s64(2147483647LL)) : a) & 0xffffffffffULL;
		c.result_b = u64(f.flags & 2 ? std::clamp(b,s64(-2147483648LL),s64(2147483647LL)) : b) & 0xffffffffffULL;
	}
	return c;
}

void tms320c54x_test_state::start_sqdst(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const lms = !strncmp(machine().options().bios(), "lms", 3);
	auto const c = sqdst_case(variant, lms); auto const &f = c.memory;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, (lms ? 0xe100 : 0xe200) | (f.x << 4) | f.y); program.write_word(f.base + 0x601, 0xf4e1);
	if (f.xaddress >= 0x60) data.write_word(f.xaddress, f.xword);
	if (f.yaddress >= 0x60) data.write_word(f.yaddress, f.yword);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, c.a ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, c.b ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	for (unsigned i = 0; i < 4; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR2 + i, f.initial[i]);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.linear ? 0 : 16);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "sqdst_reads", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_operand_reads < 2 && at == (m_pipeline_operand_reads ? f.yaddress : f.xaddress) && word == (m_pipeline_operand_reads ? f.yword : f.xword) && m_cpu->pipeline_cycles() == 5, "SQDST reads both retained operands once"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "sqdst_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "SQDST has no data write"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x600, "sqdst_fetch", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "SQDST opcode fetched once"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62142;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

std::array<u64, 2> tms320c54x_test_state::addc_expected(u64 source, u16 word, u16 st0, u16 st1, bool b, unsigned family)
{
	s64 const left = source & 0x8000000000ULL ? s64(source) - (s64(1) << 40) : s64(source);
	u64 const increment = u64(word) + (family ? 0 : (st0 >> 11) & 1);
	s64 const sum = family == 2 ? left - s64(increment) : left + s64(increment);
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = family == 2 ? (source % 0x100000000ULL) >= increment : (source % 0x100000000ULL) + increment >= 0x100000000ULL;
	u16 const status = (st0 & ~u16(0x800)) | (carry ? 0x800 : 0) | (overflow ? b ? 0x200 : 0x400 : 0);
	u64 const result = u64(overflow && (st1 & 0x200) ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
	return {result, status};
}

tms320c54x_test_state::addc_fixture tms320c54x_test_state::addc_case(unsigned v, unsigned family)
{
	static constexpr std::array<u64, 8> accumulators{0, 0xffffffffULL, 0x7fffffffULL, 0xff80000000ULL, 0x7fffffffffULL, 0x8000000000ULL, 0xffff, 0x123456789aULL};
	static constexpr std::array<u16, 8> words{0, 1, 0x7fff, 0x8000, 0xffff, 0xaaaa, 0x5555, 0xfffe};
	addc_fixture f{};
	unsigned form, pattern = 0, flags = 0, destination = 0, page = 0, mode = 0;
	bool const mapped = v >= 73728;
	if (!mapped)
	{
		form = v % 6; v /= 6; f.word = words[v % 8]; v /= 8; pattern = v % 8; v /= 8;
		flags = v % 8; v /= 8; destination = v % 2; v /= 2; f.late = v % 2; v /= 2; page = v % 2; v /= 2; f.snapshot = v;
	}
	else
	{
		v -= 73728; f.address = 8 + v % 16; v /= 16; form = v % 3; v /= 3;
		pattern = v % 4; v /= 4; flags = v % 8; v /= 8; destination = v % 2; v /= 2;
		f.late = v % 2; v /= 2; page = v % 2; v /= 2; f.snapshot = v;
	}
	f.a = destination ? 0x123456789aULL : accumulators[pattern]; f.b = destination ? accumulators[pattern] : 0xfeabcd9876ULL;
	f.t = words[pattern]; f.trn = words[(pattern + 3) % 8]; f.base = page ? 0x20000 : 0;
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x1200 + 0x100 * i;
	f.initial[0] = 1;
	f.st0 = 0x1000 | (mapped && !form ? 0 : 0x20) | (flags & 1 ? 0x800 : 0) |
		(pattern & 1 ? (destination ? 0x400 : 0x200) : 0) | (pattern & 2 ? (destination ? 0x200 : 0x400) : 0);
	f.st1 = 0x800 | (flags & 2 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0);
	if (mapped)
	{
		mode = form == 0 ? f.address : form == 1 ? 0xf8 : 0x83;
		f.extension = form == 1; f.literal = f.address;
		if (form == 2) f.initial[3] = f.address;
		if (mapped) f.word = f.address < 14 ? u16((f.address < 11 ? f.a : f.b) >> (16 * ((f.address - 8) % 3))) & ((f.address - 8) % 3 == 2 ? 0xff : 0xffff) : f.address == 14 ? f.t : f.address == 15 ? f.trn : f.initial[f.address - 16];
	}
	else
	{
		mode = std::array<unsigned, 6>{2, 2, 0xf8, 0x93, 0xe3, 0xd3}[form];
		f.extension = form == 2 || form == 4; f.literal = form == 4 ? 2 : 0x1002;
		f.address = form == 1 ? 0x3002 : form == 5 ? 0x100c : 0x1002;
		f.initial[3] = form == 4 ? 0x1000 : f.address; if (form == 1) f.st1 |= 0x4000; if (form == 5) f.bk = 13;
	}
	f.next_ar3 = !mapped && form == 3 ? 0x1003 : !mapped && form == 5 ? 0x1000 : f.initial[3];
	f.opcode = (family == 1 ? 0x0200 : family == 2 ? 0x0a00 : 0x0600) | (destination << 8) | mode; f.read_clock = f.extension ? 6 : 5;
	f.after_a = f.a; f.after_b = f.b;
	if (mapped && f.address < 14)
	{
		unsigned const part = (f.address - 8) % 3; u64 const mask = part == 2 ? 0xff : 0xffff;
		u64 &source = f.address < 11 ? f.after_a : f.after_b;
		source = (source & ~(mask << (16 * part))) | ((0xa55aULL & mask) << (16 * part));
	}
	if (f.late) (destination ? f.after_b : f.after_a) = accumulators[(pattern + 1) % 8];
	f.execute_st0 = f.st0 ^ (f.late ? 0x800 : 0);
	u64 const source = destination ? f.after_b : f.after_a;
	auto const expected = addc_expected(source, f.word, f.execute_st0, f.st1, destination, family);
	f.final_st0 = expected[1]; f.final_a = destination ? f.after_a : expected[0]; f.final_b = destination ? expected[0] : f.after_b;
	return f;
}

tms320c54x_test_state::addc_fixture tms320c54x_test_state::mapped_shift16_case(unsigned variant)
{
	unsigned const extent = 36864, selector = variant / extent, v = variant % extent;
	auto f = addc_case(73728 + v, 1);
	bool const source_b = selector & 1, subtract = selector & 2, destination_b = BIT(f.opcode, 8);
	if (source_b != destination_b) std::swap(f.a, f.b);
	if (f.address < 14) { unsigned const part = (f.address - 8) % 3; f.word = u16((f.address < 11 ? f.a : f.b) >> (16 * part)) & (part == 2 ? 0xff : 0xffff); }
	f.after_a = f.a; f.after_b = f.b;
	if (f.address < 14) { unsigned const part = (f.address - 8) % 3; u64 const mask = part == 2 ? 0xff : 0xffff; u64 &source = f.address < 11 ? f.after_a : f.after_b; source = (source & ~(mask << (16 * part))) | ((0xa55aULL & mask) << (16 * part)); }
	if (f.late) (source_b ? f.after_b : f.after_a) = std::array<u64, 5>{0, 0xffffffffULL, 0x7fffffffULL, 0xff80000000ULL, 0x7fffffffffULL}[1 + (v / 48) % 4];
	u64 const source = source_b ? f.after_b : f.after_a;
	s64 const left = source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source);
	s64 const right = s64(f.st1 & 0x100 ? s16(f.word) : s32(f.word)) * 65536;
	s64 result = subtract ? left - right : left + right;
	bool const carry = subtract ? u32(source) >= u32(right) : u64(u32(source)) + u32(right) > 0xffffffffULL;
	f.final_st0 = f.execute_st0;
	if (carry != subtract) f.final_st0 = (f.final_st0 & ~0x800) | (carry ? 0x800 : 0);
	if (result < -2147483648LL || result > 2147483647LL) { f.final_st0 |= destination_b ? 0x200 : 0x400; if (f.st1 & 0x200) result = std::clamp(result, s64(-2147483648LL), s64(2147483647LL)); }
	f.final_a = destination_b ? f.after_a : u64(result) & 0xffffffffffULL;
	f.final_b = destination_b ? u64(result) & 0xffffffffffULL : f.after_b;
	f.opcode = (subtract ? 0x4000 : 0x3c00) | (unsigned(source_b) << 9) | (unsigned(destination_b) << 8) | u8(f.opcode);
	return f;
}

tms320c54x_test_state::addc_fixture tms320c54x_test_state::mapped_logic_case(unsigned variant)
{
	unsigned const extent = 36864, operation = variant / extent, v = variant % extent;
	auto f = addc_case(73728 + v, 1);
	bool const b = BIT(f.opcode, 8); u64 const source = b ? f.after_b : f.after_a;
	u64 const result = operation == 0 ? source & f.word : operation == 1 ? source | f.word : source ^ f.word;
	f.final_a = b ? f.after_a : result; f.final_b = b ? result : f.after_b; f.final_st0 = f.execute_st0;
	f.opcode = 0x1800 | (operation << 9) | (unsigned(b) << 8) | u8(f.opcode);
	return f;
}

void tms320c54x_test_state::start_addc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const arithmetic = profile_is("unsigned-map");
	bool const shift16 = profile_is("shift16-map");
	bool const logic = profile_is("logic-map");
	unsigned const extent = shift16 || logic ? 36864 : 110592;
	auto const f = logic ? mapped_logic_case(variant) : shift16 ? mapped_shift16_case(variant) : addc_case(variant % extent, arithmetic ? 1 + variant / extent : 0);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.opcode); if (f.extension) program.write_word(f.base + 0x601, f.literal);
	program.write_word(f.base + 0x601 + unsigned(f.extension), 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, f.trn);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	if (f.address >= 0x60) data.write_word(f.address, f.word);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "unsigned_sample", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads++ && at == f.address && word == f.word && m_cpu->pipeline_cycles() == f.read_clock, "Unsigned arithmetic retains one real source in Read"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "unsigned_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "Unsigned arithmetic writes no data memory"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "unsigned_literal", [this, f](offs_t, u16 &word, u16) { if (!machine().side_effects_disabled() && f.extension) { expect(!m_pipeline_extensions_reads++ && word == f.literal, "Unsigned arithmetic fetches its address extension once"); } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62156;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::addc_chain_fixture tms320c54x_test_state::addc_chain_case(unsigned v)
{
	addc_chain_fixture f{};
	unsigned const selector = v % 4; v /= 4; unsigned const pattern = v % 4; v /= 4; unsigned const pair = v % 4; v /= 4;
	unsigned const flags = v % 8; v /= 8; f.base = v % 2 ? 0x20000 : 0; v /= 2; f.checkpoint = std::array<unsigned, 4>{3, 5, 6, 7}[v];
	f.a = std::array<u64, 4>{0xffffffffULL, 0, 0x7fffffffULL, 0xff80000000ULL}[pattern];
	f.b = std::array<u64, 4>{0x123456789aULL, 0xffffffffULL, 0x7fffffffULL, 0x8000000000ULL}[pattern];
	f.words = std::array<std::array<u16, 2>, 4>{{{1, 0}, {0xffff, 1}, {0x8000, 0xffff}, {0, 0x8000}}}[pair];
	f.st0 = 0x1020 | (flags & 1 ? 0x800 : 0); f.st1 = 0x800 | (flags & 2 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0);
	f.final_a = f.a; f.final_b = f.b; f.final_st0 = f.st0;
	for (unsigned i = 0; i < 2; ++i)
	{
		bool const b = (selector >> i) & 1; f.opcodes[i] = 0x0602 + i + (b ? 0x100 : 0);
		u64 &accumulator = b ? f.final_b : f.final_a;
		auto const expected = addc_expected(accumulator, f.words[i], f.final_st0, f.st1, b); accumulator = expected[0]; f.final_st0 = expected[1];
	}
	return f;
}

void tms320c54x_test_state::start_addc_chain(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = addc_chain_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	for (unsigned i = 0; i < 2; ++i) { program.write_word(f.base + 0x600 + i, f.opcodes[i]); data.write_word(0x1002 + i, f.words[i]); }
	program.write_word(f.base + 0x602, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1); m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "addc_chain_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_operand_reads++; expect(i < 2 && at == 0x1002 + i && word == f.words[i] && m_cpu->pipeline_cycles() == 5 + i, "Adjacent ADDC instructions retain their own unsigned word before the producer carry retires"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "addc_chain_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "ADDC chains write no data memory"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x601, "addc_chain_fetch", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_extensions_reads < 2 && at == f.base + 0x600 + m_pipeline_extensions_reads && word == f.opcodes[m_pipeline_extensions_reads], "ADDC chain fetches each opcode once"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62158;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::xmem_arithmetic_result tms320c54x_test_state::xmem_arithmetic_expected(unsigned variant)
{
	auto const f = xmem_load_case(variant % 196608);
	u64 const source = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0xffffffffULL, 0x7fffffffffULL, 0x8000000000ULL, 0x123456789aULL}[(variant / 256) % 8];
	u64 const operand = f.result();
	s64 const left = source & 0x8000000000ULL ? s64(source) - (s64(1) << 40) : s64(source);
	s64 const right = operand & 0x8000000000ULL ? s64(operand) - (s64(1) << 40) : s64(operand);
	bool const subtract = variant >= 196608;
	s64 const sum = subtract ? left - right : left + right;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = subtract ? u32(source) >= u32(operand) : u64(u32(source)) + u32(operand) > 0xffffffffULL;
	u16 const st0 = (f.st0() & ~0x0800) | (carry ? 0x0800 : 0) | (overflow ? f.destination ? 0x0200 : 0x0400 : 0);
	s64 const result = (f.flags & 2) && overflow ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum;
	return {source, u64(result) & 0xffffffffffULL, st0};
}

tms320c54x_test_state::mvdm_acc_fixture tms320c54x_test_state::mvdm_acc_case(unsigned v)
{
	unsigned const flags = (v / 1152) % 16;
	u64 const a = std::array<u64, 8>{0, 1, 0x7fffffff, 0x80000000, 0x7fffffffffULL, 0x8000000000ULL, 0xffffffffffULL, 0x123456789aULL}[(v / 144) % 8];
	return {v % 6, v / 36864, std::array<u16, 3>{0x500, 0x14, 0x19}[(v / 6) % 3],
		std::array<u16, 8>{0, 1, 0x7f, 0x80, 0xff, 0x7fff, 0x8000, 0xffff}[(v / 18) % 8],
		u16(0x800 | ((flags & 1) ? 0x100 : 0) | ((flags & 2) ? 0x200 : 0) | ((flags & 4) ? 0x4000 : 0) | ((flags & 8) ? 0x40 : 0)),
		(v / 18432) % 2 ? 0x20000U : 0U, a, a ^ 0x5aa55aa55aULL};
}

tms320c54x_test_state::mac_smem_fixture tms320c54x_test_state::mac_smem_case(unsigned variant, bool bk, bool square)
{
	unsigned const forms = bk ? 2 : 3, selectors = square ? 4 : 8, v = variant % (forms * selectors * 1024), form = v % forms, selector = (v / forms) % selectors, pair = (v / (forms * selectors)) % 64, flags = v / (forms * selectors * 64);
	unsigned const operation = selector / 2, tp = pair / 8, mp = pair % 8;
	u16 const t = std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8001, 0x1234, 0xabcd}[tp];
	u16 const memory = std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8001, 0x5678, 0xfedc}[mp];
	u64 const source = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1234567833ULL, 0x80aa001234ULL}[tp];
	u16 const st0 = (0xa604 | (tp & 1 ? 0x600 : 0) | (mp & 1 ? 0x800 : 0)) & (bk ? 0xfe00 : 0xffff);
	u16 const st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 8 ? 0x100 : 0);
	s64 product = s64(square ? s16(memory) : s16(t)) * s16(memory) * (flags & 1 ? 2 : 1);
	if (!square && (flags & 7) == 7) product = std::clamp(product, s64(-2147483648LL), s64(2147483647LL));
	s64 const signed_source = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
	s64 result = operation >= (square ? 1U : 2U) ? signed_source - product : signed_source + product;
	if (!square && (operation & 1)) result = (result + 32768) & ~s64(65535);
	bool const overflow = result < -2147483648LL || result > 2147483647LL;
	u64 const stored = u64(overflow && (flags & 2) ? result < 0 ? -2147483648LL : 2147483647LL : result) & 0xffffffffffULL;
	mac_smem_fixture f{u16(((square ? 0x3800 : 0x2800) + operation * 0x200) | ((selector & 1) << 8) | (form ? 0xf8 : bk ? 0x19 : 0x81)), memory, t, st0, st1, u16(flags & 4 ? 2 : 0), u16(bk ? 0x19 : form == 2 ? 0x11 : 0x500), u16(st0 | (overflow ? selector & 1 ? 0x200 : 0x400 : 0)), selector & 1 ? 0x246813579aULL : source, selector & 1 ? source : 0x246813579aULL, stored, 5 + unsigned(form != 0)};
	f.base = bk && (variant / 16384) % 2 ? 0x20000 : 0;
	return f;
}

tms320c54x_test_state::poly_fixture tms320c54x_test_state::poly_case(unsigned variant) const
{
	if (profile_is("mac-rpt"))
	{
		unsigned v = variant; poly_fixture f{}; f.dual = true;
		f.form = v % 4; v /= 4; f.operation = v % 4; v /= 4; f.selector = v % 4; v /= 4;
		unsigned const pattern = v % 4; v /= 4; unsigned const flags = v % 16; v /= 16;
		f.iterations = std::array<unsigned,4>{1,2,8,256}[v % 4]; v /= 4;
		f.base = v % 2 ? 0x20000 : 0; v /= 2;
		f.checkpoint = std::array<unsigned,3>{5,6+f.iterations/2,6+f.iterations}[v % 3];
		f.st0 = 0x1030 | (pattern & 1 ? 0x400 : 0) | (pattern & 2 ? 0xa00 : 0);
		f.st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 8 ? 0x100 : 0);
		f.pmst = flags & 4 ? 2 : 0; f.t = 0x5555;
		f.xseed = std::array<u16,4>{0x8000,0x7fff,0xffff,0x1234}[pattern];
		f.yseed = std::array<u16,4>{0x8000,1,0xffff,0x7fff}[pattern];
		f.a = std::array<u64,4>{0,0x7fffffff,0xff80000000,0x8000000000}[pattern];
		f.b = std::array<u64,4>{0xffffffffff,0x0180000000,0,0x123456789a}[pattern];
		return f;
	}
	if (profile_is("sqacc-repeat") || profile_is("tmac-repeat"))
	{
		unsigned v = variant; poly_fixture f{}; f.square = true;
		f.tmac = profile_is("tmac-repeat");
		unsigned const operations = f.tmac ? 8 : 4;
		f.form = v % 3; v /= 3; f.operation = v % operations; v /= operations;
		unsigned const pattern = v % 8; v /= 8; unsigned const flags = v % 16; v /= 16;
		f.iterations = std::array<unsigned,4>{1,2,8,256}[v % 4]; v /= 4;
		f.base = v % 2 ? 0x20000 : 0; v /= 2;
		f.checkpoint = std::array<unsigned,4>{5,6,7+f.iterations/2,6+f.iterations}[v % 4];
		f.st0 = 0x1030 | (pattern & 1 ? 0x400 : 0) | (pattern & 2 ? 0x200 : 0) | (pattern & 4 ? 0x800 : 0);
		f.st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0) | (flags & 8 ? 0x80 : 0);
		if (f.tmac) f.pmst = flags & 8 ? 2 : 0;
		f.t = std::array<u16,8>{0,1,0x5678,0x8000,0xffff,0x7fff,0x8000,0x1234}[pattern];
		f.a = std::array<u64,8>{0,0x12340000,0x12340000,0x0080000000,0x0180000000,0xff80000000,0xff00000000,0x7fffffffff}[pattern];
		f.b = std::array<u64,8>{0,0x8000,0x10000,0x7fffffff,0xff80000000,0x7fff8000,0xffffffffff,0x7fffffffff}[pattern];
		return f;
	}
	unsigned const form = variant % 3, pattern = (variant / 3) % 8, flags = (variant / 24) % 8, repeat = (variant / 192) % 3;
	return {form, repeat == 2 ? 3U : 1U, std::array<unsigned, 4>{4,6,8,12}[variant / 1152],
		u32((variant / 576) % 2 ? 0x20000 : 0),
		u16(0x1a30 | (pattern & 1 ? 0x0400 : 0)),
		u16(0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0)),
		std::array<u16,8>{0,1,0x5678,0x8000,0xffff,0x7fff,0x8000,0x1234}[pattern],
		std::array<u64,8>{0,0x12340000,0x12340000,0x0080000000,0x0180000000,0xff80000000,0xff00000000,0x7fffffffff}[pattern],
		std::array<u64,8>{0,0x8000,0x10000,0x7fffffff,0xff80000000,0x7fff8000,0xffffffffff,0x7fffffffff}[pattern]};
}

u16 tms320c54x_test_state::poly_memory(unsigned ordinal)
{ return std::array<u16,3>{0x8000,0xffff,0x1234}[ordinal]; }

u16 tms320c54x_test_state::mac_repeat_address(poly_fixture const &f, unsigned ordinal, bool y)
{
	if (y && f.form != 3) return 0x1c00 + (f.form == 1 || f.form == 2 ? ordinal : 0);
	return 0x1800 + (f.form == 2 ? -int(ordinal) : f.form ? int(ordinal) : 0);
}

u16 tms320c54x_test_state::mac_repeat_memory(poly_fixture const &f, unsigned ordinal, bool y)
{
	if (f.form == 3) y = false;
	return (y ? f.yseed : f.xseed) ^ u16(ordinal * (y ? 0x2222 : 0x1111));
}

std::array<u64,3> tms320c54x_test_state::mac_repeat_step(poly_fixture const &f, std::array<u64,3> state, unsigned ordinal)
{
	u64 const source = state[f.selector >> 1]; unsigned const destination = f.selector & 1;
	s64 product = s64(s16(mac_repeat_memory(f,ordinal,false))) * s16(mac_repeat_memory(f,ordinal,true)) * (f.st1 & 0x40 ? 2 : 1);
	if ((f.pmst & 2) && (f.st1 & 0x240) == 0x240) product = std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
	s64 result = (source & 0x8000000000ULL ? s64(source)-0x10000000000LL : s64(source)) + (f.operation & 2 ? -product : product);
	if (f.operation & 1) { s64 const biased = result+32768; result = (biased >= 0 ? biased/65536 : -((-biased+65535)/65536))*65536; }
	if (result < -2147483648LL || result > 2147483647LL)
	{
		state[2] |= destination ? 0x200 : 0x400;
		if (f.st1 & 0x200) result = std::clamp(result,s64(-2147483648LL),s64(2147483647LL));
	}
	state[destination] = u64(result) & 0xffffffffffULL; return state;
}

std::array<u64,3> tms320c54x_test_state::poly_reference(poly_fixture const &f)
{
	u64 a = f.a, b = f.b; u16 st0 = f.st0;
	for (unsigned i = 0; i < f.iterations; ++i)
	{
		s64 high = (a >> 16) & 0x1ffff; if (high & 0x10000) high -= 0x20000;
		s64 old_b = b; if (b & 0x8000000000ULL) old_b -= 0x10000000000LL;
		s64 result = high * s16(f.t) * (BIT(f.st1,6) ? 2 : 1) + old_b + 0x8000;
		result = (result >> 16) * 0x10000;
		if (result < -0x80000000LL || result > 0x7fffffffLL)
		{
			st0 |= 0x400;
			if (BIT(f.st1,9)) result = result < 0 ? -0x80000000LL : 0x7fffffffLL;
		}
		a = u64(result) & 0xffffffffffULL;
		u16 const memory = poly_memory(f.form == 2 ? i : 0);
		s64 const coefficient = BIT(f.st1,8) ? s16(memory) : s64(memory);
		b = u64(coefficient * 0x10000) & 0xffffffffffULL;
	}
	return {a,b,st0};
}

void tms320c54x_test_state::start_poly(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = poly_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	unsigned const repeat = f.square || f.dual ? 1 : (variant / 192) % 3;
	program.write_word(f.base + 0x600, repeat ? 0xec00 | (f.iterations - 1) : 0xf495);
	u16 const mode = f.dual ? 0 : std::array<u16,3>{0,0x82,0x92}[f.form];
	program.write_word(f.base+0x601,f.dual ? 0xb000 | (f.operation << 10) | (f.selector << 8) | std::array<u16,4>{0x01,0x9a,0x49,0x84}[f.form] :
		f.square ? (f.tmac ? 0x2800 : 0x3800) | (f.operation << 8) | mode : 0x3600 | mode);
	program.write_word(f.base + 0x602, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0,f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T,f.t); m_cpu->set_state_int(tms320c54x_device::STATE_AR2,0x1800);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0);
	m_cpu->set_input_line(0,CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI,CLEAR_LINE);
	for (unsigned i = 0; i < 3; ++i) data.write_word(0x1800+i,poly_memory(i));
	if (f.square)
	{
		for (unsigned i = 0; i < f.iterations; ++i) data.write_word(0x1800+i, square_repeat_memory(f,i));
		m_repeat_load_expected = {{{f.a, f.b, f.st0}}};
		for (unsigned i = 0; i < f.iterations; ++i) m_repeat_load_expected.push_back(square_repeat_step(f, m_repeat_load_expected.back(), i));
	}
	if (f.dual)
	{
		for (unsigned ar = 2; ar < 6; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0+ar,0x4800+ar);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0+(f.form == 1 ? 3 : 2),0x1800);
		if (f.form != 3) m_cpu->set_state_int(tms320c54x_device::STATE_AR0+(f.form == 1 ? 4 : 3),0x1c00);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0,3); m_cpu->set_state_int(tms320c54x_device::STATE_BK,0);
		for (unsigned i = 0; i < (f.form ? f.iterations : 1U); ++i) for (bool y : {false,true}) data.write_word(mac_repeat_address(f,i,y),mac_repeat_memory(f,i,y));
		m_repeat_load_expected = {{{f.a,f.b,f.st0}}};
		for (unsigned i = 0; i < f.iterations; ++i) m_repeat_load_expected.push_back(mac_repeat_step(f,m_repeat_load_expected.back(),i));
	}
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"poly_source",[this,f](offs_t at,u16 &word,u16)
	{
		if (machine().side_effects_disabled()) return;
		if (f.dual)
		{
			unsigned const ordinal = m_pipeline_operand_reads++, i = ordinal/2; bool const y = ordinal & 1;
			expect(i < f.iterations && at == mac_repeat_address(f,i,y) && word == mac_repeat_memory(f,i,y) && m_cpu->pipeline_cycles() == 6+i, "Repeated dual MAC retains both ordered Read samples on each clock");
			if (y)
			{
				auto const disable = machine().disable_side_effects();
				for (bool side : {false,true}) m_cpu->space(AS_DATA).write_word(mac_repeat_address(f,i,side),f.form ? u16(~mac_repeat_memory(f,i,side)) : mac_repeat_memory(f,i+1,side));
			}
			return;
		}
		if (f.square)
		{
			unsigned const i = m_pipeline_operand_reads++;
			expect(i < f.iterations && at == 0x1800 + (f.form == 2 ? i : 0) && word == square_repeat_memory(f,i), "Repeated square owns each fresh RAM read in order");
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, u16(~word)); return;
		}
		unsigned const i = f.form == 2 ? m_pipeline_operand_reads : 0;
		expect(m_pipeline_operand_reads < f.iterations && at == 0x1800+i && word == poly_memory(i),"POLY owns one ordered coefficient read per iteration"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"poly_no_write",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false,"POLY must not write data memory"); } });
	m_pipeline_extensions_reads = 0;
	if (f.square || f.dual) m_pipeline_fetch_tap = program.install_read_tap(f.base+0x601,f.base+0x601,"repeated_arithmetic_opcode",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) expect(!m_pipeline_extensions_reads++, "Repeated arithmetic fetches its body opcode only once"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 399980;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

tms320c54x_test_state::register_product_fixture tms320c54x_test_state::register_product_case(unsigned variant)
{
	if (variant >= 176640)
	{
		// Separate domains: POLY excludes SMUL; immediate/absolute products
		// include extension ownership; the final bank isolates EXP producers.
		unsigned v = variant - 176640;
		bool const poly = v < 2304;
		bool const immediate = !poly && v < 29952;
		bool const extended = v >= 29952 && v < 94464;
		bool const exponent = v >= 94464;
		v -= poly ? 0 : immediate ? 2304 : extended ? 29952 : 94464;
		unsigned const operation = v % (poly ? 1 : immediate ? 6 : extended ? 14 : 46); v /= poly ? 1 : immediate ? 6 : extended ? 14 : 46;
		unsigned const pattern = v % 8; v /= 8;
		unsigned flags = v % (poly ? 8 : 16); v /= poly ? 8 : 16;
		if (poly) flags = (flags & 3) | ((flags & 4) << 1); // POLY SMUL-set arithmetic is not validated.
		unsigned const mode = exponent ? 5 : v % 6; if (!exponent) v /= 6;
		unsigned const page = v % 2; v /= 2;
		auto f = register_product_case((exponent && operation < 18 ? operation : 0) + 18 * (pattern + 8 * (flags + 16 * ((mode == 5 ? 0 : mode) + 5 * (page + 2 * v)))));
		f.mode = mode; f.poly = poly; f.immediate = immediate;
		f.length = immediate || extended ? 2 : 1;
		f.checkpoint = std::array<unsigned,3>{4,6+f.length,7+f.length}[v];
		f.smem = extended || (exponent && operation >= 18);
		f.mapped_t = exponent && operation >= 32;
		if (poly) f.opcode = 0x3681;
		else if (immediate) f.opcode = std::array<u16,6>{0xf066,0xf166,0xf067,0xf167,0xf267,0xf367}[operation];
		else if (f.smem) f.opcode = std::array<u16,14>{0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2800,0x2900,0x2a00,0x2b00,0x2c00,0x2d00,0x2e00,0x2f00}[extended ? operation : (operation-18)%14] | (extended ? 0xf8 : f.mapped_t ? 0x0e : 0x81);
		if (f.mapped_t) f.st0 &= ~0x1ff;
		return f;
	}
	if (variant >= 69120)
	{
		unsigned v = variant - 69120, operation = v % 14; v /= 14;
		unsigned const pattern = v % 8; v /= 8; unsigned const flags = v % 16; v /= 16;
		unsigned const mode = v % 5; v /= 5; unsigned const page = v % 2; v /= 2;
		unsigned const snapshot = v % 3; v /= 3;
		auto f = register_product_case(18 * (pattern + 8 * (flags + 16 * (mode + 5 * (page + 2 * snapshot)))));
		f.smem = true; f.mapped_t = v;
		f.opcode = std::array<u16,14>{0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2800,0x2900,0x2a00,0x2b00,0x2c00,0x2d00,0x2e00,0x2f00}[operation] | (f.mapped_t ? 0x0e : 0x81);
		if (f.mapped_t) f.st0 &= ~0x1ff;
		return f;
	}
	unsigned v = variant, operation = v % 18; v /= 18;
	unsigned const pattern = v % 8; v /= 8; unsigned const flags = v % 16; v /= 16;
	unsigned const mode = v % 5; v /= 5; u32 const base = v % 2 ? 0x20000 : 0; v /= 2;
	u16 const opcode = operation < 2 ? 0xf48c | (operation << 8) : 0xf488 | ((operation-2)&3) | (((operation-2)&4)<<6) | (((operation-2)&8)<<6);
	return {opcode, std::array<u16,8>{0,1,0x4567,0x8000,0xffff,0x7fff,0x8001,0x1234}[pattern],
		u16(0x1804 | (pattern&1 ? 0x600 : 0)), u16(0x800 | (flags&1 ? 0x40 : 0) | (flags&2 ? 0x200 : 0) | (flags&8 ? 0x100 : 0)), u16(flags&4 ? 2 : 0),
		std::array<u64,8>{0,0x12340000,0xff87651111,0x0080000000,0x0180000000,0xff80000000,0xff00000000,0x7fffffffff}[pattern],
		std::array<u64,8>{0x1234,0x7fff8000,0x3200000,0xff80000000,0x7fffffff,0xffffffffff,0x80aa001234,0x7fffffffff}[pattern],base,mode,std::array<unsigned,3>{4,7,8}[v]};
}

u16 tms320c54x_test_state::product_input_t(register_product_fixture const &f)
{
	if (f.mode == 1 || f.mode == 2) return u16(f.b);
	if (f.mode == 5) return exponent_fixture{f.a,false,false,0,f.st0,f.st1,f.base}.exponent();
	return f.t;
}

std::array<u64,3> tms320c54x_test_state::register_product_reference(register_product_fixture const &f)
{
	u64 const a = f.mode == 3 ? 1 : f.a;
	u16 const t = product_input_t(f);
	s64 high = (a >> 16) & 0x1ffff; if (high & 0x10000) high -= 0x20000;
	unsigned const family = (f.opcode >> 8) & 0x0e;
	u16 const memory = f.mapped_t ? t : 0x8001;
	s64 product = (f.immediate ? s64(s16(t)) * s16(memory) : f.smem ? family == 4 ? s64(t) * memory : s64(s16(t)) * s16(memory) : high * s16(t)) * (BIT(f.st1,6) ? 2 : 1);
	bool const multiply = f.poly ? false : f.immediate ? (f.opcode & 0xfeff) == 0xf066 : f.smem ? family <= 4 : (f.opcode & 0xfeff) == 0xf48c;
	if (!multiply && (f.pmst&2) && (f.st1&0x240)==0x240) product = std::clamp(product,s64(-0x80000000LL),s64(0x7fffffffLL));
	u64 const source = f.poly || BIT(f.opcode,f.smem ? 8 : 9) ? f.b : a;
	s64 signed_source = source; if (source & 0x8000000000ULL) signed_source -= 0x10000000000LL;
	s64 result = multiply ? product : signed_source + ((!f.poly && !f.immediate && (f.smem ? family >= 12 : BIT(f.opcode,1))) ? -product : product);
	if (f.poly || (!f.immediate && (f.smem ? family == 2 || family == 10 || family == 14 : !multiply && BIT(f.opcode,0)))) result = ((result+0x8000)>>16)*0x10000;
	u16 st0 = f.st0;
	if (result < -0x80000000LL || result > 0x7fffffffLL)
	{
		st0 |= BIT(f.opcode,8) ? 0x200 : 0x400;
		if (BIT(f.st1,9)) result = result < 0 ? -0x80000000LL : 0x7fffffffLL;
	}
	u64 const stored = u64(result) & 0xffffffffffULL;
	if (f.poly) return {stored,u64((BIT(f.st1,8) ? s64(s16(memory)) : s64(memory))*0x10000)&0xffffffffffULL,st0};
	return {BIT(f.opcode,8) ? a : stored, BIT(f.opcode,8) ? stored : f.b, st0};
}

void tms320c54x_test_state::start_register_product(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	auto const f = register_product_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i=0;i<16;++i) program.write_word(f.base+0x600+i,0xf495);
	if (f.mode == 1 || f.mode == 2) program.write_word(f.base+0x600+(f.mode==1),0x890e);
	if (f.mode == 3) program.write_word(f.base+0x601,0xe801);
	if (f.mode == 5) program.write_word(f.base+0x601,0xf48e);
	program.write_word(f.base+0x602,f.opcode);
	if (f.length == 2) program.write_word(f.base+0x603,f.immediate ? 0x8001 : 0x1800);
	program.write_word(f.base+0x602+f.length,0xf4e1);
	if (f.mode == 4) { program.write_word(f.base+0x602+f.length,0x770e); program.write_word(f.base+0x603+f.length,0xbeef); program.write_word(f.base+0x604+f.length,0xf4e1); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0,f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T,f.t);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1,0x1800); data.write_word(0x1800,0x8001);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0);
	m_cpu->set_input_line(0,CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI,CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"product_source",[this,f](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		u16 const t = product_input_t(f);
		expect((f.smem || f.poly) && !m_pipeline_operand_reads && at==(f.mapped_t ? 0x0e : 0x1800) && word==(f.mapped_t ? t : 0x8001) && m_cpu->pipeline_cycles()==6+f.length,"Product reads its RAM or zero-latency mapped T source once in Read"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"register_product_t_producer",[this,f](offs_t at,u16 &word,u16)
	{ if (machine().side_effects_disabled()) return; expect((f.mode==1 || f.mode==2 || f.mode==4) && !m_pipeline_store_writes && at==0x0e && word==(f.mode==4 ? 0xbeef : u16(f.b)),"Only the fixture's STLM or following STM may write T"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base+0x602,f.base+0x601+f.length,"product_fetch",[this,f](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		if (at==f.base+0x602) { expect(!m_pipeline_extensions_reads && word==f.opcode,"Product owns one opcode fetch"); ++m_pipeline_extensions_reads; }
		else { expect(!m_pipeline_register_writes && word==(f.immediate ? 0x8001 : 0x1800),"Product owns one retained immediate or source-address extension"); ++m_pipeline_register_writes; }
	});
	m_pipeline_replay=false; m_idle_fixture_tick=0; m_phase=400000;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

void tms320c54x_test_state::start_mac_smem(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const bk = profile_is("mac-bk");
	bool const square = !bk && variant >= 73728;
	auto const f = mac_smem_case(square ? variant - 73728 : variant, bk, square); bool const absolute = u8(f.opcode) == 0xf8;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.opcode); if (absolute) program.write_word(f.base + 0x601, f.address);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x500);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); data.write_word(f.address, f.memory);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mac_smem_read",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads && at == f.address && word == f.memory && m_cpu->pipeline_cycles() == f.read, "MAC/MAS retain one RAM or mapped-register source in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mac_smem_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "MAC/MAS must not write data memory or republish T"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "mac_smem_literal",
		[this, f, absolute](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || !absolute) return; expect(!m_pipeline_extensions_reads && word == f.address, "MAC/MAS own one absolute source extension"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253690;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::near_accumulator_fixture() const
{ return profile_is("cala") || profile_is("cala-producer"); }

void tms320c54x_test_state::start_accumulator_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const b = variant & 1;
	bool const near = near_accumulator_fixture(), producer = profile_is("fcala-producer") || profile_is("cala-producer");
	unsigned const frame = near ? 1 : 2;
	unsigned const interrupt = producer ? 0 : variant / 72, prefix = producer ? 1 + variant / 72 : 0, continuation = 0x601 + prefix;
	unsigned const caller = std::array<unsigned, 3>{0, 2, 0x7f}[(variant / 2) % 3], hint = std::array<unsigned, 3>{0, 3, 0x7f}[(variant / 6) % 3], target = near ? caller : hint;
	u32 const base = caller << 16, destination = (target << 16) | 0x700;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(destination + i, 0xf495); }
	program.write_word(base + 0x600 + prefix, (near ? 0xf4e3 : 0xf4e7) | (b ? 0x100 : 0)); program.write_word(base + continuation, 0xf4e1); program.write_word(destination, 0x880e); program.write_word(destination + 1, near ? 0xfc00 : 0xf4e4);
	if (producer) { program.write_word(base + 0x600, b ? 0x5700 : 0x5600); m_cpu->space(AS_DATA).write_word(0x1400, 0x8000 | hint); m_cpu->space(AS_DATA).write_word(0x1401, 0x700); }
	program.write_word((target << 16) | 4, 0xf4eb); program.write_word((target << 16) | 0x40, 0xf4eb);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, producer ? 0xa628 : 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, interrupt == 1 ? 0 : 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, interrupt == 1 ? 1 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, b ? 0x1234 : 0x5500000700ULL | (hint << 16)); m_cpu->set_state_int(tms320c54x_device::STATE_B, b ? 0x5500000700ULL | (hint << 16) : 0x2345);
	if (producer) m_cpu->set_state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, 0xdeadbeef);
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_register_writes = 0;
	m_vector_acks = m_fast_stack_reads = m_pipeline_drained_fetches = 0;
	m_port_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "fcala_frame",
		[this, caller, b, interrupt, prefix, continuation, frame](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x0e) { expect(!m_pipeline_register_writes && word == (b ? 0x1234 : 0xbeef) && (interrupt ? m_cpu->pipeline_cycles() >= 12 && m_vector_acks == 1 : m_cpu->pipeline_cycles() == 12 + prefix), "Accumulator-call callee Execute follows the six-cycle call or completed interrupt return"); ++m_pipeline_register_writes; return; } if (at == 0xfff - frame) { expect(interrupt && !m_pipeline_drained_fetches && word == 0x700 && m_pipeline_store_writes == frame, "IRQ/NMI stacks only the target continuation below the complete call frame"); ++m_pipeline_drained_fetches; return; } unsigned const part = m_pipeline_store_writes++; expect(part < frame && at == 0xfff - part && word == (part ? caller : continuation) && m_cpu->pipeline_cycles() == 5 + prefix + part, "Accumulator call retains exactly its PC or PC/XPC frame"); });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "fcala_return_frame",
		[this, caller, interrupt, producer, continuation, hint, frame](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x1400 || at == 0x1401) { unsigned const part = m_port_reads++; expect(producer && part < 2 && at == 0x1400 + part && word == (part ? 0x700 : 0x8000 | hint) && m_cpu->pipeline_cycles() == 5, "DLD samples both RAM words once before accumulator publication"); return; } if (at == 0xfff - frame) { expect(interrupt && !m_fast_stack_reads && word == 0x700, "RETE removes only its interrupt frame"); ++m_fast_stack_reads; return; } unsigned const part = m_pipeline_operand_reads++; expect(part < frame && at == 0x1000 - frame + part && word == (frame == 1 || part ? continuation : caller), "RET/FRET consumes exactly the owned accumulator-call frame"); });
	m_pipeline_fetch_tap = program.install_read_tap(destination, destination, "fcala_target",
		[this, interrupt, prefix, frame](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_store_writes == frame && (interrupt ? m_pipeline_extensions_reads < 2 : !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == 8 + prefix), "Accumulator-call target fetches are initial Fetch and, only after interruption, return refetch"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253790;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_mvmd_accumulator(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const status = profile_is("mvmd-st1");
	bool const b = variant & 1, store = variant >= 144;
	unsigned const gap = (variant / 8) % 3;
	u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 2) % 4], source = b ? 0x0b : 8;
	u32 const base = (variant / 24) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, store ? (b ? 0x800b : 0x8108) : 0x8a00 | source);
	program.write_word(base + 0x601 + gap, 0x7300 | source); program.write_word(base + 0x602 + gap, status ? 7 : 0x1400);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa600); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, b && store ? value : 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, !b && store ? value : 0xfedcba9876ULL);
	data.write_word(0x1000, value); data.write_word(0x1400, 0xaaaa);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mvmd_accumulator_reads",
		[this, source, value, gap, store](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x1000) { expect(!store && !m_fast_stack_reads && word == value && m_cpu->pipeline_cycles() == 5, "POPM reads one owned stack word"); ++m_fast_stack_reads; } else { expect(at == source && word == value && !m_pipeline_operand_reads && m_pipeline_register_writes == 1 && m_cpu->pipeline_cycles() == 6 + gap, "MVMD reads the published mapped accumulator word once with no mapped-write conflict"); ++m_pipeline_operand_reads; } });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mvmd_accumulator_publication",
		[this, source, value, gap, status](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == value, "Mapped producer and MVMD retain their exact word"); if (at == source) { expect(!m_pipeline_register_writes && m_cpu->pipeline_cycles() == 6, "Mapped producer publishes once in Execute"); ++m_pipeline_register_writes; } else { expect(at == (status ? 7 : 0x1400) && !m_pipeline_store_writes && m_cpu->pipeline_cycles() == 7 + gap, "MVMD publishes one retained mapped word at its owned move clock"); ++m_pipeline_store_writes; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253830;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::shifted_unowned_fixture() const
{ return profile_is("shift-unowned"); }

tms320c54x_test_state::parallel_sub_fraction_fixture tms320c54x_test_state::parallel_sub_fraction_case(unsigned index)
{
	static constexpr u64 values[] = { 1, 0x17fff, 0x18000, 0x1ffff,
		0xffffff0001ULL, 0xffffff8000ULL, 0xffffff7fffULL, 0xffffffffffULL };
	static constexpr u16 words[] = { 0, 1, 0x7fff, 0x8000 };
	return { values[index % 8], values[(index + 3) % 8], words[(index / 8) % 4],
		bool((index / 32) & 1), bool((index / 64) & 1) };
}

void tms320c54x_test_state::start_native_dual_sub(unsigned index)
{
	static constexpr u16 values[] = { 0, 1, 0x7fff, 0x8000, 0xffff, 0xfffb, 7, 0x1234 };
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	program.write_word(0x600, 0xa245); // SUB *AR2-,*AR3-,A.
	program.write_word(0x601, 0xf4e1);
	data.write_word(0x2000, values[index % 8]);
	data.write_word(0x2100, values[(index / 8) % 8]);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PC, 0x600}, {cpu_regs::STATE_A, 0x123456}, {cpu_regs::STATE_B, 0x56789a},
		{cpu_regs::STATE_AR2, 0x2000}, {cpu_regs::STATE_AR3, 0x2100}, {cpu_regs::STATE_T, 0x4444},
		{cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0x800 | ((index / 64) << 8)}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_cpu->set_pipeline_frontend(false);
	m_rom4_codec_replay = false; m_phase = 700000 + index;
	expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save native dual SUB fixture");
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_parallel_sub_fraction(unsigned index)
{
	auto const f = parallel_sub_fraction_case(index);
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	program.write_word(0x600, 0xc401 | (f.source_b ? 0x200 : 0) | (f.destination_b ? 0x100 : 0));
	program.write_word(0x601, 0xf4e1);
	data.write_word(0x2000, f.x); data.write_word(0x2100, 0xdead);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PC, 0x600}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, f.a}, {cpu_regs::STATE_B, f.b},
		{cpu_regs::STATE_AR2, 0x2000}, {cpu_regs::STATE_AR3, 0x2100}, {cpu_regs::STATE_ST0, 0},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900); // SXM, INTM; ASM=0, OVM=0.
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_cpu->set_pipeline_frontend(false);
	m_rom4_codec_replay = false; m_phase = 525000 + index;
	expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save parallel subtraction with fractional accumulator bits");
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::xorm_fixture tms320c54x_test_state::atomic_xorm_case(unsigned index)
{
	unsigned const form = index % 8, flags = (index / 128) % 4;
	return {std::array<u16, 8>{5,5,0x83,0x93,0x8b,0xdb,0xeb,0xf8}[form],
		std::array<u16, 8>{0x1505,0x1505,0x1500,0x1500,0x1500,0x1503,0x1502,0x1507}[form],
		std::array<u16, 8>{0x1500,0x1500,0x1500,0x1501,0x14ff,0x1501,0x1502,0x1500}[form],
		std::array<u16, 4>{0,0xffff,0xff,0x8000}[(index / 8) % 4],
		std::array<u16, 4>{0,0xffff,0x5555,0x8001}[(index / 32) % 4],
		u16(0x800 | ((flags & 1) << 8) | ((flags & 2) << 8) | (form == 1 ? 0x4000 : 0)), form >= 6 ? 3U : 2U};
}

void tms320c54x_test_state::start_atomic_xorm(unsigned index)
{
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	auto const f = atomic_xorm_case(index);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	program.write_word(0x600, 0x6a00 | f.mode);
	program.write_word(0x601, f.mode == 0xf8 ? f.address : f.mask);
	if (f.words == 3) program.write_word(0x602, f.mode == 0xf8 ? f.mask : 2);
	program.write_word(0x600 + f.words, 0xf4e1);
	data.write_word(f.address, f.source);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PC, 0x600}, {cpu_regs::STATE_SP, index % 8 == 1 ? 0x1500 : 0x1000},
		{cpu_regs::STATE_A, 0x123456789ULL}, {cpu_regs::STATE_B, 0xff76543210ULL}, {cpu_regs::STATE_T, 0x1357},
		{cpu_regs::STATE_AR0, 2}, {cpu_regs::STATE_AR3, index % 8 == 5 ? 0x1503 : 0x1500}, {cpu_regs::STATE_BK, 4},
		{cpu_regs::STATE_ST0, 0x1e2a}, {cpu_regs::STATE_ST1, f.st1}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "atomic_xorm_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(at == f.address && word == f.source && !m_pipeline_operand_reads++ && !m_pipeline_store_writes, "XORM retains one destination read before its write"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "atomic_xorm_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(at == f.address && word == u16(f.source ^ f.mask) && m_pipeline_operand_reads == 1 && !m_pipeline_store_writes++, "XORM writes the XOR result once to its resolved address"); });
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 520000 + index;
	if (index < 512)
	{
		m_recurring_irq_snapshot = {0, 0};
		expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save XORM before address/literal consumption");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::staged_overflow_fixture tms320c54x_test_state::staged_overflow_case(unsigned v)
{
	staged_overflow_fixture f{}; unsigned const condition = v % 4; v /= 4;
	f.selected = condition & 1; f.mask = 1U << (10 - f.selected);
	f.delayed = v % 2; v /= 2; unsigned const flags = v % 4; v /= 4;
	f.initial = 0x1805 | ((flags & 1) << 10) | ((flags & 2) << 8);
	f.producer = v % 3; v /= 3; f.gap = v % 3; v /= 3;
	f.base = v % 2 ? 0x20000 : 0; v /= 2;
	f.checkpoint = std::array<unsigned,4>{3,6 + f.gap,7 + f.gap,10 + f.gap}[v % 4]; v /= 4;
	f.mutate = v % 2;
	f.sampled = f.producer == 0 ? f.initial : f.producer == 1 ? f.initial | f.mask : f.initial & ~f.mask;
	f.take = bool(f.sampled & f.mask) == bool(condition & 2);
	f.opcode = 0xf860 | (f.delayed ? 0x200 : 0) | (f.selected << 3) | ((condition & 2) << 3);
	return f;
}

void tms320c54x_test_state::start_staged_overflow(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = staged_overflow_case(variant); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 20; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.producer == 0 ? 0xf495 : (f.producer == 1 ? 0xf5b0 : 0xf4b0) | (10 - f.selected));
	program.write_word(f.base + 0x601 + f.gap, f.opcode); program.write_word(f.base + 0x602 + f.gap, 0x700);
	program.write_word(f.base + 0x603 + f.gap, 0xf5b0 | (10 - f.selected));
	program.write_word(f.base + 0x604 + f.gap, 0xe955);
	program.write_word(f.base + 0x605 + f.gap, 0xe811); program.write_word(f.base + 0x606 + f.gap, 0xf4e1);
	program.write_word(f.base + 0x700, 0xe822); program.write_word(f.base + 0x701, 0xf4e1);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.initial); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x800);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = program.install_read_tap(f.base + 0x602 + f.gap, f.base + 0x602 + f.gap, "overflow_target_literal", [this](offs_t, u16 &word, u16) {
		if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads++ && word == 0x700, "Overflow branch captures one original target literal"); }
	});
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x700, f.base + 0x700, "overflow_target_fetch", [this,f](offs_t, u16 &, u16) {
		if (!machine().side_effects_disabled()) { expect(f.take && !m_pipeline_operand_reads++, "Overflow branch fetches its selected target once"); }
	});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "overflow_no_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_phase = 62172; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


void tms320c54x_test_state::start_rcd_acc_case(unsigned index)
{
	unsigned const condition = index % 12;
	unsigned const sign = index / 12;
	u64 const tested = sign == 0 ? 0 : sign == 1 ? 0x12340000ULL : 0xffedcc0000ULL;
	auto &program = m_cpu->space(AS_PROGRAM);
	write_words(program, {
		{0x0700, 0xfe40 | (condition < 6 ? 2 + condition : 10 + condition - 6)}, {0x0701, 0x7710}, {0x0702, 0x4321},
		{0x0703, 0xf4e1}, {0x0720, 0xf4e1},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
	m_cpu->space(AS_DATA).write_word(0x1ec3, 0x0720);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_XPC, 0}, {cpu_regs::STATE_PC, 0x0700}, {cpu_regs::STATE_SP, 0x1ec3},
		{cpu_regs::STATE_A, condition < 6 ? tested : 0x23450000},
		{cpu_regs::STATE_B, condition < 6 ? 0x23450000 : tested}, {cpu_regs::STATE_ST1, 0x0900},
		{cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 76000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_saccd_case(unsigned index)
{
	unsigned const condition = index % 12;
	unsigned const sign = (index / 12) % 3;
	unsigned const source = (index / 36) % 2;
	unsigned const mode = (index / 72) % 4;
	unsigned const shift = index / 288;
	u64 const tested = sign == 0 ? 0 : sign == 1 ? 0x12340000ULL : 0xffedcc0000ULL;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0700, 0x9e00 | (source << 8) | (mode << 6) | (condition < 6 ? 2 + condition : 10 + condition - 6));
	program.write_word(0x0701, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x0500, 0xa55a);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_XPC, 0}, {cpu_regs::STATE_PC, 0x0700},
		{cpu_regs::STATE_A, condition < 6 ? tested : 0x23450000},
		{cpu_regs::STATE_B, condition < 6 ? 0x23450000 : tested}, {cpu_regs::STATE_AR0, 3},
		{cpu_regs::STATE_AR2, 0x0500}, {cpu_regs::STATE_BK, 0}, {cpu_regs::STATE_ST0, 0},
		{cpu_regs::STATE_ST1, 0x0900 | shift}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 75000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}


bool tms320c54x_test_state::check_branch_arithmetic_results()
{
	if (m_phase == branch_arithmetic_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = branch_arithmetic_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "BCD AEQ/BEQ admits its two-word memory arithmetic delay");
		if (tick == 9) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0x5555); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads},
			"Save branch predicate, delayed Decode and retained arithmetic Read");
		if (tick == 32)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.page + (f.taken ? 0x9101 : 0x9005) &&
				m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.destination ? f.a : f.result) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination ? f.result : f.b) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.final_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | ((f.flags & 1) ? 0x100 : 0) | ((f.flags & 2) ? 0x200 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x2000 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == (f.form == 2 ? 0x1201 : 0x1200), "BCD retains its pre-delay decision while arithmetic publishes the independent result, flags and address update once");
			if (replay_pipeline_at(branch_arithmetic_phase + 1, f.checkpoint,
				"Restore BCD arithmetic without resampling or duplicate publication")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 36863) { start_branch_arithmetic(variant + 1); return true; }
			osd_printf_info("TMS320C54x BCD arithmetic: PASS cases=36864 restore=36864 operations=ADD,SUB,ADDS,SUBS accumulators=2 predicates=AEQ,BEQ outcomes=2 forms=absolute,long_offset,long_update operands=4 sources=4 sxm_ovm=4 pages=2 snapshots=3 predicate_retained=1 operand_retained=1 data_reads=1 data_writes=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated BCD arithmetic complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == branch_arithmetic_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = branch_arithmetic_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_bitt_results()
{
	if (m_phase == bitt_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, form = variant % 8;
		bool const extended = form >= 5, invert = (variant / 256) % 2;
		unsigned const read_tick = extended ? 6 : 5, checkpoint = std::array<unsigned, 3>{4, 6, 7}[variant / 2048];
		u16 const t = (variant / 8) % 16 | ((variant / 128) % 2 ? 0xa5f0 : 0);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged BITT addressing is admitted");
		if (tick == read_tick)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(form == 5 ? 0x06d1 : 0x1201, 0x5555);
			m_cpu->set_state_int(tms320c54x_device::STATE_T, t ^ 15);
		}
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads},
			"Save BITT before or after retained Read and Execute");
		if (tick == 20)
		{
			u16 const pointer = form == 3 ? 0x1200 : form == 4 ? 0x1202 : form == 6 ? 0x1200 : 0x1201;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(extended) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa824 | (!invert ? 0x1000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0xb00 | (form == 1 ? 0x4000 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == (t ^ 15) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1200 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == pointer &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12345678ULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x81abcdef01ULL, "BITT uses retained memory and T(3:0), changes only TC, and updates its address once");
			if (replay_pipeline_at(bitt_phase + 1, checkpoint,
				"Restore BITT without resampling memory or T")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 6143) { start_bitt(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged BITT: PASS cases=6144 restore=6144 forms=DP,SP,plain,postdec,postinc,absolute,long_offset,long_update selectors=16 t_high=2 values=2 tc_initial=2 pages=2 snapshots=3 reads=1 writes=0 retained_memory_t=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated staged BITT complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == bitt_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = bitt_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == bitt_phase + 2)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 3>{4, 7, 10}[variant / 512];
		u16 const t = (variant / 2) % 16 | ((variant / 32) % 2 ? 0xa5f0 : 0);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RETD admits a two-word BITT as its complete delay span");
		if (m_pipeline_operand_reads == 2 && !m_pipeline_store_writes)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(variant % 2 ? 0x1201 : 0x06d1, 0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_T, t ^ 15); m_pipeline_store_writes = 1;
		}
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads, &m_pipeline_store_writes},
			"Save RETD capture/replay and BITT delay operand");
		if (tick == 24)
		{
			u32 const base = (variant / 256) % 2 ? 0x20000 : 0;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == base + 0x9101 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x2001 &&
				m_pipeline_operand_reads == 2 && m_pipeline_extensions_reads == 1 && m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_T) == (t ^ 15) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa824 | ((variant / 64) % 2 ? 0 : 0x1000)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0xb00 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12345678ULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x81abcdef01ULL, "RETD executes BITT once, preserves retained T and memory, balances stack and reaches only its target");
			if (replay_pipeline_at(bitt_phase + 3, checkpoint,
				"Restore RETD/BITT without duplicate stack/operand reads")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1535) { start_bitt_retd(variant + 1); return true; }
			osd_printf_info("TMS320C54x RETD BITT: PASS cases=1536 restore=1536 forms=absolute,long_offset selectors=16 t_high=2 values=2 tc_initial=2 pages=2 snapshots=3 stack_reads=1 operand_reads=1 extension_fetches=1 retained_memory_t=1 target_only=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated RETD BITT complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == bitt_phase + 3) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = bitt_phase + 2; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_delay_bitt_results()
{
	if (m_phase == delay_bitt_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delay_bitt_case(variant);
		if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) || m_cpu->state_int(tms320c54x_device::STATE_ST0) != f.status(tick)) osd_printf_info("Delay BITT status mismatch: case=%u control=%u slot=%u tick=%u st0=%04x expected=%04x\n",variant,f.control,f.slot,tick,unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)),f.status(tick));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.status(tick),"Delay BITT changes only TC at Execute, after the branch decision");
		if (tick == f.read()-1) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR4) == unsigned(f.form == 2 ? 0x5001 : 0x5000),"Delay BITT owns one Access-stage pointer update"); m_cpu->set_state_int(tms320c54x_device::STATE_AR4,0x7000); }
		if (tick == f.read())
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == f.t(),"BITT Read consumes the live T value, including its older delay-slot producer");
			if (f.upper) m_cpu->set_state_int(tms320c54x_device::STATE_T,f.final_t());
			else { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source(),u16(~f.value())); }
		}
		if (f.control != 5 && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base+0x601,0x620); }
		unsigned const checkpoint = std::array<unsigned,3>{3,f.read()-1,f.read()}[f.snapshot];
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads,m_port_reads,m_fast_stack_reads,m_pipeline_store_writes,m_pipeline_register_writes}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save delayed BITT before or after its owned operand samples"); }
		if (tick == 32)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.selected()+3 && m_cpu->state_int(tms320c54x_device::STATE_SP) == f.final_sp() && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1() && m_cpu->state_int(tms320c54x_device::STATE_T) == f.final_t() && m_cpu->state_int(tms320c54x_device::STATE_A) == f.a() && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x81abcdef01ULL && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13,"Delay BITT preserves the selected path, T retention, accumulators, modes and owner stack");
			expect(m_pipeline_operand_reads == 1 && m_port_reads == f.slot && m_fast_stack_reads == unsigned(f.control == 5) && m_pipeline_store_writes == 1 && m_pipeline_register_writes == unsigned(f.control == 4 ? 2 : f.control == 3 ? 1 : 0),"Delay BITT and its optional T producer each execute once with owner-only stack transactions");
			for (unsigned ar = 0; ar < 8; ++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0+ar) == unsigned(ar == 3 ? 0x5100 : ar == 4 ? 0x7000 : 0x4000+ar),"No retained BITT address is recomputed after pointer poisoning");
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(0x60) == u16(f.selected()),"The selected continuation leaves its distinct RAM marker"); }
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_port_reads = m_recurring_irq_snapshot[1]; m_fast_stack_reads = m_recurring_irq_snapshot[2]; m_pipeline_store_writes = m_recurring_irq_snapshot[3]; m_pipeline_register_writes = m_recurring_irq_snapshot[4]; m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(delay_bitt_phase+1, "Restore delayed BITT after clearing future pipeline state"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 49151) { start_delay_bitt(variant+1); return true; }
			osd_printf_info("TMS320C54x short control delay BITT: PASS cases=49152 restore=49152 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD,BCD_NTC,BCD_TC forms=DP,plain,postinc,SP slots=2 slot1_producer=LD_T selectors=16 t_high=2 values=2 tc_initial=2 pages=2 snapshots=3 poison_cohorts=memory,T memory_t_sample=Read tc_publish=Execute old_branch_decision=1 owner_stack_only=1 opcode_target_poison=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated short control delay BITT complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == delay_bitt_phase+1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = delay_bitt_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_mapped_addm_results()
{
	if (m_phase == ar_addm_phase)
	{
		bool const brc = profile_is("addm-brc");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto f = mapped_addm_case(variant);
		unsigned const destination = f.t ? tms320c54x_device::STATE_T : brc ? tms320c54x_device::STATE_BRC : tms320c54x_device::STATE_AR0 + f.reg;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Absolute mapped-AR ADDM is admitted");
		if (tick == f.read - 1) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x9000 + f.extensions, 0xdead); }
		if (tick == f.read) m_cpu->set_state_int(destination, 0x55aa);
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save mapped ADDM before Read, before Execute or after publication");
		}
		if (tick == 24)
		{
			if (f.short_ar_form && (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(destination) != f.result || m_cpu->state_int(tms320c54x_device::STATE_ST0) != f.final_st0 || m_cpu->state_int(tms320c54x_device::STATE_ST1) != f.st1))
				osd_printf_info("Short ADDM mismatch: variant=%u replay=%u idle=%u result=%04x/%04x st0=%04x/%04x st1=%04x/%04x reads=%u writes=%u extensions=%u\n", variant, unsigned(m_pipeline_replay), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)), unsigned(m_cpu->state_int(destination)), f.result, unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), f.final_st0, unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), f.st1, m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == f.extensions && m_cpu->state_int(destination) == f.result && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.final_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL, "Mapped ADDM preserves retained operands, arithmetic flags and unaffected accumulators");
			for (unsigned ar = 0; ar < 8; ++ar) if (f.t || ar != f.reg) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (f.short_ar_form == 2 && ar == (f.reg == 4 ? 5U : 4U) ? 0x10 + f.reg : 0x3000 + ar), "Mapped ADDM does not retarget other ARs under CMPT");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				if (f.short_ar_form) m_cpu->set_pipeline_frontend(true);
				return restore_pipeline_at(ar_addm_phase + 1, "Restore mapped ADDM without resampling or duplicate publication");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.short_ar_form ? 49151 : f.t ? 6143 : brc ? 3071 : 24575)) { start_ar_addm(variant + 1); return true; }
			if (f.short_ar_form) { osd_printf_info("TMS320C54x short mapped AR ADDM: PASS cases=49152 restore=49152 forms=DP,indirect registers=8 values=4 immediates=4 sxm_ovm_c_ova=16 cmpt=2 pages=2 snapshots=3 read=6 write=7 retained_source=1 modifying_forms=excluded physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated short mapped AR ADDM complete"); }
			if (f.t) { osd_printf_info("TMS320C54x mapped T ADDM: PASS cases=6144 restore=6144 forms=DP,absolute values=4 immediates=4 sxm_ovm_c_ova=16 cmpt=2 pages=2 snapshots=3 retained_source=1 newer_t_overwritten=1 duplicate_reads=0 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated mapped T ADDM complete"); }
			if (brc) { osd_printf_info("TMS320C54x mapped BRC ADDM: PASS cases=3072 restore=3072 values=4 immediates=4 sxm_ovm_c_ova=16 cmpt=2 pages=2 snapshots=3 read=7 write=8 reads=1 writes=1 extensions=2 retained_source=1 active_loop_composition=excluded physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated mapped BRC ADDM complete"); }
			osd_printf_info("TMS320C54x mapped AR ADDM: PASS cases=24576 restore=24576 registers=8 values=4 immediates=4 sxm_ovm_c_ova=16 cmpt=2 pages=2 snapshots=3 read=7 write=8 reads=1 writes=1 extensions=2 accumulators_preserved=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated mapped AR ADDM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == ar_addm_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = ar_addm_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == ar_addm_phase + 2)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 3>{6, 8, 10}[variant / 256];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped ADDM and legal-gap consumer remain admitted");
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes},
			"Save mapped ADDM and legal-gap indirect consumer");
		if (tick == 32)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 2 && m_pipeline_store_writes == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x4000 + (variant / 8) % 4, "Legal-gap indirect load observes the published AR value");
			if (replay_pipeline_at(ar_addm_phase + 3, checkpoint,
				"Restore mapped ADDM consumer without duplicate data transactions")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_ar_addm_load(variant + 1); return true; }
			osd_printf_info("TMS320C54x mapped AR ADDM consumers: PASS cases=768 restore=768 registers=8 patterns=4 consumers=short,long_offset legal_gaps=2_3,2_3 pages=2 snapshots=3 data_reads=2 ar_writes=1 forbidden_gap_claim=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated mapped AR ADDM consumers complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == ar_addm_phase + 3) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = ar_addm_phase + 2; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_shifted_store_dld_results()
{
	if (m_phase == 321000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const checkpoint = std::array<unsigned,3>{4,8,12}[(variant/384)%3];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"Shifted-store/DLD composition remains staged");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_store_writes,m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save shifted-store/DLD address composition");
		}
		if (tick == 30)
		{
			unsigned const reg = variant%8, pattern = (variant/8)%4;
			u64 const source = std::array<u64,4>{0x11223344,0xffffff6b76,0x7fff8000,0xff80000000}[pattern];
			u32 const loaded = std::array<u32,4>{0x1234abcd,0x80000000,0xffff0001,0x7fff8000}[pattern];
			u64 const expected = u64(s64(s32(loaded))) & 0xffffffffffULL;
			expect(m_pipeline_store_writes==2 && m_pipeline_extensions_reads==2 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0+reg)==0x1004 &&
				m_cpu->state_int(tms320c54x_device::STATE_A)==source && m_cpu->state_int(tms320c54x_device::STATE_B)==expected,
				"Two store increments precede DLD's retained address and its own increment");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(321010, "Restore store/DLD without repeated pointer updates");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant+1<1152) { start_shifted_store_dld(variant+1); return true; }
			osd_printf_info("TMS320C54x store/DLD address composition: PASS cases=1152 restore=1152 registers=8 patterns=4 gaps=0,1,2 forms=ordinary,shifted pages=2 snapshots=3 writes=2 reads=2 pointer_updates=once silicon_phase_claim=0\n");
			start_pipeline_indirect_dld(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == 321010)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 321000;
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_shifted_high_store_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 30000 && m_phase < 30128)
	{
		auto &data = m_cpu->space(AS_DATA);
		unsigned const index = m_phase - 30000;
		u64 const source = index & 16 ? 0xff87654321ULL : 0x12345678;
		u64 const shifted = (source << (index & 15)) & 0xffffffffffULL;
		s64 const signed_value = s64(shifted << 24) >> 24;
		u16 expected = u16(shifted >> 16);
		if (index & 64)
		{
			if (signed_value > 0x7fffffffLL) expected = 0x7fff;
			if (signed_value < -0x80000000LL) expected = 0x8000;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			data.read_word(0x0507) == expected && data.read_word(0x0508) == 0xabcd &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == (index & 32 ? 0x0506 : 0x0507) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff87654321ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0aa5 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0900,
			"SPRU172C shifted STH Xmem preserves accumulators/status and applies address modification and SST");
		if (index < 127) { start_shifted_high_store_case(index + 1); return true; }
		osd_printf_info("TMS320C54x shifted high store conformance: PASS variants=128\n");
		start_zero_bk_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_rounded_multiply_results()
{
	if (check_rounded_multiply()) return true;
	return false;
}

bool tms320c54x_test_state::check_bitf_offset_results()
{
	if (m_phase >= 60020 && m_phase <= 60024)
	{
		u64 const values[] = {0x8000000000ULL, 0xffffffffffULL, 0, 1, 0x7fffffffffULL};
		unsigned const index = m_phase - 60020;
		bool const taken = index < 3;
		expect_opcode(0xfa47, m_cpu->state_int(tms320c54x_device::STATE_A) == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (taken ? 2 : 3) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == (taken ? 7 : 8) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"BCD ALEQ tests signed 40-bit A with two delay words and three-cycle branch cost");
		if (index != 4)
		{
			m_port_writes = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_A, values[index + 1]);
			m_cpu->set_state_int(STATE_GENPC, 0x0600);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x delayed ALEQ conformance: PASS variants=5\n");
		m_phase = 60030;
		start_bitf_offset_case(0);
		return true;
	}
	if (m_phase >= 60030 && m_phase <= 60033)
	{
		unsigned const index = m_phase - 60030;
		expect_opcode((index & 2) ? 0x61e9 : 0x61e1,
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((index & 1) ? 0x1aa5 : 0x0aa5) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == ((index & 2) ? 0x1004 : 0x1000) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"long BITF consumes displacement before mask, changes only TC and preserves update/cycle semantics");
		if (index != 3)
		{
			start_bitf_offset_case(index + 1);
			return true;
		}
		osd_printf_info("TMS320C54x BITF offset conformance: PASS variants=4\n");
		start_bio_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_bit_results()
{
	if (m_phase == 818)
	{
		expect(m_cpu->state_int(STATE_GENPC) == 0x010934 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 7 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"BIO rising edge releases the executing wait loop through ordinary instruction flow");
		osd_printf_info("TMS320C54x live BIO wait release: PASS\n");
		start_bit_case(0);
		return true;
	}
	if (m_phase >= 819 && m_phase < 1331)
	{
		unsigned const index = m_phase - 819;
		unsigned const operand = index / 32;
		unsigned const bitcode = (index / 2) & 15;
		u16 const addresses[] = {0x0507, 0x0506, 0x0508, 0x0502};
		expect_opcode(0x9600 | (operand << 4) | bitcode,
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x0aa5 | ((index & 1) ? 0x1000 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2 + (operand & 3)) == addresses[operand >> 2] &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x12345 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x6789 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"BIT Xmem decodes all bit positions and dual address modes, modifying only TC");
		if (index < 511)
		{
			start_bit_case(index + 1);
			return true;
		}
		osd_printf_info("TMS320C54x BIT Xmem conformance: PASS variants=512\n");
		start_banz_offset_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_cmpr_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	if (m_phase >= 1427 && m_phase < 1619)
	{
		unsigned const index = m_phase - 1427;
		unsigned const ar = index & 7, condition = (index / 8) & 3, pair = index / 32;
		u16 const left = cmpr_values[pair][0], right = ar ? cmpr_values[pair][1] : left;
		bool const conditions[] = {left == right, left < right, left > right, left != right};
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x0aa5 | (conditions[condition] ? 0x1000 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == left &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == right &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"CMPR conditions are unsigned, cover every AR and modify only TC");
		if (index < 191) { start_cmpr_case(index + 1); return true; }
		osd_printf_info("TMS320C54x CMPR conformance: PASS variants=192\n");
		program.write_word(0x010990, 0x771a); program.write_word(0x010991, 1);
		program.write_word(0x010992, 0xf072); program.write_word(0x010993, 0x0995);
		program.write_word(0x010994, 0xf495); program.write_word(0x010995, 0x4907);
		program.write_word(0x010996, 0xf4e1);
		m_cpu->set_state_int(STATE_GENPC, 0x010990);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4800);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 1619;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_cmpm_offset_results()
{
	if (m_phase >= 1628 && m_phase < 1632)
	{
		unsigned const index = m_phase - 1628;
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x02a5 | ((index & 1) ? 0x1000 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR1) == ((index & 2) ? 0x1004 : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE), "CMPM consumes long displacement before immediate");
		if (index < 3) { start_cmpm_offset_case(index + 1); return true; }
		osd_printf_info("TMS320C54x CMPM offset conformance: PASS variants=4\n");
		start_compact_load_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_parallel_load_multiply_results()
{
	if (m_phase >= 95000 && m_phase < 95064)
	{
		unsigned const index = m_phase - 95000;
		bool const b = index & 1;
		s64 const product = index & 16 ? -18 : -9;
		s64 expected = 0x18000 + (index & 4 ? -product : product);
		if (index & 2) expected = (expected + 0x8000) & ~s64(0xffff);
		expect_opcode(0xa800 | ((index & 1) << 8) | ((index & 2) << 8) | ((index & 4) << 8) | (index & 32 ? 0xb9 : 0x31),
			m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == (index & 8 ? 0xff80010000ULL : 0x80010000ULL) &&
			m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == u64(expected) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0800 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == (index & 32 ? 0x0501 : 0x0500) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == (index & 32 ? 0x0601 : 0x0600) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 3,
			"Parallel LD/MAC/MAS reads old T and other accumulator, honors rounding/FRCT/SXM and updates both operands in one cycle");
		if (index < 63) { start_parallel_load_multiply_case(index + 1); return true; }
		osd_printf_info("TMS320C54x parallel load multiply conformance: PASS variants=64\n");
		start_rounded_load_case(0);
		return true;
	}
	if (m_phase >= 96000 && m_phase < 96064)
	{
		static constexpr u16 values[] = {0, 0x7fff, 0x8000, 0xffff};
		static constexpr u8 operands[] = {0x45, 0x80, 0x88, 0xf8};
		unsigned const index = m_phase - 96000, mode = index / 16;
		bool const b = index & 1;
		u16 const word = values[(index >> 2) & 3];
		u64 const source = index & 2 ? u64(s64(s16(word))) : word;
		u64 const expected = ((source << 16) | 0x8000) & 0xffffffffffULL;
		expect_opcode(0x1600 | ((index & 1) << 8) | operands[mode],
			m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == expected &&
			m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x12345678 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x0e00 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x0a00 | (index & 2 ? 0x0100 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == (mode == 2 ? 0x0f65 : 0x0f66) &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == (mode == 0 || mode == 3 ? 4 : 3),
			"LDR loads memory high plus rounding bit, honors SXM, ignores OVM and preserves flags/other accumulator");
		if (index < 63) { start_rounded_load_case(index + 1); return true; }
		osd_printf_info("TMS320C54x rounded load conformance: PASS variants=64\n");
		start_compact_store_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_acc_delayed_results()
{
	if (m_phase >= 3277 && m_phase < 3337)
	{
		unsigned const index = m_phase - 3277;
		static constexpr bool outcomes[6][5] = {
			{false, true, true, true, false}, {true, false, false, false, true},
			{true, false, true, true, true}, {false, true, false, false, false},
			{false, false, true, true, false}, {true, true, false, false, true}
		};
		bool const taken = outcomes[(index / 5) % 6][index % 5];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 2 : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x09e2 : 0x09d6) &&
			m_cpu->state_int(tms320c54x_device::STATE_XPC) == 1,
			"BCD tests all twelve signed/zero accumulator predicates before both delay words");
		if (index < 59) { start_acc_delayed_case(index + 1); return true; }
		osd_printf_info("TMS320C54x delayed accumulator predicates conformance: PASS variants=60\n");
		start_delayed_call_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_store_shift_results()
{
	if (m_phase >= 280000 && m_phase < 280120)
	{
		unsigned const index = m_phase - 280000;
		auto const &vector = STORE_SHIFTS[index / 40];
		unsigned const form = (index / 8) % 5;
		bool const source_b = index & 1;
		u32 const result = vector.results[(index >> 1) & 3];
		u16 const expected = u16(result >> (form == 0 || form == 2 ? 0 : 16));
		if (m_cpu->space(AS_DATA).read_word(0x0600) != expected)
			osd_printf_info("C54x store shift mismatch: case=%u form=%u shift=%d flags=%u actual=%04x expected=%04x\n",
				index, form, vector.shift, index & 7, m_cpu->space(AS_DATA).read_word(0x0600), expected);
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->space(AS_DATA).read_word(0x0600) == expected &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0601 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (form == 4 ? 0x0611 : 0x0610) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (form == 4 ? 0x1234 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (source_b ? 1 : vector.value) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (source_b ? vector.value : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1c00 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) ==
				(0x0800 | (index & 2 ? 0x0100 : 0) | (vector.shift & 31)) &&
			m_cpu->state_int(tms320c54x_device::STATE_PMST) == (index & 4 ? 1 : 0),
			"store right shifts obey SXM before SST, preserve source/status and update pointers");
		if (index < 119) { start_store_shift_case(index + 1); return true; }
		osd_printf_info("TMS320C54x store shift boundaries: PASS cases=120 asm=-16,-1 sxm=0,1 sst=0,1 forms=5 sources=A,B pipeline_claim=0\n");
		start_smul_control_case(0);
		return true;
	}
	if (m_phase >= 290000 && m_phase < 290192)
	{
		unsigned const index = m_phase - 290000;
		unsigned const family = index / 64, kind = (index / 16) % 4, controls = (index / 2) & 7;
		bool const destination_b = index & 1;
		bool const overflow = kind == 1 && (controls & 1);
		u64 const expected = overflow && (controls & 2) ? 0x007fffffffULL :
			SMUL_RESULTS[kind][controls & 1 ? (controls == 7 ? 2 : 1) : 0];
		u16 const status = 0x1800 | (destination_b ? 0x0400 : 0x0200) |
			(overflow ? (destination_b ? 0x0200 : 0x0400) : 0);
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(destination_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == expected &&
			m_cpu->state_int(destination_b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x12340000 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == status &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) ==
				(0x0900 | (controls & 1 ? 0x40 : 0) | (controls & 2 ? 0x200 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_PMST) == (controls & 4 ? 2 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x8000 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (family == 0 ? 0x0600 : 0x0601) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == (family == 0 ? 0x0601 : 0x0611) &&
			m_cpu->space(AS_DATA).read_word(0x0600) == 0x8000 &&
			m_cpu->space(AS_DATA).read_word(0x0610) == (family == 2 ? 0x1234 : 0x8000),
			"SMUL requires FRCT and OVM, precedes MAC/MAS rounding, and preserves other status/source");
		if (index < 191) { start_smul_control_case(index + 1); return true; }
		osd_printf_info("TMS320C54x SMUL controls: PASS cases=192 families=3 rounded=0,1 frct=0,1 ovm=0,1 smul=0,1 destinations=A,B pipeline_claim=0\n");
		start_reverse_carry_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_saccd_results()
{
	if (m_phase >= 75000 && m_phase < 75576)
	{
		unsigned const index = m_phase - 75000;
		unsigned const condition = index % 12;
		unsigned const sign = (index / 12) % 3;
		unsigned const source = (index / 36) % 2;
		unsigned const mode = (index / 72) % 4;
		unsigned const shift = index / 288;
		bool const predicates[] = {sign != 2, sign == 2, sign != 0, sign == 0, sign == 1, sign != 1};
		u16 const tested = sign == 0 ? 0 : sign == 1 ? 0x1234 : 0xedcc;
		u16 const expected = predicates[condition % 6] ? u16(((source == unsigned(condition >= 6)) ? tested : 0x2345) << shift) : 0xa55a;
		unsigned const delta[] = {0, 0xffff, 1, 3};
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->space(AS_DATA).read_word(0x0500) == expected &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == u16(0x0500 + delta[mode]),
			"SACCD conditions, accumulator selection, ASM and unconditional pointer update");
		if (index < 575) { start_saccd_case(index + 1); return true; }
		osd_printf_info("TMS320C54x SACCD conformance: PASS variants=576\n");
		start_saccd_boundary_case(0);
		return true;
	}
	if (m_phase >= 270000 && m_phase < 270096)
	{
		unsigned const index = m_phase - 270000;
		auto const &vector = SACCD_BOUNDARIES[index / 16];
		bool const source_b = index & 1, sxm = index & 2, take = !(index & 8);
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->space(AS_DATA).read_word(0x0600) == (take ? (sxm ? vector.arithmetic : vector.logical) : 0xa55a) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0601 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (source_b ? 1 : vector.value) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (source_b ? vector.value : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1c00,
			"SACCD preserves 40-bit source, obeys SXM/ASM and ignores SST");
		if (index < 95) { start_saccd_boundary_case(index + 1); return true; }
		osd_printf_info("TMS320C54x SACCD boundaries: PASS cases=96 asm=-16,-1,0,15 sxm=0,1 sst_ignored=1 sources=A,B false_store=1 pipeline_claim=0\n");
		if (profile_is("saccd")) throw emu_fatalerror(0, "TMS320C54x isolated SACCD complete");
		start_rcd_acc_case(0);
		return true;
	}
	if (m_phase >= 76000 && m_phase < 76036)
	{
		unsigned const index = m_phase - 76000;
		unsigned const sign = index / 12;
		bool const predicates[] = {sign != 2, sign == 2, sign != 0, sign == 0, sign == 1, sign != 1};
		bool const take = predicates[index % 6];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x4321 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (take ? 0x1ec4 : 0x1ec3) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (take ? 0x0721 : 0x0704),
			"conditional delayed return preserves delay instruction and conditionally pops stack");
		if (index < 35) { start_rcd_acc_case(index + 1); return true; }
		osd_printf_info("TMS320C54x RCD accumulator conditions: PASS variants=36\n");
		start_bsp_interrupt_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_shifted_arithmetic_selector_results()
{
	if (m_phase >= 170000 && m_phase < 170104)
	{
		unsigned const index = m_phase - 170000, selectors = index % 8;
		static constexpr s64 operands[] = {4, 8, 16};
		// Explicit arithmetic results for fff8 with SXM clear/set at shifts
		// -16, -1, 0, +1 and +15; independent of the core's shift helper.
		static constexpr s64 unsigned_operands[] = {0, 32764, 65528, 131056, 2147221504};
		static constexpr s64 signed_operands[] = {-1, -4, -8, -16, -262144};
		s64 const operand = index < 24 ? operands[index / 8] :
			(index >= 64 ? signed_operands : unsigned_operands)[((index - 24) / 8) % 5];
		s64 const source = (selectors & 2) ? 30 : 10;
		u64 const result = (source + ((selectors & 4) ? -operand : operand)) & 0xffffffffffULL;
		expect_opcode(0x6f82,
			m_cpu->state_int(tms320c54x_device::STATE_A) == ((selectors & 1) ? 10 : result) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == ((selectors & 1) ? result : 30) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0600 &&
			m_cpu->space(AS_DATA).read_word(0x0600) == (index < 24 ? 8 : 0xfff8) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0803,
			"Shifted ADD/SUB uses independent source and destination selectors and preserves the other accumulator");
		if (index < 103) { start_shifted_arithmetic_selector_case(index + 1); return true; }
		osd_printf_info("TMS320C54x shifted arithmetic selectors: PASS cases=104 operations=2 selectors=4 shifts=-16,-1,0,1,15 sxm=0,1 pipeline_claim=0\n");
		start_interrupt_entry_spacing_case(0); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_rounding_boundary_results()
{
	if (m_phase >= 200000 && m_phase < 200128)
	{
		unsigned const index = m_phase - 200000, selectors = index % 4, input = (index / 4) % 8;
		// Explicit SPRU172C 4-142 add-8000h results, including 40-bit wrap
		// and OVM's signed 32-bit saturation; no core helper is reused.
		static constexpr u64 raw[] = {0x8000, 0x7fff, 0x80007fff,
			0x80008000, 0xff80008000ULL, 0x8000007fffULL,
			0x8000008000ULL, 0x012345e789ULL};
		static constexpr u64 saturated[] = {0x8000, 0x7fff, 0x7fffffff,
			0x7fffffff, 0xff80008000ULL, 0x7fffffff,
			0xff80000000ULL, 0x7fffffff};
		static constexpr u64 inputs[] = {0, 0xffffffffffULL, 0x7fffffff,
			0x80000000, 0xff80000000ULL, 0x7fffffffffULL,
			0x8000000000ULL, 0x0123456789ULL};
		u64 const result = ((index / 32) & 1 ? saturated : raw)[input];
		u64 const a = selectors & 2 ? 0x123456789aULL : inputs[input];
		u64 const b = selectors & 2 ? inputs[input] : 0x123456789aULL;
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (selectors & 1 ? a : result) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (selectors & 1 ? result : b) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (index >= 64 ? 0xabcd : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (((index / 32) & 1) ? 0x0200 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(STATE_GENPC) == 0x0802,
			"RND preserves source and status across selectors, signed boundaries and OVM");
		if (index < 127) { start_rounding_boundary_case(index + 1); return true; }
		osd_printf_info("TMS320C54x RND boundaries: PASS cases=128 selectors=4 inputs=8 ovm=0,1 status_preserved=1 pipeline_claim=0\n");
		start_sqdst_overflow_case(0); return true;
	}
	if (m_phase >= 210000 && m_phase < 210008)
	{
		unsigned const index = m_phase - 210000;
		// (32768 squared) * 2 is 80000000h; adding -1 is in range.
		bool const overflow = !(index & 2);
		u64 const result = index & 2 ? 0x7fffffff : index & 1 ? 0x7fffffff : 0x80000000;
		u16 const flags = 0x0400 | (overflow || (index & 4) ? 0x0200 : 0);
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == result &&
			(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x0600) == flags &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0600 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0601 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(STATE_GENPC) == 0x0802,
			"SQDST sets sticky OVB and applies OVM to B's signed accumulation, preserving OVA/T/addresses");
		if (index < 7) { start_sqdst_overflow_case(index + 1); return true; }
		osd_printf_info("TMS320C54x SQDST B overflow: PASS cases=8 ovm=0,1 sticky_ovb=1 smul=0 pipeline_claim=0\n");
		start_sqdst_difference_case(0); return true;
	}
	if (m_phase >= 220000 && m_phase < 220032)
	{
		unsigned const index = m_phase - 220000, pair = index % 4;
		static constexpr s64 differences[2][4] = {{65534, -65535, 1, -1}, {-2, 1, -65535, 65535}};
		static constexpr bool carries[] = {true, false, true, false};
		s64 const shifted = differences[bool(index & 4)][pair] * 65536;
		bool const overflow = shifted > 0x7fffffffLL || shifted < -0x80000000LL;
		u64 const result = overflow && (index & 8) ?
			(shifted < 0 ? 0xff80000000ULL : 0x7fffffff) : u64(shifted) & 0xffffffffffULL;
		u16 const flags = 0x1200 | (carries[pair] ? 0x0800 : 0) |
			(overflow || index >= 16 ? 0x0400 : 0);
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == result &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == flags &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(STATE_GENPC) == 0x0802,
			"SQDST extends X/Y under SXM, updates carry/borrow and sticky OVA, and saturates A under OVM");
		if (index < 31) { start_sqdst_difference_case(index + 1); return true; }
		osd_printf_info("TMS320C54x SQDST difference: PASS cases=32 sxm=0,1 ovm=0,1 carry_borrow=1 sticky_ova=1 pipeline_claim=0\n");
		start_firs_sum_case(0); return true;
	}
	if (m_phase >= 230000 && m_phase < 230032)
	{
		unsigned const index = m_phase - 230000, pair = index % 4;
		static constexpr s64 sums[2][4] = {{65536, 131070, 32768, 65536}, {0, -2, 32768, -65536}};
		s64 const shifted = sums[bool(index & 4)][pair] * 65536;
		bool const overflow = shifted > 0x7fffffffLL || shifted < -0x80000000LL;
		u64 const result = overflow && (index & 8) ?
			(shifted < 0 ? 0xff80000000ULL : 0x7fffffff) : u64(shifted) & 0xffffffffffULL;
		u16 const flags = 0x1200 | (pair == 2 ? 0 : 0x0800) |
			(overflow || index >= 16 ? 0x0400 : 0);
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == result &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 5 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == flags &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0600 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0601 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(STATE_GENPC) == 0x0803,
			"FIRS computes operand carry, SXM sum, sticky OVA and OVM saturation while accumulating old A");
		if (index < 31) { start_firs_sum_case(index + 1); return true; }
		osd_printf_info("TMS320C54x FIRS sum: PASS cases=32 sxm=0,1 ovm=0,1 carry=1 sticky_ova=1 pipeline_claim=0\n");
		start_abdst_overflow_case(0); return true;
	}
	if (m_phase >= 240000 && m_phase < 240016)
	{
		unsigned const index = m_phase - 240000;
		u64 const distance = index & 1 ? 0x20000 : 0x10000;
		bool const overflow = !(index & 4);
		u64 const result = overflow ? (index & 2 ? 0x7fffffff : 0x7fffffff + distance) : distance - 1;
		u16 const flags = 0x1c00 | (overflow || (index & 8) ? 0x0200 : 0);
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == result &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == flags &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1234 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0600 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x0601 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(STATE_GENPC) == 0x0802,
			"ABDST accumulates absolute signed-17 old A with FRCT, sticky OVB and OVM saturation");
		if (index < 15) { start_abdst_overflow_case(index + 1); return true; }
		osd_printf_info("TMS320C54x ABDST overflow: PASS cases=16 frct=0,1 ovm=0,1 sticky_ovb=1 pipeline_claim=0\n");
		start_poly_boundary_case(0); return true;
	}
	if (m_phase >= 250000 && m_phase < 250064)
	{
		unsigned const index = m_phase - 250000;
		static constexpr s64 before[] = {0x7fff7fff, 0x7fff8000, -1, -32769};
		s64 const rounded = (before[index % 4] + (index & 4 ? 2 : 1) + 32768) & ~s64(65535);
		bool const overflow = rounded > 0x7fffffffLL || rounded < -0x80000000LL;
		u64 const result = overflow && (index & 8) ? 0x7fffffff : u64(rounded) & 0xffffffffffULL;
		u16 const flags = 0x1a00 | (overflow || (index & 32) ? 0x0400 : 0);
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == result &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (index & 16 ? 0xffffff0000ULL : 0xffff0000ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == flags &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x0600 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(STATE_GENPC) == 0x0802,
			"POLY rounds old-B product sum, applies OVM/sticky OVA and independently loads SXM-extended memory into B");
		if (index < 63) { start_poly_boundary_case(index + 1); return true; }
		osd_printf_info("TMS320C54x POLY boundaries: PASS cases=64 frct=0,1 ovm=0,1 sxm=0,1 sticky_ova=1 pipeline_claim=0\n");
		if (profile_is("legacy-atomic"))
		{
			osd_printf_info("TMS320C54x legacy atomic prefix: PASS transport_integration=0 staged_suite_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated legacy atomic prefix complete");
		}
		start_pipeline_frontend_case(); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_multiword_pipeline_results()
{
	if (m_phase == 251040)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		constexpr u64 results[] = { 0xe833, 0xfffffff495ULL, 0x3fff8000, 0x80010000 };
		bool const destination_b = m_pipeline_long_case == 1 || m_pipeline_long_case == 3;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),
			"Two-word frontend advances one active clock at a time");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle < 7 || destination_b ? 0 : results[m_pipeline_long_case]) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 11 ? 0x55 : cycle >= 7 && destination_b ? results[m_pipeline_long_case] : 0),
			"Literal is not executed and decoded multiword load retires while entry drains");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle < 6 ? 0x1000 : 0xfff) &&
			m_cpu->space(AS_DATA).read_word(0xfff) == (cycle < 8 ? 0xbeef : 0x602),
			"Interrupt continuation skips the owned extension but not the next instruction");
		if (cycle == 2) m_cpu->set_input_line(0, ASSERT_LINE);
		if (cycle == 3 && !m_pipeline_replay)
		{
			expect(m_pipeline_operand_reads == 1 && m_vector_acks == 0,
				"Partial Decode retains pending IRQ without acknowledgement");
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,
				"Save incomplete two-word Decode and fetched literal");
		}
		if (cycle == 11)
		{
			expect(m_vector_reads == 1 && m_vector_acks == 1 && m_pipeline_operand_reads == 1,
				"Each leg consumes one retained literal and one vector");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_vector_reads = m_vector_acks = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE && m_cpu->pipeline_cycles() == 3,
					"Restore incomplete Decode without rereading its literal");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
				m_phase = 251050;
				m_pipeline_expected_cycle = 4;
				m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
				return true;
			}
			m_pipeline_operand_tap.remove(); m_vector_prefetch_tap.remove();
			m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_long_case < 3) { start_multiword_pipeline_case(m_pipeline_long_case + 1); return true; }
			osd_printf_info("TMS320C54x staged multiword: PASS cases=4 literal_fetch=3 complete_decode=4 execute=7 vector_fetch=7 return_pc=0602 pending_decode_restore=4\n");
			start_multiword_stream_case();
			return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	if (m_phase == 251050)
	{
		expect(m_cpu->pipeline_cycles() == 3, "Scheduler settlement preserves partial Decode");
		m_cpu->resume(SUSPEND_REASON_DISABLE);
		m_phase = 251040;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_logical_immediate_results()
{
	if (m_phase == 251570)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const source_b = BIT(variant, 0), destination_b = BIT(variant, 1);
		unsigned const shift = variant >= 768 ? 16 : std::array<unsigned, 4>{0, 1, 7, 15}[(variant >> 4) & 3];
		u64 const source = BIT(variant, 6) ? 0xff89abcdefULL : 0x0012345678ULL;
		u64 const operand = u64(BIT(variant, 6) ? 0xf495 : 0x8001) << shift;
		unsigned const operation = (variant % 768) / 256;
		u64 const result = operation == 0 ? source & operand : operation == 1 ? source | operand : source ^ operand;
		u64 const initial_a = source_b ? 0x1111222233ULL : source;
		u64 const initial_b = source_b ? source : 0x4444555566ULL;
		u64 const checkpoint = BIT(variant, 7) ? 4 : 3;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 3) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 7 && !destination_b ? result : initial_a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 7 && destination_b ? result : initial_b) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xefff &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (BIT(variant, 2) << 8) | (BIT(variant, 3) << 9)),
			"Logical immediate uses unsigned operands without status changes or saturation");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save partial/complete logical immediate Decode");
		}
		if (cycle == 3) m_cpu->space(AS_PROGRAM).write_word(0x601, 0);
		if (cycle == 9)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = 1;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore logical literal without a second program read");
				return restart_pipeline_observer(251580, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1535) { start_pipeline_logical_immediate(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged logical immediates: PASS cases=1536 operations=AND,OR,XOR selectors=4 shifts=0,1,7,15,16 restore=1536 duplicate_literals=0\n");
			start_pipeline_memory_dp(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251580)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251570;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}



bool tms320c54x_test_state::check_addm_expected_results()
{
	if (m_phase == 251870)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		unsigned const form = (variant / 64) & 3; bool const absolute = form == 3;
		u64 const checkpoint = 5 + absolute + variant / 256; auto const expected = addm_expected(variant);
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 6 + absolute) && m_pipeline_store_writes == unsigned(cycle >= 7 + absolute) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (cycle >= 7 + absolute ? expected[1] : 0xaa08 | (BIT(variant, 2) << 10)) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (form == 2 && cycle >= 5 ? 0x421 : 0x420) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL,
			"ADDM retains source/address/literal and honors SXM/OVM/sticky OVA without modifying accumulators");
		if (cycle == 6 + absolute)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x420, 0xdead);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save ADDM before/after target sample or publication");
		}
		if (cycle == 10)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= 6 + absolute); m_pipeline_store_writes = unsigned(checkpoint >= 7 + absolute);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore ADDM retained sample without repeated read or arithmetic flags");
				return restart_pipeline_observer(251880, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_pipeline_addm(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged ADDM: PASS cases=768 address_forms=4 value_pairs=8 status_combinations=8 restore=768 duplicate_reads=0 duplicate_writes=0\n");
			start_pipeline_data_push(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251880)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251870;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_memory_logic_results()
{
	if (m_phase == 252010)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		unsigned const kind = variant % 6, form = (variant / 6) % 4, pair = (variant / 24) % 4;
		bool const sxm = BIT(variant / 96, 0);
		u16 const address = form == 0 ? 0xa0 : form == 1 ? 0x1020 : 0x470;
		u16 const operand = std::array<u16, 4>{0xffff, 0x8000, 0x00ff, 0x8001}[pair];
		u64 const source = std::array<u64, 4>{0, 0xffff, 0x123456789aULL, 0xff80000000ULL}[pair];
		u64 const result = kind < 2 ? source & operand : kind < 4 ? source | operand : source ^ operand;
		unsigned const checkpoint = 3 + variant / 192;
		u64 const cycle = m_cpu->pipeline_cycles();
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 5) &&
			m_cpu->state_int(BIT(kind, 0) ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == (cycle >= 6 ? result : source) &&
			m_cpu->state_int(BIT(kind, 0) ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x2233445566ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (cycle >= 4 ? form == 2 ? 0x471 : form == 3 ? 0x46f : 0x470 : 0x470) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xb601 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (sxm ? 0x100 : 0) | (form == 1 ? 0x4000 : 0)),
			"Logical Smem retains source despite later RAM changes and never sign-extends or changes flags");
		if (cycle == 5)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(address, 0xdead);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save logical address generation and sampled operand");
		}
		if (cycle == 10)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= 5);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore logical operand without re-reading memory or changing AR twice");
				return restart_pipeline_observer(252020, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 575) { start_pipeline_memory_logic(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged memory logic: PASS cases=576 operations=3 accumulators=2 address_forms=4 value_pairs=4 sxm_states=2 restore=576 duplicate_reads=0\n");
			if (profile_is("logic-short")) throw emu_fatalerror(3, "TMS320C54x isolated short memory logic complete");
			start_pipeline_mu4_logic_tail(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252020)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252010;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252030)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const checkpoint = std::array<unsigned, 5>{6, 7, 8, 10, 11}[m_pipeline_peripheral_case];
		auto const reads_at = [](u64 clock) { return unsigned(clock >= 7) + unsigned(clock >= 8); };
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == reads_at(cycle) && m_pipeline_store_writes == unsigned(cycle >= 11) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 9 ? 0x12f8 : cycle >= 8 ? 0x1208 : cycle >= 7 ? 0x1238 : 0x1234) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == (cycle > 10 ? 0x650 : 0x470 + unsigned(cycle >= 6) - unsigned(cycle >= 7) - unsigned(cycle >= 8)) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa600 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
			"Original immediate ADD, AND, OR, MAR and STL compose without atomic fallback or rereading operands");
		if (cycle == 7 || cycle == 8)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(cycle == 7 ? 0x470 : 0x471, 0xdead);
		}
		if (cycle == 10) m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x650);
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save original MU4 logical tail around operand and destination capture");
		}
		if (cycle == 14)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = reads_at(checkpoint); m_pipeline_store_writes = unsigned(checkpoint >= 11);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore original logical tail without duplicate reads or final store");
				return restart_pipeline_observer(252040, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 4) { start_pipeline_mu4_logic_tail(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged original MU4 logical tail: PASS cases=5 original_words=6 page=2 restore=5 duplicate_reads=0 duplicate_writes=0\n");
			start_pipeline_far_branch(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252040)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252030;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_ts_load_results()
{
	if (m_phase == 254580)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const producer = profile_is("ld-ts-producer");
		auto const f = ts_load_case(variant, producer); auto const &o = f.address;
		unsigned const checkpoint = std::array<unsigned, 3>{3, o.read_cycle - 1, o.read_cycle}[variant / (producer ? 1536 : 69632)];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (!f.destination && tick > o.read_cycle ? f.result : f.a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination && tick > o.read_cycle ? f.result : f.b),
			"TS load publishes only its retained source/count result in Execute");
		if (tick == 4 && (producer ? f.producer_kind == 0 : o.form >= 14))
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead);
		}
		if (tick == o.read_cycle - 1)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + o.selected) == o.next_ar, "TS load modifies its selected pointer once before Read");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + o.selected, 0x2222);
		}
		if (tick == o.read_cycle)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == f.t, "TS Read observes the legal producer's published T");
			m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t ^ 0x3f);
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(o.address, 0xdead);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads, m_pipeline_store_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save TS load before address capture or after retained memory/T Read");
		}
		if (tick == 24)
		{
			u16 const expected_st0 = (o.st1 & 0x20) && o.reg && o.form >= 2 && o.form != 17 ? (o.st0 & 0x1fff) | (o.reg << 13) : o.st0;
			expect(m_pipeline_operand_reads == (producer && f.producer_kind == 1 ? 2 : 1) &&
				m_pipeline_extensions_reads == unsigned(producer ? f.producer_kind == 0 : o.form >= 14) &&
				m_pipeline_store_writes == unsigned(producer && f.producer_kind != 1) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == o.st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0 + o.selected) == 0x2222 && m_cpu->state_int(tms320c54x_device::STATE_T) == (f.t ^ 0x3f),
				"TS load retains memory, low-six-bit count and pointer without modifying carry/overflow or replaying the producer");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; m_pipeline_store_writes = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(254590, "Restore TS load without source, shift-count or literal resampling");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (producer ? 4607 : 208895)) { start_ts_load(variant + 1); return true; }
			if (producer)
			{
				osd_printf_info("TMS320C54x TS load producers: PASS cases=4608 restore=4608 producers=STM_T,LD_T,STLM_T shift_codes=8 values=4 sxm_ovm=4 destinations=2 pages=2 snapshots=3 read_owned_t=1 forbidden_stlm_gap_claim=0 duplicate_reads=0 duplicate_writes=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated TS load producers complete");
			}
			osd_printf_info("TMS320C54x TS loads: PASS cases=208896 restore=208896 forms=17 registers=8 shift_codes=64 values=4 sxm_ovm=4 destinations=2 compatibility=2 snapshots=3 read_owned_t=1 source_reads=1 writes=0 status_preserved=1 mapped_sources=excluded silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated TS loads complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254590)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254580;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310100)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, kind = (variant / 8) % 4;
		unsigned const checkpoint = 6 + variant / 256;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Shifted store and early DAGEN writer remain independently owned");
		capture_pipeline_at(checkpoint, {&m_pipeline_store_writes, &m_pipeline_operand_reads, &m_pipeline_register_writes},
			"Save shifted-store shared-bank publication boundary");
		if (tick == 20)
		{
			unsigned const reg = variant % 8; bool const same = (variant / 32) % 2, conflict = (variant / 64) % 2;
			unsigned const target = same ? reg : reg ^ 1;
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (i == target ? 0xbeef : conflict && i == reg ? 0x2468 : 0x4000 + i), "Only the two owned register publications affect the AR bank");
			expect(m_pipeline_store_writes == 1 && m_pipeline_register_writes == 1 && m_pipeline_operand_reads == unsigned(kind == 1 || kind == 2) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 + unsigned(kind == 2), "Shifted-store conflict preserves every source, write and stack increment across replay");
			if (replay_pipeline_at(310110, checkpoint,
				"Restore pending or committed shifted-store bank collision")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_shifted_ar_conflict(variant + 1); return true; }
			osd_printf_info("TMS320C54x shifted AR store bank conflicts: PASS cases=768 restore=768 destinations=8 writers=STM,MVDK,POPM,ST targets=same,different controls=RAM pages=2 snapshots=3 store_write=7 early_write=7,8 source_reads=once writes=once silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated shifted AR bank conflicts complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310110) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310100; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_accumulator_logic_results()
{
	if (m_phase != 252570) return false;
	if (m_register_sequence->step())
	{
		report_register_unary(register_unary_kind::logic);
		m_register_sequence.reset(); start_pipeline_data_copy();
	}
	return true;
}

bool tms320c54x_test_state::check_accumulator_shift_results()
{
	if (m_phase == 252610)
	{
		if (m_register_sequence->step())
		{
			report_register_unary(register_unary_kind::shift);
			m_register_sequence.reset(); start_pipeline_long_pair();
		}
		return true;
	}

	if (m_phase == 252630)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const reverse = profile_is("drsub");
		auto const f = reverse ? long_reverse_fixture(variant) : long_pair_fixture(variant);
		bool const destination = BIT(f.opcode, 8), retired = tick >= f.read_cycle + 1;
		bool const source = (f.opcode & 0xfc00) == 0x5000 ? BIT(f.opcode, 9) : destination;
		u64 const initial_a = source ? 0x246813579aULL : f.source, initial_b = source ? f.source : 0x246813579aULL;
		unsigned const checkpoint = f.read_cycle - 2 + std::array<unsigned, 3>{0, 2, 3}[variant < 6912 ? variant / 2304 : (variant - 6912) / 4608];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (retired && !destination ? f.result : initial_a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (retired && destination ? f.result : initial_b) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (retired ? f.result_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1,
			"Long arithmetic publishes full/dual-lane result and modeled carry/overflow only at Execute");
		if (tick == f.read_cycle - 2 && f.form >= 5)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word((f.reg == 6 ? 0x20000 : 0) + 0x601, 0xdead);
		}
		if (tick == f.read_cycle - 1)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.reg) == f.next_ar, "DSUB long addressing modifies AR by its specified offset or two-word step once");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.reg, 0x2000);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2000);
		}
		if (tick == f.read_cycle)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(f.address, 0xdead); m_cpu->space(AS_DATA).write_word(f.address ^ 1, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save DSUB after Decode, paired read or Execute");
		}
		if (tick == 16)
		{
			expect(m_pipeline_operand_reads == 2 && m_pipeline_extensions_reads == (f.form >= 5), "DSUB owns exactly one pair and extension");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(252640, "Restore DSUB without duplicate reads or resampling changed operands");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (reverse)
			{
				if (variant < 6911) { start_pipeline_long_pair(variant + 1); return true; }
				osd_printf_info("TMS320C54x staged DRSUB: PASS cases=6144 restore=6144 address_forms=8 destinations=A,B patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 pair_reads=2 memory_writes=0 retained_pair=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated DRSUB complete");
			}
			if (variant == 6911) osd_printf_info("TMS320C54x staged DSUB: PASS cases=6144 address_forms=8 destinations=A,B patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=6144 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0\n");
			if (variant < 20735) { start_pipeline_long_pair(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged DADD: PASS cases=12288 address_forms=8 selectors=4 patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=12288 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0\n");
			if (profile_is("long-pair"))
			{
				osd_printf_info("TMS320C54x staged long-pair controls: PASS dsub=6144 dadd=12288 restore=18432 pair_reads=2 memory_writes=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated long-pair controls complete");
			}
			start_pipeline_long_immediate_store(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252640)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252630;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_long_accumulator_store_results()
{
	if (m_phase == 252810)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = long_accumulator_store_fixture(variant);
		bool const shifted = variant >= 38400;
		unsigned const read_cycle = shifted ? 7 : 6;
		unsigned const checkpoint = long_store_schedule::checkpoint(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Long-offset accumulator store has staged ownership");
		if (tick == 4)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word((f.reg == 6 ? 0x20000 : 0) + 0x601, 0xdead);
			if (shifted) m_cpu->space(AS_PROGRAM).write_word((f.reg == 6 ? 0x20000 : 0) + 0x602, 0xdead);
		}
		if (tick == read_cycle)
		{
			for (unsigned ar = 0; ar < 8; ++ar)
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == f.selected ? f.next_ar : 0x1007),
					"Long accumulator store updates only its selected pointer at address presentation");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2222);
			m_cpu->set_state_int((f.selector & 1) ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, f.source);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save STL/STH at partial Decode, retained address or publication");
		}
		if (tick == 16)
		{
			u16 const st0 = (f.st1 & 0x20) && f.reg && f.kind != 3 ? (f.st0 & 0x1fff) | (f.reg << 13) : f.st0;
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == (shifted ? 2 : 1) && m_cpu->space(AS_DATA).read_word(f.address) == f.value &&
					m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == 0x2222 &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == ((f.selector & 1) ? 0x11112222 : f.source) &&
					m_cpu->state_int(tms320c54x_device::STATE_B) == ((f.selector & 1) ? f.source : 0x33334444),
					"STL/STH preserves source/status and never retargets through a changed AR or literal");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore STL/STH without duplicate pointer update or RAM write");
				++m_long_store_replays;
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252820; m_check_timer->enable(true); return true;
			}
			m_long_store_coverage.add(long_store_schedule::case_key(variant));
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 == long_store_schedule::regular_cases)
			{
				expect(m_long_store_coverage.count == long_store_schedule::regular_cases && m_long_store_replays == long_store_schedule::regular_cases,
					"Long store coverage counts completed cases and actual restores");
				osd_printf_info("TMS320C54x staged long STL/STH: PASS cases=12800 forms=25 offsets=4 selectors=4 sst_sxm=4 patterns=4 compatibility=2 restore=12800 duplicate_writes=0 destination_reads=0 checkpoints=3 checkpoint_policy=balanced\n");
			}
			if (auto const next = long_store_schedule::next(variant); next < long_store_schedule::end)
				{ start_pipeline_long_accumulator_store(next); return true; }
			expect(m_long_store_coverage.count == long_store_schedule::cases && m_long_store_replays == long_store_schedule::cases,
				"Store coverage digest includes every distinct regular and shifted stimulus after replay");
			osd_printf_info("TMS320C54x staged shifted STL/STH: PASS cases=57344 forms=7 offsets=4 selectors=4 shifts=32 sst_sxm=4 patterns=4 restore=57344 duplicate_writes=0 destination_reads=0 silicon_phase_claim=0 checkpoints=3 checkpoint_policy=balanced coverage_cases=%u coverage_restores=%u coverage_fnv64=%016llx\n",
				m_long_store_coverage.count, m_long_store_replays, (unsigned long long)m_long_store_coverage.value);
			start_pipeline_long_move(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252820)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252810;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_absolute_xorm_results()
{
	if (m_phase == 252890)
	{
		bool const peripheral = profile_is("rmw-long-periph");
		bool const xorm = profile_is("xorm-absolute");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, operation = xorm ? 3 : variant % 3, arithmetic = (variant / (xorm ? 4 : peripheral ? 576 : 300)) % 64;
		bool const compatibility = (variant / (peripheral ? 288 : 19200)) % 2;
		auto const f = xorm ? absolute_xorm_fixture(variant) : peripheral ? peripheral_long_rmw_fixture(variant) : long_accumulator_store_fixture((variant / 3) % 100 + (compatibility ? 6400 : 0));
		u16 const initial_st0 = (f.st0 & ~0x0400) | (BIT(arithmetic, 2) << 10) | 0x0800;
		u16 const modified_st0 = compatibility && f.reg && f.kind != 3 ? (initial_st0 & 0x1fff) | (f.reg << 13) : initial_st0;
		auto const expected = long_rmw_expected(arithmetic, operation, modified_st0);
		unsigned const checkpoint = xorm ? std::array<unsigned, 4>{4, 6, 7, 8}[variant / 1024] : peripheral ? peripheral_rmw_schedule::checkpoints[peripheral_rmw_schedule::checkpoint_index(variant)] : long_rmw_schedule::checkpoints[long_rmw_schedule::checkpoint_index(variant)];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Long-offset RMW has three-word staged ownership");
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects();
			u32 const base = (peripheral ? bool((variant / 36864) % 2) : f.reg == 6) ? 0x20000 : 0;
			m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead); m_cpu->space(AS_PROGRAM).write_word(base + 0x602, 0xdead);
		}
		if (tick == 6)
		{
			for (unsigned ar = 0; ar < 8; ++ar)
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == f.selected ? f.next_ar : peripheral ? 0x5555 : 0x1007),
					"Long RMW modifies only its selected source pointer once at address capture");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, xorm ? f.address : 0x2222);
		}
		if (tick == (f.address >= 0x20 && f.address < 0x60 ? 8 : 7))
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0x4444);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save long RMW during Decode, address capture, sampled Read or publication");
		}
		if (tick == 20)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == 2 &&
					m_cpu->space(AS_DATA).read_word(f.address) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[1] &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.st1 | (BIT(arithmetic, 0) << 8) | (BIT(arithmetic, 1) << 9)) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == (xorm ? f.address : 0x2222) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == (xorm ? (u64(BIT(arithmetic, 0) ? s64(s16(expected[0])) : s64(expected[0])) & 0xffffffffffULL) : 0x123456789aULL) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL,
					"Long RMW preserves accumulators, operation-specific status and the captured source despite later pointer/RAM changes");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore long RMW without duplicate reads, writes or pointer updates");
				if (!xorm) ++m_long_rmw_replays;
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252900; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (!xorm) m_long_rmw_coverage.add(peripheral ? peripheral_rmw_schedule::case_key(variant) : long_rmw_schedule::case_key(variant));
			unsigned const limit = xorm ? 4096 : peripheral ? peripheral_rmw_schedule::unique_cases : long_rmw_schedule::unique_cases;
			if (variant + 1 < limit) { start_pipeline_long_rmw(variant + 1); return true; }
			if (xorm)
			{
				osd_printf_info("TMS320C54x staged absolute XORM: PASS cases=4096 restore=4096 addresses=4 value_status_pairs=64 compatibility=2 pages=2 snapshots=4 reads=1 writes=1 literal_fetches=2 flags_preserved=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated absolute XORM complete");
			}
			if (peripheral)
			{
				expect(m_long_rmw_coverage.count == limit && m_long_rmw_replays == limit,
					"Every scheduled peripheral RMW case executes and replays exactly once");
				osd_printf_info("TMS320C54x staged long peripheral RMW: PASS cases=%u restore=%u operations=ADDM,ANDM,ORM registers=8 forms=offset,preincrement offsets=3b,39,49 spans=peripheral,RAM value_pairs=8 status_states=8 compatibility=2 pages=2 snapshots=5 reads=1 writes=1 literal_fetches=2 peripheral_extra=1 ram_extra=0 double_charge=0 device_semantics=unvalidated silicon_phase_claim=0 checkpoint_policy=balanced coverage_fnv64=%016llx\n",
					m_long_rmw_coverage.count, m_long_rmw_replays, (unsigned long long)m_long_rmw_coverage.value);
				throw emu_fatalerror(0, "TMS320C54x isolated long peripheral RMW complete");
			}
			expect(m_long_rmw_coverage.count == limit && m_long_rmw_replays == limit,
				"Every scheduled long RMW case executes and replays exactly once");
			osd_printf_info("TMS320C54x staged long RMW: PASS cases=%u operations=ADDM,ANDM,ORM forms=25 offsets=4 value_pairs=8 status_states=8 compatibility=2 restore=%u duplicate_reads=0 duplicate_writes=0 snapshots=4 checkpoint_policy=balanced coverage_fnv64=%016llx\n",
				m_long_rmw_coverage.count, m_long_rmw_replays,
				(unsigned long long)m_long_rmw_coverage.value);
			if (profile_is("rmw-long")) throw emu_fatalerror(3, "TMS320C54x isolated long RAM RMW complete");
			if (profile_is("long-mvkd")) { start_pipeline_long_mvkd(); return true; }
			start_pipeline_memory_arithmetic(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252900)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252890;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252910)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		using kind = smem_schedule::kind;
		bool const subs = m_memory_kind == kind::unsigned_sub, adds = m_memory_kind == kind::unsigned_add;
		bool const shift16 = m_memory_kind == kind::shift16;
		auto const &f = m_memory_input;
		unsigned const checkpoint = std::array<unsigned, 3>{3, f.read_cycle - 1, f.read_cycle}[smem_schedule::checkpoint(variant, m_memory_kind)];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Memory ADD/SUB has retained staged ownership");
		if (f.form >= 14 && tick == 4)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word((f.reg == 6 ? 0x20000 : 0) + 0x601, 0xdead);
		}
		if (tick == f.read_cycle - 1)
		{
			for (unsigned ar = 0; ar < 8; ++ar)
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == f.selected ? f.next_ar : 0x5007),
					"Memory ADD/SUB modifies only the selected pointer once in Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2222);
		}
		if (tick == f.read_cycle)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save memory ADD/SUB during Decode, captured address or retained Read");
		}
		if (tick == 18)
		{
			u64 const other = shift16 && BIT(f.opcode, 8) != BIT(f.opcode, 9) ? f.source : 0x123456789aULL;
			expect_opcode(f.opcode, m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(f.form >= 14) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.result_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (BIT(f.opcode, 8) ? other : f.result) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (BIT(f.opcode, 8) ? f.result : other) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == 0x2222,
				"Memory ADD/SUB uses its retained sample with bit-32 carry, sticky destination overflow and signed OVM saturation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore memory ADD/SUB without operand resampling or duplicate AR modification");
				++m_memory_alu_restores;
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252920; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_memory_alu_coverage.add(smem_schedule::key(variant, m_memory_kind));
			expect(m_memory_alu_restores == m_memory_alu_coverage.count, "Every retained memory arithmetic input completed a real restore");
			if (variant + 1 < smem_schedule::raw_inputs(m_memory_kind)) { start_pipeline_memory_arithmetic(variant + 1); return true; }
			expect(m_memory_alu_coverage.count == smem_schedule::cases(m_memory_kind), "Complete the legal memory arithmetic input bank once");
			if (shift16)
			{
				osd_printf_info("TMS320C54x staged memory shift16: PASS cases=%u restore=%u forms=17 registers=8 selectors=8 patterns=8 sxm_ovm=4 compatibility=2 reads=1 data_writes=0 partial_carry=1 mapped_acceptance=0 silicon_phase_claim=0 snapshots=3 checkpoint_policy=balanced coverage_fnv64=%016llx\n", m_memory_alu_coverage.count, m_memory_alu_restores, (unsigned long long)m_memory_alu_coverage.value);
				throw emu_fatalerror(0, "TMS320C54x isolated memory shift16 complete");
			}
			if (adds)
			{
				osd_printf_info("TMS320C54x staged ADDS: PASS cases=%u restore=%u forms=17 registers=8 accumulators=2 patterns=8 sxm_ovm=4 compatibility=2 source_unsigned=1 reads=1 data_writes=0 mapped_acceptance=0 silicon_phase_claim=0 snapshots=3 checkpoint_policy=balanced coverage_fnv64=%016llx\n", m_memory_alu_coverage.count, m_memory_alu_restores, (unsigned long long)m_memory_alu_coverage.value);
				throw emu_fatalerror(0, "TMS320C54x isolated staged ADDS complete");
			}
			if (subs)
			{
				osd_printf_info("TMS320C54x staged SUBS: PASS cases=%u restore=%u forms=17 registers=8 accumulators=2 patterns=8 sxm_ovm=4 compatibility=2 source_unsigned=1 reads=1 data_writes=0 mapped_acceptance=0 silicon_phase_claim=0 snapshots=3 checkpoint_policy=balanced coverage_fnv64=%016llx\n", m_memory_alu_coverage.count, m_memory_alu_restores, (unsigned long long)m_memory_alu_coverage.value);
				throw emu_fatalerror(0, "TMS320C54x isolated staged SUBS complete");
			}
			osd_printf_info("TMS320C54x staged memory ADD/SUB: PASS cases=%u forms=17 registers=8 selectors=4 patterns=8 sxm_ovm=4 compatibility=2 restore=%u duplicate_reads=0 data_writes=0 snapshots=3 checkpoint_policy=balanced coverage_fnv64=%016llx\n", m_memory_alu_coverage.count, m_memory_alu_restores, (unsigned long long)m_memory_alu_coverage.value);
			if (profile_is("memory-alu")) throw emu_fatalerror(0, "TMS320C54x isolated memory ADD SUB complete");
			start_pipeline_register_arithmetic(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252920)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252910;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252930)
	{
		if (!m_register_sequence->step()) return true;
		osd_printf_info("TMS320C54x mapped arithmetic coverage: PASS cases=%u restore=%u coverage_fnv64=%016llx\n",
			m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
		m_register_sequence.reset();
		bool const bk = profile_is("arith-bk");
		if (bk) { osd_printf_info("TMS320C54x staged mapped BK arithmetic: PASS cases=1024 restore=1024 selectors=ADD_A,ADD_B,SUB_A,SUB_B sxm_ovm=4 patterns=8 forms=direct,absolute pages=2 snapshots=2 source=Read post_read_change=1 duplicate_reads=0 data_writes=0 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated mapped BK arithmetic complete"); }
		osd_printf_info("TMS320C54x staged mapped AR arithmetic: PASS cases=1024 registers=8 selectors=4 sxm_ovm=4 patterns=2 forms=direct,absolute restore=1024 duplicate_reads=0 data_writes=0\n");
		if (profile_is("arith-ar")) throw emu_fatalerror(3, "TMS320C54x isolated mapped AR arithmetic complete");
		start_pipeline_signed_branch(); return true;
	}
	if (m_phase == 252950)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, operation = variant % 4;
		bool const subtract = operation >= 2, destination_b = operation & 1;
		unsigned const checkpoint = variant / 16 ? 8 : 4;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Protected arithmetic has complete-word staged ownership");
		if (tick == 3) m_cpu->set_input_line(0, ASSERT_LINE);
		if (tick == 4) expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 1, "Pending IRQ is CPU-owned before the protection checkpoint");
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_vector_acks};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save XC arithmetic before/after retirement with pending IRQ");
		}
		if (tick == 36)
		{
			if (m_vector_acks != 1) osd_printf_info("XC arithmetic IRQ failure: case=%u replay=%u cycle=%llu source_reads=%u vector_fetches=%u acks=%u stack_writes=%u idle=%u\n",
				variant, m_pipeline_replay, (unsigned long long)m_cpu->pipeline_cycles(), m_pipeline_operand_reads,
				m_pipeline_extensions_reads, m_vector_acks, m_pipeline_store_writes, unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)));
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 && m_vector_acks == 1 && m_pipeline_store_writes == 2 &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (destination_b ? 0x66 : subtract ? 0x1233 : 0x1235) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (destination_b ? (subtract ? 0x1233 : 0x1235) : 0x66),
				"Protected two-word arithmetic releases and returns one pending IRQ instead of leaving XC active forever");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
				m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3];
				return restore_pipeline_at(252960, "Restore XC arithmetic protection without duplicate operand reads or IRQ acknowledgements");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 31) { start_pipeline_xc_arithmetic(variant + 1); return true; }
			m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			osd_printf_info("TMS320C54x staged XC arithmetic IRQ: PASS cases=32 forms=XC1,XC2 operations=absolute_ADD,long_SUB destinations=A,B pages=2 restore=32 duplicate_source_reads=0 duplicate_vectors=0\n");
			start_pipeline_xc_tc(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252960)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252950;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_bitfield_address_results()
{
	if (m_phase == 253010)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const compare = profile_is("cmpm-smem");
		auto const f = bitfield_address_case(variant); auto const input = bitfield_inputs(variant, compare);
		unsigned const read_cycle = f.read_cycle + 1, checkpoint = std::array<unsigned, 3>{4, read_cycle - 1, read_cycle}[smem_schedule::checkpoint(variant)];
		bool const initial_tc = (variant / 1152) % 2, result = compare ? input[0] == input[1] : bool(input[0] & input[1]);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Smem test/compare has staged source and literal ownership");
		expect(BIT(m_cpu->state_int(tms320c54x_device::STATE_ST0), 12) == (tick <= read_cycle ? initial_tc : result),
			"Smem test/compare publishes TC only at Execute, not during Decode/address/Read");
		if (tick == (f.form >= 14 ? 5 : 4))
		{
			auto const disable = machine().disable_side_effects();
			u32 const base = f.reg == 6 ? 0x20000 : 0;
			m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead); if (f.form >= 14) m_cpu->space(AS_PROGRAM).write_word(base + 0x602, 0xdead);
		}
		if (tick == read_cycle - 1)
		{
			for (unsigned ar = 0; ar < 8; ++ar)
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == f.selected ? f.next_ar : 0x5007),
					"BITF performs its source-address modification once in Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, 0x2222);
		}
		if (tick == read_cycle) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save BITF during Decode, captured address or retained source");
		}
		if (tick == 18)
		{
			u16 const expected_st0 = ((f.st1 & 0x20) && f.reg && f.form >= 2 && f.form != 17 ? (f.st0 & 0x1fff) | (f.reg << 13) : f.st0) & ~0x1000;
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == (f.form >= 14 ? 2 : 1) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == (expected_st0 | (result ? 0x1000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == 0x2222, "BITF changes only TC while retaining both its mask and source through replay");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore BITF without duplicate source reads or pointer modifications");
				++m_bitfield_restores;
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253020; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_bitfield_coverage.add(smem_schedule::key(variant));
			expect(m_bitfield_restores == m_bitfield_coverage.count, "Every retained test/compare input completed a real restore");
			if (variant + 1 < smem_schedule::raw_inputs(smem_schedule::kind::test_compare)) { start_pipeline_bitfield(variant + 1); return true; }
			expect(m_bitfield_coverage.count == smem_schedule::cases(smem_schedule::kind::test_compare), "Complete the legal test/compare source domain once");
			if (compare)
			{
				osd_printf_info("TMS320C54x staged Smem CMPM: PASS cases=%u restore=%u forms=17 registers=8 compare_pairs=8 initial_tc=2 sxm_ovm=4 compatibility=2 source_reads=1 literal_fetches=1,2 duplicate_pointer_updates=0 data_writes=0 mapped_sources=excluded silicon_phase_claim=0 snapshots=3 checkpoint_policy=balanced coverage_fnv64=%016llx\n", m_bitfield_coverage.count, m_bitfield_restores, (unsigned long long)m_bitfield_coverage.value);
				throw emu_fatalerror(0, "TMS320C54x isolated Smem CMPM complete");
			}
			osd_printf_info("TMS320C54x staged BITF: PASS cases=%u forms=17 registers=8 mask_pairs=8 initial_tc=2 sxm_ovm=4 compatibility=2 restore=%u duplicate_reads=0 data_writes=0 snapshots=3 checkpoint_policy=balanced coverage_fnv64=%016llx\n", m_bitfield_coverage.count, m_bitfield_restores, (unsigned long long)m_bitfield_coverage.value);
			if (profile_is("bitf-smem")) throw emu_fatalerror(0, "TMS320C54x isolated Smem BITF complete");
			start_pipeline_bitfield_ar7(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253020)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253010;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253030)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const short_compare = profile_is("cmpm-branch");
		bool const compare = short_compare || profile_is("cmpm-absolute");
		bool const peripheral = profile_is("bitf-peripheral");
		u16 const value = std::array<u16, 4>{0, 1, u16(compare ? 0x7f : 2), 0x8000}[variant % 4], mask = (variant / 16) % 2 ? 0x7f : peripheral ? 2 : 1;
		bool const result = compare ? value == mask : bool(value & mask), taken = result == bool((variant / 4) % 2), initial_tc = (variant / 8) % 2;
		unsigned const checkpoint = short_compare ? std::array<unsigned, 3>{3, 5, 6}[variant / 64] : peripheral ? std::array<unsigned, 3>{5, 8, 9}[variant / 128] : (compare ? std::array<unsigned, 3>{3, 7, 8} : std::array<unsigned, 3>{5, 7, 8})[variant / 32];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped AR7 BITF and its immediate branch are organic CPU instructions");
		expect(BIT(m_cpu->state_int(tms320c54x_device::STATE_ST0), 12) == (tick < (short_compare ? 7 : peripheral ? 9 : 8) ? initial_tc : result), "BITF/CMPM publishes its retained source result in Execute");
		if (short_compare && tick == 5) m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x7777);
		if (tick == 6 && compare)
		{
			expect(m_fast_stack_reads == (short_compare ? 1 : 2), "CMPM captures its extensions before later program changes");
			auto const disable = machine().disable_side_effects(); u32 const base = (short_compare ? (variant / 32) % 2 : variant % 2) ? 0x20000 : 0;
			m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead);
			if (!short_compare) m_cpu->space(AS_PROGRAM).write_word(base + 0x602, 0xbeef);
		}
		if (tick == (short_compare ? 6 : peripheral ? 8 : 7))
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x7777);
			if (compare) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x3762, 0x7777); }
			if (peripheral) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x49, 0x7777); }
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads, m_fast_stack_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save mapped BITF before/after source sampling or TC publication");
		}
		if (tick == 30)
		{
			if (compare) expect(m_fast_stack_reads == (short_compare ? 1 : 2), "CMPM never re-fetches its captured extensions");
			if (short_compare) expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x7777, "Short CMPM retains its captured address without restoring or reapplying the pointer");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(taken) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 0x55 : 0x11) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x7777 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa608 | (result ? 0x1000 : 0)),
				"Immediate TC/NTC consumes mapped AR7 BITF's retained value rather than the later register contents");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				if (compare) m_fast_stack_reads = m_recurring_irq_snapshot[2];
				return restore_pipeline_at(253040, "Restore mapped BITF/branch without source or target resampling");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			if (compare) m_operand_data_tap.remove();
			if (variant < (short_compare ? 191 : peripheral ? 383 : 95)) { start_pipeline_bitfield_ar7(variant + 1); return true; }
			if (short_compare)
			{
				osd_printf_info("TMS320C54x staged short CMPM branch: PASS cases=192 restore=192 pages=2 conditions=TC,NTC initial_tc=2 source_read=6 tc_publish=7 target_fetch=9 source_reads=1 literal_fetches=1 data_writes=0 adjacent_branch=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated short CMPM branch complete");
			}
			if (peripheral)
			{
				osd_printf_info("TMS320C54x staged peripheral BITF: PASS cases=384 restore=384 forms=offset,absolute pages=2 conditions=TC,NTC source_read=8 tc_publish=9 target_fetch=11 reads=1 destination_writes=0 peripheral_extra_clock=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated peripheral BITF complete");
			}
			if (compare)
			{
				osd_printf_info("TMS320C54x staged absolute CMPM: PASS cases=96 restore=96 source=RAM conditions=TC,NTC initial_tc=2 source_read=7 target_fetch=10 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated absolute CMPM complete");
			}
			osd_printf_info("TMS320C54x staged mapped BITF branch: PASS cases=96 source=AR7 masks=0001,007f conditions=TC,NTC initial_tc=2 restore=96 source_read=7 target_fetch=10 duplicate_reads=0 data_writes=0\n");
			start_pipeline_offset_load(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253040)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253030;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_immediate_arithmetic_results()
{
	if (m_phase == 253110)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = immediate_arithmetic_case(variant);
		unsigned const checkpoint = f.producer ? std::array<unsigned, 3>{5 + f.gap, 6 + f.gap, 8 + f.gap}[(variant - 27648) / 48] :
			std::array<unsigned, 3>{3, 4, 6}[variant / 9216];
		u64 const original_a = f.producer || (f.selector & 2) ? 0x1111222233ULL : f.source;
		u64 const original_b = f.producer || !(f.selector & 2) ? 0x4444555566ULL : f.source;
		u64 const initial_a = f.producer && tick >= 7 && !(f.selector & 2) ? f.source : original_a;
		u64 const initial_b = f.producer && tick >= 7 && (f.selector & 2) ? f.source : original_b;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (tick >= f.execute_cycle && !(f.selector & 1) ? f.result : initial_a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (tick >= f.execute_cycle && (f.selector & 1) ? f.result : initial_b) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= f.execute_cycle ? f.result_st0 : f.st0) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1,
			"Immediate SUB and shift-16 ADD retire once with 40-bit source, SXM, carry, sticky destination overflow and OVM semantics");
		if (tick == 4 || (f.producer && tick == 6 + f.gap))
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead);
			if (f.producer && tick == 6 + f.gap) m_cpu->space(AS_PROGRAM).write_word(f.base + 0x603 + f.gap, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save immediate arithmetic in partial/complete Decode or Read");
		}
		if (tick == 20)
		{
			expect(m_pipeline_extensions_reads == (f.producer ? 2U : 1U), "Immediate arithmetic consumes each source literal once");
			if (f.producer) expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE), "The load-to-SUB producer fixture drains to IDLE");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_extensions_reads = m_recurring_irq_snapshot[0];
				return restore_pipeline_at(253120, "Restore immediate arithmetic without literal refetch or premature publication");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			if (variant < 27791) { start_pipeline_immediate_arithmetic(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged immediate arithmetic: PASS sub_cases=26112 add16_cases=1536 producer_cases=144 selectors=4 shifts=0..16 patterns=8 status_states=8 pages=2 restore=27792 duplicate_literals=0 data_accesses=0 silicon_phase_claim=0\n");
			start_pipeline_accumulator_arithmetic(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253120)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253110;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253130)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const load = variant >= 196896;
		unsigned const model_variant = load ? variant - 196896 : variant;
		auto const f = load ? register_load_case(model_variant) : register_arithmetic_case_full(variant);
		unsigned const checkpoint = (f.producer ? 5 + f.gap : 3) + register_bank_schedule::checkpoint_index(variant);
		auto const expected = tick >= f.execute_cycle ? f.after : f.producer && tick >= 7 ? f.before : f.initial;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= f.execute_cycle ? f.result_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1,
			"Register ALU uses original source/destination and signed 40-bit shifts, publishing the operation's status/result only in Execute");
		if (f.producer && tick == 4)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_extensions_reads, m_pipeline_operand_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save register arithmetic in Decode, Access or Read");
		}
		if (tick == 18)
		{
			expect_opcode(f.opcode, m_pipeline_extensions_reads == 1 && m_pipeline_operand_reads == unsigned(f.producer) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.result_st0,
				"Register arithmetic retains its opcode and independently checked result/status");
			if (f.producer) expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE), "Load-to-register-arithmetic producer fixture drains to IDLE");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_extensions_reads = m_recurring_irq_snapshot[0]; m_pipeline_operand_reads = m_recurring_irq_snapshot[1];
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore register arithmetic without opcode refetch or premature publication");
				++m_register_bank_replays;
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253140; m_check_timer->enable(true); return true;
			}
			m_register_bank_coverage.add(register_bank_schedule::case_key(variant));
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			if (variant == 196895)
			{
				expect(m_register_bank_coverage.count == register_bank_schedule::arithmetic_cases && m_register_bank_replays == register_bank_schedule::arithmetic_cases,
					"Register arithmetic counts completed standalone and producer restores");
				osd_printf_info("TMS320C54x staged register arithmetic: PASS cases=65536 producer_cases=288 operations=ADD,SUB selectors=4 shifts=-16..15 shift_sources=opcode,ASM patterns=8 status_states=8 pages=2 restore=65824 duplicate_opcodes=0 data_accesses=0 silicon_phase_claim=0 checkpoints=3 checkpoint_policy=balanced producer_policy=exhaustive\n");
			}
			if (auto const next = register_bank_schedule::next(variant); next < register_bank_schedule::end)
				{ start_pipeline_accumulator_arithmetic(next); return true; }
			expect(m_register_bank_coverage.count == register_bank_schedule::cases && m_register_bank_replays == register_bank_schedule::cases,
				"Register-bank digest includes both banks and every retained producer checkpoint");
			osd_printf_info("TMS320C54x staged register LD: PASS cases=65536 producer_cases=144 selectors=4 shifts=-16..15 shift_sources=opcode,ASM patterns=8 status_states=16 pages=2 restore=65680 duplicate_opcodes=0 data_accesses=0 carry_preserved=1 silicon_phase_claim=0 checkpoints=3 checkpoint_policy=balanced producer_policy=exhaustive coverage_cases=%u coverage_restores=%u coverage_fnv64=%016llx\n",
				m_register_bank_coverage.count, m_register_bank_replays, (unsigned long long)m_register_bank_coverage.value);
			if (profile_is("reg-banks")) throw emu_fatalerror(3, "TMS320C54x isolated register banks complete");
			start_pipeline_memory_multiply(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253140)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253130;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253150)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const square = profile_is("squr-smem");
		auto const f = memory_multiply_case(variant, square); auto const &o = f.operand;
		bool const load_t = variant >= 165888;
		bool const keeps_t = variant >= 55296 && !load_t && !square;
		unsigned const v = variant % 55296;
		unsigned const index = v < 30720 ? v / 10240 : (v - 30720) / 8192;
		unsigned const checkpoint = std::array<unsigned, 3>{3, o.read_cycle - 1, o.read_cycle}[index];
		bool const destination_b = (!keeps_t && !square) || BIT(o.opcode, 8);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (!load_t && !destination_b && tick > o.read_cycle ? f.result : f.a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (!load_t && destination_b && tick > o.read_cycle ? f.result : f.b) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (tick < o.read_cycle ? f.t : tick == o.read_cycle ? keeps_t ? f.t : o.memory : 0xbeef) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick > o.read_cycle ? f.result_st0 : o.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == o.st1,
			"Memory multiply owns T in Read and B/OVB in Execute, without altering A/carry or overwriting a newer T");
		if (o.form >= 14 && tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word((o.reg == 6 ? 0x20000 : 0) + 0x601, 0xdead); }
		if (tick == o.read_cycle - 1 && o.address >= 0x60)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + o.selected) == o.next_ar, "Memory multiply modifies its pointer once in Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + o.selected, 0x2222); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2222);
		}
		if (tick == o.read_cycle)
		{
			if (o.address >= 0x60) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(o.address, 0xdead); }
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save memory multiply in Decode, Access or Read");
		}
		if (tick == 18)
		{
			expect_opcode(o.opcode, m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(o.form >= 14) &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.result_st0,
				"Memory multiply/T load has no resampling and preserves its independently checked status and later T");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(253160, "Restore memory multiply without re-reading or re-publishing T");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (square)
			{
				if (variant < 55295) { start_pipeline_memory_multiply(variant + 1); return true; }
				osd_printf_info("TMS320C54x memory SQUR: PASS cases=52224 restore=52224 forms=RAM,mapped accumulators=2 patterns=8 flags=16 t_publication=Read operand_retained=1 newer_t_preserved=1 duplicate_reads=0 data_writes=0 physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated memory SQUR complete");
			}
			if (variant == 55295) osd_printf_info("TMS320C54x staged memory multiply: PASS ram_cases=27648 mapped_cases=24576 operations=MPYA,MASA,MACA,MACAR patterns=8 flag_states=16 restore=52224 t_read_owned=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (variant == 110591) osd_printf_info("TMS320C54x staged MPYU: PASS ram_cases=27648 mapped_cases=24576 accumulators=2 t_values=2 patterns=8 flag_states=16 restore=52224 t_retained=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (variant == 165887) osd_printf_info("TMS320C54x staged MPY and MPYR: PASS cases=52224 accumulators=2 rounding=0,1 t_retained=1 restore=52224 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (variant < 221183) { start_pipeline_memory_multiply(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged memory LD T: PASS cases=52224 ram_and_mapped=1 restore=52224 read_publication=1 retirement_rewrite=0 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (profile_is("mul-t-sources")) throw emu_fatalerror(0, "TMS320C54x isolated multiply and T sources complete");
			start_pipeline_t_multiply_pair(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253160)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253150; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253190)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = t_multiply_pair_case(variant); bool const destination = (variant % 256 / 32) % 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (tick < 6 ? 0x9abc : tick <= 8 ? f[0] : 0xbeef) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (!destination && tick >= 9 ? f[2] : 0x12345678) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (destination && tick >= 9 ? f[2] : 0x87654321) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= 9 ? f[3] : 0x1800) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f[4],
			"Adjacent MPY consumes LD T publication and retains it through Execute");
		if (tick == 6 || tick == 8)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(tick == 6 ? 0x500b : 0x500a, 0xdead);
			if (tick == 8) m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
		}
		if (tick == std::array<unsigned, 3>{4, 6, 8}[variant / 256] && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save LD T/MPY through retained producer/consumer state");
		}
		if (tick == 18)
		{
			expect(m_pipeline_operand_reads == 2 && m_pipeline_extensions_reads == 2, "LD T/MPY neither resamples nor duplicates extensions");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(253200, "Restore producer and dependent multiply without resampling");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_pipeline_t_multiply_pair(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged LD T multiply pair: PASS cases=768 restore=768 t_read=6 multiply_read=8 publication=9 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (profile_is("t-load-control")) { osd_printf_info("TMS320C54x T load control: PASS load_cases=52224 pair_cases=768 restore=52992 t_publish=Read no_retirement_rewrite=1 mapped_and_RAM=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated T load control complete"); }
			start_pipeline_shifted_arithmetic(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253200)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253190; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253210)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const bk = profile_is("shifted-bk");
		auto const f = shifted_arithmetic_case(variant, bk); auto const &o = f.operand;
		bool const subc = variant >= 245760;
		bool const destination = BIT(f.extension, 8), retired = tick > o.read_cycle;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (retired ? destination ? f.a : f.result : 0x1122) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (retired ? destination ? f.result : f.b : 0x3344) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (retired ? f.result_st0 : o.st0) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == o.st1 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x9abc,
			"Shifted ADD/SUB publishes only selected accumulator and arithmetic flags in Execute");
		if (tick == 4 && (!subc || o.form >= 14))
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word((o.reg == 6 ? 0x20000 : 0) + 0x601, 0xdead);
			if (!subc && o.form >= 14) m_cpu->space(AS_PROGRAM).write_word((o.reg == 6 ? 0x20000 : 0) + 0x602, 0xdead);
		}
		if (tick == o.read_cycle - 1 && o.address >= 0x60)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + o.selected) == o.next_ar, "Shifted arithmetic modifies its selected pointer once in Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + o.selected, 0x2222); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2222);
		}
		if (tick == o.read_cycle)
		{
			auto const disable = machine().disable_side_effects();
			if (o.address >= 0x60) m_cpu->space(AS_DATA).write_word(o.address, 0xdead);
			else m_cpu->set_state_int(bk ? tms320c54x_device::STATE_BK : tms320c54x_device::STATE_AR0 + o.address - 0x10, 0xdead);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
		}
		unsigned const checkpoint_index = shifted_memory_schedule::checkpoint_index(variant, bk);
		unsigned const checkpoint = std::array<unsigned, 3>{!subc && o.form >= 14 ? 4U : 3U, o.read_cycle - 1, o.read_cycle}[checkpoint_index];
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save shifted arithmetic through Decode, Access or Read");
		}
		if (tick == 18)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(!subc) + unsigned(o.form >= 14), "Memory arithmetic neither resamples nor repeats extension fetches");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore shifted arithmetic without duplicating sampling or publication");
				++m_shifted_replays;
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253220; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_shifted_coverage.add(shifted_memory_schedule::case_key(variant, bk));
			if (bk)
			{
				if (auto const next = shifted_memory_schedule::next(variant, true); next < shifted_memory_schedule::bk_end)
				{ start_pipeline_shifted_arithmetic(next); return true; }
				expect(m_shifted_coverage.count == shifted_memory_schedule::bk_cases && m_shifted_replays == shifted_memory_schedule::bk_cases,
					"Every BK arithmetic stimulus and balanced replay completed once");
				osd_printf_info("TMS320C54x shifted BK arithmetic: PASS cases=%u restore=%u selectors=8 shifts=32 sxm_ovm=4 values=4 pages=2 snapshots=balanced source_sample=Read post_read_change=1 extension_poison=1 writes=0 physical_phase_claim=0 coverage_fnv64=%016llx\n",
					m_shifted_coverage.count, m_shifted_replays, (unsigned long long)m_shifted_coverage.value);
				throw emu_fatalerror(3, "TMS320C54x isolated shifted BK arithmetic complete");
			}
			if (variant + 1 == 147456 + shifted_memory_schedule::mapped_cases)
			{
				expect(m_shifted_coverage.count == shifted_memory_schedule::arithmetic_cases && m_shifted_replays == shifted_memory_schedule::arithmetic_cases,
					"Every shifted ADD/SUB stimulus and balanced replay completed once");
				osd_printf_info("TMS320C54x staged shifted memory ADD/SUB: PASS ram_cases=49152 mapped_ar_cases=32768 selectors=8 shifts=32 sxm_ovm=4 patterns=4 restore=81920 snapshots=balanced duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			}
			if (auto const next = shifted_memory_schedule::next(variant, false); next < shifted_memory_schedule::end)
			{ start_pipeline_shifted_arithmetic(next); return true; }
			expect(m_shifted_coverage.count == shifted_memory_schedule::cases && m_shifted_replays == shifted_memory_schedule::cases,
				"Every shifted-memory and SUBC stimulus and balanced replay completed once");
			osd_printf_info("TMS320C54x staged SUBC: PASS ram_cases=6144 mapped_ar_cases=4096 value_pairs=64 sxm_ovm=4 accumulators=2 restore=10240 snapshots=balanced duplicate_reads=0 data_writes=0 silicon_phase_claim=0 combined_cases=%u combined_restore=%u coverage_fnv64=%016llx\n",
				m_shifted_coverage.count, m_shifted_replays, (unsigned long long)m_shifted_coverage.value);
			if (profile_is("shifted-alu")) throw emu_fatalerror(3, "TMS320C54x isolated shifted memory arithmetic complete");
			start_pipeline_memory_repeat(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253220)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253210; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_rptbd_shift_store_results()
{
	if (m_phase == 310400)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = rptbd_shift_store_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Complete shifted-store delay belongs to RPTBD, not its repeated body");
		if (tick == 3 || tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + (tick == 3 ? 0x601 : 0x603), 0xdead); }
		if (tick == 8) m_cpu->set_state_int(f.kind & 1 ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, f.source);
		if (f.irq && tick == (f.irq <= 2 ? 5U : 10U)) m_cpu->set_input_line(f.irq == 2 || f.irq == 4 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; m_rptbd_saved_return = m_rptbd_return; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save RPTBD delay decode, Read or body/interrupt continuation");
		}
		if (tick == 96)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x610 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == f.final_ar3 && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x6000 + f.count + 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (f.kind & 1 ? 0x123456789aULL : 0) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.kind & 1 ? 0 : 0xfedcba9876ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x605 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_BK) == 13,
				"RPTBD executes one shifted delay write and pointer effect, then clears its source and executes the body BRC+1 times");
			expect(m_rptbd_counts.fetches == 3 && m_rptbd_counts.delay_writes == 1 && m_rptbd_counts.acks == unsigned(bool(f.irq)) &&
				m_rptbd_counts.stack_reads == unsigned(bool(f.irq)) && m_rptbd_counts.stack_writes == unsigned(bool(f.irq)), "RPTBD retains words and balances its one optional interrupt across replay");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_rptbd_counts = m_rptbd_saved_counts; m_rptbd_return = m_rptbd_saved_return;
				return restore_pipeline_at(310410, "Restore complete shifted delay without duplicate publication or pointer work");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 115199) { start_rptbd_shift_store(variant + 1); return true; }
			osd_printf_info("TMS320C54x RPTBD shifted delay stores: PASS cases=115200 restore=115200 counts=0,1,2,7 forms=direct,plain,decrement,postincrement,mapped_encoding selectors=4 shifts=-16,-1,15 sst_sxm=4 patterns=2 pages=2 cpl=2 sources=none,IRQ,NMI arrivals=delay,body snapshots=3 delay_writes=1 pointer_effect=once body_count=brc+1 execute_live_source=1 retained_words=1 balanced_frames=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated RPTBD shifted stores complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310410) { m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_phase = 310411; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 310411) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310400; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_negate_results()
{
	if (m_phase == 253410)
	{
		if (m_register_sequence->step())
		{
			report_register_unary(register_unary_kind::negate);
			m_register_sequence.reset(); start_pipeline_mapped_word_arithmetic();
		}
		return true;
	}

	if (m_phase == 253430 || m_phase == 253450)
	{
		if (!m_register_sequence->step()) return true;
		osd_printf_info("TMS320C54x mapped register coverage: PASS cases=%u restore=%u coverage_fnv64=%016llx\n",
			m_register_sequence->count(), m_register_sequence->restores(), (unsigned long long)m_register_sequence->digest());
		m_register_sequence.reset();
		if (m_phase == 253430)
		{
			osd_printf_info("TMS320C54x staged mapped word arithmetic: PASS cases=6912 registers=AL,AH,AG,BL,BH,BG,T,TRN selectors=4 forms=direct,absolute producer_cases=768 restore=6912 reads_each=1 data_writes=0 silicon_phase_claim=0\n");
			if (profile_is("mapped-word")) throw emu_fatalerror(0, "TMS320C54x isolated mapped word arithmetic complete");
			start_pipeline_mapped_bitfield(); return true;
		}
		if (status_bitfield())
		{
			osd_printf_info("TMS320C54x staged status BITF: PASS cases=768 restore=768 registers=ST0,ST1 forms=direct,absolute masks=4 initial_tc=2 pages=2 snapshots=3 reads_each=1 data_writes=0 retained_source=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated status BITF complete");
		}
		osd_printf_info("TMS320C54x staged mapped BITF: PASS cases=13824 registers=AL,AH,AG,BL,BH,BG,T,TRN masks=4 initial_tc=2 producer_cases=1536 restore=13824 reads_each=1 data_writes=0 silicon_phase_claim=0\n");
		if (profile_is("mapped-bitf")) throw emu_fatalerror(0, "TMS320C54x isolated mapped BITF complete");
		start_pipeline_immediate_multiply(); return true;
	}
	if (m_phase == 253290)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const mac = profile_is("mac-t-imm");
		auto const f = immediate_multiply_case(variant, mac); bool const retired = tick > f.read_cycle;
		u16 const t = tick > f.read_cycle ? 0xbeef : f.producer && tick < 6 ? 0x9abc : f.t;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (retired && !f.destination ? f.result : f.a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (retired && f.destination ? f.result : f.b) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == t && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (retired ? f.result_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1,
			"Immediate MPY publishes its retained signed T/literal product and destination flags only in Execute");
		if (tick == 4 + f.start) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601 + f.start, 0xdead); }
		if (tick == f.read_cycle) m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save immediate multiply after literal retention, before T read or after T read");
		}
		if (tick == 20)
		{
			expect(m_pipeline_extensions_reads == 1 && m_pipeline_store_writes == unsigned(f.producer) && m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst, "Immediate multiply has one literal fetch and no data reads/writes except the optional T producer");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_store_writes = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(253300, "Restore immediate multiply without rereading poisoned literal or newer T");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < (mac ? 172032U : 86016U)) { start_pipeline_immediate_multiply(variant + 1); return true; }
			if (mac)
			{
				osd_printf_info("TMS320C54x staged immediate MAC: PASS isolated_cases=24576 stlm_cases=147456 value_pairs=64 selectors=4 flags=16 restore=172032 literal_fetches=1 t_retained=1 accumulator_latency=unvalidated data_reads=0 projection_only_stlm_gap=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated immediate MAC complete");
			}
			osd_printf_info("TMS320C54x staged immediate MPY: PASS isolated_cases=12288 stlm_cases=73728 value_pairs=64 selectors=4 flags=16 restore=86016 literal_fetches=1 t_retained=1 data_reads=0 projection_only_stlm_gap=0 silicon_phase_claim=0\n");
			if (profile_is("mpy-t-imm")) throw emu_fatalerror(0, "TMS320C54x isolated immediate MPY complete");
			start_pipeline_multiply_mapped(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253300)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253290; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253470)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = multiply_mapped_case(variant);
		if (tick == 6) m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x7777);
		if (tick == 7 + f.gap) m_cpu->set_state_int(f.producer_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, 0x012345abcdULL);
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads},
			"Save before/after mapped accumulator read");
		if (tick == 16)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(f.consumer_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == f.sampled &&
				m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1, "LDM retains the post-MPY word despite later accumulator and T mutations");
			if (replay_pipeline_at(253480, f.checkpoint,
				"Restore adjacent MPY/LDM without duplicate sampling")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 3071) { start_pipeline_multiply_mapped(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged MPY mapped consumer: PASS cases=3072 restore=3072 sources=AL,AH,AG,BL,BH,BG gaps=0,1 reads_each=1 literal_fetches=1 data_writes=0 silicon_phase_claim=0\n");
			start_immediate_mapped(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253480)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253470; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 399920)
	{
		unsigned const tick = ++m_idle_fixture_tick, variant = m_pipeline_peripheral_case; auto const f = immediate_mapped_case(variant);
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads,m_pipeline_extensions_reads,m_pipeline_store_writes,m_pipeline_register_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save immediate ALU mapped dependency before completion");
		}
		if (tick == 40)
		{
			expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IDLE), "Immediate mapped dependency completes without instruction exclusions");
			expect(m_cpu->state_int(f.selected_b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == f.result &&
				(f.original || m_cpu->state_int(f.selected_b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == f.sampled), "Immediate ALU result and mapped consumer agree with independent arithmetic");
			expect(m_pipeline_operand_reads == unsigned(f.original ? 2 : 1) && m_pipeline_extensions_reads == unsigned(f.original) &&
				m_pipeline_register_writes == unsigned(f.original) && m_pipeline_store_writes == unsigned(f.original ? 3 : 0), "Immediate mapped replay preserves exact source and destination access counts");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint;
				m_pipeline_operand_reads=m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads=m_recurring_irq_snapshot[1]; m_pipeline_store_writes=m_recurring_irq_snapshot[2]; m_pipeline_register_writes=m_recurring_irq_snapshot[3];
				return restore_pipeline_at(399930, "Restore immediate mapped dependency without duplicate sampling");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant == 15) osd_printf_info("TMS320C54x original byte-count prefix: PASS cases=16 restore=16 words=13 sizes=8 pages=2 mapped_reads=2 data_writes=3 silicon_phase_claim=0\n");
			if (variant < 975) { start_immediate_mapped(variant+1); return true; }
			osd_printf_info("TMS320C54x immediate ALU mapped consumer: PASS cases=960 restore=960 operations=ADD,SUB,AND,OR,XOR accumulators=2 parts=3 patterns=4 gaps=0,1 sxm=2 pages=2 reads=1 writes=0 silicon_phase_claim=0\n");
			start_long_mapped_original(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == 399930) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=399920; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_accumulator_cmpm_results()
{
	if (m_phase == 62126)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const mapped = profile_is("cmpm-map") || profile_is("cmpm-map-edge");
		bool const edge = profile_is("cmpm-map-edge"); auto const f = accumulator_cmpm_case(variant, mapped, edge);
		bool const result = f.value == f.literal, taken = result == f.tc;
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == (edge && tick >= f.read - 1) && bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) == (edge || tick <= f.read ? f.initial_tc : result), "Mapped CMPM rejects unowned sources and publishes TC only at Execute");
		if (tick == f.read - 1 && !edge)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
			if (mapped) set_repeat_count_source(f.source, f.value);
		}
		if (tick == f.read && !edge)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ^ 0xffffffffffULL);
			if (mapped) set_repeat_count_source(f.source, f.value ^ 0xffff);
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x600 + (f.form == 2 ? 2 : 1), 0xdead);
			if (f.form == 2) m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead);
		}
		unsigned const checkpoint = f.read - (edge ? 2 : 1) + f.snapshot;
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save retained mapped comparison around Read/Execute");
		if (tick == 30)
		{
			expect(m_pipeline_operand_reads == unsigned(!edge) && !m_pipeline_store_writes && m_pipeline_extensions_reads == (f.form == 2 ? 2U : 1U) && bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) == !edge && m_cpu->state_int(tms320c54x_device::STATE_A) == (edge ? f.a ^ 0xffffffffffULL : taken ? 0x55 : 0x11) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.b ^ 0xffffffffffULL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa600 | ((edge ? f.initial_tc : result) ? 0x1000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && !m_cpu->state_int(tms320c54x_device::STATE_SP), "CMPM branch uses retained mapped word and literal without writes or unrelated status changes");
			if (mapped)
			{
				expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (!edge && f.source == 14 ? u16(f.value ^ 0xffff) : 0xbeef) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == (!edge && f.source == 15 ? u16(f.value ^ 0xffff) : 0xabcd), "CMPM does not overwrite the mapped operand after it has been sampled");
				for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (!edge && f.source == 16 + i ? u16(f.value ^ 0xffff) : i == f.pointer ? f.source : 0x1200 + i * 0x100), "CMPM preserves the separate indirect pointer and unrelated auxiliary registers");
			}
			if (replay_pipeline_at(62127, checkpoint,
				"Restore mapped CMPM without duplicate reads or literal fetches")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < (edge ? 3840U : mapped ? 69120U : 6912U)) { start_accumulator_cmpm(variant + 1); return true; }
			if (mapped)
			{
				if (edge) { osd_printf_info("TMS320C54x mapped CMPM exclusions: PASS cases=3840 restore=3840 addresses=80 forms=direct,indirect,absolute cpl=2 initial_tc=2 pages=2 snapshots=2 reads=0 writes=0 pointer_changes=0 status_changes=0 silicon_trap_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated mapped CMPM exclusions complete"); }
				osd_printf_info("TMS320C54x mapped CMPM: PASS cases=69120 restore=69120 registers=T,TRN,AR0..7 forms=direct,indirect,absolute cpl=2 values=8 literals=6 conditions=TC,NTC initial_tc=2 pages=2 snapshots=3 reads=1 writes=0 literal_words=1,2 source=Read tc=Execute retained=1 indirect_pointer=separate physical_phase_claim=0 control_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated mapped CMPM complete");
			}
			osd_printf_info("TMS320C54x accumulator CMPM: PASS cases=6912 restore=6912 lanes=6 forms=direct,indirect,absolute cpl=2 values=4 literals=2 conditions=TC,NTC initial_tc=2 pages=2 snapshots=3 reads=1 writes=0 source=Read tc=Execute retained=1 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated accumulator CMPM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62127) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62126; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_accumulator_stm_results()
{
	if (m_phase == 62132)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = accumulator_stm_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "STM accumulator fields use the staged frontend");
		if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == 6) { m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ^ 0xffffffffffULL); }
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (tick < 6 ? f.a : tick < 7 ? f.a ^ 0xffffffffffULL : f.final_a) && m_cpu->state_int(tms320c54x_device::STATE_B) == (tick < 6 ? f.b : tick < 7 ? f.b ^ 0xffffffffffULL : f.final_b), "STM preserves live sibling fields and truncates only AG/BG to eight bits at Execute");
		unsigned const checkpoint = std::array<unsigned, 3>{4, 6, 7}[f.snapshot];
		capture_pipeline_at(checkpoint, {&m_pipeline_store_writes, &m_pipeline_operand_reads},
			"Save STM accumulator literal or publication and mapped consumer");
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_store_writes == 2 && m_pipeline_operand_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa600 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1, "Accumulator STM and MVDK preserve flags/modes and complete exactly once");
			if (replay_pipeline_at(62133, checkpoint,
				"Restore accumulator STM without repeated publication")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 3455) { start_accumulator_stm(variant + 1); return true; }
			osd_printf_info("TMS320C54x accumulator STM: PASS cases=3456 restore=3456 lanes=6 literals=6 patterns=4 sxm_ovm=4 pages=2 snapshots=3 literal_retained=1 live_siblings=1 guard_mask=8 publication=Execute adjacent_mvdk=1 writes=2 reads=1 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated accumulator STM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62133) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62132; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254030)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 3>{3, 6, 7}[variant / 128];
		u16 const value = std::array<u16, 4>{0, 1, 0x7f, 0x8000}[variant % 4], literal = (variant / 16) % 2 ? 0x7f : 1;
		bool const tc = (variant / 4) % 2, initial_tc = (variant / 8) % 2, cpl = (variant / 32) % 2, result = value == literal, taken = result == tc;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && bool(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000) == (tick < 7 ? initial_tc : result), "Direct CMPM publishes only TC at Execute");
		if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(((variant / 64) % 2 ? 0x20000 : 0) + 0x601, 0xdead); }
		if (tick == 5) { m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa633 | (initial_tc ? 0x1000 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x6000); }
		if (tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(cpl ? 0x4014 : 0x1694, 0x7777); }
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads, &m_pipeline_drained_fetches},
			"Save direct CMPM source/literal and TC ownership");
		if (tick == 30)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_drained_fetches == 1 && m_pipeline_extensions_reads == unsigned(taken) && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 0x55 : 0x11) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x5678 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa633 | (result ? 0x1000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x6000 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (cpl ? 0x4800 : 0x800), "Direct CMPM retains its original address/value/literal through branch and status mutations");
			if (replay_pipeline_at(254040, checkpoint,
				"Restore direct CMPM without duplicate source or literal reads")) return true;
			m_pipeline_operand_tap.remove(); m_operand_data_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_direct_cmpm(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged direct CMPM: PASS cases=384 restore=384 layouts=DP,SP values=4 literals=2 initial_tc=2 conditions=TC,NTC pages=2 snapshots=3 source_read=6 tc_publish=7 target_fetch=10 reads=1 writes=0 literal_fetches=1 address_retained=1 mapped_source_acceptance=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated direct CMPM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254040) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254030; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_staged_overflow_results()
{
	if (m_phase == 62172)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = staged_overflow_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Overflow-only BC/BCD remain staged");
		if (tick == 7 + f.gap)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (f.sampled & ~f.mask), "Evaluating OV/NOV consumes only its selected sticky flag");
			if (f.mutate) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.sampled | f.mask);
		}
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads, m_pipeline_store_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save overflow predicate around Evaluate and delay execution");
		}
		if (tick == 24)
		{
			bool const slots = f.delayed || !f.take;
			u16 const status = (f.sampled & ~f.mask) | (slots || f.mutate ? f.mask : 0);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(STATE_GENPC) == f.base + (f.take ? 0x702 : 0x607 + f.gap) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (f.take ? 0x22 : 0x11) &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == (slots ? 0x55 : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == status && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_pipeline_extensions_reads == 1 && m_pipeline_operand_reads == unsigned(f.take) && !m_pipeline_store_writes,
				"Overflow decision is retained across flag mutation, delay writes and restored state");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; m_pipeline_store_writes = m_recurring_irq_snapshot[2];
				m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(true);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore overflow branch after clearing the live pipeline");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62173; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 4607) { start_staged_overflow(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged overflow branches: PASS cases=4608 restore=4608 forms=BC,BCD predicates=AOV,ANOV,BOV,BNOV flags=4 producers=seed,SSBX,RSBX gaps=0,1,2 pages=2 snapshots=4 selected_flag_clear=1 post_evaluate_mutation=2 literal_reads=1 target_fetches=once writes=0 reset_before_restore=1 arithmetic_producers=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated staged overflow branches complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62173) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62172; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_mvmd_accumulator_results()
{
	if (m_phase == 253830)
	{
		bool const status = profile_is("mvmd-st1");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, gap = (variant / 8) % 3, read = 6 + gap, checkpoint = std::array<unsigned, 3>{5, read, read + 1}[(variant % 144) / 48];
		bool const b = variant & 1, store = variant >= 144; u16 const value = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(variant / 2) % 4], source = b ? 0x0b : 8;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Mapped accumulator MVMD remains admitted");
		if (tick == read) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(source, 0xdead); }
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_fast_stack_reads},
			"Save mapped producer/MVMD around source and RAM ownership");
		if (tick == 18)
		{
			{ auto const disable = machine().disable_side_effects(); expect(m_cpu->space(AS_DATA).read_word(status ? 7 : 0x1400) == (status ? value & ~u16(0x400) : value) && m_cpu->space(AS_DATA).read_word(source) == 0xdead, "MVMD retains the source and respects the reserved status bit after later mapped-register changes"); }
			expect(m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == ((b ? 0xfedcba0000ULL : 0x1234560000ULL) | 0xdead), "Mapped low-word writes preserve the accumulator's high and guard words");
			expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == 1 && m_fast_stack_reads == unsigned(!store) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 + unsigned(!store) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa600 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (status ? value & ~u16(0x400) : 0x800) && m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (store ? value : b ? 0x123456789aULL : 0xfedcba9876ULL), "Mapped producer/MVMD preserves other status, accumulator and stack ownership");
			if (replay_pipeline_at(253840, checkpoint,
				"Replay mapped producer/MVMD without duplicate accesses")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 287) { start_mvmd_accumulator(variant + 1); return true; }
			if (status)
			{
				osd_printf_info("TMS320C54x staged MVMD ST1: PASS cases=288 restore=288 sources=AL,BL producers=POPM,STL gaps=0,1,2 pages=2 values=4 source_reads=1 status_writes=1 destination_reads=0 consumer_latency=unvalidated interrupt_composition=unvalidated silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated MVMD ST1 complete");
			}
			osd_printf_info("TMS320C54x staged MVMD accumulator: PASS cases=288 restore=288 sources=AL,BL producers=POPM,STL gaps=0,1,2 pages=2 values=4 source_reads=1 ram_writes=1 mapped_no_conflict=1 status_destination=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated MVMD accumulator complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253840) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253830; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_near_accumulator_results()
{
	if (m_phase == 253790)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const near = near_accumulator_fixture(), producer = profile_is("fcala-producer") || profile_is("cala-producer");
		unsigned const frame = near ? 1 : 2;
		unsigned const prefix = producer ? 1 + variant / 72 : 0, interrupt = producer ? 0 : variant / 72, checkpoint = (producer ? std::array<unsigned, 4>{4, 5 + prefix, 6 + prefix, 8 + prefix} : interrupt ? std::array<unsigned, 4>{4, 6, 8, 12} : std::array<unsigned, 4>{4, 5, 6, 8})[(variant % 72) / 18], caller = std::array<unsigned, 3>{0, 2, 0x7f}[(variant / 2) % 3];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Nondelayed accumulator calls are staged without atomic fallback");
		if (tick == 5 && producer)
		{
			expect(m_port_reads == 2, "DLD owns its RAM target before later input changes");
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1400, 0xdead); m_cpu->space(AS_DATA).write_word(0x1401, 0xbeef);
		}
		if (tick == 5 + prefix)
		{
			m_cpu->set_state_int((variant & 1) ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, 0xdeadbeef);
			if (interrupt) { m_cpu->set_input_line(interrupt == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE); m_cpu->set_input_line(interrupt == 2 ? INPUT_LINE_NMI : 0, CLEAR_LINE); }
		}
		capture_pipeline_at(checkpoint, {&m_pipeline_store_writes, &m_pipeline_operand_reads, &m_pipeline_extensions_reads, &m_pipeline_register_writes, &m_vector_acks, &m_fast_stack_reads, &m_pipeline_drained_fetches, &m_port_reads},
			"Save accumulator call around target sampling and its owned stack publications");
		if (tick == 48)
		{
			expect(m_pipeline_register_writes == 1, "Accumulator call executes its callee once at the six-cycle boundary");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == (caller << 16 | (0x602 + prefix)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_pipeline_store_writes == frame && m_pipeline_operand_reads == frame && m_pipeline_extensions_reads == (interrupt ? 2 : 1) && m_vector_acks == unsigned(bool(interrupt)) && m_fast_stack_reads == unsigned(bool(interrupt)) && m_pipeline_drained_fetches == unsigned(bool(interrupt)) && m_port_reads == (producer ? 2 : 0) && m_cpu->state_int((variant & 1) ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == 0xdeadbeef && m_cpu->state_int((variant & 1) ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == ((variant & 1) ? 0x1234 : 0x2345) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (producer ? 0xa628 : 0xa608) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (interrupt ? 0 : 0x800), "Accumulator call/return retains its target and exact caller frame through producer publication, IRQ/NMI and replay");
			if (replay_pipeline_at(253800, checkpoint,
				"Replay retained accumulator call and matching return")) return true;
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 215) { start_accumulator_call(variant + 1); return true; }
			if (near)
			{
				osd_printf_info(producer ? "TMS320C54x staged CALA producer: PASS cases=216 restore=216 producer=DLD accumulators=2 gaps=0,1,2 caller_pages=3 ignored_upper_targets=3 ram_reads=2 target_retained=1 frame_words=1 frame_roundtrip=1 delayed_acceptance=0 silicon_phase_claim=0\n" : "TMS320C54x staged CALA: PASS cases=216 restore=216 accumulators=2 caller_pages=3 ignored_upper_targets=3 target_sample=5 stack_write=5 target_fetch=8 callee_execute=12 sources=none,IRQ,NMI frame_words=1 frame_roundtrip=1 delayed_acceptance=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, producer ? "TMS320C54x isolated CALA producer complete" : "TMS320C54x isolated staged CALA complete");
			}
			if (producer)
			{
				osd_printf_info("TMS320C54x staged FCALA producer: PASS cases=216 restore=216 producer=DLD accumulators=2 gaps=0,1,2 caller_pages=3 target_pages=3 ram_reads=2 target_retained=1 frame_roundtrip=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated FCALA producer complete");
			}
			osd_printf_info("TMS320C54x staged FCALA: PASS cases=216 restore=216 accumulators=2 caller_pages=3 target_pages=3 target_sample=5 stack_writes=5,6 target_fetch=8 callee_execute=12 sources=none,IRQ,NMI frame_roundtrip=1 producer_latency=unvalidated irq_composition=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated staged FCALA complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253800) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253790; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_poly_results()
{
	if (m_phase == 399980)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = poly_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"POLY is admitted by the staged frontend");
		if (f.dual)
		{
			unsigned const completed = tick < 7 ? 0 : std::min(tick-6,f.iterations); auto const expected = m_repeat_load_expected[completed];
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[2], "Repeated dual MAC uses retained paired operands and the Execute-live feedback accumulator");
			u16 const t = tick < 6 ? f.t : tick > f.iterations+5 ? 0xdead : mac_repeat_memory(f,tick-6,false);
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == t, "Repeated dual MAC publishes X in Read without re-publishing T at Execute");
			if (tick == f.iterations+5) m_cpu->set_state_int(tms320c54x_device::STATE_T,0xdead);
			if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base+0x601,0xe8ff); }
		}
		if (f.square)
		{
			unsigned const completed = tick < 7 ? 0 : std::min(tick-6,f.iterations);
			auto const expected = m_repeat_load_expected[completed];
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[2], "Every repeated square samples the live accumulator at Execute and preserves carry");
			u16 const t = tick > f.iterations+5 ? 0xdead : f.tmac || tick < 6 ? f.t : square_repeat_memory(f,tick-6);
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == t, "Repeated square publishes each original source at Read without overwriting newer T at Execute");
			if (tick == f.iterations+5) m_cpu->set_state_int(tms320c54x_device::STATE_T,0xdead);
			if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base+0x601,0xe8ff); }
		}
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save POLY operand and repeated arithmetic progress");
		}
		if (tick == (f.square || f.dual ? f.iterations+24 : 48))
		{
			auto const expected = f.square || f.dual ? m_repeat_load_expected.back() : poly_reference(f);
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[2],"POLY uses signed 17-bit A high, old B, rounding, saturation and coefficient extension");
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (f.square || f.dual ? 0xdead : f.t) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && (f.dual || m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1800+(f.form == 2 ? f.iterations : 0)),"Repeated arithmetic preserves newer T/status and updates its source pointer once per iteration");
			if (f.square || f.dual) expect(m_pipeline_extensions_reads == 1 && m_cpu->state_int(STATE_GENPC) == f.base+0x603, "Repeated arithmetic retains one opcode and reaches its fresh continuation");
			if (f.dual)
			{
				for (unsigned ar = 2; ar < 6; ++ar)
				{
					u16 const address = ar == (f.form == 1 ? 3U : 2U) ? mac_repeat_address(f,f.iterations,false) : f.form != 3 && ar == (f.form == 1 ? 4U : 3U) ? mac_repeat_address(f,f.iterations,true) : 0x4800+ar;
					expect(m_cpu->state_int(tms320c54x_device::STATE_AR0+ar) == address, "Repeated dual MAC modifies each selected pointer once, with X priority on aliases");
				}
				expect(m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst, "Repeated dual MAC preserves product-saturation mode");
			}
			expect(m_pipeline_operand_reads == f.iterations*(f.dual ? 2 : 1) && !m_pipeline_store_writes && m_cpu->state_int(tms320c54x_device::STATE_IDLE),"Repeated arithmetic completes exact reads and reaches the following IDLE");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				if (f.tmac) m_cpu->set_pipeline_frontend(true);
				return restore_pipeline_at(399990, "Replay POLY without duplicate coefficient reads");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.dual || f.tmac ? 98303 : f.square ? 49151 : 4607)) { start_poly(variant+1); return true; }
			if (f.tmac) { osd_printf_info("TMS320C54x repeated T MAC: PASS cases=98304 restore=98304 operations=MAC,MACR,MAS,MASR destinations=A,B forms=DP,indirect,postincrement patterns=8 flags=16 counts=0,1,7,255 pages=2 snapshots=4 per_clock_accumulator=1 t_preserved=1 source_opcode_poison=1 data_writes=0 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated repeated T MAC complete"); }
			if (f.dual) { osd_printf_info("TMS320C54x repeated dual MAC: PASS cases=98304 restore=98304 operations=MAC,MACR,MAS,MASR selectors=4 layouts=fixed,postinc,postdec,alias patterns=4 flags=16 counts=0,1,7,255 pages=2 snapshots=3 per_clock_accumulator=1 paired_reads=1 t_publish=Read newer_t_preserved=1 source_opcode_poison=1 writes=0 physical_banks=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated repeated dual MAC complete"); }
			if (f.square) { osd_printf_info("TMS320C54x repeated square accumulate: PASS cases=49152 restore=49152 operations=SQURA,SQURS destinations=A,B forms=DP,indirect,postincrement patterns=8 flags=16 counts=0,1,7,255 pages=2 snapshots=4 per_clock_accumulator=1 t_publish=Read newer_t_preserved=1 source_opcode_poison=1 data_writes=0 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated repeated square accumulate complete"); }
			osd_printf_info("TMS320C54x staged POLY: PASS cases=4608 restore=4608 forms=direct,indirect,postincrement patterns=8 flags=8 repeat=none,0,2 pages=2 snapshots=4 coefficient_reads=iterations data_writes=0 mapped_acceptance=0 extended_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
			start_register_product(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == 399990) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 399980; m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	if (m_phase == 400000)
	{
		unsigned const variant=m_pipeline_peripheral_case, tick=++m_idle_fixture_tick; auto const f=register_product_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"T-input products are admitted by the staged frontend");
		if (tick==6+f.length)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_T,0xdead);
			auto const disable=machine().disable_side_effects();
			if ((f.smem || f.poly) && !f.mapped_t) m_cpu->space(AS_DATA).write_word(0x1800,0xdead);
			if (f.length==2) m_cpu->space(AS_PROGRAM).write_word(f.base+0x603,0xdead);
		}
		if (tick==f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot={m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_extensions_reads,m_pipeline_register_writes}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save T-input products before/after operand capture and Execute");
		}
		if (tick==24)
		{
			auto const result=register_product_reference(f);
			if (m_cpu->state_int(tms320c54x_device::STATE_A)!=result[0] || m_cpu->state_int(tms320c54x_device::STATE_B)!=result[1] || m_cpu->state_int(tms320c54x_device::STATE_ST0)!=result[2])
				osd_printf_info("product_dependency_failure: case=%u opcode=%04x layout=%u replay=%u a=%010llx b=%010llx expected_a=%010llx expected_b=%010llx\n",variant,f.opcode,f.mode,unsigned(m_pipeline_replay),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),(unsigned long long)result[0],(unsigned long long)result[1]);
			expect_opcode(f.opcode,m_cpu->state_int(tms320c54x_device::STATE_A)==result[0] && m_cpu->state_int(tms320c54x_device::STATE_B)==result[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0)==result[2],"T-input products use retained operands and Execute accumulator sources with correct arithmetic and flags");
			expect(m_cpu->state_int(tms320c54x_device::STATE_T)==(f.mode==4 ? 0xbeef : 0xdead) && m_cpu->state_int(tms320c54x_device::STATE_ST1)==f.st1,"T-input products preserve later T and ST1");
			expect(m_pipeline_operand_reads==unsigned(f.smem || f.poly) && m_pipeline_store_writes==unsigned(f.mode==1 || f.mode==2 || f.mode==4) && m_pipeline_extensions_reads==1 && m_pipeline_register_writes==f.length-1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE),"Products own only their source, extension and producer transactions and reach the following IDLE");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay=true; m_idle_fixture_tick=f.checkpoint; m_pipeline_operand_reads=m_recurring_irq_snapshot[0]; m_pipeline_store_writes=m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads=m_recurring_irq_snapshot[2]; m_pipeline_register_writes=m_recurring_irq_snapshot[3];
				return restore_pipeline_at(400010, "Replay T-input product operand ownership");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant<69119) { start_register_product(variant+1); return true; }
			if (variant>=176640)
			{
				if (variant<306431) { start_register_product(variant+1); return true; }
				osd_printf_info("TMS320C54x staged remaining T dependencies: PASS cases=129792 restore=129792 POLY=2304 immediate=27648 absolute_Smem=64512 EXP_products=35328 patterns=8 layouts=6 pages=2 snapshots=3 poly_smul=excluded source_reads=0,1 literal_fetches=0,1 t_sample=Read repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
				throw emu_fatalerror(0,"TMS320C54x isolated MAC/MAS complete");
			}
			if (variant>=69120)
			{
				if (variant<176639) { start_register_product(variant+1); return true; }
				osd_printf_info("TMS320C54x staged Smem T dependencies: PASS cases=107520 restore=107520 opcodes=14 patterns=8 flags=16 layouts=5 sources=RAM,mapped_T pages=2 snapshots=3 source_reads=1 t_sample=Read repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
				start_register_product(176640); return true;
			}
			osd_printf_info("TMS320C54x staged accumulator T products: PASS cases=69120 restore=69120 opcodes=18 patterns=8 flags=16 layouts=none,STLM_zero_gap,STLM_one_gap,A_producer,T_successor pages=2 snapshots=3 t_sample=Read accumulator_sample=Execute overflow_initial=clear,sticky data_reads=0 producer_writes=0,1 opcode_fetches=1 repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
			start_register_product(69120); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase==400010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=400000; m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	if (m_phase == 253690)
	{
		bool const bk = profile_is("mac-bk");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const square = !bk && variant >= 73728; unsigned const index = square ? variant - 73728 : variant;
		auto const f = mac_smem_case(index, bk, square);
		unsigned const checkpoint = f.read - 1 + index / (bk ? 32768 : square ? 12288 : 24576);
		bool const absolute = u8(f.opcode) == 0xf8, destination_b = BIT(f.opcode, 8);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "MAC/MAS execute without an atomic fallback");
		if (tick == 4 && absolute) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == f.read) { expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (square ? f.memory : f.t), "Smem arithmetic publishes T only for square instructions in Read"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); }
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save MAC/MAS before/after operand capture or result publication");
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (destination_b ? f.a : f.result) && m_cpu->state_int(tms320c54x_device::STATE_B) == (destination_b ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.result_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1, "MAC/MAS retain memory/T, preserve the other accumulator and apply rounding/SMUL/OVM/sticky overflow");
			expect(m_pipeline_operand_reads == 1 && !m_pipeline_store_writes && m_pipeline_extensions_reads == unsigned(absolute), "MAC/MAS never repeat their sampled source or extension");
			if (bk) expect(m_cpu->state_int(tms320c54x_device::STATE_BK) == 0xdead, "MAC/MAS do not overwrite the later BK value");
			if (replay_pipeline_at(253700, checkpoint,
				"Replay MAC/MAS retained operands and result publication")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (index < (bk ? 98303 : square ? 36863 : 73727)) { start_mac_smem(variant + 1); return true; }
			if (bk) { osd_printf_info("TMS320C54x staged BK MAC/MAS: PASS cases=98304 restore=98304 operations=MAC,MACR,MAS,MASR destinations=A,B forms=direct,absolute pairs=64 flags=16 pages=2 snapshots=3 source_reads=1 data_writes=0 bk_retained=1 t_retained=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated BK MAC/MAS complete"); }
			if (!square)
			{
				osd_printf_info("TMS320C54x staged MAC/MAS: PASS cases=73728 restore=73728 operations=MAC,MACR,MAS,MASR destinations=A,B forms=indirect_RAM,absolute_RAM,mapped_AR1 pairs=64 flags=16 source_reads=1 data_writes=0 t_retained=1 silicon_phase_claim=0\n");
				start_mac_smem(73728); return true;
			}
			osd_printf_info("TMS320C54x staged SQURA/SQURS: PASS cases=36864 restore=36864 operations=add,subtract destinations=A,B forms=indirect_RAM,absolute_RAM,mapped_AR1 pairs=64 flags=16 snapshots=3 source_reads=1 data_writes=0 t_publish=Read t_republish=0 repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
			start_poly(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253700) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253690; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_absolute_logic_results()
{
	if (m_phase == 254160)
	{
		auto const f = absolute_logic_case(m_pipeline_peripheral_case); unsigned const tick = ++m_idle_fixture_tick;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_pipeline_extensions_reads == unsigned(tick >= 3) && m_pipeline_operand_reads == unsigned(tick >= 6) && !m_pipeline_store_writes, "Absolute logic owns its literal and operand without duplicate transactions");
		expect(m_cpu->state_int(f.kind & 1 ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == (tick >= 7 ? f.result() : f.source) && m_cpu->state_int(f.kind & 1 ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x2233445566ULL, "Absolute logic zero-extends independently of SXM and publishes only at Execute");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xb601 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.status && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x2222 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Absolute logic preserves status, pointers, T and stack");
		if (tick == 3) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xf495); }
		if (tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); }
		if (tick == f.snapshot && !m_pipeline_replay) { expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save absolute logic's partial Decode, captured address, operand or publication"); }
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x603, "Absolute logic consumes its address word and reaches the exact next instruction");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_extensions_reads = unsigned(f.snapshot >= 3); m_pipeline_operand_reads = unsigned(f.snapshot >= 6); m_idle_fixture_tick = f.snapshot;
				return restore_pipeline_at(254170, "Restore absolute logic without refetching or rereading captured words");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 1535) { start_absolute_logic(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged absolute logic: PASS cases=1536 restore=1536 operations=AND,OR,XOR accumulators=2 addresses=4 values=4 sxm_states=2 pages=2 snapshots=4 literal_fetches=1 source_reads=1 data_writes=0 retained_address=1 retained_source=1 mapped_acceptance=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated absolute logic complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254170) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254160; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_accumulator_branch_results()
{
	if (m_phase == 254240)
	{
		auto const &f = m_acc_branch_fixture; unsigned const tick = ++m_idle_fixture_tick;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "BACC/FBACC stays in the retained frontend");
		if (tick == 3 && f.irq) { if (f.irq == 1) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1); else { m_cpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); } }
		if (f.producer == 1 && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1400, 0xdead); m_cpu->space(AS_DATA).write_word(0x1401, 0xbeef); }
		if (f.producer == 2 && tick == 10) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.table, 0xdead); }
		if (tick == f.sample()) m_cpu->set_state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, 0xdeadbeef);
		capture_pipeline_at(f.snapshot, {&m_pipeline_store_writes, &m_pipeline_operand_reads, &m_pipeline_extensions_reads, &m_pipeline_register_writes, &m_vector_acks, &m_port_reads, &m_fast_stack_reads, &m_pipeline_drained_fetches, &m_acc_branch_mapped_writes},
			"Save accumulator branch before/after target sampling, held Read and redirected Fetch");
		if (tick == 64)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.destination + 2 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_RTN) == (f.irq ? 0x700 : 0x5555) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == 0x1357 && m_cpu->state_int(tms320c54x_device::STATE_T) == f.marker(), "Accumulator branch neither creates a call frame nor executes flushed sequential words");
			expect(m_pipeline_register_writes == 1 && m_pipeline_drained_fetches == 1 && m_pipeline_extensions_reads == (f.irq ? 2 : 1) && m_pipeline_store_writes == unsigned(bool(f.irq)) && m_pipeline_operand_reads == unsigned(bool(f.irq)) && m_vector_acks == unsigned(bool(f.irq)) && m_port_reads == (f.producer == 1 ? 2 : 0) && m_fast_stack_reads == unsigned(f.producer == 2) && m_acc_branch_mapped_writes == unsigned(f.producer == 2), "Accumulator branch and producer own exact, nonduplicated transactions through replay and IRQ return");
			expect(m_cpu->state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == 0xdeadbeef && m_cpu->state_int(f.b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == f.other && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.st1 & ~(f.irq ? 0x800 : 0)), "Branch preserves accumulators and flags apart from deliberate capture mutation and IRQ INTM handling");
			if (replay_pipeline_at(254250, f.snapshot,
				"Restore retained branch target without resampling, creating a frame or duplicating an interrupt")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < (f.producer ? 431 : 215)) { start_accumulator_branch(m_pipeline_peripheral_case + 1); return true; }
			if (f.producer) { osd_printf_info(f.near ? "TMS320C54x staged BACC producer: PASS cases=432 restore=432 producers=DLD,READA accumulators=2 gaps=0,1,2 caller_pages=3 upper_targets=3 snapshots=4 frame_words=0 target_retained=1 mapped_publication=1 delayed_acceptance=0 silicon_phase_claim=0\n" : "TMS320C54x staged FBACC producer: PASS cases=432 restore=432 producers=DLD,READA accumulators=2 gaps=0,1,2 caller_pages=3 upper_targets=3 snapshots=4 frame_words=0 target_retained=1 mapped_publication=1 delayed_acceptance=0 silicon_phase_claim=0\n"); throw emu_fatalerror(0, f.near ? "TMS320C54x isolated BACC producer complete" : "TMS320C54x isolated FBACC producer complete"); }
			osd_printf_info(f.near ? "TMS320C54x staged BACC: PASS cases=216 restore=216 accumulators=2 caller_pages=3 upper_targets=3 snapshots=4 target_sample=5 target_fetch=8 target_execute=12 sources=none,IRQ,NMI frame_words=0 target_retained=1 sequential_flushed=1 delayed_acceptance=0 silicon_phase_claim=0\n" : "TMS320C54x staged FBACC: PASS cases=216 restore=216 accumulators=2 caller_pages=3 upper_targets=3 snapshots=4 target_sample=5 target_fetch=8 target_execute=12 sources=none,IRQ,NMI frame_words=0 target_retained=1 sequential_flushed=1 delayed_acceptance=0 silicon_phase_claim=0\n"); throw emu_fatalerror(0, f.near ? "TMS320C54x isolated staged BACC complete" : "TMS320C54x isolated staged FBACC complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254250) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254240; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254260)
	{
		auto const &f = m_offset_logic_fixture; unsigned const tick = ++m_idle_fixture_tick;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_pipeline_operand_reads == unsigned(tick >= 6) && m_pipeline_extensions_reads == unsigned(tick >= 3) && !m_pipeline_store_writes, "Long memory logic retains exact literal and source transactions");
		expect(m_cpu->state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == (tick >= 7 ? f.result() : f.source) && m_cpu->state_int(f.b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == 0x2233445566ULL, "Long memory logic zero-extends its source independently of SXM/OVM/C16 and publishes at Execute");
		expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= 5 ? f.final_st0() : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0x5555, "Long logical addressing changes only the legitimate ARP field and selected pointer");
		if (tick == 3) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xf495); }
		if (tick == 5)
		{
			for (unsigned r = 0; r < 8; ++r) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + r) == (r == f.selected ? f.next : offset_logic_fixture::initial_ar(r)), "Long logic resolves the encoded/compatibility AR and applies one offset modification at Access");
			for (unsigned r = 0; r < 8; ++r) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + r, 0x3330 + r);
		}
		if (tick >= 5) for (unsigned r = 0; r < 8; ++r) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + r) == 0x3330 + r, "Retained logical addresses do not reapply a pointer update after mutation or replay");
		if (tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); }
		if (tick == f.snapshot && !m_pipeline_replay) { expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save long logical literal, modified pointer, sampled word or result"); }
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x603 && m_cpu->state_int(tms320c54x_device::STATE_BK) == (f.linear ? 0 : 13), "Long memory logic consumes its extension and leaves BK unchanged");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.snapshot; m_pipeline_operand_reads = unsigned(f.snapshot >= 6); m_pipeline_extensions_reads = unsigned(f.snapshot >= 3); return restore_pipeline_at(254270, "Restore long memory logic without refetching, rereading or reapplying the modifier");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 294911) { start_offset_logic(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged long memory logic: PASS cases=294912 restore=294912 operations=AND,OR,XOR accumulators=2 modes=24 offsets=4 value_pairs=4 sxm_ovm=4 cmpt=2 bk=0,13 pages=2 snapshots=4 literal_fetches=1 source_reads=1 data_writes=0 pointer_updates_once=1 native_offset1462=1 mapped_acceptance=0 repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated long memory logic complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254270) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254260; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_memory_immediate_results()
{
	if (m_phase == 254340)
	{
		auto const f = memory_immediate_case(m_pipeline_peripheral_case); unsigned const tick = ++m_idle_fixture_tick;
		expect(m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst, "Memory multiply preserves the configured SMUL mode");
		u64 a = f.a; if (tick > f.read && f.address == 8) a = (a & ~0xffffULL) | 0xdead; if (tick > f.read && f.address == 9) a = (a & ~0xffff0000ULL) | 0xdead0000ULL;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (tick > f.read && !f.bdst ? f.result : a) && m_cpu->state_int(tms320c54x_device::STATE_B) == (tick > f.read && f.bdst ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick > f.read ? f.result_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1, "Memory/immediate MPY publishes signed product and sticky overflow only at Execute");
		expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (tick > f.read ? 0xdead : tick == f.read || f.address == 0x0e ? f.word : 0x9abc), "MPY publishes the memory word to T during Read, independent of later T mutation");
		if (tick == 3 || (f.words == 3 && tick == 4)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x5fe + tick, 0xf495); }
		if (tick == f.access) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == std::array<unsigned, 13>{0x140c, 0x140c, 0x140d, 0x140b, 0x140d, 0x140c, 0x140f, 0x1402, 0x140c, 0x140c, 0x140c, 0x140c, 0x140c}[f.form], "MPY applies its selected pointer modifier once"); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x3333); }
		if (tick == f.read) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); }
		if (tick == f.checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save MPY across literal, address, Read and Execute ownership"); }
		if (tick == 14)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == f.words - 1 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x3333 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x601 + f.words, "MPY keeps captured address/source/literals and resumes after the complete instruction");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Replay memory/immediate MPY without duplicate operand accesses"); ++m_memory_immediate_restores; m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 254350; m_check_timer->enable(true); return true; }
			unsigned const variant = m_pipeline_peripheral_case; m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_memory_immediate_coverage.add(memory_immediate_key(variant));
			expect(m_memory_immediate_restores == m_memory_immediate_coverage.count, "Every selected memory/immediate input completed a real restore");
			if (variant + 1 == memory_immediate_mpy_inputs) osd_printf_info("TMS320C54x memory immediate MPY: PASS cases=%u restore=%u forms=12 values=8 literals=8 flags=8 accumulators=2 pages=2 snapshots=4 checkpoint_policy=balanced source_reads=1 data_writes=0 t_publication=Read address_literal_retained=1 repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0\n", m_memory_immediate_coverage.count, m_memory_immediate_restores);
			if (variant + 1 < memory_immediate_inputs) { start_memory_immediate_multiply(variant + 1); return true; }
			osd_printf_info("TMS320C54x memory immediate MAC: PASS cases=%u restore=%u ram_forms=8 values=8 literals=8 flags=16 selectors=4 pages=2 snapshots=4 checkpoint_policy=balanced source_reads=1 data_writes=0 t_publication=Read repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0 coverage_cases=%u coverage_fnv64=%016llx\n", m_memory_immediate_coverage.count - memory_immediate_mpy_inputs, m_memory_immediate_restores - memory_immediate_mpy_inputs, m_memory_immediate_coverage.count, (unsigned long long)m_memory_immediate_coverage.value);
			throw emu_fatalerror(3, "TMS320C54x isolated memory immediate MPY complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254350) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254340; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_bd_long_accumulator_results()
{
	if (m_phase == 254540)
	{
		bool const mac = profile_is("mac-delay");
		bool const square = profile_is("square-delay");
		bool const memory_square = profile_is("squr-delay");
		bool const long_acc = profile_is("bd-maca-long");
		bool const acc = square || memory_square || long_acc || profile_is("maca-delay");
		bool const rounded = profile_is("mpyr-delay");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = long_acc ? bd_long_accumulator_case(variant) : memory_square ? delay_memory_square_case(variant) : square ? delay_square_case(variant) : acc ? delay_accumulator_multiply_case(variant) : delay_multiply_case(variant, mac, rounded);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "A complete memory MPY fits its control-delay words");
		if (acc)
		{
			u64 const mask = long_acc && tick <= f.read() ? 0xffffffffffULL : 0;
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (!f.destination && tick > f.read() ? f.result : f.a ^ mask) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination && tick > f.read() ? f.result : f.b ^ mask), "Delayed multiply publishes only its selected accumulator in Execute");
			if (tick == f.read() - 1) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.address_ar) == (!long_acc && f.form == 2 ? 0x5001 : 0x5000), "Delayed accumulator multiply owns one pointer update before poisoning");
		}
		if (((f.form && !acc) || long_acc) && tick == f.literal_clock()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.caller + 0x601 + f.delay(), 0x5010); }
		if (tick == f.read() - 1) { if (rounded) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == (f.form == 0 ? 0x5001 : 0x5000), "Delayed MPYR owns its postincrement before pointer poisoning"); m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.address_ar, 0x6000); }
		if (tick == f.read()) { expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (acc ? f.memory : f.t), "Multiply owns its T publication or retained T source at Read"); auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); }
		if (long_acc && tick == f.read()) { m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b); }
		if (tick == f.checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save delayed multiply Decode, Access and retained Read operands"); }
		if (tick == 24)
		{
			u32 const continuation = f.kind == 2 ? f.caller + 0x606 : f.target + 0x702;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == continuation && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == (f.kind == 4 ? 2U : unsigned(f.kind == 3)) && m_fast_stack_reads == unsigned(f.kind == 5) && m_pipeline_extensions_reads == unsigned(long_acc || (f.form != 0 && !acc)) && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.destination ? f.a : f.result) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_TRN) == u16(f.result) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((acc ? 0xa020 : 0xa008) | (f.overflow() ? f.destination ? 0x200 : 0x400 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((acc ? 0x800 | (f.flags & 8 ? 0x100 : 0) : 0x900) | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.kind == 4 ? 0x2ffe : f.kind == 3 ? 0x2fff : f.kind == 5 ? 0x3001 : 0x3000), "Delayed MPY preserves independent signed arithmetic, status, frames and target page through replay");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[4]; if (memory_square || long_acc) m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(254550, "Replay delayed multiply without repeated source reads or callee publications"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (long_acc ? 98303 : memory_square ? 27647 : acc ? 55295 : rounded ? 36863 : mac ? 147455 : 18431)) { start_delay_multiply(variant + 1); return true; }
			if (long_acc) { osd_printf_info("TMS320C54x BD long accumulator multiply: PASS cases=98304 restore=98304 operations=MPYA,MASA,MACA,MACAR forms=positive_offset,negative_offset,absolute address_registers=8 patterns=8 flags=16 pages=2 snapshots=4 extension_reads=1 source_reads=1 accumulators=Execute t_publish=Read target_observes_result=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated BD long accumulator multiply complete"); }
			if (memory_square) { osd_printf_info("TMS320C54x delayed memory SQUR: PASS cases=27648 restore=27648 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD destinations=2 forms=DP,indirect,postincrement patterns=8 frct_ovm_smul_sxm=16 pages=2 snapshots=3 source_reads=1 t_publication=Read newer_t_preserved=1 smul_product_clamp=0 target_observes_result=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed memory SQUR complete"); }
			if (square) { osd_printf_info("TMS320C54x delayed square accumulation: PASS cases=55296 restore=55296 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=SQURA,SQURS destinations=2 forms=DP,indirect,postincrement patterns=8 frct_ovm_smul_sxm=16 pages=2 snapshots=3 source_reads=1 t_publication=Read smul_product_clamp=0 target_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed square accumulation complete"); }
			if (rounded) { osd_printf_info("TMS320C54x delayed MPYR: PASS cases=36864 restore=36864 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=postincrement,absolute_RAM,absolute_AR7,offset destinations=2 operand_pairs=16 frct_ovm_smul=8 pages=2 snapshots=3 source_reads=1 rounding=biased_high_word smul_product_clamp=0 t_retained=1 callee_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed MPYR complete"); }
			if (acc) { osd_printf_info("TMS320C54x delayed accumulator multiply: PASS cases=55296 restore=55296 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=MPYA,MASA,MACA,MACAR forms=DP,indirect,postincrement patterns=8 frct_ovm_smul_sxm=16 pages=2 snapshots=3 source_reads=1 t_publication=Read target_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed accumulator multiply complete"); }
			if (mac) { osd_printf_info("TMS320C54x staged delay MAC/MAS: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=MAC,MACR,MAS,MASR forms=short,absolute_RAM,absolute_AR5,offset destinations=2 operand_pairs=16 frct_ovm_smul=8 pages=2 snapshots=3 source_reads=1 t_retained=1 callee_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delay MAC/MAS complete"); }
			osd_printf_info("TMS320C54x staged delay MPY: PASS cases=18432 restore=18432 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=short,absolute_RAM,absolute_AR,offset destinations=2 operand_pairs=16 frct_ovm=4 pages=2 snapshots=3 source_reads=1 callee_observes_result=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delay MPY complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254550) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254540; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_nibble_input_results()
{
	if (m_phase == 399900)
	{
		unsigned const tick = ++m_idle_fixture_tick, variant = m_pipeline_peripheral_case;
		capture_pipeline_at(14, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads, &m_pipeline_store_writes},
			"Save original nibble helper before shifted-store dependency");
		if (tick == 64)
		{
			u16 const expected = (nibble_input(variant) >> (4 * (3 - variant % 4))) & 15;
			expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == expected && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1400 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x1357, "Complete original nibble helper returns the independently selected nibble and restores its frame");
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == 1 && m_pipeline_store_writes == 2,
				"Shifted-store bypass avoids stale backing reads without suppressing real writes");
			if (replay_pipeline_at(399910, 14,
				"Restore original nibble dependency without duplicate accesses")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 127) { start_original_nibble(variant + 1); return true; }
			osd_printf_info("TMS320C54x original nibble helper: PASS cases=128 restore=128 words=23 selectors=4 values=8 sxm=2 pages=2 count_reads=1 input_reads=1 count_writes=2 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated store bypass complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 399910) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 399900; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_asm_mapped_results()
{
	if (m_phase == 255140)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = asm_mapped_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x8800 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (tick > f.read ? (f.st1 & ~31) | f.raw : f.st1), "Mapped LD ASM publishes only ASM in Execute from its retained Read");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (tick <= f.read ? f.a : 0x1122334455ULL) && m_cpu->state_int(tms320c54x_device::STATE_B) == (tick <= f.read ? f.b : 0xeeddccbbaaULL) && m_cpu->state_int(tms320c54x_device::STATE_T) == (tick <= f.read ? f.t : 0xbeef) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == (tick <= f.read ? f.trn : 0x2222), "Mapped LD ASM does not rewrite a replaced source or alter other registers");
		for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (tick < f.read - 1 ? f.initial[i] : tick <= f.read ? f.final[i] : 0x2200 + i), "Mapped LD ASM applies one Access-owned auxiliary update");
		if (tick == 3 && f.form >= 2) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == f.read) { m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1122334455ULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xeeddccbbaaULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x2222); for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x2200 + i); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads},
			"Save mapped LD ASM before and after source capture and publication");
		if (tick == 16)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(f.form >= 2), "Mapped LD ASM completes without duplicate reads or literals");
			if (replay_pipeline_at(255150, f.checkpoint,
				"Restore mapped LD ASM without resampling replaced sources")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 49151) { start_asm_mapped(variant + 1); return true; }
			osd_printf_info("TMS320C54x mapped LD ASM: PASS cases=49152 restore=49152 sources=16 forms=DP,postincrement,offset,absolute shifts=32 status_states=4 pages=2 snapshots=3 source_sample=Read publish=Execute data_writes=0 pointer_updates=once physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated mapped LD ASM complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255150) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255140; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_ts_arithmetic_results()
{
	if (m_phase == 255060)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; bool const producer = profile_is("ts-alu-producer"); auto const f = ts_arithmetic_case(variant, producer); auto const &o = f.address;
		u16 const st0 = tick > o.read_cycle ? f.result_st0 : tick >= o.read_cycle - 1 ? o.result_st0 : o.st0;
		bool const state_ok = !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (!(f.selector & 1) && tick > o.read_cycle ? f.result : f.a) && m_cpu->state_int(tms320c54x_device::STATE_B) == ((f.selector & 1) && tick > o.read_cycle ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == o.st1;
		if (!state_ok) osd_printf_info("ts_arithmetic_state: producer=%u case=%u tick=%u a=%llx b=%llx result=%llx st0=%llx expected=%04x\n", unsigned(producer), variant, tick, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)f.result, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST0), st0);
		expect(state_ok, "TS arithmetic publishes retained shifted data and ALU carry overflow saturation only in Execute");
		if (tick == 4 && (producer ? f.producer_kind == 0 : o.form >= 14)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == o.read_cycle - 1) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + o.selected) == o.next_ar, "TS arithmetic modifies its captured source pointer once"); m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + o.selected, 0x2222); }
		if (tick == o.read_cycle) { expect(m_cpu->state_int(tms320c54x_device::STATE_T) == f.t, "TS arithmetic distinguishes visible producer T from a retained restricted-gap sample"); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t ^ 63); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(o.address, 0xdead); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads, &m_pipeline_store_writes},
			"Save TS arithmetic address Read operands and publication");
		if (tick == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_T) == (f.t ^ 63) && m_cpu->state_int(tms320c54x_device::STATE_AR0 + o.selected) == 0x2222 && m_pipeline_operand_reads == (producer && f.producer_kind == 1 ? 2U : 1U) && m_pipeline_store_writes == unsigned(producer && f.producer_kind != 1) && m_pipeline_extensions_reads == unsigned(producer ? f.producer_kind == 0 : o.form >= 14), "TS arithmetic preserves owned operands and never repeats source reads writes or pointer updates");
			if (replay_pipeline_at(255070, f.checkpoint,
				"Restore TS arithmetic without resampling memory T or literals")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < (producer ? 18432U : 154368U)) { start_ts_arithmetic(variant + 1); return true; }
			if (producer) { osd_printf_info("TMS320C54x TS arithmetic producers: PASS cases=18432 restore=18432 producers=STM_T,LD_T,STLM_T gaps=0,1 shift_codes=8 memory_values=4 sxm_ovm=4 selectors=4 pages=2 snapshots=3 t_sample=Read projection_only_stlm_gap=0 duplicate_transactions=0 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated TS arithmetic producers complete"); }
			osd_printf_info("TMS320C54x TS arithmetic: PASS address_cases=6528 arithmetic_cases=147456 restore=153984 operations=ADD,SUB selectors=4 address_forms=17 registers=8 compatibility_states=2 shifts=48 memory_values=4 accumulator_patterns=8 sxm_ovm=4 pages=2 snapshots=3 t_sample=Read duplicate_transactions=0 mapped_sources=excluded physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated TS arithmetic complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255070) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255060; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_addm_brc_loop_results()
{
	if (m_phase == 276020)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, count = std::array<unsigned, 4>{0, 1, 3, 7}[variant % 4], checkpoint = std::array<unsigned, 5>{6, 7, 8, 10, 16}[variant / 64];
		bool const call = (variant / 8) % 2, delayed = (variant / 16) % 2; u32 const base = (variant / 32) % 2 ? 0x20000 : 0, body = base + (call ? 0x700 : 0x604) + (delayed ? 4 : 2);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "ADDM BRC followed by a separated RPTB or RPTBD is supported");
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, 0, 0}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save BRC arithmetic publication or consuming loop"); }
		if (tick == 128)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == (call ? base + 0x606 : body + 3) && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1201 + count && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1, "A legal-gap loop consumes the ADDM result for exactly count plus one iterations");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; return restore_pipeline_at(276030, "Restore BRC publication and loop without duplicate arithmetic writes"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 319) { start_addm_brc_loop(variant + 1); return true; }
			osd_printf_info("TMS320C54x ADDM BRC loop consumers: PASS cases=320 restore=320 counts=0,1,3,7 increments=1,-1 separators=NOP,CALL controls=RPTB,RPTBD pages=2 snapshots=5 ordered_publication=1 actual_iterations=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated ADDM BRC loop consumers complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 276030) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 276020; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_delay_compare_results()
{
	if (m_phase == 254980)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delay_compare_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Complete short CMPM BITF is admitted in a BD BCD delay span");
		if (tick == 5 + f.prefix && f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		if (tick == f.literal_clock) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x603 + f.prefix, 0xdead); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601 + f.prefix, 0xbeef); }
		if (tick == f.read - 1 && f.form == 2) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1003, "Delay comparison updates its pointer once in Access"); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x2200); }
		if (tick == f.read) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1002, 0xdead); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads, &m_vector_acks, &m_pipeline_store_writes},
			"Save delayed comparison with settled IRQ and retained operand ownership");
		if (tick == 64)
		{
			// A pending IRQ may wake the prefetched IDLE and return after it.
			u64 const pc = m_cpu->state_int(STATE_GENPC);
			bool const on_path = pc >= f.base + (f.taken ? 0x700 : 0x604 + f.prefix) && pc < f.base + (f.taken ? 0x740 : 0x640);
			bool const state_ok = on_path && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.taken ? 0x11 : 0x22) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xae20 | (f.final_tc ? 0x1000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x100 | (f.form == 1 ? 0x4000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == (f.form == 2 ? 0x2200 : 0x1002);
			if (!state_ok) osd_printf_info("delay_compare_state: case=%u replay=%u pc=%llx idle=%llu a=%llx b=%llx st0=%llx sp=%llx ar2=%llx t=%llx taken=%u tc=%u reads=%u literals=%u acks=%u\n", variant, unsigned(m_pipeline_replay), (unsigned long long)m_cpu->state_int(STATE_GENPC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST0), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR2), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_T), unsigned(f.taken), unsigned(f.final_tc), m_pipeline_operand_reads, m_pipeline_extensions_reads, m_vector_acks);
			expect(state_ok, "Delay comparison publishes TC without changing the retained branch decision or duplicating pointer updates");
			expect(m_pipeline_operand_reads == (f.prefix ? 2U : 1U) && m_pipeline_extensions_reads == 2 + unsigned(bool(f.prefix)) && !m_pipeline_store_writes && m_vector_acks == unsigned(bool(f.interrupt)), "Delay comparison retains literal and source across one deferred IRQ NMI entry");
			if (replay_pipeline_at(254990, f.checkpoint,
				"Restore delay comparison and deferred interrupt without repeated transactions")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_cpu->set_pipeline_frontend(false);
			if (variant < 27647) { start_delay_compare(variant + 1); return true; }
			osd_printf_info("TMS320C54x delayed CMPM BITF: PASS cases=27648 restore=27648 controls=BD,BCD_TC,BCD_NTC operations=2 forms=DP,SP,postincrement value_pairs=16 initial_tc=2 prefix=none,BITF sources=none,IRQ,NMI pages=2 snapshots=4 branch_decision=retained literal_reads=once operand_writes=0 interrupt_after_publication=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated delayed comparison complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254990) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254980; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_addc_chain_results()
{
	if (m_phase == 62158)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = addc_chain_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Adjacent ADDC consumes the previous ALU carry without a separating instruction");
		if (tick == 5 || tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1002 + tick - 5, 0xa55a); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads},
			"Save carry chain across the producer Execute and consumer Read");
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x603 && m_pipeline_operand_reads == 2 && m_pipeline_extensions_reads == 2 && !m_pipeline_store_writes, "ADDC chain retains two words and retires two instructions once");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == f.final_a && m_cpu->state_int(tms320c54x_device::STATE_B) == f.final_b && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.final_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "ADDC chain consumes the generated carry and updated accumulator with OVM and sticky overflow");
			if (replay_pipeline_at(62159, f.checkpoint,
				"Restore adjacent ADDC without losing a pending carry transition")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 4095) { start_addc_chain(variant + 1); return true; }
			osd_printf_info("TMS320C54x ADDC chains: PASS cases=4096 restore=4096 destinations=AA,BA,AB,BB accumulator_pairs=4 word_pairs=4 carry_states=2 sxm_ovm=4 pages=2 snapshots=4 issue_gap=0 read_retention=1 carry_sample=Execute physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated ADDC chains complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_mvdm_acc_results()
{
	if (m_phase == 62160)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = mvdm_acc_case(variant);
		unsigned const checkpoint = std::array<unsigned, 3>{5, 6, 9}[f.snapshot];
		unsigned const shift = (f.reg % 3) * 16;
		u64 const mask = (f.reg % 3 == 2 ? 0xffULL : 0xffffULL) << shift;
		u64 const merged = (((f.reg < 3 ? f.a : f.b) & ~mask) | ((u64(f.value) << shift) & mask));
		u64 const a = f.reg < 3 ? merged : f.a, b = f.reg >= 3 ? merged : f.b;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Owned MVDM source remains admitted");
		if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == 6)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == b && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1, "MVDM merges only its encoded slice at publication before the following consumer");
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, f.value ^ 0xffff);
		}
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save MVDM around literal capture destination publication and consumer");
		if (tick == 12)
		{
			expect(m_pipeline_operand_reads == 1U && m_pipeline_store_writes == 1U && m_pipeline_extensions_reads == 1, "MVDM has one retained literal and no duplicate source or destination access");
			if (m_cpu->state_int(tms320c54x_device::STATE_A) != a || m_cpu->state_int(tms320c54x_device::STATE_B) != b || m_cpu->state_int(tms320c54x_device::STATE_ST0) != 0xa608 || m_cpu->state_int(tms320c54x_device::STATE_ST1) != f.st1)
				osd_printf_info("MVDM accumulator mismatch: case=%u replay=%u a=%010llx expected=%010llx b=%010llx expected=%010llx st0=%04x st1=%04x expected_st1=%04x\n", variant, unsigned(m_pipeline_replay), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)a, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)b, unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), f.st1);
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == b && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "MVDM preserves neighboring slices and status independently of SXM OVM CPL and FRCT");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x605, "Immediate ALU consumer sees the new accumulator without a gap");
			if (replay_pipeline_at(62161, checkpoint,
				"Replay MVDM without recapturing its literal or source")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < 110592U) { start_mvdm_acc(variant + 1); return true; }
			osd_printf_info("TMS320C54x MVDM accumulator: PASS cases=110592 restore=110592 destinations=AL,AH,AG,BL,BH,BG sources=RAM,AR,BK words=8 patterns=8 status_modes=16 pages=2 snapshots=3 reads=1 writes=1 neighboring_slices=preserved guard_mask=8 source_mutation_after_read=1 consumer_gap=0 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated MVDM accumulator complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62161) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62160; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62159) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62158; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62156)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const arithmetic = profile_is("unsigned-map");
		bool const shift16 = profile_is("shift16-map");
		bool const logic = profile_is("logic-map");
		unsigned const extent = shift16 || logic ? 36864 : 110592; auto const f = logic ? mapped_logic_case(variant) : shift16 ? mapped_shift16_case(variant) : addc_case(variant % extent, arithmetic ? 1 + variant / extent : 0);
		unsigned const checkpoint = f.read_clock - 1 + f.snapshot;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Owned unsigned or logical source remains admitted");
		if (tick == f.read_clock)
		{
			expect(m_pipeline_operand_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == f.next_ar3, "Unsigned arithmetic resolves and updates its address once before operand mutation");
			set_repeat_count_source(f.address, 0xa55a);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, f.after_a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.after_b); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.execute_st0);
			for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x3330 + i);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save unsigned arithmetic before Read, after capture or after Execute");
		}
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1U && !m_pipeline_store_writes && m_pipeline_extensions_reads == unsigned(f.extension), "Unsigned arithmetic owns one source and optional extension but no memory write");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == f.final_a && m_cpu->state_int(tms320c54x_device::STATE_B) == f.final_b && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.final_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1, "Unsigned arithmetic uses retained Smem and Execute-live accumulator with sticky overflow and OVM");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (0x3330 + i), "Unsigned arithmetic never reapplies an auxiliary update after Read or restore");
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (f.address == 14 ? 0xa55a : f.t) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == (f.address == 15 ? 0xa55a : f.trn) && m_cpu->state_int(tms320c54x_device::STATE_BK) == f.bk && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Unsigned arithmetic preserves unselected registers and does not rewrite a mutated mapped operand");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; if (shift16 || logic) m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(62157, "Restore unsigned arithmetic without duplicated reads or address updates");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < extent * (logic ? 3U : shift16 ? 4U : arithmetic ? 2U : 1U)) { start_addc(variant + 1); return true; }
			if (logic)
			{
				osd_printf_info("TMS320C54x mapped memory logic: PASS cases=110592 restore=110592 operations=AND,OR,XOR mapped_sources=16 modes=DP,absolute,indirect accumulators=2 flags=8 patterns=4 late_accumulator=2 pages=2 snapshots=3 zero_extend=1 source_sample=Read accumulator_sample=Execute flags_preserved=1 writes=0 reset_before_restore=1 physical_phase_claim=0 producer_control_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated mapped memory logic complete");
			}
			if (shift16)
			{
				osd_printf_info("TMS320C54x mapped shift16 arithmetic: PASS cases=147456 restore=147456 forms=8 mapped_sources=16 modes=DP,absolute,indirect flags=8 patterns=4 late_source=2 pages=2 snapshots=3 source_sample=Read accumulator_sample=Execute partial_carry=1 sticky_overflow=1 ovm=1 writes=0 reset_before_restore=1 physical_phase_claim=0 producer_control_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated mapped shift16 arithmetic complete");
			}
			if (arithmetic)
			{
				osd_printf_info("TMS320C54x unsigned mapped arithmetic: PASS cases=221184 restore=221184 operations=ADDS,SUBS numeric_cases=147456 mapped_cases=73728 ram_forms=6 mapped_sources=16 accumulators=2 carry_inputs=ignored sxm_ovm=4 words=8 accumulator_patterns=8 late_accumulator=2 pages=2 snapshots=3 unsigned_smem=1 physical_phase_claim=0 control_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated unsigned mapped arithmetic complete");
			}
			osd_printf_info("TMS320C54x ADDC: PASS cases=110592 restore=110592 numeric_cases=73728 mapped_cases=36864 ram_forms=6 mapped_sources=16 accumulators=2 carry_states=2 sxm_ovm=4 words=8 accumulator_patterns=8 late_accumulator_carry=2 pages=2 snapshots=3 unsigned_smem=1 physical_phase_claim=0 control_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated ADDC complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62157) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62156; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_sqdst_results()
{
	if (m_phase == 62142)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const lms = !strncmp(machine().options().bios(), "lms", 3);
		auto const c = sqdst_case(variant, lms); auto const &f = c.memory;
		unsigned const checkpoint = 3 + f.snapshot;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "SQDST admits its RAM operands");
		u64 const mask = tick <= 5 ? 0xffffffffffULL : 0;
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (tick >= 6 ? c.result_a : c.a ^ mask) && m_cpu->state_int(tms320c54x_device::STATE_B) == (tick >= 6 ? c.result_b : c.b ^ mask) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= 6 ? c.result_st0 : f.st0), "SQDST computes both results from Execute-owned old A/B and retains independent overflow flags");
		if (tick == 4)
		{
			for (unsigned i = 0; i < 4; ++i) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR2 + i) == f.final[i], "SQDST updates aliased pointers with X priority"); m_cpu->set_state_int(tms320c54x_device::STATE_AR2 + i, 0x2200 + i); }
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 32);
		}
		if (tick == 5)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef, "SQDST does not publish T");
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_A, c.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, c.b);
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.xaddress, 0xdead); m_cpu->space(AS_DATA).write_word(f.yaddress, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save SQDST around address capture and paired Read");
		}
		if (tick == 12)
		{
			expect(m_pipeline_operand_reads == 2 && m_pipeline_extensions_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst, "SQDST retains memory samples without changing T or mode bits");
			for (unsigned i = 0; i < 4; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2 + i) == 0x2200 + i, "SQDST does not repeat pointer updates");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				if (lms) m_cpu->set_pipeline_frontend(true);
				m_check_timer->adjust(attotime::never); expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore SQDST without duplicate reads or publications"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62143; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < 402432U) { start_sqdst(variant + 1); return true; }
			if (lms)
			{
				osd_printf_info("TMS320C54x staged LMS: PASS address_cases=9216 arithmetic_cases=393216 restore=402432 encodings=256 layouts=3 bk_modes=2 value_pairs=64 old_a_patterns=8 old_b_patterns=8 arithmetic_flags=32 snapshots=3 reads=2 writes=0 t_unchanged=1 old_accumulators=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated\n"); throw emu_fatalerror(3,"TMS320C54x isolated LMS complete");
			}
			osd_printf_info("TMS320C54x staged SQDST: PASS address_cases=9216 arithmetic_cases=393216 restore=402432 encodings=256 layouts=3 bk_modes=2 value_pairs=64 old_a_patterns=8 old_b_patterns=8 arithmetic_flags=32 snapshots=3 reads=2 writes=0 t_unchanged=1 old_accumulators=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated SQDST complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62143) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62142; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_native_dual_sub_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 700000 && m_phase < 700256)
	{
		unsigned const index = m_phase - 700000;
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		static constexpr u16 values[] = { 0, 1, 0x7fff, 0x8000, 0xffff, 0xfffb, 7, 0x1234 };
		u16 const x = values[index % 8], y = values[(index / 8) % 8];
		bool const sxm = BIT(index, 6), ovm = BIT(index, 7);
		// SPRU172C dual-memory SUB: sign-extend each word before shifting by 16.
		s64 const difference = (sxm ? s64(s16(x)) - s16(y) : s64(x) - y) * 65536;
		bool const overflow = difference < -0x80000000LL || difference > 0x7fffffffLL;
		u64 const result = overflow && ovm ? (difference < 0 ? 0xff80000000ULL : 0x7fffffffULL) : u64(difference) & 0xffffffffffULL;
		expect_opcode(0xa245,
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == result &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0x56789a &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x4444 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1fff &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x20ff &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((x >= y ? 0x800 : 0) | (overflow ? 0x400 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | ((index / 64) << 8)) &&
			data.read_word(0x2000) == x && data.read_word(0x2100) == y,
			"Exact native dual SUB preserves sources and independently decrements both pointers");
		if (!m_rom4_codec_replay)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore native dual SUB fixture");
			m_phase = 700000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 255) { start_native_dual_sub(index + 1); return true; }
		osd_printf_info("TMS320C54x native dual SUB: PASS cases=256 restore=256 opcode=a245 patterns=8x8 sxm=0,1 ovm=0,1 physical_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated native dual SUB complete");
	}
	if (m_phase >= 525000 && m_phase < 525128)
	{
		unsigned const index = m_phase - 525000;
		auto const f = parallel_sub_fraction_case(index);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Atomic ST||SUB is admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		// SPRU172C 4-185: subtract the whole opposite accumulator, not its stored high word.
		u64 const other = f.destination_b ? f.a : f.b;
		u64 const result = (u64(s64(s16(f.x)) * 65536) - other) & 0xffffffffffULL;
		u16 const stored = u16((f.source_b ? f.b : f.a) >> 16);
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (f.destination_b ? f.a : result) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination_b ? result : f.b) &&
			data.read_word(0x2100) == stored && data.read_word(0x2000) == f.x &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
			"ST||SUB retains fractional borrow and stores the pre-instruction source even when source equals destination");
		if (!m_rom4_codec_replay)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore fractional parallel subtraction");
			m_phase = 525000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 127) { start_parallel_sub_fraction(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic ST||SUB fractions: PASS cases=128 restore=128 source_dest_pairs=4 fractional_patterns=8 memory_patterns=4 ordinary_ram=1 physical_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated ST||SUB fractions complete");
	}
	return false;
}

bool tms320c54x_test_state::check_atomic_xorm_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 520000 && m_phase < 521024)
	{
		unsigned const index = m_phase - 520000;
		auto const f = atomic_xorm_case(index);
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x600 + f.words)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save XORM after its single read/write transaction");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Atomic XORM remains admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		auto const disable = machine().disable_side_effects();
		expect(m_rom4_codec_replays == 1 && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 &&
			data.read_word(f.address) == u16(f.source ^ f.mask) && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x601 + f.words &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == f.next && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 2 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (index % 8 == 1 ? 0x1500 : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789ULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xff76543210ULL &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1357 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1e2a &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1,
			"XORM preserves arithmetic state and updates its address once independently of SXM/OVM");
		if (!m_rom4_codec_replay)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore XORM without duplicating its resolved transaction");
			m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
			m_phase = 520000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 1023) { start_atomic_xorm(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic XORM: PASS cases=1024 restore=1024 forms=8 values=4 literals=4 sxm_ovm=4 snapshots=2 reads=1 writes=1 flags_preserved=1 staged_acceptance=0 silicon_phase_claim=0\n");
		m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
		throw emu_fatalerror(3, "TMS320C54x isolated XORM complete");
	}
	return false;
}


} // namespace c54x_test
