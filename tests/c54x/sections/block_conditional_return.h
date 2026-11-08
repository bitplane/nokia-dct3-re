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
		auto initial=initial_case("conditional-return-before-block-endpoint",
			{0xf074,0x700,0x7693,0x1111,0xf495,0xf495,0xf6bf,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.program.insert(initial.program.end(),{{base+0x700,0xf072},{base+0x701,0x705},
			{base+0x702,u16(b ? 0x1192 : 0x1092)},{base+0x703,u16(b ? 0xfc4d : 0xfc45)},
			{base+0x704,0xf495},{base+0x705,0xf495},{base+0x706,0xfc00}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_BRC,count},
			{r::STATE_AR2,0x5000},{r::STATE_AR3,0x5100},{r::STATE_SP,0x3000},{r::STATE_ST1,0x900}});
		for (unsigned index=0;index<=count;++index) initial.data.push_back({0x5000+index,u16(index==exit ? 0 : 1)});
		initial.data.push_back({0x5100,0xaaaa});
		auto final=observation("return-selects-caller-with-balanced-stack-and-retained-controller",
			{{STATE_GENPC,base+0x608},{r::STATE_BRC,taken ? count-exit : 0},
			 {r::STATE_RSA,0x702},{r::STATE_REA,0x705},{r::STATE_AR2,0x5000+(taken ? exit+1 : count+1)},
			 {r::STATE_AR3,0x5101},{r::STATE_SP,0x3000},{r::STATE_ST1,0x900},{r::STATE_IDLE,1}},{{0x5100,0x1111}});
		cases.push_back({{{0,std::move(initial)},{100,std::move(final)}},8,100});
	}
	return cases;
}
}
