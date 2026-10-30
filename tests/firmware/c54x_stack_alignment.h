// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 start : {0x1200U,0xfffeU,0U,0x8000U})
	for (u16 delta : {0x3ffU,1U,0xffffU,0x8000U}) for (unsigned sxm = 0; sxm < 2; ++sxm)
	{
		u16 const sum = start + delta, aligned = sum & 0xfffe;
		auto initial = initial_case("stack-alignment-sequence", {0x7718,start,0x6bf8,0x18,delta,0x68f8,0x18,0xfffe});
		initial.program[11].value = 0x4918;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa608},{r::STATE_ST1,sxm ? 0x900 : 0x800},
			{r::STATE_SP,0x4000},{r::STATE_B,0}});
		initial.expected.push_back({r::STATE_SP,0x4000});
		clocked_register_case c{{{0,std::move(initial)},
			{5,observation("initial-sp", {{r::STATE_SP,start}})},
			{10,observation("wrapped-sum", {{r::STATE_SP,sum}})},
			{13,observation("aligned-sp", {{r::STATE_SP,aligned}})},
			{17,observation("mapped-load", {{r::STATE_SP,aligned},{r::STATE_B,aligned},
				{r::STATE_ST1,sxm ? 0x900U : 0x800U}})}},
			std::array<unsigned,4>{7,9,10,12}[(start + delta + sxm) % 4],22};
		c.require_idle = false;
		c.data_events = std::vector<clocked_data_event>{{true,0x18,start,5},{false,0x18,start,9},
			{true,0x18,sum,10},{false,0x18,sum,12},{true,0x18,aligned,13},{false,0x18,aligned,16}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
