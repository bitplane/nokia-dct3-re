// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != 3; ++variant) {
		bool const irq = variant == 2, accepted = variant != 1;
		auto initial = initial_case("atomic-XC-two-word-span-and-interrupt-release",{0xf495,0x75f8,0x100,0x124,0xff4d,
			u16(irq ? 0x00f8 : 0xe801),u16(irq ? 0x120 : 0xe902),0x75f8,0x100,0x124,0xf4e1});
		initial.program.insert(initial.program.end(),{{0x48,0x0083},{0x49,0xf4eb}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,accepted ? 0U : 1U},
			{r::STATE_ST0,0},{r::STATE_ST1,0},{r::STATE_AR3,0x121},{r::STATE_SP,0x300},{r::STATE_IMR,irq ? 4U : 0U}});
		initial.data = {{0x100,0x1111},{0x120,1},{0x121,0},{0x2ff,0xbeef}};
		auto final = observation("XC-selected-slots-stack-and-final-output-continuation",{{STATE_GENPC,0x60b},
			{r::STATE_A,accepted ? 1U : 0U},{r::STATE_B,irq ? 0U : accepted ? 2U : 1U},{r::STATE_SP,0x300}});
		final.expected_data = initial.data;
		if (irq) final.expected_data.back().value = 0x607;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-XC-markers",{{STATE_GENPC,0x601}})},
			{2,observation("XC-markers-running",{})},{96,std::move(final)}},3,96};
		c.pipeline_frontend = false;
		if (irq) {
			c.data_events = std::vector<clocked_data_event>{{false,0x100,0x1111,0,false},{false,0x120,1,0,false,2},
				{true,0x2ff,0x607,0,false},{false,0x121,0,0,false},{false,0x2ff,0x607,0,false},{false,0x100,0x1111,0,false}};
			// RTN at the second PORTW proves acceptance after the two-word ADD, before that output.
			c.irqs = {{2,0,~0U,2,0x607,{{r::STATE_A,1},{r::STATE_B,0}},1}};
			c.io_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false}};
		} else {
			c.observed_data_space = AS_IO;
			c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},6}};
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
