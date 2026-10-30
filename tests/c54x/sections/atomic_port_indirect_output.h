// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 opcodes[]{0x7582,0x7583,0x7595,0x7592,0x758a};
	constexpr u16 values[]{0xface,0xbeef,0xcafe,0xface,0xface};
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant != std::size(opcodes); ++variant) {
		auto initial = initial_case("atomic-indirect-port-source-and-modification",{0xf495,0x75f8,0x100,0x124,
			opcodes[variant],0x124,0x75f8,0x100,0x124,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR2,0x120},{r::STATE_AR3,0x122},{r::STATE_AR5,0x124},
			{r::STATE_ST0,0x1800},{r::STATE_ST1,0x100},{r::STATE_A,0x12345678},{r::STATE_B,0x76543210}});
		initial.data = {{0x100,0x1111},{0x120,0xface},{0x122,0xbeef},{0x124,0xcafe}};
		auto final = observation("old-source-output-and-one-selected-pointer-update",{{STATE_GENPC,0x60a},
			{r::STATE_AR2,variant == 3 ? 0x121U : variant == 4 ? 0x11fU : 0x120U},{r::STATE_AR3,0x122},
			{r::STATE_AR5,variant == 2 ? 0x125U : 0x124U},{r::STATE_A,0x12345678},{r::STATE_B,0x76543210},
			{r::STATE_ST0,0x1800},{r::STATE_ST1,0x100}});
		final.expected_data = initial.data;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-indirect-port-markers",{{STATE_GENPC,0x601}})},
			{2,observation("indirect-port-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,values[variant],0,false},
			{true,0x124,0x1111,0,false,{},{},5}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
