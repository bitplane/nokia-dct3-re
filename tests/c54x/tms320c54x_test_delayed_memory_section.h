// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
struct delayed_memory_contract {
	unsigned control, destination;
	u32 caller;
	u16 opcode, literal, address, memory, st0, final_st0, st1;
	u64 a,b,result;
	std::array<u16,8> pointers,updated;
	bool arithmetic=false,poison_pointers=true,paired=false;
	u16 low=0;
};
inline clocked_register_case delayed_memory_case(delayed_memory_contract const &f,unsigned checkpoint_index)
{
	unsigned const delay=f.control==5 ? 1 : 2,read=8+unsigned(f.control==1 || f.control==2 || f.control==5),literal_clock=f.control==5 ? 4 : f.control==1 || f.control==2 ? 6 : 5;
	u32 const target=f.control==4 ? f.caller^0x20000 : f.caller;
	auto initial=initial_case("delay-memory-capture-and-continuation-publication",{u16(f.control==4 ? 0xfb80|(target>>16) : std::array<u16,6>{0xf273,0xfa45,0xfa45,0xf274,0,0xfe00}[f.control])});
	for(auto &word : initial.program)word.address+=f.caller;
	if(f.control!=5)initial.program.push_back({f.caller+0x601,0x700});
	initial.program.insert(initial.program.end(),{{f.caller+0x600+delay,f.opcode},{f.caller+0x601+delay,f.literal}});
	for(u32 at : {f.caller+0x604,target+0x700})initial.program.insert(initial.program.end(),{{at,u16(0x880f|(f.destination<<8))},{at+1,0xf4e1}});
	initial.initial.insert(initial.initial.end(),{{STATE_GENPC,f.caller+0x600},{r::STATE_ST0,f.st0},{r::STATE_ST1,f.st1},{r::STATE_A,f.a},{r::STATE_B,f.b},
		{r::STATE_T,0x5aa5},{r::STATE_TRN,0x2468},{r::STATE_SP,0x3000},{r::STATE_BK,13}});
	for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,f.pointers[i]});
	initial.data={{0x3000,0x700},{f.address,f.memory}};
	if(f.paired)initial.data.push_back({u32(f.address^1),f.low});
	auto access=observation("Access-captures-address-and-applies-only-owned-updates",{{r::STATE_A,f.a},{r::STATE_B,f.b},{r::STATE_ST0,f.arithmetic ? f.st0 : f.final_st0},{r::STATE_ST1,f.st1},{r::STATE_T,0x5aa5}});
	auto sample=access,execute=observation("Execute-publishes-retained-memory-result",{{r::STATE_A,f.destination ? f.a : f.result},{r::STATE_B,f.destination ? f.result : f.b},{r::STATE_ST0,f.final_st0},{r::STATE_ST1,f.st1},{r::STATE_T,0x5aa5}});
	for(unsigned i=0;i<8;++i){access.expected.push_back({int(r::STATE_AR0+i),f.updated[i]});sample.expected.push_back({int(r::STATE_AR0+i),f.poison_pointers ? 0x2200U+i : f.updated[i]});execute.expected.push_back({int(r::STATE_AR0+i),f.poison_pointers ? 0x2200U+i : f.updated[i]});}
	auto final=execute;final.expected.insert(final.expected.end(),{{STATE_GENPC,f.control==2 ? f.caller+0x606 : target+0x702},{r::STATE_TRN,u16(f.result)},
		{r::STATE_SP,f.control==4 ? 0x2ffeU : f.control==3 ? 0x2fffU : f.control==5 ? 0x3001U : 0x3000U}});
	clocked_register_case c{{{0,std::move(initial)},{read-1,std::move(access)},{read,std::move(sample)},{read+1,std::move(execute)},{24,std::move(final)}},
		std::array<unsigned,3>{literal_clock,read-1,read}[checkpoint_index%3],24};
	clocked_register_action literal{};literal.tick=literal_clock;literal.program={{f.caller+0x601+delay,0xdead}};c.actions.push_back(literal);
	if(f.poison_pointers){clocked_register_action pointers{};pointers.tick=read-1;for(unsigned i=0;i<8;++i)pointers.values.push_back({r::STATE_AR0+i,0x2200+i});c.actions.push_back(pointers);}
	if(f.address>=0x60){clocked_register_action source{};source.tick=read;source.data={{f.address,0xdead}};if(f.paired)source.data.push_back({u32(f.address^1),0xbeef});c.actions.push_back(source);}
	c.program_events={{false,f.caller+0x601+delay,f.literal,literal_clock,true}};c.data_events=std::vector<clocked_data_event>{};
	if(f.control==5)c.data_events->push_back({false,0x3000,0x700,0,false});
	if(f.control==3 || f.control==4)c.data_events->push_back({true,0x2fff,0x604,0,false});
	if(f.control==4)c.data_events->push_back({true,0x2ffe,u16(f.caller>>16),0,false});
	c.data_events->push_back({false,f.address,f.memory,read,true});
	if(f.paired)c.data_events->push_back({false,u32(f.address^1),f.low,read,true});
	c.data_events->push_back({true,0xf,u16(f.result),read+2+unsigned(f.control==1),true});
	return c;
}
inline s64 signed_accumulator(u64 word){return word&0x8000000000ULL ? s64(word)-0x10000000000LL : s64(word);}
inline std::vector<clocked_register_case> delayed_shift_cases(bool arithmetic)
{
	std::vector<clocked_register_case> cases;
	for(unsigned control=0;control<6;++control)for(unsigned form=0;form<3;++form)for(unsigned destination=0;destination<2;++destination)
	for(unsigned raw=0;raw<32;++raw)for(unsigned operation=0;operation<(arithmetic ? 4U : 1U);++operation){
		unsigned const selector=control+form+destination+raw+operation,pattern=(control+form+raw+destination)%4,flags=(raw/4+control+destination)%4;
		delayed_memory_contract f{};f.control=control;f.destination=destination;f.caller=selector&1 ? 0x20000 : 0;
		f.address=0x1002;f.memory=std::array<u16,4>{0,1,0x8000,0xffff}[pattern];f.st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0);
		f.a=control==1 ? 0 : control==2 ? 1 : 0x123456789aULL;f.b=0xfedcba9876ULL;
		int const shift=raw<16 ? int(raw) : int(raw)-32;s64 const input=flags&1 ? s64(s16(f.memory)) : f.memory,divisor=s64(1)<<(shift<0 ? -shift : 0);
		s64 const shifted=shift>=0 ? input*(s64(1)<<shift) : input/divisor-(input<0 && input%divisor!=0);
		f.st0=f.final_st0=arithmetic ? 0xa020|(pattern<<9) : 0xa620;f.result=u64(shifted)&0xffffffffffULL;
		f.opcode=0x6f00|(form==0 ? 2 : form==1 ? 0x83 : 0x93);f.literal=0x0c40|(destination<<8)|raw;f.arithmetic=arithmetic;
		if(arithmetic){u64 const source=operation&1 ? f.b : f.a;bool const subtract=operation&2; s64 const sum=signed_accumulator(source)+(subtract ? -shifted : shifted);
			bool const carry=subtract ? u32(source)>=u32(f.result) : u64(u32(source))+u32(f.result)>0xffffffffULL,overflow=sum<-2147483648LL || sum>2147483647LL;
			f.final_st0=(f.st0&~0x800)|(carry ? 0x800 : 0)|(overflow ? destination ? 0x200 : 0x400 : 0);
			f.result=u64(flags&2 ? std::clamp(sum,s64(-2147483648LL),s64(2147483647LL)) : sum)&0xffffffffffULL;
			f.literal=0x0c00|(subtract ? 0x20 : 0)|((operation&1)<<9)|(destination<<8)|raw;
		}
		for(unsigned i=0;i<8;++i)f.pointers[i]=0x5100+i;
		f.pointers[3]=0x1002;f.updated=f.pointers;if(form==2)++f.updated[3];
		cases.push_back(delayed_memory_case(f,selector));
	}
	return cases;
}
inline std::vector<clocked_register_case> delayed_absolute_add_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned control : {0U,3U,4U,5U})for(u16 address : {u16(0x500),u16(0x13),u16(8),u16(9),u16(0xb),u16(0xc)})
	for(unsigned operand=0;operand<4;++operand)for(unsigned initial=0;initial<4;++initial)for(unsigned destination=0;destination<2;++destination)for(unsigned flags=0;flags<4;++flags){
		delayed_memory_contract f{};f.control=control;f.destination=destination;f.caller=(control+address+operand+initial+destination+flags)&1 ? 0x20000 : 0;
		f.address=f.literal=address;f.opcode=0x00f8|(destination<<8);f.memory=std::array<u16,4>{0,1,0x8000,0xffff}[operand];
		u64 const left=std::array<u64,4>{0,0x7fffffff,0xff80000000ULL,0xffffffffffULL}[initial];f.a=destination ? 0x123456789aULL : left;f.b=destination ? left : 0xfedcba9876ULL;
		if(address==8 || address==9){unsigned const shift=address==9 ? 16 : 0;f.a=(f.a&~(0xffffULL<<shift))|(u64(f.memory)<<shift);}
		if(address==0xb || address==0xc){unsigned const shift=address==0xc ? 16 : 0;f.b=(f.b&~(0xffffULL<<shift))|(u64(f.memory)<<shift);}
		f.st0=0xa008|(operand&1 ? 0x600 : 0)|(initial&1 ? 0x800 : 0);f.st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0);
		u64 const source=destination ? f.b : f.a;s64 const addend=flags&1 ? s16(f.memory) : f.memory,sum=signed_accumulator(source)+addend;
		bool const overflow=sum<-2147483648LL || sum>2147483647LL,carry=u64(u32(source))+u32(addend)>0xffffffffULL;
		f.result=overflow && (flags&2) ? (sum<0 ? 0xff80000000ULL : 0x7fffffffULL) : u64(sum)&0xffffffffffULL;
		f.final_st0=(f.st0&~0x800)|(carry ? 0x800 : 0)|(overflow ? destination ? 0x200 : 0x400 : 0);f.arithmetic=true;f.poison_pointers=false;
		for(unsigned i=0;i<8;++i)f.pointers[i]=0x5100+i;
		if(address==0x13)f.pointers[3]=f.memory;
		f.updated=f.pointers;
		cases.push_back(delayed_memory_case(f,control+address+operand+initial+destination+flags));
	}
	return cases;
}
}
