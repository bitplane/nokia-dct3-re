// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned ar=0;ar<8;++ar) for (u16 value : {0,1,0x8000,0xffff}) {
		auto initial=initial_case("mapped-BRC-copy-retains-Read-operand",{u16(0x7210|ar),0x001a,0xf495,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_BRC,0x1234},{r::STATE_RSA,0x4567},{r::STATE_REA,0x6789},
			{r::STATE_RC,0x3456},{r::STATE_ST0,0x1000},{r::STATE_ST1,0x800},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x23456789abULL}});
		for (unsigned i=0;i<8;++i) initial.initial.push_back({r::STATE_AR0+i,0x2000+i});
		auto final=observation("copy-does-not-change-loop-control",{{STATE_GENPC,0x604},
			{r::STATE_BRC,0xdead},{r::STATE_RSA,0x4567},{r::STATE_REA,0x6789},{r::STATE_RC,0x3456},
			{r::STATE_ST0,0x1000},{r::STATE_ST1,0x800},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x23456789abULL}});
		for (unsigned i=0;i<8;++i) final.expected.push_back({int(r::STATE_AR0+i),i==ar ? value : 0x2000+i});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},std::array<unsigned,4>{4,5,6,8}[cases.size()%4],16};
		c.actions={{5,{{r::STATE_BRC,value}}},{6,{{r::STATE_BRC,0xdead}}}};
		c.data_events=std::vector<clocked_data_event>{{false,0x1a,value,6},{true,0x10+ar,value,6}};
		cases.push_back(std::move(c));
	}
	for (unsigned kind=0;kind<5;++kind) {
		u16 const source=kind<3 ? 0x1b+kind : 0x1a;
		auto initial=initial_case("unqualified-mapped-copy-remains-rejected",{0xf495});
		unsigned const at=kind==3 ? 1 : 0;
		if (kind==3) initial.program[0].value=0xec01;
		initial.program[at].value=kind==4 ? 0x720e : 0x7212;
		initial.program[at+1].value=source;
		initial.initial.insert(initial.initial.end(),{{r::STATE_BRC,0x1234},{r::STATE_AR2,0x4321},{r::STATE_T,0x5678}});
		initial.expected.clear();
		auto final=observation("no-mapped-read-or-register-publication",{{r::STATE_ILLEGAL,1},
			{r::STATE_BRC,0x1234},{r::STATE_AR2,0x4321},{r::STATE_T,0x5678}});
		final.expected.pop_back();
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},3,16};
		c.require_idle=false;
		c.data_events=std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
