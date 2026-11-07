// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u64,3> sources{0x123456789aULL,0xffabcdef01ULL,0x807fff0001ULL};
	for (unsigned target=0;target<8;++target) for (bool b : {false,true})
	for (bool high : {false,true}) for (unsigned shift : {0U,1U,15U}) {
		unsigned const pointer=target>=2 && target<=5 ? target : b ? 3 : 2+(target&3);
		u16 const address=0x10+target;
		u64 const source=sources[(target+shift+b+high)%sources.size()];
		u64 const shifted=(source<<shift)&0xffffffffffULL;
		u16 const word=shifted>>(high ? 16 : 0);
		u16 const opcode=0x9800|(unsigned(b)<<8)|(unsigned(high)<<9)|((pointer-2)<<4)|shift;
		auto initial=initial_case("nonmodifying-Xmem-store-to-AR",{opcode,0xf495,u16(0x4800|address),0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x1111},{r::STATE_B,0x2222},
			{r::STATE_ST0,0x1604},{r::STATE_ST1,0x800}});
		auto final=observation("retained-mapped-address-and-one-gap-consumer",{{STATE_GENPC,0x604},
			{r::STATE_A,word},{r::STATE_B,b ? source : 0x2222},{r::STATE_ST0,0x1604},{r::STATE_ST1,0x800}});
		for (unsigned ar=0;ar<8;++ar) {
			initial.initial.push_back({int(r::STATE_AR0)+int(ar),ar==pointer ? address : 0x5000+ar});
			final.expected.push_back({int(r::STATE_AR0)+int(ar),ar==target ? word : ar==pointer ? 0x4000 : 0x5000+ar});
		}
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},3+unsigned(cases.size()%4),16};
		// Poison the pointer after Read and provide the accumulator only then:
		// the write must keep its address while consuming the Execute source.
		c.actions={{5,{{int(r::STATE_AR0)+int(pointer),0x4000},{b ? r::STATE_B : r::STATE_A,source}}}};
		c.data_events=std::vector<clocked_data_event>{{true,address,word,6},{false,address,word,7}};
		cases.push_back(std::move(c));
	}
	for (unsigned kind=0;kind<5;++kind) {
		u16 const address=std::array<u16,5>{0x18,0x19,0x09,0x15,0x15}[kind];
		auto initial=initial_case("excluded-Xmem-mapped-store",{0xf495});
		unsigned const at=kind==4 ? 1 : 0;
		if (kind==4) initial.program[0].value=0xec01;
		initial.program[at].value=kind==3 ? 0x9850 : 0x9810;
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR3,address},{r::STATE_AR5,0x5005},
			{r::STATE_A,0x1234},{r::STATE_BK,7}});
		initial.expected.clear();
		auto final=observation("no-publication-for-unqualified-Xmem-endpoint",{{r::STATE_ILLEGAL,1},
			{r::STATE_AR3,address},{r::STATE_AR5,0x5005},{r::STATE_A,0x1234},{r::STATE_SP,0x1000},{r::STATE_BK,7}});
		final.expected.pop_back();
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},3,16};
		c.require_idle=false;
		c.data_events=std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
