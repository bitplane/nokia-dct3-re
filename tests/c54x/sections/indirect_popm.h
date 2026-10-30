// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 opcodes[]{0x8a93,0x8a9b,0x8a90,0x8a9b};
	for (unsigned index = 0; index < 4; ++index) {
		auto initial = initial_case("indirect-popm", {opcodes[index],0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_AR0,0xdead},
			{r::STATE_AR3,index == 1 ? 0x800fU : index == 3 ? 0x8017U : 0x8010U},
			{r::STATE_ST0,index == 2 ? 0x6000U : 0U},{r::STATE_ST1,index == 2 ? 0x20U : 0U}});
		initial.data = {{0x1000,0x1234}};
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("indirect-mmr-pop", {{STATE_GENPC,0x602},
			{r::STATE_SP,index == 3 ? 0x1234U : 0x1001U},{r::STATE_AR0,index == 3 ? 0xdeadU : 0x1234U},
			{r::STATE_AR3,index == 1 ? 0x10U : index == 3 ? 0x18U : 0x11U},
			{r::STATE_ST0,index == 2 ? 0x6000U : 0U}},{{0x1000,0x1234}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x1000,0x1234,0,false},
			{true,index == 3 ? 0x18U : 0x10U,0x1234,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
