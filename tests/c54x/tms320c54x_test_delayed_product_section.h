// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// T-source MPY, MAC/MACR/MAS/MASR and MPYR delay compositions.
inline std::vector<clocked_register_case> delayed_product_cases(unsigned family)
{
	std::vector<clocked_register_case> cases;
	for(unsigned control=0;control<6;++control)for(unsigned form=0;form<4;++form)for(unsigned destination=0;destination<2;++destination)
	for(unsigned pair=0;pair<16;++pair)for(unsigned operation=0;operation<(family==1 ? 4U : 1U);++operation){
		unsigned const seed=control+form+destination+pair+operation,flags=seed%(family ? 8 : 4);
		u32 const caller=seed&1 ? 0x20000 : 0,target=control==4 ? caller^0x20000 : caller;
		unsigned const delay=control==5 ? 1 : 2,read=7+unsigned(form!=0)+unsigned(control==5 || ((control==1 || control==2) && form));
		unsigned const literal_clock=control==5 ? 4 : control==1 || control==2 ? 6 : 5;
		u16 const memory=std::array<u16,4>{0,1,0x8000,0xffff}[pair%4],t=std::array<u16,4>{0,1,0x8000,0xffff}[pair/4];
		u16 const address=std::array<u16,4>{0x5000,0x5000,u16(family==1 ? 0x15 : 0x17),0x5003}[form],literal=form==3 ? 3 : address;
		u64 const a=control==1 ? 0 : control==2 ? 1 : 0x123456789aULL,b=0xfedcba9876ULL,source=destination ? b : a;
		s64 product=s64(s16(memory))*s16(t)*(flags&1 ? 2 : 1);
		if(family!=2 && (flags&7)==7)product=std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
		if(family==2)product=(product+32768)&~s64(65535);
		s64 result=product;
		if(family==1){result=(source&0x8000000000ULL ? s64(source)-0x10000000000LL : s64(source))+(operation&2 ? -product : product);if(operation&1)result=(result+32768)&~s64(65535);}
		bool const overflow=result<-2147483648LL || result>2147483647LL;
		u64 const expected=u64(overflow && (flags&2) ? std::clamp(result,s64(-2147483648LL),s64(2147483647LL)) : result)&0xffffffffffULL;
		u16 const st1=0x900|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0),final_st0=0xa008|(overflow ? destination ? 0x200 : 0x400 : 0);
		u16 const opcode=(family==1 ? 0x2800+operation*0x200 : family==2 ? 0x2200 : 0x2000)|(destination<<8)|(form==0 ? (family==2 ? 0x92 : 0x82) : form==3 ? 0xe2 : 0xf8);
		auto initial=initial_case("delayed-product-retains-memory-and-T-before-poison",{u16(control==4 ? 0xfb80|(target>>16) : std::array<u16,6>{0xf273,0xfa45,0xfa45,0xf274,0,0xfe00}[control])});
		for(auto &word : initial.program)word.address+=caller;
		if(control!=5)initial.program.push_back({caller+0x601,0x700});
		initial.program.push_back({caller+0x600+delay,opcode});if(form)initial.program.push_back({caller+0x601+delay,literal});
		for(u32 at : {caller+0x604,target+0x700})initial.program.insert(initial.program.end(),{{at,u16(0x880f|(destination<<8))},{at+1,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,caller+0x600},{r::STATE_PMST,flags&4 ? 2U : 0U},{r::STATE_ST0,0xa008},{r::STATE_ST1,st1},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,t},{r::STATE_TRN,0x2468},{r::STATE_AR2,0x5000},{r::STATE_SP,0x3000}});
		if(form==2)initial.initial.push_back({family==1 ? r::STATE_AR5 : r::STATE_AR7,memory});
		initial.data={{0x3000,0x700},{address,memory}};
		auto access=observation("Access-applies-postincrement-before-pointer-poison",{{r::STATE_AR2,family==2 && !form ? 0x5001U : 0x5000U}});
		auto sample=observation("Read-retains-old-T-and-original-operand",{{r::STATE_T,t},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,0xa008}});
		auto execute=observation("Execute-uses-retained-product-not-newer-T",{{r::STATE_A,destination ? a : expected},{r::STATE_B,destination ? expected : b},{r::STATE_T,0xdead},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_AR2,0x6000}});
		auto final=execute;final.expected.insert(final.expected.end(),{{STATE_GENPC,control==2 ? caller+0x606 : target+0x702},{r::STATE_TRN,u16(expected)},
			{r::STATE_SP,control==4 ? 0x2ffeU : control==3 ? 0x2fffU : control==5 ? 0x3001U : 0x3000U}});
		clocked_register_case c{{{0,std::move(initial)},{read-1,std::move(access)},{read,std::move(sample)},{read+1,std::move(execute)},{24,std::move(final)}},std::array<unsigned,3>{3,read-1,read}[seed%3],24};
		if(form){clocked_register_action extension{};extension.tick=literal_clock;extension.program={{caller+0x601+delay,0x5010}};c.actions.push_back(extension);c.program_events={{false,caller+0x601+delay,literal,literal_clock,true}};}
		clocked_register_action pointer{};pointer.tick=read-1;pointer.values={{r::STATE_AR2,0x6000}};c.actions.push_back(pointer);
		clocked_register_action operand{};operand.tick=read;operand.data={{address,0xdead}};operand.values={{r::STATE_T,0xdead}};c.actions.push_back(operand);
		c.data_events=std::vector<clocked_data_event>{};if(control==5)c.data_events->push_back({false,0x3000,0x700,0,false});
		if(control==3 || control==4)c.data_events->push_back({true,0x2fff,0x604,0,false});
		if(control==4 && form)c.data_events->push_back({true,0x2ffe,u16(caller>>16),0,false});
		c.data_events->push_back({false,address,memory,read,true});
		if(control==4 && !form)c.data_events->push_back({true,0x2ffe,u16(caller>>16),0,false});
		c.data_events->push_back({true,0xf,u16(expected),0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
