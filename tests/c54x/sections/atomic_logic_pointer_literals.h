// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row {
		std::vector<u16> words;
		std::vector<register_setting> initial;
		std::vector<register_check> expected;
		std::vector<memory_word> data, final_data;
		std::vector<clocked_data_event> events;
	};
	std::vector<row> const rows{
		{{0xf032,0xf0f},{{r::STATE_A,0xff12345678ULL}},{{r::STATE_A,0x1438},{r::STATE_ST0,0x800}}, {},{},{}},
		{{0xf035,0xff},{{r::STATE_A,0xff12345678ULL}},{{r::STATE_A,0x1660},{r::STATE_ST0,0x800}}, {},{},{}},
		{{0xf0fb},{{r::STATE_A,0xff80000013ULL},{r::STATE_ST0,0}},{{r::STATE_A,0x4000000},{r::STATE_ST0,0x800}}, {},{},{}},
		{{0xf0fe},{{r::STATE_A,0xff80000003ULL},{r::STATE_ST0,0}},{{r::STATE_A,0x20000000},{r::STATE_ST0,0x800}}, {},{},{}},
		{{0xe741},{{r::STATE_AR1,0x1234},{r::STATE_AR4,0xcdef}},{{r::STATE_AR1,0xcdef},{r::STATE_AR4,0xcdef}}, {},{},{{false,0x14,0xcdef,0,false},{true,0x11,0xcdef,0,false}}},
		{{0x1181},{{r::STATE_B,0},{r::STATE_AR1,0xf20}},{{r::STATE_B,0xffffff8001ULL},{r::STATE_AR1,0xf20}},{{0xf20,0x8001}},{{0xf20,0x8001}},{{false,0xf20,0x8001,0,false}}},
		{{0x1a8a},{{r::STATE_A,0x120000},{r::STATE_AR2,0xf20}},{{r::STATE_A,0x1200f0},{r::STATE_AR2,0xf1f}},{{0xf20,0xf0}},{{0xf20,0xf0}},{{false,0xf20,0xf0,0,false}}},
		{{0x12d2},{{r::STATE_A,0},{r::STATE_AR2,0xf23}},{{r::STATE_A,0x8001},{r::STATE_AR2,0xf20}},{{0xf23,0x8001}},{{0xf23,0x8001}},{{false,0xf23,0x8001,0,false}}},
		{{0x1cf8,0xf20},{{r::STATE_A,0x12000f}},{{r::STATE_A,0x1200ff},{r::STATE_ST0,0x800}},{{0xf20,0xf0}},{{0xf20,0xf0}},{{false,0xf20,0xf0,0,false}}},
		{{0x6982,0xa0},{{r::STATE_AR2,0xf20}},{{r::STATE_AR2,0xf20},{r::STATE_ST0,0x800}},{{0xf20,0x1001}},{{0xf20,0x10a1}},{{false,0xf20,0x1001,0,false},{true,0xf20,0x10a1,0,false}}},
		{{0x6984,5},{{r::STATE_AR4,0xf24}},{{r::STATE_AR4,0xf24}},{{0xf24,0x8000},{0xf20,0x10a1}},{{0xf24,0x8005},{0xf20,0x10a1}},{{false,0xf24,0x8000,0,false},{true,0xf24,0x8005,0,false}}},
		{{0x1b8d},{{r::STATE_B,0x34000f},{r::STATE_AR5,0xf25}},{{r::STATE_B,0x3400ff},{r::STATE_AR5,0xf24},{r::STATE_ST0,0x800}},{{0xf25,0xf0}},{{0xf25,0xf0}},{{false,0xf25,0xf0,0,false}}},
		{{0x4595},{{r::STATE_B,0},{r::STATE_AR5,0xf30}},{{r::STATE_B,0xffff800000ULL},{r::STATE_AR5,0xf31}},{{0xf30,0xff80}},{{0xf30,0xff80}},{{false,0xf30,0xff80,0,false}}},
		{{0x4917},{{r::STATE_B,0},{r::STATE_AR7,0x8001}},{{r::STATE_B,0x8001},{r::STATE_AR7,0x8001}}, {},{},{{false,0x17,0x8001,0,false}}},
		{{0x6883,0xff0},{{r::STATE_AR3,0xf33}},{{r::STATE_AR3,0xf33},{r::STATE_ST0,0x800}},{{0xf33,0xff0f}},{{0xf33,0xf00}},{{false,0xf33,0xff0f,0,false},{true,0xf33,0xf00,0,false}}},
		{{0x6983,0xf0},{{r::STATE_AR3,0xf33}},{{r::STATE_AR3,0xf33},{r::STATE_ST0,0x800}},{{0xf33,0xf00}},{{0xf33,0xff0}},{{false,0xf33,0xf00,0,false},{true,0xf33,0xff0,0,false}}}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("logic-and-circular-pointer-literals", {0xf495}); initial.program.clear();
		initial.program.push_back({0x600,0xf495});
		for (unsigned i = 0; i != v.words.size(); ++i) initial.program.push_back({0x601 + i,v.words[i]});
		initial.program.push_back({0x601 + u32(v.words.size()),0xf4e1}); initial.data = v.data;
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,0x800},{r::STATE_ST1,0x100},{r::STATE_BK,4}});
		initial.initial.insert(initial.initial.end(),v.initial.begin(),v.initial.end());
		auto final = observation("fixed-literal-results-with-explicit-source-state", {{STATE_GENPC,0x602 + v.words.size()}});
		final.expected.insert(final.expected.end(),v.expected.begin(),v.expected.end()); final.expected_data = v.final_data;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-literal-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("literal-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = v.events; cases.push_back(std::move(c));
	}
	for (bool tc : {false,true}) {
		auto initial = initial_case("XC-two-MAR-slots-NTC", {0xf495,0xff20,0x6d91,0x6d92,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR1,0x120},{r::STATE_AR2,0x130},{r::STATE_ST0,tc ? 0x1000U : 0U}});
		auto final = observation("both-slots-execute-or-both-annul", {{STATE_GENPC,0x605},
			{r::STATE_AR1,tc ? 0x120U : 0x121U},{r::STATE_AR2,tc ? 0x130U : 0x131U},{r::STATE_ST0,tc ? 0x1000U : 0U}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-XC-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("XC-running", {})},{24,std::move(final)}},1,24};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{}; cases.push_back(std::move(c));
	}
	return cases;
}
}
