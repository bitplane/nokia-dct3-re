// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> staged_conditional_store_cases(unsigned kind)
{
	std::vector<clocked_register_case> cases;bool const mapped=kind==1,edge=kind==2;
	unsigned const total=mapped ? 1024 : edge ? 256 : 1152;
	for(unsigned index=0;index<total;++index){
		unsigned v=index,form=v%4;v/=4;unsigned condition,pattern,mode,target=0;int shift=0;
		if(mapped){target=v%8;v/=8;mode=v%16;v/=16;pattern=v%2;condition=3;shift=pattern ? -1 : 0;}
		else if(edge){mode=v%16;v/=16;target=v%4;condition=mode%12;pattern=0;}
		else{condition=v%12;v/=12;pattern=v%6;v/=6;shift=std::array<int,4>{-16,-1,0,15}[v%4];mode=(form+condition+pattern+v)%16;}
		unsigned const seed=index+form+pattern,flags=edge ? 0 : mapped ? pattern%4 : seed%4;u32 const base=seed&1 ? 0x20000 : 0;
		u16 const address=mapped ? 0x10+target : edge ? std::array<u16,4>{8,0x1a,0x28,0x5f}[target] : 0x160f+0x100*(mode&3),bk=!mapped && (pattern&1) ? 16 : 0;
		constexpr std::array<u64,6> values{0,1,0xffffffffffULL,0x7fffffff,0xff80000000ULL,0x0100000000ULL};
		u64 a=values[pattern],b=values[(pattern+3)%6];if(condition>=6)std::swap(a,b);
		u16 const t=0x1234^(pattern*0x2222),brc=0xabcd^(pattern*0x1111),st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0)|(shift&31),pmst=flags&2 ? 1 : 0;
		u16 memory=0xa55a^(pattern*0x1111);unsigned const modification=mode>>2,pointer=2+(mode&3);s16 const step=mapped && !target ? s16(memory) : 3;
		u16 const updated=modification==1 ? address-1 : modification==2 ? address+1 : modification==3 ? bk ? (address&0xfff0)|((address+3)&15) : address+step : address;
		if(mapped && target==pointer)memory=updated;
		u64 const tested=condition>=6 ? b : a;bool const negative=tested&0x8000000000ULL,zero=!tested,take=std::array<bool,6>{!negative,negative,!zero,zero,!negative && !zero,negative || zero}[condition%6];
		u16 output=memory;if(take){if(form<2)output=form ? brc : t;else{u64 const source=form==3 ? b : a;s64 const signed_source=source&0x8000000000ULL ? s64(source)-0x10000000000LL : s64(source);u64 const shifted=shift<0 ? u64(st1&0x100 ? signed_source>>-shift : s64(source>>-shift))&0xffffffffffULL : (source<<shift)&0xffffffffffULL;output=shifted>>16;}}
		u16 const opcode=0x9c00|(form<<8)|(mode<<4)|(condition<6 ? condition+2 : condition+4);
		auto initial=initial_case(edge ? "conditional-store-rejects-unowned-mapped-address" : mapped ? "conditional-store-mapped-pointer-target-alias" : "conditional-store-Access-Read-Execute-operand-ages",{opcode,0xf4e1});
		for(auto &word : initial.program)word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa630},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{r::STATE_A,a^0x1111111111ULL},{r::STATE_B,b^0x1111111111ULL},
			{r::STATE_T,t^0xffffU},{r::STATE_BRC,brc^0xffffU},{r::STATE_SP,0x3000},{r::STATE_AR0,3},{r::STATE_BK,bk}});
		for(unsigned i=1;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,0x2200+i});
		if(mapped)initial.initial.push_back({r::STATE_AR0+target,memory});
		else if(!edge)initial.data.push_back({address,memory});
		auto access=observation("Access-captures-address-and-updates-once",{{int(r::STATE_AR0+pointer),edge ? address : updated}});
		auto final=observation("Execute-live-predicate-source-with-Read-retained-shift",{{r::STATE_ST0,0xa630},{r::STATE_ST1,edge ? st1 : st1^31U},{r::STATE_PMST,pmst},{r::STATE_A,edge ? a^0x1111111111ULL : a},{r::STATE_B,edge ? b^0x1111111111ULL : b},
			{r::STATE_T,edge ? t^0xffffU : t},{r::STATE_BRC,edge ? brc^0xffffU : brc},{r::STATE_BK,bk}});
		for(unsigned i=0;i<8;++i){u16 expected=i ? 0x2200+i : 3;if(edge && i==pointer)expected=address;else if(!edge){if(i>=2 && i<6)expected=0x3300+i;if(mapped && i==target)expected=output;}final.expected.push_back({int(r::STATE_AR0+i),expected});}
		if(!edge){final.expected.push_back({STATE_GENPC,base+0x602});final.expected_data.push_back({address,output});}
		else{initial.expected.clear();access.expected.back().value=1;final.expected[8].value=1;}
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(access)},{5,observation("between-Read-and-Execute",{})},{16,std::move(final)}},edge ? 3+seed%2 : 4+seed%3,16};
		if(edge){c.states.erase(c.states.begin()+2);c.states[1].observation.expected={{int(r::STATE_AR0+pointer),address},{r::STATE_ILLEGAL,1}};c.require_idle=false;}
		c.clear_pipeline_before_restore=true;c.program_events={{false,base+0x600,opcode,0,false}};
		c.actions.push_back({3,{{r::STATE_AR0+pointer,address}},false,{{base+0x600,0xdead}}});c.data_events=std::vector<clocked_data_event>{};
		if(!edge){clocked_register_action poison{4,{}};for(unsigned i=2;i<6;++i)if(!mapped || i!=target)poison.values.push_back({r::STATE_AR0+i,0x3300+i});c.actions.push_back(std::move(poison));
			c.actions.push_back({5,{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,t},{r::STATE_BRC,brc},{r::STATE_ST1,st1^31U}},false,{},{},{{address,u16(memory^0xffff)}}});
			c.data_events->push_back({false,address,memory,5,true});c.data_events->push_back({true,address,output,6,true});}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
