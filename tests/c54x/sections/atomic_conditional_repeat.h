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
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-XC-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("XC-running", {})},{32,observation("complement-or-skip-exactly-two-words", {{STATE_GENPC,0x604},
				{r::STATE_B,carry ? 0xffffffedcbULL : 0x1234ULL},{r::STATE_ST0,carry ? 0x800U : 0U}})}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	for (unsigned absolute = 0; absolute != 2; ++absolute) {
		auto initial = initial_case("RPT-memory-count-sampled-once", {u16(absolute ? 0x47f8 : 0x4782)});
		if (absolute) initial.program[1].value = 0xd00;
		initial.program[absolute + 1].value = 0x6d93;
		initial.program[absolute + 2].value = 0xf4e1;
		initial.program.push_back({0x5ff,0xf495}); initial.data = {{0xd00,2}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_AR2,0xd00},{r::STATE_AR3,0}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-RPT-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("repeat-running", {})},{32,observation("three-pointer-updates-from-count-two", {{STATE_GENPC,0x603 + absolute},
				{r::STATE_AR2,0xd00},{r::STATE_AR3,3},{r::STATE_RC,0}},{{0xd00,2}})}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{{false,0xd00,2,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
