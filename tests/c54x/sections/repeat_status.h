// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_INSTRUCTION_EVENTS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	auto initial = initial_case("repeat-status", {0x771a,1,0xf072,0x605,0xf495,0x4907,0xf4e1});
	initial.initial.push_back({r::STATE_ST1,0x4800});
	clocked_register_case c{{{0,std::move(initial)}},3,32};
	c.pipeline_frontend = true;
	// BRAF clears at Decode of the final iteration's first instruction, not
	// after executing its endpoint (SPRU131G 7.5.8.2).
	c.states.push_back({32,observation("independent-braf-cpl", {{r::STATE_B,0x4800},{r::STATE_ST1,0x4800},
		{r::STATE_BRC,0},{STATE_GENPC,0x607}})});
	c.instruction_events = std::vector<memory_word>{{0x600,0x771a},{0x602,0xf072},
		{0x604,0xf495},{0x605,0x4907},{0x604,0xf495},{0x605,0x4907},{0x606,0xf4e1}};
	return {std::move(c)};
}
}
