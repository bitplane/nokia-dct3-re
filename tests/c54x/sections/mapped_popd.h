// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned target=0;target<21;++target)for(unsigned form=0;form<3;++form)
	for(unsigned pattern=0;pattern<4;++pattern)for(bool compatibility : {false,true}){
		unsigned const seed=target+form+pattern+compatibility;u32 const base=seed&1 ? 0x20000 : 0;
		u16 const destination=target<16 ? 8+target : 0x19+target-16,pointer=form ? destination : 0x7777;
		u16 const mode=form==0 ? destination : form==1 ? 0x97 : 0x8f,value=std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		u16 const st1=0x900|(compatibility ? 0x20 : 0),st0=compatibility && form ? 0xe600 : 0xa600;
		unsigned const publication=5+unsigned(destination<0x10 || destination>0x19);
		auto initial=initial_case("mapped-POPD-captures-destination-before-pointer-poison",{u16(0x8b00|mode),0xf4e1});
		for(auto &word : initial.program)word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa600},{r::STATE_ST1,st1},{r::STATE_SP,0x3000},{r::STATE_BK,0},
			{r::STATE_A,0x0222012200ULL},{r::STATE_B,0x0522042203ULL},{r::STATE_T,0x2206},{r::STATE_TRN,0x2207},{r::STATE_BRC,0x2211},{r::STATE_RSA,0x2212},{r::STATE_REA,0x2213}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,i==7 ? pointer : 0x2208+i});
		initial.data.push_back({0x3000,value});
		auto access=observation("Access-updates-pointer-without-consuming-SP",{{r::STATE_SP,0x3000},{r::STATE_ST0,st0},{r::STATE_AR7,form ? u16(pointer+(form==1 ? 1 : -1)) : pointer}});
		auto final=observation("exact-register-destination-and-unselected-fields",{{STATE_GENPC,base+0x602},{r::STATE_SP,0x3001},{r::STATE_ST0,st0},{r::STATE_ST1,st1}},{{0x3000,0xdead}});
		for(unsigned i=0;i<21;++i){u16 const address=i<16 ? 8+i : 0x19+i-16;u16 expected=i==2 ? 2 : i==5 ? 5 : address==0x19 || address==0x1d ? 0 : address==0x17 ? (form ? 0x2222 : pointer) : 0x2200+i;
			if(address==destination)expected=value&(address==0xa || address==0xd ? 0xff : 0xffff);
			final.expected_data.push_back({address,expected});}
		auto read=observation("Read-consumes-stack-exactly-once",{{r::STATE_SP,0x3001},{r::STATE_ST0,st0}});
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(access)},{5,std::move(read)},{16,std::move(final)}},3+seed%4,16};
		c.data_events=std::vector<clocked_data_event>{{false,0x3000,value,5,true},{true,destination,value,publication,true}};
		c.program_events={{false,base+0x600,u16(0x8b00|mode),0,false}};
		if(form)c.actions.push_back({4,{{r::STATE_AR7,0x2222}}});
		c.actions.push_back({5,{},false,{},{},{{0x3000,0xdead}}});cases.push_back(std::move(c));
	}
	return cases;
}
}
