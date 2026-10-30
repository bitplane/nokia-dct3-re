// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 3; ++variant) {
		std::vector<u16> const words = variant == 0 ? std::vector<u16>{0x0082,0xf273,0x700,0x0082,0x0082,0xffff} :
			variant == 1 ? std::vector<u16>{0x0082,0xf274,0x700,0xf495,0xf495,0x0082,0xf4e1} :
			std::vector<u16>{0x0082,0xf020,0x1234,0x0082,0xf4e1};
		auto initial = initial_case("atomic-operand-clock-interval-and-delay-IRQ",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1 + i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_AR2,0x100},{r::STATE_AR3,0x101},
			{r::STATE_SP,0x300},{r::STATE_IMR,variant == 0 ? 4U : 0U},{r::STATE_ST0,0},{r::STATE_ST1,0}});
		initial.data = {{0x100,1},{0x101,0},{0x2ff,0xbeef}};
		if (variant == 0) initial.program.insert(initial.program.end(),{{0x700,0xf4e1},{0x48,0x0083},{0x49,0xf4eb}});
		if (variant == 1) initial.program.insert(initial.program.end(),{{0x700,0x0082},{0x701,0xfe00},{0x702,0xf495},{0x703,0xf495}});
		auto final = observation("exact-operand-count-balanced-stack-and-continuation",{{STATE_GENPC,variant == 0 ? 0x701U : variant == 1 ? 0x608U : 0x606U},
			{r::STATE_A,variant == 2 ? 0x1235U : 3U},{r::STATE_SP,0x300},{r::STATE_AR2,0x100},{r::STATE_AR3,0x101}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-timing-program-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("timing-program-running",{})},{96,std::move(final)}},1,96};
		c.pipeline_frontend = false;
		if (variant == 1) c.checkpoint = 5; // Replay with the first operand timestamp already recorded.
		if (variant == 0) {
			c.data_events = std::vector<clocked_data_event>{{false,0x100,1,0,false},{false,0x100,1,0,false,2,3},
				{false,0x100,1,0,false,{},1},{true,0x2ff,0x700,0,false},{false,0x101,0,0,false},{false,0x2ff,0x700,0,false}};
			c.irqs = {{2,0,~0U,3,0x700,{{r::STATE_A,3}}}};
		} else if (variant == 1) c.data_events = std::vector<clocked_data_event>{{false,0x100,1,0,false},
			{true,0x2ff,0x606,0,false},{false,0x100,1,0,false},{false,0x2ff,0x606,0,false},{false,0x100,1,0,false}};
		else c.data_events = std::vector<clocked_data_event>{{false,0x100,1,0,false},{false,0x100,1,0,false,{},3}};
		if (variant == 1) c.data_events->back().cycles_from_first = 11;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
