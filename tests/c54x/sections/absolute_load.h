// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 values[]{0,0xffff,0x8000,0x1234};
	constexpr u64 signed_values[]{0,0xffffffffffULL,0xffffff8000ULL,0x1234};
	for (unsigned n = 0; n < 64; ++n)
	{
		bool const b = n & 1, sxm = n & 2;
		unsigned const pattern = (n / 4) % 4;
		u16 const address = n & 4 ? 0x430 : 0xa531;
		u32 const base = n & 8 ? 0x20000 : 0;
		u64 const value = sxm ? signed_values[pattern] : values[pattern];
		auto initial = initial_case("absolute-load", {u16(b ? 0x11f8 : 0x10f8),address});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},
			{r::STATE_ST0,0x61b},{r::STATE_ST1,sxm ? 0x4900 : 0x4800},{r::STATE_AR2,0x470},
			{r::STATE_A,b ? 0x2233445566ULL : 0x123456789aULL},
			{r::STATE_B,b ? 0x123456789aULL : 0x2233445566ULL}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_A,b ? 0x2233445566ULL : 0x123456789aULL},
			{r::STATE_B,b ? 0x123456789aULL : 0x2233445566ULL},
			{r::STATE_ST0,0x61b},{r::STATE_ST1,sxm ? 0x4900U : 0x4800U},{r::STATE_SP,0x1000},{r::STATE_AR2,0x470}});
		initial.data = {{address,values[pattern]},{0x650,0xbad}};
		auto loaded = observation("loaded", {}); loaded.expected = initial.expected;
		loaded.expected[b ? 2 : 1].value = value;
		clocked_register_case c{{{0,std::move(initial)},{7,std::move(loaded)}},
			std::array<unsigned,4>{3,4,6,7}[n / 16],11};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.retained_program_reads = {{base + 0x601,address}};
		c.actions.push_back({3,{},false,{{base + 0x601,0x650}}});
		clocked_register_action poison{6,{}}; poison.data = {{address,0xdead}};
		c.actions.push_back(std::move(poison));
		c.data_events = std::vector<clocked_data_event>{{false,address,values[pattern],6}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
