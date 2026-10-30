// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> register_alu_cases(bool load)
{
	std::vector<clocked_register_case> cases;
	auto add=[&](unsigned operation,unsigned selector,unsigned pattern,unsigned flags,int shift,bool asm_shift,bool producer,unsigned gap) {
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		std::array<u64,2> const initial_values=producer ? std::array<u64,2>{0x10,0x20} : std::array<u64,2>{
			std::array<u64,8>{0,1,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x80aa001234ULL,0xffffffff55ULL}[pattern],
			std::array<u64,8>{1,0xffffffff,0xff80000000ULL,0x7fffffff,0xff7fffffffULL,0x017fffffffULL,0x123456789aULL,0x80000000}[pattern]};
		auto before=initial_values;
		u16 const literal=std::array<u16,4>{0,1,0x8000,0xffff}[pattern%4];
		if (producer) before[selector>>1]=u64(s64(s16(literal)))&0xffffffffffULL;
		u64 const source=before[selector>>1];
		// Bit-by-bit signed/logical 40-bit shift reference, independent of core helpers.
		u64 operand=0;
		for (int bit=0;bit<40;++bit) {
			int const from=bit-shift;
			if ((from>=0 && from<40 && BIT(source,from)) || (shift<0 && from>=40 && (flags&1) && BIT(source,39))) operand|=u64(1)<<bit;
		}
		u64 const destination=before[selector&1];
		s64 const left=BIT(destination,39) ? s64(destination)-0x10000000000LL : s64(destination);
		s64 const right=BIT(operand,39) ? s64(operand)-0x10000000000LL : s64(operand);
		s64 const full_source=BIT(source,39) ? s64(source)-0x10000000000LL : s64(source);
		s64 const result=load ? (shift>=0 ? full_source*(s64(1)<<shift) : 0) : operation ? left-right : left+right;
		bool const overflow=(!load || shift>=0) && (result < -2147483648LL || result > 2147483647LL);
		bool const carry=operation ? u32(destination)>=u32(operand) : u64(u32(destination))+u32(operand)>0xffffffffULL;
		u16 const st0=load ? 0x1004|(flags&4 ? 0x800 : 0)|(flags&8 ? 0x600 : 0) : 0x1804|(flags&4 ? 0x600 : 0);
		u16 const st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0)|(asm_shift ? shift&31 : 0x13);
		u16 const final_st0=(load ? st0 : (st0&~0x800)|(carry ? 0x800 : 0))|(overflow ? selector&1 ? 0x200 : 0x400 : 0);
		auto after=before;
		after[selector&1]=overflow && (flags&2) ? (result<0 ? 0xff80000000ULL : 0x7fffffff) :
			load ? operand : u64(result)&0xffffffffffULL;
		u16 const opcode=(load ? asm_shift ? 0xf482 : 0xf440|(shift&31) :
			asm_shift ? 0xf480|operation : 0xf400|(operation<<5)|(shift&31))|((selector>>1)<<9)|((selector&1)<<8);
		unsigned const offset=producer ? 2+gap : 0, execute=producer ? 8+gap : 6;
		auto initial=initial_case("register-ALU-shift-and-Execute-publication",{u16(producer ? 0xf020|((selector>>1)<<8) : opcode)});
		if (producer) { initial.program[1].value=literal; initial.program[offset].value=opcode; }
		initial.program[offset+1].value=0xf4e1;
		for (auto &entry : initial.program) entry.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,initial_values[0]},
			{r::STATE_B,initial_values[1]},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		auto state=[&](char const *name,std::array<u64,2> values,u16 status) {
			return observation(name,{{r::STATE_A,values[0]},{r::STATE_B,values[1]},{r::STATE_ST0,status},{r::STATE_ST1,st1}});
		};
		clocked_register_case c{{{0,std::move(initial)},{1,state("before-Execute",initial_values,st0)},
			{execute,state("published-only-at-Execute",after,final_st0)}},
			(producer ? 5+gap : 3)+unsigned(cases.size()%3),18};
		if (producer) {
			c.actions={{4,{},false,{{base+0x601,0xdead}}}};
			c.program_events={{false,base+0x601,literal,0,false},{false,base+0x600+offset,opcode,0,false}};
			if (execute>7) c.states.insert(c.states.end()-1,{7,state("preceding-load-visible",before,st0)});
		} else c.program_events={{false,base+0x600,opcode,0,false}};
		c.forbid_data_access=true;
		cases.push_back(std::move(c));
	};
	for (unsigned op=0;op<(load ? 1U : 2U);++op) for (unsigned selector=0;selector<4;++selector)
	for (unsigned pattern=0;pattern<8;++pattern) for (unsigned flags=0;flags<(load ? 16U : 8U);++flags)
	for (int shift=-16;shift<16;++shift) {
		add(op,selector,pattern,flags,shift,false,false,0);
		if (shift==-16 || shift==-1 || shift==0 || shift==15) add(op,selector,pattern,flags,shift,true,false,0);
	}
	for (unsigned op=0;op<(load ? 1U : 2U);++op) for (unsigned selector=0;selector<4;++selector)
	for (int shift=-16;shift<16;++shift) add(op,selector,unsigned(shift+16)%8,unsigned(shift+16)%(load ? 16 : 8),shift,true,false,0);
	for (unsigned op=0;op<(load ? 1U : 2U);++op) for (unsigned selector=0;selector<4;++selector)
	for (unsigned pattern=0;pattern<4;++pattern) for (unsigned gap=0;gap<3;++gap) add(op,selector,pattern,load ? 5 : 1,0,false,true,gap);
	return cases;
}
}
