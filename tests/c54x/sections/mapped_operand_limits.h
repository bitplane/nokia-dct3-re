// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 sources[]{0,1,6,7,0x1a,0x1b,0x1c,0x1d,0x24,0x58};
	for (unsigned operation = 0; operation < 5; ++operation)
	for (unsigned selector = 0; selector < (operation == 4 ? 5U : 10U); ++selector)
	{
		bool const destination = operation == 4;
		u16 const literal = destination ? std::array<u16,5>{0,0x18,0x19,0x1a,0x1f}[selector] : sources[selector];
		auto initial = initial_case("mapped-operand-limit", {std::array<u16,5>{0x00f8,0x01f8,0x7092,0x7212,0x70f8}[operation],literal,0x18});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa008},{r::STATE_ST1,0x900},
			{r::STATE_AR2,0x1400},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
		initial.data = {{0x1400,0xaaaa}};
		initial.expected.clear();
		// These are staged-model admission limits, not silicon-illegal operands.
		auto final = observation("rejected-without-effects", {{r::STATE_ILLEGAL,1},{r::STATE_SP,0x1000},
			{r::STATE_ST0,0xa008},{r::STATE_AR2,0x1400},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0x2233445566ULL}}, {{0x1400,0xaaaa}});
		final.expected.pop_back();
		clocked_register_case c{{{0,std::move(initial)},{12,std::move(final)}},3,12};
		c.require_idle = false; c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
