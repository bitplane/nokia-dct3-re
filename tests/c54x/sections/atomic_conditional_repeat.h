// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned carry = 0; carry != 2; ++carry) {
		auto initial = initial_case("XC-two-words-tests-carry", {0xff0c,0xf495,0xf793,0xf4e1});
		initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,carry ? 0x800U : 0U},{r::STATE_B,0x1234}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-XC-snapshot", {{STATE_GENPC,0x5ff}})},
			{2,observation("XC-running", {})},{32,observation("complement-or-skip-exactly-two-words", {{STATE_GENPC,0x604},
				{r::STATE_B,carry ? 0xffffffedcbULL : 0x1234ULL},{r::STATE_ST0,carry ? 0x800U : 0U}})}},1,32};
		c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	for (unsigned form = 0; form != 5; ++form) {
		u16 const mode = std::array<u16,5>{0x82,0xf8,0xe2,0xea,0xf2}[form];
		u16 const source = form == 2 || form == 3 ? 0xd05 : form == 4 ? 0xd01 : 0xd00;
		unsigned const extended = form != 0;
		auto initial = initial_case("RPT-memory-count-sampled-once", {u16(0x4700 | mode)});
		if (extended) initial.program[1].value = mode == 0xf8 ? source : 5;
		initial.program[extended + 1].value = 0x6d93;
		initial.program[extended + 2].value = 0xf4e1;
		initial.program.push_back({0x5ff,0xf495}); initial.data = {{source,2}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_AR2,0xd00},{r::STATE_AR3,0},{r::STATE_BK,4}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-RPT-snapshot", {{STATE_GENPC,0x5ff}})},
			{2,observation("repeat-running", {})},{32,observation("three-pointer-updates-from-count-two", {{STATE_GENPC,0x603 + extended},
				{r::STATE_AR2,form >= 3 ? source : 0xd00U},{r::STATE_AR3,3},{r::STATE_RC,0}},{{source,2}})}},form >= 2 ? 7U : 1U,32};
		c.data_events = std::vector<clocked_data_event>{{false,source,2,0,false}};
		if (form >= 2) {
			// Leading NOP, then two-word RPT: its one count Read is clock 7.
			c.data_events->front().cycle = 7; c.data_events->front().check_cycle = true;
			c.actions = {{7,{},false,{},{},{{source,0xdead}}}};
			c.states.back().observation.expected_data.front().value = 0xdead;
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
