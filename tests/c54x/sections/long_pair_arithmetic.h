// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned op=0;op<3;++op) for (unsigned form : {0U,1U,2U,3U,5U,6U,7U,8U})
	for (unsigned selector=0;selector<(op==0 ? 4U : 2U);++selector) for (unsigned pattern=0;pattern<8;++pattern)
	for (unsigned dual=0;dual<2;++dual) for (unsigned sxm=0;sxm<2;++sxm)
	for (unsigned ovm=0;ovm<2;++ovm) for (unsigned odd=0;odd<2;++odd) {
		bool const destination=selector&1, source_acc=op==0 ? selector>>1 : destination;
		unsigned const reg=odd ? 6 : 1, read=form>=5 ? 6 : 5;
		u32 const base=odd ? 0x20000 : 0;
		u16 const initial_ar=(form==7 ? 0x1007 : 0x1000)+odd;
		s16 const offset=odd ? -2 : 2;
		u16 address=form==0 ? 0x402+odd : form==1 ? 0x1002+odd : form>=5 && form<=7 ? u16(initial_ar+offset) : form==8 ? 0x1400+odd : initial_ar;
		u16 const next_ar=form==2 ? initial_ar+2 : form==3 ? initial_ar-2 : form==6 || form==7 ? address : initial_ar;
		u16 mode=form<2 ? 2+odd : form==8 ? 0xf8 : std::array<u8,6>{0x90,0x88,0x98,0xe0,0xe8,0xf0}[form-2]|reg;
		if (op==0 && form<2) { mode+=2; address+=2; }
		u16 const literal=form==8 ? address : u16(offset);
		u64 const source=std::array<u64,8>{0,0x10000,0x7fffffff,0xff80000000ULL,0x0180000000ULL,0xff7fffffffULL,0x56783933,0x80aa001234ULL}[pattern];
		u32 const memory=std::array<u32,8>{1,1,0xffffffffU,1,0,0x80000000U,0x15343456,0xdeadbeefU}[pattern];
		u16 const st0=0x1808|(pattern&1 ? 0x600 : 0), st1=0x800|(dual<<7)|(sxm<<8)|(ovm<<9)|(form==1 ? 0x4000 : 0);
		u64 result; u16 status;
		if (dual) {
			s64 const a=sxm ? s16(source>>16) : u16(source>>16), b=sxm ? s16(memory>>16) : u16(memory>>16);
			s64 const high=op==0 ? a+b : op==1 ? a-b : b-a;
			u16 const low=op==0 ? u16(source)+u16(memory) : op==1 ? u16(source)-u16(memory) : u16(memory)-u16(source);
			result=((u64(high)&0xffffff)<<16)|low;
			bool const carry=op==0 ? u32(u16(source>>16))+u16(memory>>16)>0xffff : op==1 ? u16(source>>16)>=u16(memory>>16) : u16(memory>>16)>=u16(source>>16);
			status=(st0&~0x800)|(carry ? 0x800 : 0);
		} else {
			s64 const a=BIT(source,39) ? s64(source)-0x10000000000LL : s64(source);
			s64 const b=sxm && BIT(memory,31) ? s64(memory)-0x100000000LL : s64(memory);
			s64 const total=op==0 ? a+b : op==1 ? a-b : b-a;
			bool const overflow=total < -2147483648LL || total > 2147483647LL;
			bool const carry=op==0 ? u64(u32(source))+memory>0xffffffffULL : op==1 ? u32(source)>=memory : memory>=u32(source);
			result=u64(ovm && overflow ? std::clamp<s64>(total,-2147483648LL,2147483647LL) : total)&0xffffffffffULL;
			status=(st0&~0x800)|(carry ? 0x800 : 0)|(overflow ? destination ? 0x200 : 0x400 : 0);
		}
		u16 const opcode=(op==0 ? 0x5000|(unsigned(source_acc)<<9) : op==1 ? 0x5400 : 0x5800)|(unsigned(destination)<<8)|mode;
		auto initial=initial_case("paired-word-arithmetic-captures-parity-ordered-source",{opcode});
		if (form>=5) initial.program[1].value=literal;
		initial.program[1+(form>=5)].value=0xf4e1;
		for (auto &entry : initial.program) entry.address+=base;
		initial.data={{address,u16(memory>>16)},{u32(address^1),u16(memory)}};
		u64 const a=source_acc ? 0x246813579aULL : source, b=source_acc ? source : 0x246813579aULL;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,a},{r::STATE_B,b},{r::STATE_SP,0x1000},{r::STATE_AR0+reg,initial_ar},{r::STATE_BK,16}});
		auto before=observation("Read-has-not-published-result",{{r::STATE_A,a},{r::STATE_B,b},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		auto pointer=before; pointer.expected.push_back({int(r::STATE_AR0+reg),next_ar});
		auto final=observation("Execute-uses-captured-pair-not-poisoned-address-or-RAM",{{r::STATE_A,destination ? a : result},
			{r::STATE_B,destination ? result : b},{r::STATE_ST0,status},{r::STATE_ST1,st1},{r::STATE_SP,0x2000},{int(r::STATE_AR0+reg),0x2000}});
		clocked_register_case c{{{0,std::move(initial)},{1,before},{read-1,std::move(pointer)},{read,before},{read+1,std::move(final)}},
			read-2+std::array<unsigned,3>{0,2,3}[cases.size()%3],16};
		if (form>=5) c.actions.push_back({read-2,{},false,{{base+0x601,0xdead}}});
		c.actions.push_back({read-1,{{r::STATE_AR0+reg,0x2000},{r::STATE_SP,0x2000}}});
		c.actions.push_back({read,{},false,{},{},{{address,0xdead},{u32(address^1),0xbeef}}});
		c.data_events=std::vector<clocked_data_event>{{false,address,u16(memory>>16),read},{false,u32(address^1),u16(memory),read}};
		if (form>=5) c.retained_program_reads={{base+0x601,literal}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
