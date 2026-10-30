// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add=[&](bool plus,unsigned shift,unsigned selector,unsigned pattern,unsigned flags,bool producer,unsigned gap) {
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u16 const literal=producer ? 1 : std::array<u16,8>{1,1,0xffff,1,0x8000,0xffff,0xf495,0xffff}[pattern];
		u64 const source=producer ? u64(s64(s16(std::array<u16,4>{0,1,0x8000,0xffff}[pattern])))&0xffffffffffULL :
			std::array<u64,8>{0,0xffffffff,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x56783933,0x80aa001234ULL}[pattern];
		u16 const st0=0x1804|(flags&4 ? 0x600 : 0), st1=0x800|(flags&1 ? 0x100 : 0)|(flags&2 ? 0x200 : 0);
		s64 const left=BIT(source,39) ? s64(source)-0x10000000000LL : s64(source);
		s64 const right=(flags&1 ? s64(s16(literal)) : s64(literal))*(s64(1)<<shift);
		s64 const sum=plus ? left+right : left-right;
		bool const overflow=sum < -2147483648LL || sum > 2147483647LL;
		bool const carry=plus ? u64(u32(source))+u32(right)>0xffffffffULL : u32(source)>=u32(right);
		u16 const result_st0=(st0&~0x800)|(carry ? 0x800 : 0)|(overflow ? selector&1 ? 0x200 : 0x400 : 0);
		u64 const result=u64(overflow && (flags&2) ? std::clamp<s64>(sum,-2147483648LL,2147483647LL) : sum)&0xffffffffffULL;
		u16 const opcode=(shift==16 ? 0xf060|!plus : plus ? 0xf000 : 0xf010)|((selector>>1)<<9)|((selector&1)<<8)|(shift==16 ? 0 : shift);
		unsigned const offset=producer ? 2+gap : 0, execute=producer ? 9+gap : 7;
		std::array<u64,2> const original{producer || (selector&2) ? 0x1111222233ULL : source,
			producer || !(selector&2) ? 0x4444555566ULL : source};
		auto before=original; if (producer) before[selector>>1]=source;
		auto after=before; after[selector&1]=result;
		auto initial=initial_case("immediate-subtract-and-shift16-add-retain-literal",{u16(producer ? 0xf020|((selector>>1)<<8) : opcode)});
		if (producer) { initial.program[1].value=u16(source); initial.program[offset].value=opcode; }
		initial.program[offset+1].value=literal; initial.program[offset+2].value=0xf4e1;
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,original[0]},{r::STATE_B,original[1]},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		auto state=[&](char const *name,std::array<u64,2> values,u16 status) {
			return observation(name,{{r::STATE_A,values[0]},{r::STATE_B,values[1]},{r::STATE_ST0,status},{r::STATE_ST1,st1}});
		};
		clocked_register_case c{{{0,std::move(initial)},{1,state("before-arithmetic-Execute",original,st0)},
			{execute,state("Execute-publishes-result-carry-and-sticky-overflow",after,result_st0)}},
			producer ? std::array<unsigned,3>{5+gap,6+gap,8+gap}[cases.size()%3] : std::array<unsigned,3>{3,4,6}[cases.size()%3],20};
		if (producer) c.states.insert(c.states.end()-1,{7,state("producer-publishes-before-consumer",before,st0)});
		c.actions={{4,{},false,{{base+0x601,0xdead}}}};
		if (producer) c.actions.push_back({6+gap,{},false,{{base+0x601,0xdead},{base+offset+0x601,0xbeef}}});
		c.forbid_data_access=true;
		if (producer) c.program_events.push_back({false,base+0x601,u16(source),0,false});
		c.program_events.push_back({false,base+offset+0x601,literal,0,false});
		cases.push_back(std::move(c));
	};
	for (unsigned selector=0;selector<4;++selector) for (unsigned pattern=0;pattern<8;++pattern)
	for (unsigned flags=0;flags<8;++flags) {
		for (unsigned shift=0;shift<=16;++shift) add(false,shift,selector,pattern,flags,false,0);
		add(true,16,selector,pattern,flags,false,0);
	}
	for (unsigned selector=0;selector<4;++selector) for (unsigned pattern=0;pattern<4;++pattern)
	for (unsigned gap=0;gap<3;++gap) add(false,0,selector,pattern,1,true,gap);
	return cases;
}
}
