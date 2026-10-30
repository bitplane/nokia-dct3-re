// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Public R060 02:3dcb sequence: CMPR samples before the younger store's
	// pointer update, while the three writes retain their separate values.
	for (unsigned point : {6U,7U,8U})
	{
		u16 const old = point & 1 ? 0xb608 : 0xa608;
		auto initial = initial_case("store-compare-overlap", {0x8393,0x8193,0xf4ab,0x8c93});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,old},{r::STATE_ST1,0x800},
			{r::STATE_AR0,0x471},{r::STATE_AR3,0x470},{r::STATE_A,0xbeef},{r::STATE_B,0x2233445566ULL},{r::STATE_T,6}});
		clocked_register_case c{{{0,std::move(initial)},
			{5,observation("first-pointer", {{r::STATE_AR3,0x471},{r::STATE_ST0,old}})},
			{6,observation("compare-access", {{r::STATE_AR3,0x472},{r::STATE_ST0,old}})},
			{8,observation("compare-execute", {{r::STATE_AR3,0x473},{r::STATE_ST0,0xb608},{r::STATE_ST1,0x800}})}},point,12};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{{true,0x470,0x3344,6},
			{true,0x471,0x5566,7},{true,0x472,6,9}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
