// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline clocked_register_case unary_case(char const *name,u16 opcode,std::array<u64,2> before,std::array<u64,2> after,
	u16 st0,u16 status,u16 st1,u16 pmst,u32 base,unsigned checkpoint,std::optional<u16> producer={})
{
	unsigned const prefix=producer.has_value();
	auto initial=initial_case(name,{u16(producer ? ((opcode&0x200) ? 0xe900 : 0xe800)|*producer : opcode)});
	initial.opcode=opcode; // single_fetch observes the consumer, not its optional load producer.
	if (prefix) initial.program[1].value=opcode;
	initial.program[prefix+1].value=0xf4e1;
	for (auto &word : initial.program) word.address+=base;
	initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,before[0]},{r::STATE_B,before[1]},
		{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_PMST,pmst}});
	auto state=[&](char const *label,std::array<u64,2> values,u16 flags) { return observation(label,{
		{r::STATE_A,values[0]},{r::STATE_B,values[1]},{r::STATE_ST0,flags},{r::STATE_ST1,st1},{r::STATE_PMST,pmst}}); };
	clocked_register_case c{{{0,std::move(initial)},{1,state("unary-before-Execute",before,st0)},
		{6+prefix,state("unary-Execute-publishes-result-and-owned-status",after,status)}},checkpoint,16};
	if (producer) { auto loaded=before; loaded[BIT(opcode,9)]=*producer; c.states.insert(c.states.end()-1,{6,state("preceding-load-publishes",loaded,st0)}); }
	c.single_fetch=base+0x600+prefix;
	c.forbid_data_access=true;
	return c;
}
inline std::vector<clocked_register_case> register_logic_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned op=0;op<4;++op) for (unsigned selector=0;selector<4;++selector)
	for (unsigned p=0;p<4;++p) for (unsigned sxm=0;sxm<2;++sxm)
	for (int shift=op==3 ? 0 : -16;shift<(op==3 ? 1 : 16);++shift) {
		std::array<u64,2> const before{std::array<u64,4>{0,0xffffffffffULL,0x123456789aULL,0x8000000001ULL}[p],
			std::array<u64,4>{0xffffffffffULL,0x55aa55aa55ULL,0x0f0ff0f00fULL,0x7fffffffefULL}[p]};
		u64 result=0;
		for (int bit=0;bit<40;++bit) {
			int const from=bit-shift; bool const a=BIT(before[selector&1],bit), b=from>=0 && from<40 && BIT(before[selector>>1],from);
			if (op==0 ? a&&b : op==1 ? a||b : op==2 ? a!=b : !b) result|=u64(1)<<bit;
		}
		auto after=before; after[selector&1]=result;
		u16 const opcode=op==3 ? 0xf493|(selector<<8) : 0xf080+op*0x20+(selector<<8)+(shift&31);
		cases.push_back(unary_case("register-logic-bitwise-40-bit-reference",opcode,before,after,0xae1b,0xae1b,
			0xac0|(sxm<<8),1,selector&1 ? 0x20000 : 0,cases.size()%2 ? 5 : 3));
	}
	return cases;
}
inline std::vector<clocked_register_case> accumulator_shift_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned logical=0;logical<2;++logical) for (unsigned selector=0;selector<4;++selector)
	for (int shift=-16;shift<16;++shift) for (unsigned p=0;p<8;++p) for (unsigned flags=0;flags<8;++flags) {
		if (flags && shift!=-16 && shift!=-1 && shift!=0 && shift!=15) continue;
		bool const sxm=flags&1,ovm=flags&2,carry=flags&4;
		u64 const value=std::array<u64,8>{0,1,0x7fffffff,0xff80000000ULL,0x0180000000ULL,0xff7fffffffULL,0x80aa001234ULL,0xffffffff55ULL}[p];
		std::array<u64,2> const before{selector&2 ? 0x246813579aULL : value,selector&2 ? value : 0x246813579aULL};
		u16 const st0=0x1b|(p&1 ? 0x600 : 0)|(carry ? 0x800 : 0);
		u16 status=st0; u64 result=0; int const width=logical ? 32 : 40;
		for (int bit=0;bit<width;++bit) {
			int const from=bit-shift;
			if (from>=0 && from<width ? BIT(value,from) : !logical && from>=40 && sxm && BIT(value,39)) result|=u64(1)<<bit;
		}
		if (logical || shift) {
			bool const outgoing=shift && BIT(value,shift<0 ? -shift-1 : (logical ? 32 : 39)-shift);
			status=(status&~0x800)|(outgoing ? 0x800 : 0);
		}
		if (!logical && shift>0) {
			s64 const number=BIT(value,39) ? s64(value)-0x10000000000LL : s64(value), scaled=number*(s64(1)<<shift);
			if (scaled < -2147483648LL || scaled > 2147483647LL) { status|=selector&1 ? 0x200 : 0x400; if (ovm) result=scaled<0 ? 0xff80000000ULL : 0x7fffffff; }
		}
		auto after=before; after[selector&1]=result;
		cases.push_back(unary_case("accumulator-shift-width-carry-and-saturation",(logical ? 0xf0e0 : 0xf460)|(selector<<8)|(shift&31),
			before,after,st0,status,0x8c0|(sxm<<8)|(ovm<<9),1,selector&1 ? 0x20000 : 0,cases.size()%2 ? 5 : 3));
	}
	return cases;
}
inline std::vector<clocked_register_case> negate_cases(bool absolute)
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u64,16> patterns{0,1,0xffffffffffULL,0xfffffffffeULL,0x7fffffff,0xff80000000ULL,0xff80000001ULL,0x80000000,
		0x80000001,0x8000000000ULL,0x7fffffffffULL,0x100000000ULL,0xff00000000ULL,0x123456789ULL,0xff12345678ULL,0xffff};
	auto add=[&](unsigned selector,unsigned p,unsigned flags,std::optional<u16> producer) {
		u64 const source=producer ? *producer : patterns[p];
		s64 const signed_source=BIT(source,39) ? s64(source)-0x10000000000LL : s64(source);
		s64 const negated=absolute && signed_source>=0 ? signed_source : -signed_source;
		bool const overflow=negated < -2147483648LL || negated > 2147483647LL;
		u64 const result=u64(overflow && (flags&1) ? std::clamp<s64>(negated,-2147483648LL,2147483647LL) : negated)&0xffffffffffULL;
		u16 const st0=0x101b|(flags&2 ? 0x400 : 0)|(flags&4 ? 0x200 : 0)|(flags&8 ? 0x800 : 0);
		u16 const status=(st0&~0x800)|(!source ? 0x800 : 0)|(overflow ? selector&1 ? 0x200 : 0x400 : 0);
		std::array<u64,2> const before{selector&2 ? 0x123456789aULL : patterns[p],selector&2 ? patterns[p] : 0x2233445566ULL};
		auto after=before; if (producer) after[selector>>1]=source; after[selector&1]=result;
		unsigned const prefix=producer.has_value();
		cases.push_back(unary_case(absolute ? "ABS-sign-and-overflow-boundaries" : "NEG-carry-and-overflow-boundaries",
			(absolute ? 0xf485 : 0xf484)|(selector<<8),before,after,st0,status,0x800|(flags&1 ? 0x200 : 0)|(flags&16 ? 0x100 : 0),
			0,0,std::array<unsigned,3>{3+prefix,5+prefix,6+prefix}[cases.size()%3],producer));
	};
	for (unsigned p=0;p<16;++p) for (unsigned selector=0;selector<4;++selector) for (unsigned ovm=0;ovm<2;++ovm) add(selector,p,ovm,{});
	for (unsigned p : {0U,9U}) for (unsigned selector=0;selector<4;++selector) for (unsigned flags=2;flags<32;++flags) add(selector,p,flags,{});
	for (u16 source : {0,5}) for (unsigned selector=0;selector<4;++selector) for (unsigned flags : {0U,1U,14U,31U}) add(selector,3,flags,source);
	return cases;
}
}
