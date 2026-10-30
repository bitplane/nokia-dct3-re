// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 6; ++variant) {
		bool const returning = variant < 2, branch = variant == 3 || variant == 4;
		bool const taken = variant == 0 || variant == 3;
		std::vector<u16> words{0x75f8,0x100,0x124};
		if (returning) words.push_back(0xfc4d);
		else if (variant == 2) words.insert(words.end(),{0x47e2,5,0x6d10});
		else if (branch) words.insert(words.end(),{0xf84e,0x700});
		else words.insert(words.end(),{0xec01,0x6d10});
		words.insert(words.end(),{0x75f8,0x101,0x124,0xf4e1});
		auto initial = initial_case("atomic-absolute-marked-return-branch-and-repeat",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.program.insert(initial.program.end(),{{0x700,0x75f8},{0x701,0x102},{0x702,0x124},{0x703,0xf4e1}});
		u64 const b = variant == 1 || variant == 3 ? 1 : variant == 4 ? 0xff00000000ULL : 0;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,b},{r::STATE_ST0,0},{r::STATE_ST1,0},
			{r::STATE_SP,returning ? 0x2ffU : 0x300U},{r::STATE_AR0,variant == 5 ? 0x120U : 0U},{r::STATE_AR2,0x120},{r::STATE_RC,0}});
		initial.data = {{0x100,0x1111},{0x101,0x2222},{0x102,0x3333},{0x2ff,0x700},{0x125,2}};
		auto final = observation("selected-path-stack-repeat-and-port-time",{{STATE_GENPC,taken ? 0x704U : 0x601+words.size()},
			{r::STATE_A,0},{r::STATE_B,b},{r::STATE_SP,returning && !taken ? 0x2ffU : 0x300U},
			{r::STATE_AR0,variant == 2 ? 3U : variant == 5 ? 0x122U : 0U},{r::STATE_AR2,0x120},{r::STATE_RC,0}});
		final.expected_data = initial.data;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-control-markers",{{STATE_GENPC,0x601}})},
			{2,observation("control-markers-running",{})},{64,std::move(final)}},3,64};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},
			{true,0x124,u16(taken ? 0x3333 : 0x2222),0,false,{},variant == 2 ? 10U : taken ? 8U : 6U}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
