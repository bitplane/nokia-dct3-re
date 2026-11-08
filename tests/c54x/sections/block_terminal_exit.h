// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned count : {0U,2U}) for (unsigned exit : (count ? std::vector<unsigned>{0,count,count+1} : std::vector<unsigned>{0,1}))
	for (bool b : {false,true}) for (u32 base : {0U,0x20000U}) {
		bool const taken=exit<=count;
		unsigned const reads=taken ? exit+1 : count+1;
		auto initial=initial_case("conditional-forward-exit-at-block-last-word",
			{0xf072,0x604,u16(b ? 0x1192 : 0x1092),u16(b ? 0xf84d : 0xf845),0x700,0x7693,0x1111,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.program.insert(initial.program.end(),{{base+0x700,0x7693},{base+0x701,0x2222},
			{base+0x702,0xf495},{base+0x703,0xf495},{base+0x704,0xf6bf},{base+0x705,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_BRC,count},
			{r::STATE_AR2,0x5000},{r::STATE_AR3,0x5100},{r::STATE_ST1,0x900}});
		for (unsigned index=0;index<=count;++index) initial.data.push_back({0x5000+index,u16(index==exit ? 0 : 1)});
		initial.data.push_back({0x5100,0xaaaa});
		auto final=observation("endpoint-decrements-before-exit-and-explicit-clear",
			{{STATE_GENPC,base+(taken ? 0x706U : 0x608U)},{r::STATE_BRC,taken && exit<count ? count-exit-1 : 0},
			 {r::STATE_RSA,0x602},{r::STATE_REA,0x604},{r::STATE_AR2,0x5000+reads},
			 {r::STATE_AR3,0x5101},{r::STATE_ST1,0x900},{r::STATE_IDLE,1}},{{0x5100,u16(taken ? 0x2222 : 0x1111)}});
		cases.push_back({{{0,std::move(initial)},{100,std::move(final)}},8,100});
	}
	return cases;
}
}
