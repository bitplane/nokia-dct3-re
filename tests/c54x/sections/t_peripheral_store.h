// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned trn=0;trn<2;++trn) for (unsigned post=0;post<2;++post)
	for (u16 destination : {0x3b,0x48,0x49}) for (u16 value : {0,1,0x8000,0xffff})
	for (unsigned gap=0;gap<3;++gap) {
		unsigned const index=cases.size(), write=8+gap;
		u32 const base=index%2 ? 0x20000 : 0;
		u16 const source=trn ? 0xf : 0xe;
		auto initial=initial_case("instruction-produced-T-TRN-peripheral-store",{u16(0x8800|source)});
		initial.program[1+gap].value=(trn ? 0x8d00 : 0x8c00)|(post ? 0x92 : 0x82);
		initial.program[2+gap].value=0xf4e1;
		for (auto &word : initial.program) word.address+=base;
		initial.data={{destination,0xaaaa}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,value},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_AR2,destination},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,0x800},{trn ? r::STATE_TRN : r::STATE_T,0xdead}});
		auto final=observation("single-publication-and-pointer-update",{{r::STATE_A,value},{r::STATE_B,0xfedcba9876ULL},
			{r::STATE_AR2,destination+post},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800},
			{int(trn ? r::STATE_TRN : r::STATE_T),value}});
		final.expected_data={{destination,value}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("producer-store-running",{})},{16,std::move(final)}},
			std::array<unsigned,3>{5,write-1,write}[index%3],16};
		c.data_events=std::vector<clocked_data_event>{{true,source,value,6},{true,destination,value,write}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
