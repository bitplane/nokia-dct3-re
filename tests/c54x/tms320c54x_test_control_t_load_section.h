// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> control_t_load_cases(bool branch)
{
	std::vector<clocked_register_case> cases;
	for(unsigned control=0;control<(branch ? 3U : 6U);++control)for(unsigned pattern=0;pattern<4;++pattern)for(unsigned form=0;form<4;++form)
	for(unsigned slot=0;slot<2;++slot)for(unsigned accumulator=0;accumulator<2;++accumulator)for(unsigned late=0;late<(branch ? 1U : 3U);++late){
		unsigned const seed=control+pattern+form+slot+accumulator+late,branch_count=branch ? std::array<unsigned,3>{0,1,7}[control] : 0;
		u32 const base=seed&1 ? 0x20000 : 0,target_base=!branch && control==4 ? base^0x20000 : base;
		u16 const target=accumulator ? 0x580 : 0x610,value=std::array<u16,4>{0,1,0x8000,0xffff}[pattern],mode=std::array<u16,4>{0x12,0x84,0x94,0x8c}[form];
		u16 const status=0x800|((seed%4)<<8)|(seed&2 ? 0x4000 : 0),sp=0x1e00+(!branch ? control==3 ? -1 : control==4 ? -2 : control==5 ? 1 : 0 : 0);
		u16 const source=form ? 0x2800 : status&0x4000 ? sp+0x12 : 0x1412,loaded=late==2 ? 0x5aa5 : late ? value^0xffff : value,product=u16(s32(s16(loaded))*3);
		unsigned const read=branch ? 7+slot : 7+unsigned(slot!=0)+unsigned(control==5 || ((control==1 || control==2) && slot));
		u64 const a=!branch && (control==1 || control==2) ? control==1 ? 0 : 1 : 0x123456789aULL,b=0x123456789aULL;
		u16 const opcode=branch ? 0x6e8f : control==4 ? 0xfb80|(target_base>>16) : std::array<u16,6>{0xf273,0xfa45,0xfa45,0xf274,0,0xfe00}[control];
		auto initial=initial_case(branch ? "BANZD-delay-T-load-follows-selected-continuation" : "control-delay-T-load-and-newer-T-ownership",{opcode});
		for(auto &word : initial.program)word.address+=base;
		if(branch || control!=5)initial.program[1].value=target;
		unsigned const delay=!branch && control==5 ? 1 : 2;initial.program[delay+slot].value=0x3000|mode;
		for(u32 at : {base+0x604,target_base+target})initial.program.insert(initial.program.end(),{{at,u16(0x2083|(accumulator<<8))},{at+1,u16(0x8092|(accumulator<<8))},{at+2,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x2628},{r::STATE_ST1,status},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_SP,0x1e00},{r::STATE_T,0x5555},{r::STATE_BRC,13}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,i==2 ? 0x4000U : i==3 ? 0x3000U : i==4 ? 0x2800U : branch && i==7 ? branch_count : 0x3300+i});
		initial.data={{source,value},{0x3000,3},{0x3fff,0xdead},{0x4000,0xdead},{0x4001,0xdead}};if(!branch && control==5)initial.data.push_back({0x1e00,target});
		u32 const continuation=branch ? base+(branch_count ? target+3 : 0x607) : control==2 ? base+0x607 : target_base+target+3;
		auto final=observation("selected-path-product-and-only-owned-stack-effects",{{STATE_GENPC,continuation},{r::STATE_SP,sp},{r::STATE_T,loaded},{r::STATE_BRC,13},{r::STATE_ST1,status},{int(accumulator ? r::STATE_A : r::STATE_B),accumulator ? a : b}},{{0x3fff,0xdead},{0x4000,product},{0x4001,0xdead}});
		for(unsigned i=0;i<8;++i)final.expected.push_back({int(r::STATE_AR0+i),i==2 ? 0x4001U : i==3 ? 0x3000U : i==4 ? u16(0x2800+(form==2 ? 1 : form==3 ? -1 : 0)) : branch && i==7 ? u16(branch_count-1) : 0x3300+i});
		clocked_register_case c{{{0,std::move(initial)},{read,observation("T-publishes-at-Read",{{r::STATE_T,value}})},{read+1,observation("newer-T-is-not-overwritten",{{r::STATE_T,loaded}})},{80,std::move(final)}},std::array<unsigned,3>{5,read,12}[seed%3],80};c.clear_pipeline_before_restore=!branch;
		c.data_events=std::vector<clocked_data_event>{};if(!branch && control==5)c.data_events->push_back({false,0x1e00,target,0,false});
		if(!branch && (control==3 || control==4))c.data_events->push_back({true,0x1dff,0x604,6,true});
		if(!branch && control==4 && read>7)c.data_events->push_back({true,0x1dfe,u16(base>>16),7,true});
		c.data_events->push_back({false,source,value,read,true});
		if(!branch && control==4 && read==7)c.data_events->push_back({true,0x1dfe,u16(base>>16),7,true});
		c.data_events->push_back({false,0x3000,3,0,false});c.data_events->push_back({true,0x4000,product,0,false});
		if(branch || control!=5)c.actions.push_back({5,{},false,{{base+0x601,0x620}}});
		if(late)c.actions.push_back({read,{{r::STATE_T,loaded}}});
		c.actions.push_back({read+1,{},false,{{base+0x600+delay+slot,0x3000}},{},{{source,u16(~value)}}});cases.push_back(std::move(c));
	}
	return cases;
}
}
