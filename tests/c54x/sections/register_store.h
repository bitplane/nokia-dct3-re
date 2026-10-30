// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned n = 0; n < 27; ++n)
	{
		bool const sequence = n >= 24;
		unsigned const kind = n % 6, form = (n / 6) % 4;
		u16 const target = !sequence && form == 1 ? 0x1020 : 0x470;
		u16 const value = std::array<u16,6>{0x789a,0x3456,0x5566,0x3344,0xbeef,0xcafe}[kind];
		u16 const opcode = std::array<u16,6>{0x8000,0x8200,0x8100,0x8300,0x8c00,0x8d00}[kind] |
			std::array<u16,4>{0x70,0x20,0x83,0x93}[form];
		auto initial = sequence ? initial_case("register-store-sequence", {0x8393,0x8193,0xf495,0x8c93}) :
			initial_case("register-store", {opcode});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x608},
			{r::STATE_ST1,!sequence && form == 1 ? 0x4800 : 0x800},{r::STATE_AR3,0x470},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL},
			{r::STATE_T,sequence ? 6 : 0xbeef},{r::STATE_TRN,0xcafe}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_ST0,0x608},
			{r::STATE_ST1,!sequence && form == 1 ? 0x4800U : 0x800U},
			{r::STATE_SP,0x1000},{r::STATE_AR3,0x470},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL},
			{r::STATE_T,sequence ? 6U : 0xbeefU},{r::STATE_TRN,0xcafe}});
		initial.data = {{target,0xaaaa},{target + 1,0xaaaa},{target + 2,0xaaaa},{0x650,0xaaaa}};
		auto state = [&](u16 ar, std::initializer_list<memory_word> memory = {}) {
			auto result = observation("retained-store", {}); result.expected = initial.expected;
			result.expected[4].value = ar; result.expected_data = memory; return result;
		};
		unsigned const point = sequence ? std::array<unsigned,3>{5,6,8}[n - 24] : 4 + (kind + form) % 3;
		clocked_register_case c{{{0,initial}},point,12};
		c.require_idle = false; c.check_pipeline_clock = true;
		if (sequence)
		{
			c.states.push_back({5,state(0x471)});
			c.states.push_back({6,state(0x472,{{0x470,0x3344}})});
			c.states.push_back({7,state(0x472,{{0x470,0x3344},{0x471,0x5566}})});
			c.states.push_back({8,state(0x473,{{0x470,0x3344},{0x471,0x5566}})});
			c.states.push_back({9,state(0x650,{{0x470,0x3344},{0x471,0x5566},{0x472,6},{0x650,0xaaaa}})});
			c.data_events = std::vector<clocked_data_event>{{true,0x470,0x3344,6},{true,0x471,0x5566,7},{true,0x472,6,9}};
		}
		else
		{
			c.states.push_back({5,state(form == 3 ? 0x471 : 0x470)});
			c.states.push_back({6,state(0x650,{{target,value},{0x650,0xaaaa}})});
			c.data_events = std::vector<clocked_data_event>{{true,target,value,6}};
		}
		c.actions.push_back({sequence ? 8U : 5U,{{r::STATE_AR3,0x650}}});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
