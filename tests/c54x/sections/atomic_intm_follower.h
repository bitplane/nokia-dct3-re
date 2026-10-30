// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned kind = 0; kind != 3; ++kind) {
		auto initial = initial_case("atomic-INTM-clear-completes-whole-follower",{0xf6bb,u16(kind == 0 ? 0xe807 : kind == 1 ? 0x76f8 : 0xfc00)});
		if (kind == 1) { initial.program[2].value = 0x501; initial.program[3].value = 7; }
		for (auto &word : initial.program) word.address += 0x103d0;
		initial.program.push_back({0x10040,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x109d0},{r::STATE_PMST,0},{r::STATE_ST1,0x800},
			{r::STATE_SP,0x1000},{r::STATE_A,0},{r::STATE_IMR,1},{r::STATE_IFR,1}});
		initial.data = {{0x501,0},{0x1000,0x9e0}};
		u16 const continuation = kind == 0 ? 0x9d2 : kind == 1 ? 0x9d4 : 0x9e0;
		u16 const stack = kind == 2 ? 0x1000 : 0xfff;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("after-INTM-clear-before-protected-follower",{})},
			{24,observation("one-complete-follower-before-vector-entry",{{STATE_GENPC,0x10041},{r::STATE_SP,stack},
				{r::STATE_RTN,continuation},{r::STATE_A,kind == 0 ? 7U : 0U},{r::STATE_ST1,0x800},{r::STATE_IFR,0}},
				{{stack,continuation},{0x501,u16(kind == 1 ? 7 : 0)}})}},1,24};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{};
		if (kind == 1) c.data_events->push_back({true,0x501,7,0,false});
		if (kind == 2) c.data_events->push_back({false,0x1000,0x9e0,0,false});
		c.irqs = {{0,0,~0U,unsigned(kind != 0),continuation}};
		c.data_events->push_back({true,stack,continuation,0,false});
		c.program_events = {{false,0x10040,0xf4e1,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
