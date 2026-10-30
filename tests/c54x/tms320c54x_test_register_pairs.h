// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_REGISTER_PAIRS_H
#define MAME_NOKIA_TMS320C54X_TEST_REGISTER_PAIRS_H

#include "tms320c54x_test_case.h"
#include <array>
#include <string_view>

namespace c54x_test {

enum class register_pair_kind { none, saturation, rotate };
inline register_pair_kind select_register_pair(std::string_view name)
{
	return name == "sat" ? register_pair_kind::saturation : name == "rotate" ? register_pair_kind::rotate : register_pair_kind::none;
}
constexpr unsigned register_pair_count(register_pair_kind kind)
{
	return kind == register_pair_kind::saturation ? 3072 : 4608;
}

template<typename R>
clocked_register_case register_pair_case(unsigned variant, register_pair_kind kind)
{
	bool const sat = kind == register_pair_kind::saturation;
	unsigned const form = variant % 6, flags = (variant / 48) % 4;
	bool const b = sat ? variant & 1 : form & 1;
	u32 const base = (variant / (sat ? 512 : 768)) % 2 ? 0x20000 : 0;
	u16 const opcode = sat ? b ? 0xf583 : 0xf483 : 0xf490 + form / 2 + (b ? 0x100 : 0);
	u64 value = sat ? std::array<u64, 16>{
		0, 1, 0x7ffffffeULL, 0x7fffffffULL, 0x80000000ULL, 0x80000001ULL,
		0xff7fffffffULL, 0xff80000000ULL, 0xff80000001ULL, 0xffffffffffULL,
		0x7fffffffffULL, 0x8000000000ULL, 0x0100000000ULL, 0xfeffffffffULL,
		0x007fffff00ULL, 0xff80000100ULL}[(variant / 2) % 16] :
		std::array<u64, 8>{0, 1, 0x80000000ULL, 0xffffffffULL, 0x5fb0001234ULL, 0x81c0005555ULL, 0xff00000000ULL, 0x7f12345678ULL}[(variant / 6) % 8];
	u16 const initial_st0 = sat ? 0x1824 | (((variant / 32) % 4) << 9) : 0x0024 | (flags & 1 ? 0x800 : 0) | (flags & 2 ? 0x1000 : 0);
	u16 const st1 = 0x800 | (((variant / (sat ? 128 : 192)) % 4) << 8);
	// Independent unsigned 40-bit regions and literal 32-bit clamp limits.
	bool const negative = value & 0x8000000000ULL;
	bool const overflow = negative ? value < 0xff80000000ULL : value > 0x7fffffffULL;
	u64 const saturated = overflow ? negative ? 0xff80000000ULL : 0x7fffffffULL : value;
	bool carry = flags & 1;
	clocked_register_case result{{}, std::array<unsigned, 3>{5, 6, 7}[variant / (sat ? 1024 : 1536)], 16};
	for (unsigned n = 0; n < 3; ++n)
	{
		u16 status = initial_st0;
		if (n && sat)
		{
			value = saturated;
			u16 const mask = b ? 0x200 : 0x400;
			status = (status & ~mask) | (n == 1 && overflow ? mask : 0);
		}
		else if (n)
		{
			// Bit-position oracle: rotations clear guards, and ROLTC uses TC
			// rather than forwarding carry as its incoming bit.
			bool const right = form / 2 == 0, incoming = form / 2 == 2 ? bool(flags & 2) : carry;
			bool const outgoing = (value >> (right ? 0 : 31)) & 1;
			value = right ? ((value & 0xffffffffULL) >> 1) | (u64(incoming) << 31) : ((value & 0x7fffffffULL) << 1) | incoming;
			carry = outgoing;
			status = 0x0024 | (carry ? 0x800 : 0) | (flags & 2 ? 0x1000 : 0);
		}
		instruction_case state{sat ? "sat" : "rotate", opcode, {}, {}, {},
			{{int(b ? R::STATE_B : R::STATE_A), value}, {int(b ? R::STATE_A : R::STATE_B), 0x123456789aULL},
			 {R::STATE_ST0, status}, {R::STATE_ST1, st1}, {R::STATE_ILLEGAL, 0}}, 0, 0};
		state.reset_before = false;
		result.states.push_back({n ? 5 + n : 0, std::move(state)});
	}
	auto &initial = result.states.front().observation;
	for (unsigned i = 0; i < 12; ++i) initial.program.push_back({base + 0x9000 + i, 0xf495});
	initial.program[0].value = initial.program[1].value = opcode; initial.program[2].value = 0xf4e1;
	for (auto const &value : initial.expected) initial.initial.push_back({value.index, value.value});
	initial.initial.insert(initial.initial.end(), {{R::STATE_IDLE, 0}, {R::STATE_PMST, 0}, {R::STATE_TCR, 0x10},
		{R::STATE_IFR, 0}, {R::STATE_IMR, 0}, {STATE_GENPC, base + 0x9000}});
	return result;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_REGISTER_PAIRS_H
