// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	auto initial = initial_case("repeat-status", {0x771a,1,0xf072,0x605,0xf495,0x4907,0xf4e1});
	initial.initial.push_back({r::STATE_ST1,0x4800});
	clocked_register_case c{{{0,std::move(initial)}},3,32};
	c.pipeline_frontend = false;
	c.states.push_back({32,observation("independent-braf-cpl", {{r::STATE_B,0xc800},{r::STATE_ST1,0x4800}})});
	return {std::move(c)};
}
}
