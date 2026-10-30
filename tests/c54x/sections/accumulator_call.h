// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned b = 0; b < 2; ++b) {
		auto initial = initial_case("accumulator-call", {u16(b ? 0xf5e3 : 0xf4e3),0xf4e1});
		initial.program.insert(initial.program.end(), {{0x700,0xf4e1},{0x701,0xf49b}});
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0x700},{r::STATE_B,0x700}});
		clocked_register_case c{{{0,std::move(initial)}},b ? 16U : 1U,40};
		c.pipeline_frontend = false;
		c.states.push_back({16,observation("paused-callee", {{STATE_GENPC,0x701},{r::STATE_SP,0xfff},{r::STATE_IDLE,1}},{{0xfff,0x601}})});
		c.states.push_back({17,observation("resumed-fast-return", {})});
		c.states.push_back({40,observation("rtn-not-stack-word", {{STATE_GENPC,0x602},{r::STATE_SP,0x1000}},{{0xfff,0xdead}})});
		c.actions = {{16,{{r::STATE_IDLE,0}},false,{}, {},{{0xfff,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{{true,0xfff,0x601,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
