// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned target=0;target<8;++target) {
		std::array<unsigned,3> const pointers{target,(target+1)%8,4};
		for (unsigned p=0;p<3;++p) {
			if ((p>0 && pointers[p]==pointers[0]) || (p>1 && pointers[p]==pointers[1])) continue;
			unsigned const pointer=pointers[p];
			for (u16 value : {0x1234U,0x4567U}) for (bool call : {false,true}) {
				u16 const opcode=0x7680|pointer, payload=0x2000+target;
				auto initial=initial_case("nonmodifying-constant-ST-AR-alias",{0xf495});
				if (call) {
					initial.program[0].value=0x7710|pointer; initial.program[1].value=0x10+target;
					initial.program[2].value=0xf274; initial.program[3].value=0x700;
					initial.program[4].value=opcode; initial.program[5].value=value;
					initial.program.insert(initial.program.end(),{{0x700,u16(0x1080|target)},{0x701,0xf4e1}});
				} else {
					initial.program[0].value=opcode; initial.program[1].value=value;
					initial.program[2].value=0x1080|target; initial.program[3].value=0xf4e1;
				}
				initial.data={{value,payload}};
				initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_B,0x123456789aULL},
					{r::STATE_SP,0x1000},{r::STATE_ST0,0x1404},{r::STATE_ST1,0x900}});
				auto final=observation("retained-destination-and-adjacent-address-consumer",{{STATE_GENPC,call ? 0x702U : 0x604U},
					{r::STATE_A,payload},{r::STATE_B,0x123456789aULL},{r::STATE_SP,call ? 0xfffU : 0x1000U},
					{r::STATE_ST0,0x1404},{r::STATE_ST1,0x900}});
				for (unsigned ar=0;ar<8;++ar) {
					u16 const before=ar==pointer ? 0x10+target : 0x5000+ar;
					initial.initial.push_back({int(r::STATE_AR0)+int(ar),call ? 0x5000+ar : before});
					final.expected.push_back({int(r::STATE_AR0)+int(ar),ar==target ? value : before});
				}
				clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},3+unsigned(cases.size()%4)*2,32};
				c.data_events=std::vector<clocked_data_event>{};
				if (call) {
					c.data_events->push_back({true,0x10+pointer,u16(0x10+target),0,false});
					c.data_events->push_back({true,0xfff,0x606,0,false});
				}
				c.data_events->push_back({true,0x10+target,value,0,false});
				c.data_events->push_back({false,value,payload,0,false});
				cases.push_back(std::move(c));
			}
		}
	}
	// Unsupported compositions must not acquire a DAGEN publication merely
	// because the nonmodifying standard AR-alias path was admitted.
	for (unsigned kind=0;kind<5;++kind) {
		u16 const target=kind<2 ? 0x18+kind : 0x15;
		auto initial=initial_case("unqualified-constant-ST-alias-remains-rejected",{0xf495});
		unsigned const at=kind==4 ? 1 : 0;
		if (kind==4) initial.program[0].value=0xec01;
		initial.program[at].value=kind==3 ? 0x7694 : 0x7684;
		initial.program[at+1].value=0x1234;
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR4,target},{r::STATE_AR5,0x5005},
			{r::STATE_SP,0x1000},{r::STATE_BK,7},{r::STATE_ST1,kind==2 ? 0x920U : 0x900U}});
		initial.expected.clear();
		auto final=observation("no-new-register-publication-on-excluded-form",{{r::STATE_ILLEGAL,1},
			{r::STATE_AR5,0x5005},{r::STATE_SP,0x1000},{r::STATE_BK,7}});
		final.expected.pop_back(); // This negative case replaces observation's default legal-state check.
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},3,24};
		c.require_idle=false;
		c.data_events=std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
