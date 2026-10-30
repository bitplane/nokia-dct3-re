// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned address=0x20;address<0x60;++address) for (unsigned form=0;form<3;++form)
	for (unsigned pattern=0;pattern<4;++pattern) for (unsigned b=0;b<2;++b) {
		unsigned const index=cases.size(), read=form==1 ? 7 : 6;
		u32 const base=index%2 ? 0x20000 : 0;
		u16 const value=std::array<u16,4>{0,0xffff,0x8000,0x1234}[pattern];
		auto initial=initial_case("unsigned-synthetic-peripheral-endpoint",{u16(0x1200|(b<<8)|(form==1 ? 0xf8 : form==2 ? 0x81 : address))});
		initial.program[1].value=form==1 ? address : 0xf4e1;
		if (form==1) initial.program[2].value=0xf4e1;
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x1600},{r::STATE_ST1,0x900},
			{r::STATE_IMR,0},{r::STATE_AR1,address},{r::STATE_A,0xabcdef1234ULL},{r::STATE_B,0x123456789aULL}});
		auto final=observation("one-zero-extended-word-without-status-change",{{r::STATE_A,b ? 0xabcdef1234ULL : value},
			{r::STATE_B,b ? value : 0x123456789aULL},{r::STATE_ST0,0x1600},{r::STATE_ST1,0x900},
			{r::STATE_AR1,form==2 ? 0x2222U : address}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("peripheral-load-running",{})},{18,std::move(final)}},
			std::array<unsigned,3>{4,read-1,read}[index%3],18};
		c.synthetic_data_reads={{address,value}};
		c.data_events=std::vector<clocked_data_event>{{false,address,value,read}};
		if (form==1) {
			c.actions={{4,{},false,{{base+0x601,0xdead}}}};
			c.retained_program_reads={{base+0x601,u16(address)}};
		}
		if (form==2) c.actions={{read-1,{{r::STATE_AR1,0x2222}}}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
