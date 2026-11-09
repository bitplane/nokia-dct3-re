// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// A/B equality and TC predicates; the second delay word is REA.
	for (unsigned count : {0U,2U}) for (unsigned exit : (count ? std::vector<unsigned>{0,count,count+1} : std::vector<unsigned>{0,1}))
	for (unsigned predicate : {0U,1U,2U}) for (u32 base : {0U,0x20000U}) {
		bool const taken=exit<=count, tc=predicate==2;
		unsigned const reads=taken ? exit+1 : count+1;
		u16 const load=std::array<u16,3>{0x1092,0x1192,0x9680}[predicate];
		u16 const branch=std::array<u16,3>{0xfa45,0xfa4d,0xfa30}[predicate];
		auto initial=initial_case("delayed-conditional-forward-exit-ending-at-block-last-word",
			{0xf072,0x606,load,branch,0x700,0x6d94,0x6d95,0x7693,0x1111,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.program.insert(initial.program.end(),{{base+0x700,0x7693},{base+0x701,0x2222},
			{base+0x702,0xf495},{base+0x703,0xf495},{base+0x704,0xf6bf},{base+0x705,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_BRC,count},
			{r::STATE_AR2,0x5000},{r::STATE_AR3,0x5100},{r::STATE_AR4,0x6000},{r::STATE_AR5,0x6100},{r::STATE_ST1,0x900}});
		for (unsigned index=0;index<=count;++index)
			initial.data.push_back({0x5000+index,u16(tc ? (index==exit ? 0x8000 : 0) : (index==exit ? 0 : 1))});
		initial.data.push_back({0x5100,0xaaaa});
		auto final=observation("delay-words-once-endpoint-decrements-before-exit",
			{{STATE_GENPC,base+(taken ? 0x706U : 0x60aU)},{r::STATE_BRC,taken && exit<count ? count-exit-1 : 0},
			 {r::STATE_RSA,0x602},{r::STATE_REA,0x606},{r::STATE_AR2,0x5000+reads},
			 {r::STATE_AR3,0x5101},{r::STATE_AR4,0x6000+reads},{r::STATE_AR5,0x6100+reads},
			 {r::STATE_ST1,0x900},{r::STATE_IDLE,1}},{{0x5100,u16(taken ? 0x2222 : 0x1111)}});
		cases.push_back({{{0,std::move(initial)},{120,std::move(final)}},8,120});
	}
	return cases;
}
}
