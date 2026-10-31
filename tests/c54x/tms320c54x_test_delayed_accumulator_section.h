// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// MPYA/MASA/MACA/MACAR, SQURA/SQURS, SQUR, long-address accumulator products.
inline std::vector<clocked_register_case> delayed_accumulator_cases(unsigned family)
{
	std::vector<clocked_register_case> cases;
	bool const extended=family==3;
	for(unsigned control=0;control<(extended ? 1U : 6U);++control)for(unsigned form=0;form<3;++form)
	for(unsigned ar=0;ar<(extended ? 8U : 1U);++ar)for(unsigned operation=0;operation<(family==2 ? 2U : 4U);++operation)for(unsigned pattern=0;pattern<8;++pattern){
		unsigned const seed=control+form+ar+operation+pattern,flags=seed%16,destination=family==1 ? operation&1 : family==2 ? operation : 1;
		u32 const caller=seed&1 ? 0x20000 : 0,target=control==4 ? caller^0x20000 : caller;
		unsigned const address_ar=extended ? ar : 2,delay=control==5 ? 1 : 2,read=extended ? 8 : 7+unsigned(control==5);
		u16 const address=extended ? (form==2 ? 0x5000 : form ? 0x3b08 : 0x5003) : form ? 0x5000 : 0x1062;
		u16 const memory=std::array<u16,8>{0,1,0xffff,0x7fff,0x8000,0x8000,0xffff,0x5678}[pattern],literal=extended ? (form==2 ? address : form ? 0xeb08 : 3) : 0;
		s32 const high=std::array<s32,8>{0,1,-1,32767,-32768,65535,-65536,0x1234}[pattern];
		u64 a=control==1 ? 0 : control==2 ? 1 : (pattern&1 ? 0xfe00000000ULL : 0)|((u64(high)&0x1ffff)<<16)|(0x1234+pattern);
		u64 const b=std::array<u64,8>{0,1,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x1234567833ULL,0x80aa001234ULL}[pattern];
		if((family==1 || family==2) && control!=1 && control!=2)a=b^0x55;
		auto signed40=[](u64 value)->s64{return value&0x8000000000ULL ? s64(value)-0x10000000000LL : s64(value);};
		s64 product=0,result=0;
		if(family==1 || family==2){product=s64(s16(memory))*s16(memory)*(flags&1 ? 2 : 1);result=family==2 ? product : signed40(destination ? b : a)+(operation>>1 ? -product : product);}
		else {int const lane=(a>>16)&0x1ffff;product=s64(lane&0x10000 ? lane-0x20000 : lane)*s16(memory)*(flags&1 ? 2 : 1);
			if(operation && (flags&7)==7)product=std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
			result=!operation ? product : signed40(b)+(operation==1 ? -product : product);if(operation==3)result=(result+32768)&~s64(65535);
		}
		bool const overflow=result<-2147483648LL || result>2147483647LL;
		u64 const expected=u64(flags&2 ? std::clamp(result,s64(-2147483648LL),s64(2147483647LL)) : result)&0xffffffffffULL;
		u16 const st1=0x800|(flags&8 ? 0x100 : 0)|(flags&1 ? 0x40 : 0)|(flags&2 ? 0x200 : 0),final_st0=0xa020|(overflow ? destination ? 0x200 : 0x400 : 0);
		u16 const mode=extended ? (form==2 ? 0xf8 : 0xe0|ar) : form==0 ? 0x62 : form==1 ? 0x82 : 0x92;
		u16 const opcode=family==2 ? 0x2600|(destination<<8)|mode : family==1 ? 0x3800|((operation>>1)<<9)|(destination<<8)|mode : (0x3100+operation*0x200)|mode;
		auto initial=initial_case("delayed-accumulator-product-captures-memory-but-retains-live-arithmetic",{u16(control==4 ? 0xfb80|(target>>16) : std::array<u16,6>{0xf273,0xfa45,0xfa45,0xf274,0,0xfe00}[control])});
		for(auto &word : initial.program)word.address+=caller;
		if(control!=5)initial.program.push_back({caller+0x601,0x700});
		initial.program.push_back({caller+0x600+delay,opcode});if(extended)initial.program.push_back({caller+0x603,literal});
		for(u32 at : {caller+0x604,target+0x700})initial.program.insert(initial.program.end(),{{at,u16(0x880f|(destination<<8))},{at+1,0xf4e1}});
		u64 const mask=extended ? 0xffffffffffULL : 0;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,caller+0x600},{r::STATE_PMST,flags&4 ? 2U : 0U},{r::STATE_ST0,0xa020},{r::STATE_ST1,st1},
			{r::STATE_A,a^mask},{r::STATE_B,b^mask},{r::STATE_T,0x5aa5},{r::STATE_TRN,0x2468},{r::STATE_SP,0x3000},{r::STATE_AR2,0x5000},{r::STATE_AR0+address_ar,0x5000}});
		initial.data={{address,memory},{0x3000,0x700}};
		auto access=observation("Access-captures-pointer-once",{{int(r::STATE_AR0+address_ar),!extended && form==2 ? 0x5001U : 0x5000U},{r::STATE_A,a^mask},{r::STATE_B,b^mask}});
		auto sample=observation("Read-publishes-memory-to-T",{{r::STATE_T,memory},{r::STATE_A,a^mask},{r::STATE_B,b^mask},{r::STATE_ST0,0xa020}});
		auto execute=observation("Execute-sees-live-accumulators-and-preserves-newer-T",{{r::STATE_A,destination ? a : expected},{r::STATE_B,destination ? expected : b},{r::STATE_T,0xdead},
			{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{int(r::STATE_AR0+address_ar),0x6000}});
		auto final=execute;final.expected.insert(final.expected.end(),{{STATE_GENPC,control==2 ? caller+0x606 : target+0x702},{r::STATE_TRN,u16(expected)},
			{r::STATE_SP,control==4 ? 0x2ffeU : control==3 ? 0x2fffU : control==5 ? 0x3001U : 0x3000U}});
		clocked_register_case c{{{0,std::move(initial)},{read-1,std::move(access)},{read,std::move(sample)},{read+1,std::move(execute)},{24,std::move(final)}},
			extended ? std::array<unsigned,4>{3,5,8,9}[seed%4] : std::array<unsigned,3>{3,read-1,read}[seed%3],24};c.clear_pipeline_before_restore=true;
		if(extended){clocked_register_action extension{};extension.tick=5;extension.program={{caller+0x603,0x5010}};c.actions.push_back(extension);c.program_events={{false,caller+0x603,literal,5,true}};}
		clocked_register_action pointer{};pointer.tick=read-1;pointer.values={{r::STATE_AR0+address_ar,0x6000}};c.actions.push_back(pointer);
		clocked_register_action operand{};operand.tick=read;operand.data={{address,0xdead}};operand.values={{r::STATE_T,0xdead}};if(extended)operand.values.insert(operand.values.end(),{{r::STATE_A,a},{r::STATE_B,b}});c.actions.push_back(operand);
		c.data_events=std::vector<clocked_data_event>{};if(control==5)c.data_events->push_back({false,0x3000,0x700,0,false});
		if(control==3 || control==4)c.data_events->push_back({true,0x2fff,0x604,0,false});
		c.data_events->push_back({false,address,memory,read,true});if(control==4)c.data_events->push_back({true,0x2ffe,u16(caller>>16),0,false});
		c.data_events->push_back({true,0xf,u16(expected),0,false});cases.push_back(std::move(c));
	}
	return cases;
}
}
