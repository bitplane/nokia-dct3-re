// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form=0;form<3;++form) for (u16 destination : {0x3b,0x48,0x49})
	for (u16 source : {0x10,0x19,0x1400}) for (u16 value : {0,1,0x8000,0xffff}) {
		unsigned const index=cases.size(), read=form==2 ? 7 : 6;
		u32 const base=index%2 ? 0x20000 : 0;
		auto initial=initial_case("MVKD-captures-source-before-peripheral-wait",{u16(form==2 ? 0x70f8 : form ? 0x7092 : 0x7082)});
		initial.program[1].value=form==2 ? destination : source;
		if (form==2) initial.program[2].value=source;
		initial.program[form==2 ? 3 : 2].value=0xf4e1;
		for (auto &word : initial.program) word.address+=base;
		initial.data={{source,value},{destination,0xaaaa}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_AR2,destination},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}});
		auto final=observation("retained-word-single-write-and-pointer-update",{{r::STATE_A,0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_AR2,destination+unsigned(form==1)},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}});
		final.expected_data={{source,0xdead},{destination,value}};
		clocked_register_case c{{{0,std::move(initial)},{1,observation("MVKD-running",{})},{14,std::move(final)}},
			std::array<unsigned,3>{read-1,read,read+2}[index%3],14};
		c.data_events=std::vector<clocked_data_event>{{false,source,value,read},{true,destination,value,read+2}};
		c.retained_program_reads={{base+0x601,u16(form==2 ? destination : source)}};
		if (form==2) c.retained_program_reads.push_back({base+0x602,source});
		clocked_register_action poison{read,{},false,{{base+0x601,0xbeef}},{},{{source,0xdead}}};
		if (form==2) poison.program.push_back({base+0x602,0xbeef});
		c.actions.push_back(std::move(poison));
		cases.push_back(std::move(c));
	}
	return cases;
}
}
