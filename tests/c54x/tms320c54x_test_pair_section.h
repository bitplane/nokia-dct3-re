// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> pair_cases(bool sat)
{
	std::vector<clocked_register_case> cases;
	for (unsigned form=0;form<(sat ? 2U : 6U);++form) for (unsigned p=0;p<(sat ? 16U : 8U);++p)
	for (unsigned flags=0;flags<4;++flags) for (unsigned modes=0;modes<4;++modes) {
		bool const b=form&1;
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u16 const opcode=sat ? b ? 0xf583 : 0xf483 : 0xf490+form/2+(b ? 0x100 : 0);
		u64 value=sat ? std::array<u64,16>{0,1,0x7ffffffe,0x7fffffff,0x80000000,0x80000001,0xff7fffffffULL,0xff80000000ULL,
			0xff80000001ULL,0xffffffffffULL,0x7fffffffffULL,0x8000000000ULL,0x100000000ULL,0xfeffffffffULL,0x7fffff00,0xff80000100ULL}[p] :
			std::array<u64,8>{0,1,0x80000000,0xffffffff,0x5fb0001234ULL,0x81c0005555ULL,0xff00000000ULL,0x7f12345678ULL}[p];
		u64 const original=value;
		u16 const st0=sat ? 0x1824|(flags<<9) : 0x24|(flags&1 ? 0x800 : 0)|(flags&2 ? 0x1000 : 0), st1=0x800|(modes<<8);
		auto initial=initial_case(sat ? "consecutive-SAT-clamp-and-selected-overflow-clear" : "consecutive-rotate-carry-feedback-and-guard-clear",{opcode,opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,b ? 0x123456789aULL : value},
			{r::STATE_B,b ? value : 0x123456789aULL},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{5,6,7}[cases.size()%3],16};
		bool carry=flags&1;
		for (unsigned n=0;n<2;++n) {
			u16 status=st0;
			if (sat) {
				bool const negative=BIT(value,39), overflow=negative ? value<0xff80000000ULL : value>0x7fffffff;
				if (overflow) value=negative ? 0xff80000000ULL : 0x7fffffff;
				u16 const mask=b ? 0x200 : 0x400;
				status=(status&~mask)|(overflow ? mask : 0);
			} else {
				bool const right=form/2==0, incoming=form/2==2 ? bool(flags&2) : carry;
				bool const outgoing=BIT(value,right ? 0 : 31);
				value=right ? ((value&0xffffffffULL)>>1)|(u64(incoming)<<31) : ((value&0x7fffffffULL)<<1)|incoming;
				carry=outgoing; status=0x24|(carry ? 0x800 : 0)|(flags&2 ? 0x1000 : 0);
			}
			c.states.push_back({6+n,observation("one-result-per-Execute-with-other-accumulator-preserved",{{r::STATE_A,b ? 0x123456789aULL : value},
				{r::STATE_B,b ? value : 0x123456789aULL},{r::STATE_ST0,status},{r::STATE_ST1,st1}})});
		}
		// Explicit initial state remains checked until the first Execute.
		auto before=observation("no-early-pair-publication",{{r::STATE_A,b ? 0x123456789aULL : original},
			{r::STATE_B,b ? original : 0x123456789aULL},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		c.states.insert(c.states.begin()+1,{1,std::move(before)});
		c.forbid_data_access=true;
		c.program_events={{false,base+0x600,opcode,0,false},{false,base+0x601,opcode,0,false}};
		cases.push_back(std::move(c));
	}
	// Preserve the independent literal examples as native CPU expectations,
	// not a host test that only rechecks generated case layouts.
	struct literal { unsigned index; std::array<u64,2> value; std::array<u16,2> status; };
	std::array<literal,3> const goldens=sat ? std::array<literal,3>{{
		{64,{0x7fffffff,0x7fffffff},{0x1c24,0x1824}},
		{96,{0xff80000000ULL,0xff80000000ULL},{0x1c24,0x1824}},
		{112,{0xff80000000ULL,0xff80000000ULL},{0x1824,0x1824}}}} : std::array<literal,3>{{
		{20,{0x80000000,0xc0000000},{0x824,0x24}},
		{552,{1,3},{0x1824,0x1024}},
		{64,{0x5800091a,0x2c00048d},{0x24,0x24}}}};
	for (auto const &golden : goldens) for (unsigned n=0;n<2;++n)
		for (auto &check : cases[golden.index].states[2+n].observation.expected) {
			if (check.index==r::STATE_A) check.value=golden.value[n];
			if (check.index==r::STATE_ST0) check.value=golden.status[n];
		}
	return cases;
}
}
