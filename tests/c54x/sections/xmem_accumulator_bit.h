// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned address=8;address<=13;++address) for (unsigned bit : {0U,8U,15U})
	for (u16 pattern : {0U,1U,0x80U,0x8001U}) {
		unsigned const pointer=address==8 ? 5 : 2+(address+bit)%4;
		unsigned const target=address>=11, shift=((address-8)%3)*16;
		u64 const mask=shift==32 ? 0xff : 0xffff;
		std::array<u64,2> before{0x123456789aULL,0xffabcdef01ULL};
		before[target]=(before[target]&~(mask<<shift))|((u64(pattern)&mask)<<shift);
		u16 const word=shift==32 ? pattern&0xff : pattern; // AG/BG expose eight bits, not a signed 16-bit field.
		u16 const status=0xa604|((pattern&1) ? 0x1000 : 0);
		u16 const expected=(status&~0x1000)|((word&(1U<<(15-bit))) ? 0x1000 : 0);
		u16 const opcode=0x9600|((pointer-2)<<4)|bit;
		auto initial=initial_case("Xmem-BIT-accumulator-field-Read-ownership",{opcode,0xf495,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,before[0]},{r::STATE_B,before[1]},
			{r::STATE_ST0,status},{r::STATE_ST1,0x900},{int(r::STATE_AR0)+int(pointer),address}});
		auto final=observation("retained-bit-despite-later-accumulator-and-pointer",{{STATE_GENPC,0x603},
			{r::STATE_A,0x0102030405ULL},{r::STATE_B,0x8070605040ULL},
			{r::STATE_ST0,expected},{r::STATE_ST1,0x900},{int(r::STATE_AR0)+int(pointer),0x4000}});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},3+unsigned(cases.size()%4),16};
		c.actions={{5,{{r::STATE_A,0x0102030405ULL},{r::STATE_B,0x8070605040ULL},{int(r::STATE_AR0)+int(pointer),0x4000}}}};
		c.data_events=std::vector<clocked_data_event>{{false,address,word,5}};
		cases.push_back(std::move(c));
	}
	for (unsigned kind=0;kind<6;++kind) {
		u16 const address=kind==3 ? 0x18 : kind==4 ? 0x10 : 8;
		auto initial=initial_case("excluded-Xmem-mapped-read",{0xf495});
		unsigned const at=kind==5 ? 1 : 0;
		if (kind==5) initial.program[0].value=0xec01;
		initial.program[at].value=kind==0 ? 0x9410 : kind==1 ? 0x9010 : kind==2 ? 0x9650 : 0x9610;
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR3,address},{r::STATE_A,0x1234},{r::STATE_ST0,0x604}});
		initial.expected.clear();
		auto final=observation("excluded-source-rejects-without-read-or-update",{{r::STATE_ILLEGAL,1},
			{r::STATE_AR3,address},{r::STATE_A,0x1234},{r::STATE_ST0,0x604}});
		final.expected.pop_back();
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},3,16};
		c.require_idle=false;c.data_events=std::vector<clocked_data_event>{};cases.push_back(std::move(c));
	}
	return cases;
}
}
