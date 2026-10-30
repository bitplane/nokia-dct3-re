// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 9; ++variant) {
		bool const compatibility = variant < 2 || variant == 3 || variant == 7;
		bool const marked = variant == 2 || variant == 8;
		u16 const st0 = compatibility ? 0x8000 : variant == 8 ? 0x800 : 0;
		u16 const st1 = compatibility ? 0x20 : variant == 2 ? 0x300 : variant == 4 || variant == 5 || variant == 8 ? 0x100 : 0;
		constexpr u16 input[]{0,0x1234,0x8007,4,0x7fff,0xffff,0x8000,0xf0,0};
		constexpr u16 literal[]{0,0,0xfff8,0x123b,1,1,0x8000,0,0};
		constexpr u16 output[]{0,0x1234,0x8000,0x123f,0x8000,0,0,0x33,0};
		constexpr u16 flags[]{0x8000,0x8000,0xc00,0x8000,0x400,0x800,0x400,0x8000,0x800};
		std::vector<u16> words;
		if (variant < 2) words = {u16(variant ? 0x1090 : 0x6d90)};
		else if (variant < 7) words = {u16(variant == 3 ? 0x6b80 : 0x6b8a),literal[variant]};
		else if (variant == 7) words = {0x6880,0xf0f,0x6980,0x33};
		else words = {0x1c8b,0x1d93,0x1c82};
		if (marked) { words.insert(words.begin(),{0x75d6,0x124}); words.insert(words.end(),{0x75d6,0x124}); }
		words.push_back(0xf4e1);
		auto initial = initial_case("atomic-compatibility-memory-and-fixed-ADDM-boundaries",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR0,variant < 2 ? 0x10U : 0x130U},{r::STATE_AR4,0x120},
			{r::STATE_AR2,variant == 8 ? 0x150U : 0x120U},{r::STATE_AR3,0x140},{r::STATE_AR6,0x103},{r::STATE_BK,4},
			{r::STATE_A,variant == 8 ? 0x1234U : 0U},{r::STATE_B,variant == 8 ? 0x5678U : 0U},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		initial.data = {{0x120,input[variant]},{0x130,0x7777},{0x103,0x1234},{0x100,0x5678},{0x140,0xf0},{0x13f,0xff80},{0x150,0xf00}};
		auto final = observation("compatibility-alias-flags-memory-and-independent-pointers",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_AR0,variant < 2 ? 0x10U : 0x130U},{r::STATE_AR4,variant < 2 ? 0x121U : 0x120U},
			{r::STATE_AR2,variant == 8 ? 0x150U : variant >= 2 && variant < 7 && variant != 3 ? 0x11fU : 0x120U},
			{r::STATE_AR3,0x140},{r::STATE_AR6,marked ? 0x101U : 0x103U},
			{r::STATE_A,variant == 1 ? 0x1234U : variant == 8 ? 0x1dc4U : 0U},
			{r::STATE_B,variant == 8 ? 0xa9f8U : 0U},{r::STATE_ST0,flags[variant],0xec00}});
		final.expected_data = {{0x120,output[variant]},{0x130,0x7777}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-memory-program",{{STATE_GENPC,0x601}})},
			{2,observation("memory-program-running",{})},{48,std::move(final)}},marked ? 3U : 1U,48};
		c.pipeline_frontend = false;
		if (marked) {
			c.observed_data_space = AS_IO;
			c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1234,0,false},{true,0x124,0x5678,0,false,{},variant == 8 ? 5U : 4U}};
		} else {
			c.data_events = std::vector<clocked_data_event>{};
			if (variant == 1) c.data_events->push_back({false,0x120,0x1234,0,false});
			if (variant >= 2 && variant < 7) *c.data_events = {{false,0x120,input[variant],0,false},{true,0x120,output[variant],0,false}};
			if (variant == 7) *c.data_events = {{false,0x120,0xf0,0,false},{true,0x120,0,0,false},
				{false,0x120,0,0,false},{true,0x120,0x33,0,false}};
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
