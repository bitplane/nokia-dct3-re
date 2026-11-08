// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 address : {u16(0x60),u16(0x7f)}) for (bool b : {false,true})
	for (bool cpl : {false,true}) for (u32 base : {0U,0x20000U}) {
		u16 const value=b ? 0x5678 : 0x1234, st1=cpl ? 0x4800 : 0x800;
		auto initial=initial_case("STLM-direct-low-RAM-ignores-DP-and-CPL",{u16(0x8800|(b ? 0x100 : 0)|address),0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0x1234},{r::STATE_B,0x5678},
			{r::STATE_ST0,0x1ff},{r::STATE_ST1,st1},{r::STATE_SP,0x3000}});
		initial.data={{address,0xaaaa}};
		auto final=observation("one-direct-store-with-status-and-accumulators-retained",{{STATE_GENPC,base+0x602},
			{r::STATE_A,0x1234},{r::STATE_B,0x5678},{r::STATE_ST0,0x1ff},{r::STATE_ST1,st1},{r::STATE_SP,0x3000}},{{address,value}});
		clocked_register_case test{{{0,std::move(initial)},{30,std::move(final)}},3,30};
		test.data_events=std::vector<clocked_data_event>{{true,address,value,0,false}};
		cases.push_back(std::move(test));
	}
	return cases;
}
}
