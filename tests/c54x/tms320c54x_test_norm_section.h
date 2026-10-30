// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef C54X_TEST_NORM_SECTION_H
#define C54X_TEST_NORM_SECTION_H
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> norm_cases(unsigned kind)
{
	bool const producer=kind==2,alias=kind==1;
	std::vector<clocked_register_case> cases;
	std::array<u64,16> const exponent_values{0,1,2,0x7fff,0x8000,0x7fffffff,0x80000000ULL,0xffffffffULL,
		0x0100000000ULL,0x7fffffffffULL,0x8000000000ULL,0xffffffffffULL,0xffffffffcbULL,
		0x0785432105ULL,0xff80000000ULL,0xff00000000ULL};
	std::array<u64,8> const a_values{0,1,0x7fffffff,0xff80000000ULL,0x7fffffffffULL,0x8000000000ULL,0xffffffffffULL,0x123456789aULL};
	std::array<u64,8> const b_values{1,0xffffffffULL,0xff80000000ULL,0x7fffffff,0xffffffffffULL,0x123456789aULL,0x8000000000ULL,0x7fffffffffULL};
	for (unsigned selector=0;selector<4;++selector)
	for (unsigned pattern=0;pattern<(producer ? 16U : 8U);++pattern)
	for (unsigned flags=0;flags<(producer ? 4U : 8U);++flags)
	for (unsigned count=0;count<(producer ? 3U : alias ? 16U : 48U);++count) {
		unsigned const gap=producer ? count : 0,read=5+unsigned(producer)+gap,execute=read+1;
		std::array<u64,2> const before=producer ? std::array<u64,2>{exponent_values[pattern],exponent_values[(pattern+7)%16]} :
			std::array<u64,2>{a_values[pattern],b_values[pattern]};
		u64 const source=before[selector>>1];
		bool const sign=source&0x8000000000ULL;
		u64 magnitude=sign ? (~source)&0xffffffffffULL : source;
		unsigned significant=0; while (magnitude) { ++significant; magnitude>>=1; }
		int const exponent=source ? 31-int(significant) : 0;
		int const shift=producer ? (gap ? exponent : 1) : int(count)-16;
		u16 const t=producer ? 0xa5c1 : alias ? 0xa5e0|count : 0xa5c0|(shift&63);
		u16 const st0=0x1004|((producer ? pattern&1 : flags&4) ? 0xe00 : 0),st1=0x813|((flags&3)<<8);
		u64 result=0; bool overflow=false;
		for (int bit=0;bit<40;++bit) {
			int const from=bit-shift;
			if ((from>=0 && from<40 && BIT(source,from)) || (shift<0 && from>=40 && (flags&1) && sign)) result|=u64(1)<<bit;
		}
		// Inspect the mathematical wide result without overflowing a host integer.
		if (shift>=0) for (int bit=31;bit<=70;++bit) {
			int const from=bit-shift;
			bool const value=from<0 ? false : from<40 ? bool(BIT(source,from)) : sign;
			overflow|=value!=sign;
		}
		if (overflow && (flags&2)) result=sign ? 0xff80000000ULL : 0x7fffffff;
		auto after=before; after[selector&1]=result;
		u16 const final_st0=st0|(overflow ? selector&1 ? 0x200 : 0x400 : 0);
		u32 const base=(pattern+flags+count+selector)%2 ? 0x20000 : 0;
		unsigned const offset=unsigned(producer)+gap;
		u16 const opcode=0xf48f|((selector>>1)<<9)|((selector&1)<<8);
		auto initial=initial_case("NORM-retained-count-and-Execute-publication",{0xf495});
		for (auto &word : initial.program) word.address+=base;
		if (producer) initial.program[0].value=0xf48e|((selector>>1)<<8);
		initial.program[offset].value=opcode; initial.program[offset+1].value=0xf4e1;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_T,t},{r::STATE_A,before[0]},{r::STATE_B,before[1]}});
		initial.expected.insert(initial.expected.end(),{{r::STATE_A,before[0]},{r::STATE_B,before[1]},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)},
			{execute,observation("NORM-publishes-result-and-sticky-overflow",{{r::STATE_A,after[0]},
				{r::STATE_B,after[1]},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_T,0xdead}})},
			{20,observation("NORM-halts-with-retained-count",{{r::STATE_IDLE,1},{r::STATE_A,after[0]},
				{r::STATE_B,after[1]},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_T,0xdead}})}},
			producer ? std::array<unsigned,4>{3,read-1,read,execute+1}[cases.size()%4] :
			alias ? 3+unsigned(cases.size()%2) : 3+unsigned(cases.size()%4),20};
		c.actions={{read,{{r::STATE_T,0xdead}}}};
		c.forbid_data_access=true;
		c.program_events.push_back({false,base+0x600+offset,opcode,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
#endif
