// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 addresses[]{0,1,0x7f,0x100,0x7fff,0x8000,0xfffe,0xffff};
	constexpr s16 steps[]{0,1,3,-1,-3,0x7fff,-0x8000,-0x7fff};
	constexpr u16 opcodes[]{0x6dc2,0x6dd2,0x6dca,0x6dda,0xa4c1};
	for (unsigned form = 0; form < 5; ++form)
	for (unsigned position = 0; position < 8; ++position)
	for (unsigned stride = 0; stride < (form < 2 ? 1U : 8U); ++stride)
	{
		int const step = form == 0 ? -1 : form == 1 ? 1 : form == 2 ? -int(steps[stride]) : int(steps[stride]);
		auto initial = initial_case("zero-bk", {opcodes[form],0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_AR0,u16(steps[stride])},{r::STATE_AR2,addresses[position]},
			{r::STATE_AR3,0xf00},{r::STATE_BK,0},{r::STATE_ST0,0}});
		u16 const sample = addresses[position] == 0 ? 0 : 0x1234;
		if (form == 4) {
			initial.initial.push_back({r::STATE_IFR,0x1234});
			initial.data = {{0xf00,2}};
			if (addresses[position] >= 0x60) initial.data.push_back({addresses[position],sample});
		}
		clocked_register_case c{{{0,std::move(initial)}},(form + position + stride) & 1 ? 3U : 1U,24};
		c.pipeline_frontend = true;
		c.states.push_back({24,observation("linear-modulo-address", {
			{r::STATE_AR2,u16(int(addresses[position]) + step)},{r::STATE_AR3,0xf00}})});
		if (form == 4) {
			c.states.back().observation.expected.insert(c.states.back().observation.expected.end(),
				{{r::STATE_A,2U * sample},{r::STATE_T,sample},{r::STATE_IMR,0},{r::STATE_IFR,0x1234}});
			c.data_events = std::vector<clocked_data_event>{{false,addresses[position],sample,5},{false,0xf00,2,5}};
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
