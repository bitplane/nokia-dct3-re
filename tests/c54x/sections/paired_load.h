// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned destination = 0; destination < 2; ++destination)
	for (unsigned flags = 0; flags < 4; ++flags) for (unsigned negative = 0; negative < 2; ++negative)
	for (unsigned parity = 0; parity < 2; ++parity) for (unsigned compiler = 0; compiler < 2; ++compiler)
	{
		bool const sxm = flags & 1;
		u16 const offset = 0x7c + parity, address = (compiler ? 0x500 : 0x400) + offset;
		u16 const high = negative ? 0x8001 : 0x1234;
		u64 const result = (sxm && negative ? 0xff00000000ULL : 0) | (negative ? 0x8001abcdULL : 0x1234abcdULL);
		u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (flags & 2 ? 0x80 : 0) | (compiler ? 0x4000 : 0);
		auto initial = initial_case("paired-load", {u16(0x5600 | (destination << 8) | offset)});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_SP,0x500},
			{r::STATE_A,0x1111222233ULL},{r::STATE_B,0x4444555566ULL}});
		initial.data = {{address,high},{unsigned(address ^ 1),0xabcd}};
		initial.expected.insert(initial.expected.end(), {{r::STATE_A,0x1111222233ULL},{r::STATE_B,0x4444555566ULL},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_SP,0x500}});
		auto final = observation("pair-published", {{r::STATE_A,destination ? 0x1111222233ULL : result},
			{r::STATE_B,destination ? result : 0x4444555566ULL},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_SP,0x500}});
		clocked_register_case c{{{0,std::move(initial)},{6,std::move(final)}},4 + (destination + flags + negative + parity + compiler) % 2,9};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions = {{5,{}}}; c.actions.front().data = {{address,1},{unsigned(address ^ 1),2}};
		c.data_events = std::vector<clocked_data_event>{{false,address,high,5},{false,unsigned(address ^ 1),0xabcd,5}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
