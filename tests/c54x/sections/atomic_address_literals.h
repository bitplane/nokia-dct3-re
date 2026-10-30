// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](char const *name, std::initializer_list<u16> words,
		std::initializer_list<register_setting> setup, std::initializer_list<register_check> checks,
		std::vector<clocked_data_event> events = {}) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0},
			{r::STATE_AR0,1},{r::STATE_BK,4}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		auto final = observation(name,checks);
		final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		for (auto const &e : events) { initial.data.push_back({e.address,e.value}); final.expected_data.push_back({e.address,e.value}); }
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-address-operation", {{STATE_GENPC,0x600}})},
			{2,observation("operation-running", {})},{16,std::move(final)}},1,16};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	append("MAR-signed-extension",{0x6ded,0xfff9},{{r::STATE_AR5,0x1000}},{{r::STATE_AR5,0xff9}});
	append("MAR-circular-AR0-no-extension",{0x6ddc},{{r::STATE_AR4,0xc03}},{{r::STATE_AR4,0xc00},{r::STATE_AR0,1},{r::STATE_BK,4}});
	append("SUB-memory-shift16",{0x4092},{{r::STATE_AR2,0xd00},{r::STATE_A,0x50000},{r::STATE_B,0x1234}},
		{{r::STATE_A,0x20000},{r::STATE_B,0x1234},{r::STATE_AR2,0xd01}},{{false,0xd00,3,0,false}});
	append("DLD-sign-extends-B-stationary",{0x5781},{{r::STATE_AR1,0xe00},{r::STATE_A,0x1234},{r::STATE_ST1,0x100}},
		{{r::STATE_B,0xff80012345ULL},{r::STATE_A,0x1234},{r::STATE_AR1,0xe00}},
		{{false,0xe00,0x8001,0,false},{false,0xe01,0x2345,0,false}});
	return cases;
}
}
