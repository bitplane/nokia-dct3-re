// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> delayed_load_cases(bool paired)
{
	std::vector<clocked_register_case> cases;
	for(unsigned control=0;control<6;++control)for(unsigned form=0;form<4;++form)for(unsigned ar=0;ar<8;++ar)
	for(bool compatibility : {false,true})for(bool alternate : {false,true})for(unsigned destination=0;destination<2;++destination){
		if(form==3 && ar)continue; // Absolute addressing has no AR selector.
		unsigned const selector=control+form+ar+compatibility+alternate+destination, pattern=selector%4, flags=(control+form+ar+destination)%4;
		u32 const caller=selector&1 ? 0x20000 : 0, target=control==4 ? caller^0x20000 : caller;
		unsigned const selected=compatibility && !ar ? 5 : ar, delay=control==5 ? 1 : 2, read=8+unsigned(control==5 || control==1 || control==2);
		unsigned const literal_clock=control==5 ? 4 : control==1 || control==2 ? 6 : 5;
		u16 const address=paired ? (alternate ? 0x5007 : 0x5004) : (alternate ? 0x5004 : 0x5006), literal=form==3 ? address : paired ? (alternate ? 2 : 0xffff) : (alternate ? 0xffff : 1);
		u16 const memory=std::array<u16,4>{0,1,0x8000,0xffff}[pattern], low=std::array<u16,4>{0x1234,0xffff,0,0x8001}[pattern];
		u16 const st0=0xa608, final_st0=compatibility && ar && form!=3 ? (st0&0x1fff)|(ar<<13) : st0;
		u16 const st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? (paired ? 0x80 : 0x200) : 0)|(compatibility ? 0x20 : 0);
		u64 const a=control==1 ? 0 : control==2 ? 1 : 0x123456789aULL,b=0xfedcba9876ULL;
		u64 const result=paired ? ((u64(memory)<<16)|low|((flags&1)&&(memory&0x8000) ? 0xff00000000ULL : 0)) : u64(flags&1 ? s64(s16(memory)) : s64(memory))&0xffffffffffULL;
		auto initial=initial_case("delay-load-retains-literal-address-and-ordered-operands",{u16(control==4 ? 0xfb80|(target>>16) : std::array<u16,6>{0xf273,0xfa45,0xfa45,0xf274,0,0xfe00}[control])});
		for(auto &word : initial.program)word.address+=caller;
		if(control!=5)initial.program.push_back({caller+0x601,0x700});
		initial.program.insert(initial.program.end(),{{caller+0x600+delay,u16((paired ? 0x5600 : 0x1000)|(destination<<8)|(form==3 ? 0xf8 : (0xe0+form*8)|ar))},{caller+0x601+delay,literal}});
		for(u32 at : {caller+0x604,target+0x700})initial.program.insert(initial.program.end(),{{at,u16(0x880f|(destination<<8))},{at+1,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,caller+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0x5aa5},{r::STATE_TRN,0x2468},{r::STATE_SP,0x3000},{r::STATE_BK,13}});
		std::array<u16,8> pointers;for(unsigned i=0;i<8;++i)pointers[i]=0x5100+i;pointers[selected]=0x5005;
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,pointers[i]});
		initial.data={{0x3000,0x700},{address,memory}};if(paired)initial.data.push_back({u32(address^1),low});
		auto access=observation("Access-updates-selected-address-only",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_T,0x5aa5}});
		for(unsigned i=0;i<8;++i)access.expected.push_back({int(r::STATE_AR0+i),i==selected && (form==1 || form==2) ? address : pointers[i]});
		auto sampled=observation("Read-retains-data-before-source-poison",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_T,0x5aa5}});
		auto execute=observation("Execute-publishes-retained-load",{{r::STATE_A,destination ? a : result},{r::STATE_B,destination ? result : b},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_T,0x5aa5}});
		for(unsigned i=0;i<8;++i){sampled.expected.push_back({int(r::STATE_AR0+i),0x2200+i});execute.expected.push_back({int(r::STATE_AR0+i),0x2200+i});}
		auto final=execute;final.expected.insert(final.expected.end(),{{STATE_GENPC,control==2 ? caller+0x606 : target+0x702},{r::STATE_TRN,u16(result)},{r::STATE_SP,control==4 ? 0x2ffeU : control==3 ? 0x2fffU : control==5 ? 0x3001U : 0x3000U}});
		clocked_register_case c{{{0,std::move(initial)},{read-1,std::move(access)},{read,std::move(sampled)},{read+1,std::move(execute)},{24,std::move(final)}},std::array<unsigned,3>{literal_clock,read-1,read}[selector%3],24};
		clocked_register_action extension{};extension.tick=literal_clock;extension.program={{caller+0x601+delay,0xdead}};c.actions.push_back(extension);
		clocked_register_action pointer{};pointer.tick=read-1;for(unsigned i=0;i<8;++i)pointer.values.push_back({r::STATE_AR0+i,0x2200+i});c.actions.push_back(pointer);
		clocked_register_action operand{};operand.tick=read;operand.data={{address,0xdead}};if(paired)operand.data.push_back({u32(address^1),0xbeef});c.actions.push_back(operand);
		c.program_events={{false,caller+0x601+delay,literal,literal_clock,true}};
		c.data_events=std::vector<clocked_data_event>{};
		if(control==5)c.data_events->push_back({false,0x3000,0x700,0,false});
		if(control==3 || control==4)c.data_events->push_back({true,0x2fff,0x604,0,false});
		if(control==4)c.data_events->push_back({true,0x2ffe,u16(caller>>16),0,false});
		c.data_events->push_back({false,address,memory,read,true});if(paired)c.data_events->push_back({false,u32(address^1),low,read,true});
		c.data_events->push_back({true,0xf,u16(result),read+2+unsigned(control==1),true});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
