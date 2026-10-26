// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

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
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0800 | test.status);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, test.pmst);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, destination_b ? 0x12345678 : test.initial);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, destination_b ? test.initial : 0x12345678);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, test.t);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x0500);
	m_cpu->set_state_int(STATE_GENPC, 0x010980);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
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

} // namespace c54x_test
