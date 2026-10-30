// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	auto initial = initial_case("CALL-publishes-restorable-RTN", {0xf495});
	initial.program = {{0x2067f,0xf495},{0x20680,0xf074},{0x20681,0x700},{0x20682,0xf4e1},
		{0x20700,0xf4e1},{0x20701,0xf495},{0x30780,0xf074},{0x30781,0x700},{0x30700,0xf4e1}};
	initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x2067f},{r::STATE_SP,0x400}});
	initial.data = {{0x3ff,0xbeef}};
	clocked_register_case c{{{0,std::move(initial)},
		{12,observation("callee-paused-with-CALL-frame", {{STATE_GENPC,0x20701},{r::STATE_SP,0x3ff},{r::STATE_RTN,0x682}},{{0x3ff,0x682}})},
		{13,observation("fast-return-running", {})},
		{20,observation("RETF-uses-RTN-not-stack-sentinel", {{STATE_GENPC,0x20683},{r::STATE_SP,0x400},{r::STATE_RTN,0x682}},{{0x3ff,0xdead}})},
		{21,observation("second-real-CALL-running", {})},
		{48,observation("second-CALL-overwrites-saved-RTN", {{STATE_GENPC,0x30701},{r::STATE_SP,0x3ff},{r::STATE_RTN,0x782}},{{0x3ff,0x782}})}},12,48};
	// Snapshot after the first CALL, with the stack poisoned and RETF ready.
	// Execute a second real CALL before restore so replay must recover the old latch.
	c.actions = {{12,{{r::STATE_IDLE,0}},false,{{0x20701,0xf49b}},{},{{0x3ff,0xdead}}},
		{20,{{STATE_GENPC,0x30780},{r::STATE_IDLE,0}}}};
	c.pipeline_frontend = false;
	c.data_events = std::vector<clocked_data_event>{{true,0x3ff,0x682,0,false},{true,0x3ff,0x782,0,false}};
	return {std::move(c)};
}
}
