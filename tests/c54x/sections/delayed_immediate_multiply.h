// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
// Two-word MPY Smem,#lk,dst filling a two-word control delay (ROM4 83fb:
// BCD 83ff,BGEQ; MPY *AR1+,#07fd,A). Read publishes Smem to T; Execute
// uses the retained Smem and literal, not later RAM, T or program words.
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned control=0;control<5;++control)for(unsigned destination=0;destination<2;++destination)for(unsigned pair=0;pair<16;++pair){
		unsigned const seed=control+destination+pair,flags=seed%8;
		u32 const caller=seed&1 ? 0x20000 : 0,target=control==4 ? caller^0x20000 : caller;
		bool const conditional=control==1 || control==2;
		unsigned const read=8+unsigned(conditional),literal_clock=conditional ? 6 : 5;
		u16 const memory=std::array<u16,4>{0,1,0x8000,0xffff}[pair%4],lk=std::array<u16,4>{0x07fd,1,0x8000,0xffff}[pair/4];
		u64 const a=control==1 ? 0 : control==2 ? 1 : 0x123456789aULL,b=0xfedcba9876ULL;
		s64 product=s64(s16(memory))*s16(lk)*(flags&1 ? 2 : 1);
		// SMUL saturates only before MAC/MAS accumulation; plain MPY overflows into OVM.
		bool const overflow=product<-2147483648LL || product>2147483647LL;
		u64 const expected=u64(overflow && (flags&2) ? std::clamp(product,s64(-2147483648LL),s64(2147483647LL)) : product)&0xffffffffffULL;
		u16 const st1=0x900|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0),final_st0=0xa008|(overflow ? destination ? 0x200 : 0x400 : 0);
		u16 const opcode=0x6292|(destination<<8);
		auto initial=initial_case("delayed-immediate-multiply-retains-Smem-and-literal",{u16(control==4 ? 0xfb80|(target>>16) : std::array<u16,4>{0xf273,0xfa45,0xfa45,0xf274}[control])});
		for(auto &word : initial.program)word.address+=caller;
		initial.program.push_back({caller+0x601,0x700});
		initial.program.insert(initial.program.end(),{{caller+0x602,opcode},{caller+0x603,lk}});
		for(u32 at : {caller+0x604,target+0x700})initial.program.insert(initial.program.end(),{{at,u16(0x880f|(destination<<8))},{at+1,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,caller+0x600},{r::STATE_PMST,flags&4 ? 2U : 0U},{r::STATE_ST0,0xa008},{r::STATE_ST1,st1},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,0x2222},{r::STATE_TRN,0x2468},{r::STATE_AR2,0x5000},{r::STATE_SP,0x3000}});
		initial.data={{0x5000,memory}};
		auto access=observation("Access-applies-postincrement-before-pointer-poison",{{r::STATE_AR2,0x5001}});
		auto sample=observation("Read-publishes-Smem-to-T",{{r::STATE_T,memory},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,0xa008}});
		auto execute=observation("Execute-uses-retained-Smem-and-literal",{{r::STATE_A,destination ? a : expected},{r::STATE_B,destination ? expected : b},{r::STATE_T,0xdead},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_AR2,0x6000}});
		auto final=execute;final.expected.insert(final.expected.end(),{{STATE_GENPC,control==2 ? caller+0x606 : target+0x702},{r::STATE_TRN,u16(expected)},
			{r::STATE_SP,control==4 ? 0x2ffeU : control==3 ? 0x2fffU : 0x3000U}});
		clocked_register_case c{{{0,std::move(initial)},{read-1,std::move(access)},{read,std::move(sample)},{read+1,std::move(execute)},{24,std::move(final)}},std::array<unsigned,3>{3,read-1,read}[seed%3],24};
		clocked_register_action extension{};extension.tick=literal_clock;extension.program={{caller+0x603,0x5010}};c.actions.push_back(extension);
		c.program_events={{false,caller+0x603,lk,literal_clock,true}};
		clocked_register_action pointer{};pointer.tick=read-1;pointer.values={{r::STATE_AR2,0x6000}};c.actions.push_back(pointer);
		clocked_register_action operand{};operand.tick=read;operand.data={{0x5000,0xdead}};operand.values={{r::STATE_T,0xdead}};c.actions.push_back(operand);
		c.data_events=std::vector<clocked_data_event>{};
		if(control==3 || control==4)c.data_events->push_back({true,0x2fff,0x604,0,false});
		if(control==4)c.data_events->push_back({true,0x2ffe,u16(caller>>16),0,false});
		c.data_events->push_back({false,0x5000,memory,read,true});
		c.data_events->push_back({true,0xf,u16(expected),0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
