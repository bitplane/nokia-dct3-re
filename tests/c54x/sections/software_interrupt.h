// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 128; ++index) {
		bool const trap = index >= 64, masked = index & 32;
		unsigned const vector = index & 31;
		u16 const table = index & 1 ? 0x4000 : 0x2000;
		u32 const target = 0x10000 | (table + vector * 4);
		u16 const flags = !trap && vector >= 16 ? u16(0xffff & ~(1U << (vector - 16))) : 0xffff;
		u16 const st1 = masked ? 0x980 : 0x180;
		auto initial = initial_case("software-interrupt", {u16((trap ? 0xf4c0 : 0xf7c0) | vector),0xf4e1});
		for (auto &word : initial.program) word.address += 0x10000;
		initial.program.insert(initial.program.end(),{{target,0xe955},{target + 1,0xf4e1},{target + 2,0xf49b}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x10600},{r::STATE_PMST,table},{r::STATE_SP,0x1000},
			{r::STATE_ST1,st1},{r::STATE_IFR,0xffff},{r::STATE_IMR,0}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,trap ? 24U : 48U};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("IPTR-vector-entry-and-mask-semantics", {{STATE_GENPC,target + 2},
			{r::STATE_SP,0xfff},{r::STATE_ST1,trap ? st1 : 0x980U},{r::STATE_IFR,flags},{r::STATE_IMR,0},{r::STATE_B,0x55}},{{0xfff,0x601}})});
		if (!trap) {
			c.actions = {{24,{{STATE_GENPC,target + 2},{r::STATE_IDLE,0}},false,{}, {},{{0xfff,0xdead}}}};
			c.states.push_back({25,observation("fast-return-in-progress", {})});
			c.states.push_back({48,observation("RTN-not-corrupted-stack-word", {{STATE_GENPC,0x10602},{r::STATE_SP,0x1000},
				{r::STATE_ST1,0x180},{r::STATE_IFR,flags},{r::STATE_IMR,0}},{{0xfff,0xdead}})});
		}
		c.data_events = std::vector<clocked_data_event>{{true,0xfff,0x601,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
